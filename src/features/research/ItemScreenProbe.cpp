#include "features/research/ItemScreenProbe.h"
#ifdef LAMIUM_ITEM_SCREEN_PROBE
#include "app/TraceLog.h"
#include "features/inspection/render/ItemIcon.h"
#include "ui/SettingsScreen.h"
#include "ui/Widgets.h"
#include "mc/client/input/KeyboardManager.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/ItemStack.h"
#include <algorithm>
#include <chrono>
#include <format>
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <windows.h>
#pragma comment(lib, "user32.lib") // GetAsyncKeyState, probe builds only

namespace lamium::researchTrace::itemScreen {
namespace {
ll::event::ListenerPtr renderListener;
TraceBudget screenBudget, treeBudget, costBudget, failBudget, openBudget;
// One report per screen name, once its layout has stood still for half a second.
std::map<std::string, std::string> reported;
std::string settling;
int stillFrames = 0;
bool f8Down = false, f9Down = false, f10Down = false;
// 0 not focused, 1 focused, 2 focused with the platform keyboard claimed. Keys and text arrive off this thread.
std::atomic<int> focus{0};
bool keyboardOwned = false;
std::mutex textMutex;
std::string typed, keysSeen;
TraceBudget textBudget, keyBudget;
int frames = 0;
double spent = 0, worst = 0;
// Shown controls with a size, down to `depth` levels: name and box. Names with "recipe", "search" or "tab" in them are kept at any depth.
void describe(UIControl const& control, int depth, std::string& out, int& count) {
    if (count >= 260 || depth > 14) return;
    auto const& children = *control.mChildren;
    if (children.size() > 600) return; // not a child list
    for (auto const& child : children) {
        if (!child || count >= 260) continue;
        std::string const& name = *child->mName;
        if (name.size() > 120 || !static_cast<int>(child->mVisible)) continue;
        glm::vec2 const at = *child->mCachedPosition, size = *child->mSize;
        bool const named = name.find("recipe") != std::string::npos || name.find("search") != std::string::npos || name.find("tab") != std::string::npos;
        if ((depth <= 7 && size.x > 0 && size.y > 0) || named) {
            ++count;
            out += std::format(" {}:{}[{:.0f},{:.0f} {:.0f}x{:.0f}]", depth, name, at.x, at.y, size.x, size.y);
        }
        describe(*child, depth + 1, out, count);
    }
}
void draw(ll::event::UIRenderEvent& event) {
    auto& view = event.screenView();
    // Other views (the HUD under the screen) come through here too: they are not this probe's business.
    if (!view.mHasFocus || !view.mController || !view.mController->_isContainerScreen()) return;
    auto* tree = view.mVisualTree.get();
    if (!tree) return;
    glm::vec2 const screen = *view.mSize;
    std::string const name = view.getScreenName();
    // F8 opens Lamium's screen over this one (the settings key is not dispatched while a game screen is open).
    bool const down = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    if (down && !f8Down)
        if (auto client = ll::service::getClientInstance()) {
            traceLog(openBudget, 20, "L-359 opening Lamium's screen over '{}'", name);
            ui::open(*client);
        }
    f8Down = down;
    // The text field: F9 focuses it, F10 focuses it with the platform keyboard claimed, the same key or Esc leaves.
    bool const f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0, f10 = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
    int wanted = focus.load();
    if (f9 && !f9Down) wanted = wanted == 1 ? 0 : 1;
    if (f10 && !f10Down) wanted = wanted == 2 ? 0 : 2;
    f9Down = f9;
    f10Down = f10;
    if (auto client = ll::service::getClientInstance()) {
        auto& keyboard = client->getKeyboardManager();
        if (keyboardOwned && wanted != 2) { keyboard.disableKeyboard(); keyboard.releaseKeyboardOwnership(); keyboardOwned = false; }
        if (wanted == 2 && !keyboardOwned) {
            bool const claimed = keyboard.tryClaimKeyboardOwnership();
            bool const enabled = claimed && keyboard.tryEnableKeyboard({}, 128, true, false, false, Vec2{screen.x / 2, screen.y - 20}, 20.0f);
            if (claimed && !enabled) keyboard.releaseKeyboardOwnership();
            keyboardOwned = enabled;
            traceLog(textBudget, 200, "L-359 text field on '{}': keyboard claimed {}, enabled {}", name, claimed, enabled);
        }
    }
    if (wanted != focus.load()) {
        focus = wanted;
        traceLog(textBudget, 200, "L-359 text field on '{}': focus {}", name, wanted);
    }
    {
        std::lock_guard lock(textMutex);
        float const w = 200, x = (screen.x - w) / 2, y = screen.y - 22;
        ui::panel(event.uiRenderContext(), x, y, w, 16, .97f);
        ui::label(event.uiRenderContext(), x + 4, y + 4, w - 8,
            std::format("[{}] {}", wanted == 0 ? "F9 / F10" : wanted == 1 ? "typing" : "typing+kbd", typed + (wanted ? "_" : "")));
        ui::label(event.uiRenderContext(), x, y - 10, w, keysSeen);
    }
    static std::string const windowName = "root_panel";
    auto window = tree->getControlByName(windowName, true);
    glm::vec2 at{}, size{};
    if (window && !window->mCachedPositionDirty) { at = *window->mCachedPosition; size = *window->mSize; }
    bool const laidOut = window && size.x > 0 && size.y > 0 && size.x < screen.x - 1;
    std::string const now = std::format("{} {:.0f},{:.0f} {:.0f}x{:.0f}", name, at.x, at.y, size.x, size.y);
    if (now != settling) { settling = now; stillFrames = 0; }
    else if (++stillFrames == 30 && reported[name] != now) {
        reported[name] = now;
        traceLog(screenBudget, 60, "L-359 item screen '{}': root_panel {} at {:.0f},{:.0f} size {:.0f}x{:.0f}; view {:.0f}x{:.0f}", name,
            window ? "found" : "absent", at.x, at.y, size.x, size.y, screen.x, screen.y);
        // From the top of the tree: every control that is shown.
        std::shared_ptr<UIControl> top = window;
        if (!top) {
            static std::string const anyName = "variables_button_mappings_and_controls";
            top = tree->getControlByName(anyName, true);
        }
        for (int up = 0; top && up < 16; ++up) {
            auto parent = top->mParent.lock();
            if (!parent) break;
            top = parent;
        }
        if (top) {
            std::string controls;
            int count = 0;
            describe(*top, 1, controls, count);
            for (size_t from = 0, part = 1; from < controls.size() && part <= 6; from += 3500, ++part)
                traceLog(treeBudget, 120, "L-359 item screen '{}' controls, part {} ({} in all):{}", name, part, count, controls.substr(from, 3500));
        } else traceLog(treeBudget, 120, "L-359 item screen '{}': no control to start from", name);
    }
    if (!laidOut) return;
    // 300 icons in the room right of the window, 18 units apart: the cost of a full item list.
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return;
    auto& inventory = player->getInventory();
    std::vector<ItemStack const*> stacks;
    for (int slot = 0; slot < inventory.getContainerSize() && stacks.size() < 36; ++slot)
        if (auto const& stack = inventory.getItem(slot); !stack.isNull()) stacks.push_back(&stack);
    if (stacks.empty()) return;
    float const left = at.x + size.x + 6, room = screen.x - left - 4;
    int const columns = static_cast<int>(room / 18);
    if (columns < 1) return;
    std::vector<inspection::render::IconAt> icons;
    for (int i = 0; i < 300; ++i) {
        float const y = 4 + static_cast<float>(i / columns) * 18;
        if (y + 16 > screen.y) break;
        icons.push_back({stacks[static_cast<size_t>(i) % stacks.size()], left + static_cast<float>(i % columns) * 18, y});
    }
    auto const started = std::chrono::steady_clock::now();
    inspection::render::drawItemIcons(event.uiRenderContext(), icons, 17);
    double const took = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    spent += took;
    worst = std::max(worst, took);
    if (++frames == 120) {
        traceLog(costBudget, 60, "L-359 icons on '{}': {} icons of {} stacks, {:.3f} ms a frame on average, {:.3f} ms at worst over 120 frames", name,
            icons.size(), stacks.size(), spent / frames, worst);
        frames = 0;
        spent = worst = 0;
    }
}
}
bool type(std::string const& text) {
    if (!focus.load()) return false;
    std::lock_guard lock(textMutex);
    std::string shown;
    for (unsigned char c : text) shown += c < 32 ? std::format("\\x{:02x}", c) : std::string(1, static_cast<char>(c));
    traceLog(textBudget, 200, "L-359 text field got text '{}'", shown);
    for (char c : text) if (static_cast<unsigned char>(c) >= 32) typed += c;
    if (typed.size() > 60) typed.erase(0, typed.size() - 60);
    return true;
}
bool key(int code, bool down) {
    if (!focus.load() || code == 0x78 || code == 0x79) return false; // F9 and F10 are the probe's own
    std::lock_guard lock(textMutex);
    traceLog(keyBudget, 300, "L-359 text field kept key {:#04x} {} from the game", code, down ? "down" : "up");
    if (down) {
        keysSeen = std::format("key {:#04x}", code);
        if (code == 0x1B) focus = 0; // Esc leaves the field, not the screen
        if (code == 0x08 && !typed.empty()) typed.pop_back();
    }
    return true;
}
void start() {
    renderListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) {
        try {
            draw(event);
        } catch (std::exception const& error) { traceLog(failBudget, 6, "L-359 item screen probe failed: {}", error.what()); } catch (...) {}
    });
    Runtime::instance().self().getLogger().warn("L-359 item screen probe enabled");
}
void stop() {
    if (renderListener) {
        ll::event::EventBus::getInstance().removeListener(renderListener);
        renderListener.reset();
    }
    reported.clear();
}
}
#else
namespace lamium::researchTrace::itemScreen { void start() {} void stop() {} bool type(std::string const&) { return false; } bool key(int, bool) { return false; } }
#endif

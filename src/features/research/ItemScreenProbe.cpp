#include "features/research/ItemScreenProbe.h"
#ifdef LAMIUM_ITEM_SCREEN_PROBE
#include "app/TraceLog.h"
#include "features/inspection/render/ItemIcon.h"
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
#include <string>
#include <vector>

namespace lamium::researchTrace::itemScreen {
namespace {
ll::event::ListenerPtr renderListener;
TraceBudget screenBudget, treeBudget, costBudget, failBudget;
std::string lastSeen;
int frames = 0;
double spent = 0, worst = 0;
// The controls under `control`, three levels down: name, box, whether shown.
void describe(UIControl const& control, int depth, std::string& out, int& count) {
    if (count >= 80 || depth > 3) return;
    auto const& children = *control.mChildren;
    if (children.size() > 400) return; // not a child list
    for (auto const& child : children) {
        if (!child || count >= 80) continue;
        std::string const& name = *child->mName;
        if (name.size() > 120) continue;
        glm::vec2 const at = *child->mCachedPosition, size = *child->mSize;
        ++count;
        out += std::format(" {}{}[{:.0f},{:.0f} {:.0f}x{:.0f}{}]", std::string(static_cast<size_t>(depth), '>'), name, at.x, at.y, size.x, size.y,
            static_cast<int>(child->mVisible) ? "" : " hidden");
        describe(*child, depth + 1, out, count);
    }
}
void draw(ll::event::UIRenderEvent& event) {
    auto& view = event.screenView();
    if (!view.mHasFocus || !view.mController || !view.mController->_isContainerScreen()) { lastSeen.clear(); return; }
    auto* tree = view.mVisualTree.get();
    if (!tree) return;
    static std::string const windowName = "root_panel";
    auto window = tree->getControlByName(windowName, true);
    glm::vec2 const screen = *view.mSize;
    std::string const name = view.getScreenName();
    if (!window) {
        if (lastSeen != name + " none") {
            lastSeen = name + " none";
            traceLog(screenBudget, 80, "L-359 item screen '{}': no root_panel; view {:.0f}x{:.0f}", name, screen.x, screen.y);
        }
        return;
    }
    if (window->mCachedPositionDirty) return;
    glm::vec2 const at = *window->mCachedPosition, size = *window->mSize;
    if (size.x <= 0 || size.y <= 0 || size.x >= screen.x - 1) return; // not laid out yet
    std::string const seen = std::format("{} {:.0f},{:.0f} {:.0f}x{:.0f} in {:.0f}x{:.0f}", name, at.x, at.y, size.x, size.y, screen.x, screen.y);
    if (seen != lastSeen) {
        lastSeen = seen;
        traceLog(screenBudget, 80, "L-359 item screen '{}': root_panel at {:.0f},{:.0f} size {:.0f}x{:.0f}; view {:.0f}x{:.0f}; room left {:.0f}, right {:.0f}, "
                                   "below {:.0f}",
            name, at.x, at.y, size.x, size.y, screen.x, screen.y, at.x, screen.x - (at.x + size.x), screen.y - (at.y + size.y));
        // From the top of the tree, so a recipe book beside the window shows up with its box.
        std::shared_ptr<UIControl> top = window;
        for (int up = 0; up < 12; ++up) {
            auto parent = top->mParent.lock();
            if (!parent) break;
            top = parent;
        }
        std::string controls;
        int count = 0;
        describe(*top, 1, controls, count);
        traceLog(treeBudget, 80, "L-359 item screen '{}' controls ({}, at most 80, from '{}'):{}", name, count, *top->mName,
            controls.size() > 6000 ? controls.substr(0, 6000) + " ..." : controls);
    }
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
    lastSeen.clear();
}
}
#else
namespace lamium::researchTrace::itemScreen { void start() {} void stop() {} }
#endif

#include "ui/SettingsScreen.h"
#include "settings/Options.h"
#include "ui/SettingsLayout.h"
#include "ui/SearchQuery.h"
#include "ui/Localization.h"
#include "app/Runtime.h"
#include "features/camera/Zoom.h"
#include "input/Actions.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/FontRepository.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/TextAlignment.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/client/gui/screens/SceneFactory.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/gui/screens/interfaces/ISceneStack.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/deps/input/MouseAction.h"
#include <array>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace lamium::ui {
namespace {
std::recursive_mutex mutex;
IClientInstance* client = nullptr;
std::shared_ptr<AbstractScene> scene;
int selected = 0;
int hovered = -1;
int command = 0;
int commandRow = 0;
int firstVisible = 0;
bool seen = false;
bool closing = false;
SearchQuery query;
bool searchFocused = false;
bool textHook = false;
std::vector<settings::Option const*> visibleOptions;
int rowCount() { return static_cast<int>(visibleOptions.size()) + 2; }
void filterOptions() {
    visibleOptions.clear();
    for (auto const& option : settings::options) {
        auto searchable = std::string(option.id) + " " + std::string(option.feature) + " "
            + translated(option.label) + " " + translated(option.feature);
        if (query.matches(searchable)) visibleOptions.push_back(&option);
    }
    selected = 0; hovered = -1; firstVisible = 0; command = 0;
}
std::string error;
std::array<ll::event::ListenerPtr, 5> listeners;
bool backgroundHook = false;
constexpr mce::Color white{1.0f,1.0f,1.0f,1.0f};

bool ownsTop() {
    return client && scene && client->getSceneFactory().getCurrentSceneStack()->getTopScene() == scene.get();
}
LL_TYPE_INSTANCE_HOOK(SettingsWorldBackground, ll::memory::HookPriority::Normal, UIScene,
    &UIScene::$renderGameBehind, bool) {
    std::lock_guard lock(mutex);
    if (scene.get() == this) return true;
    return origin();
}
LL_TYPE_INSTANCE_HOOK(SettingsSearchText, ll::memory::HookPriority::Normal, UIScene,
    &UIScene::$handleTextChar, void, std::string const& text, FocusImpact impact) {
    std::lock_guard lock(mutex);
    if (scene.get() == this && ownsTop()) {
        if (searchFocused && query.append(text)) filterOptions();
        return;
    }
    origin(text, impact);
}
void clear() { client = nullptr; scene.reset(); seen = false; closing = false; command = 0; hovered = -1; }
void close() {
    if (ownsTop()) {
        if (!closing) client->getSceneFactory().getCurrentSceneStack()->schedulePopScreen(1);
        closing = true;
    } else clear();
}
void activate(int row, int direction) {
    // Read current preferences for every edit so another action cannot be
    // overwritten by a stale copy captured when the screen opened.
    auto value = Runtime::instance().preferences();
    if (row == rowCount() - 1) { close(); return; }
    if (row == 0) { searchFocused = true; return; }
    if (row < 0 || row >= rowCount() - 1) return;
    searchFocused = false;
    visibleOptions[row-1]->adjust(value, direction);
    error = Runtime::instance().save(value) ? std::string{} : translated("saveError");
}
void label(MinecraftUIRenderContext& context, float x, float y, float width, std::string text) {
    auto& font = context.mClient.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default").getFont();
    TextMeasureData const measure{1.0f, 0.0f, true, false, false, ::ui::TextAlignment::Left};
    CaretMeasureData const caret{-1, false};
    context.drawText(font, RectangleArea{x,x+width,y,y+14}, std::move(text), white, 1.0f,
        ::ui::TextAlignment::Left, measure, caret);
}
void render(ll::event::UIRenderEvent& event) {
    std::lock_guard lock(mutex);
    auto& context = event.uiRenderContext();
    auto& current = context.mClient;
    auto& view = event.screenView();
    glm::vec2 size = view.mSize;
    if (!scene) {
        if (gameplayScreen(current.getScreenName()) && Runtime::instance().preferences().ui.gameplayHints) {
            label(context, 6, 6, size.x-12, gameplayKeyHint(current));
            context.flushText(0, std::nullopt);
        }
        return;
    }
    if (&current != client) return;
    if (!ownsTop()) { if (seen) clear(); return; }
    seen = true;
    int action = closing ? 0 : std::exchange(command, 0);
    if (action == 1) activate(commandRow, 1);
    if (action == -1) activate(commandRow, -1);
    if (action == 2) close();
    if (!scene) return;
    auto layout = SettingsLayout::fit(size.x, size.y, rowCount(), selected, firstVisible);
    firstVisible = layout.first;
    float width = layout.width, left = layout.left, top = layout.top;
    context.fillRectangle(RectangleArea{0,size.x,0,size.y}, mce::Color{.07f,.08f,.11f,1.0f}, .25f);
    context.fillRectangle(RectangleArea{left-6,left+width+6,top-6,layout.footer+34},
        mce::Color{.07f,.08f,.11f,1.0f}, .78f);
    context.flushImages(white,1,HashedString{"ui_fillColor"});
    if (!layout.visible) {
        hovered = -1;
        label(context, 4, 4, std::max(1.0f, size.x - 8), translated("smallWindow"));
        context.flushText(0, std::nullopt);
        return;
    }
    label(context,left,top,width,translated("title"));
    if (layout.subtitle) label(context,left,top+18,width,translated(visibleOptions.empty() ? "noResults" : "subtitle"));
    auto const preferences = Runtime::instance().preferences();
    auto rowLabel = [&](int index) {
        if (index == rowCount() - 1) return translated("close");
        if (index == 0) return translated("search", query.value() + (searchFocused ? "_" : ""));
        auto const& option = *visibleOptions[index-1];
        auto value = option.read(preferences);
        if (auto flag = std::get_if<bool>(&value))
            return translated(option.label, translated(*flag ? "on" : "off"));
        return translated(option.label, std::get<float>(value));
    };
    glm::vec2 pointer = view.mPointerLocationPrevious;
    hovered = layout.hit(pointer.x, pointer.y);
    for (int i=layout.first;i<layout.first+layout.visible;++i) {
        float y = layout.rowY(i);
        context.fillRectangle(RectangleArea{left,left+width,y,y+20},
            selected == i ? mce::Color{.28f,.24f,.43f,1.0f}
                : hovered == i ? mce::Color{.22f,.23f,.30f,1.0f} : mce::Color{.15f,.16f,.21f,1.0f},1);
        context.flushImages(white,1,HashedString{"ui_fillColor"});
        label(context,left+6,y+5,width-12,rowLabel(i));
    }
    label(context,left,layout.footer,width,error.empty() ? translated("navigation") : error);
    if (layout.secondHint)
        label(context,left,layout.footer+15,width,translated("adjustment"));
    context.flushText(0,std::nullopt);
}
}
void open(IClientInstance& current) {
    std::lock_guard lock(mutex);
    if (scene || !gameplayScreen(current.getScreenName())) return;
    Zoom::instance().reset();
    selected = 0; hovered = -1; command = 0; error.clear(); seen = false; closing = false;
    firstVisible = 0; commandRow = 0;
    query.clear(); searchFocused = false; filterOptions();
    // This native information screen supplies focus/cursor ownership. It has no
    // form ID, packet, or server callback. Lamium draws and handles its own UI.
    scene = current.getSceneFactory().createCommonDialogInfoScreen("Lamium", "");
    if (!scene) return;
    client = &current;
    current.getSceneFactory().getCurrentSceneStack()->pushScreen(scene, false);
}
bool ownsInput() {
    std::lock_guard lock(mutex);
    return scene != nullptr;
}
void start() {
    if (!startLocalization()) throw std::runtime_error("Could not install Lamium action translations");
    backgroundHook = SettingsWorldBackground::hook(true) == 0;
    if (!backgroundHook) throw std::runtime_error("Could not install settings world background hook");
    textHook = SettingsSearchText::hook(true) == 0;
    if (!textHook) throw std::runtime_error("Could not install settings text input hook");
    auto& bus = ll::event::EventBus::getInstance();
    listeners[0] = bus.emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) {
        std::lock_guard lock(mutex);
        if (scene && seen && &event.uiRenderContext().mClient == client && !ownsTop()) clear();
        if (!scene) render(event);
    });
    listeners[4] = bus.emplaceListener<ll::event::BeforeUIRenderEvent>([](auto& event) {
        std::lock_guard lock(mutex);
        auto* native = dynamic_cast<UIScene*>(scene.get());
        if (!native || native->mScreenView.get() != &event.screenView()) return;
        // Only replace our focus-owning dialog's drawing. Other native screens
        // and the world keep their normal rendering and resource-pack behavior.
        event.cancel();
        render(event);
    });
    listeners[1] = bus.emplaceListener<ll::event::input::MouseInputEvent>([](auto& event) {
        std::lock_guard lock(mutex);
        if (!ownsTop()) return;
        if (event.actionButtonId() == MouseAction::ActionMove || event.actionButtonId() == MouseAction::ActionMoveRelative) return;
        // A button may already be down when F8 opens the panel. Let vanilla
        // observe its release, just as we do for keys, so it cannot stay held.
        if (event.actionButtonId() != MouseAction::ActionWheel && event.buttonData() == MouseAction::DataUp) return;
        event.cancel();
        if (event.actionButtonId() == MouseAction::ActionWheel && event.buttonData() != 0) {
            searchFocused = false;
            selected = std::clamp(selected + (event.buttonData() > 0 ? -1 : 1), 0, rowCount()-1);
        }
        if (event.actionButtonId() == MouseAction::ActionLeft && event.buttonData() == MouseAction::DataDown && hovered >= 0) {
            selected = hovered; commandRow = hovered; command = 1;
        }
        if (event.actionButtonId() == MouseAction::ActionRight && event.buttonData() == MouseAction::DataDown && hovered >= 0 && hovered < rowCount()-1) {
            selected = hovered; commandRow = hovered; command = -1;
        }
    });
    listeners[2] = bus.emplaceListener<ll::event::input::KeyInputEvent>([](auto& event) {
        std::lock_guard lock(mutex);
        if (!ownsTop()) return;
        // Let key-up through so keys pressed before opening cannot stick.
        if (!event.isDown()) return;
        event.cancel();
        if (searchFocused) {
            switch (event.keyCode()) {
            case 0x08: if (query.backspace()) filterOptions(); break;
            case 0x1b: searchFocused = false; break;
            case 0x0d: case 0x09: case 0x28:
                searchFocused = false; selected = 1; break;
            }
            return;
        }
        switch (event.keyCode()) {
        case 0x1b: command = 2; break;
        case 0x26: selected = (selected+rowCount()-1)%rowCount(); break;
        case 0x09: case 0x28: selected = (selected+1)%rowCount(); break;
        case 0x25: if (selected < rowCount()-1) { commandRow = selected; command = -1; } break;
        case 0x27: if (selected < rowCount()-1) { commandRow = selected; command = 1; } break;
        case 0x0d: case 0x20: commandRow = selected; command = 1; break;
        }
    });
    listeners[3] = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) {
        std::lock_guard lock(mutex); clear();
    });
}
void stop() {
    std::lock_guard lock(mutex);
    close();
    clear();
    for (auto& listener : listeners) {
        if (listener) ll::event::EventBus::getInstance().removeListener(listener);
        listener.reset();
    }
    stopLocalization();
    if (textHook) { SettingsSearchText::unhook(true); textHook = false; }
    if (backgroundHook) { SettingsWorldBackground::unhook(true); backgroundHook = false; }
}
}


#include "ui/SettingsScreen.h"
#include "app/Runtime.h"
#include "features/camera/Zoom.h"
#include "input/Actions.h"
#include "ll/api/event/EventBus.h"
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
#include "mc/client/gui/screens/interfaces/ISceneStack.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/deps/input/MouseAction.h"
#include <array>
#include <format>
#include <mutex>

namespace lamium::ui {
namespace {
std::recursive_mutex mutex;
IClientInstance* client = nullptr;
std::shared_ptr<AbstractScene> scene;
Settings draft;
int selected = 0;
int hovered = -1;
int command = 0;
bool seen = false;
std::string error;
std::array<ll::event::ListenerPtr, 4> listeners;
constexpr mce::Color white{1.0f,1.0f,1.0f,1.0f};

bool ownsTop() {
    return client && scene && client->getSceneFactory().getCurrentSceneStack()->getTopScene() == scene.get();
}
void clear() { client = nullptr; scene.reset(); seen = false; command = 0; hovered = -1; }
void close() {
    if (ownsTop()) client->getSceneFactory().getCurrentSceneStack()->schedulePopScreen(1);
    clear();
}
void activate(int direction) {
    switch (selected) {
    case 0: draft.camera.zoom = !draft.camera.zoom; break;
    case 1: draft.camera.magnification += direction * .5f; break;
    case 2: draft.camera.wheelStep += direction * .1f; break;
    case 3:
        if (Runtime::instance().save(draft)) close();
        else error = "Could not save settings. Please try again.";
        break;
    case 4: close(); break;
    }
    draft.normalize();
}
void label(MinecraftUIRenderContext& context, float x, float y, float width, std::string text) {
    auto& font = context.mClient.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default").getFont();
    TextMeasureData const measure{1.0f, 0.0f, true, false, false, ::ui::TextAlignment::Left};
    CaretMeasureData const caret{-1, false};
    context.drawText(font, RectangleArea{x,x+width,y,y+14}, std::move(text), white, 1.0f,
        ::ui::TextAlignment::Left, measure, caret);
}
void render(ll::event::AfterUIRenderEvent& event) {
    std::lock_guard lock(mutex);
    auto& context = event.uiRenderContext();
    auto& current = context.mClient;
    auto& view = event.screenView();
    glm::vec2 size = view.mSize;
    if (!scene) {
        if (gameplayScreen(current.getScreenName())) {
            label(context, 6, 6, size.x-12, "Lamium: F8 settings | Hold C to zoom");
            context.flushText(0, std::nullopt);
        }
        return;
    }
    if (&current != client) return;
    if (!ownsTop()) { if (seen) clear(); return; }
    seen = true;
    int action = std::exchange(command, 0);
    if (action == 1) activate(1);
    if (action == -1) activate(-1);
    if (action == 2) close();
    if (!scene) return;
    float width = std::min(330.0f, size.x-16);
    float left = (size.x-width)*.5f;
    float top = std::max(8.0f, (size.y-190)*.5f);
    context.fillRectangle(RectangleArea{0,size.x,0,size.y}, mce::Color{.07f,.08f,.11f,1.0f}, 1);
    context.flushImages(white,1,HashedString{"ui_fillColor"});
    label(context,left,top,width,"Lamium / Camera");
    label(context,left,top+18,width,"Local settings - no server installation required");
    std::array<std::string,5> rows{
        std::string{"Zoom: "} + (draft.camera.zoom ? "On" : "Off"),
        std::format("Magnification: {:.1f}x", draft.camera.magnification),
        std::format("Wheel step: {:.1f}", draft.camera.wheelStep),
        "Save and close", "Cancel"
    };
    glm::vec2 pointer = view.mPointerLocationPrevious;
    hovered = -1;
    for (int i=0;i<5;++i) {
        float y = top+42+i*22.0f;
        if (pointer.x >= left && pointer.x <= left+width && pointer.y >= y && pointer.y < y+20) hovered = i;
        bool highlight = selected == i || hovered == i;
        context.fillRectangle(RectangleArea{left,left+width,y,y+20},
            highlight ? mce::Color{.28f,.24f,.43f,1.0f} : mce::Color{.15f,.16f,.21f,1.0f},1);
        context.flushImages(white,1,HashedString{"ui_fillColor"});
        label(context,left+6,y+5,width-12,rows[i]);
    }
    label(context,left,top+158,width,"Up/Down: select | Left/Right: adjust | Enter: apply");
    label(context,left,top+173,width,error.empty() ? "Click to increase/toggle. Esc cancels. Keys: Minecraft settings." : error);
    context.flushText(0,std::nullopt);
}
}
void open(IClientInstance& current) {
    std::lock_guard lock(mutex);
    if (scene || !gameplayScreen(current.getScreenName())) return;
    Zoom::instance().reset();
    draft = Runtime::instance().preferences();
    selected = 0; hovered = -1; command = 0; error.clear(); seen = false;
    // This native information screen supplies focus/cursor ownership. It has no
    // form ID, packet, or server callback. Lamium draws and handles its own UI.
    scene = current.getSceneFactory().createCommonDialogInfoScreen("Lamium", "");
    if (!scene) return;
    client = &current;
    current.getSceneFactory().getCurrentSceneStack()->pushScreen(scene, false);
}
void start() {
    auto& bus = ll::event::EventBus::getInstance();
    listeners[0] = bus.emplaceListener<ll::event::AfterUIRenderEvent>(render);
    listeners[1] = bus.emplaceListener<ll::event::input::MouseInputEvent>([](auto& event) {
        std::lock_guard lock(mutex);
        if (!ownsTop()) return;
        if (event.actionButtonId() == MouseAction::ActionMove || event.actionButtonId() == MouseAction::ActionMoveRelative) return;
        event.cancel();
        if (event.actionButtonId() == MouseAction::ActionLeft && event.buttonData() == MouseAction::DataDown && hovered >= 0) {
            selected = hovered; command = 1;
        }
    });
    listeners[2] = bus.emplaceListener<ll::event::input::KeyInputEvent>([](auto& event) {
        std::lock_guard lock(mutex);
        if (!ownsTop()) return;
        // Let key-up through so keys pressed before opening cannot stick.
        if (!event.isDown()) return;
        event.cancel();
        switch (event.keyCode()) {
        case 0x1b: command = 2; break;
        case 0x26: selected = (selected+4)%5; break;
        case 0x09: case 0x28: selected = (selected+1)%5; break;
        case 0x25: if (selected < 3) command = -1; break;
        case 0x27: if (selected < 3) command = 1; break;
        case 0x0d: case 0x20: command = 1; break;
        }
    });
    listeners[3] = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) {
        std::lock_guard lock(mutex); clear();
    });
}
void stop() {
    std::lock_guard lock(mutex);
    close();
    for (auto& listener : listeners) {
        if (listener) ll::event::EventBus::getInstance().removeListener(listener);
        listener.reset();
    }
}
}


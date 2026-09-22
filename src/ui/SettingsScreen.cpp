#include "ui/SettingsScreen.h"
#include "ui/SettingsLayout.h"
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
int commandRow = 0;
int firstVisible = 0;
bool seen = false;
bool closing = false;
constexpr int rowCount = 10;
std::string error;
std::array<ll::event::ListenerPtr, 4> listeners;
constexpr mce::Color white{1.0f,1.0f,1.0f,1.0f};

bool ownsTop() {
    return client && scene && client->getSceneFactory().getCurrentSceneStack()->getTopScene() == scene.get();
}
void clear() { client = nullptr; scene.reset(); seen = false; closing = false; command = 0; hovered = -1; }
void close() {
    if (ownsTop()) {
        if (!closing) client->getSceneFactory().getCurrentSceneStack()->schedulePopScreen(1);
        closing = true;
    } else clear();
}
void activate(int row, int direction) {
    switch (row) {
    case 0: draft.camera.zoom = !draft.camera.zoom; break;
    case 1: draft.camera.magnification += direction * .5f; break;
    case 2: draft.camera.wheelStep += direction * .1f; break;
    case 3: draft.lighting.nightVision = !draft.lighting.nightVision; break;
    case 4: draft.inspection.containerPreviews = !draft.inspection.containerPreviews; break;
    case 5: draft.inspection.durability = !draft.inspection.durability; break;
    case 6: draft.inventory.sorting = !draft.inventory.sorting; break;
    case 7: draft.inventory.sortContainers = !draft.inventory.sortContainers; break;
    case 8:
        if (Runtime::instance().save(draft)) close();
        else error = "Could not save settings. Please try again.";
        break;
    case 9: close(); break;
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
    auto layout = SettingsLayout::fit(size.x, size.y, rowCount, selected, firstVisible);
    firstVisible = layout.first;
    float width = layout.width, left = layout.left, top = layout.top;
    context.fillRectangle(RectangleArea{0,size.x,0,size.y}, mce::Color{.07f,.08f,.11f,1.0f}, 1);
    context.flushImages(white,1,HashedString{"ui_fillColor"});
    if (!layout.visible) {
        hovered = -1;
        label(context, 4, 4, std::max(1.0f, size.x - 8), "Enlarge window | Esc: cancel");
        context.flushText(0, std::nullopt);
        return;
    }
    label(context,left,top,width,"Lamium / Settings");
    if (layout.subtitle) label(context,left,top+18,width,"Camera, lighting, items and inventory");
    std::array<std::string,rowCount> rows{
        std::string{"Zoom: "} + (draft.camera.zoom ? "On" : "Off"),
        std::format("Magnification: {:.1f}x", draft.camera.magnification),
        std::format("Wheel step: {:.1f}", draft.camera.wheelStep),
        std::string{"NightVision: "} + (draft.lighting.nightVision ? "On" : "Off"),
        std::string{"Container previews: "} + (draft.inspection.containerPreviews ? "On" : "Off"),
        std::string{"Durability: "} + (draft.inspection.durability ? "On" : "Off"),
        std::string{"Inventory sorting: "} + (draft.inventory.sorting ? "On" : "Off"),
        std::string{"Sort storage containers: "} + (draft.inventory.sortContainers ? "On" : "Off"),
        "Save and close", "Cancel"
    };
    glm::vec2 pointer = view.mPointerLocationPrevious;
    hovered = layout.hit(pointer.x, pointer.y);
    for (int i=layout.first;i<layout.first+layout.visible;++i) {
        float y = layout.rowY(i);
        context.fillRectangle(RectangleArea{left,left+width,y,y+20},
            selected == i ? mce::Color{.28f,.24f,.43f,1.0f}
                : hovered == i ? mce::Color{.22f,.23f,.30f,1.0f} : mce::Color{.15f,.16f,.21f,1.0f},1);
        context.flushImages(white,1,HashedString{"ui_fillColor"});
        label(context,left+6,y+5,width-12,rows[i]);
    }
    label(context,left,layout.footer,width,error.empty() ? "Arrows / wheel: navigate | Enter | Esc" : error);
    if (layout.secondHint)
        label(context,left,layout.footer+15,width,"Left click: increase/toggle | Right: decrease");
    context.flushText(0,std::nullopt);
}
}
void open(IClientInstance& current) {
    std::lock_guard lock(mutex);
    if (scene || !gameplayScreen(current.getScreenName())) return;
    Zoom::instance().reset();
    draft = Runtime::instance().preferences();
    selected = 0; hovered = -1; command = 0; error.clear(); seen = false; closing = false;
    firstVisible = 0; commandRow = 0;
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
        if (event.actionButtonId() == MouseAction::ActionWheel && event.buttonData() != 0) {
            selected = std::clamp(selected + (event.buttonData() > 0 ? -1 : 1), 0, rowCount-1);
        }
        if (event.actionButtonId() == MouseAction::ActionLeft && event.buttonData() == MouseAction::DataDown && hovered >= 0) {
            selected = hovered; commandRow = hovered; command = 1;
        }
        if (event.actionButtonId() == MouseAction::ActionRight && event.buttonData() == MouseAction::DataDown && hovered >= 0 && hovered < rowCount-2) {
            selected = hovered; commandRow = hovered; command = -1;
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
        case 0x26: selected = (selected+rowCount-1)%rowCount; break;
        case 0x09: case 0x28: selected = (selected+1)%rowCount; break;
        case 0x25: if (selected < rowCount-2) { commandRow = selected; command = -1; } break;
        case 0x27: if (selected < rowCount-2) { commandRow = selected; command = 1; } break;
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
}
}


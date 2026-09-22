#include "input/Actions.h"
#include "app/Runtime.h"
#include "features/camera/Zoom.h"
#include "features/inventory/Inventory.h"
#include "ui/SettingsScreen.h"
#include "ui/Localization.h"
#include "ll/api/input/KeyRegistry.h"
#include "mc/client/input/KeyboardRemappingLayout.h"
#include "mc/client/options/IOptionRegistry.h"

namespace lamium {
namespace {
bool usesNative(input::Action action) {
    return !Runtime::instance().preferences().bindings[static_cast<size_t>(action)];
}
}
std::string bindingChordName(IClientInstance& client, input::Chord const& chord) {
    auto layout = client.getOptions().getCurrentKeyboardRemapping();
    if (chord.empty()) return ui::translated("unbound");
    std::string result;
    for (auto token : chord) {
        if (!result.empty()) result += " + ";
        if (token.device == input::Device::Wheel) result += ui::translated(token.code > 0 ? "wheelUp" : "wheelDown");
        else if (token.device == input::Device::Mouse) result += ui::translated("mouseButton", token.code);
        else result += layout ? layout->getMappedKeyName(token.code) : std::to_string(token.code);
    }
    return result;
}
std::string actionBindingName(IClientInstance& client, input::Action action) {
    auto layout = client.getOptions().getCurrentKeyboardRemapping();
    auto override = Runtime::instance().preferences().bindings[static_cast<size_t>(action)];
    if (override) return bindingChordName(client, *override);
    if (!layout) return ui::translated("unbound");
    auto const& mapping = layout->getKeymappingByAction("key.Lamium." + std::string(input::actions[static_cast<size_t>(action)].id));
    if (!mapping.isAssigned()) return ui::translated("unbound");
    return static_cast<RemappingLayout const&>(*layout).getMappedKeyName(mapping);
}
std::string gameplayKeyHint(IClientInstance& client) {
    auto layout = client.getOptions().getCurrentKeyboardRemapping();
    if (!layout) return ui::translated("controls");
    return ui::translated("gameplay",
        actionBindingName(client, input::Action::Settings), actionBindingName(client, input::Action::Zoom),
        actionBindingName(client, input::Action::NightVision));
}

void registerActions() {
    auto& offhand = ll::input::KeyRegistry::getInstance().getOrCreateKey("hideoffhand", {});
    offhand.registerButtonDownHandler([](FocusImpact, IClientInstance& client) {
        if (!usesNative(input::Action::HideOffhand) || ui::ownsInput()) return;
        auto& runtime = Runtime::instance();
        if (!runtime.enabled() || !gameplayScreen(client.getScreenName())) return;
        auto settings = runtime.preferences();
        settings.visuals.hideOffhand = !settings.visuals.hideOffhand;
        if (!runtime.save(settings)) runtime.self().getLogger().error("Could not save offhand visibility setting");
    });
    // Optional overlays start unbound; users choose a key in either settings UI.
    auto& borders = ll::input::KeyRegistry::getInstance().getOrCreateKey("chunkborders", {});
    borders.registerButtonDownHandler([](FocusImpact, IClientInstance& client) {
        if (!usesNative(input::Action::ChunkBorders) || ui::ownsInput()) return;
        auto& runtime = Runtime::instance();
        if (!runtime.enabled() || !gameplayScreen(client.getScreenName())) return;
        auto settings = runtime.preferences();
        settings.overlays.chunkBorders = !settings.overlays.chunkBorders;
        if (!runtime.save(settings)) runtime.self().getLogger().error("Could not save Chunk Borders setting");
    });
    auto& sort = ll::input::KeyRegistry::getInstance().getOrCreateKey("sort", {0x52});
    sort.registerButtonDownHandler([](FocusImpact, IClientInstance& client) {
        if (usesNative(input::Action::Sort) && !ui::ownsInput()) inventory::requestSort(client);
    });
    // N is Minecraft's notification shortcut; avoid clearing either binding
    // when Minecraft resolves duplicate keys after a remap.
    auto& nightVision = ll::input::KeyRegistry::getInstance().getOrCreateKey("nightvision", {0x4a});
    nightVision.registerButtonDownHandler([](FocusImpact, IClientInstance& client) {
        if (!usesNative(input::Action::NightVision) || ui::ownsInput()) return;
        auto& runtime = Runtime::instance();
        if (!runtime.enabled() || !gameplayScreen(client.getScreenName())) return;
        auto settings = runtime.preferences();
        settings.lighting.nightVision = !settings.lighting.nightVision;
        if (!runtime.save(settings)) runtime.self().getLogger().error("Could not save NightVision setting");
    });
    auto& settings = ll::input::KeyRegistry::getInstance().getOrCreateKey("settings", {0x77});
    settings.registerButtonDownHandler([](FocusImpact, IClientInstance& client) {
        if (Runtime::instance().enabled() && usesNative(input::Action::Settings)) ui::open(client);
    });
    auto& zoom = ll::input::KeyRegistry::getInstance().getOrCreateKey("zoom", {0x43});
    zoom.registerButtonDownHandler([](FocusImpact, IClientInstance& client) {
        if (Runtime::instance().enabled() && usesNative(input::Action::Zoom) && !ui::ownsInput()) Zoom::instance().press(client);
    });
    zoom.registerButtonUpHandler([](FocusImpact, IClientInstance&) {
        if (usesNative(input::Action::Zoom)) Zoom::instance().release();
    });
}
}

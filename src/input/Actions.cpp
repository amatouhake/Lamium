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
std::string gameplayKeyHint(IClientInstance& client) {
    auto layout = client.getOptions().getCurrentKeyboardRemapping();
    if (!layout) return ui::translated("controls");
    auto keyName = [&](std::string_view action) {
        auto const& mapping = layout->getKeymappingByAction("key.Lamium." + std::string(action));
        if (!mapping.isAssigned()) return ui::translated("unbound");
        // Use the live remapping, not KeyHandle's original default key codes.
        return static_cast<RemappingLayout const&>(*layout).getMappedKeyName(mapping);
    };
    return ui::translated("gameplay",
        keyName("settings"), keyName("zoom"), keyName("nightvision"));
}

void registerActions() {
    auto& sort = ll::input::KeyRegistry::getInstance().getOrCreateKey("sort", {0x52});
    sort.registerButtonDownHandler([](FocusImpact, IClientInstance& client) { inventory::requestSort(client); });
    // N is Minecraft's notification shortcut; avoid clearing either binding
    // when Minecraft resolves duplicate keys after a remap.
    auto& nightVision = ll::input::KeyRegistry::getInstance().getOrCreateKey("nightvision", {0x4a});
    nightVision.registerButtonDownHandler([](FocusImpact, IClientInstance& client) {
        auto& runtime = Runtime::instance();
        if (!runtime.enabled() || !gameplayScreen(client.getScreenName())) return;
        auto settings = runtime.preferences();
        settings.lighting.nightVision = !settings.lighting.nightVision;
        if (!runtime.save(settings)) runtime.self().getLogger().error("Could not save NightVision setting");
    });
    auto& settings = ll::input::KeyRegistry::getInstance().getOrCreateKey("settings", {0x77});
    settings.registerButtonDownHandler([](FocusImpact, IClientInstance& client) {
        if (Runtime::instance().enabled()) ui::open(client);
    });
    auto& zoom = ll::input::KeyRegistry::getInstance().getOrCreateKey("zoom", {0x43});
    zoom.registerButtonDownHandler([](FocusImpact, IClientInstance& client) {
        if (Runtime::instance().enabled()) Zoom::instance().press(client);
    });
    zoom.registerButtonUpHandler([](FocusImpact, IClientInstance&) { Zoom::instance().release(); });
}
}

#include "input/Actions.h"
#include "app/Runtime.h"
#include "features/camera/Zoom.h"
#include "features/inventory/Inventory.h"
#include "ui/SettingsScreen.h"
#include "ll/api/input/KeyRegistry.h"

namespace lamium {
void registerActions() {
    auto& sort = ll::input::KeyRegistry::getInstance().getOrCreateKey("sort", {0x52});
    sort.registerButtonDownHandler([](FocusImpact, IClientInstance& client) { inventory::requestSort(client); });
    auto& nightVision = ll::input::KeyRegistry::getInstance().getOrCreateKey("nightvision", {0x4e});
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

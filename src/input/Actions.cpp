#include "input/Actions.h"
#include "app/Runtime.h"
#include "features/camera/Zoom.h"
#include "ui/SettingsScreen.h"
#include "ll/api/input/KeyRegistry.h"

namespace lamium {
void registerActions() {
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

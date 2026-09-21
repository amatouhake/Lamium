#include "input/Actions.h"
#include "app/Runtime.h"
#include "features/camera/Zoom.h"
#include "ll/api/input/KeyRegistry.h"

namespace lamium {
void registerActions() {
    auto& zoom = ll::input::KeyRegistry::getInstance().getOrCreateKey("zoom", {0x43});
    zoom.registerButtonDownHandler([](FocusImpact, IClientInstance& client) {
        if (Runtime::instance().enabled()) Zoom::instance().press(client);
    });
    zoom.registerButtonUpHandler([](FocusImpact, IClientInstance&) { Zoom::instance().release(); });
}
}

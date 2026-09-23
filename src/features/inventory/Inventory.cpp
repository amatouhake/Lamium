#include "features/inventory/Inventory.h"
#include "features/inventory/HandRestock.h"
#include "features/inventory/game/ScreenTracker.h"
#include "features/inventory/game/TextInputTracker.h"
#include "features/inventory/game/SortSession.h"
#include "features/inventory/game/RequestTracker.h"
#include "features/inventory/game/RestockTrace.h"
#include "app/Runtime.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/world/item/registry/CreativeItemRegistry.h"
#include "mc/world/item/registry/ItemRegistryRef.h"

namespace lamium::inventory {
bool start() {
    try {
        game::installRequestTracker();
        game::restockTrace::start();
        restock::start();
        game::TextInputTracker::getInstance().install();
        game::ScreenTracker::getInstance().install();
        return true;
    } catch (std::exception const& error) {
        Runtime::instance().self().getLogger().error("Inventory initialization failed: {}", error.what());
        stop();
        return false;
    }
}
void stop() {
    game::SortSession::cancel("inventory feature stopped");
    restock::stop();
    game::restockTrace::stop();
    game::ScreenTracker::getInstance().uninstall();
    game::TextInputTracker::getInstance().uninstall();
    game::removeRequestTracker();
}
void requestSort(IClientInstance& client) {
    auto& runtime = Runtime::instance();
    auto settings = runtime.preferences();
    if (!runtime.enabled() || !settings.inventory.sorting) return;
    auto& tracker = game::ScreenTracker::getInstance();
    auto controller = tracker.current();
    if (!controller || game::TextInputTracker::getInstance().isEditing(tracker.currentView())) return;
    try {
        auto region = game::SortSession::selectRegion(*controller, settings.inventory.sortContainers);
        if (!region) return;
        CreativeItemRegistry const* registry = nullptr;
        if (auto* level = client.getLevel()) {
            registry = level->getItemRegistry().getCreativeItemRegistry().get();
            if (registry && registry->mCreativeItems->empty()) registry = nullptr;
        }
        game::SortSession::run(*controller, *region, registry, runtime.self().getLogger());
    } catch (std::exception const& error) {
        runtime.self().getLogger().error("Inventory sort stopped: {}", error.what());
    }
}
}

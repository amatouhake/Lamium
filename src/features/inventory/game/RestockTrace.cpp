#include "features/inventory/game/RestockTrace.h"
#ifdef LAMIUM_RESTOCK_TRACE
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/gui/screens/models/ClientInstanceScreenModel.h"
#include "mc/world/containers/managers/controllers/HudContainerManagerController.h"
#include "mc/world/containers/managers/models/ContainerManagerModel.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/ItemStack.h"
#include <atomic>

namespace lamium::inventory::game::restockTrace {
namespace {
bool installed = false;
std::atomic<unsigned> samples{};
void inspect(std::shared_ptr<HudContainerManagerController> const& controller) noexcept {
    try {
        if (!controller) return;
        auto model = controller->mContainerManagerModel.lock();
        if (!model || !model->isClientSide() || samples.fetch_add(1) >= 8) return;
        Player& player = model->mPlayer;
        auto& logger = Runtime::instance().self().getLogger();
        logger.info("Restock HUD trace: collections={} closed={}",controller->mContainers->size(),bool(controller->mContainersClosed));
        unsigned collections = 0;
        for (auto const& [name,unused] : controller->mContainers.get()) {
            if (++collections > 32) break;
            // Only engine collection names and indices, never item names/NBT,
            // account identifiers, addresses or world paths.
            std::string label = name.substr(0,64);
            for (char& c : label) if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z')
                && !(c >= '0' && c <= '9') && c != '_') c = '?';
            int size = controller->getContainerSize(name);
            logger.info("Restock HUD collection: {} size={}",label,size);
            if (size < 1 || size > 36) continue;
            for (int slot = 0; slot < size; ++slot) {
                auto const& stack = controller->getItemStack(name,slot);
                if (stack.isNull() || stack.mCount <= 0) continue;
                int match = -1, matches = 0;
                for (int inventorySlot = 0; inventorySlot < 36; ++inventorySlot) {
                    auto const& candidate = player.getInventory().getItem(inventorySlot);
                    if (!candidate.isNull() && candidate.mCount == stack.mCount && candidate.matchesItem(stack)) {
                        match = inventorySlot;
                        ++matches;
                    }
                }
                // Ambiguous duplicate stacks cannot establish a slot mapping.
                logger.info("Restock HUD slot: {}[{}] inventory={} matches={}",label,slot,matches == 1 ? match : -1,matches);
            }
        }
    } catch (...) {} // Read-only diagnostics must never alter vanilla behavior.
}
LL_TYPE_INSTANCE_HOOK(CreateHud, ll::memory::HookPriority::Normal, ClientInstanceScreenModel,
    &ClientInstanceScreenModel::createHudContainerManagerController, std::shared_ptr<HudContainerManagerController>) {
    auto result = origin();
    inspect(result);
    return result;
}
// L-17: Sort's screen-backed transfers succeed while the HUD controller's
// handlePlaceAmount returns false. Compare the two controllers' closed,
// client-side and simulation flags without touching transfers.
void inspectContext(char const* side, ContainerManagerController& controller, std::atomic<unsigned>& budget) noexcept {
    try {
        if (budget.fetch_add(1) >= 16) return;
        auto model = controller.mContainerManagerModel.lock();
        Runtime::instance().self().getLogger().info("Restock transfer context: side={} closed={} client={} simulation={}",
            side, bool(controller.mContainersClosed), bool(model && model->isClientSide()),
            bool(controller._isContainerSimulationEnabled()));
    } catch (...) {} // Read-only diagnostics must never alter vanilla behavior.
}
std::atomic<unsigned> useSamples{}, screenSamples{};
} // namespace
void inspectUseController(ContainerManagerController& controller) noexcept { inspectContext("hud", controller, useSamples); }
void inspectScreenController(ContainerManagerController& controller) noexcept {
    inspectContext("screen", controller, screenSamples);
}
void start() {
    if (installed) return;
    samples = 0;
    useSamples = 0;
    screenSamples = 0;
    installed = CreateHud::hook(true) == 0;
    if (!installed) throw std::runtime_error("Could not install restock HUD diagnostics");
    Runtime::instance().self().getLogger().warn("Restock HUD diagnostics enabled: first 8 client controllers only; no transfers");
}
void stop() {
    if (installed && CreateHud::unhook(true)) installed = false;
}
}
#else
namespace lamium::inventory::game::restockTrace {
void start() {}
void stop() {}
void inspectUseController(ContainerManagerController&) noexcept {}
void inspectScreenController(ContainerManagerController&) noexcept {}
}
#endif

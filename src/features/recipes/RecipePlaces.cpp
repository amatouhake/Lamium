#include "features/recipes/RecipePlaces.h"
#include "app/Versions.h"
#include "features/schematic/SchematicItems.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/block/actor/BlockActorType.h"
#include "mc/world/level/block/actor/ChestBlockActor.h"
#include <chrono>
#include <string>

namespace lamium::recipes::placesGame {
namespace {
std::optional<Open> last;
void const* lastController = nullptr;
std::chrono::steady_clock::time_point lastAt{};
// The screens of a block's storage, by the names the game gives them (seen with craft_trace, 2026-11-19).
bool storage(std::string const& screen) {
    return screen == "small_chest_screen" || screen == "large_chest_screen" || screen == "barrel_screen" || screen == "shulker_box_screen"
        || screen == "ender_chest_screen";
}
std::optional<Open> read(ScreenView& screen) {
    if (!versionSensitiveAllowed(capabilities::Id::CountedPlaces) || !screen.mController || !screen.mController->_isContainerScreen()) return std::nullopt;
    std::string const name = screen.getScreenName();
    if (!storage(name)) return std::nullopt;
    auto& controller = static_cast<ContainerScreenController&>(*screen.mController);
    auto manager = controller.mContainerManagerController;
    static std::string const items = "container_items";
    // A minecart's chest has an entity and no position.
    if (!manager || manager->mContainersClosed || !manager->hasContainerController(items) || controller.mEntityUniqueID->rawID != -1) return std::nullopt;
    int const size = manager->getContainerSize(items);
    if (size <= 0 || size > 54) return std::nullopt;
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return std::nullopt;
    BlockPos const pos = controller.mBlockPos;
    auto& region = player->getDimensionBlockSource();
    Open out;
    out.block = region.getBlock(pos).getTypeName();
    if (out.block.empty() || out.block.size() > 200 || out.block == "minecraft:air") return std::nullopt;
    out.ender = name == "ender_chest_screen";
    out.spot = {static_cast<int>(player->getDimensionId()), pos.x, pos.y, pos.z};
    if (size == 54) {
        // The other half, as the chest itself knows it; else the one neighbour of the same block.
        if (auto const* actor = static_cast<BlockSource const&>(region).getBlockEntity(pos); actor && actor->mType == BlockActorType::Chest) {
            auto const& chest = static_cast<ChestBlockActor const&>(*actor);
            if (chest.mLargeChestPaired) {
                BlockPos const other = chest.mLargeChestPairedPosition;
                if (other.y == pos.y && std::abs(other.x - pos.x) + std::abs(other.z - pos.z) == 1) out.pair = Spot{out.spot.dimension, other.x, other.y, other.z};
            }
        }
        if (!out.pair) {
            int found = 0;
            for (auto [dx, dz] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}})
                if (region.getBlock(BlockPos{pos.x + dx, pos.y, pos.z + dz}).getTypeName() == out.block)
                    ++found, out.pair = Spot{out.spot.dimension, pos.x + dx, pos.y, pos.z + dz};
            if (found != 1) out.pair.reset();
        }
    }
    std::vector<schematic::items::Carried> stacks;
    for (int i = 0; i < size; ++i) schematic::items::addStack(manager->getItemStack(items, i), stacks, &controller);
    for (auto const& stack : stacks)
        if (stack.name.size() <= 200) addStored(out.items, stack.name, stack.aux, stack.count);
    return out;
}
}
namespace {
ll::event::ListenerPtr renderListener;
std::chrono::steady_clock::time_point bundlesAt{};
}
void start() {
    renderListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) {
        try {
            auto& screen = event.screenView();
            if (!screen.mHasFocus || !screen.mController || !screen.mController->_isContainerScreen()) return;
            auto const now = std::chrono::steady_clock::now();
            if (now - bundlesAt < std::chrono::milliseconds(250)) return;
            bundlesAt = now;
            auto client = ll::service::getClientInstance();
            if (auto* player = client ? client->getLocalPlayer() : nullptr)
                schematic::items::carriedStacks(*player, &static_cast<ContainerScreenController&>(*screen.mController));
        } catch (...) {}
    });
}
void stop() {
    if (renderListener) { ll::event::EventBus::getInstance().removeListener(renderListener); renderListener.reset(); }
}
std::optional<Open> const& open(ScreenView& screen) {
    auto const now = std::chrono::steady_clock::now();
    void const* const controller = screen.mController.get();
    if (controller == lastController && now - lastAt < std::chrono::milliseconds(250)) return last;
    lastController = controller;
    lastAt = now;
    try { last = read(screen); } catch (...) { last.reset(); }
    return last;
}
}

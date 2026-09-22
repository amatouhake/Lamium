#include "features/inventory/ToolSwitch.h"
#include "features/inventory/ToolChoice.h"
#include "app/Runtime.h"
#include "ui/SettingsScreen.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/item/Item.h"
#include <stdexcept>

namespace lamium::inventory::tools {
namespace {
bool installed = false;
void selectTool(Player& player, BlockPos const& pos) {
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.preferences().inventory.toolSwitch || ui::ownsInput()) return;
    auto client = ll::service::getClientInstance();
    if (!client || client->getLocalPlayer() != &player || player.isCreative() || player.isSpectator()) return;
    auto* supplies = player.mInventory.get();
    if (!supplies || supplies->mSelectedContainerId != ContainerID::Inventory) return;
    int selected = supplies->mSelected;
    if (selected < 0 || selected >= 9) return;
    auto const& block = player.getDimensionBlockSource().getBlock(pos);
    bool requiresTool = block.getBlockType().mRequiresCorrectToolForDrops;
    std::array<ToolCandidate,9> candidates;
    for (int slot=0; slot<9; ++slot) {
        auto const& stack = player.getInventory().getItem(slot);
        if (stack.isNull() || !stack.mItem) continue;
        candidates[slot] = {stack.mItem->getDestroySpeed(stack,block),
            !requiresTool || stack.mItem->canDestroySpecial(block)};
    }
    if (auto slot = chooseHotbarTool(candidates,selected))
        supplies->selectSlot(*slot,ContainerID::Inventory);
}
LL_TYPE_INSTANCE_HOOK(ToolSwitchStart, ll::memory::HookPriority::Normal, GameMode,
    &GameMode::$startDestroyBlock, bool, BlockPos const& pos, uchar face, bool& destroyed) {
    try { selectTool(mPlayer,pos); }
    catch (std::exception const& error) {
        static bool reported = false;
        if (!reported) { Runtime::instance().self().getLogger().error("Tool selection failed: {}",error.what()); reported = true; }
    }
    return origin(pos,face,destroyed);
}
}
void start() {
    if (installed) return;
    installed = ToolSwitchStart::hook(true) == 0;
    if (!installed) throw std::runtime_error("Could not install tool switch hook");
}
void stop() {
    if (installed && ToolSwitchStart::unhook(true)) installed = false;
}
}

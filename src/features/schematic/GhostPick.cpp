#include "features/schematic/GhostPick.h"
#include "features/schematic/GhostCommon.h"
#include "features/schematic/GhostRenderer.h"
#include "features/inventory/BringToHand.h"
#include "features/inventory/PickConfirm.h"
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/phys/HitResult.h"
#include "app/Versions.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/block/BlockType.h"
#include <cmath>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace lamium::schematic::ghosts {
namespace {
// The first ghost the pick ray meets in front of what the game picked. On
// the game's own ray (as the target card does, L-222), so the ghost picked
// is the one the card names.
std::optional<GhostAt> pickedGhost(LocalPlayer& player, HitResult const& hit) {
    Vec3 from = hit.mStartPos, way = hit.mRayDir;
    double length = std::sqrt(double(way.x) * way.x + double(way.y) * way.y + double(way.z) * way.z);
    if (!(length > 0) || !std::isfinite(length)) {
        from = player.getEyePos();
        way = player.getViewVector(1.f);
        length = std::sqrt(double(way.x) * way.x + double(way.y) * way.y + double(way.z) * way.z);
        if (!(length > 0)) return std::nullopt;
    }
    double reach = player.getPickRange();
    bool onHit = hit.mType == HitResultType::Tile || hit.mType == HitResultType::Entity;
    if (onHit) {
        double ex = hit.mPos.x - from.x, ey = hit.mPos.y - from.y, ez = hit.mPos.z - from.z;
        reach = std::sqrt(ex * ex + ey * ey + ez * ez) - .05;
    }
    if (!(reach > 0)) return std::nullopt;
    return ghostAlong(player.getDimensionBlockSource(), static_cast<int>(player.getDimensionId()), from.x, from.y, from.z,
                      way.x / length, way.y / length, way.z / length, reach, onHit);
}
// The ghost's block as one item, as the game's own pick would give it: the
// name and data value both paths use (skull kinds and monster eggs differ
// only in the data value).
ItemStack pickedStack(Block const& block) {
    ItemStack stack;
    auto item = block.getBlockType().asItemInstance(block, nullptr);
    if (item.isNull()) return stack;
    try { stack.reinit(item.getTypeName(), 1, item.getAuxValue()); } catch (...) { stack = ItemStack(); }
    return stack;
}
// The pick whose result is still to be read back, and the ghost-pass frame
// it was sent on (L-263).
inventory::PickPending pendingPick;
int pickFrame = 0;
LL_TYPE_INSTANCE_HOOK(GhostPickHook, ll::memory::HookPriority::Normal, LocalPlayer, &LocalPlayer::pickBlock, void,
                      HitResult const& hit, bool withData) {
    try {
        if (auto ghost = pickedGhost(*this, hit); ghost && ghost->block) {
            // The ghost's block or nothing: never the real block behind it.
            // A ghost without an item (fire, water) does nothing, and a
            // failure past this point cannot hand the pick back to vanilla.
            try {
                ItemStack stack = pickedStack(*ghost->block);
                if (!stack.isNull()) {
                    std::string name = stack.getTypeName();
                    short aux = stack.getAuxValue();
                    int slot = inventory::bringToHand(*this, name, aux);
                    // Creative mode gives it when it is not carried, like
                    // vanilla (L-263); not on a game build whose transaction
                    // is unchecked.
                    if (slot < 0 && isCreative() && versionSensitiveAllowed(capabilities::Id::SchematicCreativePick))
                        slot = inventory::giveToHand(*this, stack);
                    if (slot >= 0) pendingPick = inventory::PickPending{slot, pickFrame, std::move(name), aux};
                }
            } catch (...) {}
            return;
        }
    } catch (...) {}
    origin(hit, withData);
}
bool installed = false;
}
void startPick() {
    if (installed) return;
    if (GhostPickHook::hook() != 0) throw std::runtime_error("Could not install the schematic pick block");
    installed = true;
}
void stopPick() {
    pendingPick.clear();
    if (installed && GhostPickHook::unhook()) installed = false;
}
void stepPick(LocalPlayer& player) {
    ++pickFrame;
    if (!pendingPick.due(pickFrame)) return;
    int slot = pendingPick.slot;
    try {
        std::string held;
        short aux = 0;
        if (slot >= 0 && slot < 36) {
            auto const& stack = player.getInventory().getItem(slot);
            if (!stack.isNull() && stack.mItem) {
                held = stack.getTypeName();
                aux = stack.getAuxValue();
            }
        }
        if (!inventory::pickConfirmed(held, aux, pendingPick.name, pendingPick.aux))
            log(std::format("pick not confirmed: slot {} wanted {} {} holds {}", slot, pendingPick.name, pendingPick.aux,
                            held.empty() ? "nothing" : std::format("{} {}", held, aux)));
    } catch (...) {}
    pendingPick.clear();
}
}

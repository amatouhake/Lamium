#include "features/schematic/GhostPick.h"
#include "features/schematic/GhostCommon.h"
#include "features/schematic/GhostRenderer.h"
#include "features/schematic/PickRay.h"
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
#ifdef LAMIUM_PICK_TRACE
#include "app/TraceLog.h"
#endif
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
    // L-285: when the view ray meets nothing the game picks the block under
    // the feet as an indirect hit with its own ray, straight down from the
    // player (trace 2026-11-01). That ray is not the aim: search the view.
    if (hit.mIndirectHit || !(length > 0) || !std::isfinite(length)) {
        from = player.getEyePos();
        way = player.getViewVector(1.f);
        length = std::sqrt(double(way.x) * way.x + double(way.y) * way.y + double(way.z) * way.z);
        if (!(length > 0)) return std::nullopt;
    }
    // L-285: only a hit on the ray is something the player aimed at, and
    // only that hit shortens the search; taking the feet fallback for a hit
    // put the ghost behind the feet block and the pick brought the feet block.
    bool hitReal = hit.mType == HitResultType::Tile || hit.mType == HitResultType::Entity;
    bool aimed = hitReal && !hit.mIndirectHit && onRay(from.x, from.y, from.z, way.x, way.y, way.z, hit.mPos.x, hit.mPos.y, hit.mPos.z,
                                  pickRayTolerance);
    double hitDistance = 0;
    if (aimed) {
        double ex = hit.mPos.x - from.x, ey = hit.mPos.y - from.y, ez = hit.mPos.z - from.z;
        hitDistance = std::sqrt(ex * ex + ey * ey + ez * ez);
    }
    double reach = ghostReach(aimed, hitDistance, player.getPickRange());
    if (!(reach > 0)) return std::nullopt;
    // `throughBlocks` says nothing solid lies within `reach`: when the game's
    // own pick stopped at the hit we shortened for, and when its block hit is
    // the feet fallback (its ray met nothing) — a block cell the walk grazes
    // there is only the walk (L-224). No hit at all, or an entity off the ray,
    // proves nothing about the blocks, so a real block still stops the search.
    bool throughBlocks = aimed || hit.mType == HitResultType::Tile;
    auto ghost = ghostAlong(player.getDimensionBlockSource(), static_cast<int>(player.getDimensionId()), from.x, from.y,
                            from.z, way.x / length, way.y / length, way.z / length, reach, throughBlocks);
#ifdef LAMIUM_PICK_TRACE
    {
        // L-285: what the game hands the pick when the feet block wins.
        static TraceBudget budget;
        Vec3 eye = player.getEyePos(), view = player.getViewVector(1.f);
        traceLog(budget, 64,
            "L-285 pick type={} block={},{},{} pos={:.3f},{:.3f},{:.3f} start={:.3f},{:.3f},{:.3f} dir={:.3f},{:.3f},{:.3f} "
            "indirect={} liquid={} eye={:.3f},{:.3f},{:.3f} view={:.3f},{:.3f},{:.3f} aimed={} reach={:.2f} through={} ghost={}",
            static_cast<int>(hit.mType), hit.mBlock.x, hit.mBlock.y, hit.mBlock.z, hit.mPos.x, hit.mPos.y, hit.mPos.z,
            hit.mStartPos.x, hit.mStartPos.y, hit.mStartPos.z, hit.mRayDir.x, hit.mRayDir.y, hit.mRayDir.z,
            hit.mIndirectHit, hit.mIsHitLiquid, eye.x, eye.y, eye.z, view.x, view.y, view.z, aimed, reach, throughBlocks,
            ghost ? std::format("{},{},{}", ghost->cell.x, ghost->cell.y, ghost->cell.z) : std::string("none"));
    }
#endif
    return ghost;
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

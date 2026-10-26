#include "features/schematic/GhostPick.h"
#include "features/schematic/GhostCommon.h"
#include "features/schematic/GhostRenderer.h"
#include "features/inventory/BringToHand.h"
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/phys/HitResult.h"
#include <cmath>
#ifdef LAMIUM_SCHEMATIC_CREATIVE_PICK
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include <chrono>
#include <format>
#include <string>
#endif
#include <optional>
#include <stdexcept>

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
#ifdef LAMIUM_SCHEMATIC_CREATIVE_PICK
// L-263 experiment: the last creative give, looked at again on the next
// pick so the log shows whether the server kept it.
struct Given { int slot = -1; std::string item; std::chrono::steady_clock::time_point at; };
Given lastGiven;
void reportGiven(LocalPlayer& player) {
    if (lastGiven.slot < 0) return;
    auto const& now = player.getInventory().getItem(lastGiven.slot);
    auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - lastGiven.at).count();
    log(std::format("creative pick: slot {} sent {}, now holds {} ({:.1f} s later)", lastGiven.slot, lastGiven.item,
                    now.isNull() || !now.mItem ? std::string("nothing") : now.getTypeName(), seconds));
    lastGiven = {};
}
// The ghost's block as one item, as the game's own pick would give it.
ItemStack pickedStack(Block const& block) {
    ItemStack stack;
    auto item = block.getBlockType().asItemInstance(block, nullptr);
    if (item.isNull()) return stack;
    try { stack.reinit(item.getTypeName(), 1, item.getAuxValue()); } catch (...) { stack = ItemStack(); }
    return stack;
}
#endif
LL_TYPE_INSTANCE_HOOK(GhostPickHook, ll::memory::HookPriority::Normal, LocalPlayer, &LocalPlayer::pickBlock, void,
                      HitResult const& hit, bool withData) {
    try {
#ifdef LAMIUM_SCHEMATIC_CREATIVE_PICK
        reportGiven(*this);
#endif
        if (auto ghost = pickedGhost(*this, hit); ghost && ghost->block)
            if (auto item = describe(*ghost->block, "").item; !item.empty()) {
                // The ghost's block or nothing: never the real block behind it.
#ifdef LAMIUM_SCHEMATIC_CREATIVE_PICK
                // Creative mode gives it when it is not carried, like vanilla.
                if (!inventory::bringToHand(*this, item) && isCreative())
                    if (auto stack = pickedStack(*ghost->block); !stack.isNull()) {
                        auto* supplies = mInventory.get();
                        int before = supplies ? supplies->mSelected : -1;
                        int slot = inventory::giveToHand(*this, stack);
                        if (slot == inventory::notListed) log(std::format("creative pick: {} skipped: not in the creative inventory", item));
                        else if (slot < 0) log(std::format("creative pick: {} not sent (no slot, or another move in progress)", item));
                        else log(std::format("creative pick: {} to slot {} (selected before: {})", item, slot, before));
                        if (slot >= 0) lastGiven = {slot, item, std::chrono::steady_clock::now()};
                    }
#else
                inventory::bringToHand(*this, item);
#endif
                return;
            }
#ifdef LAMIUM_SCHEMATIC_CREATIVE_PICK
    } catch (std::exception const& error) {
        log(std::string("creative pick failed: ") + error.what());
#endif
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
    if (installed && GhostPickHook::unhook()) installed = false;
}
}

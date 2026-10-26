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
LL_TYPE_INSTANCE_HOOK(GhostPickHook, ll::memory::HookPriority::Normal, LocalPlayer, &LocalPlayer::pickBlock, void,
                      HitResult const& hit, bool withData) {
    try {
        if (auto ghost = pickedGhost(*this, hit); ghost && ghost->block)
            if (auto item = describe(*ghost->block, "").item; !item.empty()) {
                // The ghost's block or nothing: never the real block behind it.
                inventory::bringToHand(*this, item);
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
    if (installed && GhostPickHook::unhook()) installed = false;
}
}

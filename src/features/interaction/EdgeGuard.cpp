#include "features/interaction/EdgeGuard.h"
#include "features/interaction/EdgeGuardPlan.h"
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"
#include "mc/deps/ecs/gamerefs_entity/EntityRegistry.h"
#include "mc/deps/ecs/strict/StrictEntityContext.h"
#include "mc/deps/vanilla_components/AABBShapeComponent.h"
#include "mc/deps/vanilla_components/IConstBlockSource.h"
#include "mc/deps/vanilla_components/MoveRequestComponent.h"
#include "mc/entity/systems/move_collision_system/MoveCollisionSystem.h"
#include "mc/world/phys/AABB.h"
#include <cmath>
#include <stdexcept>
#include <vector>

namespace lamium::interaction::edgeGuard {
namespace {
// Drops up to this height (stairs, slabs, a carpet) stay walkable, as with
// vanilla sneaking.
constexpr float stepHeight = 0.6f;
bool installed = false;
bool reportedServerCopy = false;

bool isLocal(StrictEntityContext const& entity, LocalPlayer& player) {
    auto const& own = player.getEntityContext();
    return static_cast<EntityId const&>(entity.mEntity) == own.mEntity
        && static_cast<uint const&>(entity.mRegistryId) == static_cast<uint const&>(own.mRegistry.mId);
}
void guard(StrictEntityContext const& entity, AABBShapeComponent const& shape, MoveRequestComponent& request,
           IConstBlockSource const& region, GetCollisionShapeInterface const& collisionShape) {
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.preferences().interaction.edgeGuard) return;
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return;
    // In a local world the integrated server moves its own copy of this
    // player and corrects the client to it, so that copy is guarded too. It
    // is the other registry's entity whose box sits where the player's does.
    if (!isLocal(entity, *player)) {
        auto const& own = player->getAABB();
        auto const& other = shape.mAABB.get();
        if (std::abs(own.min.x - other.min.x) > .3f || std::abs(own.min.y - other.min.y) > .3f
            || std::abs(own.min.z - other.min.z) > .3f || std::abs((own.max.x - own.min.x) - (other.max.x - other.min.x)) > .05f)
            return;
        if (!reportedServerCopy) {
            reportedServerCopy = true;
            runtime.self().getLogger().info("Edge Guard: also guarding the local server's copy of the player");
        }
    }
    // Only walking on the ground; vanilla sneaking already guards edges.
    if (!player->isOnGround() || player->isSneaking() || player->isFlying() || player->isGliding()
        || player->isSwimming() || player->isInWater() || player->getVehicle()) return;
    Vec3& speed = request.mSpeed;
    if (speed.y > 0) return; // A jump may leave the edge on purpose.
    AABB const& box = shape.mAABB;
    std::vector<AABB> shapes;
    auto supported = [&](double dx, double dz) {
        AABB probe{Vec3{box.min.x + static_cast<float>(dx), box.min.y - stepHeight, box.min.z + static_cast<float>(dz)},
                   Vec3{box.max.x + static_cast<float>(dx), box.min.y, box.max.z + static_cast<float>(dz)}};
        shapes.clear();
        region.fetchCollisionShapes(shapes, probe, false, collisionShape, nullptr);
        return !shapes.empty();
    };
    if (!supported(0, 0)) return; // Already over a drop: do not freeze in the air.
    auto [dx, dz] = guardEdge(speed.x, speed.z, supported);
    speed.x = static_cast<float>(dx);
    speed.z = static_cast<float>(dz);
}
LL_STATIC_HOOK(EdgeGuardHook, ll::memory::HookPriority::Normal, &MoveCollisionSystem::fetchCollisionShapes, void,
    StrictEntityContext const& entity, AABBShapeComponent const& aabb, MaxAutoStepComponent const& autoStep,
    Optional<CollidableMobNearFlagComponent const> collidableMobNear, MoveRequestComponent& request,
    Optional<MinecartFlagComponent const> isMinecart,
    ViewT<StrictEntityContext, Include<CollidableMobFlagComponent>, AABBShapeComponent const> const& collidableMobs,
    ViewT<StrictEntityContext, AABBShapeComponent const, ActorDataFlagComponent const> const& stackableView,
    ViewT<StrictEntityContext, Include<FallingBlockFlagComponent>> const& fallingBlocks, IConstBlockSource const& region,
    LocalSpatialEntityFetcher& fetcher, GetCollisionShapeInterface const& collisionShape,
    std::vector<BlockSourceVisitor::CollisionShape>& tempCollisionShapes,
    std::vector<BlockSourceVisitor::CollisionShape>& scratchCollisionShapes, std::vector<AABB>& tempShapes) {
    try { guard(entity, aabb, request, region, collisionShape); } catch (...) {}
    origin(entity, aabb, autoStep, collidableMobNear, request, isMinecart, collidableMobs, stackableView, fallingBlocks,
        region, fetcher, collisionShape, tempCollisionShapes, scratchCollisionShapes, tempShapes);
}
}
void start() {
    if (installed) return;
    if (EdgeGuardHook::hook(true) != 0) throw std::runtime_error("Could not install the edge guard hook");
    installed = true;
}
void stop() {
    if (installed && EdgeGuardHook::unhook(true)) installed = false;
}
}

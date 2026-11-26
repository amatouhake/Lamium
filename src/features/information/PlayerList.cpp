#include "features/information/PlayerList.h"
#include "features/map/RadarFaces.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/player/PlayerListEntry.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/PlayerLocationReceiver.h"
#include "mc/world/actor/player/LayeredAbilities.h"
#include "mc/world/actor/player/PermissionsHandler.h"
#include <atomic>
#include <unordered_map>

namespace lamium::information::playerList {
namespace {
std::atomic<bool> keyHeld{false};
// Where each player was last seen (L-131): the server sends positions only for
// players in your dimension, and hides them when either of you leaves it.
// Kept per world, client thread only.
Level const* seenIn = nullptr;
std::unordered_map<std::int64_t, int> lastDimension;
double distanceBetween(Vec3 const& a, Vec3 const& b) {
    double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
}
void setHeld(bool value) { keyHeld = value; }
bool held() { return keyHeld.load(); }
// On world exit, client thread: the next world's Level may reuse the address.
void forget() {
    seenIn = nullptr;
    lastDimension.clear();
}

std::vector<Row> collect(IClientInstance& client) {
    std::vector<Row> rows;
    auto* self = client.getLocalPlayer();
    if (!self) return rows;
    auto& level = self->getLevel();
    map::faces::frame();
    int dimension = static_cast<int>(self->getDimensionId());
    auto const selfId = self->getOrCreateUniqueID();
    auto const here = self->getPosition();
    if (seenIn != &level) {
        seenIn = &level;
        lastDimension.clear();
    }
    // Loaded players share your dimension: the client holds only its own.
    std::unordered_map<std::int64_t, Vec3> loaded;
    for (auto* actor : level.getRuntimeActorList())
        if (actor && actor != self && actor->hasType(ActorType::Player)) loaded[actor->getOrCreateUniqueID().rawID] = actor->getPosition();
    // Locator Bar positions (L-89): sent only for players in your dimension.
    std::unordered_map<std::int64_t, Vec3> located;
    if (auto receiver = level.getPlayerLocationReceiver())
        for (auto const& [id, at] : *receiver->mCurrentPlayerLocationData)
            if (at) located[id.rawID] = *at;
    for (auto const& [uuid, entry] : level.getPlayerList()) {
        Row row;
        row.name = *entry.mName;
        row.host = entry.mIsHost;
        row.platform = static_cast<int>(static_cast<BuildPlatform>(entry.mBuildPlatform));
        row.face = map::faces::headOf(*entry.mSkin);
        auto id = entry.mId->rawID;
        row.self = id == selfId.rawID;
        if (auto* abilities = level.getPlayerAbilities(*entry.mId))
            row.permission = static_cast<int>(static_cast<PlayerPermissionLevel>(abilities->mPermissions->mPlayerPermissions));
        if (row.self) {
            row.dimensionCurrent = true;
        } else if (auto found = loaded.find(id); found != loaded.end()) {
            row.dimensionCurrent = true;
            row.distance = distanceBetween(found->second, here);
        } else if (auto spot = located.find(id); spot != located.end()) {
            row.dimensionCurrent = true;
            row.distance = distanceBetween(spot->second, here);
        }
        if (row.dimensionCurrent) lastDimension[id] = dimension;
        if (auto last = lastDimension.find(id); last != lastDimension.end()) row.dimension = last->second;
        if (row.distance && !std::isfinite(*row.distance)) row.distance.reset();
        rows.push_back(std::move(row));
    }
    order(rows);
    return rows;
}
}

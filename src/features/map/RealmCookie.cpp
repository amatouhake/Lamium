#include "features/map/RealmCookie.h"
#include "app/Versions.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/game/INetworkGameConnector.h"
#include "mc/client/gui/GameConnectionInfoEx.h"
#include "mc/client/social/MultiplayerGameInfo.h"
#include <algorithm>
#include <cstring>

namespace lamium::map::realm {
namespace {
// std::optional<std::variant<GameConnectionInfoEx, Social::MultiplayerGameInfo, ExperienceJoinRequestInfo,
// Realms::RealmId>> as MSVC lays it out: the alternatives' union, the variant's index byte after it (padded to 8),
// then the optional's flag (padded to 8). Two alternatives are opaque in the SDK: the experience request is at most
// 120 bytes (MultiplayerGameInfo holds an optional of it in 128) and the Realm id 8 (RealmJoinedRecord::mRealmId),
// so the union is as large as the larger known one.
constexpr size_t unionSize = std::max({sizeof(GameConnectionInfoEx), sizeof(Social::MultiplayerGameInfo), size_t{120}});
static_assert(unionSize % 8 == 0 && alignof(GameConnectionInfoEx) == 8 && alignof(Social::MultiplayerGameInfo) == 8);
struct Raw { alignas(8) unsigned char bytes[unionSize + 16]; };
}
Cookie read(IMinecraftGame& game) noexcept {
    Cookie cookie;
    cookie.kind = -3;
    try {
        cookie.inRealm = game.isInRealm();
        // Only a Realm's id is used, and the copy below is never destroyed (it keeps what the connection holds, such
        // as its identity verifier): outside a Realm nothing is read.
        if (!cookie.inRealm || !versionSensitiveAllowed(capabilities::Id::RealmIdentity)) return cookie;
        // The getter is virtual and not exported, and the alternatives' destructors are not exported either: the copy
        // is taken as bytes through the same vtable slot (a member function returns any struct through a hidden
        // pointer) and never destroyed. What it holds on the heap stays, a few hundred bytes a join.
        INetworkGameConnector& connector = game;
        using Getter = Raw (INetworkGameConnector::*)();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-mismatch"
        auto const getter = reinterpret_cast<Getter>(&INetworkGameConnector::getReconnectionCookie);
#pragma clang diagnostic pop
        Raw const raw = (connector.*getter)();
        unsigned char const engaged = raw.bytes[unionSize + 8];
        auto const index = static_cast<signed char>(raw.bytes[unionSize]);
        cookie.kind = engaged == 0 ? -1 : engaged == 1 && index >= 0 && index <= 3 ? index : -2;
        std::int64_t value = 0;
        if (cookie.kind == 3) std::memcpy(&value, raw.bytes, sizeof value);
        else if (cookie.kind == 1) value = reinterpret_cast<Social::MultiplayerGameInfo const*>(raw.bytes)->realmId;
        cookie.id = value > 0 ? value : 0;
    } catch (...) {}
    return cookie;
}
}

#pragma once
#include <cstdint>
class IMinecraftGame;

// Which Realm the game is in (BACKLOG L-358), from the reconnect cookie the game keeps for the current connection.
// Called on the client thread at a join; read-only.
namespace lamium::map::realm {
struct Cookie {
    bool inRealm = false; // The game says so (IMinecraftGame::isInRealm).
    int kind = -1;        // The cookie's alternative: 0 connection, 1 multiplayer game, 2 experience, 3 Realm id;
                          // -1 none, -2 not as expected (nothing taken), -3 not read (not in a Realm,
                          // or the capability off).
    std::int64_t id = 0;  // The Realm's id when the alternative carries one, else 0.
};
Cookie read(IMinecraftGame&) noexcept;
}

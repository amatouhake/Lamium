#pragma once
#include "features/map/ServerIdentity.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// The saved data page (BACKLOG L-343 part 3): what every world and server
// has saved (waypoints, schematic placements, the map, the death layout),
// which of it the joined server uses, and the player's choice of another.
// Implemented with the session (WaypointSession.cpp), which owns the files.
namespace lamium::map::waypoints {
struct SavedData {
    enum class Kind { Server, Friend, Local };
    Kind kind = Kind::Server;
    // A server's or a friend's world's key in the config folders; a local world's folder name.
    std::string key;
    std::string name;    // As shown: "host:port", or the world's name.
    std::string host;    // A server's address without the port, for the any-port switch.
    std::string levelId;
    int waypoints = 0, placements = 0;
    struct Mark { std::string name; int color = 0; };
    std::vector<Mark> marks; // The first few waypoints, to tell whose data it is.
    struct Dot { int x = 0, z = 0, color = 0; };
    std::vector<Dot> dots;   // Overworld waypoints, for the map picture.
    std::filesystem::path mapFolder; // Empty when it has no saved map.
    long long mapBytes = 0;
    bool mapMore = false;    // The map was larger than the walk counts.
    std::int64_t used = 0;   // Last written, seconds since the epoch; 0 unknown.
    bool inUse = false;      // The joined world's data.
    servers::Maybe maybe = servers::Maybe::No;
    bool anyPort = false;
    // The servers recorded as using this data: which address, and whether at one port or at any.
    struct User { std::string host; int port = 0; bool anyPort = false; };
    std::vector<User> users;
    bool canUse = false, canDelete = false;
};
inline constexpr size_t savedMarks = 5, savedDots = 64, savedDataLimit = 300;
// Reads the folders: call when the page opens and after a change, not every frame.
std::vector<SavedData> savedData();
// The joined server uses this server data from now on; everything reloads at once.
bool useData(std::string const& key);
// Deletes the data's waypoints, placements, map and death layout. Never the data in use.
bool removeData(SavedData const&);
// On: the address's other ports follow too (how many is returned; below zero on failure), at once for the joined server.
int setDataAnyPort(std::string const& key, bool on);
// The data the joined server uses, as shown; empty when there is none to choose (a local world, no world).
std::string dataName();
// The notice on the tool screens: other data that may be the joined server's and holds more of one kind than
// the data in use. Worked out at a join and after a change, so asking every frame is cheap.
enum class DataKind { Waypoints, Placements, Map };
std::optional<SavedData> fullerData(DataKind kind);
// "Do not show again": no such notice for the joined server from now on.
void quietFullerData();
}

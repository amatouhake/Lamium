#pragma once
#include "features/map/ServerIdentity.h"
#include "features/map/Waypoints.h"
#include <filesystem>
#include <string>
#include <string_view>

namespace lamium::map {
// Waypoint documents: one per local world (in the world's Lamium folder,
// beside shapes) or per server (in Lamium's config folder, named by the
// server's key: ServerIdentity.h).
std::string encodeWaypoints(WaypointSet const&);
// Tolerant of missing fields; rejects other versions and broken documents.
WaypointSet decodeWaypoints(std::string_view);
WaypointSet readWaypoints(std::filesystem::path const&);
void writeWaypoints(std::filesystem::path const&, WaypointSet const&);
// Which key each server's data is under (L-343): one document in Lamium's
// config folder. Entries that make no sense are dropped; another version or
// a broken document is rejected, and is then left alone.
std::string encodeServers(std::vector<servers::Entry> const&);
std::vector<servers::Entry> decodeServers(std::string_view);
std::vector<servers::Entry> readServers(std::filesystem::path const&);
void writeServers(std::filesystem::path const&, std::vector<servers::Entry> const&);
}

#pragma once
#include "features/schematic/CheckIgnores.h"
#include "features/schematic/Placement.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// The placements of one world (BACKLOG L-93): saved like waypoints, in a local
// world's Lamium folder or per server address and port in the config folder.
namespace lamium::schematic {
inline constexpr size_t maxPlacements = 64, maxPlacementName = 64, maxFileBytes = 260;

struct SavedPlacement {
    std::string name;
    // Relative to the schematics folder, with forward slashes.
    std::string file;
    int dimension = 0;
    Placement placement;
    Layers layers;
    bool visible = true;
    bool countExtras = true; // Extra blocks show red (decided default); off ignores them.
    bool entities = true;
    IgnoreList ignored; // check differences disregarded here (L-183); optional key "ignored"
    std::uint64_t id = 0; // session id (app/SessionIds.h, L-139); not saved
};
struct PlacementSet {
    std::vector<SavedPlacement> placements;
    int selected = -1; // Index into placements, or -1.
};
inline int steppedPlacement(int selected, int count, int amount) {
    if (count <= 0) return -1;
    int from = selected < 0 && amount < 0 ? 0 : selected;
    return ((from + amount) % count + count) % count;
}
// Keeps the selection valid; clamps rotations, layers and names.
void normalize(PlacementSet&);
std::string encodePlacements(PlacementSet const&);
// Tolerant of missing fields; rejects other versions and broken documents.
PlacementSet decodePlacements(std::string_view);
PlacementSet readPlacements(std::filesystem::path const&);
void writePlacements(std::filesystem::path const&, PlacementSet const&);
// A file name the placement may refer to: relative, no "..", no drive or root.
bool safeSchematicPath(std::string_view relative);
// What the ghosts and the check of a placement depend on: equal keys mean
// its built sections and check can be kept. Name and visibility are not in it.
std::string drawKey(SavedPlacement const&);
// What its check depends on: the draw key and the ignored differences, which
// change the check's verdicts and marks but not the ghosts.
std::string checkKey(SavedPlacement const&);
}

#pragma once
#include "features/schematic/CheckIgnores.h"
#include "features/schematic/Placement.h"
#include <algorithm>
#include <cstdint>
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
// Removes the placement at `index`; the selected one stays selected where it
// moved to, or none when it went. False when there is no such placement.
inline bool removePlacementAt(PlacementSet& set, int index) {
    if (index < 0 || index >= static_cast<int>(set.placements.size())) return false;
    set.placements.erase(set.placements.begin() + index);
    if (set.selected == index) set.selected = -1;
    else if (set.selected > index) --set.selected;
    return true;
}
// Shows or hides the placements with these session ids at once (L-190).
inline void showPlacements(PlacementSet& set, std::vector<std::uint64_t> const& ids, bool visible) {
    for (auto& p : set.placements)
        if (std::find(ids.begin(), ids.end(), p.id) != ids.end()) p.visible = visible;
}
// Removes the placements with these session ids at once (L-190); the selected
// one stays selected where it moved to, or none when it went.
inline void removePlacements(PlacementSet& set, std::vector<std::uint64_t> const& ids) {
    std::uint64_t selected = set.selected >= 0 && set.selected < static_cast<int>(set.placements.size())
        ? set.placements[static_cast<size_t>(set.selected)].id : 0;
    auto gone = [&](SavedPlacement const& p) { return std::find(ids.begin(), ids.end(), p.id) != ids.end(); };
    bool selectedGone = set.selected >= 0 && set.selected < static_cast<int>(set.placements.size())
        && gone(set.placements[static_cast<size_t>(set.selected)]);
    std::erase_if(set.placements, gone);
    set.selected = -1;
    if (!selectedGone)
        for (size_t i = 0; i < set.placements.size(); ++i)
            if (set.placements[i].id == selected) set.selected = static_cast<int>(i);
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
// The draw key without the layer view: equal keys keep the resolved palette
// and every built section whose shown layers did not change (L-236).
std::string meshKey(SavedPlacement const&);
// What its check depends on: the draw key and the ignored differences, which
// change the check's verdicts and marks but not the ghosts.
std::string checkKey(SavedPlacement const&);
}

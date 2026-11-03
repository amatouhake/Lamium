#pragma once
// Where the ghosts of two placements share cells (BACKLOG L-141), the one
// earlier in the list draws them and the later one leaves them out: two
// ghosts on the same planes flickered. Pure; used by the ghost pass.
#include "features/schematic/Placement.h"
#include "features/schematic/PlacementStore.h"
#include "features/schematic/SchematicSession.h"
#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace lamium::schematic::overlap {
// Whether a placement shown in this dimension has a ghost block (not void,
// not air, within its shown layers) at a world cell.
// Its palette index there, or none.
inline std::optional<int> paletteAt(session::Shown const& shown, int dimension, Point cell);
inline bool drawsCell(session::Shown const& shown, int dimension, Point cell) { return paletteAt(shown, dimension, cell).has_value(); }
inline std::optional<int> paletteAt(session::Shown const& shown, int dimension, Point cell) {
    if (!shown.structure || !shown.placement.visible || shown.placement.dimension != dimension) return std::nullopt;
    auto const& structure = *shown.structure;
    auto const& placement = shown.placement.placement;
    auto local = toLocal(structure.size, placement, cell);
    if (!local) return std::nullopt;
    Point offset{cell.x - placement.origin.x, cell.y - placement.origin.y, cell.z - placement.origin.z};
    if (!layerShown(shown.placement.layers, structure.size, placement, offset, *local)) return std::nullopt;
    auto at = static_cast<size_t>(structure.cell(local->x, local->y, local->z));
    if (at >= structure.blocks.size()) return std::nullopt;
    auto index = structure.blocks[at];
    if (index == voidCell || static_cast<size_t>(index) >= structure.palette.size()
        || structure.palette[static_cast<size_t>(index)].isAir()) return std::nullopt;
    return static_cast<int>(index);
}
// The block entity the file stores for the ghost in this world cell (L-184):
// the same data the real block shows in the target card, from the file rather
// than from the world. Null when this placement draws no ghost there or the
// file stores none; otherwise it points into `shown.structure`.
inline nbt::Compound const* blockEntityAt(session::Shown const& shown, int dimension, Point cell) {
    if (!drawsCell(shown, dimension, cell)) return nullptr;
    auto const& structure = *shown.structure;
    auto local = toLocal(structure.size, shown.placement.placement, cell);
    auto found = structure.blockEntities.find(structure.cell(local->x, local->y, local->z));
    return found == structure.blockEntities.end() ? nullptr : &found->second;
}
inline bool boxesMeet(session::Shown const& a, session::Shown const& b) {
    if (!a.structure || !b.structure || a.placement.dimension != b.placement.dimension) return false;
    Size sa = placedSize(a.structure->size, a.placement.placement.rotation);
    Size sb = placedSize(b.structure->size, b.placement.placement.rotation);
    Point oa = a.placement.placement.origin, ob = b.placement.placement.origin;
    return oa.x < ob.x + sb.x && ob.x < oa.x + sa.x && oa.y < ob.y + sb.y && ob.y < oa.y + sa.y
        && oa.z < ob.z + sb.z && ob.z < oa.z + sa.z;
}
// Boxes that meet or touch (one cell apart): ghosts on the shared border
// face each other in the same plane.
inline bool boxesTouch(session::Shown const& a, session::Shown const& b) {
    if (!a.structure || !b.structure || a.placement.dimension != b.placement.dimension) return false;
    Size sa = placedSize(a.structure->size, a.placement.placement.rotation);
    Size sb = placedSize(b.structure->size, b.placement.placement.rotation);
    Point oa = a.placement.placement.origin, ob = b.placement.placement.origin;
    return oa.x <= ob.x + sb.x && ob.x <= oa.x + sa.x && oa.y <= ob.y + sb.y && ob.y <= oa.y + sa.y
        && oa.z <= ob.z + sb.z && ob.z <= oa.z + sa.z;
}
// The other shown placements touching this one: their ghosts next to its
// cells decide which border faces it keeps.
inline std::vector<size_t> touching(std::span<session::Shown const> placements, size_t index) {
    std::vector<size_t> out;
    for (size_t j = 0; j < placements.size(); ++j)
        if (j != index && index < placements.size() && placements[j].placement.visible && boxesTouch(placements[j], placements[index]))
            out.push_back(j);
    return out;
}
// The earlier shown placements whose boxes meet this one's: only these can
// own its cells, so a section build looks at them alone.
inline std::vector<size_t> earlierMeeting(std::span<session::Shown const> placements, size_t index) {
    std::vector<size_t> out;
    for (size_t j = 0; j < index && j < placements.size(); ++j)
        if (placements[j].placement.visible && boxesMeet(placements[j], placements[index])) out.push_back(j);
    return out;
}
inline bool ownedEarlier(std::span<session::Shown const> placements, std::span<size_t const> earlier, int dimension, Point cell) {
    for (size_t j : earlier)
        if (j < placements.size() && drawsCell(placements[j], dimension, cell)) return true;
    return false;
}
// Those placements as draw keys with their loaded structure: part of this
// one's built sections' key, so moving, hiding or changing one, or its file
// changing on disk (a new structure), rebuilds the overlap.
inline std::string earlierKey(std::span<session::Shown const> placements, size_t index) {
    std::string key;
    for (size_t j : earlierMeeting(placements, index))
        key += std::format("#{}@{}", drawKey(placements[j].placement), static_cast<void const*>(placements[j].structure.get()));
    return key;
}
// The same for every touching placement, earlier or later: its border
// faces change with them too.
inline std::string touchingKey(std::span<session::Shown const> placements, size_t index) {
    std::string key;
    for (size_t j : touching(placements, index))
        key += std::format("#{}@{}", drawKey(placements[j].placement), static_cast<void const*>(placements[j].structure.get()));
    return key;
}
}

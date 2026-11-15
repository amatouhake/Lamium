#pragma once
// Checking placements against the world (BACKLOG L-93): the selected
// placement in bounded passes (published as a Verification), the Placed
// list's progress for the others, and whether the schematic's entities
// stand at their spots. Run by the ghost pass on the render thread; it owns
// the scans and the published results.
#include "features/schematic/ResolvedPlacement.h"
#include "features/schematic/SchematicSession.h"
#include "features/schematic/Verification.h"
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>
class BlockSource;
class LocalPlayer;
class Vec3;
namespace lamium::schematic::ghosts {
// Which of each placement's entities stand at their spots (fills
// Resolved::entityPlaced), from the entities the client knows around them.
void checkEntities(BlockSource& region, LocalPlayer& player, session::Snapshot const& snapshot, int dimension,
                   std::vector<Resolved>& resolved);
void stepScan(BlockSource& region, session::Snapshot const& snapshot, int dimension, Vec3 const& camera,
              std::vector<Resolved> const& resolved);
void stepProgress(BlockSource& region, session::Snapshot const& snapshot, int dimension, std::vector<Resolved> const& resolved);
// The checked placement's cell at `world` (mismatchAt in GhostRenderer.h).
std::optional<Mismatch> checkedCell(BlockSource& region, Point world, std::vector<Resolved> const& resolved);
// A shulker box's opening (block entity data, L-179): the file's turned with
// the placement, and the world's at `world`; -1 when not a shulker box or
// unreadable. The Check list and the in-world marks share them.
int expectedShulkerFace(Structure const& structure, std::int32_t cell, Placement const& placement);
int actualShulkerFace(BlockSource& region, Point world);
// The cells of the checked placement whose block matches but whose block
// entity data does not (L-303), from its last finished pass. The in-world
// marks take them from here: reading every block entity again while building
// a section would cost a save per cell. `key` is the placement's drawKey.
// `ignored`: the cells holding a compared block entity whose every difference
// a rule covered in that pass (L-183), for the same reason.
struct EntityMistakes {
    std::string key;
    std::set<std::tuple<int, int, int>> cells, ignored;
    bool has(Point p) const { return cells.contains({p.x, p.y, p.z}); }
    bool passed(Point p) const { return ignored.contains({p.x, p.y, p.z}); }
};
std::shared_ptr<EntityMistakes const> entityMistakes();
// L-183: whether the placement's ignore entries take the cell at `world` out
// of the mistakes: an ignored place, or a wrong state whose every difference
// a rule covers. The in-world marks ask it for a cell an entry touches
// before marking it. It reads no block entity: a cell comparing one takes
// the last pass's verdict from `judged` (null: no pass for this placement).
bool ignoredAt(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point world,
               EntityMistakes const* judged);
// Drops the running pass and publishes an empty result.
void resetVerification();
}

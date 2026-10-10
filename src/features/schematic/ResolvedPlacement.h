#pragma once
// A placement's palette as game blocks, turned by the game's own transform,
// with what each entry asks the player to place and the schematic's
// entities where the placement puts them. Owned by the ghost pass
// (GhostRenderer.cpp), read by the meshes, verification and marks.
#include "features/schematic/GhostCommon.h"
#include "features/schematic/PlacementStore.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>
class Block;
namespace lamium::schematic::ghosts {
// An entity of the schematic where the placement puts it.
struct EntityGhost {
    std::string identifier, name, icon; // icon: an item of the same name, if the game has one
    Position at;   // world position of its feet
    Point offset;  // its cell inside the placed box, for layers
    float yaw = 0; // world facing, degrees (0 = south)
};
struct Resolved {
    std::shared_ptr<Structure const> keep; // keeps `structure` alive across snapshots
    Structure const* structure = nullptr;
    int rotation = 0;
    Mirror mirror = Mirror::None;
    std::vector<Block const*> blocks;
    // Palette entries that are opaque full blocks with no mesh on the ghost
    // path (honey block): they must not hide a neighbor's face.
    std::vector<bool> meshless;
    // Per palette entry, the sides of the cell its mesh reaches (any layer,
    // honey's slightly inset outer cube included), as sidesReached bits: a
    // mistake mark next to a block reaching all six leaves out its face
    // there, and near the camera a pair of faces in one plane keeps one.
    std::vector<int> sideMasks;
    // Per palette entry, the sides its mesh covers entirely (sidesCovered):
    // water in a waterlogged cell is not drawn there, as the block hides it.
    std::vector<int> coverMasks;
    // Per palette entry: the other half of a two-block-tall block (turned
    // like `blocks`) and the step up (+1) or down (-1) to it; null and 0
    // for other blocks.
    std::vector<Block const*> halves;
    std::vector<int> halfSteps;
    std::vector<ItemInfo> items;
    std::vector<EntityGhost> entities;
    // Whether each entity stands at its spot; nullopt while it cannot be
    // judged (too far for the client to know its entities, chunk not loaded).
    std::vector<std::optional<bool>> entityPlaced;
};
Resolved resolve(Structure const& structure, SavedPlacement const& placement);
}

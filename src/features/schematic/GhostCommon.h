#pragma once
// What the schematic ghost files share (BACKLOG L-136): the ghost pass
// (GhostRenderer.cpp), its section meshes (GhostMesh), the verification
// (GhostVerify), the area save (AreaSave) and the in-world marks
// (GhostMarks). Game glue; the pure rules are in GhostFaces.h,
// LiquidShape.h and Verify.h.
#include "features/schematic/Structure.h"
#include "overlay/Depth.h"
#include <chrono>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>
class Block;
enum class BlockRenderLayer : unsigned char;
namespace lamium::schematic::ghosts {
using Clock = std::chrono::steady_clock;
inline constexpr double drawDistance = 192; // Sections and entities farther than this are not built or drawn.
inline constexpr size_t maxEntities = 512;  // per placement, and per saved area
inline constexpr float towardEye = overlay::depth::ghostPull; // Depth rules: overlay/Depth.h.

void log(std::string const& text);
bool blended(BlockRenderLayer layer);
// What one palette entry asks the player to place.
struct ItemInfo {
    std::string item, name, icon; // icon: the item as binary NBT for ItemStack::fromTag
    int perBlock = 1;
};
// The game block for a palette entry, looked up again each call.
Block const* lookup(PaletteBlock const& entry);
ItemInfo describe(Block const& block, std::string_view fallback);
// A block's states as text, for naming what differs (the target card).
std::map<std::string, std::string> blockStates(Block const& block);
}

#pragma once
// What the schematic ghost files share (BACKLOG L-136): the ghost pass
// (GhostRenderer.cpp), its section meshes (GhostMesh), the verification
// (GhostVerify), the area save (AreaSave) and the in-world marks
// (GhostMarks). Game glue; the pure rules are in GhostFaces.h,
// LiquidShape.h and Verify.h.
#include "features/schematic/Structure.h"
#include "overlay/Depth.h"
#include <glm/glm.hpp>
#include <chrono>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>
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
    // Items one cell of this entry asks for: 0 for a second half or a flowing
    // liquid cell, which the game fills from a source (L-289).
    int perBlock = 1;
};
// The game block for a palette entry, looked up again each call.
Block const* lookup(PaletteBlock const& entry);
ItemInfo describe(Block const& block, std::string_view fallback);
// A block's states as text, for naming what differs (the target card).
std::map<std::string, std::string> blockStates(Block const& block);
// The check relaxations the settings turn on (rules::Category bits, L-262),
// and whether `actual` passes for `expected`, or for its kind, under them.
unsigned checkRules();
// "Ignore sign text" (L-303): a sign's block entity fields are not compared.
bool signTextIgnored();
// "Ignore block entities" (L-303): no block entity field is compared.
bool blockEntitiesIgnored();
// A block whose mesh is only its base: its block actor draws the rest from
// the file's data (a piston's head, L-178; a lectern's book, a framed item,
// L-302), so its ghost needs both. A potted plant is not one: the pot's mesh
// draws it from the world's actor, so a ghost pot draws the file's plant
// itself (GhostMesh.cpp), raised by `potSoil` onto the pot's soil.
bool actorDrawsOverMesh(Block const& block);
// The plant a flower pot's block entity data names (`PlantBlock`), as a game block.
Block const* pottedPlant(nbt::Compound const& data);
// Seats a plant tessellated at the pot's cell (vertices from `from` on) in
// the pot: centered on the cell, as the game draws every potted plant (in
// the world grass and flowers stand off center by their position), and
// lifted onto the soil. `cell`: the cell's low corner in the same space.
void seatPottedPlant(std::vector<glm::vec3>& positions, size_t from, glm::vec3 cell);
inline constexpr float potSoil = 4.f / 16;
bool relaxedSame(Block const& expected, Block const& actual, unsigned on);
bool relaxedSameType(Block const& expected, Block const& actual, unsigned on);
}

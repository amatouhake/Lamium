#pragma once
// Draws schematic placements as ghost blocks in the world (BACKLOG L-93),
// using the path found by the ghost probe: a private BlockTessellator,
// in-world tessellation per section, tinted vertex colors and the
// moving-block renderer's materials, lit as if fully bright.
#include "features/schematic/SaveArea.h"
#include "features/schematic/PlacementStore.h"
#include "features/schematic/Verification.h"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
class Block;
class BlockSource;
class BlockPos;
class BlockTessellator;
class ScreenContext;
class Tessellator;
enum class BlockRenderLayer : unsigned char;
namespace lamium::schematic::ghosts {
// The game block for a palette entry (its name, states and version), or null.
Block const* gameBlock(PaletteBlock const& entry);
// What a palette entry is called and its item icon (binary NBT, "" if none).
struct BlockLabel {
    std::string name, icon;
};
BlockLabel blockLabel(PaletteBlock const& entry);
// The checked placement's cell at `world` as it is now (any state, Correct
// included); nullopt outside it or before the first check.
std::optional<Mismatch> mismatchAt(BlockSource& region, Point world);
// Whether a block hides the neighbor faces it touches: an opaque full block
// that the in-world tessellation draws (a honey block draws nothing there).
bool coversNeighbors(Block const& block, BlockTessellator& tessellator, ScreenContext& screen);
// Which sides of its cell a block's mesh reaches, as bits in faces::offsets
// order (63: all six), within `epsilon` of the cell's planes; blended layers
// count only with `blendedToo`.
int sidesReached(Block const& block, BlockTessellator& tessellator, ScreenContext& screen, bool blendedToo, float epsilon);
// Calls `visit` with each render layer a block draws in: its own, then its
// extra ones (honey and slime blocks). Liquids get one call with nullopt
// (the tessellator's default pass).
void eachLayer(Block const& block, BlockSource& region, BlockPos const& pos,
               std::function<void(std::optional<BlockRenderLayer>)> const& visit);
// Tessellates the block's geometry for one render layer (nullopt: the
// tessellator's current one) in the world at `pos`.
void tessellateLayer(BlockTessellator& tessellator, Tessellator& batch, Block const& block, BlockPos const& pos,
                     std::optional<BlockRenderLayer> layer);
// 1 for water, 2 for lava, 0 for anything else.
int liquidKind(Block const& block);
// A liquid block's liquid_depth (0 for a source or anything else).
int liquidDepth(Block const& block);
// Appends a missing liquid at `pos` as a simple shell: the faces where
// `open(side)` holds (faces::offsets order) and that no opaque neighbor
// hides, each top corner (cx, cz: 0 or 1) at `corner(cx, cz)` above the
// cell's floor (LiquidShape.h), textured with the liquid's own atlas
// texture and colored (water blue, both translucent). False when nothing
// was added.
bool liquidShell(BlockTessellator& tessellator, Tessellator& batch, BlockPos const& pos, Block const& liquid,
                 std::function<bool(int side)> const& open, std::function<float(int cx, int cz)> const& corner);
// Puts a quad-list batch's quads in `order` (indices of the current quads),
// moving every vertex stream with its positions.
void reorderQuads(Tessellator& batch, std::vector<std::uint32_t> const& order);
void start();
void stop();
// The latest finished verification of the selected placement (never null).
std::shared_ptr<Verification const> verification();
// Progress for the Placed list (L-93 screen review): correct / total in the
// placement's shown layers, counted in the background for about a second
// after each wantProgress(); nullopt until a pass finished.
void wantProgress();
std::optional<Tally> progress(SavedPlacement const&);
// Marks a cell in the world for a while ("Show in world").
void point(Point cell);

// Saving an area: the world render reads it in bounded steps (it needs the
// world's blocks) and writes the file when done. One save at a time.
struct SaveRequest {
    Area area;
    int dimension = 0;
    bool entities = false;
    std::filesystem::path path;
    std::string file; // the name shown in messages
};
// False while another save runs.
bool save(SaveRequest request);
// How far the running save is; `waiting` while no loaded column is left.
struct SaveStatus {
    std::string file;
    std::uint64_t done = 0, total = 0;
    bool waiting = false;
};
std::optional<SaveStatus> saveStatus();
void stopSaving();
// A finished save's message for a toast, handed out once.
std::optional<std::string> takeSaveMessage();
}

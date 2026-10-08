#pragma once
// Draws schematic placements as ghost blocks in the world (BACKLOG L-93),
// using the path found by the ghost probe: a private BlockTessellator,
// in-world tessellation per section, tinted vertex colors and the
// moving-block renderer's materials, lit as if fully bright.
#include "features/schematic/SaveArea.h"
#include "features/schematic/PlacementStore.h"
#include "features/schematic/Verification.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
class Block;
class BlockSource;
class BlockTessellator;
class ScreenContext;
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

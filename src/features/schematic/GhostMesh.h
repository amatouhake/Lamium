#pragma once
// The ghost meshes of one placement section (BACKLOG L-93): blocks through a
// private tessellator against the schematic's own neighbors, faces culled
// against ghosts, liquids as shells, mistake marks and outlines. Built by
// the ghost pass (GhostRenderer.cpp), which owns the sections.
#include "features/schematic/ResolvedPlacement.h"
#include "features/schematic/SchematicSession.h"
#include "features/schematic/Nbt.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/level/BlockPos.h"
#include <glm/vec3.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>
class BlockSource;
class BlockTessellator;
class ScreenContext;
namespace lamium::schematic { class SchematicRegion; }
namespace lamium::schematic::ghosts {
// Ghost meshes are built per section of this many blocks a side: one
// changed block re-tessellates its section, which took 4-12 ms at 16.
constexpr int sectionSize = 8;

// A ghost drawn by its block-entity renderer, with the file's block entity
// data for it (bed color, skull type and rotation, sign text, banner).
struct EntityCell { BlockPos pos; Block const* block; std::optional<nbt::Compound> data; };
struct Section {
    glm::vec3 origin{};
    // faces: every render layer but the blended ones, drawn alpha-tested;
    // blend: blended layers (stained glass, honey, slime) and the mistake
    // marks, drawn last; marks: mistake marks over a real blended block,
    // which the blend mesh's depth would hide.
    std::optional<mce::Mesh> faces, blend, marks;
    std::uint32_t faceVertices = 0, blendVertices = 0, markVertices = 0;
    // Outlines, one mesh per color (lines::colored draws one color at a time).
    struct ColorLines {
        glm::vec3 color{};
        std::optional<mce::Mesh> mesh;
        std::uint32_t vertices = 0;
    };
    std::vector<std::unique_ptr<ColorLines>> lines;
    std::vector<EntityCell> entities;
    Clock::time_point built{}, checked{};
    std::optional<Clock::time_point> due; // An early rebuild after a looked-at block changed.
    std::uint64_t signature = 0;          // the world's blocks in the section when built
    bool complete = false; // false while some chunk was not loaded
    std::uint64_t wantedStamp = 0; // the wanted list that last included it
};
using SectionKey = std::tuple<int, int, int, int>; // placement, section x, y, z

// Where the camera was when sections are built.
struct BuildCamera {
    // The cells the camera is in (eye and feet). Ghosts within one cell of
    // them are drawn whole: no face dropped, none skipped as enclosed. From
    // inside a schematic, or with the camera at a cell border, what is around
    // the player then shows as blocks instead of hollow space.
    std::array<std::optional<Point>, 2> cells;
    Vec3 eye{};
    // Vibrant Visuals (or ray tracing): mistake faces are plain quads drawn
    // with the overlay face material, which keeps its color there; the
    // blended mesh and the hologram material drew them colorless.
    bool vibrant = false;
    bool near(Point p, int reach = 1) const;
};

// The world's blocks in a section's cells, hashed: equal values mean nothing
// there changed and the built meshes still hold.
std::uint64_t signatureOf(BlockSource& region, session::Shown const& shown, SectionKey key);
void buildSection(ScreenContext& screen, BlockSource& region, SchematicRegion& view, BlockTessellator& own,
                  session::Shown const& shown, Resolved const& blocks, SectionKey key, BuildCamera const& camera, Section& out);
}

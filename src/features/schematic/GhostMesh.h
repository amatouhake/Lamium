#pragma once
// The ghost meshes of one placement section (BACKLOG L-93): blocks through a
// private tessellator against the schematic's own neighbors, faces culled
// against ghosts, liquids as shells, mistake marks and outlines. Built by
// the ghost pass (GhostRenderer.cpp), which owns the sections.
#include "features/schematic/ResolvedPlacement.h"
#include "features/schematic/SchematicSession.h"
#include "features/schematic/Nbt.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/level/BlockPos.h"
#include <glm/vec3.hpp>
#include <array>
#include <cstdint>
#include <functional>
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

// A section's blended quads (see-through ghosts and mistake marks), kept on
// the CPU: every section's are sorted together, far to near from the current
// camera, into one mesh when drawn (L-193). Sorted per section, the quads of
// two placements' sections at one spot drew in a fixed order and kept the
// order of the last build after the camera moved (L-192).
struct BlendQuads {
    glm::vec3 origin{}; // the positions are relative to it
    std::vector<glm::vec3> positions;
    std::vector<glm::vec4> normals, tangents;
    std::vector<unsigned> colors, mers;
    std::vector<unsigned short> bones, pbr;
    std::vector<glm::vec2> uvs[3];
    std::vector<unsigned char> geo;
    std::array<bool, 15> enabled{};
};
struct Section {
    glm::vec3 origin{};
    // faces: every render layer but the blended ones, drawn alpha-tested;
    // blend: blended layers (stained glass, honey, slime) and the mistake
    // marks, drawn last; marks: mistake marks over a real blended block,
    // which the blend mesh's depth would hide.
    std::optional<mce::Mesh> faces, marks, blendMesh;
    // The blended quads twice: uploaded in build order for when the section
    // is far, and kept for the camera-sorted mesh of the near sections.
    std::shared_ptr<BlendQuads const> blend;
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
// A ghost drawn at a cell: its block and the sides of the cell it reaches
// (sidesReached bits); no block where none is drawn.
struct NeighborGhost {
    Block const* block = nullptr;
    int sides = 0;
};
// Each palette block's shape facts (meshless, sides reached, sides covered),
// once per resolved placement; buildSection does it for its own.
void prepareShapes(ScreenContext& screen, BlockTessellator& own, Resolved const& blocks);
// The ghost a placement draws at a cell (needs prepareShapes).
NeighborGhost ghostDrawnAt(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point n);
// `owned`: cells an earlier placement draws (L-141), left out here.
// `others`: another touching placement's ghost at a cell, for the faces on
// the border between them.
void buildSection(ScreenContext& screen, BlockSource& region, SchematicRegion& view, BlockTessellator& own,
                  session::Shown const& shown, Resolved const& blocks, SectionKey key, BuildCamera const& camera,
                  std::function<bool(Point)> const& owned, std::function<NeighborGhost(Point)> const& others, Section& out);
}

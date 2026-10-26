#include "features/schematic/GhostMesh.h"
#include "app/Runtime.h"
#include "features/schematic/GhostRenderer.h"
#include "features/schematic/GhostVerify.h"
#include "features/schematic/GhostFaces.h"
#include "features/schematic/LiquidShape.h"
#include "features/schematic/SchematicRegion.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/SupplementaryFieldAutoGenerationMode.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/deps/minecraft_renderer/framebuilder/dragon/RenderMetadata.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/deps/core_graphics/enums/PrimitiveMode.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include "mc/deps/minecraft_renderer/renderer/MeshData.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockRenderLayer.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/block/actor/BlockActorType.h"
#include "mc/world/level/chunk/ChunkState.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/material/Material.h"
#include "mc/world/phys/AABB.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <format>
#include <map>
#include <tuple>

namespace lamium::schematic::ghosts {
namespace {
struct Outline { glm::vec3 min, max; float r, g, b; };
// Near the camera, a pair of ghost faces in one plane keeps only the face
// toward the camera for every pair (true), or only for pairs of opaque full
// blocks (false). True: no flicker where a see-through block (a spawner)
// meets another, but from just outside the spawner's face there is gone
// while it shows from afar. False: the same faces near and far, flickering
// a little there like real blocks do (decided 2026-10-09: true, ghosts
// flickered more than real blocks).
constexpr bool pairAllGhostFaces = true;
// Light UVs and colors: the in-world mesh carries both, but fill when absent.
void finishColors(Tessellator& batch, float r, float g, float b) {
    auto& data = batch.mMeshData.get();
    size_t vertices = data.mPositions->size();
    auto& uv1 = data.mTextureUVs[1].get();
    if (uv1.size() != vertices) uv1.assign(vertices, glm::vec2{1.f, 1.f});
    auto& colors = data.mColors.get();
    if (colors.size() != vertices) colors.assign(vertices, 0xffffffffu);
    auto scale = [](std::uint32_t value, int shift, float factor) {
        return static_cast<std::uint32_t>(std::lround(std::clamp(((value >> shift) & 255) * factor, 0.f, 255.f))) << shift;
    };
    for (auto& c : colors) c = scale(c, 0, r) | scale(c, 8, g) | scale(c, 16, b) | (c & 0xff000000u);
}

// A ghost with an opaque full block on all six sides cannot be seen: real
// ones, or ghosts that will be drawn there (shown layers, nothing placed).
// An opaque full ghost will be drawn at `n`: nothing real is there, the
// placement asks for an opaque full block in a shown layer.
bool ghostOpaqueAt(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point n) {
    auto const& structure = *shown.structure;
    auto const& placement = shown.placement;
    Point const& origin = placement.placement.origin;
    auto local = toLocal(structure.size, placement.placement, n);
    if (!local || !layerShown(placement.layers, structure.size, placement.placement, {n.x - origin.x, n.y - origin.y, n.z - origin.z}, *local)) return false;
    auto index = structure.blocks[static_cast<size_t>(structure.cell(local->x, local->y, local->z))];
    if (index == voidCell || static_cast<size_t>(index) >= blocks.blocks.size()) return false;
    Block const* ghost = blocks.blocks[static_cast<size_t>(index)];
    bool drawn = static_cast<size_t>(index) >= blocks.meshless.size() || !blocks.meshless[static_cast<size_t>(index)];
    return ghost && drawn && ghost->getBlockType().mIsOpaqueFullBlock && region.getBlock(BlockPos{n.x, n.y, n.z}).isAir();
}
// The sides of its cell that the ghost drawn at `n` reaches (sidesReached
// bits), 0 where no ghost is drawn (nothing expected in a shown layer,
// something real there, a liquid).
NeighborGhost ghostAt(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point n) {
    auto const& structure = *shown.structure;
    auto const& placement = shown.placement;
    Point const& origin = placement.placement.origin;
    auto local = toLocal(structure.size, placement.placement, n);
    if (!local || !layerShown(placement.layers, structure.size, placement.placement, {n.x - origin.x, n.y - origin.y, n.z - origin.z}, *local)) return {};
    auto index = structure.blocks[static_cast<size_t>(structure.cell(local->x, local->y, local->z))];
    if (index == voidCell || static_cast<size_t>(index) >= blocks.blocks.size() || static_cast<size_t>(index) >= blocks.sideMasks.size()) return {};
    Block const* block = blocks.blocks[static_cast<size_t>(index)];
    if (!block || liquidKind(*block)) return {};
    BlockPos pos{n.x, n.y, n.z};
    auto* chunk = region.getChunkAt(pos);
    if (!chunk || chunk->mLoadState->load() < ChunkState::Loaded || !region.getBlock(pos).isAir()) return {};
    return {block, blocks.sideMasks[static_cast<size_t>(index)]};
}
int ghostSidesAt(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point n) {
    return ghostAt(region, shown, blocks, n).sides;
}
bool ghostBoxAt(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point n) {
    return ghostSidesAt(region, shown, blocks, n) == 63;
}
// What the placement has at `n` (shown layers, either layer), or the world
// where the placement says nothing (outside it, hidden layers, structure
// void), for a liquid shell's faces and slope. The file decides inside, so
// real liquids nearby do not bend a ghost liquid's surface.
liquids::Cell liquidAt(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point n) {
    auto const& structure = *shown.structure;
    auto const& placement = shown.placement;
    Point const& origin = placement.placement.origin;
    auto local = toLocal(structure.size, placement.placement, n);
    if (local && layerShown(placement.layers, structure.size, placement.placement, {n.x - origin.x, n.y - origin.y, n.z - origin.z}, *local)) {
        auto cell = static_cast<size_t>(structure.cell(local->x, local->y, local->z));
        bool said = false, solid = false;
        for (auto const* layer : {&structure.blocks, &structure.liquids}) {
            if (cell >= layer->size()) continue;
            auto index = (*layer)[cell];
            if (index == voidCell || static_cast<size_t>(index) >= blocks.blocks.size()) continue;
            said = true;
            Block const* block = blocks.blocks[static_cast<size_t>(index)];
            if (!block) continue;
            if (int kind = liquidKind(*block)) return {kind, layer == &structure.blocks ? liquidDepth(*block) : 0, false};
            solid = solid || block->getMaterial().mSolid;
        }
        if (said) return {0, 0, solid};
    }
    BlockPos pos{n.x, n.y, n.z};
    Block const& real = region.getBlock(pos);
    if (int kind = liquidKind(real)) return {kind, liquidDepth(real), false};
    if (int kind = liquidKind(region.getBlock(pos, 1))) return {kind, 0, false};
    return {0, 0, static_cast<bool>(real.getMaterial().mSolid)};
}
bool enclosed(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point at) {
    for (auto const& d : faces::offsets) {
        Point n{at.x + d[0], at.y + d[1], at.z + d[2]};
        if (region.getBlock(BlockPos{n.x, n.y, n.z}).getBlockType().mIsOpaqueFullBlock) continue;
        if (!ghostOpaqueAt(region, shown, blocks, n)) return false;
    }
    return true;
}
// Drops the quads of a just tessellated ghost that lie on a side touching an
// opaque ghost: unseen from outside, and they fought with the neighbor's own
// face. Real opaque full neighbors (not blended ones) hide the quad too: the tessellator culls
// against them for most blocks, but not the honey block's outer cube, which
// fought with the real face in the same plane. A dropped
// quad collapses to one point, so no other vertex data has to move.
// Near the camera (this cell or the neighbor within one cell of it) the pair
// keeps one face instead: the one facing the camera. Every such plane then
// has exactly one face, so the cells around the camera look solid even when
// the near clip plane cuts the closest face, with nothing fighting in one
// plane (the material draws both sides).
void cullAgainstGhosts(Tessellator& batch, size_t from, BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point at,
                       BuildCamera const& camera, Block const& self, bool selfBlended,
                       std::function<NeighborGhost(Point)> const& others) {
    auto& positions = batch.mMeshData->mPositions.get();
    std::array<std::optional<bool>, 6> hidden;
    for (size_t q = from; q + 4 <= positions.size(); q += 4) {
        std::array<faces::Vertex, 4> quad;
        for (size_t k = 0; k < 4; ++k) quad[k] = {positions[q + k].x, positions[q + k].y, positions[q + k].z};
        int side = faces::sideOf(quad, at.x, at.y, at.z);
        if (side < 0) continue;
        auto& known = hidden[static_cast<size_t>(side)];
        if (!known) {
            auto const& d = faces::offsets[side];
            Point n{at.x + d[0], at.y + d[1], at.z + d[2]};
            BlockPos np{n.x, n.y, n.z};
            Block const& real = region.getBlock(np);
            // Honey and slime count as opaque full blocks but are see-through.
            bool realBlended = blended(real.getBlockType().getRenderLayer(real, region, np));
            // A real see-through block whose face covers this side (glass,
            // ice) writes depth in the same plane: the ghost's face fought it
            // there, darker or lighter by section and camera position
            // (L-192, L-193). Its own face usually shows instead; where the
            // game culls its back face (inside it, adjacent glass, a beacon)
            // both faces can be missing (L-245).
            bool realOpaque = (real.getBlockType().mIsOpaqueFullBlock && !realBlended)
                || (realBlended && real.getBlockType().canProvideFullSupport(real, faces::facingToward(side)));
            // This placement's ghost there, else a touching placement's.
            NeighborGhost near = ghostAt(region, shown, blocks, n);
            bool another = !near.block && others;
            if (another) near = others(n);
            bool reaches = near.block && (near.sides >> (side ^ 1) & 1);
            bool nearBlended = near.block && blended(near.block->getBlockType().getRenderLayer(*near.block, region, np));
            bool ghostOpaque = !realOpaque
                && (another ? near.block && near.sides == 63 && near.block->getBlockType().mIsOpaqueFullBlock && !nearBlended
                            : ghostOpaqueAt(region, shown, blocks, n));
            bool shared = !realOpaque && (ghostOpaque || (pairAllGhostFaces && reaches));
            bool nearCamera = shared && (camera.near(at) || camera.near(n));
            bool beyond = nearCamera && faces::beyond(side, at.x, at.y, at.z, camera.eye.x, camera.eye.y, camera.eye.z);
            bool sameType = near.block && &near.block->getBlockType() == &self.getBlockType();
            // Two see-through ghosts of a kind never keep a face between them,
            // not even next to the camera: kept there by the camera pairing,
            // two sections built with the camera on either side each kept
            // theirs and the pair flickered (L-193, seen in the log).
            known = (!realOpaque && selfBlended && nearBlended && sameType && reaches)
                || faces::dropFace(realOpaque, ghostOpaque, shared, nearCamera, beyond)
                || (!realOpaque && !nearCamera && selfBlended && faces::dropBlendedFace(reaches, nearBlended, sameType, side));
        }
        if (*known) for (size_t k = 1; k < 4; ++k) positions[q + k] = positions[q];
    }
}
// Collapses quads that repeat an earlier quad of the same ghost: a face the
// tessellator emits in both windings (crop planes) is drawn twice by the
// two-sided ghost material, and the two fight in one plane (flicker).
void dropDuplicateQuads(Tessellator& batch, size_t from) {
    auto& positions = batch.mMeshData->mPositions.get();
    auto quadAt = [&](size_t q) {
        std::array<faces::Vertex, 4> quad;
        for (size_t k = 0; k < 4; ++k) quad[k] = {positions[q + k].x, positions[q + k].y, positions[q + k].z};
        return quad;
    };
    for (size_t q = from + 4; q + 4 <= positions.size(); q += 4) {
        if (positions[q] == positions[q + 1] && positions[q] == positions[q + 2]) continue;
        auto quad = quadAt(q);
        for (size_t p = from; p < q; p += 4)
            if (faces::sameQuad(quad, quadAt(p))) {
                for (size_t k = 1; k < 4; ++k) positions[q + k] = positions[q];
                break;
            }
    }
}
// In a cell the camera is in, a ghost keeps only the faces turned toward
// the camera, as the game draws blocks: seen from inside, the block is not
// there. The ghost material draws both sides, so from inside a door or a
// spawner its own faces fought with a neighbor's in the same plane, and a
// stair's two faces at half height (the lower half's top, the step's
// bottom) fought with each other.
void dropBackFaces(Tessellator& batch, size_t from, BuildCamera const& camera) {
    auto& data = static_cast<mce::MeshData&>(batch.mMeshData);
    auto& positions = *data.mPositions;
    auto const& normals = *data.mNormals;
    glm::vec3 eye{static_cast<float>(camera.eye.x), static_cast<float>(camera.eye.y), static_cast<float>(camera.eye.z)};
    for (size_t q = from; q + 4 <= positions.size(); q += 4) {
        glm::vec3 n = normals.size() == positions.size() ? glm::vec3(normals[q])
                                                          : glm::cross(positions[q + 1] - positions[q], positions[q + 2] - positions[q]);
        glm::vec3 center = (positions[q] + positions[q + 1] + positions[q + 2] + positions[q + 3]) * .25f;
        if (faces::facesAway(n, center, eye))
            for (size_t k = 1; k < 4; ++k) positions[q + k] = positions[q];
    }
}
// The cells of one section inside a placement's box.
// `halo` widens the section by that many cells on every side.
template <class Visit>
void eachCell(session::Shown const& shown, SectionKey key, int halo, Visit&& visit) {
    auto const& placement = shown.placement;
    Size placed = placedSize(shown.structure->size, placement.placement.rotation);
    Point const& origin = placement.placement.origin;
    auto [index, sx, sy, sz] = key;
    Point low{sx * sectionSize - halo, sy * sectionSize - halo, sz * sectionSize - halo};
    int span = sectionSize + 2 * halo;
    for (int x = std::max(low.x, origin.x - halo); x < std::min(low.x + span, origin.x + placed.x + halo); ++x)
        for (int y = std::max(low.y, origin.y - halo); y < std::min(low.y + span, origin.y + placed.y + halo); ++y)
            for (int z = std::max(low.z, origin.z - halo); z < std::min(low.z + span, origin.z + placed.z + halo); ++z)
                visit(x, y, z);
}
// What the tessellator sees at `n` while it draws the ghost at `at` (palette
// entry `drawn`): the placement's block where a ghost is drawn (shown layer,
// nothing real there), so doors find their other half and fences and panes
// connect to their schematic neighbors; the world elsewhere. A door's other
// half is supplied where the file has none or the world holds something
// else there (water, the area's edge), unless the real half is placed. An opaque full ghost that
// must not hide its neighbor's face (no mesh, or either cell next to the
// camera, where cullAgainstGhosts keeps the face toward the camera) reads
// as the world, so the tessellator keeps that face.
Block const* ghostNeighbor(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point n, Point at, int drawn,
                           BuildCamera const& camera) {
    if (drawn >= 0 && static_cast<size_t>(drawn) < blocks.halves.size() && blocks.halfSteps[static_cast<size_t>(drawn)]
        && n.x == at.x && n.z == at.z && n.y == at.y + blocks.halfSteps[static_cast<size_t>(drawn)]) {
        Block const* half = blocks.halves[static_cast<size_t>(drawn)];
        if (&region.getBlock(BlockPos{n.x, n.y, n.z}).getBlockType() != &half->getBlockType()) return half;
    }
    auto const& structure = *shown.structure;
    auto const& placement = shown.placement;
    Point const& origin = placement.placement.origin;
    auto local = toLocal(structure.size, placement.placement, n);
    if (!local || !layerShown(placement.layers, structure.size, placement.placement, {n.x - origin.x, n.y - origin.y, n.z - origin.z}, *local)) return nullptr;
    auto index = structure.blocks[static_cast<size_t>(structure.cell(local->x, local->y, local->z))];
    if (index == voidCell || static_cast<size_t>(index) >= blocks.blocks.size()) return nullptr;
    Block const* ghost = blocks.blocks[static_cast<size_t>(index)];
    if (!ghost || !region.getBlock(BlockPos{n.x, n.y, n.z}).isAir()) return nullptr;
    if (ghost->getBlockType().mIsOpaqueFullBlock) {
        bool meshless = static_cast<size_t>(index) < blocks.meshless.size() && blocks.meshless[static_cast<size_t>(index)];
        if (meshless || camera.near(n) || camera.near(at)) return nullptr;
    }
    return ghost;
}
}

bool BuildCamera::near(Point p, int reach) const {
    return std::any_of(cells.begin(), cells.end(), [&](auto const& c) {
        return c && std::abs(c->x - p.x) <= reach && std::abs(c->y - p.y) <= reach && std::abs(c->z - p.z) <= reach;
    });
}
// The world's blocks in a section's cells, hashed: equal values mean nothing
// there changed and the built meshes still hold.
std::uint64_t signatureOf(BlockSource& region, session::Shown const& shown, SectionKey key) {
    std::uint64_t hash = 1469598103934665603ull;
    // One cell around the section too: its border ghosts' faces and
    // enclosure depend on the neighbors there.
    eachCell(shown, key, 1, [&](int x, int y, int z) {
        BlockPos pos{x, y, z};
        auto* chunk = region.getChunkAt(pos);
        bool loaded = chunk && chunk->mLoadState->load() >= ChunkState::Loaded;
        Block const* block = loaded ? &region.getBlock(pos) : nullptr;
        auto value = block ? reinterpret_cast<std::uintptr_t>(block) : 1;
        hash = (hash ^ static_cast<std::uint64_t>(value)) * 1099511628211ull;
        // A shulker box's opening arrives with its block actor, after the block.
        if (block && block->getBlockType().mBlockEntityType == BlockActorType::ShulkerBox)
            hash = (hash ^ static_cast<std::uint64_t>(actualShulkerFace(region, {x, y, z}) + 2)) * 1099511628211ull;
    });
    return hash;
}

void prepareShapes(ScreenContext& screen, BlockTessellator& own, Resolved const& blocks) {
    if (blocks.meshless.size() == blocks.blocks.size()) return;
    auto& meshless = const_cast<Resolved&>(blocks).meshless;
    meshless.assign(blocks.blocks.size(), false);
    for (size_t i = 0; i < blocks.blocks.size(); ++i)
        if (auto const* b = blocks.blocks[i]; b && b->getBlockType().mIsOpaqueFullBlock) meshless[i] = !coversNeighbors(*b, own, screen);
    auto& masks = const_cast<Resolved&>(blocks).sideMasks;
    masks.assign(blocks.blocks.size(), 0);
    for (size_t i = 0; i < blocks.blocks.size(); ++i)
        if (auto const* b = blocks.blocks[i]) masks[i] = sidesReached(*b, own, screen, true, .02f);
    auto& covers = const_cast<Resolved&>(blocks).coverMasks;
    covers.assign(blocks.blocks.size(), 0);
    for (size_t i = 0; i < blocks.blocks.size(); ++i)
        if (auto const* b = blocks.blocks[i]; b && !liquidKind(*b)) covers[i] = sidesCovered(*b, own, screen);
}
NeighborGhost ghostDrawnAt(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point n) {
    return ghostAt(region, shown, blocks, n);
}

void buildSection(ScreenContext& screen, BlockSource& region, SchematicRegion& view, BlockTessellator& own,
                  session::Shown const& shown, Resolved const& blocks, SectionKey key, BuildCamera const& camera,
                  std::function<bool(Point)> const& owned, std::function<NeighborGhost(Point)> const& others, Section& out) {
    auto const& structure = *shown.structure;
    auto const& placement = shown.placement;
    Size placed = placedSize(structure.size, placement.placement.rotation);
    Point const& origin = placement.placement.origin;
    auto const slab = placedSlab(placement.layers, structure.size, placement.placement);
    auto [index, sx, sy, sz] = key;
    Point low{sx * sectionSize, sy * sectionSize, sz * sectionSize};
    // Reset in place: meshes cannot be copied or assigned.
    out.faces.reset(); out.blend.reset(); out.blendMesh.reset(); out.lines.clear(); out.marks.reset();
    out.faceVertices = out.blendVertices = out.markVertices = 0;
    out.entities.clear();
    out.due.reset();
    out.origin = {static_cast<float>(low.x), static_cast<float>(low.y), static_cast<float>(low.z)};
    out.built = out.checked = Clock::now();
    out.signature = signatureOf(region, shown, key);
    out.complete = true;
    auto const tint = faces::ghostTint(Runtime::instance().snapshot()->schematic.ghostTint);
    // L-252: without marks a mistake is drawn like a correct cell (nothing).
    bool const showMarks = Runtime::instance().snapshot()->schematic.mistakeMarks;

    prepareShapes(screen, own, blocks);
    Point drawing{};
    int drawn = -1;
    view.answer = [&](BlockPos const& p) { return ghostNeighbor(region, shown, blocks, {p.x, p.y, p.z}, drawing, drawn, camera); };
    struct Clear { SchematicRegion& view; ~Clear() { view.answer = nullptr; } } clear{view};
    Tessellator batch(screen.tessellator.mBufferResourceService), see(screen.tessellator.mBufferResourceService);
    batch.begin({}, mce::PrimitiveMode::QuadList, 4096, false);
    see.begin({}, mce::PrimitiveMode::QuadList, 256, false);
    // Mistakes also get tinted faces just outside the real block, so they
    // stay visible next to the vanilla selection outline.
    std::vector<Outline> outlines, marks;
    std::vector<std::pair<Point, Block const*>> liquids; // cells missing their liquid
    auto cellBox = [](BlockPos p) { return std::pair{glm::vec3(p.x, p.y, p.z), glm::vec3(p.x + 1, p.y + 1, p.z + 1)}; };
    for (int x = std::max(low.x, origin.x); x < std::min(low.x + sectionSize, origin.x + placed.x); ++x)
        for (int y = std::max(low.y, origin.y); y < std::min(low.y + sectionSize, origin.y + placed.y); ++y)
            for (int z = std::max(low.z, origin.z); z < std::min(low.z + sectionSize, origin.z + placed.z); ++z) {
                if (!slab.shown({x - origin.x, y - origin.y, z - origin.z})) continue;
                auto local = toLocal(structure.size, placement.placement, {x, y, z});
                if (!local) continue;
                auto paletteIndex = structure.blocks[static_cast<size_t>(structure.cell(local->x, local->y, local->z))];
                if (paletteIndex == voidCell || static_cast<size_t>(paletteIndex) >= blocks.blocks.size()) continue;
                if (owned && owned({x, y, z})) continue;
                Block const* expected = blocks.blocks[static_cast<size_t>(paletteIndex)];
                bool expectsAir = structure.palette[static_cast<size_t>(paletteIndex)].isAir();
                BlockPos pos{x, y, z};
                auto* chunk = region.getChunkAt(pos);
                // While a chunk arrives the client shows placeholder blocks:
                // neither counts as built until it is loaded.
                if (!chunk || chunk->mLoadState->load() < ChunkState::Loaded) { out.complete = false; continue; }
                Block const& actual = region.getBlock(pos);
                if (actual.getMaterial().mType == SharedTypes::v1_26_20::MaterialType::ClientRequestPlaceholder) {
                    out.complete = false;
                    continue;
                }
                auto [boxLow, boxHigh] = cellBox(pos);
                if (expectsAir) {
                    // An extra block: red outline (the real block hides any ghost).
                    if (showMarks && placement.countExtras && !actual.isAir()) {
                        marks.push_back({boxLow, boxHigh, 1.f, .25f, .2f});
                    }
                    continue;
                }
                if (!expected) { outlines.push_back({boxLow, boxHigh, 1.f, .55f, .1f}); continue; } // unknown block name
                if (&actual == expected) {
                    // Placed, but waterlogged where the file has no water, or
                    // not where it has: a state mistake (yellow).
                    Block const* fileLiquid = nullptr;
                    if (auto cellIndex = static_cast<size_t>(structure.cell(local->x, local->y, local->z)); cellIndex < structure.liquids.size())
                        if (auto index = structure.liquids[cellIndex]; index != voidCell && static_cast<size_t>(index) < blocks.blocks.size())
                            fileLiquid = blocks.blocks[static_cast<size_t>(index)];
                    Block const& extra = region.getExtraBlock(pos);
                    // A shulker box opening another way is a state mistake too.
                    int face = expectedShulkerFace(structure, structure.cell(local->x, local->y, local->z), placement.placement);
                    int placedFace = face >= 0 ? actualShulkerFace(region, {x, y, z}) : -1;
                    if (showMarks && (fileLiquid != (extra.isAir() ? nullptr : &extra) || (placedFace >= 0 && placedFace != face)))
                        marks.push_back({boxLow, boxHigh, 1.f, .8f, .2f});
                    continue;
                }
                if (!actual.isAir()) {
                    bool sameType = &actual.getBlockType() == &expected->getBlockType();
                    // Something else is there: red, or yellow when only the state differs.
                    Outline mark = sameType ? Outline{boxLow, boxHigh, 1.f, .8f, .2f} : Outline{boxLow, boxHigh, 1.f, .25f, .2f};
                    if (showMarks) marks.push_back(mark);
                    continue;
                }
                // A missing liquid: a shell, built after the blocks.
                if (liquidKind(*expected)) {
                    liquids.push_back({{x, y, z}, expected});
                    continue;
                }
                // Within two cells of the camera a ghost may own the face the
                // camera sees, so it is never skipped as enclosed.
                if (!camera.near({x, y, z}, 2) && enclosed(region, shown, blocks, {x, y, z})) continue;
                drawing = {x, y, z};
                drawn = paletteIndex;
                glm::vec3 shapeLow{1e9f}, shapeHigh{-1e9f};
                bool meshed = false;
                size_t batchFrom = batch.mMeshData->mPositions->size(), seeFrom = see.mMeshData->mPositions->size();
                // Each render layer the block draws in, into the mesh of its kind.
                eachLayer(*expected, region, pos, [&](std::optional<BlockRenderLayer> layer) {
                    Tessellator& target = layer && blended(*layer) ? see : batch;
                    size_t before = target.mMeshData->mPositions->size();
                    tessellateLayer(own, target, *expected, pos, layer);
                    cullAgainstGhosts(target, before, region, shown, blocks, {x, y, z}, camera, *expected, &target == &see, others);
                    if (std::any_of(camera.cells.begin(), camera.cells.end(), [&](auto const& c) { return c == Point{x, y, z}; }))
                        dropBackFaces(target, before, camera);
                    auto const& positions = target.mMeshData->mPositions.get();
                    for (size_t v = before; v < positions.size(); ++v) {
                        shapeLow = glm::min(shapeLow, positions[v]);
                        shapeHigh = glm::max(shapeHigh, positions[v]);
                    }
                    meshed = meshed || positions.size() > before;
                });
                dropDuplicateQuads(batch, batchFrom);
                dropDuplicateQuads(see, seeFrom);
                // A piston's mesh is its base; the head and arm are its block
                // actor's to draw (L-178), with the file's progress and state.
                if (meshed && expected->getBlockType().mBlockEntityType == BlockActorType::PistonArm) {
                    std::optional<nbt::Compound> data;
                    if (auto found = structure.blockEntities.find(structure.cell(local->x, local->y, local->z));
                        found != structure.blockEntities.end())
                        data = found->second;
                    out.entities.push_back({pos, expected, std::move(data)});
                }
                if (!meshed) {
                    // No block mesh: block entities draw through their renderer;
                    // others keep the outline alone.
                    std::optional<nbt::Compound> data;
                    if (auto found = structure.blockEntities.find(structure.cell(local->x, local->y, local->z));
                        found != structure.blockEntities.end())
                        data = found->second;
                    out.entities.push_back({pos, expected, std::move(data)});
                    // The block's own selection outline (chest, bed, banner...),
                    // read through the view so the ghost is at the cell.
                    AABB buffer;
                    AABB const& shape = expected->getBlockType().getOutline(*expected, view, pos, buffer);
                    glm::vec3 low{shape.min.x, shape.min.y, shape.min.z}, high{shape.max.x, shape.max.y, shape.max.z};
                    glm::vec3 cell{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
                    // Some outlines come relative to the cell.
                    if (glm::any(glm::greaterThan(glm::abs(low - cell), glm::vec3{2.f}))) { low += cell; high += cell; }
                    bool usable = glm::all(glm::greaterThan(high - low, glm::vec3{1e-3f})) && glm::all(glm::lessThan(high - low, glm::vec3{3.f}));
                    outlines.push_back(usable ? Outline{low, high, .35f, .85f, 1.f} : Outline{boxLow, boxHigh, .35f, .85f, 1.f});
                    continue;
                }
                outlines.push_back({shapeLow, shapeHigh, .35f, .85f, 1.f});
            }

    if (batch.mCount) {
        // Section-relative vertices keep float precision far from the origin.
        for (auto& p : batch.mMeshData->mPositions.get()) p -= out.origin;
        finishColors(batch, tint[0], tint[1], tint[2]);
        out.faceVertices = batch.mCount;
        out.faces.emplace(batch.end(Tessellator::UploadMode::Buffered, "Lamium schematic ghosts", SupplementaryFieldAutoGenerationMode{}));
    }
    finishColors(see, tint[0], tint[1], tint[2]);
    // The second layer: water in a waterlogged block, under its ghost (the
    // world's cell still empty). A placed block missing its water gets a
    // yellow mark instead: a shell there looked like real water.
    if (!structure.liquids.empty())
        for (int x = std::max(low.x, origin.x); x < std::min(low.x + sectionSize, origin.x + placed.x); ++x)
            for (int y = std::max(low.y, origin.y); y < std::min(low.y + sectionSize, origin.y + placed.y); ++y)
                for (int z = std::max(low.z, origin.z); z < std::min(low.z + sectionSize, origin.z + placed.z); ++z) {
                    if (!slab.shown({x - origin.x, y - origin.y, z - origin.z})) continue;
                    if (owned && owned({x, y, z})) continue;
                    auto local = toLocal(structure.size, placement.placement, {x, y, z});
                    if (!local) continue;
                    auto index = structure.liquids[static_cast<size_t>(structure.cell(local->x, local->y, local->z))];
                    if (index == voidCell || static_cast<size_t>(index) >= blocks.blocks.size()) continue;
                    Block const* liquid = blocks.blocks[static_cast<size_t>(index)];
                    int kind = liquid ? liquidKind(*liquid) : 0;
                    if (!kind) continue;
                    BlockPos pos{x, y, z};
                    auto* chunk = region.getChunkAt(pos);
                    if (!chunk || chunk->mLoadState->load() < ChunkState::Loaded) continue;
                    Block const& real = region.getBlock(pos);
                    if (liquidKind(real) == kind || liquidKind(region.getBlock(pos, 1)) == kind) continue;
                    // A real block there: placed (yellow if unwaterlogged) or wrong (red).
                    if (!real.isAir()) continue;
                    liquids.push_back({{x, y, z}, liquid});
                }
    drawn = -1;
    for (auto const& [at, liquid] : liquids) {
        int kind = liquidKind(*liquid);
        drawing = at;
        auto around = [&](int dx, int dy, int dz) { return liquidAt(region, shown, blocks, {at.x + dx, at.y + dy, at.z + dz}); };
        // In a waterlogged cell the ghost hides its water where it covers a
        // side (a stair's back): drawn there, the water showed through the
        // translucent ghost, unlike the opaque real block.
        int covered = 0;
        if (auto local = toLocal(structure.size, placement.placement, at)) {
            auto index = structure.blocks[static_cast<size_t>(structure.cell(local->x, local->y, local->z))];
            if (index != voidCell && static_cast<size_t>(index) < blocks.coverMasks.size()) covered = blocks.coverMasks[static_cast<size_t>(index)];
        }
        liquidShell(own, see, BlockPos{at.x, at.y, at.z}, *liquid, [&](int side) {
            // Never against an opaque block, ghost or real: near the camera
            // the tessellator keeps such faces (see ghostNeighbor), and the
            // shell's face then shared a plane with the block's.
            auto const& d = faces::offsets[side];
            Point n{at.x + d[0], at.y + d[1], at.z + d[2]};
            // Nor where a neighbor ghost has a face in the same plane
            // (farmland, a slab's side): the two fought there.
            return !(covered >> side & 1) && around(d[0], d[1], d[2]).kind != kind && !ghostOpaqueAt(region, shown, blocks, n)
                && !(ghostSidesAt(region, shown, blocks, n) >> (side ^ 1) & 1)
                && !region.getBlock(BlockPos{n.x, n.y, n.z}).getBlockType().mIsOpaqueFullBlock;
        }, [&](int cx, int cz) { return liquids::corner(kind, around, cx, cz); }, liquids::flow(kind, around));
    }
    if (!marks.empty()) {
        // Mistakes mark whole cells with a tinted box just outside the real
        // block. The boxes go into the blended mesh, sorted with the blended
        // ghosts: as a separate translucent draw the engine ordered them
        // against those ghosts differently from frame to frame (flicker).
        // Each box is a white concrete block tessellated at the cell, its
        // faces recolored, moved 0.01 out and textured with one texel.
        // Where marks of one color touch, the faces between them and the
        // outlines of cells inside a run are left out: dense wrong or extra
        // areas drew every one of them.
        auto cellOf = [](Outline const& m) {
            return std::tuple<int, int, int>{static_cast<int>(std::floor(m.min.x)), static_cast<int>(std::floor(m.min.y)),
                                             static_cast<int>(std::floor(m.min.z))};
        };
        std::map<std::tuple<int, int, int>, float> colorAt; // keyed cell -> green channel, which tells red from yellow
        for (auto const& m : marks) colorAt[cellOf(m)] = m.g;
        auto sameAt = [&](std::tuple<int, int, int> cell, int side, float g) {
            auto const& d = faces::offsets[side];
            auto found = colorAt.find({std::get<0>(cell) + d[0], std::get<1>(cell) + d[1], std::get<2>(cell) + d[2]});
            return found != colorAt.end() && found->second == g;
        };
        auto white = Block::tryGetFromRegistry(HashedString{"minecraft:white_concrete"});
        drawn = -1;
        // Over a real blended block (honey, stained glass) the box goes into
        // a mesh without depth writes, or the world's translucent layer,
        // drawn after the ghosts, would lose that block behind it.
        Tessellator quads(screen.tessellator.mBufferResourceService);
        quads.begin({}, mce::PrimitiveMode::QuadList, 48, false);
        constexpr int sides[6][4] = {{0,2,6,4},{1,5,7,3},{0,4,5,1},{2,3,7,6},{0,1,3,2},{4,6,7,5}}; // as faces::offsets
        for (auto const& m : marks) {
            auto cell = cellOf(m);
            Point at{std::get<0>(cell), std::get<1>(cell), std::get<2>(cell)};
            auto hidesBox = [&](int side) {
                auto const& d = faces::offsets[side];
                return sameAt(cell, side, m.g) || ghostBoxAt(region, shown, blocks, {at.x + d[0], at.y + d[1], at.z + d[2]});
            };
            if (!white || camera.vibrant) {
                quads.color(m.r, m.g, m.b, .3f);
                glm::vec3 a = m.min - glm::vec3{.01f} - out.origin, b = m.max + glm::vec3{.01f} - out.origin, c[8];
                for (int i = 0; i < 8; ++i) c[i] = {i & 1 ? b.x : a.x, i & 2 ? b.y : a.y, i & 4 ? b.z : a.z};
                int open = 0;
                for (int side = 0; side < 6; ++side) {
                    if (hidesBox(side)) continue;
                    ++open;
                    for (int k = 0; k < 4; ++k) quads.vertex(c[sides[side][k]].x, c[sides[side][k]].y, c[sides[side][k]].z);
                    // The overlay face material under Vibrant Visuals draws both
                    // sides; a second, reversed quad in the same plane flickered.
                    if (!camera.vibrant)
                        for (int k = 3; k >= 0; --k) quads.vertex(c[sides[side][k]].x, c[sides[side][k]].y, c[sides[side][k]].z);
                    out.markVertices += camera.vibrant ? 4 : 8;
                }
                if (open) outlines.push_back(m);
                continue;
            }
            drawing = at;
            size_t from = see.mMeshData->mPositions->size();
            tessellateLayer(own, see, *white, BlockPos{at.x, at.y, at.z}, std::nullopt);
            auto& data = static_cast<mce::MeshData&>(see.mMeshData);
            auto& positions = *data.mPositions;
            auto& uvs = *data.mTextureUVs[0];
            auto& colors = *data.mColors;
            if (colors.size() != positions.size()) colors.resize(positions.size(), 0xffffffffu);
            auto channel = [](float v, int shift) { return static_cast<std::uint32_t>(std::lround(std::clamp(v, 0.f, 1.f) * 255)) << shift; };
            std::uint32_t tint = channel(m.r, 0) | channel(m.g, 8) | channel(m.b, 16) | channel(.3f, 24);
            glm::vec3 center = glm::vec3(at.x, at.y, at.z) + glm::vec3(.5f);
            int open = 0;
            for (size_t q = from; q + 4 <= positions.size(); q += 4) {
                std::array<faces::Vertex, 4> quad;
                for (size_t k = 0; k < 4; ++k) quad[k] = {positions[q + k].x, positions[q + k].y, positions[q + k].z};
                int side = faces::sideOf(quad, at.x, at.y, at.z);
                // The box reaches 0.01 into the next cell: against a ghost that
                // fills its side the face would lie on (or just past) the
                // ghost's own and fight with it at grazing angles (Depth.h
                // rules 1 and 4). The ghost's face shows there instead.
                bool keep = side >= 0 && !hidesBox(side);
                if (!keep) {
                    for (size_t k = 1; k < 4; ++k) positions[q + k] = positions[q];
                    continue;
                }
                ++open;
                glm::vec2 texel{0.f};
                if (uvs.size() == positions.size()) {
                    for (size_t k = 0; k < 4; ++k) texel += uvs[q + k] * .25f;
                    for (size_t k = 0; k < 4; ++k) uvs[q + k] = texel;
                }
                for (size_t k = 0; k < 4; ++k) {
                    positions[q + k] = center + (positions[q + k] - center) * 1.02f;
                    colors[q + k] = tint;
                }
            }
            if (open) outlines.push_back(m);
        }
        // Ended either way, so the tessellator never stays open.
        auto mesh = quads.end(Tessellator::UploadMode::Buffered, "Lamium schematic mistakes", SupplementaryFieldAutoGenerationMode{});
        if (out.markVertices) out.marks.emplace(std::move(mesh));
    }
    if (see.mCount) {
        // Blended quads far to near from where the camera was at the build;
        // sections themselves are ordered when drawn.
        auto const& positions = see.mMeshData->mPositions.get();
        glm::vec3 eye{static_cast<float>(camera.eye.x), static_cast<float>(camera.eye.y), static_cast<float>(camera.eye.z)};
        reorderQuads(see, faces::farToNear(std::span<glm::vec3 const>(positions), eye));
        for (auto& p : see.mMeshData->mPositions.get()) p -= out.origin;
        auto& data = static_cast<mce::MeshData&>(see.mMeshData);
        auto kept = std::make_shared<BlendQuads>();
        kept->origin = out.origin;
        kept->positions = *data.mPositions;
        kept->normals = *data.mNormals;
        kept->tangents = *data.mTangents;
        kept->colors = *data.mColors;
        kept->mers = *data.mMERS;
        kept->bones = *data.mBoneId0s;
        kept->pbr = *data.mPBRTextureIndices;
        for (int i = 0; i < 3; ++i) kept->uvs[i] = *data.mTextureUVs[i];
        kept->geo = *data.mGeoType;
        kept->enabled = *data.mFieldEnabled;
        out.blendVertices = see.mCount;
        out.blend = std::move(kept);
        out.blendMesh.emplace(see.end(Tessellator::UploadMode::Buffered, "Lamium schematic blended ghosts", SupplementaryFieldAutoGenerationMode{}));
    } else {
        // Ended either way, so the tessellator never stays open.
        see.end(Tessellator::UploadMode::Buffered, "Lamium schematic blended ghosts", SupplementaryFieldAutoGenerationMode{});
    }
    // "None" (L-254) leaves out the light-blue ghost outlines; mistake and
    // unknown-block outlines stay, thin.
    int const lineChoice = Runtime::instance().snapshot()->schematic.ghostLines;
    if (lineChoice == 3)
        std::erase_if(outlines, [](Outline const& o) { return o.r == .35f && o.g == .85f && o.b == 1.f; });
    if (!outlines.empty()) {
        std::map<std::tuple<float, float, float>, std::vector<Outline const*>> byColor;
        for (auto const& o : outlines) byColor[{o.r, o.g, o.b}].push_back(&o);
        constexpr int edges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
        int const width = lineChoice == 3 ? 0 : lineChoice;
        // Thin: game lines; wider: bars as quads, drawn with the same material.
        int const perEdge = width > 0 ? 32 : 2;
        auto corners = [](Outline const& o, glm::vec3 grow, glm::vec3 shift, glm::vec3 (&c)[8]) {
            glm::vec3 low = o.min - grow - shift, high = o.max + grow - shift;
            for (int i = 0; i < 8; ++i) c[i] = {i & 1 ? high.x : low.x, i & 2 ? high.y : low.y, i & 4 ? high.z : low.z};
        };
        auto key = [](glm::vec3 const& p, glm::vec3 const& q) { return faces::edgeKey({p.x, p.y, p.z}, {q.x, q.y, q.z}); };
        std::map<std::array<int, 6>, int> strongest;
        for (auto const& o : outlines) {
            glm::vec3 c[8];
            corners(o, glm::vec3{0.f}, glm::vec3{0.f}, c);
            int rank = faces::outlineRank(o.r, o.g, o.b);
            for (auto [i, j] : edges) {
                auto [found, added] = strongest.try_emplace(key(c[i], c[j]), rank);
                if (!added) found->second = std::max(found->second, rank);
            }
        }
        for (auto const& [color, group] : byColor) {
            int const rank = faces::outlineRank(std::get<0>(color), std::get<1>(color), std::get<2>(color));
            float const half = faces::rankedBarHalfWidth(width, rank);
            // Bars sit on the edge itself, so neighbors' bars share one center.
            glm::vec3 const grow{half > 0 ? 0.f : .002f};
            Tessellator lines(screen.tessellator.mBufferResourceService);
            lines.begin({}, half > 0 ? mce::PrimitiveMode::QuadList : mce::PrimitiveMode::LineList,
                        static_cast<int>(group.size() * 12 * perEdge), false);
            auto [r, g, b] = color;
            lines.color(r, g, b, 1.f);
            std::uint32_t drawn = 0;
            for (auto const* o : group) {
                glm::vec3 c[8], world[8];
                corners(*o, grow, out.origin, c);
                corners(*o, glm::vec3{0.f}, glm::vec3{0.f}, world);
                for (auto [i, j] : edges) {
                    if (strongest[key(world[i], world[j])] > rank) continue;
                    drawn += static_cast<std::uint32_t>(perEdge);
                    if (half <= 0) {
                        lines.vertex(c[i].x, c[i].y, c[i].z);
                        lines.vertex(c[j].x, c[j].y, c[j].z);
                        continue;
                    }
                    for (auto const& v : faces::barQuads({c[i].x, c[i].y, c[i].z}, {c[j].x, c[j].y, c[j].z}, half))
                        lines.vertex(v[0], v[1], v[2]);
                }
            }
            auto mesh = lines.end(Tessellator::UploadMode::Buffered, "Lamium schematic outlines", SupplementaryFieldAutoGenerationMode{});
            if (!drawn) continue;
            auto entry = std::make_unique<Section::ColorLines>();
            entry->color = {r, g, b};
            entry->vertices = drawn;
            entry->mesh.emplace(std::move(mesh));
            out.lines.push_back(std::move(entry));
        }
    }
}
}

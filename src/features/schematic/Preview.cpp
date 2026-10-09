#include "features/schematic/Preview.h"
#include "features/schematic/GhostRenderer.h"
#include "features/schematic/GhostFaces.h"
#include "features/schematic/EntityModels.h"
#include "features/schematic/SchematicRegion.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "features/schematic/LiquidShape.h"
#include "features/schematic/SchematicRegion.h"
#include "app/Runtime.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/ActorShaderManager.h"
#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/client/renderer/SupplementaryFieldAutoGenerationMode.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/blockactor/BlockActorRenderDispatcher.h"
#include "mc/client/renderer/blockactor/MovingBlockActorRenderer.h"
#include "mc/client/renderer/ptexture/LightTexture.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core_graphics/enums/PrimitiveMode.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include "mc/deps/minecraft_renderer/renderer/MeshData.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureDescription.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/deps/renderer/Camera.h"
#include "mc/deps/renderer/MatrixStack.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec4.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/material/Material.h"
#include "mc/world/phys/AABB.h"
#include "mc/world/level/block/BlockRenderLayer.h"
#include "mc/world/level/block/BrightnessPair.h"
#include "mc/world/level/block/actor/BlockActorRendererId.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <unordered_map>
#include <limits>
#include <array>
#include <optional>
#include <variant>

namespace lamium::schematic::preview {
namespace {
// The normalized depth of the build's nearest and farthest points in the UI
// pass (see draw()): inside both 0..1 and -1..1, nearer smaller (the pass
// keeps the first fragment, so its test is "less").
constexpr float previewNear = .25f, previewFar = .75f;
// More visible blocks than this are not previewed; the box keeps its text.
constexpr size_t maxBlocks = 120000;
// Blocks tessellated per frame: large previews build over several frames
// while the last finished mesh stays on screen.
constexpr size_t blocksPerFrame = 3000;

void log(std::string const& text) {
    try { Runtime::instance().self().getLogger().info("Schematic preview: {}", text); } catch (...) {}
}

// The finished mesh on screen.
struct Ready {
    Structure const* structure = nullptr;
    Order order;
    std::optional<mce::Mesh> mesh;
    std::uint32_t vertices = 0;
    std::uint64_t tintKey = 0;
    bool valid() const { return mesh && mesh->isValid(); }
    // Something to draw: the mesh, or block entities and entities alone
    // (a file of only chests and signs has no mesh at all).
    bool drawable() const;
};
Ready ready;
// The finished quads of the last built structure, unsorted.
struct Kept {
    std::shared_ptr<Structure const> structure;
    mce::PrimitiveMode mode{};
    std::vector<glm::vec3> positions;
    std::vector<glm::vec4> normals, tangents;
    std::vector<unsigned> colors, mers;
    std::vector<unsigned short> bones, pbr;
    std::vector<glm::vec2> uvs[3];
    std::vector<unsigned char> geo;
    std::array<bool, 15> enabled{};
    std::pair<glm::vec3, glm::vec3> aabb{};
    std::pair<glm::vec2, glm::vec2> uvAabb{};
    std::vector<std::uint32_t> quadCells; // the cell each quad belongs to
    std::vector<bool> quadLiquid;         // whether each quad is a liquid's
    Cut cut;
    // Drawn by their block-entity renderers after the mesh: cell index and
    // block, with their actors made on first draw.
    std::vector<std::pair<std::uint32_t, Block const*>> actorCells;
    std::vector<std::shared_ptr<BlockActor>> actors;
    std::vector<Block const*> palette; // the file's game blocks, for the actors' view
    int baseX = 0, baseY = 0, baseZ = 0; // where cell (0, 0, 0) was tessellated
};
Kept kept;
bool Ready::drawable() const {
    return valid() || (structure && kept.structure.get() == structure && (!kept.actorCells.empty() || !structure->entities.empty()));
}
// The mesh being built.
struct Job {
    std::shared_ptr<Structure const> structure;
    Order order;
    std::vector<Block const*> palette;
    // Per palette entry: a two-block-tall block's other half and the step to it.
    std::vector<Block const*> halves;
    std::vector<int> halfSteps;
    int drawn = -1;         // the palette entry being tessellated
    BlockPos drawingAt{};   // and where
    std::vector<std::uint32_t> cells;
    size_t next = 0;
    std::unique_ptr<SchematicRegion> region; // outlives `blocks`, which reads through it
    std::unique_ptr<BlockTessellator> blocks;
    std::unique_ptr<Tessellator> batch;
    bool failed = false; // too large or nothing to draw: not tried again
    // Where cell (0, 0, 0) is tessellated: around the player at the top of
    // the world, inside the build height, so biome-tinted blocks (grass
    // tops, leaves, vines) take the player's biome. Above the build limit
    // no biome answered and they stayed untinted gray; up there the sky
    // light is full and the world is nearly always air. Builds taller than
    // the world go above the limit as before.
    int baseX = 0, baseY = 384, baseZ = 0;
    std::vector<bool> covers; // per palette entry: hides the faces it touches
    std::vector<std::uint32_t> quadCells;
    std::vector<bool> quadLiquid;
    std::vector<std::pair<std::uint32_t, Block const*>> actorCells;
    Cut cut;
};
std::optional<Job> job;
Last lastDrawn;
// Where and how the last preview was drawn, for clicks.
struct Placed {
    std::shared_ptr<Structure const> structure;
    View view;
    Cut cut;
    float scale = 0, cx = 0, cy = 0;
} placed;

Block const* blockAt(Job const& j, int x, int y, int z) {
    auto const& s = *j.structure;
    if (!j.cut.keeps(x, y, z)) return nullptr;
    auto index = s.blocks[static_cast<size_t>(s.cell(x, y, z))];
    return index >= 0 && static_cast<size_t>(index) < j.palette.size() ? j.palette[static_cast<size_t>(index)] : nullptr;
}

bool covers(Job const& j, int x, int y, int z) {
    auto const& s = *j.structure;
    if (x < 0 || y < 0 || z < 0 || x >= s.size.x || y >= s.size.y || z >= s.size.z || !j.cut.keeps(x, y, z)) return false;
    auto index = s.blocks[static_cast<size_t>(s.cell(x, y, z))];
    return index >= 0 && static_cast<size_t>(index) < j.covers.size() && j.covers[static_cast<size_t>(index)];
}

void start(ScreenContext& screen, BlockSource& region, std::shared_ptr<Structure const> const& structure, Order order, Cut cut,
           int baseX, int baseZ) {
    job.emplace();
    job->cut = cut;
    job->structure = structure;
    job->order = order;
    auto const& s = *structure;
    job->palette.assign(s.palette.size(), nullptr);
    job->halves.assign(s.palette.size(), nullptr);
    job->halfSteps.assign(s.palette.size(), 0);
    for (size_t i = 0; i < s.palette.size(); ++i) {
        if (s.palette[i].isAir()) continue;
        job->palette[i] = ghosts::gameBlock(s.palette[i]);
        if (auto half = otherHalf(s.palette[i]); half && job->palette[i])
            if ((job->halves[i] = ghosts::gameBlock(half->block))) job->halfSteps[i] = half->step;
    }
    {
        int top = region.getMaxHeight(), bottom = region.getMinHeight();
        job->baseY = s.size.y <= top - bottom ? top - s.size.y : top + 64;
    }
    job->baseX = baseX;
    job->baseZ = baseZ;
    // Blocks see the file's blocks as neighbors (doors, fences, panes),
    // at the spots step() draws them.
    job->region = std::make_unique<SchematicRegion>(region);
    job->region->answer = [](BlockPos const& p) -> Block const* {
        if (!job) return nullptr;
        // A door's other half, also where the file has none (the area's edge).
        if (job->drawn >= 0 && job->halfSteps[static_cast<size_t>(job->drawn)] && p.x == job->drawingAt.x && p.z == job->drawingAt.z
            && p.y == job->drawingAt.y + job->halfSteps[static_cast<size_t>(job->drawn)])
            return job->halves[static_cast<size_t>(job->drawn)];
        auto const& size = job->structure->size;
        int x = p.x - job->baseX, y = p.y - job->baseY, z = p.z - job->baseZ;
        if (x < 0 || y < 0 || z < 0 || x >= size.x || y >= size.y || z >= size.z) return nullptr;
        return blockAt(*job, x, y, z);
    };
    job->blocks = std::make_unique<BlockTessellator>(job->region.get());
    // Primed with one appended block: in-world tessellation on a fresh
    // tessellator crashed in the ghost probe.
    if (auto stone = Block::tryGetFromRegistry(HashedString{"minecraft:stone"})) {
        Tessellator primer(screen.tessellator.mBufferResourceService);
        primer.begin({}, mce::PrimitiveMode::QuadList, 64, false);
        job->blocks->appendTessellatedBlock(primer, *stone);
    }
    job->covers.assign(job->palette.size(), false);
    for (size_t i = 0; i < job->palette.size(); ++i)
        if (job->palette[i]) job->covers[i] = ghosts::coversNeighbors(*job->palette[i], *job->blocks, screen);
    // The UI pass keeps the first fragment drawn at a spot (its depth test
    // rejects equal depth), so blocks go near to far: the reverse of the
    // painter's order.
    job->cells = visibleCells(s.size.x, s.size.y, s.size.z, order, [&](int x, int y, int z) { return blockAt(*job, x, y, z) != nullptr; },
        [&](int x, int y, int z) { return covers(*job, x, y, z); });
    std::reverse(job->cells.begin(), job->cells.end());
    if (job->cells.empty() || job->cells.size() > maxBlocks) {
        job->failed = true;
        if (!job->cells.empty()) log(std::format("{} visible blocks, over the {} limit", job->cells.size(), maxBlocks));
        return;
    }
    job->batch = std::make_unique<Tessellator>(screen.tessellator.mBufferResourceService);
    job->batch->begin({}, mce::PrimitiveMode::QuadList, static_cast<int>(std::min<size_t>(job->cells.size() * 24, 1 << 20)), false);
}

// Lighting of its own: the UI pass lit faces by their normals through a
// matrix that flattens depth, so faces flashed bright at some angles and
// large builds lit half and half. All normals point up (one light for all)
// and each face is darkened by its direction, like the world's shading:
// top full, sides 80% and 60%, bottom 50%.
void shade(Tessellator& batch) {
    auto& data = static_cast<mce::MeshData&>(batch.mMeshData);
    auto const& positions = *data.mPositions;
    auto& normals = *data.mNormals;
    auto& colors = *data.mColors;
    size_t vertices = positions.size();
    if (colors.size() != vertices) colors.assign(vertices, 0xffffffffu);
    for (size_t q = 0; q + 4 <= vertices; q += 4) {
        glm::vec3 n = normals.size() == vertices ? glm::vec3(normals[q]) : glm::cross(positions[q + 1] - positions[q], positions[q + 2] - positions[q]);
        float length = glm::length(n);
        float factor = 1;
        if (length > 1e-6f) {
            n /= length;
            // Slanted quads (flames, plants) stay as they are.
            factor = n.y > .5f ? 1.f : n.y < -.5f ? .5f : std::abs(n.x) > .9f ? .6f : std::abs(n.z) > .9f ? .8f : 1.f;
        }
        for (size_t k = 0; k < 4; ++k) {
            auto& c = colors[q + k];
            auto channel = [&](int shift) { return static_cast<std::uint32_t>(std::lround(((c >> shift) & 255) * factor)) << shift; };
            c = channel(0) | channel(8) | channel(16) | (c & 0xff000000u);
        }
    }
    if (normals.size() == vertices) for (auto& v : normals) v = glm::vec4{0, 1, 0, 0};
}

// Reorders the batch's quads near to far for a viewer in the order's octant
// (the UI pass keeps the first fragment at a spot). Every per-vertex stream
// moves with its positions.
// Blocks are ordered by their cell first and quads within a block by their
// center: a large face's center can be nearer than a small block in front
// of it (a trapdoor before a structure block drew behind it).
// The kept quads near to far for a viewer in the order's octant.
// Blocks are ordered by their cell first and quads within a block by their
// center: a large face's center can be nearer than a small block in front
// of it (a trapdoor before a structure block drew behind it). In a cell
// with a liquid, its surface splits the block: the part on the viewer's
// side of the surface is nearer than the liquid, the rest farther (a
// stair's top above the water looked submerged when sorted by centers).
std::vector<std::uint32_t> quadOrder(std::vector<glm::vec3> const& positions, Order order, std::vector<std::uint32_t> const& cells,
                                     std::vector<bool> const& liquid, Size size) {
    size_t quads = positions.size() / 4;
    glm::vec3 toward = glm::normalize(glm::vec3{order.x * .6f, order.y * .7f, order.z * .4f});
    struct Key { float block; int rank; float quad; std::uint32_t index; };
    std::vector<Key> keys(quads);
    bool byCell = cells.size() == quads;
    bool liquids = byCell && liquid.size() == quads;
    // Each liquid cell's surface: the highest point of its liquid's quads.
    std::unordered_map<std::uint32_t, float> surface;
    if (liquids)
        for (size_t q = 0; q < quads; ++q)
            if (liquid[q]) {
                float top = std::max({positions[q * 4].y, positions[q * 4 + 1].y, positions[q * 4 + 2].y, positions[q * 4 + 3].y});
                auto [at, added] = surface.try_emplace(cells[q], top);
                if (!added) at->second = std::max(at->second, top);
            }
    for (size_t q = 0; q < quads; ++q) {
        auto c = (positions[q * 4] + positions[q * 4 + 1] + positions[q * 4 + 2] + positions[q * 4 + 3]) * .25f;
        float block = 0;
        if (byCell) {
            int cell = static_cast<int>(cells[q]);
            glm::vec3 at(cell / (size.y * size.z), cell / size.z % size.y, cell % size.z);
            block = -glm::dot(at, toward);
        }
        // Near to far within a cell: 0 the block's part on the viewer's side
        // of the surface, 1 the liquid, 2 the block's part beyond it.
        int rank = 0;
        if (liquids)
            if (auto found = surface.find(cells[q]); found != surface.end())
                rank = liquid[q] ? 1 : (c.y > found->second) == (toward.y > 0) ? 0 : 2;
        keys[q] = {block, rank, -glm::dot(c, toward), static_cast<std::uint32_t>(q)};
    }
    std::stable_sort(keys.begin(), keys.end(), [](Key const& a, Key const& b) {
        return a.block != b.block ? a.block < b.block : a.rank != b.rank ? a.rank < b.rank : a.quad < b.quad;
    });
    std::vector<std::uint32_t> sorted;
    sorted.reserve(quads);
    for (auto const& key : keys) sorted.push_back(key.index);
    return sorted;
}

// Tessellates the next blocks of the job: each at its cell relative to the
// structure's center, keeping only the faces turned to the viewer and not
// covered by a neighbor. True when the job finished.
bool step() {
    auto& j = *job;
    auto const& s = *j.structure;
    auto& batch = *j.batch;
    glm::vec3 center{s.size.x / 2.f, s.size.y / 2.f, s.size.z / 2.f};
    auto& positions = batch.mMeshData->mPositions.get();
    // Only an opaque full block hides the face it touches (a stair or a
    // trapdoor next to stone leaves the stone's face partly open).
    auto occupied = [&](int x, int y, int z) { return covers(j, x, y, z); };
    for (size_t done = 0; j.next < j.cells.size() && done < blocksPerFrame; ++j.next, ++done) {
        auto cell = static_cast<int>(j.cells[j.next]);
        int x = cell / (s.size.y * s.size.z), y = cell / s.size.z % s.size.y, z = cell % s.size.z;
        glm::vec3 at = glm::vec3(x, y, z) - center;
        size_t from = positions.size();
        // Tessellated as in the world (block states place slabs, trapdoors and
        // grindstones; the item-style path did not), at the top of the world
        // above the player (see Job::baseY), then moved to its cell.
        BlockPos spot{j.baseX + x, j.baseY + y, j.baseZ + z};
        static_cast<bool&>(batch.mApplyTransform) = false;
        j.drawn = s.blocks[static_cast<size_t>(cell)];
        j.drawingAt = spot;
        // What the file has in a cell for a liquid surface (either layer).
        auto liquidIn = [&](int lx, int ly, int lz) {
            if (lx < 0 || ly < 0 || lz < 0 || lx >= s.size.x || ly >= s.size.y || lz >= s.size.z || !j.cut.keeps(lx, ly, lz))
                return liquids::Cell{};
            auto c = static_cast<size_t>(s.cell(lx, ly, lz));
            bool solid = false;
            for (auto const* layer : {&s.blocks, &s.liquids}) {
                if (c >= layer->size()) continue;
                auto i = (*layer)[c];
                if (i < 0 || static_cast<size_t>(i) >= j.palette.size() || !j.palette[static_cast<size_t>(i)]) continue;
                Block const& b = *j.palette[static_cast<size_t>(i)];
                if (int kind = ghosts::liquidKind(b)) return liquids::Cell{kind, layer == &s.blocks ? ghosts::liquidDepth(b) : 0, false};
                solid = solid || b.getMaterial().mSolid;
            }
            return liquids::Cell{0, 0, solid};
        };
        size_t shellFrom = std::numeric_limits<size_t>::max(); // where the liquid quads start, if any
        auto shell = [&](Block const& liquid) {
            int kind = ghosts::liquidKind(liquid);
            shellFrom = positions.size();
            auto around = [&](int dx, int dy, int dz) { return liquidIn(x + dx, y + dy, z + dz); };
            ghosts::liquidShell(*j.blocks, batch, spot, liquid, [&](int side) {
                auto const& d = faces::offsets[side];
                return around(d[0], d[1], d[2]).kind != kind;
            }, [&](int cx, int cz) { return liquids::corner(kind, around, cx, cz); });
        };
        // Every render layer of the block (honey and slime draw in two);
        // liquids as shells, also the water of a waterlogged block.
        Block const& block = *blockAt(j, x, y, z);
        if (ghosts::liquidKind(block)) {
            shell(block);
        } else {
            ghosts::eachLayer(block, *j.region, spot, [&](std::optional<BlockRenderLayer> layer) {
                ghosts::tessellateLayer(*j.blocks, batch, block, spot, layer);
            });
            // No mesh: a block entity, drawn by its renderer after the mesh.
            if (positions.size() == from) j.actorCells.push_back({static_cast<std::uint32_t>(cell), &block});
            if (static_cast<size_t>(cell) < s.liquids.size())
                if (auto i = s.liquids[static_cast<size_t>(cell)]; i >= 0 && static_cast<size_t>(i) < j.palette.size() && j.palette[static_cast<size_t>(i)]
                    && ghosts::liquidKind(*j.palette[static_cast<size_t>(i)]))
                    shell(*j.palette[static_cast<size_t>(i)]);
        }
        glm::vec3 move = at - glm::vec3(spot.x, spot.y, spot.z);
        for (size_t v = from; v < positions.size(); ++v) positions[v] += move;
        j.quadCells.insert(j.quadCells.end(), (positions.size() - from) / 4, static_cast<std::uint32_t>(cell));
        size_t liquidFrom = std::clamp(shellFrom, from, positions.size());
        j.quadLiquid.insert(j.quadLiquid.end(), (liquidFrom - from) / 4, false);
        j.quadLiquid.insert(j.quadLiquid.end(), (positions.size() - liquidFrom) / 4, true);
        // Faces lying on the cell's side against an occupied neighbor are
        // never seen: collapse them. Everything else stays (back faces too:
        // the near-to-far sort below puts them behind the front ones).
        for (size_t q = from; q + 4 <= positions.size(); q += 4) {
            for (int axis = 0; axis < 3; ++axis) {
                float a = positions[q][axis] - at[axis];
                bool flat = true;
                for (size_t k = 1; k < 4; ++k) flat = flat && std::abs(positions[q + k][axis] - positions[q][axis]) < 1e-4f;
                if (!flat || (a > .001f && a < .999f)) continue;
                int d[3]{0, 0, 0};
                d[axis] = a >= .999f ? 1 : -1;
                if (occupied(x + d[0], y + d[1], z + d[2])) for (size_t k = 1; k < 4; ++k) positions[q + k] = positions[q];
                break;
            }
        }
    }
    if (j.next < j.cells.size()) return false;
    shade(batch);
    // The quads do not depend on the view, only their order: keep them so
    // another view is a sort and an upload, not a new tessellation (the
    // front vanished for a second while a large build was redone).
    auto& data = static_cast<mce::MeshData&>(batch.mMeshData);
    kept.structure = j.structure;
    kept.mode = data.mMode;
    kept.positions = *data.mPositions;
    kept.normals = *data.mNormals;
    kept.tangents = *data.mTangents;
    kept.colors = *data.mColors;
    kept.bones = *data.mBoneId0s;
    for (int i = 0; i < 3; ++i) kept.uvs[i] = *data.mTextureUVs[i];
    kept.pbr = *data.mPBRTextureIndices;
    kept.mers = *data.mMERS;
    kept.geo = *data.mGeoType;
    kept.enabled = *data.mFieldEnabled;
    kept.aabb = *data.mAABB;
    kept.uvAabb = *data.mUVAABB;
    kept.quadCells = std::move(j.quadCells);
    kept.quadLiquid = std::move(j.quadLiquid);
    kept.actorCells = std::move(j.actorCells);
    kept.actors.assign(kept.actorCells.size(), nullptr);
    kept.palette = j.palette;
    kept.baseX = j.baseX;
    kept.baseY = j.baseY;
    kept.baseZ = j.baseZ;
    kept.cut = j.cut;
    return true;
}

// One mesh from the kept quads in `quads`, tinted, built in `out`
// (meshes cannot be assigned).
void meshFrom(ScreenContext& screen, std::vector<std::uint32_t> const& quads, Tint const* tint, std::optional<mce::Mesh>& out) {
    if (quads.empty()) return;
    size_t total = kept.positions.size();
    Tessellator batch(screen.tessellator.mBufferResourceService);
    batch.begin({}, kept.mode, static_cast<int>(quads.size() * 4), false);
    auto& data = static_cast<mce::MeshData&>(batch.mMeshData);
    auto pick = [&](auto const& from, auto& to) {
        to.clear();
        if (from.size() != total) return;
        to.reserve(quads.size() * 4);
        for (auto q : quads)
            for (size_t k = 0; k < 4; ++k) to.push_back(from[q * 4 + k]);
    };
    pick(kept.positions, *data.mPositions);
    pick(kept.normals, *data.mNormals);
    pick(kept.tangents, *data.mTangents);
    pick(kept.colors, *data.mColors);
    pick(kept.bones, *data.mBoneId0s);
    for (int i = 0; i < 3; ++i) pick(kept.uvs[i], *data.mTextureUVs[i]);
    pick(kept.pbr, *data.mPBRTextureIndices);
    pick(kept.mers, *data.mMERS);
    pick(kept.geo, *data.mGeoType);
    if (tint && tint->color && kept.quadCells.size() * 4 == total && data.mColors->size() == quads.size() * 4) {
        auto const& s = *kept.structure;
        auto& colors = *data.mColors;
        for (size_t i = 0; i < quads.size(); ++i) {
            int cell = static_cast<int>(kept.quadCells[quads[i]]);
            std::uint32_t m = tint->color(cell / (s.size.y * s.size.z), cell / s.size.z % s.size.y, cell % s.size.z);
            if (m == 0xffffffffu) continue;
            for (size_t k = 0; k < 4; ++k) {
                auto& c = colors[i * 4 + k];
                auto channel = [&](int shift) {
                    return ((((c >> shift) & 255) * ((m >> shift) & 255) + 127) / 255) << shift;
                };
                c = channel(0) | channel(8) | channel(16) | (c & 0xff000000u);
            }
        }
    }
    *data.mFieldEnabled = kept.enabled;
    *data.mAABB = kept.aabb;
    *data.mUVAABB = kept.uvAabb;
    static_cast<unsigned&>(batch.mCount) = static_cast<unsigned>(quads.size() * 4);
    out.emplace(batch.end(Tessellator::UploadMode::Buffered, "Lamium schematic preview", SupplementaryFieldAutoGenerationMode{}));
}
// Block entities and entities of the kept structure, in the preview's model
// space (cell corners at their cell minus the center), inside the pushed
// model matrix: block entities by their renderers against a view answering
// the file's blocks where the cells were tessellated, entities as the
// world's entity ghosts draw them.
void drawActors(MinecraftUIRenderContext& context, BlockSource& region, Structure const& s) {
    if (kept.structure.get() != &s) return;
    glm::vec3 center{s.size.x / 2.f, s.size.y / 2.f, s.size.z / 2.f};
    auto& screen = static_cast<ScreenContext&>(context.mScreenContext);
    IClientInstance& client = context.mClient;
    if (!kept.actorCells.empty()) {
        BaseActorRenderContext actorContext(screen, client, client.getMinecraftGame_DEPRECATED());
        SchematicRegion view(region);
        view.fullLight = true;
        view.answer = [&](BlockPos const& p) -> Block const* {
            int x = p.x - kept.baseX, y = p.y - kept.baseY, z = p.z - kept.baseZ;
            if (x < 0 || y < 0 || z < 0 || x >= s.size.x || y >= s.size.y || z >= s.size.z || !kept.cut.keeps(x, y, z)) return nullptr;
            auto index = s.blocks[static_cast<size_t>(s.cell(x, y, z))];
            return index >= 0 && static_cast<size_t>(index) < kept.palette.size() ? kept.palette[static_cast<size_t>(index)] : nullptr;
        };
        for (size_t i = 0; i < kept.actorCells.size(); ++i) {
            auto [cell, block] = kept.actorCells[i];
            int x = static_cast<int>(cell) / (s.size.y * s.size.z), y = static_cast<int>(cell) / s.size.z % s.size.y, z = static_cast<int>(cell) % s.size.z;
            if (!kept.cut.keeps(x, y, z)) continue;
            BlockPos worldPos{kept.baseX + x, kept.baseY + y, kept.baseZ + z};
            if (!kept.actors[i]) {
                auto found = s.blockEntities.find(static_cast<std::int32_t>(cell));
                kept.actors[i] = ghosts::makeBlockActor(*block, worldPos, found != s.blockEntities.end() ? &found->second : nullptr,
                                                        Placement{}, region);
                if (!kept.actors[i]) continue;
            }
            Vec3 renderPos{x - center.x, y - center.y, z - center.z};
            ghosts::renderBlockActor(actorContext, view, *kept.actors[i], *block, renderPos, worldPos);
        }
    }
    std::vector<models::Spot> spots;
    for (auto const& entity : s.entities) {
        if (entity.identifier.empty() || spots.size() >= 256) continue;
        if (!kept.cut.keeps(static_cast<int>(std::floor(entity.x)), static_cast<int>(std::floor(entity.y)), static_cast<int>(std::floor(entity.z))))
            continue;
        float yaw = 0;
        if (auto const* turn = entity.data.find("Rotation"); turn && turn->as<nbt::List>() && !turn->as<nbt::List>()->items.empty())
            if (auto const* v = turn->as<nbt::List>()->items.front().as<float>()) yaw = *v;
        spots.push_back({{entity.x - center.x, entity.y - center.y, entity.z - center.z}, entity.identifier, yaw});
    }
    if (!spots.empty()) models::draw(screen, client, Vec3{0, 0, 0}, spots, [](std::function<void()> const& draw) { draw(); }, false);
}
// Uploads the kept quads sorted far to near for `order`, as one mesh drawn
// blended over real depth: nearer quads cover farther ones, water lets them
// show through. Two meshes (solids, then liquids) swapped order from frame
// to frame (a rail under water flickered).
void upload(ScreenContext& screen, Order order, Tint const* tint) {
    ready.tintKey = tint ? tint->key : 0;
    ready.mesh.reset();
    ready.structure = kept.structure.get();
    ready.order = order;
    ready.vertices = 0;
    if (kept.positions.empty()) return;
    auto sorted = quadOrder(kept.positions, order, kept.quadCells, kept.quadLiquid, kept.structure->size);
    std::reverse(sorted.begin(), sorted.end());
    meshFrom(screen, sorted, tint, ready.mesh);
    ready.vertices = static_cast<std::uint32_t>(sorted.size() * 4);
}
void clear() {
    placed = {};
    kept = {};
    ready.mesh.reset();
    ready.structure = nullptr;
    ready.vertices = 0;
    job.reset();
}
} // namespace

bool draw(MinecraftUIRenderContext& context, std::shared_ptr<Structure const> const& structure, float x, float y, float width, float height,
          View view, Tint const* tint) {
    if (!structure || width < 8 || height < 8) return false;
    IClientInstance& client = context.mClient;
    auto* region = client.getRegion();
    if (!region) return false;
    auto& screen = static_cast<ScreenContext&>(context.mScreenContext);
    auto order = drawOrder(view);
    auto const& size = structure->size;
    int baseX = -size.x / 2, baseZ = -size.z / 2;
    if (auto* player = client.getLocalPlayer()) {
        auto at = player->getPosition();
        baseX += static_cast<int>(std::floor(at.x));
        baseZ += static_cast<int>(std::floor(at.z));
    }
    Cut cut = cutFor(view, size.x, size.y, size.z, view.peel);
    lastDrawn.maxZoom = maxZoom(view, size.x, size.y, size.z, width, height);
    lastDrawn.axis = cut.axis;
    lastDrawn.layers = layersAlong(cut, size.x, size.y, size.z);
    view.zoom = std::clamp(view.zoom, .1f, lastDrawn.maxZoom);
    try {
        std::uint64_t key = tint ? tint->key : 0;
        bool current = ready.structure == structure.get() && ready.order == order && ready.tintKey == key && ready.drawable();
        // A new cut needs new quads (faces inside the build become visible);
        // the last mesh stays up while they build.
        if (kept.structure == structure && !(kept.cut == cut) && (!job || job->structure != structure || !(job->cut == cut)))
            start(screen, *region, structure, order, cut, baseX, baseZ);
        else if (!current && kept.structure == structure && kept.cut == cut) {
            upload(screen, order, tint);
        } else if (!current && kept.structure != structure && (!job || job->structure != structure || !(job->cut == cut))) {
            start(screen, *region, structure, order, cut, baseX, baseZ);
        }
        if (job && !job->failed && step()) {
            job.reset();
            upload(screen, order, tint);
        }
        if (ready.structure != structure.get() || !ready.drawable()) return false;
        auto& dispatcher = client.getBlockEntityRenderDispatcher();
        auto* moving = static_cast<MovingBlockActorRenderer*>(dispatcher.mRenderers.get()[BlockActorRendererId::MovingBlock].get());
        auto* lightTexture = client.getLightTexture();
        if (!moving || !lightTexture) return false;
        // The block blend material, which writes depth: depth corrects what
        // the far-to-near order gets wrong inside a cell (without depth
        // writes, a stair's top above the water looked submerged). Its
        // cost, accepted (maintainer, 2026-10-09): empty texels write depth
        // too, so of seagrass's two crossing quads the one drawn first can
        // hide part of the other.
        mce::MaterialPtr const& material = moving->mBlockMaterials[static_cast<int>(BlockRenderLayer::RenderlayerBlend)].get();
        if (!material.mRenderMaterialInfoPtr) return false;
        std::variant<std::monostate, mce::TexturePtr, mce::ClientTexture, mce::ServerTexture> texture{moving->mAtlasTexture.get()};

        // Model (centered blocks) to UI: right, down and toward the viewer,
        // scaled to fit. Depth is real but kept inside the pass's depth
        // range: read from the current matrices, the build spans normalized
        // depth 0.25 (nearest) to 0.75 (a depth as deep as the build is wide
        // fell outside it and cut blocks apart; a fixed 0.5 UI units drew
        // nothing). Quads are sorted far to near and blended, so water
        // shows what lies behind it.
        auto r = project(view, 1, 0, 0), u = project(view, 0, 1, 0), f = project(view, 0, 0, 1);
        float scale = fitScale(structure->size.x, structure->size.y, structure->size.z, width, height) * std::max(view.zoom, .1f);
        float depth = 0, middle = 0; // UI z per block toward the viewer, and the build center's z
        if (!screen.camera.viewMatrixStack->stack->empty() && !screen.camera.projectionMatrixStack->stack->empty()
            && !screen.camera.worldMatrixStack->stack->empty()) {
            glm::mat4 clip = *screen.camera.projectionMatrixStack->top()._m * *screen.camera.viewMatrixStack->top()._m
                * *screen.camera.worldMatrixStack->top()._m;
            glm::vec4 base = clip * glm::vec4(x + width / 2, y + height / 2, 0, 1);
            glm::vec4 step = clip * glm::vec4(x + width / 2, y + height / 2, 1, 1) - base;
            float half = .5f * std::sqrt(static_cast<float>(structure->size.x * structure->size.x + structure->size.y * structure->size.y
                                                            + structure->size.z * structure->size.z));
            if (std::abs(step.z) > 1e-12f && std::abs(step.w) < 1e-6f && std::abs(base.w) > 1e-6f && half > 0) {
                auto at = [&](float ndc) { return (ndc * base.w - base.z) / step.z; };
                float nearest = at(previewNear), farthest = at(previewFar);
                middle = (nearest + farthest) / 2;
                depth = (farthest - nearest) / (2 * half);
            }
            static bool logged = false;
            if (!std::exchange(logged, true))
                log(std::format("depth: z at UI z 0 is {:.6g}/{:.6g}, per UI unit {:.6g}/{:.6g}; build middle z {:.6g}, {:.6g} per block",
                    base.z, base.w, step.z, step.w, middle, depth));
        }
        glm::mat4 model{1.f};
        model[0] = {scale * r.right, scale * r.down, -depth * r.toward, 0};
        model[1] = {scale * u.right, scale * u.down, -depth * u.toward, 0};
        model[2] = {scale * f.right, scale * f.down, -depth * f.toward, 0};
        model[3] = {x + width / 2, y + height / 2, middle, 1};
        placed = {structure, view, cut, scale, x + width / 2, y + height / 2};
        context.flushText(0, std::nullopt);
        // No clipping: the UI scissor (in GUI units, in pixels, or committed
        // by a UI draw) never reached this mesh, so the zoom is held at the
        // size that fits the box (maxZoom).
        auto ref = screen.camera.worldMatrixStack->push(false);
        ref.stack->_isDirty = true;
        ref.mat->_m = ref.mat->_m.get() * model;
        auto pop = [&] {
            ref.stack->_isDirty = true;
            if (ref.stack->sortOrigin->has_value() && (ref.stack->stack->size() - 1) <= ref.stack->sortOrigin->value())
                ref.stack->sortOrigin->reset();
            ref.stack->stack->pop_back();
            ref.mat = nullptr;
            ref.stack = nullptr;
        };
        try {
            BrightnessPair full;
            full.sky->mValue = 15;
            full.block->mValue = 15;
            ActorShaderManager::setupShaderParameters(screen, *region, full, glm::vec4{1, 1, 1, 1}, 1.f, true, *lightTexture, Vec2{1, 1},
                Vec4{0, 0, 1, 1});
            // Block entities and entities first: the mesh's empty texels write
            // depth (a campfire's flames hid what stood behind them), and the
            // mesh drawn after still covers them where blocks are in front.
            drawActors(context, *region, *structure);
            if (ready.valid()) {
                ActorShaderManager::setupShaderParameters(screen, *region, full, glm::vec4{1, 1, 1, 1}, 1.f, true, *lightTexture, Vec2{1, 1},
                    Vec4{0, 0, 1, 1});
                ready.mesh->renderMesh(screen, material, texture, 0, ready.vertices, OffscreenCaptureDescription{}, nullptr);
            }
        } catch (...) {
            pop();
            throw;
        }
        pop();
        return true;
    } catch (std::exception const& error) {
        static bool reported = false;
        if (!std::exchange(reported, true)) log(std::string("drawing failed: ") + error.what());
        job.reset();
        clear();
        return false;
    }
}

void reset() { clear(); }
Last last() { return lastDrawn; }
std::optional<Cell> pickAt(float x, float y) {
    if (!placed.structure || placed.scale <= 0) return std::nullopt;
    auto const& s = *placed.structure;
    auto occupied = [&](int cx, int cy, int cz) {
        if (!placed.cut.keeps(cx, cy, cz)) return false;
        auto index = s.blocks[static_cast<size_t>(s.cell(cx, cy, cz))];
        return index >= 0 && static_cast<size_t>(index) < s.palette.size() && !s.palette[static_cast<size_t>(index)].isAir();
    };
    return pick(placed.view, s.size.x, s.size.y, s.size.z, (x - placed.cx) / placed.scale, (y - placed.cy) / placed.scale, occupied);
}
} // namespace lamium::schematic::preview

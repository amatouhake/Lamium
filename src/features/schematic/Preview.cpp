#include "features/schematic/Preview.h"
#include "features/schematic/GhostRenderer.h"
#include "app/Runtime.h"
#include "mc/client/game/IClientInstance.h"
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
#include "mc/world/phys/AABB.h"
#include "mc/world/level/block/BlockRenderLayer.h"
#include "mc/world/level/block/BrightnessPair.h"
#include "mc/world/level/block/actor/BlockActorRendererId.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <optional>
#include <variant>

namespace lamium::schematic::preview {
namespace {
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
};
Kept kept;
// The mesh being built.
struct Job {
    std::shared_ptr<Structure const> structure;
    Order order;
    std::vector<Block const*> palette;
    std::vector<std::uint32_t> cells;
    size_t next = 0;
    std::unique_ptr<BlockTessellator> blocks;
    std::unique_ptr<Tessellator> batch;
    bool failed = false; // too large or nothing to draw: not tried again
    int height = 320;    // the dimension's build limit
    std::vector<bool> covers; // per palette entry: hides the faces it touches
    std::vector<std::uint32_t> quadCells;
};
std::optional<Job> job;

Block const* blockAt(Job const& j, int x, int y, int z) {
    auto const& s = *j.structure;
    auto index = s.blocks[static_cast<size_t>(s.cell(x, y, z))];
    return index >= 0 && static_cast<size_t>(index) < j.palette.size() ? j.palette[static_cast<size_t>(index)] : nullptr;
}

bool covers(Job const& j, int x, int y, int z) {
    auto const& s = *j.structure;
    if (x < 0 || y < 0 || z < 0 || x >= s.size.x || y >= s.size.y || z >= s.size.z) return false;
    auto index = s.blocks[static_cast<size_t>(s.cell(x, y, z))];
    return index >= 0 && static_cast<size_t>(index) < j.covers.size() && j.covers[static_cast<size_t>(index)];
}

void start(ScreenContext& screen, BlockSource& region, std::shared_ptr<Structure const> const& structure, Order order) {
    job.emplace();
    job->structure = structure;
    job->order = order;
    auto const& s = *structure;
    job->palette.assign(s.palette.size(), nullptr);
    for (size_t i = 0; i < s.palette.size(); ++i)
        if (!s.palette[i].isAir()) job->palette[i] = ghosts::gameBlock(s.palette[i]);
    job->height = region.getMaxHeight();
    job->blocks = std::make_unique<BlockTessellator>(&region);
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
void sortQuads(Tessellator& batch, Order order, std::vector<std::uint32_t> const& cells, Size size) {
    auto& data = static_cast<mce::MeshData&>(batch.mMeshData);
    auto& positions = *data.mPositions;
    size_t quads = positions.size() / 4;
    if (quads < 2) return;
    glm::vec3 toward = glm::normalize(glm::vec3{order.x * .6f, order.y * .7f, order.z * .4f});
    struct Key { float block, quad; std::uint32_t index; };
    std::vector<Key> keys(quads);
    bool byCell = cells.size() == quads;
    for (size_t q = 0; q < quads; ++q) {
        auto c = (positions[q * 4] + positions[q * 4 + 1] + positions[q * 4 + 2] + positions[q * 4 + 3]) * .25f;
        float block = 0;
        if (byCell) {
            int cell = static_cast<int>(cells[q]);
            glm::vec3 at(cell / (size.y * size.z), cell / size.z % size.y, cell % size.z);
            block = -glm::dot(at, toward);
        }
        keys[q] = {block, -glm::dot(c, toward), static_cast<std::uint32_t>(q)};
    }
    std::stable_sort(keys.begin(), keys.end(), [](Key const& a, Key const& b) { return a.block != b.block ? a.block < b.block : a.quad < b.quad; });
    auto permute = [&](auto& stream) {
        if (stream.size() != quads * 4) return;
        auto copy = stream;
        for (size_t q = 0; q < quads; ++q)
            for (size_t k = 0; k < 4; ++k) stream[q * 4 + k] = copy[keys[q].index * 4 + k];
    };
    permute(positions);
    permute(*data.mNormals);
    permute(*data.mTangents);
    permute(*data.mColors);
    permute(*data.mBoneId0s);
    for (int i = 0; i < 3; ++i) permute(*data.mTextureUVs[i]);
    permute(*data.mPBRTextureIndices);
    permute(*data.mMERS);
    permute(*data.mGeoType);
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
        // grindstones; the item-style path did not), at a spot above the
        // build limit so no real neighbor or light changes it, then moved to
        // its cell.
        BlockPos spot{x, j.height + 64 + y, z};
        static_cast<bool&>(batch.mApplyTransform) = false;
        j.blocks->tessellateInWorld(batch, *blockAt(j, x, y, z), spot, false);
        glm::vec3 move = at - glm::vec3(spot.x, spot.y, spot.z);
        for (size_t v = from; v < positions.size(); ++v) positions[v] += move;
        j.quadCells.insert(j.quadCells.end(), (positions.size() - from) / 4, static_cast<std::uint32_t>(cell));
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
    return true;
}

// Uploads the kept quads sorted for `order`.
void upload(ScreenContext& screen, Order order, Tint const* tint) {
    ready.tintKey = tint ? tint->key : 0;
    ready.mesh.reset();
    ready.structure = kept.structure.get();
    ready.order = order;
    ready.vertices = static_cast<std::uint32_t>(kept.positions.size());
    if (!ready.vertices) return;
    Tessellator batch(screen.tessellator.mBufferResourceService);
    batch.begin({}, kept.mode, static_cast<int>(ready.vertices), false);
    auto& data = static_cast<mce::MeshData&>(batch.mMeshData);
    *data.mPositions = kept.positions;
    *data.mNormals = kept.normals;
    *data.mTangents = kept.tangents;
    *data.mColors = kept.colors;
    if (tint && tint->color && kept.quadCells.size() * 4 == kept.colors.size()) {
        auto const& s = *kept.structure;
        auto& colors = *data.mColors;
        for (size_t q = 0; q < kept.quadCells.size(); ++q) {
            int cell = static_cast<int>(kept.quadCells[q]);
            std::uint32_t m = tint->color(cell / (s.size.y * s.size.z), cell / s.size.z % s.size.y, cell % s.size.z);
            if (m == 0xffffffffu) continue;
            for (size_t k = 0; k < 4; ++k) {
                auto& c = colors[q * 4 + k];
                auto channel = [&](int shift) {
                    return ((((c >> shift) & 255) * ((m >> shift) & 255) + 127) / 255) << shift;
                };
                c = channel(0) | channel(8) | channel(16) | (c & 0xff000000u);
            }
        }
    }
    *data.mBoneId0s = kept.bones;
    for (int i = 0; i < 3; ++i) *data.mTextureUVs[i] = kept.uvs[i];
    *data.mPBRTextureIndices = kept.pbr;
    *data.mMERS = kept.mers;
    *data.mGeoType = kept.geo;
    *data.mFieldEnabled = kept.enabled;
    *data.mAABB = kept.aabb;
    *data.mUVAABB = kept.uvAabb;
    static_cast<unsigned&>(batch.mCount) = ready.vertices;
    sortQuads(batch, order, kept.quadCells, kept.structure->size);
    ready.mesh.emplace(batch.end(Tessellator::UploadMode::Buffered, "Lamium schematic preview", SupplementaryFieldAutoGenerationMode{}));
}
void clear() {
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
    try {
        std::uint64_t key = tint ? tint->key : 0;
        bool current = ready.structure == structure.get() && ready.order == order && ready.tintKey == key && ready.mesh && ready.mesh->isValid();
        if (!current && kept.structure == structure) {
            upload(screen, order, tint);
        } else if (!current && (!job || job->structure != structure)) {
            start(screen, *region, structure, order);
        }
        if (job && !job->failed && step()) {
            job.reset();
            upload(screen, order, tint);
        }
        if (ready.structure != structure.get() || !ready.mesh || !ready.mesh->isValid()) return false;
        auto& dispatcher = client.getBlockEntityRenderDispatcher();
        auto* moving = static_cast<MovingBlockActorRenderer*>(dispatcher.mRenderers.get()[BlockActorRendererId::MovingBlock].get());
        auto* lightTexture = client.getLightTexture();
        if (!moving || !lightTexture) return false;
        mce::MaterialPtr const& material = moving->mBlockMaterials[static_cast<int>(BlockRenderLayer::RenderlayerAlphatest)].get();
        if (!material.mRenderMaterialInfoPtr) return false;
        std::variant<std::monostate, mce::TexturePtr, mce::ClientTexture, mce::ServerTexture> texture{moving->mAtlasTexture.get()};

        // Model (centered blocks) to UI: right, down and toward the viewer,
        // scaled to fit. Depth stays flat: the UI pass keeps the first
        // fragment at a spot (a real z cut blocks apart), so quads are
        // sorted near to far instead.
        auto r = project(view, 1, 0, 0), u = project(view, 0, 1, 0), f = project(view, 0, 0, 1);
        float scale = fitScale(structure->size.x, structure->size.y, structure->size.z, width, height) * std::max(view.zoom, .1f);
        glm::mat4 model{1.f};
        model[0] = {scale * r.right, scale * r.down, 0, 0};
        model[1] = {scale * u.right, scale * u.down, 0, 0};
        model[2] = {scale * f.right, scale * f.down, 0, 0};
        model[3] = {x + width / 2, y + height / 2, 0, 1};
        context.flushText(0, std::nullopt);
        // The UI applies its scissor when it draws, not when it is set, so a
        // nearly invisible fill is drawn right after setting it (and after
        // clearing it): the mesh drawn in between is clipped too. Neither GUI
        // units nor pixels alone clipped a zoomed preview.
        RectangleArea box{x, x + width, y, y + height};
        auto commit = [&] { context.fillRectangle(box, mce::Color{0, 0, 0, 1}, .004f); };
        context.enableScissorTest(box);
        commit();
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
            ready.mesh->renderMesh(screen, material, texture, 0, ready.vertices, OffscreenCaptureDescription{}, nullptr);
        } catch (...) {
            pop();
            context.disableScissorTest();
            commit();
            throw;
        }
        pop();
        context.disableScissorTest();
        commit();
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
} // namespace lamium::schematic::preview

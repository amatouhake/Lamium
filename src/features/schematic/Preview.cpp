#include "features/schematic/Preview.h"
#include "features/schematic/GhostRenderer.h"
#include "app/Runtime.h"
#include "mc/client/game/IClientInstance.h"
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
#include "mc/world/level/block/BlockRenderLayer.h"
#include "mc/world/level/block/BrightnessPair.h"
#include "mc/world/level/block/actor/BlockActorRendererId.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
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
};
Ready ready;
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
};
std::optional<Job> job;

Block const* blockAt(Job const& j, int x, int y, int z) {
    auto const& s = *j.structure;
    auto index = s.blocks[static_cast<size_t>(s.cell(x, y, z))];
    return index >= 0 && static_cast<size_t>(index) < j.palette.size() ? j.palette[static_cast<size_t>(index)] : nullptr;
}

void start(ScreenContext& screen, BlockSource& region, std::shared_ptr<Structure const> const& structure, Order order) {
    job.emplace();
    job->structure = structure;
    job->order = order;
    auto const& s = *structure;
    job->palette.assign(s.palette.size(), nullptr);
    for (size_t i = 0; i < s.palette.size(); ++i)
        if (!s.palette[i].isAir()) job->palette[i] = ghosts::gameBlock(s.palette[i]);
    // The UI pass keeps the first fragment drawn at a spot (its depth test
    // rejects equal depth), so blocks go near to far: the reverse of the
    // painter's order.
    job->cells = visibleCells(s.size.x, s.size.y, s.size.z, order, [&](int x, int y, int z) { return blockAt(*job, x, y, z) != nullptr; },
        [&](int x, int y, int z) { auto const* b = blockAt(*job, x, y, z); return b && b->getBlockType().mIsOpaqueFullBlock; });
    std::reverse(job->cells.begin(), job->cells.end());
    if (job->cells.empty() || job->cells.size() > maxBlocks) {
        job->failed = true;
        if (!job->cells.empty()) log(std::format("{} visible blocks, over the {} limit", job->cells.size(), maxBlocks));
        return;
    }
    job->blocks = std::make_unique<BlockTessellator>(&region);
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
            factor = n.y > .5f ? 1.f : n.y < -.5f ? .5f : std::abs(n.x) > std::abs(n.z) ? .6f : .8f;
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
void sortQuads(Tessellator& batch, Order order) {
    auto& data = static_cast<mce::MeshData&>(batch.mMeshData);
    auto& positions = *data.mPositions;
    size_t quads = positions.size() / 4;
    if (quads < 2) return;
    glm::vec3 toward = glm::normalize(glm::vec3{order.x * .6f, order.y * .7f, order.z * .4f});
    std::vector<std::pair<float, std::uint32_t>> keys(quads);
    for (size_t q = 0; q < quads; ++q) {
        auto c = (positions[q * 4] + positions[q * 4 + 1] + positions[q * 4 + 2] + positions[q * 4 + 3]) * .25f;
        keys[q] = {-glm::dot(c, toward), static_cast<std::uint32_t>(q)};
    }
    std::stable_sort(keys.begin(), keys.end(), [](auto const& a, auto const& b) { return a.first < b.first; });
    auto permute = [&](auto& stream) {
        if (stream.size() != quads * 4) return;
        auto copy = stream;
        for (size_t q = 0; q < quads; ++q)
            for (size_t k = 0; k < 4; ++k) stream[q * 4 + k] = copy[keys[q].second * 4 + k];
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
    auto occupied = [&](int x, int y, int z) {
        if (x < 0 || y < 0 || z < 0 || x >= s.size.x || y >= s.size.y || z >= s.size.z) return false;
        auto const* b = blockAt(j, x, y, z);
        return b && b->getBlockType().mIsOpaqueFullBlock;
    };
    for (size_t done = 0; j.next < j.cells.size() && done < blocksPerFrame; ++j.next, ++done) {
        auto cell = static_cast<int>(j.cells[j.next]);
        int x = cell / (s.size.y * s.size.z), y = cell / s.size.z % s.size.y, z = cell % s.size.z;
        glm::vec3 at = glm::vec3(x, y, z) - center;
        size_t from = positions.size();
        static_cast<bool&>(batch.mApplyTransform) = true;
        static_cast<glm::mat4x4&>(batch.mTransformMatrix) = glm::translate(glm::mat4{1.f}, at);
        j.blocks->appendTessellatedBlock(batch, *blockAt(j, x, y, z));
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
    sortQuads(batch, j.order);
    ready.mesh.reset();
    ready.structure = j.structure.get();
    ready.order = j.order;
    ready.vertices = static_cast<std::uint32_t>(positions.size());
    if (ready.vertices)
        ready.mesh.emplace(batch.end(Tessellator::UploadMode::Buffered, "Lamium schematic preview", SupplementaryFieldAutoGenerationMode{}));
    return true;
}
void clear() {
    ready.mesh.reset();
    ready.structure = nullptr;
    ready.vertices = 0;
    job.reset();
}
} // namespace

bool draw(MinecraftUIRenderContext& context, std::shared_ptr<Structure const> const& structure, float x, float y, float width, float height,
          View view) {
    if (!structure || width < 8 || height < 8) return false;
    IClientInstance& client = context.mClient;
    auto* region = client.getRegion();
    if (!region) return false;
    auto& screen = static_cast<ScreenContext&>(context.mScreenContext);
    auto order = drawOrder(view);
    try {
        bool current = ready.structure == structure.get() && ready.order == order && ready.mesh && ready.mesh->isValid();
        if (!current && (!job || job->structure != structure || !(job->order == order))) start(screen, *region, structure, order);
        if (job && !job->failed && step()) job.reset();
        // The last finished mesh of this structure stays up while another
        // order builds.
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
        float scale = fitScale(structure->size.x, structure->size.y, structure->size.z, width, height);
        glm::mat4 model{1.f};
        model[0] = {scale * r.right, scale * r.down, 0, 0};
        model[1] = {scale * u.right, scale * u.down, 0, 0};
        model[2] = {scale * f.right, scale * f.down, 0, 0};
        model[3] = {x + width / 2, y + height / 2, 0, 1};
        context.flushText(0, std::nullopt);
        context.enableScissorTest(RectangleArea{x, x + width, y, y + height});
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
            throw;
        }
        pop();
        context.disableScissorTest();
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

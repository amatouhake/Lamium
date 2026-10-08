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
    job->cells = visibleCells(s.size.x, s.size.y, s.size.z, order, [&](int x, int y, int z) { return blockAt(*job, x, y, z) != nullptr; });
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

// Tessellates the next blocks of the job: each at its cell relative to the
// structure's center, keeping only the faces turned to the viewer and not
// covered by a neighbor. True when the job finished.
bool step() {
    auto& j = *job;
    auto const& s = *j.structure;
    auto& batch = *j.batch;
    glm::vec3 center{s.size.x / 2.f, s.size.y / 2.f, s.size.z / 2.f};
    auto& positions = batch.mMeshData->mPositions.get();
    auto occupied = [&](int x, int y, int z) {
        return x >= 0 && y >= 0 && z >= 0 && x < s.size.x && y < s.size.y && z < s.size.z && blockAt(j, x, y, z) != nullptr;
    };
    for (size_t done = 0; j.next < j.cells.size() && done < blocksPerFrame; ++j.next, ++done) {
        auto cell = static_cast<int>(j.cells[j.next]);
        int x = cell / (s.size.y * s.size.z), y = cell / s.size.z % s.size.y, z = cell % s.size.z;
        glm::vec3 at = glm::vec3(x, y, z) - center;
        size_t from = positions.size();
        static_cast<bool&>(batch.mApplyTransform) = true;
        static_cast<glm::mat4x4&>(batch.mTransformMatrix) = glm::translate(glm::mat4{1.f}, at);
        j.blocks->appendTessellatedBlock(batch, *blockAt(j, x, y, z));
        for (size_t q = from; q + 4 <= positions.size(); q += 4) {
            auto n = glm::cross(positions[q + 1] - positions[q], positions[q + 2] - positions[q]);
            if (glm::length(n) < 1e-6f) continue;
            n = glm::normalize(n);
            int axis = std::abs(n.x) > .9f ? 0 : std::abs(n.y) > .9f ? 1 : std::abs(n.z) > .9f ? 2 : -1;
            if (axis < 0) continue;
            // Outward from the cell's middle, whatever the quad's winding;
            // a quad through the middle (a slab's top) keeps its winding.
            float middle = (positions[q][axis] + positions[q + 2][axis]) / 2 - at[axis] - .5f;
            int sign = std::abs(middle) > .01f ? (middle > 0 ? 1 : -1) : (n[axis] > 0 ? 1 : -1);
            int d[3]{0, 0, 0};
            d[axis] = sign;
            bool hide = !facesViewer(j.order, d[0], d[1], d[2]);
            float local = positions[q][axis] - at[axis];
            bool boundary = sign > 0 ? local > .999f : local < .001f;
            if (!hide && boundary) hide = occupied(x + d[0], y + d[1], z + d[2]);
            if (hide) for (size_t k = 1; k < 4; ++k) positions[q + k] = positions[q];
        }
    }
    if (j.next < j.cells.size()) return false;
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
        // scaled to fit; depth is flattened since blocks come far to near.
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

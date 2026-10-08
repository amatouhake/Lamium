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
#include <optional>
#include <variant>

namespace lamium::schematic::preview {
namespace {
// More visible blocks than this are not previewed (tessellating them would
// stall the frame); the box keeps its text.
constexpr size_t maxBlocks = 24000;

void log(std::string const& text) {
    try { Runtime::instance().self().getLogger().info("Schematic preview: {}", text); } catch (...) {}
}

struct Built {
    Structure const* structure = nullptr;
    std::shared_ptr<Structure const> keep;
    Order order;
    std::optional<mce::Mesh> mesh;
    std::uint32_t vertices = 0;
    bool failed = false; // too large or nothing to draw: not tried again
};
Built built;
// Meshes cannot be copied or assigned: reset field by field.
void clear() {
    built.mesh.reset();
    built.structure = nullptr;
    built.keep.reset();
    built.order = {};
    built.vertices = 0;
    built.failed = false;
}

// Builds the mesh for one draw order: visible cells far to near, each block
// tessellated at its cell (relative to the structure's center), keeping only
// the faces turned to the viewer and not covered by a neighbor.
void build(ScreenContext& screen, BlockSource& region, std::shared_ptr<Structure const> const& structure, Order order) {
    clear();
    built.structure = structure.get();
    built.keep = structure;
    built.order = order;
    auto const& s = *structure;
    std::vector<Block const*> palette(s.palette.size(), nullptr);
    for (size_t i = 0; i < s.palette.size(); ++i)
        if (!s.palette[i].isAir()) palette[i] = ghosts::gameBlock(s.palette[i]);
    auto blockAt = [&](int x, int y, int z) -> Block const* {
        auto index = s.blocks[static_cast<size_t>(s.cell(x, y, z))];
        return index >= 0 && static_cast<size_t>(index) < palette.size() ? palette[static_cast<size_t>(index)] : nullptr;
    };
    auto occupied = [&](int x, int y, int z) { return blockAt(x, y, z) != nullptr; };
    auto cells = visibleCells(s.size.x, s.size.y, s.size.z, order, occupied);
    if (cells.empty() || cells.size() > maxBlocks) {
        built.failed = true;
        if (!cells.empty()) log(std::format("{} visible blocks, over the {} limit", cells.size(), maxBlocks));
        return;
    }
    BlockTessellator own(&region);
    Tessellator batch(screen.tessellator.mBufferResourceService);
    batch.begin({}, mce::PrimitiveMode::QuadList, static_cast<int>(std::min<size_t>(cells.size() * 24, 1 << 20)), false);
    glm::vec3 center{s.size.x / 2.f, s.size.y / 2.f, s.size.z / 2.f};
    auto& positions = batch.mMeshData->mPositions.get();
    for (auto cell : cells) {
        int x = static_cast<int>(cell) / (s.size.y * s.size.z), y = static_cast<int>(cell) / s.size.z % s.size.y, z = static_cast<int>(cell) % s.size.z;
        auto const* block = blockAt(x, y, z);
        glm::vec3 at = glm::vec3(x, y, z) - center;
        size_t from = positions.size();
        static_cast<bool&>(batch.mApplyTransform) = true;
        static_cast<glm::mat4x4&>(batch.mTransformMatrix) = glm::translate(glm::mat4{1.f}, at);
        own.appendTessellatedBlock(batch, *block);
        // Faces turned away, or on the cell's side against an occupied
        // neighbor, collapse to a point.
        for (size_t q = from; q + 4 <= positions.size(); q += 4) {
            auto n = glm::cross(positions[q + 1] - positions[q], positions[q + 2] - positions[q]);
            if (glm::length(n) < 1e-6f) continue;
            n = glm::normalize(n);
            int axis = std::abs(n.x) > .9f ? 0 : std::abs(n.y) > .9f ? 1 : std::abs(n.z) > .9f ? 2 : -1;
            bool hide = false;
            if (axis >= 0) {
                int sign = n[axis] > 0 ? 1 : -1;
                int d[3]{0, 0, 0};
                d[axis] = sign;
                hide = !facesViewer(order, d[0], d[1], d[2]);
                // On the cell boundary (not a slab's middle) with a neighbor there.
                float local = positions[q][axis] - at[axis];
                bool boundary = sign > 0 ? local > .999f : local < .001f;
                if (!hide && boundary) {
                    int nx = x + d[0], ny = y + d[1], nz = z + d[2];
                    hide = nx >= 0 && ny >= 0 && nz >= 0 && nx < s.size.x && ny < s.size.y && nz < s.size.z && occupied(nx, ny, nz);
                }
            }
            if (hide) for (size_t k = 1; k < 4; ++k) positions[q + k] = positions[q];
        }
    }
    built.vertices = static_cast<std::uint32_t>(positions.size());
    if (!built.vertices) { built.failed = true; return; }
    built.mesh.emplace(batch.end(Tessellator::UploadMode::Buffered, "Lamium schematic preview", SupplementaryFieldAutoGenerationMode{}));
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
        if (built.structure != structure.get() || !(built.order == order) || (built.mesh && !built.mesh->isValid()))
            build(screen, *region, structure, order);
        if (built.failed || !built.mesh) return false;
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
            built.mesh->renderMesh(screen, material, texture, 0, built.vertices, OffscreenCaptureDescription{}, nullptr);
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
        built.failed = true;
        return false;
    }
}

void reset() { clear(); }
} // namespace lamium::schematic::preview

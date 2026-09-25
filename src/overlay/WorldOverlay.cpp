#include "overlay/WorldOverlay.h"
#include "overlay/ChunkBorders.h"
#include "overlay/Hitboxes.h"
#include "features/camera/Zoom.h"
#include "overlay/LightOverlay.h"
#include "overlay/ShapeSession.h"
#include "overlay/ShapeWorkspace.h"
#include "overlay/LocalShapePath.h"
#include "features/interaction/BreakingRestriction.h"
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/client/renderer/SupplementaryFieldAutoGenerationMode.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BrightnessPair.h"
#include "mc/deps/core_graphics/enums/PrimitiveMode.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureDescription.h"
#include "mc/common/client/renderer/helpers/MeshHelpers.h"
#include "mc/deps/renderer/Camera.h"
#include "mc/deps/renderer/MatrixStack.h"
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <chrono>
#include <map>
#include <span>
#include <mutex>
#include <atomic>
#include "ll/api/event/client/ClientStartJoinLevelEvent.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/deps/core/utility/FilePathManager.h"
#ifdef LAMIUM_SHAPE_TRACE
#include "mc/network/GameConnectionInfo.h"
#include <atomic>
#endif

namespace lamium::overlay {
namespace {
bool installed = false;
std::mutex shapeMutex;
ShapeWorkspace shapeWorkspace;
auto const& shapeCollection = shapeWorkspace.collection();
ll::event::ListenerPtr exitListener;
ll::event::ListenerPtr startJoinListener, joinListener;
std::atomic<bool> joiningLocal{false};
bool identityFailed = false;
#ifdef LAMIUM_SHAPE_TRACE
std::atomic<unsigned> identitySamples{0};
void traceIdentity(ll::event::ClientJoinLevelEvent& event) noexcept {
    try {
        if (event.self().getLocalPlayer() != &event.player() || identitySamples.fetch_add(1) >= 32) return;
        auto id = event.player().getLevel().getLevelId();
        // Hex avoids log control characters; cap data even in a diagnostic build.
        std::string encoded;
        constexpr char digits[] = "0123456789abcdef";
        for (unsigned char byte : id.substr(0,128)) {
            encoded += digits[byte >> 4]; encoded += digits[byte & 15];
        }
        auto connection = event.self().getGameConnectionInfo();
        Runtime::instance().self().getLogger().info(
            "Shape identity trace: local={} connection={} levelIdBytes={} levelIdHex={}",
            joiningLocal.load(), connection ? static_cast<int>(connection->mType) : -1, id.size(), encoded);
    } catch (...) {} // Diagnostics never change joining behavior.
}
#endif
void joinWorld(ll::event::ClientJoinLevelEvent& event) noexcept {
#ifdef LAMIUM_SHAPE_TRACE
    traceIdentity(event);
#endif
    try {
        if (event.self().getLocalPlayer() != &event.player()) return;
        std::lock_guard lock(shapeMutex);
        shapeWorkspace.leave();
        identityFailed = false;
        if (!joiningLocal) return;
        try {
            auto paths = event.self().getMinecraftGame_DEPRECATED().getFilePathManager();
            auto path = localShapePath(std::filesystem::u8path(paths->mWorlds->value),
                event.player().getLevel().getLevelId());
            if (path) shapeWorkspace.enter(*path);
        } catch (...) { identityFailed = true; throw; }
    } catch (std::exception const& error) {
        Runtime::instance().self().getLogger().error("Shape workspace load failed: {}", error.what());
    } catch (...) {}
}
// A shape being created is previewed as lines and never persisted.
ShapeCollection draftCollection(1);
constexpr ShapeId draftKey = ~ShapeId{0};
bool hasShapes() {
    std::lock_guard lock(shapeMutex);
    return !shapeCollection.entries().empty() || !draftCollection.entries().empty();
}

// Uploaded shape meshes, owned by the render thread. Vertices are relative to
// the shape's first block so the mesh is reused while the camera moves; only a
// revision change rebuilds it. World exit asks the next frame to release them.
struct ShapeMesh {
    uint64_t revision = 0;
    Cell origin{};
    std::optional<mce::Mesh> faces, lines;
    uint32_t faceVertices = 0, lineVertices = 0;
    int variant = -1; // The FaceMaterial variant the mesh was built for.
};
// Faces need an unlit, alpha-blended material, chosen per graphics mode:
// - Fancy: the hologram pointer material (vertex color, alpha blending,
//   depth-tested without depth writes). It culls, so faces get both windings.
// - Simple: that material is not loaded (its pointer resolves but draws
//   nothing); use the two-sided, additive lightning material, fainter.
// - Vibrant Visuals / ray tracing: the deferred pipeline does not show the
//   hologram material here. Try lightning and draw full-strength outlines so
//   the shape stays readable even if faces are not shown.
// The block selection overlay was unsuitable: it multiplies the scene color.
struct FaceMaterial { mce::MaterialPtr material; int variant; bool twoSided; float alpha; bool strongLines; };
FaceMaterial faceMaterial(IClientInstance& client) {
    auto mode = client.getOptions().getGraphicsMode();
    static std::atomic<int> reported{-1};
    if (reported.exchange(static_cast<int>(mode)) != static_cast<int>(mode)) {
        try {
            Runtime::instance().self().getLogger().info("Shape faces use the {} material (graphics mode {})",
                mode == GraphicsMode::Fancy ? "hologram pointer" : "lightning", static_cast<int>(mode));
        } catch (...) {}
    }
    if (mode == GraphicsMode::Fancy) {
        mce::MaterialPtr hologram(mce::RenderMaterialGroup::switchable(), HashedString{"holo_hand_pointer"});
        if (hologram.mRenderMaterialInfoPtr) return {std::move(hologram), 0, true, .28f, false};
    }
    mce::MaterialPtr lightning(mce::RenderMaterialGroup::common(), HashedString{"lightning"});
    if (mode == GraphicsMode::Simple) return {std::move(lightning), 1, false, .13f, false};
    return {std::move(lightning), 2, false, .13f, true};
}
std::map<ShapeId, ShapeMesh> shapeMeshes;
std::atomic<bool> releaseMeshes{false};
std::array<float,3> shapeColor(ShapeColor color, bool draft) {
    if (draft) return {.62f,.83f,1.f};
    switch (color) {
    case ShapeColor::Yellow: return {.95f,.8f,.24f};
    case ShapeColor::Pink: return {.94f,.5f,.75f};
    case ShapeColor::White: return {.95f,.95f,.95f};
    default: return {.25f,.82f,.88f};
    }
}
// Faces sit a hair inside their block so they never share a plane with the
// terrain face next to them, which otherwise flickers (z-fighting).
Point inset(Point p, Face face) {
    constexpr double depth = .005;
    switch (face) {
    case Face::West: p.x += depth; break;
    case Face::East: p.x -= depth; break;
    case Face::Down: p.y += depth; break;
    case Face::Up: p.y -= depth; break;
    case Face::North: p.z += depth; break;
    case Face::South: p.z -= depth; break;
    }
    return p;
}
void buildShapeMesh(ScreenContext& screen, ShapeMesh& mesh, ManagedShape const& shape, bool draft, FaceMaterial const& material) {
    bool twoSided = material.twoSided;
    mesh.faces.reset(); mesh.lines.reset();
    mesh.faceVertices = mesh.lineVertices = 0;
    mesh.revision = shape.revision;
    mesh.variant = material.variant;
    if (!shape.faces.empty()) mesh.origin = shape.faces.front().cell;
    else if (!shape.lines.empty()) mesh.origin = {static_cast<int>(shape.lines.front().from.x),
        static_cast<int>(shape.lines.front().from.y), static_cast<int>(shape.lines.front().from.z)};
    auto [r, g, b] = shapeColor(shape.definition.color, draft);
    bool faces = !draft && shape.definition.style == ShapeStyle::Face;
    auto relative = [&](Tessellator& batch, Point p) {
        batch.vertex(static_cast<float>(p.x - mesh.origin.x), static_cast<float>(p.y - mesh.origin.y),
            static_cast<float>(p.z - mesh.origin.z));
    };
    if (faces && !shape.faces.empty()) {
        Tessellator batch(screen.tessellator.mBufferResourceService);
        int sides = twoSided ? 2 : 1;
        batch.begin({}, mce::PrimitiveMode::QuadList, static_cast<int>(shape.faces.size()*4*sides), false);
        batch.color(r, g, b, material.alpha);
        for (auto const& face : shape.faces) {
            auto corners = faceVertices(face);
            for (auto& corner : corners) corner = inset(corner, face.face);
            for (auto p : corners) relative(batch, p);
            // The reverse winding keeps faces visible from inside the shape.
            if (twoSided) for (auto it = corners.rbegin(); it != corners.rend(); ++it) relative(batch, *it);
        }
        mesh.faces.emplace(batch.end(Tessellator::UploadMode::Buffered, "Lamium shape faces", SupplementaryFieldAutoGenerationMode{}));
        mesh.faceVertices = static_cast<uint32_t>(shape.faces.size()*4*sides);
    }
    if (!shape.lines.empty()) {
        Tessellator batch(screen.tessellator.mBufferResourceService);
        batch.begin({}, mce::PrimitiveMode::LineList, static_cast<int>(shape.lines.size()*2), false);
        // Faces carry a faint outline so the block grid stays readable.
        batch.color(r, g, b, faces && !material.strongLines ? .45f : 1.f);
        for (auto const& line : shape.lines) { relative(batch, line.from); relative(batch, line.to); }
        mesh.lines.emplace(batch.end(Tessellator::UploadMode::Buffered, "Lamium shape lines", SupplementaryFieldAutoGenerationMode{}));
        mesh.lineVertices = static_cast<uint32_t>(shape.lines.size()*2);
    }
}
// Draws a mesh built relative to `origin`, scaled toward the eye. The
// projection is unchanged, but depth moves slightly nearer in proportion to
// distance, so faces that run along or through existing blocks stay in front
// of those blocks' own faces instead of flickering against them.
template <class Draw>
void withTowardEye(BaseActorRenderContext& context, Cell origin, Draw&& draw) {
    ScreenContext& screen = context.mScreenContext;
    Vec3 const camera = context.mImpl->mCameraPosition;
    auto ref = screen.camera.worldMatrixStack->push(false);
    ref.stack->_isDirty = true;
    constexpr float towardEye = .997f;
    glm::vec3 offset{static_cast<float>(origin.x - camera.x), static_cast<float>(origin.y - camera.y),
        static_cast<float>(origin.z - camera.z)};
    ref.mat->_m = glm::scale(glm::translate(ref.mat->_m.get(), offset * towardEye), glm::vec3{towardEye});
    draw();
    // Pop manually, matching the proven LeviSchematic pattern for this stack.
    ref.stack->_isDirty = true;
    if (ref.stack->sortOrigin->has_value() && (ref.stack->stack->size() - 1) <= ref.stack->sortOrigin->value())
        ref.stack->sortOrigin->reset();
    ref.stack->stack->pop_back();
    ref.mat = nullptr;
    ref.stack = nullptr;
}
void drawShape(BaseActorRenderContext& context, FaceMaterial const& faceMaterial, ShapeId id,
               ManagedShape const& shape, bool draft) {
    if (!context.mImpl) return;
    ScreenContext& screen = context.mScreenContext;
    auto& mesh = shapeMeshes[id];
    if (mesh.revision != shape.revision || mesh.variant != faceMaterial.variant
        || (mesh.faces && !mesh.faces->isValid()) || (mesh.lines && !mesh.lines->isValid()))
        buildShapeMesh(screen, mesh, shape, draft, faceMaterial);
    mce::MaterialPtr lineMaterial(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    withTowardEye(context, mesh.origin, [&] {
        if (mesh.faces && faceMaterial.material.mRenderMaterialInfoPtr)
            mesh.faces->renderMesh(screen, faceMaterial.material, gsl::span<mce::ClientTexture const*>{}, 0, mesh.faceVertices,
                OffscreenCaptureDescription{}, nullptr);
        if (mesh.lines && lineMaterial.mRenderMaterialInfoPtr)
            mesh.lines->renderMesh(screen, lineMaterial, gsl::span<mce::ClientTexture const*>{}, 0, mesh.lineVertices,
                OffscreenCaptureDescription{}, nullptr);
    });
}
// Per-batch colors so one frame can carry Java-style color coding.
struct LineBatch { std::span<Line const> lines; float r, g, b, a = 1; };
void drawLines(BaseActorRenderContext& context, std::span<LineBatch const> batches) {
    if (batches.empty() || !context.mImpl) return;
    ScreenContext& screen = context.mScreenContext;
    Tessellator& shared = screen.tessellator;
    // Own the temporary tessellation state. Never reset or reuse a partially
    // assembled vanilla batch. Mesh lifetime follows the engine submission API.
    size_t count = 0;
    for (auto const& group : batches) count += group.lines.size();
    if (!count) return;
    Tessellator batch(shared.mBufferResourceService);
    batch.begin({}, mce::PrimitiveMode::LineList, static_cast<int>(count * 2), false);
    Vec3 const camera = context.mImpl->mCameraPosition;
    for (auto const& group : batches) {
        batch.color(group.r, group.g, group.b, group.a);
        for (auto const& line : group.lines) for (auto p : {line.from, line.to})
            batch.vertex(static_cast<float>(p.x - camera.x), static_cast<float>(p.y - camera.y),
                static_cast<float>(p.z - camera.z));
    }
    auto mesh = batch.end(Tessellator::UploadMode::Buffered, "Lamium world lines", SupplementaryFieldAutoGenerationMode{});
    mce::MaterialPtr material(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    if (!material.mRenderMaterialInfoPtr) return;
    mesh.renderMesh(screen, material, gsl::span<mce::ClientTexture const*>{}, 0,
        static_cast<uint>(count * 2), OffscreenCaptureDescription{}, nullptr);
}
void drawLines(BaseActorRenderContext& context, std::span<Line const> lines, bool hitboxes = false) {
    LineBatch single{lines, 1, 1, 1};
    if (!hitboxes) { single.r = .2f; single.g = .85f; single.b = 1.f; }
    drawLines(context, std::span<LineBatch const>{&single, 1});
}
// Light overlay (BACKLOG L-16). Per chunk column: the markers read last and a
// mesh of spawn tints and filled numbers, rebuilt only when the markers, the
// viewing quarter, the number mode or the face material change. Owned by the
// render thread; world exit and switching the overlay off release it.
struct LightChunk {
    std::vector<LightMarker> markers;
    uint64_t revision = 1;
    std::optional<mce::Mesh> faces, lines;
    uint32_t faceVertices = 0, lineVertices = 0;
    struct Built {
        uint64_t revision; Facing facing; LightValue value; int variant;
        bool operator==(Built const&) const = default;
    };
    std::optional<Built> built;
};
struct LightView { int dimension; int radius; bool operator==(LightView const&) const = default; };
std::map<ChunkColumn, LightChunk> lightChunks;
LightSchedule lightSchedule;
std::optional<LightView> lightView;
void releaseLight() { lightChunks.clear(); lightSchedule.clear(); lightView.reset(); }
void buildLightMesh(ScreenContext& screen, LightChunk& chunk, Cell origin, LightChunk::Built key, FaceMaterial const& material) {
    chunk.faces.reset(); chunk.lines.reset();
    chunk.faceVertices = chunk.lineVertices = 0;
    chunk.built = key;
    std::vector<Quad> always, night, numbers, sky;
    std::vector<Line> lines;
    bool both = key.value == LightValue::Both;
    for (auto const& marker : chunk.markers) {
        auto risk = spawnRisk(marker.light);
        if (risk == SpawnRisk::Always) always.push_back(lightTintQuad(marker.air));
        else if (risk == SpawnRisk::Night) night.push_back(lightTintQuad(marker.air));
        unsigned value = key.value == LightValue::Sky ? marker.light.sky : marker.light.block;
        appendLightNumberQuads(numbers, marker.air, value, key.facing, both ? -1 : 0);
        if (both) appendLightNumberQuads(sky, marker.air, marker.light.sky, key.facing, 1);
        // Vibrant Visuals may not show the faces; keep the digits readable as lines.
        if (material.strongLines) {
            appendLightNumberLines(lines, marker.air, value, key.facing, both ? -1 : 0);
            if (both) appendLightNumberLines(lines, marker.air, marker.light.sky, key.facing, 1);
        }
    }
    auto relative = [&](Tessellator& batch, Point p) {
        batch.vertex(static_cast<float>(p.x - origin.x), static_cast<float>(p.y - origin.y), static_cast<float>(p.z - origin.z));
    };
    size_t quads = always.size() + night.size() + numbers.size() + sky.size();
    if (quads) {
        // Blended (Fancy) faces take shape-like alpha; additive ones add up
        // quickly on bright ground, so they stay as faint as shape faces.
        bool blended = material.variant == 0;
        float tint = material.alpha, ink = blended ? .85f : .3f;
        std::array<std::tuple<std::vector<Quad> const*, float, float, float, float>, 4> groups{{
            {&always, .88f, .31f, .22f, tint}, {&night, 1.f, .76f, .29f, tint},
            {&numbers, 1.f, 1.f, 1.f, ink}, {&sky, .62f, .82f, 1.f, ink}}};
        // Like shape faces, add the reverse winding only for the culling
        // (Fancy) material. The others are already two-sided, and a second
        // copy at the same depth flickers against the first.
        size_t sides = material.twoSided ? 2 : 1;
        Tessellator batch(screen.tessellator.mBufferResourceService);
        batch.begin({}, mce::PrimitiveMode::QuadList, static_cast<int>(quads * 4 * sides), false);
        for (auto const& [group, r, g, b, a] : groups) {
            batch.color(r, g, b, a);
            for (auto const& quad : *group) {
                for (auto p : quad.corners) relative(batch, p);
                if (sides == 2) for (auto it = quad.corners.rbegin(); it != quad.corners.rend(); ++it) relative(batch, *it);
            }
        }
        chunk.faces.emplace(batch.end(Tessellator::UploadMode::Buffered, "Lamium light overlay", SupplementaryFieldAutoGenerationMode{}));
        chunk.faceVertices = static_cast<uint32_t>(quads * 4 * sides);
    }
    if (!lines.empty()) {
        Tessellator batch(screen.tessellator.mBufferResourceService);
        batch.begin({}, mce::PrimitiveMode::LineList, static_cast<int>(lines.size() * 2), false);
        batch.color(1.f, 1.f, 1.f, 1.f);
        for (auto const& line : lines) { relative(batch, line.from); relative(batch, line.to); }
        chunk.lines.emplace(batch.end(Tessellator::UploadMode::Buffered, "Lamium light lines", SupplementaryFieldAutoGenerationMode{}));
        chunk.lineVertices = static_cast<uint32_t>(lines.size() * 2);
    }
}
void drawLightOverlay(BaseActorRenderContext& context, IClientInstance& client, LocalPlayer& player,
                      Settings::Overlays const& preferences) {
    if (!context.mImpl) return;
    // Like Chunk Borders, follow the rendered view while the camera is detached.
    Vec3 position = player.getFeetPos();
    if (Zoom::instance().detachedCameraActive()) position = context.mImpl->mCameraPosition;
    Cell center{checkedCoordinate(std::floor(position.x)), checkedCoordinate(std::floor(position.y)),
                checkedCoordinate(std::floor(position.z))};
    int radius = static_cast<int>(preferences.lightRange);
    LightView view{static_cast<int>(player.getDimensionId()), radius};
    if (lightView != view) { releaseLight(); lightView = view; }
    auto wanted = chunksInRange(center, radius);
    std::erase_if(lightChunks, [&](auto const& entry) {
        return std::find(wanted.begin(), wanted.end(), entry.first) == wanted.end();
    });
    lightSchedule.keepOnly(wanted);

    // Read a bounded number of cells per frame: the viewer's band spans the
    // same distance up and down as sideways, within the dimension.
    auto& dimension = player.getDimension();
    auto const& height = dimension.mHeightRange;
    int low = std::max(center.y - radius, static_cast<int>(height->mMin) + 1);
    int high = std::min(center.y + radius, static_cast<int>(height->mMax) - 1);
    if (low <= high) {
        constexpr size_t cellBudget = 32768;
        size_t perColumn = 256 * static_cast<size_t>(high - low + 1);
        double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        auto& region = player.getDimensionBlockSource();
        for (auto column : lightSchedule.pick(wanted, chunkOf(center), now, std::max<size_t>(1, cellBudget / perColumn))) {
            Cell first{column.x * 16, low, column.z * 16};
            if (!region.getChunkAt(BlockPos{first.x, first.y, first.z})) continue;
            auto markers = sampleLightBox(first, {first.x + 15, high, first.z + 15}, [&](Cell cell) -> std::optional<LightSurface> {
                BlockPos air{cell.x,cell.y,cell.z}, floor{cell.x,cell.y-1,cell.z};
                if (!region.getBlock(air).isAir() || !region.getBlock(floor)._isSolid()) return {};
                auto light = region.getBrightnessPair(air);
                return LightSurface{light.block->mValue,light.sky->mValue};
            });
            auto& chunk = lightChunks[column];
            if (chunk.markers != markers) { chunk.markers = std::move(markers); ++chunk.revision; }
        }
    }

    // Follow the rendered view: the detached camera's direction during
    // Freelook/FreeCamera, else the player's.
    Facing facing = Facing::North;
    switch (preferences.lightFacing) {
    case LightFacing::North: facing = Facing::North; break;
    case LightFacing::East: facing = Facing::East; break;
    case LightFacing::South: facing = Facing::South; break;
    case LightFacing::West: facing = Facing::West; break;
    case LightFacing::View:
        if (auto ray = Zoom::instance().detachedViewRay(client)) facing = facingFromDirection(ray->dx, ray->dz);
        else facing = facingFromYaw(player.getRotation().z);
        break;
    }
    auto material = faceMaterial(client);
    mce::MaterialPtr lineMaterial(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    ScreenContext& screen = context.mScreenContext;
    // Rebuild nearest first within a per-frame budget, so turning around at a
    // large range spreads the work; a chunk not rebuilt yet keeps its old mesh.
    constexpr size_t markerBudget = 4096;
    size_t rebuilt = 0;
    for (auto column : wanted) {
        auto found = lightChunks.find(column);
        if (found == lightChunks.end() || found->second.markers.empty()) continue;
        auto& chunk = found->second;
        Cell origin{column.x * 16, 0, column.z * 16};
        LightChunk::Built key{chunk.revision, facing, preferences.lightValue, material.variant};
        bool invalid = (chunk.faces && !chunk.faces->isValid()) || (chunk.lines && !chunk.lines->isValid());
        if ((chunk.built != key || invalid) && (rebuilt == 0 || rebuilt + chunk.markers.size() <= markerBudget || invalid)) {
            buildLightMesh(screen, chunk, origin, key, material);
            rebuilt += chunk.markers.size();
        }
        if (!chunk.built) continue;
        withTowardEye(context, origin, [&] {
            if (chunk.faces && material.material.mRenderMaterialInfoPtr)
                chunk.faces->renderMesh(screen, material.material, gsl::span<mce::ClientTexture const*>{}, 0,
                    chunk.faceVertices, OffscreenCaptureDescription{}, nullptr);
            if (chunk.lines && lineMaterial.mRenderMaterialInfoPtr)
                chunk.lines->renderMesh(screen, lineMaterial, gsl::span<mce::ClientTexture const*>{}, 0,
                    chunk.lineVertices, OffscreenCaptureDescription{}, nullptr);
        });
    }
}
LL_TYPE_INSTANCE_HOOK(WorldLines, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$renderEntityEffects, void, BaseActorRenderContext& context) {
    origin(context);
    auto& runtime = Runtime::instance();
    if (!runtime.enabled()) return;
    auto preferences = runtime.preferences().overlays;
    bool breaking = runtime.preferences().interaction.breaking;
    if (releaseMeshes.exchange(false)) { shapeMeshes.clear(); releaseLight(); }
    if (!preferences.light && !lightChunks.empty()) releaseLight();
    bool shapesShown = preferences.shapes && hasShapes();
    if (!preferences.chunkBorders && !preferences.hitboxes && !preferences.light && !breaking && !shapesShown) return;
    IClientInstance& client = context.mClientInstance;
    auto* player = client.getLocalPlayer();
    if (!player) return;
    try {
        if (shapesShown) {
            std::lock_guard lock(shapeMutex);
            auto const faces = faceMaterial(client);
            int dimensionId = static_cast<int>(player->getDimensionId());
            shapeCollection.forVisible(dimensionId,
                [&](ShapeId id, ManagedShape const& shape) { drawShape(context, faces, id, shape, false); });
            draftCollection.forVisible(dimensionId,
                [&](ShapeId, ManagedShape const& shape) { drawShape(context, faces, draftKey, shape, true); });
            // Release meshes of removed shapes.
            if (shapeMeshes.size() > shapeCollection.entries().size() + draftCollection.entries().size())
                std::erase_if(shapeMeshes, [&](auto const& entry) {
                    return entry.first == draftKey ? draftCollection.entries().empty() : !shapeCollection.find(entry.first);
                });
        }
        if (breaking) {
            auto region = interaction::breaking::region();
            if (region) {
                thread_local std::optional<interaction::RestrictionRegion> cached;
                thread_local std::vector<Line> lines;
                if (cached != region) { lines = gridSurfaceLines(region->preview(4)); cached = region; }
                drawLines(context,lines);
            }
        }
        auto& dimension = player->getDimension();
        if (preferences.light) drawLightOverlay(context, client, *player, preferences);
        if (preferences.chunkBorders) {
            auto const& range = dimension.mHeightRange;
            Vec3 const position = player->getPosition();
            Point center{position.x, position.y, position.z};
            // A detached camera looks from away from the body; center the
            // borders on the rendered view instead of the player chunk.
            if (context.mImpl && Zoom::instance().detachedCameraActive()) {
                Vec3 const camera = context.mImpl->mCameraPosition;
                center = {camera.x, camera.y, camera.z};
            }
            thread_local ChunkBorderCache borders;
            auto const& groups = borders.get(center, range->mMin, range->mMax);
            std::array<LineBatch, 5> colored{{{groups.yellow, chunkYellow[0], chunkYellow[1], chunkYellow[2]},
                                               {groups.blue, chunkBlue[0], chunkBlue[1], chunkBlue[2]},
                                               {groups.red, chunkRed[0], chunkRed[1], chunkRed[2]},
                                               {groups.purple, chunkPurple[0], chunkPurple[1], chunkPurple[2]},
                                               {groups.teal, chunkTeal[0], chunkTeal[1], chunkTeal[2]}}};
            drawLines(context, colored);
        }
        if (preferences.hitboxes && context.mImpl) {
            Vec3 const camera = context.mImpl->mCameraPosition;
            std::vector<Line> white, red, blue;
            // Only borrow client actors during this pass. No entity pointers or
            // bounds survive world exit or a subsequent frame.
            for (auto* actor : player->getLevel().getRuntimeActorList()) {
                if (!actor || actor == player || &actor->getDimension() != &dimension) continue;
                auto const& bounds = actor->getAABB();
                Point min{bounds.min.x,bounds.min.y,bounds.min.z}, max{bounds.max.x,bounds.max.y,bounds.max.z};
                if (!hitboxInRange(min,max,{camera.x,camera.y,camera.z},preferences.hitboxDistance)) continue;
                auto edges = wireBox(min,max);
                white.insert(white.end(),edges.begin(),edges.end());
                // Java shows the eye box and look line for mobs only; items
                // and other eyeless entities keep the white bounds alone.
                if (!actor->hasType(ActorType::Mob)) continue;
                Vec3 const eye = actor->getEyePos();
                auto marker = eyeBox({eye.x, eye.y, eye.z});
                red.insert(red.end(),marker.begin(),marker.end());
                Vec3 const view = actor->getViewVector();
                blue.push_back(lookLine({eye.x, eye.y, eye.z}, view.x, view.y, view.z));
            }
            std::array<LineBatch, 3> colored{{{white, 1, 1, 1}, {red, 1, 0, 0}, {blue, 0, 0, 1}}};
            drawLines(context, colored);
        }
    } catch (std::exception const& error) {
        // Rate-limit repeated failures without swallowing the vanilla pass.
        static bool reported = false;
        if (!reported) { runtime.self().getLogger().error("World overlay drawing failed: {}", error.what()); reported = true; }
    }
}
}
namespace shapes {
std::vector<Summary> list() {
    std::lock_guard lock(shapeMutex);
    std::vector<Summary> result;
    result.reserve(shapeCollection.entries().size());
    for (auto const& [id, shape] : shapeCollection.entries()) result.push_back({id, shape.definition});
    return result;
}
std::optional<ShapeDefinition> find(ShapeId id) {
    std::lock_guard lock(shapeMutex);
    auto shape = shapeCollection.find(id);
    return shape ? std::optional(shape->definition) : std::nullopt;
}
ShapeId add(ShapeDefinition definition) {
    std::lock_guard lock(shapeMutex);
    if (identityFailed) throw std::runtime_error("Shape workspace unavailable");
    return shapeWorkspace.change([&](auto& values) { return values.add(std::move(definition)); });
}
void edit(ShapeId id, ShapeDefinition definition) {
    std::lock_guard lock(shapeMutex);
    shapeWorkspace.change([&](auto& values) { values.edit(id, std::move(definition)); });
}
void setVisible(ShapeId id, bool visible) {
    std::lock_guard lock(shapeMutex);
    shapeWorkspace.change([&](auto& values) { values.setVisible(id, visible); });
}
void rename(ShapeId id, std::string name) {
    std::lock_guard lock(shapeMutex);
    shapeWorkspace.change([&](auto& values) { values.rename(id,std::move(name)); });
}
bool remove(ShapeId id) {
    std::lock_guard lock(shapeMutex);
    return shapeWorkspace.change([&](auto& values) { return values.remove(id); });
}
void setDraft(std::optional<ShapeDefinition> definition) {
    std::lock_guard lock(shapeMutex);
    draftCollection.clear();
    if (definition) draftCollection.add(std::move(*definition));
}
void clear() {
    std::lock_guard lock(shapeMutex);
    draftCollection.clear();
    releaseMeshes = true;
    shapeWorkspace.leave();
    joiningLocal = false;
    identityFailed = false;
}
Storage storage() {
    std::lock_guard lock(shapeMutex);
    return identityFailed || shapeWorkspace.failedToLoad() ? Storage::LoadFailed
        : shapeWorkspace.persistent() ? Storage::LocalWorld : Storage::Session;
}
}
void start() {
    if (installed) return;
    try {
        installed = WorldLines::hook(true) == 0;
        if (!installed) throw std::runtime_error("Could not install world overlay render hook");
        exitListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientExitLevelEvent>(
            [](auto&) { shapes::clear(); });
        if (!exitListener) throw std::runtime_error("Could not subscribe shape world exit");
#ifdef LAMIUM_SHAPE_TRACE
        identitySamples = 0;
#endif
        auto& bus = ll::event::EventBus::getInstance();
        startJoinListener = bus.emplaceListener<ll::event::ClientStartJoinLevelEvent>(
            [](auto& event) { shapes::clear(); std::lock_guard lock(shapeMutex); joiningLocal = event.isJoiningLocalServer(); });
        joinListener = bus.emplaceListener<ll::event::ClientJoinLevelEvent>(joinWorld);
        if (!startJoinListener || !joinListener) throw std::runtime_error("Could not subscribe shape world entry");
    } catch (...) { stop(); throw; }
}
void stop() {
    for (auto* listener : {&startJoinListener,&joinListener}) if (*listener) {
        ll::event::EventBus::getInstance().removeListener(*listener);
        listener->reset();
    }
    if (exitListener) {
        ll::event::EventBus::getInstance().removeListener(exitListener);
        exitListener.reset();
    }
    shapes::clear();
    if (installed && WorldLines::unhook(true)) installed = false;
}
}

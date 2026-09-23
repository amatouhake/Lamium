#include "overlay/WorldOverlay.h"
#include "overlay/ChunkBorders.h"
#include "overlay/Hitboxes.h"
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
    bool twoSided = false;
};
// Faces need an unlit, alpha-blended material. The block selection overlay
// multiplies the scene color, so it followed day and night; it also culls back
// faces. Prefer the hologram pointer material (vertex color, alpha blending,
// depth-tested without depth writes), available with fancy graphics, and emit
// both windings since it culls. Otherwise use the lightning material, which is
// two-sided but additive.
struct FaceMaterial { mce::MaterialPtr material; bool twoSided; };
FaceMaterial faceMaterial() {
    mce::MaterialPtr hologram(mce::RenderMaterialGroup::switchable(), HashedString{"holo_hand_pointer"});
    bool preferred = hologram.mRenderMaterialInfoPtr != nullptr;
    static std::atomic<int> reported{-1};
    if (reported.exchange(preferred ? 1 : 0) != (preferred ? 1 : 0)) {
        try {
            Runtime::instance().self().getLogger().info("Shape faces use the {} material",
                preferred ? "hologram pointer" : "lightning fallback");
        } catch (...) {}
    }
    if (preferred) return {std::move(hologram), true};
    return {mce::MaterialPtr(mce::RenderMaterialGroup::common(), HashedString{"lightning"}), false};
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
void buildShapeMesh(ScreenContext& screen, ShapeMesh& mesh, ManagedShape const& shape, bool draft, bool twoSided) {
    mesh.faces.reset(); mesh.lines.reset();
    mesh.faceVertices = mesh.lineVertices = 0;
    mesh.revision = shape.revision;
    mesh.twoSided = twoSided;
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
        batch.color(r, g, b, twoSided ? .28f : .2f);
        for (auto const& face : shape.faces) {
            auto corners = faceVertices(face);
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
        batch.color(r, g, b, faces ? .45f : 1.f);
        for (auto const& line : shape.lines) { relative(batch, line.from); relative(batch, line.to); }
        mesh.lines.emplace(batch.end(Tessellator::UploadMode::Buffered, "Lamium shape lines", SupplementaryFieldAutoGenerationMode{}));
        mesh.lineVertices = static_cast<uint32_t>(shape.lines.size()*2);
    }
}
void drawShape(BaseActorRenderContext& context, FaceMaterial const& faceMaterial, ShapeId id,
               ManagedShape const& shape, bool draft) {
    if (!context.mImpl) return;
    ScreenContext& screen = context.mScreenContext;
    auto& mesh = shapeMeshes[id];
    if (mesh.revision != shape.revision || mesh.twoSided != faceMaterial.twoSided
        || (mesh.faces && !mesh.faces->isValid()) || (mesh.lines && !mesh.lines->isValid()))
        buildShapeMesh(screen, mesh, shape, draft, faceMaterial.twoSided);
    mce::MaterialPtr lineMaterial(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    Vec3 const camera = context.mImpl->mCameraPosition;
    auto ref = screen.camera.worldMatrixStack->push(false);
    ref.stack->_isDirty = true;
    ref.mat->_m = glm::translate(ref.mat->_m.get(), glm::vec3{static_cast<float>(mesh.origin.x - camera.x),
        static_cast<float>(mesh.origin.y - camera.y), static_cast<float>(mesh.origin.z - camera.z)});
    if (mesh.faces && faceMaterial.material.mRenderMaterialInfoPtr)
        mesh.faces->renderMesh(screen, faceMaterial.material, gsl::span<mce::ClientTexture const*>{}, 0, mesh.faceVertices,
            OffscreenCaptureDescription{}, nullptr);
    if (mesh.lines && lineMaterial.mRenderMaterialInfoPtr)
        mesh.lines->renderMesh(screen, lineMaterial, gsl::span<mce::ClientTexture const*>{}, 0, mesh.lineVertices,
            OffscreenCaptureDescription{}, nullptr);
    // Pop manually, matching the proven LeviSchematic pattern for this stack.
    ref.stack->_isDirty = true;
    if (ref.stack->sortOrigin->has_value() && (ref.stack->stack->size() - 1) <= ref.stack->sortOrigin->value())
        ref.stack->sortOrigin->reset();
    ref.stack->stack->pop_back();
    ref.mat = nullptr;
    ref.stack = nullptr;
}
void drawLines(BaseActorRenderContext& context, std::span<Line const> lines, bool hitboxes = false) {
    if (lines.empty() || !context.mImpl) return;
    ScreenContext& screen = context.mScreenContext;
    Tessellator& shared = screen.tessellator;
    // Own the temporary tessellation state. Never reset or reuse a partially
    // assembled vanilla batch. Mesh lifetime follows the engine submission API.
    Tessellator batch(shared.mBufferResourceService);
    batch.begin({}, mce::PrimitiveMode::LineList, static_cast<int>(lines.size()*2), false);
    if (hitboxes) batch.color(1.f,1.f,1.f,1.f);
    else batch.color(.2f,.85f,1.f,1.f);
    Vec3 const camera = context.mImpl->mCameraPosition;
    for (auto const& line : lines) for (auto p : {line.from, line.to})
        batch.vertex(static_cast<float>(p.x-camera.x), static_cast<float>(p.y-camera.y), static_cast<float>(p.z-camera.z));
    auto mesh = batch.end(Tessellator::UploadMode::Buffered, "Lamium world lines", SupplementaryFieldAutoGenerationMode{});
    mce::MaterialPtr material(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    if (!material.mRenderMaterialInfoPtr) return;
    mesh.renderMesh(screen, material, gsl::span<mce::ClientTexture const*>{}, 0,
        static_cast<uint>(lines.size()*2), OffscreenCaptureDescription{}, nullptr);
}
LL_TYPE_INSTANCE_HOOK(WorldLines, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$renderEntityEffects, void, BaseActorRenderContext& context) {
    origin(context);
    auto& runtime = Runtime::instance();
    if (!runtime.enabled()) return;
    auto preferences = runtime.preferences().overlays;
    bool breaking = runtime.preferences().interaction.breaking;
    if (releaseMeshes.exchange(false)) shapeMeshes.clear();
    bool shapesShown = preferences.shapes && hasShapes();
    if (!preferences.chunkBorders && !preferences.hitboxes && !preferences.light && !breaking && !shapesShown) return;
    IClientInstance& client = context.mClientInstance;
    auto* player = client.getLocalPlayer();
    if (!player) return;
    try {
        if (shapesShown) {
            std::lock_guard lock(shapeMutex);
            auto const faces = faceMaterial();
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
        if (preferences.light) {
            Vec3 const position = player->getFeetPos();
            Cell center{checkedCoordinate(std::floor(position.x)), checkedCoordinate(std::floor(position.y)),
                        checkedCoordinate(std::floor(position.z))};
            auto& region = player->getDimensionBlockSource();
            auto const& height = dimension.mHeightRange;
            auto markers = sampleLightSurfaces(center,4,2,[&](Cell cell) -> std::optional<LightSurface> {
                if (cell.y <= height->mMin || cell.y >= height->mMax) return {};
                BlockPos air{cell.x,cell.y,cell.z}, floor{cell.x,cell.y-1,cell.z};
                if (!region.getChunkAt(air) || !region.getBlock(air).isAir()
                    || !region.getBlock(floor)._isSolid()) return {};
                auto light = region.getBrightnessPair(air);
                return LightSurface{light.block->mValue,light.sky->mValue};
            });
            std::vector<Line> lines;
            // At most two seven-segment digits per marker. Append directly to
            // the render batch instead of allocating a vector for each floor.
            lines.reserve(markers.size()*14);
            for (auto const& marker : markers) {
                appendLightNumberLines(lines,marker.air,preferences.skyLight ? marker.light.sky : marker.light.block);
            }
            drawLines(context,lines,true);
        }
        if (preferences.chunkBorders) {
            auto const& range = dimension.mHeightRange;
            Vec3 const position = player->getPosition();
            thread_local ChunkBorderCache borders;
            drawLines(context, borders.get({position.x,position.y,position.z}, range->mMin, range->mMax));
        }
        if (preferences.hitboxes && context.mImpl) {
            Vec3 const camera = context.mImpl->mCameraPosition;
            std::vector<Line> lines;
            // Only borrow client actors during this pass. No entity pointers or
            // bounds survive world exit or a subsequent frame.
            for (auto* actor : player->getLevel().getRuntimeActorList()) {
                if (!actor || actor == player || &actor->getDimension() != &dimension) continue;
                auto const& bounds = actor->getAABB();
                Point min{bounds.min.x,bounds.min.y,bounds.min.z}, max{bounds.max.x,bounds.max.y,bounds.max.z};
                if (!hitboxInRange(min,max,{camera.x,camera.y,camera.z},preferences.hitboxDistance)) continue;
                auto edges = wireBox(min,max);
                lines.insert(lines.end(),edges.begin(),edges.end());
            }
            drawLines(context,lines,true);
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

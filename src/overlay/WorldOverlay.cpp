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
void drawShape(BaseActorRenderContext& context, FaceMaterial const& faceMaterial, ShapeId id,
               ManagedShape const& shape, bool draft) {
    if (!context.mImpl) return;
    ScreenContext& screen = context.mScreenContext;
    auto& mesh = shapeMeshes[id];
    if (mesh.revision != shape.revision || mesh.variant != faceMaterial.variant
        || (mesh.faces && !mesh.faces->isValid()) || (mesh.lines && !mesh.lines->isValid()))
        buildShapeMesh(screen, mesh, shape, draft, faceMaterial);
    mce::MaterialPtr lineMaterial(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    Vec3 const camera = context.mImpl->mCameraPosition;
    auto ref = screen.camera.worldMatrixStack->push(false);
    ref.stack->_isDirty = true;
    // Scale the shape toward the eye. The projection is unchanged, but depth
    // moves slightly nearer in proportion to distance, so faces that run
    // through existing blocks stay in front of those blocks' own faces instead
    // of flickering against them (the inset alone only helps faces in air).
    constexpr float towardEye = .997f;
    glm::vec3 offset{static_cast<float>(mesh.origin.x - camera.x), static_cast<float>(mesh.origin.y - camera.y),
        static_cast<float>(mesh.origin.z - camera.z)};
    ref.mat->_m = glm::scale(glm::translate(ref.mat->_m.get(), offset * towardEye), glm::vec3{towardEye});
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

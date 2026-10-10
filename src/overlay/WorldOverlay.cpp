#include "overlay/WorldOverlay.h"
#include "overlay/FaceMaterial.h"
#include "overlay/CellMesh.h"
#include "overlay/ChunkBorders.h"
#include "overlay/Hitboxes.h"
#include "overlay/Depth.h"
#include "features/camera/CameraSessions.h"
#include "overlay/LightOverlay.h"
#include "overlay/ShapeSession.h"
#include "overlay/ShapeWorkspace.h"
#include "overlay/LocalShapePath.h"
#include "features/interaction/BreakingRestriction.h"
#include "mc/world/phys/HitResult.h"
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
#include "mc/world/level/Tick.h"
#include "mc/legacy/ActorRuntimeID.h"
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
#include <unordered_map>
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

} // namespace
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
namespace {
// Uploaded shape meshes (CellMesh), owned by the render thread: only a
// revision change rebuilds one. World exit asks the next frame to release them.
std::map<ShapeId, CellMesh> shapeMeshes;
// The breaking region of the press in progress (L-15): white faces with a
// faint grid, like a face-style shape.
CellMesh restrictionMesh;
std::atomic<bool> releaseMeshes{false};
struct EyeTrack { EyeOffsetInterpolator offset; uint64_t seenFrame = 0; };
thread_local std::unordered_map<ActorRuntimeID, EyeTrack> eyeTracks;
thread_local uint64_t eyeFrame = 0;
thread_local int eyeDimension = 0;
std::array<float,3> shapeColor(ShapeColor color, bool draft) {
    if (draft) return {.62f,.83f,1.f};
    switch (color) {
    case ShapeColor::Yellow: return {.95f,.8f,.24f};
    case ShapeColor::Pink: return {.94f,.5f,.75f};
    case ShapeColor::White: return {.95f,.95f,.95f};
    default: return {.25f,.82f,.88f};
    }
}
// Draws a mesh built relative to `origin`, scaled toward the eye. The
// projection is unchanged, but depth moves slightly nearer in proportion to
// distance, so faces that run along or through existing blocks stay in front
// of those blocks' own faces instead of flickering against them.
template <class Draw>
void withTowardEye(BaseActorRenderContext& context, Cell origin, Draw&& draw, float towardEye = depth::facePull) {
    Vec3 const camera = context.mImpl->mCameraPosition;
    glm::vec3 offset{static_cast<float>(origin.x - camera.x), static_cast<float>(origin.y - camera.y),
        static_cast<float>(origin.z - camera.z)};
    drawPulled(context.mScreenContext, offset, towardEye, draw);
}
// A shape (or a draft, or the breaking region) as a cell overlay: faces in
// its color with a faint outline, or lines alone.
void drawShape(BaseActorRenderContext& context, FaceMaterial const& faceMaterial, ShapeId id,
               ManagedShape const& shape, bool draft) {
    if (!context.mImpl) return;
    auto [r, g, b] = shapeColor(shape.definition.color, draft);
    CellStyle style{r, g, b, !draft && shape.definition.style == ShapeStyle::Face};
    auto& mesh = shapeMeshes[id];
    if (mesh.stale(shape.revision, style, faceMaterial))
        buildCellMesh(context.mScreenContext, mesh, shape.faces, shape.lines, style, faceMaterial, shape.revision);
    drawCellMesh(context.mScreenContext, context.mImpl->mCameraPosition, mesh, faceMaterial);
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
    // Tints and digits are separate meshes so digits can be drawn nudged
    // further toward the eye: a fixed lift alone runs out of depth precision
    // tens of blocks away and the layers fought there.
    std::optional<mce::Mesh> tints, digits, lines;
    uint32_t tintVertices = 0, digitVertices = 0, lineVertices = 0;
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
    chunk.tints.reset(); chunk.digits.reset(); chunk.lines.reset();
    chunk.tintVertices = chunk.digitVertices = chunk.lineVertices = 0;
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
    // Blended (Fancy) faces take shape-like alpha; additive ones add up
    // quickly on bright ground, so they stay as faint as shape faces.
    bool blended = material.variant == 0;
    float tint = material.alpha, ink = blended ? .85f : .3f;
    // Like shape faces, add the reverse winding only for the culling (Fancy)
    // material. The others are already two-sided, and a second copy at the
    // same depth flickers against the first.
    size_t sides = material.twoSided ? 2 : 1;
    using Group = std::tuple<std::vector<Quad> const*, float, float, float, float>;
    auto build = [&](std::initializer_list<Group> groups, std::optional<mce::Mesh>& mesh, uint32_t& vertices, char const* name) {
        size_t quads = 0;
        for (auto const& group : groups) quads += std::get<0>(group)->size();
        if (!quads) return;
        Tessellator batch(screen.tessellator.mBufferResourceService);
        batch.begin({}, mce::PrimitiveMode::QuadList, static_cast<int>(quads * 4 * sides), false);
        for (auto const& [group, r, g, b, a] : groups) {
            batch.color(r, g, b, a);
            for (auto const& quad : *group) {
                for (auto p : quad.corners) relative(batch, p);
                if (sides == 2) for (auto it = quad.corners.rbegin(); it != quad.corners.rend(); ++it) relative(batch, *it);
            }
        }
        mesh.emplace(batch.end(Tessellator::UploadMode::Buffered, name, SupplementaryFieldAutoGenerationMode{}));
        vertices = static_cast<uint32_t>(quads * 4 * sides);
    };
    build({Group{&always, .88f, .31f, .22f, tint}, Group{&night, 1.f, .76f, .29f, tint}},
        chunk.tints, chunk.tintVertices, "Lamium light tints");
    build({Group{&numbers, 1.f, 1.f, 1.f, ink}, Group{&sky, .62f, .82f, 1.f, ink}},
        chunk.digits, chunk.digitVertices, "Lamium light digits");
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
    if (CameraSessions::instance().detachedCameraActive()) position = context.mImpl->mCameraPosition;
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
        if (auto ray = CameraSessions::instance().detachedViewRay(client)) facing = facingFromDirection(ray->dx, ray->dz);
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
        bool invalid = (chunk.tints && !chunk.tints->isValid()) || (chunk.digits && !chunk.digits->isValid())
            || (chunk.lines && !chunk.lines->isValid());
        if ((chunk.built != key || invalid) && (rebuilt == 0 || rebuilt + chunk.markers.size() <= markerBudget || invalid)) {
            buildLightMesh(screen, chunk, origin, key, material);
            rebuilt += chunk.markers.size();
        }
        if (!chunk.built) continue;
        // Each layer sits nearer the eye in proportion to distance (screen
        // positions do not change), so tint, digits and lines stay apart in
        // depth at any range.
        auto faces = [&](std::optional<mce::Mesh>& mesh, uint32_t vertices, float towardEye) {
            if (!mesh || !material.material.mRenderMaterialInfoPtr) return;
            withTowardEye(context, origin, [&] {
                mesh->renderMesh(screen, material.material, gsl::span<mce::ClientTexture const*>{}, 0,
                    vertices, OffscreenCaptureDescription{}, nullptr);
            }, towardEye);
        };
        faces(chunk.tints, chunk.tintVertices, depth::facePull);
        faces(chunk.digits, chunk.digitVertices, depth::digitPull);
        if (chunk.lines && lineMaterial.mRenderMaterialInfoPtr)
            withTowardEye(context, origin, [&] {
                chunk.lines->renderMesh(screen, lineMaterial, gsl::span<mce::ClientTexture const*>{}, 0,
                    chunk.lineVertices, OffscreenCaptureDescription{}, nullptr);
            }, depth::linePull);
    }
}
LL_TYPE_INSTANCE_HOOK(WorldLines, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$renderEntityEffects, void, BaseActorRenderContext& context) {
    origin(context);
    auto& runtime = Runtime::instance();
    if (!runtime.enabled()) return;
    auto const settings = runtime.snapshot();
    auto const& preferences = settings->overlays;
    bool breaking = settings->interaction.breaking;
    if (releaseMeshes.exchange(false)) { shapeMeshes.clear(); restrictionMesh.release(); releaseLight(); eyeTracks.clear(); }
    if (!preferences.hitboxes) eyeTracks.clear();
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
            // Faint faces around the allowed cells near the anchor while the
            // button is held; the targeted block is left out so the vanilla
            // outline stays visible.
            auto region = interaction::breaking::region();
            if (region) {
                std::optional<Cell> target;
                auto const& hit = client.getLatestHitResult();
                if (hit.mType == HitResultType::Tile) target = Cell{hit.mBlock.x, hit.mBlock.y, hit.mBlock.z};
                thread_local std::optional<interaction::RestrictionRegion> cachedRegion;
                thread_local std::optional<Cell> cachedTarget;
                thread_local CellSurface restriction;
                thread_local uint64_t restrictionRevision = 0;
                if (cachedRegion != region || cachedTarget != target || !restrictionRevision) {
                    auto cells = region->preview(4);
                    if (target) cells.erase(*target);
                    restriction = cellSurface(cells);
                    ++restrictionRevision;
                    cachedRegion = region;
                    cachedTarget = target;
                }
                auto const material = faceMaterial(client);
                auto [r, g, b] = shapeColor(ShapeColor::White, false);
                CellStyle style{r, g, b, true};
                if (restrictionMesh.stale(restrictionRevision, style, material))
                    buildCellMesh(context.mScreenContext, restrictionMesh, restriction.faces, restriction.lines, style, material,
                                  restrictionRevision);
                if (context.mImpl) drawCellMesh(context.mScreenContext, context.mImpl->mCameraPosition, restrictionMesh, material);
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
            if (context.mImpl && CameraSessions::instance().detachedCameraActive()) {
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
            int const dimensionId = static_cast<int>(player->getDimensionId());
            if (eyeDimension != dimensionId) { eyeTracks.clear(); eyeDimension = dimensionId; }
            uint64_t const tick = player->getLevel().getCurrentTick().tickID;
            ++eyeFrame;
            std::vector<Line> white, red, blue;
            // Only borrow client actors during this pass. No entity pointers or
            // bounds survive world exit or a subsequent frame.
            for (auto* actor : player->getLevel().getRuntimeActorList()) {
                if (!actor || actor == player || &actor->getDimension() != &dimension) continue;
                auto const& bounds = actor->getAABB();
                float const alpha = context.getFrameAlpha(*actor);
                Vec3 const simulated = actor->getPosition();
                Vec3 const rendered = actor->getInterpolatedPosition(alpha);
                Point const offset = hitboxRenderOffset({simulated.x,simulated.y,simulated.z},
                                                        {rendered.x,rendered.y,rendered.z});
                Point min = moveHitboxPoint({bounds.min.x,bounds.min.y,bounds.min.z}, offset);
                Point max = moveHitboxPoint({bounds.max.x,bounds.max.y,bounds.max.z}, offset);
                if (!hitboxInRange(min,max,{camera.x,camera.y,camera.z},preferences.hitboxDistance)) continue;
                auto edges = wireBox(min,max);
                white.insert(white.end(),edges.begin(),edges.end());
                // Java shows the eye box and look line for mobs only; items
                // and other eyeless entities keep the white bounds alone.
                if (!actor->hasType(ActorType::Mob)) continue;
                Vec3 const rawEye = actor->getEyePos();
                Point const rawOffset{rawEye.x-simulated.x,rawEye.y-simulated.y,rawEye.z-simulated.z};
                if (!finite(rawOffset)) continue;
                auto& track = eyeTracks[actor->getRuntimeID()];
                track.seenFrame = eyeFrame;
                Point const eye = moveHitboxPoint({rendered.x,rendered.y,rendered.z},
                    track.offset.sample(tick, rawOffset, alpha));
                auto marker = eyeBox(eye);
                red.insert(red.end(),marker.begin(),marker.end());
                Vec3 const view = actor->getViewVector(alpha);
                blue.push_back(lookLine(eye, view.x, view.y, view.z));
            }
            std::erase_if(eyeTracks, [](auto const& entry) { return entry.second.seenFrame != eyeFrame; });
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
std::vector<Footprint> footprints(int dimension) {
    std::lock_guard lock(shapeMutex);
    // Boxes by shape and the revision they were computed for.
    static std::map<ShapeId, std::pair<uint64_t, std::optional<ShapeFootprint>>> cache;
    std::erase_if(cache, [](auto const& entry) { return !shapeCollection.find(entry.first); });
    std::vector<Footprint> result;
    for (auto const& [id, shape] : shapeCollection.entries()) {
        if (shape.definition.dimension != dimension) continue;
        auto& cached = cache[id];
        if (cached.first != shape.revision || !cached.first) cached = {shape.revision, shapeFootprint(shape)};
        if (cached.second)
            result.push_back({id, shape.definition.name, shape.definition.color, shape.definition.visible, *cached.second});
    }
    return result;
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

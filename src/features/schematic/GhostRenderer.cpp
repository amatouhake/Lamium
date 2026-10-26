#include "features/schematic/GhostRenderer.h"
#include "mc/world/level/block/actor/BlockActorType.h"
#include "features/schematic/AreaSave.h"
#include "features/schematic/PlacementOverlap.h"
#include "features/schematic/EntityModels.h"
#include "features/schematic/GhostActors.h"
#include "features/schematic/GhostMarks.h"
#include "features/schematic/GhostMesh.h"
#include "features/schematic/RayCells.h"
#include "features/schematic/GhostVerify.h"
#include "overlay/LineColor.h"
#include "features/schematic/SchematicItems.h"
#include "features/schematic/SchematicRegion.h"
#include "features/schematic/SchematicSession.h"
#include "features/schematic/Selection.h"
#include "overlay/FaceMaterial.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "app/CapabilityHooks.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/ActorShaderManager.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/deps/minecraft_renderer/framebuilder/dragon/RenderMetadata.h"
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/blockactor/BlockActorRenderDispatcher.h"
#include "mc/client/renderer/blockactor/MovingBlockActorRenderer.h"
#include "mc/client/renderer/game/LevelRendererCamera.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/deps/core_graphics/enums/PrimitiveMode.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include "mc/deps/minecraft_renderer/renderer/MeshData.h"
#include "mc/client/renderer/SupplementaryFieldAutoGenerationMode.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureDescription.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/deps/renderer/Camera.h"
#include "mc/deps/renderer/MatrixStack.h"
#include "mc/options/GraphicsMode.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockRenderLayer.h"
#include "mc/world/level/block/BrightnessPair.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/block/actor/BlockActorRendererId.h"
#include "mc/world/phys/HitResult.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <format>
#include <map>
#include <set>
#include <optional>
#include <tuple>
#include <variant>
#include <vector>

// The ghost pass (BACKLOG L-93, split by L-136): which sections to build and
// draw each frame, the drawing of the built meshes and block actors, and the
// lifecycle (feature off, world exit, dimension and graphics mode changes).
// The meshes are built in GhostMesh, checked in GhostVerify, saves run in
// AreaSave and the other in-world marks are drawn by GhostMarks.
namespace lamium::schematic::ghosts {
namespace {
#ifdef LAMIUM_SCHEMATIC_PERF_TRACE
// Measurement for the update plan (SCHEMATIC.md C): where the ghost pass
// spends its time and why sections are rebuilt, logged every 5 s.
struct Perf {
    std::chrono::steady_clock::time_point since = std::chrono::steady_clock::now();
    std::uint64_t frames = 0, frameNs = 0, maxFrameNs = 0, buildNs = 0, maxBuildNs = 0, builds = 0, checkNs = 0, checks = 0,
                  scanNs = 0, blendNs = 0, waiting = 0, maxWaiting = 0, dueLatencyNs = 0, dueCount = 0, maxDueLatencyNs = 0,
                  prepareNs = 0, facesNs = 0, linesNs = 0, actorsNs = 0, framesNs = 0, entitiesNs = 0, extrasNs = 0;
    std::uint64_t reasons[5]{}; // new, looked-at change, signature changed, mesh gone, chunk incomplete
    std::size_t sections = 0;
    // L-193: blended re-sorts, and frames whose camera position differs from
    // the previous blended frame.
    std::uint64_t orderChanges = 0, cameraMoves = 0, blendedFrames = 0;
} perf;
Vec3 lastCamera{};
struct PerfTimer {
    std::uint64_t& into;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    ~PerfTimer() { into += static_cast<std::uint64_t>((std::chrono::steady_clock::now() - start).count()); }
};
void perfReport();
#endif
constexpr int checkBudget = 32;       // Sections whose blocks are compared per frame.
// Look at the world this often: quickly near the camera, where blocks are
// being placed, slowly elsewhere. A section is rebuilt only when its blocks
// changed since it was built.
constexpr std::chrono::milliseconds refreshNear{250}, refreshFar{2000};
constexpr double nearDistance = 24;
constexpr std::chrono::milliseconds lookedDelay{100};
// Section rebuilds per frame stop once this much time went into them (a
// count of 3 made 20-30 ms frames when large sections came together).
constexpr std::chrono::microseconds buildTimeBudget{3000};
constexpr int sectionLimit = 32;
std::map<SectionKey, Section> sections;
std::vector<Resolved> resolved;
std::vector<std::string> builtKeys; // drawKey of each resolved placement
// Created once per cell; a null result is remembered too.
// Keyed by the block as well: two placements may want different block
// entities in one cell.
std::map<std::tuple<int, int, int, Block const*>, std::optional<std::shared_ptr<BlockActor>>> actors;
std::vector<std::pair<BlockPos, Block const*>> watched; // Recently looked-at cells and what was there.
std::uint64_t builtRevision = 0;
// The camera's cells and position the sections near it are built for, and
// the graphics mode they are built in (sections are rebuilt when it changes).
BuildCamera buildCamera;
struct Wanted { std::tuple<int, int, int, int> key; double distance; bool seen; };
// The wanted sections of the last listing (drawPlacements).
struct WantedCache {
    bool valid = false;
    Point cell;
    std::uint64_t revision = 0, stamp = 0;
    int dimension = -1;
    Clock::time_point at{};
    std::vector<Wanted> list;
} wantedCache;
int lastGraphicsMode = -1;
int builtDimension = -1;
std::atomic<bool> releaseRequested{false};
ll::event::ListenerPtr exitListener;
bool installed = false, blendInstalled = false;

// Every section's blended quads in one mesh, sorted far to near from where
// the camera was (L-192, L-193). Sorted again when the camera has moved or
// the quads changed, at most this often; standing still keeps one order.
struct Combined {
    std::optional<mce::Mesh> mesh;
    std::uint32_t vertices = 0;
    glm::dvec3 anchor{}; // positions are relative to it
    std::uint64_t key = 0;
    Vec3 sortedFrom{};
    Clock::time_point sortedAt{};
    // The sections in the mesh: they skip their own meshes until the next
    // sort, so one entering the budget meanwhile is not lost (water ghosts
    // blinked out by section between sorts) and one leaving is not doubled.
    std::set<SectionKey> members;
} combined;
// Meshes cannot be assigned: cleared member by member.
void forgetCombined() {
    combined.mesh.reset();
    combined.vertices = 0;
    combined.key = 0;
    combined.members.clear();
}
constexpr std::chrono::milliseconds resortEvery{50};
constexpr float resortDistance = .25f;
constexpr size_t manyBlendedQuads = 20000;
// The nearest sections (to their closest point) join the camera-sorted mesh
// up to this many quads; the rest draw their own meshes, sections far to
// near. A fixed 24-block radius left far overlaps flickering, and sorting a
// whole large liquid ghost was too slow (terrain_water_lamium).
constexpr size_t sortedBlendedQuads = 30000;

void release() {
    forgetCombined();
    sections.clear();
    wantedCache.valid = false;
    resetMarks();
    resolved.clear();
    models::reset();
    builtKeys.clear();
    actors.clear();
    watched.clear();
    builtRevision = 0;
    resetVerification();
}
void drawPlacements(BaseActorRenderContext& context, IClientInstance& client, LocalPlayer& player) {
#ifdef LAMIUM_SCHEMATIC_PERF_TRACE
    auto stage = std::chrono::steady_clock::now();
    auto lap = [&](std::uint64_t& into) {
        auto now = std::chrono::steady_clock::now();
        into += static_cast<std::uint64_t>((now - stage).count());
        stage = now;
    };
#define LAMIUM_PERF_LAP(field) lap(perf.field)
#else
#define LAMIUM_PERF_LAP(field)
#endif
    auto snapshot = session::snapshot();
    int dimension = static_cast<int>(player.getDimensionId());
    // A file replaced on disk loads as a new structure without a new
    // revision; its placement must be resolved again (its palette changed).
    bool replaced = resolved.size() != snapshot.placements.size();
    for (size_t i = 0; !replaced && i < resolved.size(); ++i)
        replaced = resolved[i].structure != snapshot.placements[i].structure.get();
    if (replaced || snapshot.revision != builtRevision || dimension != builtDimension) {
        // Placements whose draw key is unchanged keep their resolved blocks
        // and built sections (moved to their new index); only changed ones
        // are rebuilt, so the others do not blink.
        if (dimension != builtDimension) release();
        std::vector<Resolved> nextResolved;
        std::vector<std::string> nextKeys;
        std::map<SectionKey, Section> nextSections;
        std::vector<bool> taken(resolved.size());
        for (int i = 0; i < static_cast<int>(snapshot.placements.size()); ++i) {
            auto const& shown = snapshot.placements[static_cast<size_t>(i)];
            // Overlapping earlier placements are part of the key: they decide
            // which cells this one leaves out (L-141).
            auto key = drawKey(shown.placement) + overlap::touchingKey(snapshot.placements, static_cast<size_t>(i));
            int old = -1;
            for (int j = 0; j < static_cast<int>(builtKeys.size()); ++j)
                if (!taken[static_cast<size_t>(j)] && builtKeys[static_cast<size_t>(j)] == key
                    && resolved[static_cast<size_t>(j)].structure == shown.structure.get()) { old = j; break; }
            if (old >= 0) {
                taken[static_cast<size_t>(old)] = true;
                nextResolved.push_back(std::move(resolved[static_cast<size_t>(old)]));
                for (auto it = sections.begin(); it != sections.end();) {
                    if (std::get<0>(it->first) != old) { ++it; continue; }
                    auto node = sections.extract(it++);
                    std::get<0>(node.key()) = i;
                    nextSections.insert(std::move(node));
                }
            } else {
                nextResolved.push_back(shown.structure ? resolve(*shown.structure, shown.placement) : Resolved{});
                nextResolved.back().keep = shown.structure;
            }
            nextKeys.push_back(std::move(key));
        }
        resolved = std::move(nextResolved);
        builtKeys = std::move(nextKeys);
        sections = std::move(nextSections);
        actors.clear();
        builtRevision = snapshot.revision;
        builtDimension = dimension;
    }
    ScreenContext& screen = context.mScreenContext;
    Vec3 const camera = context.mImpl->mCameraPosition;
    auto& region = player.getDimensionBlockSource();
    // When the camera moves to another cell, rebuild the sections around
    // the old and the new cells (which ghosts are drawn whole changes).
    {
        auto cellAt = [](double x, double y, double z) {
            return Point{static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), static_cast<int>(std::floor(z))};
        };
        std::array<std::optional<Point>, 2> now{cellAt(camera.x, camera.y, camera.z), cellAt(camera.x, camera.y - 1.62, camera.z)};
        buildCamera.eye = camera;
        if (now != buildCamera.cells) {
            auto section = [](int v) { return static_cast<int>(std::floor(v / static_cast<double>(sectionSize))); };
            auto mark = [&](std::optional<Point> const& c) {
                if (!c) return;
                for (int i = 0; i < static_cast<int>(snapshot.placements.size()); ++i)
                    for (int dx = -2; dx <= 2; dx += 2) for (int dy = -2; dy <= 2; dy += 2) for (int dz = -2; dz <= 2; dz += 2)
                        if (auto found = sections.find({i, section(c->x + dx), section(c->y + dy), section(c->z + dz)}); found != sections.end())
                            found->second.due = Clock::now();
            };
            for (auto const& c : buildCamera.cells) mark(c);
            for (auto const& c : now) mark(c);
            buildCamera.cells = now;
        }
    }

    // What the camera can see: a section entirely outside one side of the
    // view is neither drawn nor built before the ones in view. Without the
    // camera's matrices everything counts as in view.
    std::optional<glm::mat4> clip;
    if (!screen.camera.viewMatrixStack->stack->empty() && !screen.camera.projectionMatrixStack->stack->empty()
        && !screen.camera.worldMatrixStack->stack->empty())
        clip = *screen.camera.projectionMatrixStack->top()._m * *screen.camera.viewMatrixStack->top()._m
            * *screen.camera.worldMatrixStack->top()._m;
    auto inView = [&](int sx, int sy, int sz) {
        if (!clip) return true;
        glm::vec3 low{static_cast<float>(sx * sectionSize - camera.x), static_cast<float>(sy * sectionSize - camera.y),
                      static_cast<float>(sz * sectionSize - camera.z)};
        int outside[6]{};
        for (int k = 0; k < 8; ++k) {
            glm::vec4 c = *clip * glm::vec4(low + glm::vec3(k & 1 ? sectionSize : 0, k & 2 ? sectionSize : 0, k & 4 ? sectionSize : 0), 1.f);
            outside[0] += c.x < -c.w; outside[1] += c.x > c.w; outside[2] += c.y < -c.w;
            outside[3] += c.y > c.w; outside[4] += c.w <= 0; outside[5] += c.z > c.w;
        }
        return std::none_of(std::begin(outside), std::end(outside), [](int n) { return n == 8; });
    };
    // Sections near the camera, for visible placements in this dimension,
    // nearest first. Listed again when the camera enters another cell, the
    // placements change or half a second passed; each frame they are only
    // split by whether they are in view (with 8-block sections the full list
    // runs to thousands).
    Point cameraCell{static_cast<int>(std::floor(camera.x)), static_cast<int>(std::floor(camera.y)), static_cast<int>(std::floor(camera.z))};
    if (!wantedCache.valid || !(wantedCache.cell == cameraCell) || wantedCache.revision != snapshot.revision
        || wantedCache.dimension != dimension || Clock::now() - wantedCache.at > std::chrono::milliseconds(500)) {
    std::vector<Wanted> wanted;
    for (int i = 0; i < static_cast<int>(snapshot.placements.size()); ++i) {
        auto const& shown = snapshot.placements[static_cast<size_t>(i)];
        if (!shown.structure || !shown.placement.visible || shown.placement.dimension != dimension) continue;
        Size placed = placedSize(shown.structure->size, shown.placement.placement.rotation);
        Point const& o = shown.placement.placement.origin;
        auto section = [](double v) { return static_cast<int>(std::floor(v / sectionSize)); };
        // Only the sections of the box within the draw distance of the camera.
        int fromX = std::max(section(o.x), section(camera.x - drawDistance)), toX = std::min(section(o.x + placed.x - 1), section(camera.x + drawDistance));
        int fromY = std::max(section(o.y), section(camera.y - drawDistance)), toY = std::min(section(o.y + placed.y - 1), section(camera.y + drawDistance));
        int fromZ = std::max(section(o.z), section(camera.z - drawDistance)), toZ = std::min(section(o.z + placed.z - 1), section(camera.z + drawDistance));
        for (int sx = fromX; sx <= toX; ++sx)
            for (int sy = fromY; sy <= toY; ++sy)
                for (int sz = fromZ; sz <= toZ; ++sz) {
                    double cx = (sx + .5) * sectionSize - camera.x, cy = (sy + .5) * sectionSize - camera.y,
                           cz = (sz + .5) * sectionSize - camera.z;
                    double distance = std::sqrt(cx * cx + cy * cy + cz * cz);
                    if (distance <= drawDistance) wanted.push_back({{i, sx, sy, sz}, distance, false});
                }
    }
    std::sort(wanted.begin(), wanted.end(), [](auto const& a, auto const& b) { return a.distance < b.distance; });
    // Sections no longer wanted go.
    std::uint64_t stamp = ++wantedCache.stamp;
    for (auto const& w : wanted)
        if (auto found = sections.find(w.key); found != sections.end()) found->second.wantedStamp = stamp;
    std::erase_if(sections, [&](auto const& entry) { return entry.second.wantedStamp != stamp; });
    wantedCache.list = std::move(wanted);
    wantedCache.cell = cameraCell;
    wantedCache.revision = snapshot.revision;
    wantedCache.dimension = dimension;
    wantedCache.at = Clock::now();
    wantedCache.valid = true;
    }
    // In view first, each part nearest first.
    std::vector<Wanted> wanted;
    wanted.reserve(wantedCache.list.size());
    for (int pass = 0; pass < 2; ++pass)
        for (auto w : wantedCache.list) {
            w.seen = inView(std::get<1>(w.key), std::get<2>(w.key), std::get<3>(w.key));
            if (w.seen == (pass == 0)) wanted.push_back(w);
        }

    // The block in the crosshair and the cell against its face are where a
    // block is broken or placed next: when either changes, rebuild its
    // section soon instead of waiting for the periodic refresh. A broken
    // block vanishes at once, so its ghost returns at once. A placed block
    // exists a few frames before its terrain mesh is drawn, so its ghost goes
    // a little later; dropping it at once left an empty cell for a moment.
    std::vector<BlockPos> looked;
    if (auto const& hit = client.getLatestHitResult(); hit.mType == HitResultType::Tile) {
        static constexpr int offsets[6][3] = {{0,-1,0},{0,1,0},{0,0,-1},{0,0,1},{-1,0,0},{1,0,0}};
        BlockPos at = hit.mBlock;
        looked.push_back(at);
        if (hit.mFacing < 6) looked.push_back(BlockPos{at.x + offsets[hit.mFacing][0], at.y + offsets[hit.mFacing][1],
                                                         at.z + offsets[hit.mFacing][2]});
    }
    for (auto const& [pos, seen] : watched) {
        Block const& current = region.getBlock(pos);
        if (&current == seen) continue;
        auto due = Clock::now() + (current.isAir() ? Clock::duration{} : std::chrono::duration_cast<Clock::duration>(lookedDelay));
        auto section = [](int v) { return static_cast<int>(std::floor(v / static_cast<double>(sectionSize))); };
        // The cell's own section and any section across a face of it.
        for (int i = 0; i < static_cast<int>(snapshot.placements.size()); ++i)
            for (int sx = section(pos.x - 1); sx <= section(pos.x + 1); ++sx)
                for (int sy = section(pos.y - 1); sy <= section(pos.y + 1); ++sy)
                    for (int sz = section(pos.z - 1); sz <= section(pos.z + 1); ++sz)
                        if (auto found = sections.find({i, sx, sy, sz}); found != sections.end())
                            if (!found->second.due || due < *found->second.due) found->second.due = due;
    }
    // Keep the previous positions one more frame: placing moves the crosshair.
    std::vector<std::pair<BlockPos, Block const*>> next;
    for (auto const& pos : looked) next.push_back({pos, &region.getBlock(pos)});
    for (auto const& [pos, seen] : watched)
        if (next.size() < 6 && std::none_of(next.begin(), next.end(), [&](auto const& n) { return n.first == pos; }))
            next.push_back({pos, &region.getBlock(pos)});
    watched = std::move(next);

    LAMIUM_PERF_LAP(prepareNs);
    // Build missing sections and rebuild changed ones, in view and nearest
    // first, within the budgets.
    int budget = sectionLimit, checks = checkBudget;
    auto buildsStart = std::chrono::steady_clock::now();
    // The view outlives the tessellator that reads through it.
    std::unique_ptr<SchematicRegion> view;
    std::unique_ptr<BlockTessellator> own;
    auto now = Clock::now();
    // Ghost blocks come from the game's block tessellator and mesh streams;
    // frames, the Check list and materials do not, and keep working without
    // them. The in-world mistake outlines are built with the sections, so
    // they stop too.
    bool tessellate = versionSensitiveAllowed(capabilities::Id::SchematicBlocks);
    for (auto const& w : wanted) {
        if (!tessellate) break;
        if (!budget || (budget < sectionLimit && std::chrono::steady_clock::now() - buildsStart >= buildTimeBudget)) break;
        auto found = sections.find(w.key);
        auto refreshAfter = w.distance <= nearDistance ? std::chrono::duration_cast<Clock::duration>(refreshNear)
            : std::chrono::duration_cast<Clock::duration>(refreshFar);
        bool stale = found == sections.end() || !found->second.complete || (found->second.due && now >= *found->second.due)
            || (found->second.faces && !found->second.faces->isValid())
            || (found->second.blendMesh && !found->second.blendMesh->isValid())
            || std::any_of(found->second.lines.begin(), found->second.lines.end(), [](auto const& l) { return !l->mesh || !l->mesh->isValid(); })
            || (found->second.marks && !found->second.marks->isValid());
#ifdef LAMIUM_SCHEMATIC_PERF_TRACE
        int reason = found == sections.end() ? 0 : !found->second.complete ? 4 : (found->second.due && now >= *found->second.due) ? 1 : 3;
        if (stale && reason == 1) {
            auto late = static_cast<std::uint64_t>((now - *found->second.due).count()) + static_cast<std::uint64_t>(std::chrono::nanoseconds(lookedDelay).count());
            perf.dueLatencyNs += late;
            ++perf.dueCount;
            perf.maxDueLatencyNs = std::max(perf.maxDueLatencyNs, late);
        }
#endif
        if (!stale && now - found->second.checked > refreshAfter && checks > 0) {
            --checks;
            auto const index = static_cast<size_t>(std::get<0>(w.key));
#ifdef LAMIUM_SCHEMATIC_PERF_TRACE
            PerfTimer timer{perf.checkNs};
            ++perf.checks;
            reason = 2;
#endif
            stale = signatureOf(region, snapshot.placements[index], w.key) != found->second.signature;
            found->second.checked = now;
        }
        if (!stale) continue;
#ifdef LAMIUM_SCHEMATIC_PERF_TRACE
        ++perf.reasons[reason];
#endif
        if (!own) {
            // A private tessellator, primed with one appended block: in-world
            // tessellation on a fresh one crashed in the probe.
            view = std::make_unique<SchematicRegion>(region);
            own = std::make_unique<BlockTessellator>(view.get());
            static bool logged = false;
            if (!logged) log(std::format("tessellator default render layer {}", static_cast<int&>(own->mRenderingLayer)));
            logged = true;
            if (auto stone = Block::tryGetFromRegistry(HashedString{"minecraft:stone"})) {
                Tessellator primer(screen.tessellator.mBufferResourceService);
                primer.begin({}, mce::PrimitiveMode::QuadList, 64, false);
                own->appendTessellatedBlock(primer, *stone);
            }
        }
        auto const index = static_cast<size_t>(std::get<0>(w.key));
#ifdef LAMIUM_SCHEMATIC_PERF_TRACE
        auto buildStart = std::chrono::steady_clock::now();
#endif
        auto earlier = overlap::earlierMeeting(snapshot.placements, index);
        std::function<bool(Point)> owned;
        if (!earlier.empty())
            owned = [&](Point cell) { return overlap::ownedEarlier(snapshot.placements, earlier, dimension, cell); };
        auto neighbors = overlap::touching(snapshot.placements, index);
        std::function<NeighborGhost(Point)> others;
        if (!neighbors.empty()) {
            for (size_t j : neighbors) if (j < resolved.size()) prepareShapes(screen, *own, resolved[j]);
            others = [&](Point cell) {
                for (size_t j : neighbors)
                    if (j < resolved.size())
                        if (auto ghost = ghostDrawnAt(region, snapshot.placements[j], resolved[j], cell); ghost.block) return ghost;
                return NeighborGhost{};
            };
        }
        buildSection(screen, region, *view, *own, snapshot.placements[index], resolved[index], w.key, buildCamera, owned,
                     others, sections[w.key]);
#ifdef LAMIUM_SCHEMATIC_PERF_TRACE
        auto took = static_cast<std::uint64_t>((std::chrono::steady_clock::now() - buildStart).count());
        perf.buildNs += took;
        perf.maxBuildNs = std::max(perf.maxBuildNs, took);
        ++perf.builds;
#endif
        --budget;
    }
#ifdef LAMIUM_SCHEMATIC_PERF_TRACE
    {
        // Sections in range that have never been built: what the budget left waiting.
        std::uint64_t waiting = 0;
        for (auto const& w : wanted) waiting += sections.find(w.key) == sections.end();
        perf.waiting += waiting;
        perf.maxWaiting = std::max(perf.maxWaiting, waiting);
        perf.sections = sections.size();
    }
#endif

    {
#ifdef LAMIUM_SCHEMATIC_PERF_TRACE
        PerfTimer timer{perf.scanNs};
#endif
        checkEntities(region, player, snapshot, dimension, resolved);
        stepScan(region, snapshot, dimension, camera, resolved);
        stepProgress(region, snapshot, dimension, resolved);
    }

    // Draw: alpha-tested ghost faces (empty texels let water and glass show
    // through), then outlines, then block-entity models. Blended ghosts,
    // liquids and mistake marks are drawn later (drawBlended).
    auto& dispatcher = client.getBlockEntityRenderDispatcher();
    auto* moving = static_cast<MovingBlockActorRenderer*>(dispatcher.mRenderers.get()[BlockActorRendererId::MovingBlock].get());
    if (!moving) return;
    mce::TexturePtr const& atlas = moving->mAtlasTexture.get();
    auto* lightTexture = client.getLightTexture();
    mce::MaterialPtr const& faces = moving->mBlockMaterials[static_cast<int>(BlockRenderLayer::RenderlayerAlphatest)].get();
    auto fullBright = [&] {
        BrightnessPair full;
        full.sky->mValue = 15;
        full.block->mValue = 15;
        ActorShaderManager::setupShaderParameters(screen, region, full, glm::vec4{1, 1, 1, 1}, 1.f, true, *lightTexture,
            Vec2{1, 1}, Vec4{0, 0, 1, 1});
    };
    mce::MaterialPtr lineMaterial = overlay::lines::material();
    // Vertex-colored and blended without depth writes, as shape faces use in
    // Fancy graphics; under Vibrant Visuals the overlay face material.
    mce::MaterialPtr markMaterial = buildCamera.vibrant ? overlay::faceMaterial(client).material
                                                  : mce::MaterialPtr(mce::RenderMaterialGroup::switchable(), HashedString{"holo_hand_pointer"});
    std::variant<std::monostate, mce::TexturePtr, mce::ClientTexture, mce::ServerTexture> texture{atlas};
    std::unique_ptr<SchematicRegion> actorView;
    // In three passes, so the light is set up once for all faces (block
    // actors set up their own after): the per-section setups and draws were
    // the largest fixed cost with many large placements (C, measured).
    float const lineDistance = Runtime::instance().snapshot()->schematic.outlineDistance; // 0: no limit
    std::vector<std::pair<SectionKey const*, Section*>> shown;
    for (auto& [key, section] : sections)
        if (inView(std::get<1>(key), std::get<2>(key), std::get<3>(key))) shown.push_back({&key, &section});
    auto offsetOf = [&](Section const& section) {
        return glm::vec3{static_cast<float>(section.origin.x - camera.x), static_cast<float>(section.origin.y - camera.y),
                         static_cast<float>(section.origin.z - camera.z)};
    };
#ifdef LAMIUM_SCHEMATIC_PERF_TRACE
    stage = std::chrono::steady_clock::now();
#endif
    if (faces.mRenderMaterialInfoPtr && lightTexture) {
        fullBright();
        for (auto [key, section] : shown)
            if (section->faces)
                translated(screen, offsetOf(*section), [&] {
                    section->faces->renderMesh(screen, faces, texture, 0, section->faceVertices, OffscreenCaptureDescription{}, nullptr);
                });
    }
    LAMIUM_PERF_LAP(facesNs);
    for (auto [key, section] : shown) {
        // Outlines only near the camera: far away they are a few pixels of
        // noise, and a draw each.
        glm::vec3 offset = offsetOf(*section);
        glm::vec3 nearest = glm::clamp(glm::vec3{0.f}, offset, offset + glm::vec3(static_cast<float>(sectionSize)));
        bool lined = !section->lines.empty() && (lineDistance <= 0 || glm::length(nearest) <= lineDistance);
        if (!lined && !section->marks) continue;
        translated(screen, offset, [&] {
            if (section->marks && markMaterial.mRenderMaterialInfoPtr)
                section->marks->renderMesh(screen, markMaterial, gsl::span<mce::ClientTexture const*>{}, 0, section->markVertices,
                    OffscreenCaptureDescription{}, nullptr);
            if (lined && lineMaterial.mRenderMaterialInfoPtr)
                for (auto const& l : section->lines)
                    overlay::lines::colored(screen, l->color.r, l->color.g, l->color.b, [&] {
                        l->mesh->renderMesh(screen, lineMaterial, gsl::span<mce::ClientTexture const*>{}, 0, l->vertices,
                            OffscreenCaptureDescription{}, nullptr);
                    });
        });
    }
    LAMIUM_PERF_LAP(linesNs);
    for (auto [keyPtr, sectionPtr] : shown) {
        auto const& key = *keyPtr;
        auto& section = *sectionPtr;
        for (auto const& [pos, block, data] : section.entities) {
            // The renderer reads the block at the cell (wall or standing
            // banner, which head, facing): the ghost's, not the world's air.
            if (!actorView) {
                actorView = std::make_unique<SchematicRegion>(region);
                actorView->fullLight = true;
            }
            // A piston's renderer may also look at its head's cell: the
            // placement's ghost there (the arm block), not the world's air.
            bool piston = block->getBlockType().mBlockEntityType == BlockActorType::PistonArm;
            auto placementIndex = static_cast<size_t>(std::get<0>(key));
            actorView->answer = [&, piston, placementIndex](BlockPos const& at) -> Block const* {
                if (at == pos) return block;
                if (!piston || placementIndex >= resolved.size()) return nullptr;
                return ghostDrawnAt(region, snapshot.placements[placementIndex], resolved[placementIndex], {at.x, at.y, at.z}).block;
            };
            auto& actor = actors[{pos.x, pos.y, pos.z, block}];
            if (!actor)
                actor = makeBlockActor(*block, pos, data ? &*data : nullptr,
                                       snapshot.placements[static_cast<size_t>(std::get<0>(key))].placement.placement, region);
            if (!*actor) continue;
            Vec3 renderPos{static_cast<float>(pos.x - camera.x), static_cast<float>(pos.y - camera.y), static_cast<float>(pos.z - camera.z)};
            renderBlockActor(context, *actorView, **actor, *block, renderPos, pos);
        }
    }
    if (actorView) actorView->answer = nullptr;
    LAMIUM_PERF_LAP(actorsNs);
    drawPlacementFrames(screen, snapshot, dimension, camera);
    LAMIUM_PERF_LAP(framesNs);
    drawEntities(screen, client, snapshot, dimension, camera, resolved);
    LAMIUM_PERF_LAP(entitiesNs);
    drawNameTags(screen, client, region, *moving, camera);
    LAMIUM_PERF_LAP(extrasNs);
#undef LAMIUM_PERF_LAP
}
// The blended meshes (stained glass, honey, slime, liquids, mistake marks),
// sections far to near, in the block entities' alpha pass: after the
// world's own translucent blocks, so those are not lost behind a ghost's
// depth, and without depth writes (a beacon beam's material), so one
// sorted mesh decides what lies over what. Drawn as separate passes before,
// marks and blended ghosts swapped order from frame to frame.
std::uint64_t ghostFrame = 0, blendedFrame = 0;
void drawBlended(BaseActorRenderContext& context) {
    if (sections.empty() || !context.mImpl || blendedFrame == ghostFrame) return;
    blendedFrame = ghostFrame;
    IClientInstance& client = context.mClientInstance;
    auto* player = client.getLocalPlayer();
    auto* lightTexture = client.getLightTexture();
    auto& dispatcher = client.getBlockEntityRenderDispatcher();
    auto* moving = static_cast<MovingBlockActorRenderer*>(dispatcher.mRenderers.get()[BlockActorRendererId::MovingBlock].get());
    if (!player || !lightTexture || !moving) return;
    mce::MaterialPtr material(mce::RenderMaterialGroup::switchable(), HashedString{"beacon_beam_transparent"});
    if (!material.mRenderMaterialInfoPtr) material = mce::MaterialPtr(mce::RenderMaterialGroup::common(), HashedString{"beacon_beam_transparent"});
    static bool named = false;
    if (!std::exchange(named, true)) log(material.mRenderMaterialInfoPtr ? "blended ghosts use beacon_beam_transparent" : "beacon_beam_transparent not found");
    if (!material.mRenderMaterialInfoPtr) return;
    ScreenContext& screen = context.mScreenContext;
    Vec3 const camera = context.mImpl->mCameraPosition;
    auto& region = player->getDimensionBlockSource();
    std::variant<std::monostate, mce::TexturePtr, mce::ClientTexture, mce::ServerTexture> texture{moving->mAtlasTexture.get()};
    struct Blended { double gap; SectionKey key; Section const* section; };
    std::vector<Blended> blendedSections;
    for (auto const& [sectionKey, section] : sections) {
        if (!section.blend || section.blend->positions.empty()) continue;
        auto gap = [](double v, double low) { return std::max({low - v, 0., v - (low + sectionSize)}); };
        double gx = gap(camera.x, section.origin.x), gy = gap(camera.y, section.origin.y), gz = gap(camera.z, section.origin.z);
        blendedSections.push_back({gx * gx + gy * gy + gz * gz, sectionKey, &section});
    }
    std::stable_sort(blendedSections.begin(), blendedSections.end(), [](auto const& a, auto const& b) { return a.gap < b.gap; });
    std::vector<BlendQuads const*> parts;
    std::vector<SectionKey> partKeys;
    std::uint64_t key = 1469598103934665603ull;
    size_t sorted = 0;
    for (auto const& b : blendedSections) {
        size_t quads = b.section->blend->positions.size() / 4;
        if (parts.empty() || sorted + quads <= sortedBlendedQuads || !b.section->blendMesh) {
            parts.push_back(b.section->blend.get());
            partKeys.push_back(b.key);
            sorted += quads;
            key = (key ^ reinterpret_cast<std::uintptr_t>(b.section->blend.get())) * 1099511628211ull;
        }
    }
    if (parts.empty()) forgetCombined();
    if (blendedSections.empty()) return;
    auto now = Clock::now();
    float moved = std::max({std::abs(camera.x - combined.sortedFrom.x), std::abs(camera.y - combined.sortedFrom.y),
                            std::abs(camera.z - combined.sortedFrom.z)});
    // Many quads (large liquid ghosts) sort and upload less often while moving.
    size_t total = 0;
    for (auto const* b : parts) total += b->positions.size() / 4;
    auto every = total > manyBlendedQuads ? resortEvery * 5 : resortEvery;
    // A new set of sections waits for the interval too: while walking, the
    // last section in the budget changed nearly every frame, and the sort and
    // upload ran each frame (6-7 ms with a large liquid ghost).
    bool resort = !parts.empty()
        && (!combined.mesh || !combined.mesh->isValid()
            || ((key != combined.key || moved >= resortDistance) && now - combined.sortedAt >= every));
    if (resort) {
        combined.mesh.reset();
        combined.anchor = {std::floor(camera.x), std::floor(camera.y), std::floor(camera.z)};
        // Every quad by its center, far to near from the camera.
        struct Quad { float distance; std::uint32_t part, index; };
        std::vector<Quad> quads;
        for (std::uint32_t p = 0; p < parts.size(); ++p) {
            auto const& b = *parts[p];
            for (std::uint32_t q = 0; q < b.positions.size() / 4; ++q) {
                glm::vec3 c = (b.positions[q * 4] + b.positions[q * 4 + 1] + b.positions[q * 4 + 2] + b.positions[q * 4 + 3]) * .25f
                    + b.origin;
                float dx = c.x - camera.x, dy = c.y - camera.y, dz = c.z - camera.z;
                quads.push_back({dx * dx + dy * dy + dz * dz, p, q});
            }
        }
        std::stable_sort(quads.begin(), quads.end(), [](Quad const& a, Quad const& b) { return a.distance > b.distance; });
        Tessellator batch(screen.tessellator.mBufferResourceService);
        batch.begin({}, mce::PrimitiveMode::QuadList, static_cast<int>(quads.size() * 4), false);
        auto& data = static_cast<mce::MeshData&>(batch.mMeshData);
        // A stream is kept only when every part has it for every vertex.
        auto merge = [&](auto member, auto& to) {
            to.clear();
            for (auto const* b : parts)
                if ((b->*member).size() != b->positions.size()) return;
            to.reserve(quads.size() * 4);
            for (auto const& q : quads)
                for (std::uint32_t k = 0; k < 4; ++k) to.push_back((parts[q.part]->*member)[q.index * 4 + k]);
        };
        auto& positions = *data.mPositions;
        positions.clear();
        positions.reserve(quads.size() * 4);
        glm::vec3 low{1e9f}, high{-1e9f};
        for (auto const& q : quads) {
            auto const& b = *parts[q.part];
            glm::dvec3 shift = glm::dvec3(b.origin) - combined.anchor;
            for (std::uint32_t k = 0; k < 4; ++k) {
                glm::vec3 v = glm::vec3(glm::dvec3(b.positions[q.index * 4 + k]) + shift);
                positions.push_back(v);
                low = glm::min(low, v);
                high = glm::max(high, v);
            }
        }
        merge(&BlendQuads::normals, *data.mNormals);
        merge(&BlendQuads::tangents, *data.mTangents);
        merge(&BlendQuads::colors, *data.mColors);
        merge(&BlendQuads::mers, *data.mMERS);
        merge(&BlendQuads::bones, *data.mBoneId0s);
        merge(&BlendQuads::pbr, *data.mPBRTextureIndices);
        merge(&BlendQuads::geo, *data.mGeoType);
        for (int i = 0; i < 3; ++i) {
            auto& to = *data.mTextureUVs[i];
            to.clear();
            bool all = std::all_of(parts.begin(), parts.end(), [&](auto const* b) { return b->uvs[i].size() == b->positions.size(); });
            if (!all) continue;
            to.reserve(quads.size() * 4);
            for (auto const& q : quads)
                for (std::uint32_t k = 0; k < 4; ++k) to.push_back(parts[q.part]->uvs[i][q.index * 4 + k]);
        }
        *data.mFieldEnabled = parts.front()->enabled;
        *data.mAABB = {low, high};
        static_cast<unsigned&>(batch.mCount) = static_cast<unsigned>(quads.size() * 4);
        combined.vertices = static_cast<std::uint32_t>(quads.size() * 4);
        combined.mesh.emplace(batch.end(Tessellator::UploadMode::Buffered, "Lamium schematic blended ghosts", SupplementaryFieldAutoGenerationMode{}));
        combined.key = key;
        combined.members = {partKeys.begin(), partKeys.end()};
        combined.sortedFrom = camera;
        combined.sortedAt = now;
#ifdef LAMIUM_SCHEMATIC_PERF_TRACE
        ++perf.orderChanges;
#endif
    }
#ifdef LAMIUM_SCHEMATIC_PERF_TRACE
    ++perf.blendedFrames;
    if (camera.x != lastCamera.x || camera.y != lastCamera.y || camera.z != lastCamera.z) ++perf.cameraMoves;
    lastCamera = camera;
#endif
    BrightnessPair full;
    full.sky->mValue = 15;
    full.block->mValue = 15;
    ActorShaderManager::setupShaderParameters(screen, region, full, glm::vec4{1, 1, 1, 1}, 1.f, true, *lightTexture, Vec2{1, 1},
        Vec4{0, 0, 1, 1});
    // Sections not in the sorted mesh, far to near by their centers.
    std::vector<std::pair<double, Section const*>> far;
    for (auto const& b : blendedSections) {
        if (combined.members.contains(b.key) || !b.section->blendMesh) continue;
        double cx = b.section->origin.x + sectionSize / 2. - camera.x, cy = b.section->origin.y + sectionSize / 2. - camera.y,
               cz = b.section->origin.z + sectionSize / 2. - camera.z;
        far.push_back({cx * cx + cy * cy + cz * cz, b.section});
    }
    std::sort(far.begin(), far.end(), [](auto const& a, auto const& b) { return a.first > b.first; });
    for (auto const& [distance, section] : far) {
        glm::vec3 at{static_cast<float>(section->origin.x - camera.x), static_cast<float>(section->origin.y - camera.y),
                     static_cast<float>(section->origin.z - camera.z)};
        translated(screen, at, [&] {
            section->blendMesh->renderMesh(screen, material, texture, 0, section->blendVertices, OffscreenCaptureDescription{}, nullptr);
        });
    }
    if (!combined.mesh || !combined.vertices) return;
    glm::vec3 offset{static_cast<float>(combined.anchor.x - camera.x), static_cast<float>(combined.anchor.y - camera.y),
                     static_cast<float>(combined.anchor.z - camera.z)};
    translated(screen, offset, [&] {
        combined.mesh->renderMesh(screen, material, texture, 0, combined.vertices, OffscreenCaptureDescription{}, nullptr);
    });
}
// Calls of the alpha pass per ghost frame, logged once (spike: is it once a frame?).
int alphaCalls = 0;

LL_TYPE_INSTANCE_HOOK(GhostBlendPass, ll::memory::HookPriority::Normal, LevelRendererCamera,
    &LevelRendererCamera::$renderBlockEntities, void, BaseActorRenderContext& context, bool renderAlphaLayer) {
    origin(context, renderAlphaLayer);
    if (!renderAlphaLayer) return;
    ++alphaCalls;
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.snapshot()->schematic.enabled) return;
    try {
#ifdef LAMIUM_SCHEMATIC_PERF_TRACE
        PerfTimer timer{perf.blendNs};
#endif
        if (versionSensitiveAllowed(capabilities::Id::SchematicBlend)) drawBlended(context);
    } catch (std::exception const& error) {
        static bool reported = false;
        if (!std::exchange(reported, true)) log(std::string("drawing blended ghosts failed: ") + error.what());
    }
}
#ifdef LAMIUM_SCHEMATIC_PERF_TRACE
void perfReport() {
    auto now = std::chrono::steady_clock::now();
    if (now - perf.since < std::chrono::seconds(5) || !perf.frames) return;
    auto ms = [](std::uint64_t ns) { return static_cast<double>(ns) / 1e6; };
    auto per = [&](std::uint64_t ns) { return ms(ns) / static_cast<double>(perf.frames); };
    log(std::format("perf {} frames: ghost pass {:.3f} ms/frame (max {:.2f}), builds {} ({:.3f} ms/frame, max {:.2f} ms each; "
                    "new {} looked-at {} changed {} mesh-gone {} incomplete {}), checks {} ({:.3f} ms/frame), check/scan {:.3f} ms/frame, "
                    "blended pass {:.3f} ms/frame, sections {}, never-built waiting avg {:.1f} max {}, looked-at latency avg {:.0f} ms max {:.0f} ms; "
                    "prepare {:.3f}, faces {:.3f}, lines {:.3f}, block actors {:.3f}, frames {:.3f}, entities {:.3f}, name tags {:.3f} ms/frame; "
                    "blended frames {}, re-sorts {}, camera moves {}",
                    perf.frames, per(perf.frameNs), ms(perf.maxFrameNs), perf.builds, per(perf.buildNs), ms(perf.maxBuildNs), perf.reasons[0],
                    perf.reasons[1], perf.reasons[2], perf.reasons[3], perf.reasons[4], perf.checks, per(perf.checkNs), per(perf.scanNs),
                    per(perf.blendNs), perf.sections, static_cast<double>(perf.waiting) / static_cast<double>(perf.frames), perf.maxWaiting,
                    perf.dueCount ? ms(perf.dueLatencyNs) / static_cast<double>(perf.dueCount) : 0., ms(perf.maxDueLatencyNs),
                    per(perf.prepareNs), per(perf.facesNs), per(perf.linesNs), per(perf.actorsNs), per(perf.framesNs), per(perf.entitiesNs),
                    per(perf.extrasNs), perf.blendedFrames, perf.orderChanges, perf.cameraMoves));
    perf = Perf{};
}
#endif

LL_TYPE_INSTANCE_HOOK(GhostPass, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$renderEntityEffects, void, BaseActorRenderContext& context) {
    origin(context);
    if (++ghostFrame == 600) log(std::format("block entity alpha pass ran {} times in 600 frames", alphaCalls));
#ifdef LAMIUM_SCHEMATIC_PERF_TRACE
    auto frameStart = std::chrono::steady_clock::now();
    struct FrameEnd {
        std::chrono::steady_clock::time_point start;
        ~FrameEnd() {
            auto took = static_cast<std::uint64_t>((std::chrono::steady_clock::now() - start).count());
            perf.frameNs += took;
            perf.maxFrameNs = std::max(perf.maxFrameNs, took);
            ++perf.frames;
            perfReport();
        }
    } frameEnd{frameStart};
#endif
    auto& runtime = Runtime::instance();
    if (releaseRequested.exchange(false)) { release(); stopSave(); }
    if (!runtime.enabled() || !runtime.snapshot()->schematic.enabled || !context.mImpl) {
        stopSave();
        if (!sections.empty() || !resolved.empty()) release();
        return;
    }
    IClientInstance& client = context.mClientInstance;
    auto* player = client.getLocalPlayer();
    if (!player) return;
    try {
        // A new graphics mode rebuilds the sections (mistake faces differ).
        // The graphics mode, the ghost tint, the outline width, the mistake
        // marks and the check relaxations are built into the sections: a
        // change rebuilds them.
        auto const& look = runtime.snapshot()->schematic;
        int graphics = static_cast<int>(client.getOptions().getGraphicsMode());
        if (int mode = (((graphics * 101 + look.ghostTint) * 4 + look.ghostLines) * 2 + look.mistakeMarks) * 8 + static_cast<int>(checkRules());
            mode != lastGraphicsMode) {
            if (lastGraphicsMode >= 0) release();
            lastGraphicsMode = mode;
            // The graphics mode alone: the combined key made every mode look
            // like Vibrant Visuals, and mistake faces then hid their outlines.
            buildCamera.vibrant = graphics >= static_cast<int>(GraphicsMode::Advanced);
        }
        drawPlacements(context, client, *player);
        // Faces of the point, the selection and waiting columns: the overlay's
        // face material for the graphics mode (the hologram one is not shown
        // under Vibrant Visuals or in Simple).
        auto faces = overlay::faceMaterial(client);
        drawPoint(context.mScreenContext, context.mImpl->mCameraPosition, faces.material, faces.twoSided);
        stepSave(player->getDimensionBlockSource(), *player);
        drawSelection(context.mScreenContext, context.mImpl->mCameraPosition, static_cast<int>(player->getDimensionId()), faces);
        drawWaitingColumns(context.mScreenContext, context.mImpl->mCameraPosition, faces.material, faces.twoSided);
    } catch (std::exception const& error) {
        static bool reported = false;
        if (!std::exchange(reported, true)) log(std::string("drawing failed: ") + error.what());
    }
}
}

std::optional<Mismatch> mismatchAt(BlockSource& region, Point world) { return checkedCell(region, world, resolved); }
std::optional<GhostAt> ghostAlong(BlockSource& region, int dimension, double x, double y, double z, double dx, double dy,
                                  double dz, double reach, bool throughBlocks) {
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.snapshot()->schematic.enabled) return std::nullopt;
    auto snapshot = session::snapshot();
    for (auto const& cell : cellsAlong(x, y, z, dx, dy, dz, reach)) {
        BlockPos pos{cell.x, cell.y, cell.z};
        if (!region.getChunkAt(pos)) return std::nullopt;
        // A real block hides the ghosts behind it; one standing in its own
        // cell is the card's ordinary schematic line. Real water and lava do
        // not: the game's ray passes through them too (L-264).
        if (Block const& real = region.getBlock(pos); !real.isAir() && !liquidKind(real)) {
            if (throughBlocks) continue;
            return std::nullopt;
        }
        for (size_t i = 0; i < snapshot.placements.size() && i < resolved.size(); ++i) {
            auto const& shown = snapshot.placements[i];
            if (resolved[i].structure != shown.structure.get()) continue;
            auto index = overlap::paletteAt(shown, dimension, cell);
            if (!index || static_cast<size_t>(*index) >= resolved[i].blocks.size()) continue;
            if (auto const* block = resolved[i].blocks[static_cast<size_t>(*index)])
                return GhostAt{cell, block, shown.placement.name};
        }
    }
    return std::nullopt;
}
void start() {
    if (capabilities::registry().status(capabilities::Id::SchematicBlocks).runtime.reason == capabilities::Reason::RemovalFailed
        || capabilities::registry().status(capabilities::Id::SchematicEntities).runtime.reason == capabilities::Reason::RemovalFailed) return;
    if (installed) return;
    capabilities::begin(capabilities::Id::SchematicBlocks);
    capabilities::begin(capabilities::Id::SchematicEntities);
    capabilities::begin(capabilities::Id::SchematicBlend);
    capabilities::begin(capabilities::Id::SchematicActorLight);
    installed = GhostPass::hook(true) == 0;
    if (!installed) {
        capabilities::hooks(capabilities::Id::SchematicBlocks,false);
        capabilities::hooks(capabilities::Id::SchematicEntities,false);
        throw std::runtime_error("Could not install the schematic ghost pass");
    }
    capabilities::hooks(capabilities::Id::SchematicBlend,
        capabilities::attachHook<GhostBlendPass>(blendInstalled,{capabilities::Id::SchematicBlend}));
    capabilities::hooks(capabilities::Id::SchematicActorLight,startActorLight());
    exitListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientExitLevelEvent>(
        [](auto&) { releaseRequested = true; items::forget(); selection::clear(); });
    capabilities::ready(capabilities::Id::SchematicBlocks);
    capabilities::ready(capabilities::Id::SchematicEntities);
}
void stop() {
    capabilities::stop(capabilities::Id::SchematicBlocks);
    capabilities::stop(capabilities::Id::SchematicEntities);
    capabilities::stop(capabilities::Id::SchematicBlend);
    capabilities::stop(capabilities::Id::SchematicActorLight);
    if (exitListener) {
        ll::event::EventBus::getInstance().removeListener(exitListener);
        exitListener.reset();
    }
    capabilities::detachHook<GhostBlendPass>(blendInstalled,{capabilities::Id::SchematicBlend});
    stopActorLight();
    capabilities::detachHook<GhostPass>(installed,{capabilities::Id::SchematicBlocks,capabilities::Id::SchematicEntities});
    releaseRequested = true;
}
void invalidateResources() { releaseRequested = true; }
}

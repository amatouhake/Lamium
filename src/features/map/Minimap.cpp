#include "features/map/Minimap.h"
#include "features/map/MapCave.h"
#include "features/map/MapColors.h"
#include "features/map/MapImage.h"
#include "features/map/MapRadar.h"
#include "features/map/MapRegion.h"
#include "features/map/MapStore.h"
#include "features/map/RadarFaces.h"
#include "features/map/SchematicMarks.h"
#include "features/map/WaypointSession.h"
#include "features/map/Waypoints.h"
#include "features/map/MapTiles.h"
#include "features/map/MapLighting.h"
#include "features/map/MapView.h"
#include "features/information/InfoHud.h"
#include "features/camera/CameraSessions.h"
#include "app/Runtime.h"
#include "ui/Widgets.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/client/renderer/TextureGroup.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/network/packet/AvailableCommandsPacket.h"
#include "mc/network/packet/AvailableCommandsPacketPayload.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/player/PlayerListEntry.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/PlayerLocationReceiver.h"
#include "mc/deps/core/container/Blob.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/image/Image.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core_graphics/ImageBuffer.h"
#include "mc/client/renderer/block/BlockGraphics.h"
#include "mc/client/renderer/texture/TextureUVCoordinateSet.h"
#include "mc/common/FacingID.h"
#include "mc/deps/core_graphics/ImageDescription.h"
#include "mc/deps/core_graphics/enums/TextureFormat.h"
#include "mc/world/level/biome/biome_color_sampling/BiomeColorSampling.h"
#include "mc/client/world/level/biome/biome_color_sampling/TessellationPolicy.h"
#include "mc/deps/core/math/Color.h"
#include "mc/world/level/block/TintMethod.h"
#include "mc/world/level/material/Material.h"
#include "mc/world/phys/AABB.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/BrightnessPair.h"
#include "mc/world/level/biome/Biome.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/Level.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorType.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <format>
#include <string>
#include <unordered_map>

namespace lamium::map {
namespace {
// Texture side: enough for the usual size, finer while enlarged.
constexpr int normalPixels = 256, enlargedPixels = 512;
std::atomic<bool> enlargeHeld{false};
std::atomic<bool> facesHeld{false}; // The radar's hold key flips faces and dots.
// Per-frame scan budget; one chunk is the smallest step, so a frame may run
// over by one chunk's scan.
constexpr double scanBudgetSeconds = .0015, recordBudgetSeconds = .001;
// Recorded around the player: about what the client keeps loaded.
constexpr int recordBlocks = 384;
// Kept around the player beyond the widest view (enlarged, turning) so
// zooming back is instant and an enlarged map does not evict what it shows.
int const keepChunks = chunkRadius(zoomSteps.back() * 2, true) + 4;

std::atomic<unsigned> worldGeneration{0};
// How far the frame is between two ticks, as the world was drawn. The map
// center and the radar dots both use positions interpolated by it; mixing
// ticked mob positions with a smoothly moving center made dots wobble.
std::atomic<float> frameAlpha{1.f};
LL_TYPE_INSTANCE_HOOK(FrameAlphaHook, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$renderEntityEffects, void, BaseActorRenderContext& context) {
    origin(context);
    try {
        if (auto* player = context.mClientInstance.getLocalPlayer()) {
            float alpha = context.getFrameAlpha(*player);
            if (std::isfinite(alpha)) frameAlpha = std::clamp(alpha, 0.f, 1.f);
            // The death screen hides the HUD, so the death is noticed here.
            waypoints::watchDeath(context.mClientInstance);
        }
    } catch (...) {}
}
bool hooked = false;
// The commands the server lets this player run arrive in one list; /tp in it
// means teleport really works, also when LeviLamina forces commands on in a
// world without cheats. -1 unknown, reset per world.
std::atomic_int tpListed{-1};
LL_TYPE_INSTANCE_HOOK(CommandListHook, ll::memory::HookPriority::Normal, ClientNetworkHandler,
    &ClientNetworkHandler::$handle, void, NetworkIdentifier const& source, AvailableCommandsPacket const& packet) {
    try {
        bool listed = false;
        for (auto const& command : packet.mCommands.get())
            if (command.name.get() == "tp" || command.name.get() == "teleport") listed = true;
        tpListed = listed;
    } catch (...) {}
    origin(source, packet);
}
bool commandsHooked = false;
// An actor's feet, interpolated for this frame.
Vec3 drawnFeet(Actor const& actor) {
    auto feet = actor.getFeetPos(), position = actor.getPosition();
    auto drawn = actor.getInterpolatedPosition(frameAlpha.load());
    return {drawn.x, drawn.y - (position.y - feet.y), drawn.z};
}
ll::event::ListenerPtr exitListener, joinListener;

ResourceLocation const& textureLocation() {
    // Never destroyed: its destructor is game code, which must not run while
    // the process tears down after the game.
    static auto const* location = new ResourceLocation(Core::PathView("lamium/minimap"), ResourceFileSystem::Raw);
    return *location;
}
double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

struct Diagnostics {
    int lines = 0;
    double windowStart = 0;
    int composes = 0, uploads = 0, chunks = 0;
    double composeSeconds = 0, uploadSeconds = 0, scanSeconds = 0;
    void log(std::string const& text) {
        // Bounded: the log is for the first runs of an experimental feature.
        if (lines >= 40) return;
        ++lines;
        try { Runtime::instance().self().getLogger().info("Minimap: {}", text); } catch (...) {}
    }
    void report(double time, size_t tiles) {
        if (windowStart == 0) { windowStart = time; return; }
        if (time - windowStart < 30) return;
        log(std::format("{} composes avg {:.2f} ms, {} uploads avg {:.2f} ms, {} chunks scanned avg {:.3f} ms, {} tiles kept",
            composes, composes ? composeSeconds * 1000 / composes : 0., uploads, uploads ? uploadSeconds * 1000 / uploads : 0.,
            chunks, chunks ? scanSeconds * 1000 / chunks : 0., tiles));
        *this = Diagnostics{lines, time};
    }
};

struct State {
    TileCache surface, cave; // Each view keeps its own data, so switching is instant.
    unsigned generation = ~0u, storeEpoch = ~0u;
    int dimension = -1;
    unsigned revision = 0;
    ViewSwitch automatic;
    std::optional<int> layer; // Cave view height; see stableLayer.
    std::vector<std::uint32_t> terrain, image;
    struct Key {
        double x = NAN, z = NAN, yaw = NAN;
        int zoom = -1, pixels = 0;
        bool round = false, rotate = false, cave = false;
        unsigned revision = ~0u;
        bool operator==(Key const&) const = default;
    } composed;
    // What the uploaded image shows on top of the terrain.
    struct Overlay {
        unsigned composes = ~0u;
        double angle = NAN, x = NAN, y = NAN;
        bool visible = false;
        std::vector<PlacedDot> dots;
        struct Mark {
            double x, y;
            int color; // -1: the death point.
            bool operator==(Mark const&) const = default;
        };
        std::vector<Mark> marks;
        struct Outline {
            std::array<Point, 4> corners;
            bool selected;
            bool operator==(Outline const&) const = default;
        };
        std::vector<Outline> outlines;
        bool operator==(Overlay const&) const = default;
    } shown;
    unsigned composes = 0;
    double evictedAt = 0;
    bool uploaded = false, failed = false, everUploaded = false;
    int textureSize = 0;
    int texturesLogged = 0, tintsLogged = 0, fallbacksLogged = 0;
    Diagnostics diagnostics;
};
State state;
// The key's forced view and the view shown last; the key runs from the
// input queue, the map from the HUD draw.
std::atomic<ViewForce> viewForce{ViewForce::Auto};
std::atomic<ViewMode> shownView{ViewMode::Surface};
// Fill for ground not loaded yet: the card color, half transparent.
constexpr std::uint32_t unknownFill = packColor(16, 17, 19, 150);

void releaseTexture(IClientInstance& client) {
    if (!state.uploaded) return;
    state.uploaded = false;
    try {
        if (auto group = client.getTextureGroup()) group->unloadTexture(textureLocation(), false);
    } catch (...) {}
}
void forget() {
    state.surface.clear();
    state.cave.clear();
    state.automatic = {};
    state.layer.reset();
    viewForce = ViewForce::Auto;
    state.terrain.clear();
    state.image.clear();
    state.composed = {};
    state.shown = {};
    ++state.revision;
}
// How a block shows on the map, by block state. Texture averages are cached
// by path, since many states share a texture.
struct BlockLook {
    std::uint32_t color = 0; // Untinted top texture average; 0: use the map color.
    TintMethod tint = TintMethod::None;
    bool skip = false;    // Not drawn; the map looks through it.
    bool pending = false; // The client's stand-in for blocks not received yet.
    bool cover = false; // Thin but covers its column (snow layers, carpets).
};
std::unordered_map<std::uint64_t, BlockLook> blockLooks;
std::unordered_map<std::string, std::uint32_t> textureColors;
// Starts over in another world or dimension, and after the saved map was
// attached or cleared, so every chunk is scanned (and recorded) again.
void follow(int dimension) {
    unsigned generation = worldGeneration.load(), epoch = store::epoch();
    if (generation == state.generation && dimension == state.dimension && epoch == state.storeEpoch) return;
    forget();
    // A new world may bring other resource packs.
    if (generation != state.generation) {
        blockLooks.clear();
        textureColors.clear();
        faces::forget(nullptr);
    }
    state.generation = generation;
    state.dimension = dimension;
    state.storeEpoch = epoch;
}

// Texture images loaded per frame; the first view of a new area loads many.
constexpr int textureLoadsPerFrame = 6;
int textureLoadsLeft = 0;

std::uint32_t textureAverage(IClientInstance& client, ResourceLocation const& location) {
    auto group = client.getTextureGroup();
    if (!group) return 0;
    auto* image = group->getCachedImageOrLoadSync(location, false);
    if (!image) return 0;
    auto const& description = *image->mImageDescription;
    auto format = description.mTextureFormat;
    size_t count = size_t(description.mWidth) * description.mHeight;
    auto const& storage = *image->mStorage;
    bool usable = (format == mce::TextureFormat::R8g8b8a8Unorm || format == mce::TextureFormat::R8g8b8a8UnormSrgb)
        && count && storage.size() >= count * 4;
    auto color = usable ? averageColor(storage.data(), count).value_or(0) : 0;
    // The first loads show whether texture images arrive as expected.
    if (++state.texturesLogged <= 8)
        state.diagnostics.log(std::format("texture {} format {} {}x{} -> {:08x}", location.mPath->value,
            static_cast<unsigned>(format), description.mWidth, description.mHeight, color));
    return color;
}
// Null while the frame's texture loads are used up; the chunk waits.
BlockLook const* blockLook(IClientInstance& client, Block const& block) {
    std::uint64_t key = block.mSerializationIdHash;
    if (auto found = blockLooks.find(key); found != blockLooks.end()) return &found->second;
    BlockLook look;
    auto const& type = block.getBlockType();
    auto material = type.mMaterial.mType;
    using Material = SharedTypes::v1_26_20::MaterialType;
    look.tint = type.mTintMethod;
    if (material == Material::Air || material == Material::Glass || material == Material::StructureVoid
        || material == Material::Barrier || material == Material::Portal) look.skip = true;
    else if (material == Material::ClientRequestPlaceholder) look.pending = true;
    else if (auto const* graphics = BlockGraphics::getForBlock(block)) {
        auto const& shape = *graphics->mVisualShape;
        look.cover = material != Material::Plant && shape.max.x - shape.min.x > .99f && shape.max.z - shape.min.z > .99f;
        auto const& uv = graphics->getTexture(static_cast<std::uint64_t>(FacingID::Up), type.getVariant(block));
        auto const& path = uv.sourceFileLocation->mPath->value;
        if (auto cached = textureColors.find(path); cached != textureColors.end()) look.color = cached->second;
        else {
            if (textureLoadsLeft <= 0) return nullptr;
            --textureLoadsLeft;
            look.color = textureColors[path] = textureAverage(client, *uv.sourceFileLocation);
        }
    }
    // Which blocks fall back or stand in, for the first runs of the feature.
    if ((look.pending || (!look.skip && !look.color)) && ++state.fallbacksLogged <= 8)
        state.diagnostics.log(std::format("{} {}", look.pending ? "stand-in block" : "no texture color for",
                                          block.getTypeName()));
    return &blockLooks.emplace(key, look).first->second;
}
// The tint the world's block renderer applies, so biome-specific foliage
// (swamps) matches the world. The getMap* samplers are the cartography map's
// colors and missed it (2026-10-07).
// Set when the renderer's tint was not ready; the chunk is scanned again soon.
bool tintNotReady = false;
// The last surface column used the stand-in map tint.
bool columnProvisional = false;
std::uint32_t biomeTint(TintMethod tint, BlockSource& region, BlockPos const& pos, Block const& block,
                        std::uint32_t color) {
    if (tint == TintMethod::None || tint == TintMethod::RedStoneWire || tint >= TintMethod::Size) return color;
    auto value = BiomeColorSampling::getTessellationPolicy(tint).get(block, region, pos, nullptr);
    if (usableTint(value.r, value.g, value.b)) return tinted(color, value.r, value.g, value.b);
    // Not ready yet: the cartography map's tint for now (it misses swamp
    // foliage but is never black), and the chunk is scanned again.
    tintNotReady = true;
    auto const& biome = region.getBiome(pos);
    int sample;
    switch (tint) {
    case TintMethod::Grass: sample = BiomeColorSampling::getMapGrassColor(biome, pos); break;
    case TintMethod::DefaultFoliage: sample = BiomeColorSampling::getMapDefaultFoliageColor(biome, pos); break;
    case TintMethod::BirchFoliage: sample = BiomeColorSampling::getMapBirchFoliageColor(biome, pos); break;
    case TintMethod::EvergreenFoliage: sample = BiomeColorSampling::getMapEvergreenFoliageColor(biome, pos); break;
    case TintMethod::DryFoliage: sample = BiomeColorSampling::getMapDryFoliageColor(biome, pos); break;
    case TintMethod::Water: sample = BiomeColorSampling::getWaterColor(biome, pos); break;
    default: return color;
    }
    if (++state.tintsLogged <= 4)
        state.diagnostics.log(std::format("renderer tint {} not ready at {} {} {}; map tint used", static_cast<int>(tint),
                                          pos.x, pos.y, pos.z));
    auto part = [&](int shift) { return ((sample >> shift) & 0xFF) / 255.f; };
    return tinted(color, part(16), part(8), part(0));
}
std::uint32_t mapColor(BlockSource& region, BlockPos const& pos, Block const& block) {
    auto color = block.getBlockType().getMapColor(region, pos, block);
    if (!(color.a > 0)) return 0;
    auto byte = [](float v) { return static_cast<int>(std::lround(std::clamp(v, 0.f, 1.f) * 255)); };
    return packColor(byte(color.r), byte(color.g), byte(color.b));
}
enum class Scan { Done, Waiting };
// Set by the column scans when a block had not arrived; the column stays unknown.
bool sawPending = false;
std::uint32_t blockColor(BlockLook const& look, BlockSource& region, BlockPos const& pos, Block const& block) {
    return look.color ? biomeTint(look.tint, region, pos, block, look.color) : mapColor(region, pos, block);
}
// One surface column. The heightmap gives the top light-blocking block; a
// covering block just above it (snow layer, carpet) wins, blocks that are
// not drawn (glass) are looked through. Colors are what the world shows: the
// top texture's average times the biome tint.
std::optional<Column> surfaceColumn(IClientInstance& client, BlockSource& region, int x, int z, short minY,
                                    bool voidFloor) {
    auto top = region.getHeightmapPos(BlockPos{x, 0, z});
    int y = top.y;
    for (int steps = 0; y >= minY && steps < 16; --y, ++steps) {
        BlockPos pos{x, y, z};
        auto const& block = region.getBlock(pos);
        auto const* look = blockLook(client, block);
        if (!look) return std::nullopt;
        if (look->pending) { sawPending = true; return Column{}; }
        if (look->skip || (y == top.y && !look->cover)) continue;
        tintNotReady = false;
        if (auto color = blockColor(*look, region, pos, block)) {
            if (tintNotReady) sawPending = columnProvisional = true;
            return Column{color, static_cast<std::int16_t>(y)};
        }
    }
    // Nothing to stand on: the End's void is known and as dark as a drop.
    // Elsewhere a section not received yet reads as air, not as a stand-in;
    // recording it as known saved black over explored land (2026-10-07).
    if (voidFloor) return Column{caveDeep, minY};
    sawPending = true;
    return Column{};
}
// One cave column around the player's height `layer`.
std::optional<Column> caveColumnAt(IClientInstance& client, BlockSource& region, int x, int z, int layer, short minY,
                                   short maxY) {
    std::array<bool, caveAbove + caveBelow + 1> solid{};
    int top = layer + caveAbove;
    for (size_t i = 0; i < solid.size(); ++i) {
        int y = top - static_cast<int>(i);
        if (y < minY || y >= maxY) continue;
        auto const* look = blockLook(client, region.getBlock(BlockPos{x, y, z}));
        if (!look) return std::nullopt;
        // Stand-ins the client never asked for are mostly hidden rock: draw
        // them as rock without a color rather than leaving holes.
        solid[i] = look->pending || (!look->skip && look->cover);
    }
    auto hit = caveFloor(solid, top, layer);
    std::uint32_t color = 0;
    if (hit.kind == CaveHit::Kind::Floor) {
        BlockPos pos{x, hit.y, z};
        auto const& block = region.getBlock(pos);
        if (auto const* look = blockLook(client, block); look && !look->pending)
            color = blockColor(*look, region, pos, block);
    }
    return caveColumn(hit, layer, color);
}
// Where a scanned cache meets the saved world map: chunks it scans are
// recorded there, and chunks the client has not loaded are filled from it.
struct Saved {
    bool on = false;
    MapLayer layer;
};
Scan scanChunk(IClientInstance& client, BlockSource& region, TileCache& cache, ChunkKey key, bool cave, int layer,
               double time, Saved saved) {
    std::array<Column, 256> columns{};
    bool loaded = false, stored = false;
    sawPending = false;
    short minY = region.getMinHeight(), maxY = region.getMaxHeight();
    BlockPos origin{key.x * 16, std::max<int>(minY, std::min<int>(layer, maxY - 1)), key.z * 16};
    std::array<bool, 256> provisional{};
    if (region.getChunkAt(origin)) {
        for (int dz = 0; dz < 16; ++dz)
            for (int dx = 0; dx < 16; ++dx) {
                int x = origin.x + dx, z = origin.z + dz;
                columnProvisional = false;
                auto column = cave ? caveColumnAt(client, region, x, z, layer, minY, maxY)
                                   : surfaceColumn(client, region, x, z, minY, state.dimension == 2);
                if (!column) return Scan::Waiting;
                columns[static_cast<size_t>(columnIndex(x, z))] = *column;
                provisional[static_cast<size_t>(columnIndex(x, z))] = columnProvisional;
                // Blocks not received yet are the client's stand-ins, so
                // every other column is known, empty or not.
                loaded = loaded || column->color;
            }
        // Saved colors stand in for blocks not received yet and for the
        // stand-in tint, like outside the render distance (maintainer,
        // 2026-10-07); the stand-in tint shows only where nothing is saved.
        std::array<Column, 256> known{};
        if (sawPending && saved.on && store::cached(saved.layer, key, known)) {
            fillFromSaved(columns, provisional, known);
            loaded = true;
        }
    } else if (saved.on) {
        stored = loaded = store::cached(saved.layer, key, columns);
    }
    auto* existing = cache.find(key);
    bool same = existing && existing->loaded == loaded && existing->layer == layer && existing->partial == sawPending
        && existing->stored == stored && std::equal(columns.begin(), columns.end(), existing->columns.begin(),
                      [](Column const& a, Column const& b) { return a.color == b.color && a.height == b.height; });
    auto& tile = cache.put(key);
    tile.scannedAt = time;
    if (same) return Scan::Done;
    tile.columns = columns;
    tile.loaded = loaded;
    tile.layer = layer;
    tile.partial = sawPending;
    tile.stored = stored;
    cache.changed(key);
    if (saved.on && loaded && !stored) store::record(saved.layer, key, columns);
    ++state.revision;
    return Scan::Done;
}
void scan(IClientInstance& client, LocalPlayer& player, TileCache& cache, double centerX, double centerZ, bool cave,
          int layer, double blocks, bool rotate, double time, Saved saved, double budget = scanBudgetSeconds) {
    auto& region = player.getDimensionBlockSource();
    auto center = chunkOf(blockFloor(centerX), blockFloor(centerZ));
    auto start = now();
    textureLoadsLeft = textureLoadsPerFrame;
    if (!cave) layer = 0;
    for (auto key : scanOrder(cache, center, chunkRadius(static_cast<int>(blocks), rotate), time, 64, layer, cave ? 0 : 1 << 20)) {
        if (scanChunk(client, region, cache, key, cave, layer, time, saved) == Scan::Waiting) break;
        ++state.diagnostics.chunks;
        if (now() - start >= budget) break;
    }
    state.diagnostics.scanSeconds += now() - start;
    if (time - state.evictedAt > 1) {
        state.surface.evict(center, keepChunks);
        state.cave.evict(center, keepChunks);
        state.evictedAt = time;
    }
}

bool upload(IClientInstance& client, int pixels) {
    // A new size needs a new texture.
    if (state.uploaded && state.textureSize != pixels) releaseTexture(client);
    auto group = client.getTextureGroup();
    if (!group) return false;
    auto start = now();
    state.textureSize = pixels;
    mce::Image image(pixels, pixels, mce::ImageFormat::RGBA8Unorm, mce::ImageUsage::SRGB);
    image.mAlphaUsage = mce::AlphaUsage::Transparent;
    image.setRawImage(mce::Blob(reinterpret_cast<std::uint8_t const*>(state.image.data()),
                                state.image.size() * sizeof(std::uint32_t)));
    cg::ImageBuffer buffer(std::move(image));
    if (state.uploaded) {
        if (!group->updateTextureInPlace(textureLocation(), std::move(buffer))) {
            state.uploaded = false;
            state.diagnostics.log("in-place update refused; uploading again");
            return false;
        }
    } else {
        group->uploadTexture(textureLocation(), std::move(buffer));
        state.uploaded = true;
        if (!state.everUploaded) state.diagnostics.log(std::format("texture uploaded in {:.2f} ms", (now() - start) * 1000));
        state.everUploaded = true;
    }
    state.diagnostics.uploadSeconds += now() - start;
    ++state.diagnostics.uploads;
    return true;
}

// Owned per-frame values; nothing from the game outlives the call. The map
// follows the FreeCamera camera while it flies, otherwise the player.
struct Snapshot {
    double x, y, z; // Followed point, at foot height.
    float yaw;
    double playerX, playerY, playerZ;
    float playerYaw;
    int dimension;
    bool covered = false;
    int skyLight = 15;
    std::string biome;
    int worldTime = -1;
};
std::optional<Snapshot> snapshot(IClientInstance& client, bool biome) {
    auto* player = client.getLocalPlayer();
    if (!player) return std::nullopt;
    auto p = drawnFeet(*player);
    auto rotation = player->getRotation();
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return std::nullopt;
    float yaw = std::isfinite(rotation.z) ? rotation.z : 0.f;
    Snapshot value{p.x, p.y, p.z, yaw, p.x, p.y, p.z, yaw, static_cast<int>(player->getDimensionId())};
    value.worldTime = player->getLevel().getTime();
    auto& sessions = CameraSessions::instance();
    if (sessions.blocksPerspective())
        if (auto ray = sessions.detachedViewRay(client)) {
            auto eye = player->getEyePos();
            double feet = player->getFeetPos().y - eye.y; // The camera's eye stands where a player's would.
            if (std::isfinite(ray->x) && std::isfinite(ray->y) && std::isfinite(ray->z)) {
                value.x = ray->x;
                value.y = ray->y + feet;
                value.z = ray->z;
                if (ray->dx != 0 || ray->dz != 0)
                    value.yaw = static_cast<float>(std::atan2(-ray->dx, ray->dz) * 180 / 3.14159265358979323846);
            }
        }
    auto& region = player->getDimensionBlockSource();
    BlockPos feetPos{blockFloor(value.x), blockFloor(value.y), blockFloor(value.z)};
    if (region.getChunkAt(feetPos)) {
        BlockPos head{feetPos.x, feetPos.y + 1, feetPos.z};
        int roofs = 0;
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (region.getHeightmapPos(BlockPos{feetPos.x + dx, 0, feetPos.z + dz}).y > head.y + 1) ++roofs;
        value.covered = coveredByMost(roofs);
        if (head.y >= region.getMinHeight() && head.y < region.getMaxHeight())
            value.skyLight = region.getBrightnessPair(head).sky->mValue;
        if (biome) value.biome = information::biomeName(region.getBiome(feetPos).mHash->getString());
    }
    return value;
}
}

// L-89: players without a loaded Actor, at the last position vanilla's
// Locator Bar state received (sent while they move; a HIDE, sneaking or
// another dimension, empties it). A loaded player always comes from its
// Actor above (the same actor list, so the two never overlap or leave a
// gap); an id no longer in the player list is gone.
void distantPlayers(LocalPlayer& self, std::vector<ActorUniqueID> const& loaded, std::vector<Dot>& dots, double centerX,
                    double centerZ, double reach, double playerY, bool withFaces) {
    auto& level = self.getLevel();
    auto receiver = level.getPlayerLocationReceiver();
    if (!receiver) return;
    auto const selfId = self.getOrCreateUniqueID();
    auto const& list = level.getPlayerList();
    for (auto const& entry : *receiver->mCurrentPlayerLocationData) {
        ActorUniqueID const& id = entry.first;
        std::optional<Vec3> const& at = entry.second;
        if (!at || id == selfId || std::find(loaded.begin(), loaded.end(), id) != loaded.end()) continue;
        if (!std::isfinite(at->x) || std::abs(at->x - centerX) > reach || std::abs(at->z - centerZ) > reach) continue;
        auto listed = std::find_if(list.begin(), list.end(), [&](auto const& player) { return *player.second.mId == id; });
        if (listed == list.end()) continue;
        int face = withFaces ? faces::headOf(*listed->second.mSkin) : -1;
        // The position is the eye, as Actor::getPosition gives it.
        double feet = at->y - 1.62;
        dots.push_back({DotKind::Player, at->x, at->z, feet - playerY, *listed->second.mName, face, true});
    }
}
void setEnlarged(bool held) { enlargeHeld = held; }
void setFacesHeld(bool held) { facesHeld = held; }
// Owned dots for this frame from the client's actors near the map center.
std::vector<Dot> collectDots(IClientInstance& client, double centerX, double centerZ, double reach, double playerY,
                             bool invisible, bool withFaces) {
    std::vector<Dot> dots;
    auto* player = client.getLocalPlayer();
    if (!player) return dots;
    auto const* dimension = &player->getDimension();
    std::vector<ActorUniqueID> loadedPlayers;
    for (auto* actor : player->getLevel().getRuntimeActorList()) {
        if (actor && actor != player && actor->hasType(ActorType::Player)) loadedPlayers.push_back(actor->getOrCreateUniqueID());
        if (!actor || actor == player || &actor->getDimension() != dimension) continue;
        if (!actor->isAlive() || (!invisible && actor->isInvisible())) continue;
        auto kind = classify(actor->hasType(ActorType::Player), actor->hasType(ActorType::ItemEntity),
                             actor->hasType(ActorType::Monster), actor->hasType(ActorType::Mob));
        if (!kind) continue;
        auto p = drawnFeet(*actor);
        if (!std::isfinite(p.x) || std::abs(p.x - centerX) > reach || std::abs(p.z - centerZ) > reach) continue;
        int face = -1;
        if (withFaces && *kind == DotKind::Player) face = faces::headOf(*actor);
        else if (withFaces && *kind != DotKind::Item) face = faces::faceOf(client, *actor);
        dots.push_back({*kind, p.x, p.z, p.y - playerY, *kind == DotKind::Player ? actor->getNameTag() : std::string{}, face});
    }
    distantPlayers(*player, loadedPlayers, dots, centerX, centerZ, reach, playerY, withFaces);
    return dots;
}
ViewForce pressViewKey() {
    auto next = pressForce(viewForce.load(), shownView.load());
    viewForce = next;
    return next;
}

std::optional<ui::hud_editor::Box> drawMinimap(MinecraftUIRenderContext& context, float width, float height,
                                               ui::HudElement const& element, Settings::Map const& settings,
                                               bool preview, float cardOpacity) {
    auto& client = context.mClient;
    if (!preview && !settings.minimap) {
        releaseTexture(client);
        // The world map's recording keeps using the scanned chunks.
        if (!settings.worldMap && (state.surface.size() || state.cave.size())) forget();
        return std::nullopt;
    }
    if (state.failed) return std::nullopt;
    auto view = snapshot(client, settings.biome);
    if (!view) return std::nullopt;
    try {
        auto time = now();
        follow(view->dimension);
        int zoom = clampZoomIndex(settings.zoom);
        auto wanted = chooseView(state.automatic.mode, view->dimension == 1, view->covered, view->skyLight);
        auto mode = applyForce(viewForce.load(), state.automatic.update(wanted, time));
        shownView = mode;
        bool cave = mode == ViewMode::Cave;
        auto& cache = cave ? state.cave : state.surface;
        state.layer = stableLayer(state.layer.value_or(0), blockFloor(view->y), !state.layer);
        int layer = *state.layer;
        // Held enlarge: twice the side and twice the blocks, the same scale.
        bool enlarged = enlargeHeld.load() && !preview;
        int pixels = enlarged ? enlargedPixels : normalPixels;
        double blocks = blocksAcross(zoom) * (enlarged ? 2 : 1), perPixel = blocks / pixels;
        float zoomScale = std::clamp(std::isfinite(element.scale) ? element.scale : 100.f, 75.f, 150.f) / 100;
        float size = std::round(height * std::clamp(settings.size, 10.f, 40.f) / 100 * zoomScale);
        // Markers keep their on-screen size when the map is enlarged.
        float baseSize = size;
        if (enlarged) size = std::min(size * 2, std::round(height * .85f));
        double marker = pixels / 216.0 * baseSize / size; // A mockup pixel, in texture pixels.
        Saved saved{settings.worldMap && cave == (view->dimension == 1), mapLayer(view->dimension, layer)};
        if (auto* player = client.getLocalPlayer())
            scan(client, *player, cache, view->x, view->z, cave, layer, blocks, settings.rotate, time, saved);

        auto transform = settings.rotate ? ViewTransform::headingUp(view->yaw) : ViewTransform::northUp();
        auto snapped = snapCenter(transform, view->x, view->z, perPixel);
        double centerX = snapped.x, centerZ = snapped.z;
        State::Key key{centerX, centerZ, settings.rotate ? view->yaw : 0.f, zoom * 2 + enlarged, pixels, settings.round,
                       settings.rotate, cave, state.revision};
        auto const& last = state.composed;
        bool moved = !(std::abs(key.x - last.x) < perPixel / 4 && std::abs(key.z - last.z) < perPixel / 4)
            || (key.rotate && !(std::abs(key.yaw - last.yaw) < .25));
        bool changed = key.zoom != last.zoom || key.pixels != last.pixels || key.round != last.round
            || key.rotate != last.rotate || key.cave != last.cave || key.revision != last.revision;
        // Composing reads shaded colors only, so it can follow every frame.
        if (changed || moved) {
            auto start = now();
            Frame frame{pixels, centerX, centerZ, blocks, transform, settings.round, unknownFill};
            composeTerrain(cache, frame, state.terrain);
            state.diagnostics.composeSeconds += now() - start;
            ++state.diagnostics.composes;
            ++state.composes;
            state.composed = key;
        }
        // The player's arrow: at the center, or where the player is while the
        // map follows a flying camera.
        // Relative to the unsnapped center: the arrow stays in the middle
        // instead of creeping across a pixel and jumping back.
        auto at = worldToPixel(transform, view->x, view->z, view->playerX, view->playerZ, blocks, pixels, 4);
        auto facing = heading(view->playerYaw);
        auto onMap = transform.toMap(facing.x, facing.z);
        State::Overlay overlay{state.composes, std::atan2(onMap.x, -onMap.z), at.x, at.y, at.inside};
        if (settings.radar) {
            RadarSwitches switches{settings.radarPlayers, settings.radarHostile, settings.radarPassive, settings.radarItems};
            faces::frame();
            bool withFaces = settings.radarFaces != facesHeld.load();
            overlay.dots = placeDots(collectDots(client, centerX, centerZ, blocks * .75, view->playerY, settings.radarInvisible,
                                                 withFaces),
                                     switches, transform,
                                     centerX, centerZ, blocks, pixels, settings.round,
                                     // A face is wider than a dot: keep it off the frame.
                                     withFaces ? 6 * marker * dotScale(blocksAcross(zoom)) : 3);
        }
        if (settings.waypoints && settings.waypointsMinimap) {
            auto set = waypoints::current();
            double margin = 6 * marker;
            auto place = [&](double x, double z, int color) {
                auto m = mapMarker(transform, centerX, centerZ, x, z, blocks, pixels, settings.round, margin);
                overlay.marks.push_back({std::round(m.x), std::round(m.y), color});
            };
            for (auto const& w : set.waypoints) {
                if (!w.visible) continue;
                if (auto at = shownPosition(w.x, w.y, w.z, w.dimension, view->dimension, settings.waypointsCrossScale))
                    place(at->x, at->z, w.color);
            }
            if (set.death && set.death->dimension == view->dimension) place(set.death->x + .5, set.death->z + .5, -1);
        }
        for (auto const& mark : placementMarks(view->dimension)) {
            if (!mark.visible) continue;
            auto const& a = mark.area;
            State::Overlay::Outline outline{{}, mark.selected};
            std::array<std::array<int, 2>, 4> world{{{a.x0, a.z0}, {a.x1, a.z0}, {a.x1, a.z1}, {a.x0, a.z1}}};
            for (size_t i = 0; i < 4; ++i) {
                auto p = worldToPixel(transform, centerX, centerZ, world[i][0], world[i][1], blocks, pixels);
                // Quarter pixels: a still map is not redrawn for rounding noise.
                outline.corners[i] = {std::round(p.x * 4) / 4, std::round(p.y * 4) / 4};
            }
            overlay.outlines.push_back(outline);
        }
        auto const& shown = state.shown;
        bool redraw = !state.uploaded || overlay.composes != shown.composes || overlay.visible != shown.visible
            || !(std::abs(overlay.angle - shown.angle) < .004) || !(std::abs(overlay.x - shown.x) < .25)
            || !(std::abs(overlay.y - shown.y) < .25) || overlay.dots != shown.dots || overlay.marks != shown.marks
            || overlay.outlines != shown.outlines;
        if (redraw && !state.terrain.empty()) {
            state.image = state.terrain;
            // The look agreed in docs/demos/minimap.html, smaller on wide maps.
            double unit = marker * dotScale(blocksAcross(zoom));
            for (auto const& outline : overlay.outlines)
                drawOutline(state.image, pixels, outline.corners, outline.selected ? packColor(255, 255, 255) : placementColor,
                            std::max(1.0, 1.5 * marker), settings.round);
            // Faces are drawn over the map on screen pixels, below.
            for (auto const& dot : overlay.dots)
                if (dot.face < 0)
                    drawDot(state.image, pixels, dot.px + .5, dot.py + .5, 3 * unit, 2 * unit, dotColor(dot.kind), dot.alpha);
            for (auto const& mark : overlay.marks) {
                if (mark.color < 0) drawCross(state.image, pixels, mark.x + .5, mark.y + .5, 11 * marker);
                else drawDiamond(state.image, pixels, mark.x + .5, mark.y + .5, 12 * marker,
                                 waypointColors[static_cast<size_t>(clampColor(mark.color))]);
            }
            if (overlay.visible) drawArrow(state.image, pixels, overlay.x, overlay.y, overlay.angle, 16 * marker);
            drawFrame(state.image, pixels, settings.round, pixels / std::max(16.f, size));
            if (upload(client, pixels)) state.shown = overlay;
        }
        state.diagnostics.report(time, state.surface.size() + state.cave.size());

        // Layout: the map, compass letters straddling its frame, then the
        // optional lines centered under it.
        std::vector<std::string> lines;
        if (settings.coordinates)
            lines.push_back(std::format("{}, {}, {}", blockFloor(view->x), blockFloor(view->y), blockFloor(view->z)));
        if (settings.biome && !view->biome.empty()) lines.push_back(view->biome);
        float textScale = zoomScale, lineHeight = 10 * textScale;
        std::vector<float> lineWidths;
        float linesWidth = 0;
        for (auto const& line : lines) {
            lineWidths.push_back(ui::textWidthScaled(context, line, textScale));
            linesWidth = std::max(linesWidth, lineWidths.back());
        }
        float margin = settings.compass ? 5 * textScale : 0; // Half a letter outside the frame.
        bool card = element.background == ui::ElementBackground::Card;
        float pad = card ? 3 : 0;
        float contentW = std::max(size + 2 * margin, linesWidth + 2);
        float contentH = size + 2 * margin + (lines.empty() ? 0 : 2 + lines.size() * lineHeight);
        float boxW = contentW + 2 * pad, boxH = contentH + 2 * pad;
        auto placement = ui::placeElement(width, height, boxW, boxH, element);
        if (card) ui::card(context, placement.x, placement.y, boxW, boxH, cardOpacity);
        float mapX = placement.x + pad + (contentW - size) / 2, mapY = placement.y + pad + margin;
        auto mapTint = daylightTint(view->worldTime, view->dimension, cave, settings.daylightTint);
        if (state.uploaded && !ui::runtimeImage(context, textureLocation(), {mapX, mapY, size, size}, 1.f, mapTint)) {
            // Resource reloads drop runtime textures; upload again next frame.
            state.uploaded = false;
            state.diagnostics.log("texture missing at draw; uploading again");
        }
        if (settings.compass) {
            auto points = compassPoints(transform, pixels, 0, settings.round);
            constexpr std::array<std::string_view, 4> letters{"N", "E", "S", "W"};
            for (size_t i = 0; i < 4; ++i) {
                float cx = mapX + static_cast<float>(points[i].x) * size / pixels;
                float cy = mapY + static_cast<float>(points[i].y) * size / pixels;
                ui::labelScaled(context, cx - 10, cy - 4.5f * textScale, 20, std::string(letters[i]), textScale,
                                ui::palette::text, ui::Align::Center, true);
            }
        }
        // Mob faces and player heads (docs/demos/radar-icons.html, B): about 8 mockup pixels,
        // shrinking with range like the dots. A face that cannot be drawn
        // this frame is skipped; its dot returns once faces are off.
        for (auto const& dot : state.shown.dots) {
            if (dot.face < 0) continue;
            float fx = mapX + static_cast<float>(dot.px + .5) * size / pixels, fy = mapY + static_cast<float>(dot.py + .5) * size / pixels;
            faces::draw(context, dot.face, fx, fy, static_cast<float>(8 * dotScale(blocksAcross(zoom))) * baseSize / 216, dot.alpha);
        }
        // Player names beside their dots, inside the map where they fit.
        for (auto const& dot : state.shown.dots) {
            if (dot.name.empty()) continue;
            float scale = textScale * .75f;
            float w = ui::textWidthScaled(context, dot.name, scale);
            float dx = mapX + static_cast<float>(dot.px + .5) * size / pixels, dy = mapY + static_cast<float>(dot.py + .5) * size / pixels;
            float gap = static_cast<float>(6 * dotScale(blocksAcross(zoom))) * baseSize / 216;
            float x = dx + gap + w <= mapX + size ? dx + gap : dx - gap - w;
            ui::labelScaled(context, x, dy - 4 * scale, w + 2, dot.name, scale, dot.distant ? ui::palette::dim : ui::palette::text,
                            ui::Align::Left, true);
        }
        float center = mapX + size / 2;
        for (size_t i = 0; i < lines.size(); ++i)
            ui::labelScaled(context, center - lineWidths[i] / 2, mapY + size + margin + 2 + i * lineHeight,
                            lineWidths[i] + 2, lines[i], textScale, ui::palette::text, ui::Align::Left, element.shadow);
        context.flushText(0, std::nullopt);
        return ui::hud_editor::Box{placement.x, placement.y, boxW, boxH};
    } catch (std::exception const& error) {
        // Fail open: no minimap for the rest of the session, the game unaffected.
        state.failed = true;
        state.diagnostics.log(std::format("disabled after an error: {}", error.what()));
    } catch (...) {
        state.failed = true;
        state.diagnostics.log("disabled after an unknown error");
    }
    return std::nullopt;
}

void record(IClientInstance& client, Settings::Map const& settings, bool scanning) {
    if ((scanning && !settings.worldMap) || state.failed) return;
    try {
        auto place = waypoints::place();
        auto* player = client.getLocalPlayer();
        if (!place.active || !player) return;
        auto view = snapshot(client, false);
        if (!view) return;
        auto time = now();
        follow(view->dimension);
        // The Nether is recorded as the cave view, by layer; elsewhere the surface.
        bool cave = view->dimension == 1;
        state.layer = stableLayer(state.layer.value_or(0), blockFloor(view->y), !state.layer);
        int layer = cave ? *state.layer : 0;
        Saved saved{true, mapLayer(view->dimension, layer)};
        store::frame(place.world, place.mapFolder, saved.layer, view->playerX, view->playerZ, time);
        if (!scanning || !settings.worldMap) return;
        scan(client, *player, cave ? state.cave : state.surface, view->playerX, view->playerZ, cave, layer, recordBlocks,
             false, time, saved, recordBudgetSeconds);
    } catch (std::exception const& error) {
        state.failed = true;
        state.diagnostics.log(std::format("recording stopped after an error: {}", error.what()));
    } catch (...) {
        state.failed = true;
        state.diagnostics.log("recording stopped after an unknown error");
    }
}

void start() {
    // Without it the map still works, with ticked positions (alpha 1).
    if (!hooked) hooked = FrameAlphaHook::hook(true) == 0;
    if (!hooked) Runtime::instance().self().getLogger().warn("Minimap frame interpolation unavailable");
    auto& bus = ll::event::EventBus::getInstance();
    if (!commandsHooked) commandsHooked = CommandListHook::hook(true) == 0;
    if (!commandsHooked) Runtime::instance().self().getLogger().warn("Map teleport: the command list is unavailable");
    exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { ++worldGeneration; tpListed = -1; });
    joinListener = bus.emplaceListener<ll::event::ClientJoinLevelEvent>([](auto&) { ++worldGeneration; });
    if (!exitListener || !joinListener) {
        stop();
        throw std::runtime_error("Could not subscribe minimap world changes");
    }
}
void stop() {
    for (auto* listener : {&exitListener, &joinListener})
        if (*listener) {
            ll::event::EventBus::getInstance().removeListener(*listener);
            listener->reset();
        }
    if (hooked && FrameAlphaHook::unhook(true)) hooked = false;
    if (commandsHooked && CommandListHook::unhook(true)) commandsHooked = false;
}
std::optional<bool> teleportListed() {
    int value = tpListed.load();
    if (value < 0) return std::nullopt;
    return value == 1;
}
}

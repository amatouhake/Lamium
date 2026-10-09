#include "features/schematic/GhostRenderer.h"
#include "features/schematic/SchematicSession.h"
#include "features/schematic/SchematicItems.h"
#include "features/schematic/Selection.h"
#include "features/schematic/GhostFaces.h"
#include "features/schematic/EntityModels.h"
#include "features/schematic/SchematicRegion.h"
#include "features/schematic/LiquidShape.h"
#include "mc/world/level/block/VanillaStates.h"
#include "overlay/Depth.h"
#include "app/AtomicFile.h"
#include "ui/Localization.h"
#include "app/Runtime.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/ActorShaderManager.h"
#include "mc/client/renderer/BaseActorRenderer.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/gui/Font.h"
#include "mc/deps/minecraft_renderer/renderer/Type.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/FontRepository.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/client/renderer/SupplementaryFieldAutoGenerationMode.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/block/BlockGraphics.h"
#include "mc/client/renderer/texture/TextureUVCoordinateSet.h"
#include "mc/client/renderer/blockactor/BlockActorRenderDispatcher.h"
#include "mc/client/renderer/blockactor/MovingBlockActorRenderer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/client/renderer/game/LevelRendererCamera.h"
#include "mc/client/renderer/ptexture/LightTexture.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core_graphics/enums/PrimitiveMode.h"
#include "mc/deps/minecraft_renderer/framebuilder/dragon/RenderMetadata.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include "mc/deps/minecraft_renderer/renderer/MeshData.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureDescription.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/item/SaveContextFactory.h"
#include "mc/deps/renderer/Camera.h"
#include "mc/deps/renderer/MatrixStack.h"
#include "mc/locale/I18n.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/phys/AABB.h"
#include "mc/util/Mirror.h"
#include "mc/util/Rotation.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/phys/HitResult.h"
#include "mc/world/level/ShapeType.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/common/client/renderer/helpers/MeshHelpers.h"
#include "features/schematic/Verification.h"
#include <mutex>
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockRenderLayer.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/block/BrightnessPair.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/block/actor/BlockActorRendererId.h"
#include "mc/world/level/block/actor/VanillaBlockActorFactory.h"
#include "mc/dataloadhelper/DefaultDataLoadHelper.h"
#include "mc/world/level/ILevel.h"
#include "mc/world/level/block/states/VanillaBlockStateTransformUtils.h"
#include "mc/world/level/chunk/ChunkState.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/material/Material.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

namespace lamium::schematic::ghosts {
namespace {
using Clock = std::chrono::steady_clock;
constexpr int sectionSize = 16;
constexpr int sectionBudget = 3;      // Sections rebuilt per frame.
constexpr int checkBudget = 8;        // Sections whose blocks are compared per frame.
// Look at the world this often: quickly near the camera, where blocks are
// being placed, slowly elsewhere. A section is rebuilt only when its blocks
// changed since it was built.
constexpr std::chrono::milliseconds refreshNear{250}, refreshFar{2000};
constexpr double nearDistance = 24;
constexpr std::chrono::milliseconds lookedDelay{100};
constexpr double drawDistance = 192;  // Sections farther than this are not built or drawn.
constexpr float towardEye = overlay::depth::ghostPull; // Depth rules: overlay/Depth.h.

struct Outline { glm::vec3 min, max; float r, g, b; };
// A ghost drawn by its block-entity renderer, with the file's block entity
// data for it (bed color, skull type and rotation, sign text, banner).
struct EntityCell { BlockPos pos; Block const* block; std::optional<nbt::Compound> data; };
struct Section {
    glm::vec3 origin{};
    // faces: every render layer but the blended ones, drawn alpha-tested;
    // blend: blended layers (stained glass, honey, slime) and the mistake
    // marks, drawn last; marks: mistake marks over a real blended block,
    // which the blend mesh's depth would hide.
    std::optional<mce::Mesh> faces, blend, lines, marks;
    std::uint32_t faceVertices = 0, blendVertices = 0, lineVertices = 0, markVertices = 0;
    std::vector<EntityCell> entities;
    Clock::time_point built{}, checked{};
    std::optional<Clock::time_point> due; // An early rebuild after a looked-at block changed.
    std::uint64_t signature = 0;          // the world's blocks in the section when built
    bool complete = false; // false while some chunk was not loaded
};
using SectionKey = std::tuple<int, int, int, int>; // placement, section x, y, z
// Game blocks for a placement's palette, turned by the game's own transform.
// What one palette entry asks the player to place.
struct ItemInfo {
    std::string item, name, icon; // icon: the item as binary NBT for ItemStack::fromTag
    int perBlock = 1;
};
// An entity of the schematic where the placement puts it.
struct EntityGhost {
    std::string identifier, name, icon; // icon: an item of the same name, if the game has one
    Position at;   // world position of its feet
    Point offset;  // its cell inside the placed box, for layers
    float yaw = 0; // world facing, degrees (0 = south)
};
struct Resolved {
    std::shared_ptr<Structure const> keep; // keeps `structure` alive across snapshots
    Structure const* structure = nullptr;
    int rotation = 0;
    Mirror mirror = Mirror::None;
    std::vector<Block const*> blocks;
    // Palette entries that are opaque full blocks with no mesh on the ghost
    // path (honey block): they must not hide a neighbor's face.
    std::vector<bool> meshless;
    // Per palette entry, the sides of the cell its mesh reaches (any layer,
    // honey's slightly inset outer cube included), as sidesReached bits: a
    // mistake mark next to a block reaching all six leaves out its face
    // there, and near the camera a pair of faces in one plane keeps one.
    std::vector<int> sideMasks;
    // Per palette entry: the other half of a two-block-tall block (turned
    // like `blocks`) and the step up (+1) or down (-1) to it; null and 0
    // for other blocks.
    std::vector<Block const*> halves;
    std::vector<int> halfSteps;
    std::vector<ItemInfo> items;
    std::vector<EntityGhost> entities;
    // Whether each entity stands at its spot; nullopt while it cannot be
    // judged (too far for the client to know its entities, chunk not loaded).
    std::vector<std::optional<bool>> entityPlaced;
};

std::map<SectionKey, Section> sections;
std::vector<Resolved> resolved;
std::vector<std::string> builtKeys; // drawKey of each resolved placement
// Created once per cell; a null result is remembered too.
// Keyed by the block as well: two placements may want different block
// entities in one cell.
std::map<std::tuple<int, int, int, Block const*>, std::optional<std::shared_ptr<BlockActor>>> actors;
std::vector<std::pair<BlockPos, Block const*>> watched; // Recently looked-at cells and what was there.
// Entities are looked up this often, and only this close to the player: the
// client does not know entities beyond its tracking range.
constexpr std::chrono::milliseconds entityRefresh{250};
constexpr double entityRange = 48;
constexpr size_t maxEntities = 512; // per placement
// The dashed frame of an entity without a model: mob-sized, except for
// dropped items and experience orbs, whose real box is a quarter block.
struct FrameSize { float width, height; };
FrameSize entityFrame(std::string_view identifier) {
    if (identifier == "minecraft:item" || identifier == "minecraft:xp_orb") return {.25f, .25f};
    return {.8f, 1.8f};
}
constexpr float nameTagScale = .025f; // blocks per font pixel
Clock::time_point entitiesChecked{};
// Names over missing entities' frames, collected with the frames and drawn
// as name tags in the same pass.
std::vector<std::pair<Position, std::string>> labels;

// Verification of the selected placement, a bounded number of cells per
// frame; a finished pass is published and the next begins.
constexpr std::uint64_t scanBudget = 16384;
struct Cell {
    bool inside = false;      // false: outside the structure or a structure void
    CellState state = CellState::Unknown;
    int palette = -1;
    bool air = false, visible = false;
    Point world, offset;
    Block const* expected = nullptr;
    Block const* actual = nullptr;
};
struct Scan {
    std::uint64_t revision = 0;
    std::string key; // drawKey of the placement being checked
    int placement = -1;
    std::uint64_t next = 0;
    Tally tally;
    std::vector<Mismatch> mismatches;
    // The nearest cells of each kind (missing, other mistakes), kept as
    // max-heaps on distance; turned into rows when the pass ends.
    std::vector<std::pair<double, Cell>> nearMissing, nearMistakes;
    std::map<std::string, MaterialLine> all, shown;
};
Scan scan;
std::mutex resultMutex;
std::shared_ptr<Verification const> published = std::make_shared<Verification const>();
void publish(std::shared_ptr<Verification const> value) {
    std::lock_guard lock(resultMutex);
    published = std::move(value);
}
// Saving an area. The request is handed over under the mutex; the job itself
// belongs to the render thread.
constexpr std::uint64_t saveBudget = 32768; // cells read per frame
// Columns are read when their chunk is loaded; the others wait until the
// player comes near. While nothing can be read, a reminder every so often.
constexpr std::chrono::seconds waitReminder{8};
struct SaveJob {
    SaveRequest request;
    StructureBuilder builder;
    std::vector<Column> columns;
    std::vector<std::uint64_t> progress; // cells read per column
    std::uint64_t done = 0;
    std::map<Block const*, PaletteBlock> palette;
    Clock::time_point lastProgress = Clock::now(), lastReminder{};
    // Entities are taken per column when it is read: the client forgets
    // the ones far behind a player walking along a large area.
    std::set<std::int64_t> entitiesSeen;
};
std::mutex saveMutex;
std::optional<SaveRequest> pendingSave;
std::optional<std::string> saveMessage;
std::optional<SaveStatus> status; // published copy of the job's progress
bool saveBusy = false;
std::atomic<bool> stopRequested{false};
std::optional<SaveJob> saveJob;
void finishSave(std::string message) {
    saveJob.reset();
    std::lock_guard lock(saveMutex);
    saveBusy = false;
    status.reset();
    saveMessage = std::move(message);
}
// A save in progress or waiting ends when the world is left, the feature is
// off or the player stops it.
void stopSave() {
    {
        std::lock_guard lock(saveMutex);
        if (!saveBusy) return;
        pendingSave.reset();
    }
    stopRequested = false;
    finishSave(ui::translated("schematic.save.stopped"));
}
// "Show in world": a marked cell until `pointUntil`.
std::mutex pointMutex;
std::optional<Point> pointAt;
Clock::time_point pointUntil{};
std::uint64_t builtRevision = 0;
// The cells the camera is in (eye and feet). Ghosts within one cell of
// them are drawn whole: no face dropped, none skipped as enclosed. From
// inside a schematic, or with the camera at a cell border, what is around
// the player then shows as blocks instead of hollow space.
std::array<std::optional<Point>, 2> cameraCells;
// Near the camera, a pair of ghost faces in one plane keeps only the face
// toward the camera for every pair (true), or only for pairs of opaque full
// blocks (false). True: no flicker where a see-through block (a spawner)
// meets another, but from just outside the spawner's face there is gone
// while it shows from afar. False: the same faces near and far, flickering
// a little there like real blocks do (decided 2026-10-09: true, ghosts
// flickered more than real blocks).
constexpr bool pairAllGhostFaces = true;
bool nearCamera(Point p, int reach = 1) {
    return std::any_of(cameraCells.begin(), cameraCells.end(), [&](auto const& c) {
        return c && std::abs(c->x - p.x) <= reach && std::abs(c->y - p.y) <= reach && std::abs(c->z - p.z) <= reach;
    });
}
Vec3 buildCamera{}; // the camera position the sections near it are built for
int builtDimension = -1;
std::atomic<bool> releaseRequested{false};
ll::event::ListenerPtr exitListener;
bool installed = false;

void log(std::string const& text) {
    try { Runtime::instance().self().getLogger().info("Schematic ghosts: {}", text); } catch (...) {}
}
void release() {
    sections.clear();
    resolved.clear();
    models::reset();
    builtKeys.clear();
    actors.clear();
    watched.clear();
    entitiesChecked = {};
    labels.clear();
    builtRevision = 0;
    scan = {};
    publish(std::make_shared<Verification const>());
}

::Rotation gameRotation(int quarterTurns) {
    switch (quarterTurns) {
    case 1: return ::Rotation::Clockwise90;
    case 2: return ::Rotation::Clockwise180;
    case 3: return ::Rotation::CounterClockwise90;
    default: return ::Rotation::None;
    }
}
// The game names a mirror by the axis it reflects across: its Z mirror
// flips east-west states, Lamium's X (seen in game with `mixture` mirrored
// beside a structure block, 2026-10-09: chests, stairs, pane and fence
// connections all faced the other way).
::Mirror gameMirror(Mirror mirror) {
    return mirror == Mirror::X ? ::Mirror::Z : mirror == Mirror::Z ? ::Mirror::X : ::Mirror::None;
}
// The palette entry as the NBT the game's block registry reads.
Block const* lookup(PaletteBlock const& entry) {
    nbt::Root root;
    root.compound.set("name", {entry.name});
    root.compound.set("states", {entry.states});
    root.compound.set("version", {entry.version});
    auto tag = CompoundTag::fromBinaryNbt(nbt::write(root));
    if (!tag) return nullptr;
    auto block = Block::tryGetFromRegistry(*tag);
    return block ? &*block : nullptr;
}
bool flagged(nbt::Compound const& states, char const* name) {
    std::int64_t value = 0;
    auto const* tag = states.find(name);
    return tag && tag->integer(value) && value != 0;
}
// A block's states as text, for naming what differs (the target card).
std::map<std::string, std::string> blockStates(Block const& block) {
    std::map<std::string, std::string> out;
    auto const& tags = block.mSerializationId->mTags;
    auto found = tags.find("states");
    if (found == tags.end()) return out;
    auto* states = std::get_if<CompoundTag>(&found->second.mTagStorage);
    if (!states) return out;
    for (auto const& [key, value] : states->mTags) {
        if (auto* v = std::get_if<ByteTag>(&value.mTagStorage)) out[key] = std::to_string(v->data);
        else if (auto* v = std::get_if<IntTag>(&value.mTagStorage)) out[key] = std::to_string(v->data);
        else if (auto* v = std::get_if<StringTag>(&value.mTagStorage)) out[key] = *v;
    }
    return out;
}
ItemInfo describe(Block const& block, std::string_view fallback) {
    ItemInfo out;
    auto item = block.getBlockType().asItemInstance(block, nullptr);
    if (item.isNull()) { out.name = std::string(fallback); return out; }
    out.item = item.getTypeName();
    out.name = item.getName();
    nbt::Root tag;
    tag.compound.set("Name", {out.item});
    tag.compound.set("Count", {std::int8_t{1}});
    tag.compound.set("Damage", {static_cast<std::int16_t>(item.getAuxValue())});
    out.icon = nbt::write(tag);
    return out;
}
ItemInfo itemFor(PaletteBlock const& entry, Block const* block) {
    if (entry.isAir()) return {};
    ItemInfo out = block ? describe(*block, entry.name) : ItemInfo{"", entry.name, "", 1};
    out.perBlock = itemsPerBlock(entry.name, flagged(entry.states, "upper_block_bit") || flagged(entry.states, "head_piece_bit"));
    return out;
}
Resolved resolve(Structure const& structure, SavedPlacement const& placement) {
    Resolved out{nullptr, &structure, placement.placement.rotation, placement.placement.mirror, {}};
    out.blocks.reserve(structure.palette.size());
    unsigned missing = 0;
    auto turn = [&](Block const* block) {
        if (block && (out.rotation || out.mirror != Mirror::None))
            if (auto const* turned = VanillaBlockStateTransformUtils::transformBlock(*block, gameRotation(out.rotation), gameMirror(out.mirror)))
                return turned;
        return block;
    };
    for (auto const& entry : structure.palette) {
        Block const* block = entry.isAir() ? nullptr : lookup(entry);
        if (!block && !entry.isAir()) ++missing;
        out.items.push_back(itemFor(entry, block));
        out.blocks.push_back(turn(block));
        auto half = block ? otherHalf(entry) : std::nullopt;
        Block const* other = half ? turn(lookup(half->block)) : nullptr;
        out.halves.push_back(other);
        out.halfSteps.push_back(other ? half->step : 0);
        if (block)
            if (int extra = block->getBlockType().getExtraRenderLayers())
                log(std::format("{}: render layer {}, extra layers {:#x}", entry.name,
                    static_cast<int>(static_cast<BlockRenderLayer>(block->getBlockType().mRenderLayer)), extra));
    }
    if (missing) log(std::format("{}: {} palette entries are not known blocks", placement.file, missing));
    Size placed = placedSize(structure.size, placement.placement.rotation);
    for (auto const& entity : structure.entities) {
        if (entity.identifier.empty() || out.entities.size() >= maxEntities) continue;
        EntityGhost ghost{entity.identifier, {}, {}, toWorldPosition(structure.size, placement.placement, {entity.x, entity.y, entity.z}), {}};
        auto const& o = placement.placement.origin;
        ghost.offset = {std::clamp(static_cast<int>(std::floor(ghost.at.x)) - o.x, 0, placed.x - 1),
                        std::clamp(static_cast<int>(std::floor(ghost.at.y)) - o.y, 0, placed.y - 1),
                        std::clamp(static_cast<int>(std::floor(ghost.at.z)) - o.z, 0, placed.z - 1)};
        if (auto const* turn = entity.data.find("Rotation"); turn && turn->as<nbt::List>() && !turn->as<nbt::List>()->items.empty())
            if (auto const* yaw = turn->as<nbt::List>()->items.front().as<float>()) ghost.yaw = toWorldYaw(*yaw, placement.placement);
        auto key = entityNameKey(entity.identifier);
        ghost.name = getI18n().get(key, getI18n().getCurrentLanguage());
        if (ghost.name.empty() || ghost.name == key) ghost.name = entity.identifier;
        nbt::Root tag;
        tag.compound.set("Name", {entity.identifier});
        tag.compound.set("Count", {std::int8_t{1}});
        tag.compound.set("Damage", {std::int16_t{0}});
        ghost.icon = nbt::write(tag);
        out.entities.push_back(std::move(ghost));
    }
    out.entityPlaced.assign(out.entities.size(), std::nullopt);
    return out;
}

// Loads the file's block entity data into a ghost's block actor: at its
// world cell, a skull's rotation turned with the placement (bed parts and
// standing banners and signs turn through their block states).
void loadBlockEntity(BlockActor& actor, nbt::Compound data, BlockPos const& pos, Placement const& placement, BlockSource& region) {
    data.set("x", {std::int32_t{pos.x}});
    data.set("y", {std::int32_t{pos.y}});
    data.set("z", {std::int32_t{pos.z}});
    if (auto* rotation = data.find("Rotation"); rotation && rotation->as<float>())
        data.set("Rotation", {toWorldYaw(*rotation->as<float>(), placement)});
    nbt::Root root;
    root.compound = std::move(data);
    auto tag = CompoundTag::fromBinaryNbt(nbt::write(root));
    if (!tag) return;
    DefaultDataLoadHelper helper;
    actor.load(region.getILevel(), *tag, helper);
}
bool blended(BlockRenderLayer layer) {
    return layer == BlockRenderLayer::RenderlayerBlend || layer == BlockRenderLayer::RenderlayerBlendToOpaque;
}
// Light UVs and colors: the in-world mesh carries both, but fill when absent.
void finishColors(Tessellator& batch, float r, float g, float b) {
    auto& data = batch.mMeshData.get();
    size_t vertices = data.mPositions->size();
    auto& uv1 = data.mTextureUVs[1].get();
    if (uv1.size() != vertices) uv1.assign(vertices, glm::vec2{1.f, 1.f});
    auto& colors = data.mColors.get();
    if (colors.size() != vertices) colors.assign(vertices, 0xffffffffu);
    auto scale = [](std::uint32_t value, int shift, float factor) {
        return static_cast<std::uint32_t>(std::lround(std::clamp(((value >> shift) & 255) * factor, 0.f, 255.f))) << shift;
    };
    for (auto& c : colors) c = scale(c, 0, r) | scale(c, 8, g) | scale(c, 16, b) | (c & 0xff000000u);
}

// A ghost with an opaque full block on all six sides cannot be seen: real
// ones, or ghosts that will be drawn there (shown layers, nothing placed).
// An opaque full ghost will be drawn at `n`: nothing real is there, the
// placement asks for an opaque full block in a shown layer.
bool ghostOpaqueAt(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point n) {
    auto const& structure = *shown.structure;
    auto const& placement = shown.placement;
    Size placed = placedSize(structure.size, placement.placement.rotation);
    Point const& origin = placement.placement.origin;
    auto local = toLocal(structure.size, placement.placement, n);
    if (!local || !layerShown(placement.layers, placed, {n.x - origin.x, n.y - origin.y, n.z - origin.z})) return false;
    auto index = structure.blocks[static_cast<size_t>(structure.cell(local->x, local->y, local->z))];
    if (index == voidCell || static_cast<size_t>(index) >= blocks.blocks.size()) return false;
    Block const* ghost = blocks.blocks[static_cast<size_t>(index)];
    bool drawn = static_cast<size_t>(index) >= blocks.meshless.size() || !blocks.meshless[static_cast<size_t>(index)];
    return ghost && drawn && ghost->getBlockType().mIsOpaqueFullBlock && region.getBlock(BlockPos{n.x, n.y, n.z}).isAir();
}
// The sides of its cell that the ghost drawn at `n` reaches (sidesReached
// bits), 0 where no ghost is drawn (nothing expected in a shown layer,
// something real there, a liquid).
int ghostSidesAt(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point n) {
    auto const& structure = *shown.structure;
    auto const& placement = shown.placement;
    Size placed = placedSize(structure.size, placement.placement.rotation);
    Point const& origin = placement.placement.origin;
    auto local = toLocal(structure.size, placement.placement, n);
    if (!local || !layerShown(placement.layers, placed, {n.x - origin.x, n.y - origin.y, n.z - origin.z})) return 0;
    auto index = structure.blocks[static_cast<size_t>(structure.cell(local->x, local->y, local->z))];
    if (index == voidCell || static_cast<size_t>(index) >= blocks.blocks.size() || static_cast<size_t>(index) >= blocks.sideMasks.size()) return 0;
    Block const* block = blocks.blocks[static_cast<size_t>(index)];
    if (!block || liquidKind(*block)) return 0;
    BlockPos pos{n.x, n.y, n.z};
    auto* chunk = region.getChunkAt(pos);
    if (!chunk || chunk->mLoadState->load() < ChunkState::Loaded || !region.getBlock(pos).isAir()) return 0;
    return blocks.sideMasks[static_cast<size_t>(index)];
}
bool ghostBoxAt(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point n) {
    return ghostSidesAt(region, shown, blocks, n) == 63;
}
// What the placement has at `n` (shown layers, either layer), or the world
// where the placement says nothing (outside it, hidden layers, structure
// void), for a liquid shell's faces and slope. The file decides inside, so
// real liquids nearby do not bend a ghost liquid's surface.
liquids::Cell liquidAt(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point n) {
    auto const& structure = *shown.structure;
    auto const& placement = shown.placement;
    Size placed = placedSize(structure.size, placement.placement.rotation);
    Point const& origin = placement.placement.origin;
    auto local = toLocal(structure.size, placement.placement, n);
    if (local && layerShown(placement.layers, placed, {n.x - origin.x, n.y - origin.y, n.z - origin.z})) {
        auto cell = static_cast<size_t>(structure.cell(local->x, local->y, local->z));
        bool said = false, solid = false;
        for (auto const* layer : {&structure.blocks, &structure.liquids}) {
            if (cell >= layer->size()) continue;
            auto index = (*layer)[cell];
            if (index == voidCell || static_cast<size_t>(index) >= blocks.blocks.size()) continue;
            said = true;
            Block const* block = blocks.blocks[static_cast<size_t>(index)];
            if (!block) continue;
            if (int kind = liquidKind(*block)) return {kind, layer == &structure.blocks ? liquidDepth(*block) : 0, false};
            solid = solid || block->getMaterial().mSolid;
        }
        if (said) return {0, 0, solid};
    }
    BlockPos pos{n.x, n.y, n.z};
    Block const& real = region.getBlock(pos);
    if (int kind = liquidKind(real)) return {kind, liquidDepth(real), false};
    if (int kind = liquidKind(region.getBlock(pos, 1))) return {kind, 0, false};
    return {0, 0, static_cast<bool>(real.getMaterial().mSolid)};
}
bool enclosed(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point at) {
    for (auto const& d : faces::offsets) {
        Point n{at.x + d[0], at.y + d[1], at.z + d[2]};
        if (region.getBlock(BlockPos{n.x, n.y, n.z}).getBlockType().mIsOpaqueFullBlock) continue;
        if (!ghostOpaqueAt(region, shown, blocks, n)) return false;
    }
    return true;
}
// Drops the quads of a just tessellated ghost that lie on a side touching an
// opaque ghost: unseen from outside, and they fought with the neighbor's own
// face. Real opaque full neighbors (not blended ones) hide the quad too: the tessellator culls
// against them for most blocks, but not the honey block's outer cube, which
// fought with the real face in the same plane. A dropped
// quad collapses to one point, so no other vertex data has to move.
// Near the camera (this cell or the neighbor within one cell of it) the pair
// keeps one face instead: the one facing the camera. Every such plane then
// has exactly one face, so the cells around the camera look solid even when
// the near clip plane cuts the closest face, with nothing fighting in one
// plane (the material draws both sides).
void cullAgainstGhosts(Tessellator& batch, size_t from, BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point at) {
    auto& positions = batch.mMeshData->mPositions.get();
    std::array<std::optional<bool>, 6> hidden;
    for (size_t q = from; q + 4 <= positions.size(); q += 4) {
        std::array<faces::Vertex, 4> quad;
        for (size_t k = 0; k < 4; ++k) quad[k] = {positions[q + k].x, positions[q + k].y, positions[q + k].z};
        int side = faces::sideOf(quad, at.x, at.y, at.z);
        if (side < 0) continue;
        auto& known = hidden[static_cast<size_t>(side)];
        if (!known) {
            auto const& d = faces::offsets[side];
            Point n{at.x + d[0], at.y + d[1], at.z + d[2]};
            BlockPos np{n.x, n.y, n.z};
            Block const& real = region.getBlock(np);
            // Honey and slime count as opaque full blocks but are see-through.
            if (real.getBlockType().mIsOpaqueFullBlock && !blended(real.getBlockType().getRenderLayer(real, region, np))) {
                known = true;
            } else {
                known = ghostOpaqueAt(region, shown, blocks, n);
                bool pair = *known || (pairAllGhostFaces && (ghostSidesAt(region, shown, blocks, n) >> (side ^ 1) & 1));
                if (pair && (nearCamera(at) || nearCamera(n)))
                    known = !faces::beyond(side, at.x, at.y, at.z, buildCamera.x, buildCamera.y, buildCamera.z);
            }
        }
        if (*known) for (size_t k = 1; k < 4; ++k) positions[q + k] = positions[q];
    }
}
// In a cell the camera is in, a ghost keeps only the faces turned toward
// the camera, as the game draws blocks: seen from inside, the block is not
// there. The ghost material draws both sides, so from inside a door or a
// spawner its own faces fought with a neighbor's in the same plane, and a
// stair's two faces at half height (the lower half's top, the step's
// bottom) fought with each other.
void dropBackFaces(Tessellator& batch, size_t from) {
    auto& data = static_cast<mce::MeshData&>(batch.mMeshData);
    auto& positions = *data.mPositions;
    auto const& normals = *data.mNormals;
    glm::vec3 eye{static_cast<float>(buildCamera.x), static_cast<float>(buildCamera.y), static_cast<float>(buildCamera.z)};
    for (size_t q = from; q + 4 <= positions.size(); q += 4) {
        glm::vec3 n = normals.size() == positions.size() ? glm::vec3(normals[q])
                                                          : glm::cross(positions[q + 1] - positions[q], positions[q + 2] - positions[q]);
        if (glm::dot(n, n) < 1e-12f) continue;
        glm::vec3 center = (positions[q] + positions[q + 1] + positions[q + 2] + positions[q + 3]) * .25f;
        if (glm::dot(eye - center, n) < 0)
            for (size_t k = 1; k < 4; ++k) positions[q + k] = positions[q];
    }
}
// The cells of one section inside a placement's box.
// `halo` widens the section by that many cells on every side.
template <class Visit>
void eachCell(session::Shown const& shown, SectionKey key, int halo, Visit&& visit) {
    auto const& placement = shown.placement;
    Size placed = placedSize(shown.structure->size, placement.placement.rotation);
    Point const& origin = placement.placement.origin;
    auto [index, sx, sy, sz] = key;
    Point low{sx * sectionSize - halo, sy * sectionSize - halo, sz * sectionSize - halo};
    int span = sectionSize + 2 * halo;
    for (int x = std::max(low.x, origin.x - halo); x < std::min(low.x + span, origin.x + placed.x + halo); ++x)
        for (int y = std::max(low.y, origin.y - halo); y < std::min(low.y + span, origin.y + placed.y + halo); ++y)
            for (int z = std::max(low.z, origin.z - halo); z < std::min(low.z + span, origin.z + placed.z + halo); ++z)
                visit(x, y, z);
}
// The world's blocks in a section's cells, hashed: equal values mean nothing
// there changed and the built meshes still hold.
std::uint64_t signatureOf(BlockSource& region, session::Shown const& shown, SectionKey key) {
    std::uint64_t hash = 1469598103934665603ull;
    // One cell around the section too: its border ghosts' faces and
    // enclosure depend on the neighbors there.
    eachCell(shown, key, 1, [&](int x, int y, int z) {
        BlockPos pos{x, y, z};
        auto* chunk = region.getChunkAt(pos);
        auto value = chunk && chunk->mLoadState->load() >= ChunkState::Loaded ? reinterpret_cast<std::uintptr_t>(&region.getBlock(pos)) : 1;
        hash = (hash ^ static_cast<std::uint64_t>(value)) * 1099511628211ull;
    });
    return hash;
}

// What the tessellator sees at `n` while it draws the ghost at `at` (palette
// entry `drawn`): the placement's block where a ghost is drawn (shown layer,
// nothing real there), so doors find their other half and fences and panes
// connect to their schematic neighbors; the world elsewhere. A door's other
// half is supplied where the file has none or the world holds something
// else there (water, the area's edge), unless the real half is placed. An opaque full ghost that
// must not hide its neighbor's face (no mesh, or either cell next to the
// camera, where cullAgainstGhosts keeps the face toward the camera) reads
// as the world, so the tessellator keeps that face.
Block const* ghostNeighbor(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Point n, Point at, int drawn) {
    if (drawn >= 0 && static_cast<size_t>(drawn) < blocks.halves.size() && blocks.halfSteps[static_cast<size_t>(drawn)]
        && n.x == at.x && n.z == at.z && n.y == at.y + blocks.halfSteps[static_cast<size_t>(drawn)]) {
        Block const* half = blocks.halves[static_cast<size_t>(drawn)];
        if (&region.getBlock(BlockPos{n.x, n.y, n.z}).getBlockType() != &half->getBlockType()) return half;
    }
    auto const& structure = *shown.structure;
    auto const& placement = shown.placement;
    Size placed = placedSize(structure.size, placement.placement.rotation);
    Point const& origin = placement.placement.origin;
    auto local = toLocal(structure.size, placement.placement, n);
    if (!local || !layerShown(placement.layers, placed, {n.x - origin.x, n.y - origin.y, n.z - origin.z})) return nullptr;
    auto index = structure.blocks[static_cast<size_t>(structure.cell(local->x, local->y, local->z))];
    if (index == voidCell || static_cast<size_t>(index) >= blocks.blocks.size()) return nullptr;
    Block const* ghost = blocks.blocks[static_cast<size_t>(index)];
    if (!ghost || !region.getBlock(BlockPos{n.x, n.y, n.z}).isAir()) return nullptr;
    if (ghost->getBlockType().mIsOpaqueFullBlock) {
        bool meshless = static_cast<size_t>(index) < blocks.meshless.size() && blocks.meshless[static_cast<size_t>(index)];
        if (meshless || nearCamera(n) || nearCamera(at)) return nullptr;
    }
    return ghost;
}

void buildSection(ScreenContext& screen, BlockSource& region, SchematicRegion& view, BlockTessellator& own,
                  session::Shown const& shown, Resolved const& blocks, SectionKey key, Section& out) {
    auto const& structure = *shown.structure;
    auto const& placement = shown.placement;
    Size placed = placedSize(structure.size, placement.placement.rotation);
    Point const& origin = placement.placement.origin;
    auto [index, sx, sy, sz] = key;
    Point low{sx * sectionSize, sy * sectionSize, sz * sectionSize};
    // Reset in place: meshes cannot be copied or assigned.
    out.faces.reset(); out.blend.reset(); out.lines.reset(); out.marks.reset();
    out.faceVertices = out.blendVertices = out.lineVertices = out.markVertices = 0;
    out.entities.clear();
    out.due.reset();
    out.origin = {static_cast<float>(low.x), static_cast<float>(low.y), static_cast<float>(low.z)};
    out.built = out.checked = Clock::now();
    out.signature = signatureOf(region, shown, key);
    out.complete = true;

    if (blocks.meshless.size() != blocks.blocks.size()) {
        auto& meshless = const_cast<Resolved&>(blocks).meshless;
        meshless.assign(blocks.blocks.size(), false);
        for (size_t i = 0; i < blocks.blocks.size(); ++i)
            if (auto const* b = blocks.blocks[i]; b && b->getBlockType().mIsOpaqueFullBlock) meshless[i] = !coversNeighbors(*b, own, screen);
        auto& masks = const_cast<Resolved&>(blocks).sideMasks;
        masks.assign(blocks.blocks.size(), 0);
        for (size_t i = 0; i < blocks.blocks.size(); ++i)
            if (auto const* b = blocks.blocks[i]) masks[i] = sidesReached(*b, own, screen, true, .02f);
    }
    Point drawing{};
    int drawn = -1;
    view.answer = [&](BlockPos const& p) { return ghostNeighbor(region, shown, blocks, {p.x, p.y, p.z}, drawing, drawn); };
    struct Clear { SchematicRegion& view; ~Clear() { view.answer = nullptr; } } clear{view};
    Tessellator batch(screen.tessellator.mBufferResourceService), see(screen.tessellator.mBufferResourceService);
    batch.begin({}, mce::PrimitiveMode::QuadList, 4096, false);
    see.begin({}, mce::PrimitiveMode::QuadList, 256, false);
    // Mistakes also get tinted faces just outside the real block, so they
    // stay visible next to the vanilla selection outline.
    std::vector<Outline> outlines, marks;
    std::vector<std::pair<Point, Block const*>> liquids; // cells missing their liquid
    auto cellBox = [](BlockPos p) { return std::pair{glm::vec3(p.x, p.y, p.z), glm::vec3(p.x + 1, p.y + 1, p.z + 1)}; };
    for (int x = std::max(low.x, origin.x); x < std::min(low.x + sectionSize, origin.x + placed.x); ++x)
        for (int y = std::max(low.y, origin.y); y < std::min(low.y + sectionSize, origin.y + placed.y); ++y)
            for (int z = std::max(low.z, origin.z); z < std::min(low.z + sectionSize, origin.z + placed.z); ++z) {
                Point offset{x - origin.x, y - origin.y, z - origin.z};
                if (!layerShown(placement.layers, placed, offset)) continue;
                auto local = toLocal(structure.size, placement.placement, {x, y, z});
                if (!local) continue;
                auto paletteIndex = structure.blocks[static_cast<size_t>(structure.cell(local->x, local->y, local->z))];
                if (paletteIndex == voidCell || static_cast<size_t>(paletteIndex) >= blocks.blocks.size()) continue;
                Block const* expected = blocks.blocks[static_cast<size_t>(paletteIndex)];
                bool expectsAir = structure.palette[static_cast<size_t>(paletteIndex)].isAir();
                BlockPos pos{x, y, z};
                auto* chunk = region.getChunkAt(pos);
                // While a chunk arrives the client shows placeholder blocks:
                // neither counts as built until it is loaded.
                if (!chunk || chunk->mLoadState->load() < ChunkState::Loaded) { out.complete = false; continue; }
                Block const& actual = region.getBlock(pos);
                if (actual.getMaterial().mType == SharedTypes::v1_26_20::MaterialType::ClientRequestPlaceholder) {
                    out.complete = false;
                    continue;
                }
                auto [boxLow, boxHigh] = cellBox(pos);
                if (expectsAir) {
                    // An extra block: red outline (the real block hides any ghost).
                    if (placement.countExtras && !actual.isAir()) {
                        marks.push_back({boxLow, boxHigh, 1.f, .25f, .2f});
                    }
                    continue;
                }
                if (!expected) { outlines.push_back({boxLow, boxHigh, 1.f, .55f, .1f}); continue; } // unknown block name
                if (&actual == expected) continue; // placed correctly
                if (!actual.isAir()) {
                    bool sameType = &actual.getBlockType() == &expected->getBlockType();
                    // Something else is there: red, or yellow when only the state differs.
                    Outline mark = sameType ? Outline{boxLow, boxHigh, 1.f, .8f, .2f} : Outline{boxLow, boxHigh, 1.f, .25f, .2f};
                    marks.push_back(mark);
                    continue;
                }
                // A missing liquid: a shell, built after the blocks.
                if (liquidKind(*expected)) {
                    liquids.push_back({{x, y, z}, expected});
                    continue;
                }
                // Within two cells of the camera a ghost may own the face the
                // camera sees, so it is never skipped as enclosed.
                if (!nearCamera({x, y, z}, 2) && enclosed(region, shown, blocks, {x, y, z})) continue;
                drawing = {x, y, z};
                drawn = paletteIndex;
                glm::vec3 shapeLow{1e9f}, shapeHigh{-1e9f};
                bool meshed = false;
                // Each render layer the block draws in, into the mesh of its kind.
                eachLayer(*expected, region, pos, [&](std::optional<BlockRenderLayer> layer) {
                    Tessellator& target = layer && blended(*layer) ? see : batch;
                    size_t before = target.mMeshData->mPositions->size();
                    tessellateLayer(own, target, *expected, pos, layer);
                    cullAgainstGhosts(target, before, region, shown, blocks, {x, y, z});
                    if (std::any_of(cameraCells.begin(), cameraCells.end(), [&](auto const& c) { return c == Point{x, y, z}; }))
                        dropBackFaces(target, before);
                    auto const& positions = target.mMeshData->mPositions.get();
                    for (size_t v = before; v < positions.size(); ++v) {
                        shapeLow = glm::min(shapeLow, positions[v]);
                        shapeHigh = glm::max(shapeHigh, positions[v]);
                    }
                    meshed = meshed || positions.size() > before;
                });
                if (!meshed) {
                    // No block mesh: block entities draw through their renderer;
                    // others keep the outline alone.
                    std::optional<nbt::Compound> data;
                    if (auto found = structure.blockEntities.find(structure.cell(local->x, local->y, local->z));
                        found != structure.blockEntities.end())
                        data = found->second;
                    out.entities.push_back({pos, expected, std::move(data)});
                    outlines.push_back({boxLow, boxHigh, .35f, .85f, 1.f});
                    continue;
                }
                outlines.push_back({shapeLow, shapeHigh, .35f, .85f, 1.f});
            }

    if (batch.mCount) {
        // Section-relative vertices keep float precision far from the origin.
        for (auto& p : batch.mMeshData->mPositions.get()) p -= out.origin;
        finishColors(batch, .62f, .85f, 1.f);
        out.faceVertices = batch.mCount;
        out.faces.emplace(batch.end(Tessellator::UploadMode::Buffered, "Lamium schematic ghosts", SupplementaryFieldAutoGenerationMode{}));
    }
    finishColors(see, .62f, .85f, 1.f);
    // The second layer: water in a waterlogged block, where neither the
    // block nor its extra block in the world holds it.
    if (!structure.liquids.empty())
        for (int x = std::max(low.x, origin.x); x < std::min(low.x + sectionSize, origin.x + placed.x); ++x)
            for (int y = std::max(low.y, origin.y); y < std::min(low.y + sectionSize, origin.y + placed.y); ++y)
                for (int z = std::max(low.z, origin.z); z < std::min(low.z + sectionSize, origin.z + placed.z); ++z) {
                    if (!layerShown(placement.layers, placed, {x - origin.x, y - origin.y, z - origin.z})) continue;
                    auto local = toLocal(structure.size, placement.placement, {x, y, z});
                    if (!local) continue;
                    auto index = structure.liquids[static_cast<size_t>(structure.cell(local->x, local->y, local->z))];
                    if (index == voidCell || static_cast<size_t>(index) >= blocks.blocks.size()) continue;
                    Block const* liquid = blocks.blocks[static_cast<size_t>(index)];
                    int kind = liquid ? liquidKind(*liquid) : 0;
                    if (!kind) continue;
                    BlockPos pos{x, y, z};
                    auto* chunk = region.getChunkAt(pos);
                    if (!chunk || chunk->mLoadState->load() < ChunkState::Loaded) continue;
                    if (liquidKind(region.getBlock(pos)) == kind || liquidKind(region.getBlock(pos, 1)) == kind) continue;
                    liquids.push_back({{x, y, z}, liquid});
                }
    drawn = -1;
    for (auto const& [at, liquid] : liquids) {
        int kind = liquidKind(*liquid);
        drawing = at;
        auto around = [&](int dx, int dy, int dz) { return liquidAt(region, shown, blocks, {at.x + dx, at.y + dy, at.z + dz}); };
        liquidShell(own, see, BlockPos{at.x, at.y, at.z}, *liquid, [&](int side) {
            // Never against an opaque block, ghost or real: near the camera
            // the tessellator keeps such faces (see ghostNeighbor), and the
            // shell's face then shared a plane with the block's.
            auto const& d = faces::offsets[side];
            Point n{at.x + d[0], at.y + d[1], at.z + d[2]};
            return around(d[0], d[1], d[2]).kind != kind && !ghostOpaqueAt(region, shown, blocks, n)
                && !region.getBlock(BlockPos{n.x, n.y, n.z}).getBlockType().mIsOpaqueFullBlock;
        }, [&](int cx, int cz) { return liquids::corner(kind, around, cx, cz); });
    }
    if (!marks.empty()) {
        // Mistakes mark whole cells with a tinted box just outside the real
        // block. The boxes go into the blended mesh, sorted with the blended
        // ghosts: as a separate translucent draw the engine ordered them
        // against those ghosts differently from frame to frame (flicker).
        // Each box is a white concrete block tessellated at the cell, its
        // faces recolored, moved 0.01 out and textured with one texel.
        // Where marks of one color touch, the faces between them and the
        // outlines of cells inside a run are left out: dense wrong or extra
        // areas drew every one of them.
        auto cellOf = [](Outline const& m) {
            return std::tuple<int, int, int>{static_cast<int>(std::floor(m.min.x)), static_cast<int>(std::floor(m.min.y)),
                                             static_cast<int>(std::floor(m.min.z))};
        };
        std::map<std::tuple<int, int, int>, float> colorAt; // keyed cell -> green channel, which tells red from yellow
        for (auto const& m : marks) colorAt[cellOf(m)] = m.g;
        auto sameAt = [&](std::tuple<int, int, int> cell, int side, float g) {
            auto const& d = faces::offsets[side];
            auto found = colorAt.find({std::get<0>(cell) + d[0], std::get<1>(cell) + d[1], std::get<2>(cell) + d[2]});
            return found != colorAt.end() && found->second == g;
        };
        auto white = Block::tryGetFromRegistry(HashedString{"minecraft:white_concrete"});
        drawn = -1;
        // Over a real blended block (honey, stained glass) the box goes into
        // a mesh without depth writes, or the world's translucent layer,
        // drawn after the ghosts, would lose that block behind it.
        Tessellator quads(screen.tessellator.mBufferResourceService);
        quads.begin({}, mce::PrimitiveMode::QuadList, 48, false);
        constexpr int sides[6][4] = {{0,2,6,4},{1,5,7,3},{0,4,5,1},{2,3,7,6},{0,1,3,2},{4,6,7,5}}; // as faces::offsets
        for (auto const& m : marks) {
            auto cell = cellOf(m);
            Point at{std::get<0>(cell), std::get<1>(cell), std::get<2>(cell)};
            auto hidesBox = [&](int side) {
                auto const& d = faces::offsets[side];
                return sameAt(cell, side, m.g) || ghostBoxAt(region, shown, blocks, {at.x + d[0], at.y + d[1], at.z + d[2]});
            };
            if (!white) {
                quads.color(m.r, m.g, m.b, .3f);
                glm::vec3 a = m.min - glm::vec3{.01f} - out.origin, b = m.max + glm::vec3{.01f} - out.origin, c[8];
                for (int i = 0; i < 8; ++i) c[i] = {i & 1 ? b.x : a.x, i & 2 ? b.y : a.y, i & 4 ? b.z : a.z};
                int open = 0;
                for (int side = 0; side < 6; ++side) {
                    if (hidesBox(side)) continue;
                    ++open;
                    for (int k = 0; k < 4; ++k) quads.vertex(c[sides[side][k]].x, c[sides[side][k]].y, c[sides[side][k]].z);
                    for (int k = 3; k >= 0; --k) quads.vertex(c[sides[side][k]].x, c[sides[side][k]].y, c[sides[side][k]].z);
                    out.markVertices += 8;
                }
                if (open) outlines.push_back(m);
                continue;
            }
            drawing = at;
            size_t from = see.mMeshData->mPositions->size();
            tessellateLayer(own, see, *white, BlockPos{at.x, at.y, at.z}, std::nullopt);
            auto& data = static_cast<mce::MeshData&>(see.mMeshData);
            auto& positions = *data.mPositions;
            auto& uvs = *data.mTextureUVs[0];
            auto& colors = *data.mColors;
            if (colors.size() != positions.size()) colors.resize(positions.size(), 0xffffffffu);
            auto channel = [](float v, int shift) { return static_cast<std::uint32_t>(std::lround(std::clamp(v, 0.f, 1.f) * 255)) << shift; };
            std::uint32_t tint = channel(m.r, 0) | channel(m.g, 8) | channel(m.b, 16) | channel(.3f, 24);
            glm::vec3 center = glm::vec3(at.x, at.y, at.z) + glm::vec3(.5f);
            int open = 0;
            for (size_t q = from; q + 4 <= positions.size(); q += 4) {
                std::array<faces::Vertex, 4> quad;
                for (size_t k = 0; k < 4; ++k) quad[k] = {positions[q + k].x, positions[q + k].y, positions[q + k].z};
                int side = faces::sideOf(quad, at.x, at.y, at.z);
                // The box reaches 0.01 into the next cell: against a ghost that
                // fills its side the face would lie on (or just past) the
                // ghost's own and fight with it at grazing angles (Depth.h
                // rules 1 and 4). The ghost's face shows there instead.
                bool keep = side >= 0 && !hidesBox(side);
                if (!keep) {
                    for (size_t k = 1; k < 4; ++k) positions[q + k] = positions[q];
                    continue;
                }
                ++open;
                glm::vec2 texel{0.f};
                if (uvs.size() == positions.size()) {
                    for (size_t k = 0; k < 4; ++k) texel += uvs[q + k] * .25f;
                    for (size_t k = 0; k < 4; ++k) uvs[q + k] = texel;
                }
                for (size_t k = 0; k < 4; ++k) {
                    positions[q + k] = center + (positions[q + k] - center) * 1.02f;
                    colors[q + k] = tint;
                }
            }
            if (open) outlines.push_back(m);
        }
        // Ended either way, so the tessellator never stays open.
        auto mesh = quads.end(Tessellator::UploadMode::Buffered, "Lamium schematic mistakes", SupplementaryFieldAutoGenerationMode{});
        if (out.markVertices) out.marks.emplace(std::move(mesh));
    }
    if (see.mCount) {
        // Blended quads far to near from where the camera was at the build;
        // sections themselves are ordered when drawn.
        auto const& positions = see.mMeshData->mPositions.get();
        glm::vec3 eye{static_cast<float>(buildCamera.x), static_cast<float>(buildCamera.y), static_cast<float>(buildCamera.z)};
        std::vector<std::pair<float, std::uint32_t>> far;
        for (size_t q = 0; q + 4 <= positions.size(); q += 4) {
            glm::vec3 c = (positions[q] + positions[q + 1] + positions[q + 2] + positions[q + 3]) * .25f;
            far.push_back({-glm::dot(c - eye, c - eye), static_cast<std::uint32_t>(q / 4)});
        }
        std::stable_sort(far.begin(), far.end());
        std::vector<std::uint32_t> order;
        for (auto const& f : far) order.push_back(f.second);
        reorderQuads(see, order);
        for (auto& p : see.mMeshData->mPositions.get()) p -= out.origin;
        out.blendVertices = see.mCount;
        out.blend.emplace(see.end(Tessellator::UploadMode::Buffered, "Lamium schematic blended ghosts", SupplementaryFieldAutoGenerationMode{}));
    } else {
        // Ended either way, so the tessellator never stays open.
        see.end(Tessellator::UploadMode::Buffered, "Lamium schematic blended ghosts", SupplementaryFieldAutoGenerationMode{});
    }
    if (!outlines.empty()) {
        Tessellator lines(screen.tessellator.mBufferResourceService);
        lines.begin({}, mce::PrimitiveMode::LineList, static_cast<int>(outlines.size() * 24), false);
        constexpr int edges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
        for (auto const& o : outlines) {
            lines.color(o.r, o.g, o.b, 1.f);
            glm::vec3 a = o.min - glm::vec3{.002f} - out.origin, b = o.max + glm::vec3{.002f} - out.origin, c[8];
            for (int i = 0; i < 8; ++i) c[i] = {i & 1 ? b.x : a.x, i & 2 ? b.y : a.y, i & 4 ? b.z : a.z};
            for (auto [i, j] : edges) { lines.vertex(c[i].x, c[i].y, c[i].z); lines.vertex(c[j].x, c[j].y, c[j].z); }
        }
        out.lineVertices = static_cast<std::uint32_t>(outlines.size() * 24);
        out.lines.emplace(lines.end(Tessellator::UploadMode::Buffered, "Lamium schematic outlines", SupplementaryFieldAutoGenerationMode{}));
    }
}

// Which of each placement's entities stand at their spots, from the
// entities the client knows around the placement.
void checkEntities(BlockSource& region, LocalPlayer& player, session::Snapshot const& snapshot, int dimension) {
    auto now = Clock::now();
    if (now - entitiesChecked < entityRefresh) return;
    entitiesChecked = now;
    Vec3 feet = player.getFeetPos();
    for (size_t i = 0; i < snapshot.placements.size() && i < resolved.size(); ++i) {
        auto const& shown = snapshot.placements[i];
        auto& r = resolved[i];
        std::fill(r.entityPlaced.begin(), r.entityPlaced.end(), std::nullopt);
        bool wanted = shown.placement.visible || static_cast<int>(i) == snapshot.selected;
        if (r.entities.empty() || !shown.structure || !shown.placement.entities || !wanted || shown.placement.dimension != dimension)
            continue;
        std::vector<EntitySpot> expected;
        std::vector<size_t> judged;
        double lowX = 1e18, lowY = 1e18, lowZ = 1e18, highX = -1e18, highY = -1e18, highZ = -1e18;
        for (size_t e = 0; e < r.entities.size(); ++e) {
            auto const& at = r.entities[e].at;
            double dx = at.x - feet.x, dy = at.y - feet.y, dz = at.z - feet.z;
            if (dx * dx + dy * dy + dz * dz > entityRange * entityRange) continue;
            BlockPos pos{static_cast<int>(std::floor(at.x)), static_cast<int>(std::floor(at.y)), static_cast<int>(std::floor(at.z))};
            auto* chunk = region.getChunkAt(pos);
            if (!chunk || chunk->mLoadState->load() < ChunkState::Loaded) continue;
            expected.push_back({r.entities[e].identifier, at.x, at.y, at.z});
            judged.push_back(e);
            lowX = std::min(lowX, at.x); lowY = std::min(lowY, at.y); lowZ = std::min(lowZ, at.z);
            highX = std::max(highX, at.x); highY = std::max(highY, at.y); highZ = std::max(highZ, at.z);
        }
        if (expected.empty()) continue;
        constexpr float grow = static_cast<float>(entityReach) + .5f;
        AABB area{Vec3{static_cast<float>(lowX) - grow, static_cast<float>(lowY) - grow, static_cast<float>(lowZ) - grow},
                  Vec3{static_cast<float>(highX) + grow, static_cast<float>(highY) + grow, static_cast<float>(highZ) + grow}};
        // Owned copies: the actors are not kept past this call.
        std::vector<std::string> names;
        std::vector<Vec3> positions;
        for (Actor* actor : region.fetchEntities(&player, area, false, false)) {
            if (!actor || actor->mRemoved || names.size() >= 4 * maxEntities) continue;
            names.push_back(actor->getTypeName());
            positions.push_back(actor->getFeetPos());
        }
        std::vector<EntitySpot> actual;
        for (size_t a = 0; a < names.size(); ++a) actual.push_back({names[a], positions[a].x, positions[a].y, positions[a].z});
        auto placed = matchEntities(expected, actual);
        for (size_t k = 0; k < judged.size(); ++k) r.entityPlaced[judged[k]] = placed[k];
    }
}
// The schematic's entities in the finished pass: their own material lines
// and a "not placed" row for each one missing in the shown layers.
void addEntityResults(Verification& result, session::Shown const& shown, Resolved const& r) {
    if (!shown.placement.entities) return;
    Size placed = placedSize(shown.structure->size, shown.placement.placement.rotation);
    std::map<std::string, MaterialLine> all, visible;
    for (size_t e = 0; e < r.entities.size(); ++e) {
        auto const& ghost = r.entities[e];
        bool here = r.entityPlaced[e].value_or(false);
        bool inLayer = layerShown(shown.placement.layers, placed, ghost.offset);
        for (auto* lines : {&all, &visible}) {
            if (lines == &visible && !inLayer) continue;
            auto& line = (*lines)[ghost.identifier];
            if (line.name.empty()) { line.item = ghost.identifier; line.name = ghost.name; line.icon = ghost.icon; line.entity = true; }
            ++line.needed;
            if (here) ++line.placed;
        }
        if (inLayer && r.entityPlaced[e] == false && result.mismatches.size() < maxMismatches) {
            Point cell{static_cast<int>(std::floor(ghost.at.x)), static_cast<int>(std::floor(ghost.at.y)), static_cast<int>(std::floor(ghost.at.z))};
            result.mismatches.push_back({CellState::Missing, cell, ghost.icon, {}, ghost.name, {}, true});
        }
    }
    for (auto& [key, line] : all) result.materials.push_back(std::move(line));
    for (auto& [key, line] : visible) result.visibleMaterials.push_back(std::move(line));
}

// One cell of a placement, the n-th of its placed box (x, then y, then z
// fastest), compared with the world.
Cell classifyCell(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Size placed, std::uint64_t n) {
    Cell c;
    auto const& structure = *shown.structure;
    auto const& placement = shown.placement;
    int ox = static_cast<int>(n / (static_cast<std::uint64_t>(placed.y) * placed.z));
    int oy = static_cast<int>(n / placed.z % placed.y);
    int oz = static_cast<int>(n % placed.z);
    c.offset = {ox, oy, oz};
    c.world = {placement.placement.origin.x + ox, placement.placement.origin.y + oy, placement.placement.origin.z + oz};
    auto local = toLocal(structure.size, placement.placement, c.world);
    if (!local) return c;
    auto paletteIndex = structure.blocks[static_cast<size_t>(structure.cell(local->x, local->y, local->z))];
    if (paletteIndex == voidCell || static_cast<size_t>(paletteIndex) >= blocks.blocks.size()) return c;
    c.inside = true;
    c.palette = static_cast<int>(paletteIndex);
    c.air = structure.palette[static_cast<size_t>(paletteIndex)].isAir();
    c.expected = blocks.blocks[static_cast<size_t>(paletteIndex)];
    c.visible = layerShown(placement.layers, placed, c.offset);
    BlockPos pos{c.world.x, c.world.y, c.world.z};
    auto* chunk = region.getChunkAt(pos);
    if (!chunk || chunk->mLoadState->load() < ChunkState::Loaded) { c.state = CellState::Unknown; return c; }
    c.actual = &region.getBlock(pos);
    auto const* actual = c.actual;
    auto const* expected = c.expected;
    if (actual->getMaterial().mType == SharedTypes::v1_26_20::MaterialType::ClientRequestPlaceholder) c.state = CellState::Unknown;
    else if (c.air) c.state = actual->isAir() ? CellState::Correct : placement.countExtras ? CellState::Extra : CellState::Ignored;
    else if (!expected) c.state = CellState::Unknown;
    else if (actual == expected) c.state = CellState::Correct;
    else if (actual->isAir()) c.state = CellState::Missing;
    else c.state = &actual->getBlockType() == &expected->getBlockType() ? CellState::State : CellState::Wrong;
    return c;
}
// A classified cell as a row of the Check list.
Mismatch mismatchFor(Cell const& c, Resolved const& blocks) {
    Mismatch m{c.state, c.world, {}, {}, {}, {}};
    if (!c.air && c.palette >= 0) {
        auto const& info = blocks.items[static_cast<size_t>(c.palette)];
        m.expected = info.icon;
        m.expectedName = info.name;
    }
    if (c.actual && !c.actual->isAir()) {
        auto info = describe(*c.actual, c.actual->getTypeName());
        m.actual = info.icon;
        m.actualName = info.name;
        if (c.state == CellState::State && c.expected) {
            m.states = stateDifferences(blockStates(*c.expected), blockStates(*c.actual));
            m.identifier = c.expected->getTypeName();
        }
    }
    return m;
}
// Progress of every placement for the Placed list: correct / total in its
// shown layers, counted in the background only while the list is on screen.
// The selected placement takes its numbers from the full check instead.
constexpr std::uint64_t progressBudget = 8192;
std::atomic<std::int64_t> progressWanted{0}; // steady-clock milliseconds of the last request
struct ProgressScan {
    size_t index = 0;
    std::string key;
    std::uint64_t next = 0;
    Tally tally;
} progressScan;
std::mutex progressMutex;
std::map<std::string, Tally> progressDone; // by drawKey
void recordProgress(std::string const& key, Tally const& tally) {
    std::lock_guard lock(progressMutex);
    progressDone[key] = tally;
}
std::int64_t steadyMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}
void stepProgress(BlockSource& region, session::Snapshot const& snapshot, int dimension) {
    if (steadyMs() - progressWanted.load() > 1000) return;
    size_t count = snapshot.placements.size();
    if (!count || resolved.size() != count) return;
    for (std::uint64_t budget = progressBudget, tries = 0; budget && tries <= count; ) {
        if (progressScan.index >= count) progressScan.index = 0;
        auto const& shown = snapshot.placements[progressScan.index];
        bool usable = static_cast<int>(progressScan.index) != snapshot.selected && shown.structure
            && shown.placement.dimension == dimension && resolved[progressScan.index].structure == shown.structure.get();
        auto key = drawKey(shown.placement);
        if (!usable) { ++progressScan.index; progressScan.key.clear(); ++tries; continue; }
        if (progressScan.key != key) { progressScan.key = key; progressScan.next = 0; progressScan.tally = {}; }
        Size placed = placedSize(shown.structure->size, shown.placement.placement.rotation);
        std::uint64_t total = static_cast<std::uint64_t>(placed.x) * placed.y * placed.z;
        for (; progressScan.next < total && budget; ++progressScan.next, --budget) {
            auto c = classifyCell(region, shown, resolved[progressScan.index], placed, progressScan.next);
            if (c.inside && c.visible) progressScan.tally.add(c.state, !c.air);
        }
        if (progressScan.next < total) return;
        recordProgress(key, progressScan.tally);
        progressScan.key.clear();
        ++progressScan.index;
        ++tries;
    }
}
void stepScan(BlockSource& region, session::Snapshot const& snapshot, int dimension, Vec3 const& camera) {
    int index = snapshot.selected;
    bool valid = index >= 0 && index < static_cast<int>(snapshot.placements.size())
        && snapshot.placements[static_cast<size_t>(index)].structure
        && snapshot.placements[static_cast<size_t>(index)].placement.dimension == dimension;
    if (!valid) {
        if (scan.placement != -1) {
            scan = {};
            scan.revision = snapshot.revision;
            auto none = std::make_shared<Verification>();
            none->revision = snapshot.revision;
            publish(std::move(none));
        }
        return;
    }
    // A change to another placement, or to this one's name or visibility,
    // keeps the pass going.
    auto key = drawKey(snapshot.placements[static_cast<size_t>(index)].placement)
        + std::format("|{}", static_cast<void const*>(snapshot.placements[static_cast<size_t>(index)].structure.get()));
    if (scan.key != key) {
        scan = {};
        scan.key = key;
    }
    scan.revision = snapshot.revision;
    scan.placement = index;
    auto const& shown = snapshot.placements[static_cast<size_t>(index)];
    auto const& structure = *shown.structure;
    auto const& placement = shown.placement;
    auto const& blocks = resolved[static_cast<size_t>(index)];
    Size placed = placedSize(structure.size, placement.placement.rotation);
    std::uint64_t total = static_cast<std::uint64_t>(placed.x) * placed.y * placed.z;
    auto addMaterial = [](std::map<std::string, MaterialLine>& lines, ItemInfo const& info, bool correct) {
        auto key = info.item.empty() ? "block:" + info.name : info.item;
        auto& line = lines[key];
        if (line.name.empty()) { line.item = info.item; line.name = info.name; line.icon = info.icon; }
        line.needed += static_cast<std::uint64_t>(info.perBlock);
        if (correct) line.placed += static_cast<std::uint64_t>(info.perBlock);
    };
    for (std::uint64_t budget = scanBudget; scan.next < total && budget; ++scan.next, --budget) {
        auto c = classifyCell(region, shown, blocks, placed, scan.next);
        if (!c.inside) continue;
        auto paletteIndex = c.palette;
        bool visible = c.visible, air = c.air;
        CellState state = c.state;
        Block const* actual = c.actual;
        Block const* expected = c.expected;
        Point world = c.world;
        if (!air) {
            auto const& info = blocks.items[static_cast<size_t>(paletteIndex)];
            addMaterial(scan.all, info, state == CellState::Correct);
            if (visible) addMaterial(scan.shown, info, state == CellState::Correct);
        }
        if (!visible) continue;
        scan.tally.add(state, !air);
        bool mistake = state == CellState::Missing || state == CellState::Wrong || state == CellState::State || state == CellState::Extra;
        if (!mistake) continue;
        // Capped per kind and nearest first: in a large build the missing
        // blocks alone filled the list and the wrong ones never showed.
        auto& near = state == CellState::Missing ? scan.nearMissing : scan.nearMistakes;
        double dx = world.x + .5 - camera.x, dy = world.y + .5 - camera.y, dz = world.z + .5 - camera.z;
        double distance = dx * dx + dy * dy + dz * dz;
        auto farther = [](auto const& a, auto const& b) { return a.first < b.first; };
        if (near.size() < maxMismatches) { near.push_back({distance, c}); std::push_heap(near.begin(), near.end(), farther); }
        else if (distance < near.front().first) {
            std::pop_heap(near.begin(), near.end(), farther);
            near.back() = {distance, c};
            std::push_heap(near.begin(), near.end(), farther);
        }
    }
    if (scan.next < total) return;
    auto result = std::make_shared<Verification>();
    result->revision = scan.revision;
    result->placement = index;
    result->complete = true;
    result->visible = scan.tally;
    recordProgress(drawKey(placement), scan.tally);
    for (auto* near : {&scan.nearMistakes, &scan.nearMissing})
        for (auto const& [distance, cell] : *near) scan.mismatches.push_back(mismatchFor(cell, blocks));
    result->mismatches = std::move(scan.mismatches);
    for (auto& [key, line] : scan.all) result->materials.push_back(std::move(line));
    for (auto& [key, line] : scan.shown) result->visibleMaterials.push_back(std::move(line));
    addEntityResults(*result, shown, blocks);
    sortMismatches(result->mismatches, camera.x, camera.y, camera.z);
    sortMaterials(result->materials);
    sortMaterials(result->visibleMaterials);
    publish(std::move(result));
    // Start the next pass.
    scan.next = 0;
    scan.tally = {};
    scan.mismatches.clear();
    scan.nearMissing.clear();
    scan.nearMistakes.clear();
    scan.all.clear();
    scan.shown.clear();
}

template <class Draw>
void translated(ScreenContext& screen, glm::vec3 offset, Draw&& draw) {
    auto ref = screen.camera.worldMatrixStack->push(false);
    ref.stack->_isDirty = true;
    ref.mat->_m = glm::scale(glm::translate(ref.mat->_m.get(), offset * towardEye), glm::vec3{towardEye});
    // Pop manually, as the world overlay does for this stack, also when the
    // draw throws: a pushed matrix left behind would shift the whole world.
    auto pop = [&] {
        ref.stack->_isDirty = true;
        if (ref.stack->sortOrigin->has_value() && (ref.stack->stack->size() - 1) <= ref.stack->sortOrigin->value())
            ref.stack->sortOrigin->reset();
        ref.stack->stack->pop_back();
        ref.mat = nullptr;
        ref.stack = nullptr;
    };
    try { draw(); } catch (...) { pop(); throw; }
    pop();
}

// The palette entry of a game block: its serialized name, states and version.
PaletteBlock paletteEntry(Block const& block) {
    auto bytes = block.mSerializationId->toBinaryNbt();
    auto root = nbt::read({reinterpret_cast<std::uint8_t const*>(bytes.data()), bytes.size()});
    PaletteBlock out;
    if (auto const* name = root.compound.find("name"); name && name->as<std::string>()) out.name = *name->as<std::string>();
    if (auto const* states = root.compound.find("states"); states && states->as<nbt::Compound>()) out.states = *states->as<nbt::Compound>();
    std::int64_t version = 0;
    if (auto const* v = root.compound.find("version"); v && v->integer(version)) out.version = static_cast<std::int32_t>(version);
    if (out.name.empty()) out.name = block.getTypeName();
    return out;
}
bool vanillaLoads(std::string const& bytes, LocalPlayer& player) {
    try {
        auto tag = CompoundTag::fromBinaryNbt(bytes);
        if (!tag) return false;
        StructureTemplate probe("lamium:save_check", player.getLevel().getUnknownBlockTypeRegistry());
        return probe.load(*tag);
    } catch (...) {
        return false;
    }
}
void stepSave(BlockSource& region, LocalPlayer& player) {
    if (stopRequested.exchange(false)) { stopSave(); return; }
    if (!saveJob) {
        std::optional<SaveRequest> request;
        {
            std::lock_guard lock(saveMutex);
            request = std::exchange(pendingSave, std::nullopt);
        }
        if (!request) return;
        auto columns = chunkColumns(request->area);
        auto count = columns.size();
        saveJob.emplace(SaveJob{*request, StructureBuilder(request->area.size(), request->area.low()), std::move(columns),
                                std::vector<std::uint64_t>(count), 0, {}});
    }
    auto& job = *saveJob;
    if (static_cast<int>(player.getDimensionId()) != job.request.dimension) {
        finishSave(ui::translated("schematic.save.otherDimension"));
        return;
    }
    Point low = job.request.area.low();
    auto const& structure = job.builder.structure();
    int height = structure.size.y;
    std::uint64_t total = structure.cells(), budget = saveBudget;
    auto loaded = [&](BlockPos const& pos) {
        auto* chunk = region.getChunkAt(pos);
        return chunk && chunk->mLoadState->load() >= ChunkState::Loaded;
    };
    auto entry = [&](Block const& block) -> PaletteBlock const& {
        auto found = job.palette.find(&block);
        if (found == job.palette.end()) found = job.palette.emplace(&block, paletteEntry(block)).first;
        return found->second;
    };
    std::uint64_t before = job.done;
    for (size_t c = 0; c < job.columns.size() && budget; ++c) {
        auto const& column = job.columns[c];
        auto& read = job.progress[c];
        std::uint64_t cells = column.cells(height);
        if (read >= cells || !loaded(BlockPos{column.lowX, low.y, column.lowZ})) continue;
        int width = column.highX - column.lowX + 1, depth = column.highZ - column.lowZ + 1;
        bool wasRead = false;
        for (; read < cells && budget; ++read, --budget) {
            // Within a column: z fastest, then x, then y.
            int z = column.lowZ + static_cast<int>(read % depth);
            int x = column.lowX + static_cast<int>(read / depth % width);
            int y = low.y + static_cast<int>(read / (static_cast<std::uint64_t>(width) * depth));
            BlockPos pos{x, y, z};
            Block const& block = region.getBlock(pos);
            // A placeholder: the chunk is still arriving; come back later.
            if (block.getMaterial().mType == SharedTypes::v1_26_20::MaterialType::ClientRequestPlaceholder) break;
            auto cell = structure.cell(x - low.x, y - low.y, z - low.z);
            job.builder.setBlock(cell, entry(block));
            Block const& extra = region.getExtraBlock(pos);
            if (!extra.isAir()) job.builder.setLiquid(cell, entry(extra));
            // Block entity data as the client knows it (bed color, sign
            // text, banner, skull; a container's items only if synced).
            if (auto const* actor = region.getBlockEntity(pos)) {
                CompoundTag tag;
                if (auto context = SaveContextFactory::createCloneSaveContext(); context && actor->save(tag, *context)) {
                    try {
                        auto bytes = tag.toBinaryNbt();
                        auto read = nbt::read({reinterpret_cast<std::uint8_t const*>(bytes.data()), bytes.size()});
                        job.builder.setBlockEntity(cell, std::move(read.compound));
                    } catch (...) {}
                }
            }
            ++job.done;
            wasRead = read + 1 >= cells;
        }
        if (wasRead && job.request.entities) {
            AABB columnBox{Vec3{static_cast<float>(column.lowX), static_cast<float>(low.y), static_cast<float>(column.lowZ)},
                           Vec3{static_cast<float>(column.highX + 1), static_cast<float>(low.y + height), static_cast<float>(column.highZ + 1)}};
            for (Actor* actor : region.fetchEntities(&player, columnBox, false, false)) {
                if (!actor || actor->mRemoved || actor->hasType(ActorType::Player) || structure.entities.size() >= maxEntities) continue;
                if (!job.entitiesSeen.insert(actor->getOrCreateUniqueID().rawID).second) continue;
                // What the client knows: type, position and facing.
                auto feetAt = actor->getFeetPos();
                auto rotation = actor->getRotation();
                EntityRecord record{actor->getTypeName(), feetAt.x - low.x, feetAt.y - low.y, feetAt.z - low.z, {}};
                record.data.set("identifier", {record.identifier});
                nbt::List pos{nbt::Type::Float, {}}, turn{nbt::Type::Float, {}};
                for (float v : {feetAt.x, feetAt.y, feetAt.z}) pos.items.push_back({v});
                for (float v : {rotation.y, rotation.x}) turn.items.push_back({v});
                record.data.set("Pos", {std::move(pos)});
                record.data.set("Rotation", {std::move(turn)});
                job.builder.addEntity(std::move(record));
            }
        }
    }
    auto now = Clock::now();
    if (job.done != before) job.lastProgress = now;
    {
        std::lock_guard lock(saveMutex);
        status = SaveStatus{job.request.file, job.done, total, now - job.lastProgress > std::chrono::seconds(1)};
    }
    if (job.done < total) {
        if (now - job.lastProgress > std::chrono::seconds(1) && now - job.lastReminder > waitReminder) {
            job.lastReminder = now;
            // Point at the nearest column still waiting.
            auto feet = player.getFeetPos();
            Column const* nearest = nullptr;
            double best = 1e18;
            for (size_t c = 0; c < job.columns.size(); ++c) {
                if (job.progress[c] >= job.columns[c].cells(height)) continue;
                double dx = (job.columns[c].lowX + job.columns[c].highX) / 2.0 - feet.x, dz = (job.columns[c].lowZ + job.columns[c].highZ) / 2.0 - feet.z;
                if (dx * dx + dz * dz < best) { best = dx * dx + dz * dz; nearest = &job.columns[c]; }
            }
            static constexpr std::array<char const*, 8> directions{"schematic.dir.n", "schematic.dir.ne", "schematic.dir.e",
                "schematic.dir.se", "schematic.dir.s", "schematic.dir.sw", "schematic.dir.w", "schematic.dir.nw"};
            double dx = nearest ? (nearest->lowX + nearest->highX + 1) / 2.0 - feet.x : 0;
            double dz = nearest ? (nearest->lowZ + nearest->highZ + 1) / 2.0 - feet.z : 0;
            std::lock_guard lock(saveMutex);
            saveMessage = ui::translated("schematic.save.waiting", job.request.file, static_cast<int>(job.done * 100 / total),
                ui::translated(directions[static_cast<size_t>(compassOctant(dx, dz))]),
                static_cast<int>(std::lround(std::hypot(dx, dz))));
        }
        return;
    }
    try {
        std::filesystem::create_directories(job.request.path.parent_path());
        auto bytes = writeStructure(structure);
        writeFileReplacing(job.request.path, bytes, "schematic");
        auto sz = structure.size;
        // The game's own loader reads the file as written; the file stays
        // either way, the prompt only says so.
        bool loads = vanillaLoads(bytes, player);
        log(std::format("saved schematic {} ({}x{}x{}, {} layer(s)); the game's structure loader {} it", job.request.file,
                        sz.x, sz.y, sz.z, structure.liquids.empty() ? 1 : 2, loads ? "accepted" : "rejected"));
        finishSave(ui::translated(loads ? "schematic.save.done" : "schematic.save.gameRejected", job.request.file, sz.x, sz.y, sz.z));
    } catch (std::exception const& error) {
        log(std::string("could not save an area: ") + error.what());
        finishSave(ui::translated("schematic.save.failed", job.request.file));
    }
}
// The area chosen for saving: a white frame; corner 1 outlined red and
// corner 2 blue on the block's own edges, with tinted faces just outside so
// a full block still shows which corner it is.
void drawSelection(ScreenContext& screen, Vec3 const& camera, int dimension) {
    auto state = selection::current();
    if (state.dimension != dimension || (!state.first && !state.second)) return;
    Area area = state.area().value_or(Area{state.first ? *state.first : *state.second, state.first ? *state.first : *state.second});
    mce::MaterialPtr lineMaterial(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    mce::MaterialPtr faceMaterial(mce::RenderMaterialGroup::switchable(), HashedString{"holo_hand_pointer"});
    if (!lineMaterial.mRenderMaterialInfoPtr) return;
    Point low = area.low();
    Size size = area.size();
    constexpr int edges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    auto corners = [](glm::vec3 a, glm::vec3 b, glm::vec3 (&c)[8]) {
        for (int k = 0; k < 8; ++k) c[k] = {k & 1 ? b.x : a.x, k & 2 ? b.y : a.y, k & 4 ? b.z : a.z};
    };
    Tessellator lines(screen.tessellator.mBufferResourceService);
    lines.begin({}, mce::PrimitiveMode::LineList, 72, false);
    auto box = [&](glm::vec3 a, glm::vec3 b) {
        glm::vec3 c[8];
        corners(a, b, c);
        for (auto [p, q] : edges) { lines.vertex(c[p].x, c[p].y, c[p].z); lines.vertex(c[q].x, c[q].y, c[q].z); }
    };
    glm::vec3 base{static_cast<float>(low.x - camera.x), static_cast<float>(low.y - camera.y), static_cast<float>(low.z - camera.z)};
    lines.color(1.f, 1.f, 1.f, 1.f);
    box(base - glm::vec3{.03f}, base + glm::vec3{static_cast<float>(size.x), static_cast<float>(size.y), static_cast<float>(size.z)} + glm::vec3{.03f});
    Tessellator faces(screen.tessellator.mBufferResourceService);
    faces.begin({}, mce::PrimitiveMode::QuadList, 96, false);
    constexpr int sides[6][4] = {{0,2,6,4},{1,5,7,3},{0,4,5,1},{2,3,7,6},{0,1,3,2},{4,6,7,5}};
    for (int i = 0; i < 2; ++i) {
        auto const& corner = i == 0 ? state.first : state.second;
        if (!corner) continue;
        glm::vec3 color = i == 0 ? glm::vec3{1.f, .25f, .2f} : glm::vec3{.25f, .5f, 1.f};
        glm::vec3 at{static_cast<float>(corner->x - camera.x), static_cast<float>(corner->y - camera.y), static_cast<float>(corner->z - camera.z)};
        lines.color(color.r, color.g, color.b, 1.f);
        box(at - glm::vec3{.005f}, at + glm::vec3{1.005f});
        faces.color(color.r, color.g, color.b, .25f);
        glm::vec3 c[8];
        corners(at - glm::vec3{.01f}, at + glm::vec3{1.01f}, c);
        for (auto const& side : sides) {
            for (int k = 0; k < 4; ++k) faces.vertex(c[side[k]].x, c[side[k]].y, c[side[k]].z);
            for (int k = 3; k >= 0; --k) faces.vertex(c[side[k]].x, c[side[k]].y, c[side[k]].z);
        }
    }
    translated(screen, glm::vec3{0}, [&] {
        if (faceMaterial.mRenderMaterialInfoPtr && faces.mCount)
            MeshHelpers::renderMeshImmediately(screen, faces, faceMaterial, OffscreenCaptureDescription{});
        MeshHelpers::renderMeshImmediately(screen, lines, lineMaterial, OffscreenCaptureDescription{});
    });
}

// While a save waits, the chunk columns it still has to read: yellow frames
// standing on the area's floor, nearest first.
void drawWaitingColumns(ScreenContext& screen, Vec3 const& camera) {
    if (!saveJob) return;
    auto const& job = *saveJob;
    int height = job.builder.structure().size.y;
    Point low = job.request.area.low();
    std::vector<std::pair<double, Column const*>> waiting;
    for (size_t c = 0; c < job.columns.size(); ++c) {
        auto const& column = job.columns[c];
        if (job.progress[c] >= column.cells(height)) continue;
        double dx = (column.lowX + column.highX + 1) / 2.0 - camera.x, dz = (column.lowZ + column.highZ + 1) / 2.0 - camera.z;
        waiting.push_back({dx * dx + dz * dz, &column});
    }
    if (waiting.empty()) return;
    std::sort(waiting.begin(), waiting.end(), [](auto const& a, auto const& b) { return a.first < b.first; });
    if (waiting.size() > 256) waiting.resize(256);
    mce::MaterialPtr lineMaterial(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    mce::MaterialPtr faceMaterial(mce::RenderMaterialGroup::switchable(), HashedString{"holo_hand_pointer"});
    if (!lineMaterial.mRenderMaterialInfoPtr) return;
    Tessellator lines(screen.tessellator.mBufferResourceService), faces(screen.tessellator.mBufferResourceService);
    lines.begin({}, mce::PrimitiveMode::LineList, static_cast<int>(waiting.size() * 24), false);
    faces.begin({}, mce::PrimitiveMode::QuadList, static_cast<int>(waiting.size() * 8), false);
    lines.color(1.f, .8f, .25f, 1.f);
    faces.color(1.f, .8f, .25f, .18f);
    constexpr int edges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    for (auto const& [distance, column] : waiting) {
        glm::vec3 a{static_cast<float>(column->lowX - camera.x) + .05f, static_cast<float>(low.y - camera.y),
                    static_cast<float>(column->lowZ - camera.z) + .05f};
        glm::vec3 b{static_cast<float>(column->highX + 1 - camera.x) - .05f, static_cast<float>(low.y + height - camera.y),
                    static_cast<float>(column->highZ + 1 - camera.z) - .05f};
        glm::vec3 c[8];
        for (int k = 0; k < 8; ++k) c[k] = {k & 1 ? b.x : a.x, k & 2 ? b.y : a.y, k & 4 ? b.z : a.z};
        for (auto [p, q] : edges) { lines.vertex(c[p].x, c[p].y, c[p].z); lines.vertex(c[q].x, c[q].y, c[q].z); }
        // The floor, seen from both sides, so the column reads from above too.
        glm::vec3 floor[4]{c[0], c[1], c[5], c[4]};
        for (int k = 0; k < 4; ++k) faces.vertex(floor[k].x, floor[k].y + .02f, floor[k].z);
        for (int k = 3; k >= 0; --k) faces.vertex(floor[k].x, floor[k].y + .02f, floor[k].z);
    }
    translated(screen, glm::vec3{0}, [&] {
        if (faceMaterial.mRenderMaterialInfoPtr) MeshHelpers::renderMeshImmediately(screen, faces, faceMaterial, OffscreenCaptureDescription{});
        MeshHelpers::renderMeshImmediately(screen, lines, lineMaterial, OffscreenCaptureDescription{});
    });
}

// Missing entities: a dashed frame where each should stand, in the ghost
// color, with its name drawn by the HUD. Entities already there show nothing.
// Every placement's box in the ghosts' light blue (L-93 screen review): the
// selected one solid, the others dashed. The line material ignores alpha
// (checked 2026-10-08), so the shape tells them apart.
void drawPlacementFrames(ScreenContext& screen, session::Snapshot const& snapshot, int dimension, Vec3 const& camera) {
    mce::MaterialPtr lineMaterial(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    if (!lineMaterial.mRenderMaterialInfoPtr) return;
    constexpr int edges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    Tessellator lines(screen.tessellator.mBufferResourceService);
    int count = 0;
    for (size_t i = 0; i < snapshot.placements.size(); ++i) {
        auto const& shown = snapshot.placements[i];
        bool selected = static_cast<int>(i) == snapshot.selected;
        if (!shown.structure || shown.placement.dimension != dimension || (!shown.placement.visible && !selected)) continue;
        if (!count) lines.begin({}, mce::PrimitiveMode::LineList, static_cast<int>(snapshot.placements.size() * 24 * 8), false);
        ++count;
        Size size = placedSize(shown.structure->size, shown.placement.placement.rotation);
        auto const& o = shown.placement.placement.origin;
        glm::vec3 a{static_cast<float>(o.x - camera.x) - .02f, static_cast<float>(o.y - camera.y) - .02f,
                    static_cast<float>(o.z - camera.z) - .02f};
        glm::vec3 b = a + glm::vec3{static_cast<float>(size.x) + .04f, static_cast<float>(size.y) + .04f,
                                    static_cast<float>(size.z) + .04f};
        lines.color(.35f, .85f, 1.f, 1.f);
        glm::vec3 c[8];
        for (int k = 0; k < 8; ++k) c[k] = {k & 1 ? b.x : a.x, k & 2 ? b.y : a.y, k & 4 ? b.z : a.z};
        for (auto [p, q] : edges) {
            if (selected) { lines.vertex(c[p].x, c[p].y, c[p].z); lines.vertex(c[q].x, c[q].y, c[q].z); continue; }
            constexpr float dash = .5f, gap = .5f;
            glm::vec3 from = c[p], to = c[q];
            float length = glm::length(to - from);
            glm::vec3 step = (to - from) / length;
            for (float t = 0; t < length; t += dash + gap) {
                glm::vec3 s0 = from + step * t, s1 = from + step * std::min(length, t + dash);
                lines.vertex(s0.x, s0.y, s0.z);
                lines.vertex(s1.x, s1.y, s1.z);
            }
        }
    }
    if (!count) return;
    translated(screen, glm::vec3{0}, [&] {
        MeshHelpers::renderMeshImmediately(screen, lines, lineMaterial, OffscreenCaptureDescription{});
    });
}
// Missing entities: their game model with part outlines (L-115), or a dashed
// frame when the entity has no model.
void drawEntities(ScreenContext& screen, IClientInstance& client, session::Snapshot const& snapshot, int dimension, Vec3 const& camera) {
    std::vector<models::Spot> spots;
    std::vector<std::pair<size_t, std::string>> names; // spot index, name
    for (size_t i = 0; i < snapshot.placements.size() && i < resolved.size(); ++i) {
        auto const& shown = snapshot.placements[i];
        if (!shown.structure || !shown.placement.visible || !shown.placement.entities || shown.placement.dimension != dimension) continue;
        Size placed = placedSize(shown.structure->size, shown.placement.placement.rotation);
        auto const& r = resolved[i];
        for (size_t e = 0; e < r.entities.size(); ++e) {
            if (r.entityPlaced[e] != false || !layerShown(shown.placement.layers, placed, r.entities[e].offset)) continue;
            auto const& at = r.entities[e].at;
            double dx = at.x - camera.x, dy = at.y - camera.y, dz = at.z - camera.z, distance = dx * dx + dy * dy + dz * dz;
            if (distance > drawDistance * drawDistance) continue;
            spots.push_back({at, r.entities[e].identifier, r.entities[e].yaw});
            if (distance < 32 * 32) names.push_back({spots.size() - 1, r.entities[e].name});
        }
    }
    auto modelled = models::draw(screen, client, camera, spots, [&](std::function<void()> const& draw) { translated(screen, glm::vec3{0}, draw); });
    // A model says what the entity is; only the frames get name tags.
    std::vector<std::pair<Position, std::string>> named;
    for (auto& [index, name] : names) {
        if (modelled[index] || named.size() >= 64) continue;
        auto const& at = spots[index].at;
        named.push_back({{at.x, at.y + entityFrame(spots[index].identifier).height + .3, at.z}, std::move(name)});
    }
    labels = std::move(named);
    std::vector<std::pair<Position, FrameSize>> frames;
    for (size_t i = 0; i < spots.size(); ++i)
        if (!modelled[i]) frames.push_back({spots[i].at, entityFrame(spots[i].identifier)});
    mce::MaterialPtr lineMaterial(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    if (frames.empty() || !lineMaterial.mRenderMaterialInfoPtr) return;
    // Each edge as dashes. The frame does not claim the entity's real size,
    // which the client cannot know without the entity.
    constexpr int edges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    Tessellator lines(screen.tessellator.mBufferResourceService);
    lines.begin({}, mce::PrimitiveMode::LineList, static_cast<int>(frames.size() * 12 * 12), false);
    lines.color(.35f, .85f, 1.f, 1.f);
    for (auto const& [at, size] : frames) {
        // Shorter dashes on small frames so every edge still shows a dash.
        float dash = size.height < 1 ? .06f : .2f, gap = size.height < 1 ? .04f : .15f, half = size.width / 2;
        glm::vec3 base{static_cast<float>(at.x - camera.x), static_cast<float>(at.y - camera.y), static_cast<float>(at.z - camera.z)};
        glm::vec3 a = base + glm::vec3{-half, 0, -half}, b = base + glm::vec3{half, size.height, half}, c[8];
        for (int k = 0; k < 8; ++k) c[k] = {k & 1 ? b.x : a.x, k & 2 ? b.y : a.y, k & 4 ? b.z : a.z};
        for (auto [p, q] : edges) {
            glm::vec3 from = c[p], to = c[q];
            float length = glm::length(to - from);
            glm::vec3 step = (to - from) / length;
            for (float t = 0; t < length; t += dash + gap) {
                glm::vec3 s0 = from + step * t, s1 = from + step * std::min(length, t + dash);
                lines.vertex(s0.x, s0.y, s0.z);
                lines.vertex(s1.x, s1.y, s1.z);
            }
        }
    }
    translated(screen, glm::vec3{0}, [&] { MeshHelpers::renderMeshImmediately(screen, lines, lineMaterial, OffscreenCaptureDescription{}); });
}

// The names of missing entities like a named entity's tag: a dark plate with
// the text, facing the camera, a fixed size in the world. Drawn with the
// game's name tag materials (the both-sides variants).
void drawNameTags(ScreenContext& screen, IClientInstance& client, BlockSource& region, BaseActorRenderer& renderer, Vec3 const& camera) {
    if (labels.empty()) return;
    auto const& backgroundMaterial = renderer.mNameTagBackgroundWithBackfaceMat.get();
    auto const& textMaterial = renderer.mNameTagTextWithBackfaceMat.get();
    if (!backgroundMaterial.mRenderMaterialInfoPtr || !textMaterial.mRenderMaterialInfoPtr) return;
    if (screen.camera.viewMatrixStack->stack->empty()) return;
    auto view = *screen.camera.viewMatrixStack->top()._m;
    // The view's rotation rows are the camera axes.
    glm::vec3 right{view[0][0], view[1][0], view[2][0]}, up{view[0][1], view[1][1], view[2][1]};
    glm::vec3 across = glm::cross(right, -up);
    auto& font = client.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default").getFont();
    auto background = BaseActorRenderer::NAME_TAG_BACKGROUND_COLOR();
    // Smooth fonts (Japanese, Chinese) keep their own material, which reads
    // text constants the name tag material does not set: without them the
    // glyphs got colored fringes (L-117).
    bool smoothFont = !font.materialCanBeOverridden();
    for (auto const& [at, name] : labels) {
        // Not through walls: a block between the camera and the tag hides it.
        Vec3 to{static_cast<float>(at.x), static_cast<float>(at.y), static_cast<float>(at.z)};
        auto hit = region.clip(camera, to, false, ShapeType::Outline, 64, false, false, nullptr,
            [](BlockSource const&, Block const&, bool) { return true; }, false);
        if (hit.mType == HitResultType::Tile) continue;
        float width = static_cast<float>(font.getLineLength(name, 1.f, false));
        // The glyph sheet of the first character past ASCII: its sheet may be
        // a smooth (multi-channel) one whose material must not be replaced.
        int sheet = 0, first = 0;
        for (size_t i = 0; i < name.size();) {
            auto c = static_cast<unsigned char>(name[i]);
            int length = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
            if (c >= 0x80 && i + length <= name.size()) {
                first = c < 0xE0 ? (c & 0x1F) : c < 0xF0 ? (c & 0x0F) : (c & 0x07);
                for (int k = 1; k < length; ++k) first = (first << 6) | (static_cast<unsigned char>(name[i + k]) & 0x3F);
                sheet = first >> 8;
                break;
            }
            i += length;
        }
        bool smooth = smoothFont || sheet != 0;
        // Glyphs of scaled sheets (Japanese: 1.333) are measured at their
        // sheet's scale but drawn at 1, so the plate came out too wide.
        if (sheet != 0) {
            width = 0;
            for (size_t i = 0; i < name.size();) {
                auto c = static_cast<unsigned char>(name[i]);
                size_t length = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
                length = std::min(length, name.size() - i);
                int point = length == 1 ? c : length == 2 ? (c & 0x1F) : length == 3 ? (c & 0x0F) : (c & 0x07);
                for (size_t k = 1; k < length; ++k) point = (point << 6) | (static_cast<unsigned char>(name[i + k]) & 0x3F);
                float scale = font.getScaleFactor(point);
                width += static_cast<float>(font.getLineLength(std::string_view(name).substr(i, length), 1.f, false)) / (scale > 0 ? scale : 1.f);
                i += length;
            }
        }
        static std::set<int> described;
        if (described.insert(sheet).second) {
            auto shift = font.getTranslationFactor();
            log(std::format("name tags: sheet {} type {} overridable {} scale {:.3f} / char {:.3f}, shift {:.2f},{:.2f}, \"{}\" {} px", sheet,
                static_cast<int>(font.getType(sheet)), font.materialCanBeOverridden(), font.getScaleFactor(), font.getScaleFactor(first), shift.x, shift.y,
                name, width));
        }
        glm::vec3 offset{static_cast<float>(at.x - camera.x), static_cast<float>(at.y - camera.y), static_cast<float>(at.z - camera.z)};
        // Font pixels: x to the camera's right, y downward.
        glm::mat4 model{glm::vec4(right * nameTagScale, 0), glm::vec4(-up * nameTagScale, 0), glm::vec4(across * nameTagScale, 0),
                        glm::vec4(offset, 1)};
        auto ref = screen.camera.worldMatrixStack->push(false);
        ref.stack->_isDirty = true;
        ref.mat->_m = ref.mat->_m.get() * model;
        Tessellator plate(screen.tessellator.mBufferResourceService);
        plate.begin({}, mce::PrimitiveMode::QuadList, 8, false);
        plate.color(background.r, background.g, background.b, background.a);
        float x0 = -width / 2 - 1, x1 = width / 2 + 1, y0 = -1, y1 = 8;
        glm::vec2 quad[4]{{x0, y0}, {x0, y1}, {x1, y1}, {x1, y0}};
        for (int k = 0; k < 4; ++k) plate.vertex(quad[k].x, quad[k].y, .01f);
        for (int k = 3; k >= 0; --k) plate.vertex(quad[k].x, quad[k].y, .01f);
        MeshHelpers::renderMeshImmediately(screen, plate, backgroundMaterial, OffscreenCaptureDescription{});
        mce::Color white{1.f, 1.f, 1.f, 1.f}, black{0.f, 0.f, 0.f, 1.f};
        // The smooth sheet's edge softness follows the on-screen size of a
        // font pixel (the UI passes its GUI scale): about nameTagScale times
        // 1080 px over the view height at that distance (70 degrees). At 1
        // the edges spread over the glyphs and darkened them.
        float onScreen = nameTagScale * 1080.f / (2 * std::max(glm::length(offset), .5f) * .7f);
        if (smooth) font.setTextConstantsInScreenContext(screen, sheet, std::clamp(onScreen, .5f, 8.f), white, false);
        font.drawCached(screen, name, -width / 2, 0, white, false, false, false, smooth ? nullptr : &textMaterial, -1, false, 0, white, black, 0, 0,
            OffscreenCaptureDescription{}, false);
        ref.stack->_isDirty = true;
        if (ref.stack->sortOrigin->has_value() && (ref.stack->stack->size() - 1) <= ref.stack->sortOrigin->value())
            ref.stack->sortOrigin->reset();
        ref.stack->stack->pop_back();
        ref.mat = nullptr;
        ref.stack = nullptr;
    }
}

void drawPlacements(BaseActorRenderContext& context, IClientInstance& client, LocalPlayer& player) {
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
            auto key = drawKey(shown.placement);
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
        buildCamera = camera;
        if (now != cameraCells) {
            auto section = [](int v) { return static_cast<int>(std::floor(v / static_cast<double>(sectionSize))); };
            auto mark = [&](std::optional<Point> const& c) {
                if (!c) return;
                for (int dx = -2; dx <= 2; dx += 2) for (int dy = -2; dy <= 2; dy += 2) for (int dz = -2; dz <= 2; dz += 2)
                    for (auto& [key, built] : sections)
                        if (std::get<1>(key) == section(c->x + dx) && std::get<2>(key) == section(c->y + dy) && std::get<3>(key) == section(c->z + dz))
                            built.due = Clock::now();
            };
            for (auto const& c : cameraCells) mark(c);
            for (auto const& c : now) mark(c);
            cameraCells = now;
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
    // Sections near the camera, for visible placements in this dimension.
    struct Wanted { SectionKey key; double distance; bool seen; };
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
                    if (distance <= drawDistance) wanted.push_back({{i, sx, sy, sz}, distance, inView(sx, sy, sz)});
                }
    }
    std::sort(wanted.begin(), wanted.end(), [](auto const& a, auto const& b) {
        return a.seen != b.seen ? a.seen : a.distance < b.distance;
    });
    std::set<SectionKey> keep;
    for (auto const& w : wanted) keep.insert(w.key);
    std::erase_if(sections, [&](auto const& entry) { return !keep.contains(entry.first); });

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
        for (auto& [key, built] : sections)
            if (std::abs(std::get<1>(key) - section(pos.x)) <= 1 && std::abs(std::get<2>(key) - section(pos.y)) <= 1
                && std::abs(std::get<3>(key) - section(pos.z)) <= 1
                && section(pos.x - 1) <= std::get<1>(key) && std::get<1>(key) <= section(pos.x + 1)
                && section(pos.y - 1) <= std::get<2>(key) && std::get<2>(key) <= section(pos.y + 1)
                && section(pos.z - 1) <= std::get<3>(key) && std::get<3>(key) <= section(pos.z + 1))
                if (!built.due || due < *built.due) built.due = due;
    }
    // Keep the previous positions one more frame: placing moves the crosshair.
    std::vector<std::pair<BlockPos, Block const*>> next;
    for (auto const& pos : looked) next.push_back({pos, &region.getBlock(pos)});
    for (auto const& [pos, seen] : watched)
        if (next.size() < 6 && std::none_of(next.begin(), next.end(), [&](auto const& n) { return n.first == pos; }))
            next.push_back({pos, &region.getBlock(pos)});
    watched = std::move(next);

    // Build missing sections and rebuild changed ones, in view and nearest
    // first, within the budgets.
    int budget = sectionBudget, checks = checkBudget;
    // The view outlives the tessellator that reads through it.
    std::unique_ptr<SchematicRegion> view;
    std::unique_ptr<BlockTessellator> own;
    auto now = Clock::now();
    for (auto const& w : wanted) {
        if (!budget) break;
        auto found = sections.find(w.key);
        auto refreshAfter = w.distance <= nearDistance ? std::chrono::duration_cast<Clock::duration>(refreshNear)
            : std::chrono::duration_cast<Clock::duration>(refreshFar);
        bool stale = found == sections.end() || !found->second.complete || (found->second.due && now >= *found->second.due)
            || (found->second.faces && !found->second.faces->isValid()) || (found->second.blend && !found->second.blend->isValid())
            || (found->second.lines && !found->second.lines->isValid())
            || (found->second.marks && !found->second.marks->isValid());
        if (!stale && now - found->second.checked > refreshAfter && checks > 0) {
            --checks;
            auto const index = static_cast<size_t>(std::get<0>(w.key));
            stale = signatureOf(region, snapshot.placements[index], w.key) != found->second.signature;
            found->second.checked = now;
        }
        if (!stale) continue;
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
        buildSection(screen, region, *view, *own, snapshot.placements[index], resolved[index], w.key, sections[w.key]);
        --budget;
    }

    checkEntities(region, player, snapshot, dimension);
    stepScan(region, snapshot, dimension, camera);
    stepProgress(region, snapshot, dimension);

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
    mce::MaterialPtr lineMaterial(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    // Vertex-colored and blended without depth writes, as shape faces use in Fancy graphics.
    mce::MaterialPtr markMaterial(mce::RenderMaterialGroup::switchable(), HashedString{"holo_hand_pointer"});
    std::variant<std::monostate, mce::TexturePtr, mce::ClientTexture, mce::ServerTexture> texture{atlas};
    std::unique_ptr<SchematicRegion> actorView;
    for (auto& [key, section] : sections) {
        if (!inView(std::get<1>(key), std::get<2>(key), std::get<3>(key))) continue;
        glm::vec3 offset{static_cast<float>(section.origin.x - camera.x), static_cast<float>(section.origin.y - camera.y),
                         static_cast<float>(section.origin.z - camera.z)};
        translated(screen, offset, [&] {
            if (section.faces && faces.mRenderMaterialInfoPtr && lightTexture) {
                fullBright();
                section.faces->renderMesh(screen, faces, texture, 0, section.faceVertices, OffscreenCaptureDescription{}, nullptr);
            }
            if (section.marks && markMaterial.mRenderMaterialInfoPtr)
                section.marks->renderMesh(screen, markMaterial, gsl::span<mce::ClientTexture const*>{}, 0, section.markVertices,
                    OffscreenCaptureDescription{}, nullptr);
            if (section.lines && lineMaterial.mRenderMaterialInfoPtr)
                section.lines->renderMesh(screen, lineMaterial, gsl::span<mce::ClientTexture const*>{}, 0, section.lineVertices,
                    OffscreenCaptureDescription{}, nullptr);
        });
        for (auto const& [pos, block, data] : section.entities) {
            // The renderer reads the block at the cell (wall or standing
            // banner, which head, facing): the ghost's, not the world's air.
            if (!actorView) actorView = std::make_unique<SchematicRegion>(region);
            actorView->answer = [&](BlockPos const& at) { return at == pos ? block : nullptr; };
            auto& actor = actors[{pos.x, pos.y, pos.z, block}];
            if (!actor) {
                actor = VanillaBlockActorFactory::createBlockActor(pos, block->getBlockType());
                if (*actor && data) loadBlockEntity(**actor, *data, pos, snapshot.placements[static_cast<size_t>(std::get<0>(key))].placement.placement,
                                                    region);
            }
            auto* component = *actor ? (*actor)->_getRenderComponent() : nullptr;
            if (!component) continue;
            Vec3 renderPos{static_cast<float>(pos.x - camera.x), static_cast<float>(pos.y - camera.y), static_cast<float>(pos.z - camera.z)};
            mce::MaterialPtr none(mce::RenderMaterialGroup::common(), HashedString{"lamium_no_forced_material"});
            dispatcher.render(context, *actorView, *component, *block, renderPos, pos, false, none, nullptr, 0, std::nullopt);
        }
    }
    if (actorView) actorView->answer = nullptr;
    drawPlacementFrames(screen, snapshot, dimension, camera);
    drawEntities(screen, client, snapshot, dimension, camera);
    drawNameTags(screen, client, region, *moving, camera);
}

// The cell chosen with "Show in world": a pulsing tinted box with outlines
// and a tall beam of crossed faces above it, readable from far away.
void drawPoint(ScreenContext& screen, Vec3 const& camera) {
    std::optional<Point> at;
    {
        std::lock_guard lock(pointMutex);
        if (pointAt && Clock::now() > pointUntil) pointAt.reset();
        at = pointAt;
    }
    if (!at) return;
    mce::MaterialPtr lineMaterial(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    mce::MaterialPtr faceMaterial(mce::RenderMaterialGroup::switchable(), HashedString{"holo_hand_pointer"});
    float pulse = .5f + .5f * std::sin(std::chrono::duration<float>(Clock::now().time_since_epoch()).count() * 6.f);
    glm::vec3 offset{static_cast<float>(at->x - camera.x), static_cast<float>(at->y - camera.y), static_cast<float>(at->z - camera.z)};
    constexpr float grow = .04f, beam = 64.f, half = .12f;
    if (faceMaterial.mRenderMaterialInfoPtr) {
        Tessellator faces(screen.tessellator.mBufferResourceService);
        faces.begin({}, mce::PrimitiveMode::QuadList, 48 + 16, false);
        faces.color(1.f, 1.f, 1.f, .25f + .3f * pulse);
        glm::vec3 a{-grow}, b{1 + grow}, c[8];
        for (int i = 0; i < 8; ++i) c[i] = {i & 1 ? b.x : a.x, i & 2 ? b.y : a.y, i & 4 ? b.z : a.z};
        constexpr int sides[6][4] = {{0,2,6,4},{1,5,7,3},{0,4,5,1},{2,3,7,6},{0,1,3,2},{4,6,7,5}};
        for (auto const& side : sides) {
            for (int k = 0; k < 4; ++k) faces.vertex(c[side[k]].x, c[side[k]].y, c[side[k]].z);
            for (int k = 3; k >= 0; --k) faces.vertex(c[side[k]].x, c[side[k]].y, c[side[k]].z);
        }
        faces.color(1.f, 1.f, 1.f, .45f);
        // Two crossed faces make the beam visible from every side.
        glm::vec3 beamQuads[2][4] = {{{.5f - half, 1, .5f}, {.5f + half, 1, .5f}, {.5f + half, beam, .5f}, {.5f - half, beam, .5f}},
                                     {{.5f, 1, .5f - half}, {.5f, 1, .5f + half}, {.5f, beam, .5f + half}, {.5f, beam, .5f - half}}};
        for (auto const& quad : beamQuads) {
            for (int k = 0; k < 4; ++k) faces.vertex(quad[k].x, quad[k].y, quad[k].z);
            for (int k = 3; k >= 0; --k) faces.vertex(quad[k].x, quad[k].y, quad[k].z);
        }
        translated(screen, offset, [&] { MeshHelpers::renderMeshImmediately(screen, faces, faceMaterial, OffscreenCaptureDescription{}); });
    }
    if (!lineMaterial.mRenderMaterialInfoPtr) return;
    Tessellator lines(screen.tessellator.mBufferResourceService);
    lines.begin({}, mce::PrimitiveMode::LineList, 26, false);
    lines.color(1.f, 1.f, 1.f, 1.f);
    glm::vec3 a{-grow}, b{1 + grow}, c[8];
    for (int i = 0; i < 8; ++i) c[i] = {i & 1 ? b.x : a.x, i & 2 ? b.y : a.y, i & 4 ? b.z : a.z};
    constexpr int edges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    for (auto [i, j] : edges) { lines.vertex(c[i].x, c[i].y, c[i].z); lines.vertex(c[j].x, c[j].y, c[j].z); }
    lines.vertex(.5f, 1.f, .5f);
    lines.vertex(.5f, beam, .5f);
    translated(screen, offset, [&] { MeshHelpers::renderMeshImmediately(screen, lines, lineMaterial, OffscreenCaptureDescription{}); });
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
    std::vector<std::pair<double, Section const*>> order;
    for (auto const& [key, section] : sections) {
        if (!section.blend) continue;
        double cx = section.origin.x + sectionSize / 2. - camera.x, cy = section.origin.y + sectionSize / 2. - camera.y,
               cz = section.origin.z + sectionSize / 2. - camera.z;
        order.push_back({cx * cx + cy * cy + cz * cz, &section});
    }
    std::sort(order.begin(), order.end(), [](auto const& a, auto const& b) { return a.first > b.first; });
    for (auto const& [distance, section] : order) {
        glm::vec3 offset{static_cast<float>(section->origin.x - camera.x), static_cast<float>(section->origin.y - camera.y),
                         static_cast<float>(section->origin.z - camera.z)};
        translated(screen, offset, [&] {
            BrightnessPair full;
            full.sky->mValue = 15;
            full.block->mValue = 15;
            ActorShaderManager::setupShaderParameters(screen, region, full, glm::vec4{1, 1, 1, 1}, 1.f, true, *lightTexture,
                Vec2{1, 1}, Vec4{0, 0, 1, 1});
            section->blend->renderMesh(screen, material, texture, 0, section->blendVertices, OffscreenCaptureDescription{}, nullptr);
        });
    }
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
        drawBlended(context);
    } catch (std::exception const& error) {
        static bool reported = false;
        if (!std::exchange(reported, true)) log(std::string("drawing blended ghosts failed: ") + error.what());
    }
}

LL_TYPE_INSTANCE_HOOK(GhostPass, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$renderEntityEffects, void, BaseActorRenderContext& context) {
    origin(context);
    if (++ghostFrame == 600) log(std::format("block entity alpha pass ran {} times in 600 frames", alphaCalls));
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
        drawPlacements(context, client, *player);
        drawPoint(context.mScreenContext, context.mImpl->mCameraPosition);
        stepSave(player->getDimensionBlockSource(), *player);
        drawSelection(context.mScreenContext, context.mImpl->mCameraPosition, static_cast<int>(player->getDimensionId()));
        drawWaitingColumns(context.mScreenContext, context.mImpl->mCameraPosition);
    } catch (std::exception const& error) {
        static bool reported = false;
        if (!std::exchange(reported, true)) log(std::string("drawing failed: ") + error.what());
    }
}
}

Block const* gameBlock(PaletteBlock const& entry) { return lookup(entry); }
std::optional<Mismatch> mismatchAt(BlockSource& region, Point world) {
    std::shared_ptr<Verification const> result;
    {
        std::lock_guard lock(resultMutex);
        result = published;
    }
    auto snapshot = session::snapshot();
    int index = result->placement;
    if (index < 0 || static_cast<size_t>(index) >= snapshot.placements.size() || static_cast<size_t>(index) >= resolved.size()) return std::nullopt;
    auto const& shown = snapshot.placements[static_cast<size_t>(index)];
    if (!shown.structure || resolved[static_cast<size_t>(index)].structure != shown.structure.get()) return std::nullopt;
    Size placed = placedSize(shown.structure->size, shown.placement.placement.rotation);
    auto const& o = shown.placement.placement.origin;
    int ox = world.x - o.x, oy = world.y - o.y, oz = world.z - o.z;
    if (ox < 0 || oy < 0 || oz < 0 || ox >= placed.x || oy >= placed.y || oz >= placed.z) return std::nullopt;
    auto n = (static_cast<std::uint64_t>(ox) * placed.y + static_cast<std::uint64_t>(oy)) * placed.z + static_cast<std::uint64_t>(oz);
    auto c = classifyCell(region, shown, resolved[static_cast<size_t>(index)], placed, n);
    if (!c.inside) return std::nullopt;
    return mismatchFor(c, resolved[static_cast<size_t>(index)]);
}
BlockLabel blockLabel(PaletteBlock const& entry) {
    auto const* block = lookup(entry);
    auto info = block ? describe(*block, entry.name) : ItemInfo{"", entry.name, ""};
    return {info.name, info.icon};
}
void eachLayer(Block const& block, BlockSource& region, BlockPos const& pos, std::function<void(std::optional<BlockRenderLayer>)> const& visit) {
    // Liquids are drawn as shells (liquidShell), not through this path.
    if (block.getMaterial().mLiquid) { visit(std::nullopt); return; }
    auto const& type = block.getBlockType();
    auto own = type.getRenderLayer(block, region, pos);
    visit(own);
    // Extra layers as a bit per layer (the honey and slime blocks' other cube).
    int extra = type.getExtraRenderLayers();
    for (int layer = 0; layer < static_cast<int>(BlockRenderLayer::RenderlayerCount); ++layer)
        if ((extra >> layer & 1) && layer != static_cast<int>(own)) visit(static_cast<BlockRenderLayer>(layer));
}
void tessellateLayer(BlockTessellator& tessellator, Tessellator& batch, Block const& block, BlockPos const& pos,
                     std::optional<BlockRenderLayer> layer) {
    auto& current = static_cast<int&>(tessellator.mRenderingLayer);
    int was = current;
    if (layer) current = static_cast<int>(*layer);
    // A shape set by the previous block (the honey block's two cubes) must
    // not carry over: a slab drawn next drew full height.
    auto& shapeSet = static_cast<bool&>(tessellator.mCurrentShapeSet);
    shapeSet = false;
    tessellator.tessellateInWorld(batch, block, pos, false);
    shapeSet = false;
    current = was;
}
int liquidKind(Block const& block) {
    if (!block.getMaterial().mLiquid) return 0;
    auto type = block.getMaterial().mType;
    return type == SharedTypes::v1_26_20::MaterialType::Water ? 1 : type == SharedTypes::v1_26_20::MaterialType::Lava ? 2 : 0;
}
int liquidDepth(Block const& block) {
    return liquidKind(block) ? block.getState<int>(VanillaStates::LiquidDepth()).value_or(0) : 0;
}
bool liquidShell(BlockTessellator& tessellator, Tessellator& batch, BlockPos const& pos, Block const& liquid,
                 std::function<bool(int side)> const& open, std::function<float(int cx, int cz)> const& corner) {
    // A white concrete cube tessellated at the cell gives quads with every
    // vertex stream the mesh needs; its faces are then reshaped, retextured
    // with the liquid's texture and recolored. The tessellator already
    // leaves out faces against opaque blocks.
    static auto const* white = [] {
        auto block = Block::tryGetFromRegistry(HashedString{"minecraft:white_concrete"});
        return block ? &*block : nullptr;
    }();
    auto const* cubeGraphics = white ? BlockGraphics::getForBlock(*white) : nullptr;
    auto const* liquidGraphics = BlockGraphics::getForBlock(liquid);
    if (!white || !cubeGraphics || !liquidGraphics) return false;
    int kind = liquidKind(liquid);
    size_t from = batch.mMeshData->mPositions->size();
    tessellateLayer(tessellator, batch, *white, pos, std::nullopt);
    auto& data = static_cast<mce::MeshData&>(batch.mMeshData);
    auto& positions = *data.mPositions;
    auto& uvs = *data.mTextureUVs[0];
    auto& colors = *data.mColors;
    if (colors.size() != positions.size()) colors.resize(positions.size(), 0xffffffffu);
    // Water's texture is gray and tinted by the biome; lava's is colored,
    // and lava is not see-through.
    auto channel = [](float v, int shift) { return static_cast<std::uint32_t>(std::lround(std::clamp(v, 0.f, 1.f) * 255)) << shift; };
    std::uint32_t tint = kind == 1 ? channel(.25f, 0) | channel(.45f, 8) | channel(.95f, 16) | channel(.55f, 24)
                                   : channel(1.f, 0) | channel(1.f, 8) | channel(1.f, 16) | channel(1.f, 24);
    TextureUVCoordinateSet const& cube = cubeGraphics->getTexture(1, 0);
    float heights[2][2];
    for (int cx = 0; cx < 2; ++cx)
        for (int cz = 0; cz < 2; ++cz) heights[cx][cz] = corner(cx, cz);
    // Which way the surface falls (x, z): toward its lower corners.
    glm::vec2 downhill{(heights[0][0] + heights[0][1]) - (heights[1][0] + heights[1][1]),
                       (heights[0][0] + heights[1][0]) - (heights[0][1] + heights[1][1])};
    bool any = false;
    for (size_t q = from; q + 4 <= positions.size(); q += 4) {
        std::array<faces::Vertex, 4> quad;
        for (size_t k = 0; k < 4; ++k) quad[k] = {positions[q + k].x, positions[q + k].y, positions[q + k].z};
        int side = faces::sideOf(quad, pos.x, pos.y, pos.z);
        if (side < 0 || !open(side)) {
            for (size_t k = 1; k < 4; ++k) positions[q + k] = positions[q];
            continue;
        }
        any = true;
        // Texture slots follow the faces: 0 down, 1 up (still), 2-5 the
        // sides (flowing). A sloped top flows: the flowing texture turned
        // downhill, one texture per block (scaled down only as far as a
        // diagonal turn needs to stay inside its atlas cell).
        bool flows = side == 3 && std::hypot(downhill.x, downhill.y) > 1e-4f;
        TextureUVCoordinateSet const& own = liquidGraphics->getTexture(side == 2 ? 0 : side == 3 && !flows ? 1 : 2, 0);
        for (size_t k = 0; k < 4; ++k) {
            // Top vertices go to their corner's height.
            auto& v = positions[q + k];
            float fx = v.x - static_cast<float>(pos.x), fz = v.z - static_cast<float>(pos.z);
            if (v.y > static_cast<float>(pos.y) + .5f) v.y = static_cast<float>(pos.y) + heights[fx > .5f][fz > .5f];
            colors[q + k] = tint;
            if (uvs.size() != positions.size()) continue;
            float u = 0, w = 0;
            if (flows) {
                // The texture's v axis along the flow.
                glm::vec2 along = glm::normalize(downhill), across{-along.y, along.x};
                glm::vec2 offset{fx - .5f, fz - .5f};
                float fit = 1.f / (std::abs(along.x) + std::abs(along.y));
                u = .5f + glm::dot(offset, across) * fit;
                w = .5f + glm::dot(offset, along) * fit;
            } else {
                float du = cube._u1 - cube._u0, dv = cube._v1 - cube._v0;
                u = du != 0 ? (uvs[q + k].x - cube._u0) / du : 0;
                w = dv != 0 ? (uvs[q + k].y - cube._v0) / dv : 0;
            }
            uvs[q + k] = {own._u0 + u * (own._u1 - own._u0), own._v0 + w * (own._v1 - own._v0)};
        }
    }
    static bool logged = false;
    if (!logged) {
        logged = true;
        auto const& still = liquidGraphics->getTexture(1, 0);
        log(std::format("liquid texture u {:.4f}-{:.4f} v {:.4f}-{:.4f} ({}x{}), cube u {:.4f}-{:.4f}", static_cast<float>(still._u0),
            static_cast<float>(still._u1), static_cast<float>(still._v0), static_cast<float>(still._v1),
            static_cast<int>(still._texSizeW), static_cast<int>(still._texSizeH), static_cast<float>(cube._u0), static_cast<float>(cube._u1)));
    }
    return any;
}
void reorderQuads(Tessellator& batch, std::vector<std::uint32_t> const& order) {
    auto& data = static_cast<mce::MeshData&>(batch.mMeshData);
    size_t quads = order.size();
    auto permute = [&](auto& stream) {
        if (stream.size() != quads * 4) return;
        auto copy = stream;
        for (size_t q = 0; q < quads; ++q)
            for (size_t k = 0; k < 4; ++k) stream[q * 4 + k] = copy[order[q] * 4 + k];
    };
    permute(*data.mPositions);
    permute(*data.mNormals);
    permute(*data.mTangents);
    permute(*data.mColors);
    permute(*data.mBoneId0s);
    for (int i = 0; i < 3; ++i) permute(*data.mTextureUVs[i]);
    permute(*data.mPBRTextureIndices);
    permute(*data.mMERS);
    permute(*data.mGeoType);
}
int sidesReached(Block const& block, BlockTessellator& tessellator, ScreenContext& screen, bool blendedToo, float epsilon) {
    static std::map<std::tuple<Block const*, bool, float>, int> known;
    auto key = std::tuple{&block, blendedToo, epsilon};
    if (auto found = known.find(key); found != known.end()) return found->second;
    // Tessellated once above the build limit, where nothing culls it.
    Tessellator scratch(screen.tessellator.mBufferResourceService);
    scratch.begin({}, mce::PrimitiveMode::QuadList, 64, false);
    BlockPos above{0, 2000, 0};
    eachLayer(block, *static_cast<BlockSource*&>(tessellator.mRegion), above, [&](std::optional<BlockRenderLayer> layer) {
        if (blendedToo || !layer || !blended(*layer)) tessellateLayer(tessellator, scratch, block, above, layer);
    });
    auto const& positions = scratch.mMeshData->mPositions.get();
    int sides = 0;
    for (size_t q = 0; q + 4 <= positions.size(); q += 4) {
        std::array<faces::Vertex, 4> quad;
        for (size_t k = 0; k < 4; ++k) quad[k] = {positions[q + k].x, positions[q + k].y, positions[q + k].z};
        if (int side = faces::sideOf(quad, above.x, above.y, above.z, epsilon); side >= 0) sides |= 1 << side;
    }
    scratch.end(Tessellator::UploadMode::Buffered, "Lamium schematic probe", SupplementaryFieldAutoGenerationMode{});
    return known[key] = sides;
}
bool coversNeighbors(Block const& block, BlockTessellator& tessellator, ScreenContext& screen) {
    // Only the layers drawn alpha-tested hide a face, and only where they
    // reach the side: a blended block (honey, whose opaque inner cube stops
    // short of the sides) shows what is behind it.
    return block.getBlockType().mIsOpaqueFullBlock && sidesReached(block, tessellator, screen, false, 1e-3f) == 63;
}
void wantProgress() { progressWanted = steadyMs(); }
std::optional<Tally> progress(SavedPlacement const& placement) {
    std::lock_guard lock(progressMutex);
    auto found = progressDone.find(drawKey(placement));
    if (found == progressDone.end()) return std::nullopt;
    return found->second;
}
std::shared_ptr<Verification const> verification() {
    std::lock_guard lock(resultMutex);
    return published;
}
bool save(SaveRequest request) {
    std::lock_guard lock(saveMutex);
    if (saveBusy) return false;
    saveBusy = true;
    pendingSave = std::move(request);
    return true;
}
std::optional<SaveStatus> saveStatus() {
    std::lock_guard lock(saveMutex);
    return status;
}
void stopSaving() { stopRequested = true; }
std::optional<std::string> takeSaveMessage() {
    std::lock_guard lock(saveMutex);
    return std::exchange(saveMessage, std::nullopt);
}
void point(Point cell) {
    std::lock_guard lock(pointMutex);
    pointAt = cell;
    pointUntil = Clock::now() + std::chrono::seconds(30);
}
void start() {
    if (installed) return;
    installed = GhostPass::hook(true) == 0;
    if (!installed) throw std::runtime_error("Could not install the schematic ghost pass");
    if (GhostBlendPass::hook(true) != 0) log("could not install the blended ghost pass");
    exitListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientExitLevelEvent>(
        [](auto&) { releaseRequested = true; items::forget(); selection::clear(); });
}
void stop() {
    if (exitListener) {
        ll::event::EventBus::getInstance().removeListener(exitListener);
        exitListener.reset();
    }
    GhostBlendPass::unhook(true);
    if (installed && GhostPass::unhook(true)) installed = false;
    releaseRequested = true;
}
}

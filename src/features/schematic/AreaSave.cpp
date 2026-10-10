#include "features/schematic/AreaSave.h"
#include "features/schematic/GhostCommon.h"
#include "features/schematic/GhostRenderer.h"
#include "app/AtomicFile.h"
#include "ui/Localization.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/item/SaveContextFactory.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/chunk/ChunkState.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include "mc/world/level/material/Material.h"
#include "mc/world/phys/AABB.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <format>
#include <map>
#include <mutex>
#include <set>

namespace lamium::schematic::ghosts {
namespace {
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
}

void stopSave() {
    {
        std::lock_guard lock(saveMutex);
        if (!saveBusy) return;
        pendingSave.reset();
    }
    stopRequested = false;
    finishSave(ui::translated("schematic.save.stopped"));
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
WaitingColumns waitingColumns(double x, double z, size_t limit) {
    WaitingColumns out;
    if (!saveJob) return out;
    auto const& job = *saveJob;
    out.height = job.builder.structure().size.y;
    out.lowY = job.request.area.low().y;
    std::vector<std::pair<double, Column const*>> waiting;
    for (size_t c = 0; c < job.columns.size(); ++c) {
        auto const& column = job.columns[c];
        if (job.progress[c] >= column.cells(out.height)) continue;
        double dx = (column.lowX + column.highX + 1) / 2.0 - x, dz = (column.lowZ + column.highZ + 1) / 2.0 - z;
        waiting.push_back({dx * dx + dz * dz, &column});
    }
    std::sort(waiting.begin(), waiting.end(), [](auto const& a, auto const& b) { return a.first < b.first; });
    if (waiting.size() > limit) waiting.resize(limit);
    for (auto const& [distance, column] : waiting) out.nearest.push_back(*column);
    return out;
}
}

#include "features/information/NoteSounds.h"
#include "features/information/NoteEvent.h"
#include "app/Capabilities.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "mc/deps/core/utility/pub_sub/Connector.h"
#include "mc/deps/shared_types/legacy/LevelSoundEvent.h"
#include "mc/sound/SoundEventIdentifier.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/LevelSoundManager.h"
#include <atomic>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <variant>

namespace lamium::information {
namespace {
// The note events for a block cell; only the loaded dimension is keyed, so a
// note remembered before a portal trip can never answer for the other side.
struct Cell {
    int x, y, z, dimension;
    bool operator<(Cell const& other) const {
        return std::tie(x, y, z, dimension) < std::tie(other.x, other.y, other.z, other.dimension);
    }
};
std::mutex mutex;
std::map<Cell, int> notes;
std::unique_ptr<Bedrock::PubSub::Subscription> subscription;
// Read every frame without the lock: attached, or given up on for this world.
std::atomic<bool> attached{false}, failed{false};
std::atomic<int> activeDimension{0};
// One entry per note block played since the world loaded; a long session would
// otherwise grow without bound.
constexpr std::size_t noteLimit = 4096;

void log(std::string_view text) {
    try {
        Runtime::instance().self().getLogger().info("Note sounds: {}", text);
    } catch (...) {}
}
Cell cellAt(Vec3 const& pos) {
    return {static_cast<int>(std::floor(pos.x)), static_cast<int>(std::floor(pos.y)),
            static_cast<int>(std::floor(pos.z)), activeDimension.load()};
}
void onSound(SoundEventIdentifier const& id, Vec3 const& pos, int data, ActorSoundIdentifier const&, bool) {
    if (!versionSensitiveAllowed(capabilities::Id::NoteBlockSounds)) return;
    auto const* sound = std::get_if<SharedTypes::Legacy::LevelSoundEvent>(&id.mSound.get());
    if (!sound) return;
    // A block placed in the cell is a new block entity with its own note.
    if (*sound == SharedTypes::Legacy::LevelSoundEvent::Place) {
        std::scoped_lock guard(mutex);
        notes.erase(cellAt(pos));
        return;
    }
    if (*sound != SharedTypes::Legacy::LevelSoundEvent::Note) return;
    // The data carries the instrument above the note; the pitch is the low byte.
    auto note = noteFromEventData(data);
    if (!note) return;
    std::scoped_lock guard(mutex);
    if (notes.size() >= noteLimit) notes.clear();
    notes[cellAt(pos)] = *note;
}
}

std::optional<int> playedNote(BlockPos const& pos, int dimension) {
    if (!versionSensitiveAllowed(capabilities::Id::NoteBlockSounds)) return std::nullopt;
    std::scoped_lock lock(mutex);
    auto found = notes.find(Cell{pos.x, pos.y, pos.z, dimension});
    if (found == notes.end()) return std::nullopt;
    return found->second;
}

void beginNoteSounds(Level& level, int dimension) {
    if (activeDimension.exchange(dimension) != dimension) {
        // Events heard before this frame saw the change carry the old id.
        std::scoped_lock lock(mutex);
        notes.clear();
    }
    if (attached || failed) return;
    // A version the capability does not trust never joins.
    if (capabilities::registry().status(capabilities::Id::NoteBlockSounds).compatibility
        != capabilities::Reason::None)
        return;
    std::scoped_lock lock(mutex);
    capabilities::begin(capabilities::Id::NoteBlockSounds);
    try {
        auto& connector = level.getLevelSoundManager().getOnLevelSoundEventConnector();
        subscription.reset(new Bedrock::PubSub::Subscription(
            connector.connect(onSound, Bedrock::PubSub::ConnectPosition::AtBack, nullptr)));
        attached = true;
        capabilities::hooks(capabilities::Id::NoteBlockSounds, true);
        log("joined the level's sound events");
    } catch (...) {
        // Once per world: retrying every frame would only repeat the failure.
        subscription.reset();
        failed = true;
        capabilities::hooks(capabilities::Id::NoteBlockSounds, false);
        log("could not join the level's sound events; the card keeps the loaded note");
    }
}

void endNoteSounds() {
    if (!attached) return;
    std::scoped_lock lock(mutex);
    // The subscription holds its connection weakly, so disconnecting is safe
    // even after the level that published it is gone.
    subscription.reset();
    attached = false;
    notes.clear();
}

void forgetNoteSounds() {
    endNoteSounds();
    std::scoped_lock lock(mutex);
    notes.clear();
    failed = false;
}
}

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
Bedrock::PubSub::Subscription* subscription = nullptr;
std::atomic<int> activeDimension{0};
// One entry per note block played since the world loaded; a long session would
// otherwise grow without bound.
constexpr std::size_t noteLimit = 4096;

void log(std::string_view text) {
    try {
        Runtime::instance().self().getLogger().info("Note sounds: {}", text);
    } catch (...) {}
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
    activeDimension = dimension;
    std::scoped_lock lock(mutex);
    if (subscription) return;
    capabilities::begin(capabilities::Id::NoteBlockSounds);
    try {
        auto& connector = level.getLevelSoundManager().getOnLevelSoundEventConnector();
        subscription = new Bedrock::PubSub::Subscription(connector.connect(
            [](SoundEventIdentifier const& id, Vec3 const& pos, int data, ActorSoundIdentifier const&, bool) {
                // The data carries the instrument above the note; the pitch is
                // the low byte, and the high byte tells us it is a note block.
                auto note = noteFromEventData(data);
                if (!note) return;
                auto const* sound = std::get_if<SharedTypes::Legacy::LevelSoundEvent>(&id.mSound.get());
                if (!sound || *sound != SharedTypes::Legacy::LevelSoundEvent::Note) return;
                std::scoped_lock guard(mutex);
                if (notes.size() >= noteLimit) notes.clear();
                notes[Cell{static_cast<int>(std::floor(pos.x)), static_cast<int>(std::floor(pos.y)),
                           static_cast<int>(std::floor(pos.z)), activeDimension.load()}] = *note;
            },
            Bedrock::PubSub::ConnectPosition::AtBack, nullptr));
        capabilities::hooks(capabilities::Id::NoteBlockSounds, subscription != nullptr);
        log("joined the level's sound events");
    } catch (...) {
        subscription = nullptr;
        capabilities::hooks(capabilities::Id::NoteBlockSounds, false);
        log("could not join the level's sound events; the card keeps the loaded note");
    }
}

void forgetNoteSounds() {
    std::scoped_lock lock(mutex);
    notes.clear();
    // The subscription belongs to the level that is being destroyed, so it is
    // dropped rather than unsubscribed; the next world attaches its own.
    subscription = nullptr;
}
}

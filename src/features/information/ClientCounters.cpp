#include "features/information/ClientCounters.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRenderer.h"
#include "mc/client/particle/ParticleEngine.h"
#include "mc/client/particlesystem/particle/ParticleSystemEngine.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/chunk/ChunkSource.h"
#include "mc/world/actor/ActorCategory.h"
#include "mc/world/actor/ActorType.h"
#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"
#include <unordered_map>
#include <chrono>
#include <limits>

namespace lamium::information {
namespace {
using Clock = std::chrono::steady_clock;
ClientCounters cached;
Clock::time_point lastRead{};
// Localized type names by identifier, kept until the language changes.
std::unordered_map<std::string, std::string> names;
std::string namesLanguage;
std::string typeName(Actor const& actor, std::string const& id) {
    auto language = getI18n().getCurrentLanguage();
    std::string code = language->getFullLanguageCode();
    if (code != namesLanguage) {
        names.clear();
        namesLanguage = code;
    }
    if (auto found = names.find(id); found != names.end()) return found->second;
    auto key = actor.getEntityLocNameString();
    auto name = getI18n().get(key, language);
    if (name == key) name.clear();
    return names.emplace(id, name).first->second;
}
int clamped(std::uint64_t value) {
    return static_cast<int>(std::min<std::uint64_t>(value, std::numeric_limits<int>::max()));
}
ClientCounters read(IClientInstance& client) {
    ClientCounters out;
    auto* player = client.getLocalPlayer();
    if (!player) return out;
    try {
        auto const& dimension = player->getDimension();
        int count = 0;
        EntityKinds kinds{};
        std::unordered_map<std::string, TypeCount> types;
        for (auto* actor : player->getLevel().getRuntimeActorList()) {
            if (!actor || &actor->getDimension() != &dimension) continue;
            ++count;
            auto kind = entityKind(actor->isPlayer(), actor->hasType(ActorType::ItemEntity),
                                   actor->hasCategory(ActorCategory::Monster), actor->hasCategory(ActorCategory::Mob));
            ++kinds[static_cast<size_t>(kind)];
            auto const& id = actor->getTypeName();
            auto& type = types[id];
            if (!type.count) {
                type.id = id;
                type.name = typeName(*actor, id);
            }
            ++type.count;
        }
        out.entities = count;
        out.entityKinds = kinds;
        for (auto& [id, type] : types) out.entityTypes.push_back(std::move(type));
    } catch (...) {}
    try {
        // The size only: the map is filled by loading threads, so it is never walked here.
        out.chunks = clamped(player->getDimension().getChunkSource().getStorage().size());
    } catch (...) {}
    try {
        if (auto* renderer = client.getLevelRenderer()) {
            std::uint64_t total = 0;
            auto& engine = renderer->getParticleEngine();
            for (auto n : static_cast<uint const(&)[105]>(engine.particleCount)) total += n;
            if (auto systems = static_cast<Bedrock::NonOwnerPointer<ParticleSystemEngine> const&>(renderer->mParticleSystemEngine))
                total += static_cast<std::uint64_t const&>(systems->mTotalParticleCount);
            out.particles = clamped(total);
        }
    } catch (...) {}
    return out;
}
}
ClientCounters clientCounters(IClientInstance& client) {
    auto now = Clock::now();
    if (now - lastRead >= std::chrono::seconds(1)) {
        cached = read(client);
        lastRead = now;
    }
    return cached;
}
}

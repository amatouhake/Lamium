#include "features/information/SlimeTrace.h"
#ifdef LAMIUM_SLIME_TRACE
#include "app/TraceLog.h"
#include "features/information/SlimeChunk.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/biome/SlimeSpawnRules.h"
#include "mc/world/level/biome/SpawnConditions.h"
#include <atomic>
#include <climits>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>

namespace lamium::information::slimeTrace {
namespace {
// Two checks. Passive: every call the game makes (a local world's spawner
// runs in this process) against the rule for that position's chunk; a true
// answer outside a rule chunk disproves the rule. Active: the chunks loaded
// around the player asked at a set of heights, which also shows the height
// limit.
TraceBudget callBudget, mismatchBudget, summaryBudget, probeBudget;
std::atomic<unsigned> calls{0}, answeredTrue{0}, trueOutside{0}, falseInside{0};
std::atomic<int> highestTrue{INT_MIN}, lowestTrue{INT_MAX}, lowestFalseInside{INT_MAX};
thread_local bool probing = false;
bool hooked = false;
ll::event::ListenerPtr tickListener;
unsigned ticks = 0, probes = 0;
std::optional<std::pair<int, int>> lastProbe;
int chunkOf(int block) { return static_cast<int>(std::floor(block / 16.0)); }
void lower(std::atomic<int>& slot, int value) {
    int seen = slot.load();
    while (value < seen && !slot.compare_exchange_weak(seen, value)) {}
}
void raise(std::atomic<int>& slot, int value) {
    int seen = slot.load();
    while (value > seen && !slot.compare_exchange_weak(seen, value)) {}
}
LL_STATIC_HOOK(SlimeRuleHook, ll::memory::HookPriority::Normal, &SlimeSpawnRules::checkSpawnRulesCallback, bool,
    SpawnConditions const& conditions, BlockSource& region) {
    bool result = origin(conditions, region);
    if (probing) return result;
    try {
        auto const& pos = static_cast<BlockPos const&>(conditions.pos);
        int cx = chunkOf(pos.x), cz = chunkOf(pos.z);
        bool rule = isSlimeChunk(cx, cz);
        auto n = ++calls;
        if (result) {
            ++answeredTrue;
            raise(highestTrue, pos.y);
            lower(lowestTrue, pos.y);
        }
        if (result && !rule) {
            ++trueOutside;
            traceLog(mismatchBudget, 60, "L-232 MISMATCH game=true rule=false at ({} {} {}) chunk {} {}", pos.x, pos.y, pos.z, cx, cz);
        }
        if (!result && rule) {
            ++falseInside;
            lower(lowestFalseInside, pos.y);
        }
        traceLog(callBudget, 80, "L-232 call #{} at ({} {} {}) chunk {} {} game={} rule={} surface={} underground={} light={}", n,
            pos.x, pos.y, pos.z, cx, cz, result, rule, static_cast<bool>(conditions.isOnSurface),
            static_cast<bool>(conditions.isUnderground), static_cast<int>(conditions.rawBrightness));
    } catch (...) {}
    return result;
}
// Every loaded chunk within four of the player, at fixed heights: the game's
// answer per height next to the rule.
void probe(LocalPlayer& player) {
    auto& region = player.getDimensionBlockSource();
    auto const& feet = player.getPosition();
    int px = chunkOf(static_cast<int>(std::floor(feet.x))), pz = chunkOf(static_cast<int>(std::floor(feet.z)));
    if (lastProbe == std::pair{px, pz} || probes >= 40) return;
    lastProbe = {px, pz};
    ++probes;
    constexpr int heights[] = {-63, -40, -20, 0, 20, 30, 38, 39, 40, 41, 45, 50, 63, 64, 100, 200};
    int checked = 0, agree = 0, disagree = 0;
    std::string ruleChunks, limits;
    probing = true;
    for (int dz = -4; dz <= 4; ++dz)
        for (int dx = -4; dx <= 4; ++dx) {
            int cx = px + dx, cz = pz + dz;
            BlockPos center{cx * 16 + 8, 0, cz * 16 + 8};
            if (!region.getChunkAt(center)) continue;
            bool rule = isSlimeChunk(cx, cz);
            std::string answers;
            bool any = false;
            int top = INT_MIN;
            for (int y : heights) {
                SpawnConditions conditions{};
                conditions.isUnderground = true;
                conditions.rawBrightness = 0;
                conditions.pos = BlockPos{cx * 16 + 8, y, cz * 16 + 8};
                bool game = SlimeSpawnRules::checkSpawnRulesCallback(conditions, region);
                answers += game ? '1' : '0';
                if (game) { any = true; top = y; }
            }
            ++checked;
            // A chunk agrees when the game answers yes at some height exactly
            // where the rule says slime.
            if (any == rule) ++agree;
            else {
                ++disagree;
                traceLog(mismatchBudget, 60, "L-232 PROBE MISMATCH chunk {} {} rule={} answers={}", cx, cz, rule, answers);
            }
            if (rule) {
                ruleChunks += std::format(" ({},{})", cx, cz);
                limits += std::format(" ({},{}):{}:{}", cx, cz, answers, top == INT_MIN ? std::string{"none"} : std::to_string(top));
            }
        }
    probing = false;
    traceLog(probeBudget, 80, "L-232 probe #{} around chunk {} {}: loaded {} agree {} disagree {}; rule chunks{}", probes, px, pz,
        checked, agree, disagree, ruleChunks.empty() ? std::string{" none"} : ruleChunks);
    traceLog(probeBudget, 80, "L-232 probe #{} heights {{-63 -40 -20 0 20 30 38 39 40 41 45 50 63 64 100 200}} per rule chunk (answers:highest yes){}",
        probes, limits.empty() ? std::string{" none"} : limits);
}
void tick() noexcept {
    try {
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        if (!player) return;
        if (++ticks % 100 == 0 && static_cast<int>(player->getDimensionId()) == 0) probe(*player);
        if (ticks % 600 == 0) {
            auto show = [](int v) { return v == INT_MIN || v == INT_MAX ? std::string{"-"} : std::to_string(v); };
            traceLog(summaryBudget, 200, "L-232 summary: calls {} true {} trueOutsideRule {} falseInsideRule {} trueY {}..{} lowestFalseInsideY {}",
                calls.load(), answeredTrue.load(), trueOutside.load(), falseInside.load(), show(lowestTrue.load()),
                show(highestTrue.load()), show(lowestFalseInside.load()));
        }
    } catch (std::exception const& error) {
        probing = false;
        traceLog(probeBudget, 80, "L-232 probe failed: {}", error.what());
    } catch (...) { probing = false; }
}
}
void start() {
    if (SlimeRuleHook::hook(true) != 0) throw std::runtime_error("Could not install L-232 diagnostics");
    hooked = true;
    tickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientLevelTickEvent>([](auto&) { tick(); });
    Runtime::instance().self().getLogger().warn("L-232 slime chunk diagnostics enabled");
}
void stop() {
    if (tickListener) { ll::event::EventBus::getInstance().removeListener(tickListener); tickListener.reset(); }
    if (hooked) { SlimeRuleHook::unhook(true); hooked = false; }
    lastProbe.reset();
}
}
#else
namespace lamium::information::slimeTrace { void start() {} void stop() {} }
#endif

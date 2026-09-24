#include "features/information/InfoLines.h"
#include "settings/SettingsStore.h"
#include <limits>
void check(bool, char const*);
void infoLinesTests() {
    using namespace lamium;
    using namespace lamium::information;
    check(mergeLineOrder({}) == defaultLineOrder(), "empty order falls back to every line");
    check(mergeLineOrder({"ping", "coordinates"}).front() == "ping"
          && mergeLineOrder({"ping", "coordinates"})[1] == "coordinates"
          && mergeLineOrder({"ping", "coordinates"}).size() == infoLineIds.size(),
          "stored order wins and missing lines append");
    auto cleaned = mergeLineOrder({"nope", "fps", "fps"});
    check(cleaned.front() == "fps" && cleaned.size() == infoLineIds.size()
          && std::find(cleaned.begin(), cleaned.end(), "nope") == cleaned.end(),
          "unknown ids drop out and duplicates collapse");
    check(decodeSettings("{}").information.lineOrder == defaultLineOrder(), "fresh settings list every line");
    auto stored = decodeSettings(R"({"information":{"lineOrder":["ping","nope"]}})");
    check(stored.information.lineOrder.front() == "ping" && stored.information.lineOrder.size() == infoLineIds.size(),
          "stored line order loads and merges");
    check(formatRotation(178.44f, -12.34f) == "178.4 / -12.3", "rotation keeps one decimal");
    auto chunk = chunkPosition(101.3, -32.7);
    check(chunk.chunkX == 6 && chunk.chunkZ == -3 && chunk.inX == 5 && chunk.inZ == 15
          && formatChunk(chunk) == "6, -3 (5, 15)", "chunk and in-chunk position handle negatives");
    check(dayCount(0) == 0 && dayCount(24000) == 1 && dayTicks(24000 + 6000) == 6000
          && formatClock(0) == "06:00" && formatClock(6000) == "12:00" && formatClock(12000) == "18:00"
          && formatClock(18000) == "00:00" && formatClock(24000) == "06:00",
          "day count and clock follow total world ticks");
    check(moonPhase(0) == 0 && moonPhase(7 * 24000) == 7 && moonPhase(8 * 24000) == 0
          && moonPhaseKey(0) == "moon.full" && moonPhaseKey(7) == "moon.waxingGibbous",
          "moon phase cycles eight days from the full moon");
    check(formatSpeed(4.26) == "4.3", "speed keeps one decimal");
    SpeedSampler speed;
    speed.sample(0, 64, 0, 10.0);
    speed.sample(1, 64, 0, 10.2);
    check(!speed.read(), "speed needs half a second of movement");
    speed.sample(4.3, 64, 0, 11.0);
    auto blocksPerSecond = speed.read();
    check(blocksPerSecond && *blocksPerSecond > 4.2 && *blocksPerSecond < 4.4, "speed averages position deltas");
    speed.sample(500, 64, 0, 11.5);
    check(!speed.read(), "teleports restart the window");
    speed.sample(500, 64, 0, 11.6);
    speed.sample(501, 64, 0, 14.0);
    check(!speed.read(), "stalls restart the window");
    speed.sample(0, 64, 0, std::numeric_limits<double>::quiet_NaN());
    check(!speed.read(), "invalid input restarts the window");
}

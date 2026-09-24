#include "features/information/InfoLines.h"
#include "settings/SettingsStore.h"
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
}

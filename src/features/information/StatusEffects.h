#pragma once
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

// Status Effects HUD (BACKLOG L-147, docs/demos/status-effects-hud.html).
// Pure ordering and formatting; InfoHud reads the local player's effects.
namespace lamium::information::statusEffects {
struct Effect {
    unsigned id = 0;
    int ticks = 0;      // Remaining; negative for an infinite effect
    int amplifier = 0;  // 0 is level I
    bool ambient = false; // From a beacon or conduit: renewed every few seconds
    bool harmful = false;
    std::string name;
};
inline bool infinite(Effect const& effect) { return effect.ticks < 0; }
// Under 10 s, colored only (no blinking). Beacon effects are refreshed with
// short times and would always look like they are running out.
inline constexpr int runningOutTicks = 200;
inline bool runningOut(Effect const& effect) {
    return !effect.ambient && !infinite(effect) && effect.ticks < runningOutTicks;
}
// Shortest first so the next to run out is on top; infinite ones last.
inline bool earlier(Effect const& a, Effect const& b) {
    if (infinite(a) != infinite(b)) return infinite(b);
    if (a.ticks != b.ticks) return a.ticks < b.ticks;
    return a.id < b.id;
}
// The HUD's rows for this frame (L-247). A row keeps the place it entered with
// until its effect ends, so a beacon renewing the duration does not move it. A
// new effect enters above the first row with a longer time; an ended effect's
// row closes up. `previous` is the last frame's row ids, top to bottom.
// Pure on purpose (the L-247 decision), so its few short vectors are rebuilt
// per frame: a handful of effects, under the caller's own per-row strings and
// width measurements.
inline std::vector<Effect> arrange(std::vector<unsigned> const& previous, std::vector<Effect> const& current) {
    std::vector<Effect> rows;
    rows.reserve(current.size());
    for (unsigned id : previous) {
        bool placed = std::any_of(rows.begin(), rows.end(), [id](Effect const& row) { return row.id == id; });
        auto effect = std::find_if(current.begin(), current.end(), [id](Effect const& e) { return e.id == id; });
        if (!placed && effect != current.end()) rows.push_back(*effect);
    }
    std::vector<Effect> entering;
    for (auto const& effect : current)
        if (std::find(previous.begin(), previous.end(), effect.id) == previous.end()) entering.push_back(effect);
    std::stable_sort(entering.begin(), entering.end(), earlier);
    for (auto const& effect : entering) {
        auto at = std::find_if(rows.begin(), rows.end(), [&](Effect const& row) { return earlier(effect, row); });
        rows.insert(at, effect);
    }
    return rows;
}
// `arrange`'s rows as the ids to hand back as `previous` next frame.
inline std::vector<unsigned> rowIds(std::vector<Effect> const& rows) {
    std::vector<unsigned> ids;
    ids.reserve(rows.size());
    for (auto const& row : rows) ids.push_back(row.id);
    return ids;
}
// Whether a frame that draws no rows ends the row order (L-247). Only a read
// that succeeded does: an unreadable frame must keep the rows, and the layout
// editor's samples never set them.
inline bool endsRowOrder(bool readSucceeded, bool preview) { return readSucceeded && !preview; }
// M:SS, H:MM:SS from an hour, rounded up so an active effect never reads 0:00.
// Empty for an infinite effect: the caller writes a translated word (the
// game font has no infinity sign in Japanese, checked 2026-10-12).
inline std::string timeText(int ticks) {
    if (ticks < 0) return {};
    int seconds = (ticks + 19) / 20, minutes = seconds / 60, hours = minutes / 60;
    auto two = [](int value) { return (value < 10 ? "0" : "") + std::to_string(value); };
    if (hours > 0) return std::to_string(hours) + ":" + two(minutes % 60) + ":" + two(seconds % 60);
    return std::to_string(minutes) + ":" + two(seconds % 60);
}
// Every level in Arabic numerals, 1 included, in a column of their own
// (Roman to X then numbers looked mixed, maintainer 2026-10-12).
inline std::string levelText(int amplifier) { return std::to_string(std::max(amplifier, 0) + 1); }
// The vanilla pack's effect icons, textures/ui/<name>_effect, by effect id
// (MobEffectIds). The effect's own icon name drew nothing (2026-10-12).
// Instant health and damage and saturation have no icon.
inline std::string iconPath(unsigned id) {
    static constexpr std::string_view names[]{
        "", "speed", "slowness", "haste", "mining_fatigue", "strength", "", "", "jump_boost", "nausea",
        "regeneration", "resistance", "fire_resistance", "water_breathing", "invisibility", "blindness",
        "night_vision", "hunger", "weakness", "poison", "wither", "health_boost", "absorption", "",
        "levitation", "poison", "conduit_power", "slow_falling", "bad_omen", "village_hero", "darkness",
        "trial_omen", "wind_charged", "weaving", "oozing", "infested", "raid_omen", "breath_of_the_nautilus"};
    if (id >= std::size(names) || names[id].empty()) return {};
    return "textures/ui/" + std::string(names[id]) + "_effect";
}
}

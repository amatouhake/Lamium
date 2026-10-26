#pragma once
#include <array>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// What the check may leave out (L-262): a table of rules, each in a
// category the settings turn on. A state rule stops one state from being
// compared; a name rule makes two block names the same block. A new
// category or a missed state is a table row, not new code. Names and states
// are the game's (VanillaBlockTypeIds.h, VanillaStates.h, SDK 26.51.8).
namespace lamium::schematic::rules {
enum Category : unsigned { Signal = 1, Lit = 2, Growth = 4 };

struct StateRule { std::string_view state; Category category; };
inline constexpr std::array<StateRule, 17> stateRules{{
    // Redstone power the circuit sets, not the builder.
    {"redstone_signal", Signal}, {"powered_bit", Signal}, {"button_pressed_bit", Signal},
    {"rail_data_bit", Signal}, {"output_lit_bit", Signal}, {"triggered_bit", Signal},
    {"sculk_sensor_phase", Signal},
    // How far a plant has grown.
    {"growth", Growth}, {"age", Growth}, {"age_bit", Growth}, {"kelp_age", Growth},
    {"growing_plant_age", Growth}, {"twisting_vines_age", Growth}, {"weeping_vines_age", Growth},
    {"propagule_stage", Growth}, {"bamboo_leaf_size", Growth}, {"bamboo_stalk_thickness", Growth},
}};
// The first name is the one both compare as.
struct NameRule { std::string_view name, other; Category category; };
inline constexpr std::array<NameRule, 4> nameRules{{
    {"minecraft:redstone_lamp", "minecraft:lit_redstone_lamp", Lit},
    {"minecraft:unpowered_repeater", "minecraft:powered_repeater", Lit},
    {"minecraft:unpowered_comparator", "minecraft:powered_comparator", Lit},
    {"minecraft:redstone_torch", "minecraft:unlit_redstone_torch", Lit},
}};

inline bool ignoredState(std::string_view state, unsigned on) {
    for (auto const& r : stateRules)
        if ((on & r.category) && r.state == state) return true;
    return false;
}
inline std::string_view canonicalName(std::string_view name, unsigned on) {
    for (auto const& r : nameRules)
        if ((on & r.category) && (r.name == name || r.other == name)) return r.name;
    return name;
}
// Same kind of block once the name rules apply.
inline bool sameType(std::string_view expected, std::string_view actual, unsigned on) {
    return canonicalName(expected, on) == canonicalName(actual, on);
}
// Same block once the rules apply: same kind and every compared state equal.
// A state only one side has still counts.
inline bool sameBlock(std::string_view expectedName, std::map<std::string, std::string> const& expected,
                      std::string_view actualName, std::map<std::string, std::string> const& actual, unsigned on) {
    if (!sameType(expectedName, actualName, on)) return false;
    auto compared = [&](std::map<std::string, std::string> const& a, std::map<std::string, std::string> const& b) {
        for (auto const& [key, value] : a) {
            if (ignoredState(key, on)) continue;
            auto found = b.find(key);
            if (found == b.end() || found->second != value) return false;
        }
        return true;
    };
    return compared(expected, actual) && compared(actual, expected);
}
}

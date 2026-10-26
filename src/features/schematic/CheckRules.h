#pragma once
#include <array>
#include <optional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// How the check may relax (L-262): a table of rules, each in a category the
// settings turn on. A state rule stops one state from being compared. A
// name rule makes a block's off and on forms one kind of block, so the
// other form is a state mistake (yellow) instead of a wrong block (red);
// they never pass as the same block (maintainer 2026-10-26). A new category
// or a missed state is a table row, not new code. Names and states are the
// game's (VanillaBlockTypeIds.h, VanillaStates.h, SDK 26.51.8).
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
// `state`: what the difference is called, as the block's state in Java
// Edition, where the two forms are one block (maintainer 2026-10-26).
struct NameRule { std::string_view off, on, state; Category category; };
inline constexpr std::array<NameRule, 4> nameRules{{
    {"minecraft:redstone_lamp", "minecraft:lit_redstone_lamp", "lit", Lit},
    {"minecraft:unpowered_repeater", "minecraft:powered_repeater", "powered", Lit},
    {"minecraft:unpowered_comparator", "minecraft:powered_comparator", "powered", Lit},
    {"minecraft:unlit_redstone_torch", "minecraft:redstone_torch", "lit", Lit},
}};

inline bool ignoredState(std::string_view state, unsigned on) {
    for (auto const& r : stateRules)
        if ((on & r.category) && r.state == state) return true;
    return false;
}
inline std::string_view canonicalName(std::string_view name, unsigned on) {
    for (auto const& r : nameRules)
        if ((on & r.category) && (r.off == name || r.on == name)) return r.off;
    return name;
}
// For a name in a name rule's pair: whether it is the on form, and the
// state that names the difference; nothing for other names.
struct Switched { bool on; std::string_view state; };
inline std::optional<Switched> switchedOn(std::string_view name) {
    for (auto const& r : nameRules) {
        if (r.on == name) return Switched{true, r.state};
        if (r.off == name) return Switched{false, r.state};
    }
    return std::nullopt;
}
// Same kind of block once the name rules apply.
inline bool sameType(std::string_view expected, std::string_view actual, unsigned on) {
    return canonicalName(expected, on) == canonicalName(actual, on);
}
// Same block once the state rules apply: the same name and every compared
// state equal. A state only one side has still counts.
inline bool sameBlock(std::string_view expectedName, std::map<std::string, std::string> const& expected,
                      std::string_view actualName, std::map<std::string, std::string> const& actual, unsigned on) {
    if (expectedName != actualName) return false;
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

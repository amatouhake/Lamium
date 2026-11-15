#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Held-item durability HUD (BACKLOG L-61, docs/demos/durability-hud.html).
// Pure row selection and formatting; InfoHud samples the stacks each frame.
namespace lamium::information::durability {
enum class Look { BarAndNumber, Number, Bar };
enum class Slot { MainHand, Offhand, Head, Chest, Legs, Feet };
// Items whose row also counts ammo (L-150).
enum class Weapon { Other, Bow, Crossbow, Elytra };
inline Weapon weaponOf(std::string_view name) {
    if (name == "minecraft:bow") return Weapon::Bow;
    if (name == "minecraft:crossbow") return Weapon::Crossbow;
    if (name == "minecraft:elytra") return Weapon::Elytra;
    return Weapon::Other;
}
struct Sample {
    bool damageable = false;
    int damage = 0;
    int max = 0;
    Weapon weapon = Weapon::Other;
};
// Samples in Slot order.
using Samples = std::array<Sample, 6>;
struct Row {
    Slot slot;
    int remaining;
    int max;
    float ratio() const { return max > 0 ? std::clamp(static_cast<float>(remaining) / max, 0.f, 1.f) : 1.f; }
};
inline bool usable(Sample const& sample) { return sample.damageable && sample.max > 0; }
inline Row row(Slot slot, Sample const& sample) {
    return {slot, sample.max - std::clamp(sample.damage, 0, sample.max), sample.max};
}
// A worn elytra has its own switch, apart from the armor pieces, and keeps
// the chest position (L-150, maintainer 2026-10-12). The main hand has a
// switch too (L-349), last so older calls keep their meaning.
inline std::vector<Row> rows(Samples const& samples, bool offhand, bool armor, bool elytra = true, bool mainHand = true) {
    std::vector<Row> out;
    auto add = [&](Slot slot) {
        auto const& sample = samples[static_cast<size_t>(slot)];
        if (usable(sample)) out.push_back(row(slot, sample));
    };
    if (mainHand) add(Slot::MainHand);
    if (offhand) add(Slot::Offhand);
    bool wings = samples[static_cast<size_t>(Slot::Chest)].weapon == Weapon::Elytra;
    for (auto slot : {Slot::Head, Slot::Chest, Slot::Legs, Slot::Feet})
        if (slot == Slot::Chest && wings ? elytra : armor) add(slot);
    return out;
}
// ---- Ammo beside a bow, crossbow or elytra row (L-150) ----
enum class Ammo { Arrows, Fireworks };
// Tipped arrows share the arrow item and count with plain ones.
inline std::vector<Ammo> ammoFor(Weapon weapon) {
    switch (weapon) {
    case Weapon::Bow: return {Ammo::Arrows};
    case Weapon::Crossbow: return {Ammo::Arrows, Ammo::Fireworks};
    case Weapon::Elytra: return {Ammo::Fireworks};
    default: return {};
    }
}
// Totals over the stacks the client sees (inventory and offhand); a loaded
// crossbow's charge is shown by its mark, not counted.
struct AmmoCounts {
    int arrows = 0;
    int fireworks = 0;
    void add(std::string_view name, int count) {
        if (count <= 0) return;
        if (name == "minecraft:arrow") arrows += count;
        else if (name == "minecraft:firework_rocket") fireworks += count;
    }
    int of(Ammo ammo) const { return ammo == Ammo::Arrows ? arrows : fireworks; }
};
inline bool showsBar(Look look) { return look != Look::Number; }
// Bar look shows the number only once a quarter or less remains.
inline bool showsNumber(Look look, Row const& row) { return look != Look::Bar || row.ratio() < .25f; }
inline std::string numberText(Look look, Row const& row) {
    if (look == Look::Bar) return std::to_string(row.remaining);
    return std::to_string(row.remaining) + "/" + std::to_string(row.max);
}
// Bar fill width in GUI units, rounded like the vanilla slot bar.
inline float barFill(Row const& row, float width) {
    return std::clamp(std::round(row.ratio() * width), 0.f, width);
}
}

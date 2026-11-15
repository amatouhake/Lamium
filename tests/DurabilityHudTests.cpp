#include "features/information/DurabilityHud.h"
#include "settings/SettingsStore.h"
#include "settings/Options.h"
void check(bool, char const*);
void durabilityHudTests() {
    using namespace lamium::information::durability;
    Sample pick{true, 373, 1561}, shield{true, 35, 336}, helmet{true, 11, 363}, chestplate{true, 9, 528},
        elytra{true, 59, 432}, legs{true, 15, 495}, boots{true, 334, 429}, dirt{};
    auto slots = [](std::vector<Row> const& rows) {
        std::vector<Slot> out;
        for (auto const& row : rows) out.push_back(row.slot);
        return out;
    };
    Samples worn{pick, shield, helmet, chestplate, legs, boots};
    check(slots(rows(worn, false, false)) == std::vector{Slot::MainHand},
          "the default shows only the damageable held item");
    check(rows(Samples{dirt, shield, helmet}, false, false).empty(),
          "a non-damageable held item draws nothing without options");
    check(slots(rows(worn, true, true))
              == std::vector{Slot::MainHand, Slot::Offhand, Slot::Head, Slot::Chest, Slot::Legs, Slot::Feet},
          "rows follow hand, offhand, head, chest, legs, feet");
    check(slots(rows(Samples{dirt, dirt, helmet, dirt, dirt, boots}, false, true))
              == std::vector{Slot::Head, Slot::Feet}, "empty or non-damageable armor slots are skipped");
    Samples wearingElytra{pick, shield, helmet, elytra, legs, boots};
    check(slots(rows(wearingElytra, false, false)) == std::vector{Slot::MainHand},
          "a worn elytra gets no row of its own without the armor option");
    check(slots(rows(wearingElytra, false, true))
              == std::vector{Slot::MainHand, Slot::Head, Slot::Chest, Slot::Legs, Slot::Feet},
          "a worn elytra is an ordinary chest row among the armor rows");
    Sample wings{true, 59, 432, Weapon::Elytra};
    Samples flying{pick, shield, helmet, wings, legs, boots};
    check(slots(rows(flying, false, false, true)) == std::vector{Slot::MainHand, Slot::Chest},
          "the elytra switch shows a worn elytra with armor rows off");
    check(slots(rows(flying, false, true, false)) == std::vector{Slot::MainHand, Slot::Head, Slot::Legs, Slot::Feet},
          "armor rows leave the elytra to its own switch");
    check(slots(rows(flying, false, true, true))
              == std::vector{Slot::MainHand, Slot::Head, Slot::Chest, Slot::Legs, Slot::Feet},
          "with both on the elytra keeps the chest position");
    check(slots(rows(worn, false, false, true)) == std::vector{Slot::MainHand},
          "the elytra switch adds no row for a chestplate");
    check(rows(worn, false, false, true, false).empty()
              && slots(rows(worn, true, true, true, false)) == std::vector{Slot::Offhand, Slot::Head, Slot::Chest, Slot::Legs, Slot::Feet},
          "the main hand switch off drops only the held item's row (L-349)");

    check(weaponOf("minecraft:bow") == Weapon::Bow && weaponOf("minecraft:crossbow") == Weapon::Crossbow
              && weaponOf("minecraft:elytra") == Weapon::Elytra && weaponOf("minecraft:trident") == Weapon::Other,
          "bows, crossbows and elytra are told apart by name");
    check(ammoFor(Weapon::Bow) == std::vector{Ammo::Arrows}
              && ammoFor(Weapon::Crossbow) == std::vector{Ammo::Arrows, Ammo::Fireworks}
              && ammoFor(Weapon::Elytra) == std::vector{Ammo::Fireworks} && ammoFor(Weapon::Other).empty(),
          "a bow counts arrows, a crossbow arrows and fireworks, an elytra fireworks");
    AmmoCounts counts;
    counts.add("minecraft:arrow", 64);
    counts.add("minecraft:arrow", 12); // tipped arrows are the same item
    counts.add("minecraft:firework_rocket", 9);
    counts.add("minecraft:snowball", 16);
    counts.add("minecraft:arrow", 0);
    check(counts.of(Ammo::Arrows) == 76 && counts.of(Ammo::Fireworks) == 9,
          "ammo totals add arrows of every kind and fireworks, nothing else");

    Row full{Slot::MainHand, 1561, 1561}, quarter{Slot::MainHand, 390, 1561}, low{Slot::MainHand, 118, 1561};
    check(numberText(Look::BarAndNumber, quarter) == "390/1561" && numberText(Look::Number, quarter) == "390/1561",
          "number looks show remaining over maximum");
    check(!showsBar(Look::Number) && showsBar(Look::Bar) && showsBar(Look::BarAndNumber), "only the number look omits the bar");
    check(!showsNumber(Look::Bar, full) && !showsNumber(Look::Bar, Row{Slot::MainHand, 391, 1561})
              && showsNumber(Look::Bar, quarter) && numberText(Look::Bar, low) == "118",
          "the bar look adds the remaining number below a quarter");
    check(showsNumber(Look::Number, full) && showsNumber(Look::BarAndNumber, full), "number looks always show it");
    check(barFill(full, 32) == 32 && barFill(Row{Slot::MainHand, 0, 10}, 32) == 0 && barFill(Row{Slot::MainHand, 5, 10}, 32) == 16,
          "bar fill tracks remaining durability in whole units");
    check(row(Slot::MainHand, Sample{true, 5000, 100}).remaining == 0
              && row(Slot::MainHand, Sample{true, -3, 100}).remaining == 100,
          "damage outside the item's range is clamped");

    using namespace lamium;
    Settings defaults;
    check(!defaults.information.durabilityHud && defaults.information.durabilityLook == 0
              && defaults.information.durabilityOffhand && defaults.information.durabilityArmor
              && defaults.hud.durability.anchor == ui::Anchor::BottomLeft,
          "the durability HUD starts off, bar and number, bottom left, with offhand and armor rows");
    auto loaded = decodeSettings(R"({"information":{"durabilityHud":true,"durabilityLook":2,"durabilityOffhand":true,"durabilityArmor":true},
        "hud":{"durability":{"anchor":2,"dx":-10,"dy":20}}})");
    check(loaded.information.durabilityHud && loaded.information.durabilityLook == 2
              && loaded.information.durabilityOffhand && loaded.information.durabilityArmor
              && loaded.hud.durability.anchor == ui::Anchor::TopRight && loaded.hud.durability.dy == 20,
          "durability HUD options load");
    check(defaults.information.durabilityAmmo && defaults.information.durabilityElytra
              && !decodeSettings(R"({"information":{"durabilityAmmo":false}})").information.durabilityAmmo
              && !decodeSettings(R"({"information":{"durabilityElytra":false}})").information.durabilityElytra,
          "ammo and the elytra row show by default and their switches load");
    check(defaults.information.durabilityMainHand && decodeSettings(R"({"information":{"durabilityHud":true}})").information.durabilityMainHand
              && !decodeSettings(R"({"information":{"durabilityMainHand":false}})").information.durabilityMainHand,
          "the main hand row shows by default, a file without the key shows it, and an explicit off loads (L-349)");
    check(decodeSettings(R"({"information":{"durabilityLook":9}})").information.durabilityLook == 2,
          "an unknown look is clamped");
    auto* look = settings::find("information.durabilityLook");
    check(look && std::get<settings::ChoiceValue>(look->read(defaults)).label == "durabilityLook.barAndNumber",
          "the look row defaults to bar and number");
}

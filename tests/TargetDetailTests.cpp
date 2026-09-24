#include "features/information/TargetInfo.h"
#include "features/information/TargetRows.h"
void check(bool, char const*);
void targetDetailTests() {
    using namespace lamium::information;
    auto integer = [](std::string_view key, long long number, std::string_view id = "minecraft:wheat") {
        return interpretBlockState(key, StateKind::Integer, number, {}, id);
    };
    auto growth = integer("growth", 5);
    check(growth && growth->label == "target.growth" && growth->value == "5 / 7" && !growth->valueIsKey
          && growth->progress && *growth->progress > 0.71f && *growth->progress < 0.72f,
          "wheat growth reads 5 of 7 with progress");
    auto beetroot = integer("growth", 3, "minecraft:beetroot");
    check(beetroot && beetroot->value == "3 / 3" && beetroot->progress && *beetroot->progress == 1.f,
          "beetroot growth uses its own maximum");
    auto unknownCrop = integer("growth", 2, "minecraft:torchflower");
    check(unknownCrop && unknownCrop->value == "2" && !unknownCrop->progress,
          "unknown crops show the stage without progress");
    auto wart = integer("age", 2, "minecraft:nether_wart");
    check(wart && wart->label == "target.age" && wart->value == "2 / 3", "nether wart age reads 2 of 3");
    auto power = integer("redstone_signal", 12, "minecraft:redstone_wire");
    check(power && power->label == "target.power" && power->value == "12" && power->progress
          && *power->progress > 0.79f && *power->progress < 0.81f, "redstone power reads with progress");
    check(!integer("redstone_signal", 16, "minecraft:redstone_wire"), "out of range power stays raw");
    auto facing = integer("facing_direction", 2, "minecraft:oak_door");
    check(facing && facing->label == "target.facing" && facing->value == "target.dirNorth" && facing->valueIsKey,
          "facing direction maps to a named direction");
    check(!integer("facing_direction", 6, "minecraft:oak_door"), "unknown facing stays raw");
    auto cardinal = interpretBlockState(
        "minecraft:cardinal_direction", StateKind::Text, 0, "east", "minecraft:oak_door");
    check(cardinal && cardinal->value == "East" && !cardinal->valueIsKey, "cardinal direction capitalizes");
    auto open = integer("open_bit", 1, "minecraft:oak_door");
    check(open && open->label == "target.open" && open->value == "target.yes", "open doors read open");
    auto half = integer("upper_block_bit", 0, "minecraft:oak_door");
    check(half && half->label == "target.half" && half->value == "target.lower", "lower door half reads lower");
    auto hinge = integer("door_hinge_bit", 1, "minecraft:oak_door");
    check(hinge && hinge->value == "target.hingeRight", "hinge side reads right");
    check(!integer("unknown_state", 1, "minecraft:stone"), "unknown states stay raw");
    check(!growthMax("minecraft:torchflower", false) && growthMax("minecraft:cocoa", true) == 2,
          "growth maxima cover common crops only");
    TargetInfo target;
    target.name = "Zombie";
    target.identifier = "minecraft:zombie";
    target.details.push_back({"target.health", "16 / 20", false, 0.8f});
    target.details.push_back({"target.age", "target.adult", true, {}});
    target.states.push_back("custom: 1");
    auto rows = targetRows(target, true, 10, {}, {"Health: 16 / 20", "Age: Adult"});
    check(rows.lines.size() == 5 && rows.lines[0] == "Zombie" && rows.lines[1] == "minecraft:zombie"
          && rows.lines[2] == "Health: 16 / 20" && rows.lines[3] == "Age: Adult"
          && rows.lines[4] == "custom: 1" && !rows.showOmitted, "details list before leftover raw states");
    auto tight = targetRows(target, false, 3, {}, {"Health: 16 / 20", "Age: Adult"});
    check(tight.lines.size() == 3 && tight.lines[1] == "Health: 16 / 20" && tight.lines[2] == "Age: Adult"
          && !tight.showOmitted && tight.omittedStates == 1, "details share capacity with raw states");
}

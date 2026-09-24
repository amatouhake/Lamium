#include "features/information/TargetCard.h"
void check(bool, char const*);
void targetCardTests() {
    using namespace lamium::information;
    TargetInfo mob{"Zombie", "minecraft:zombie"};
    mob.details.push_back({"target.armor", "2"});
    mob.details.push_back({"target.health", "16 / 20", false, .8f, DetailKind::Health});
    mob.details.push_back({"target.age", "target.adult", true});
    CardOptions options;
    auto rows = cardRows(mob, options);
    check(rows.size() == 1 && rows[0].label == "target.health" && rows[0].meter == Meter::Hearts,
          "health leads the card and other details wait for their switch");
    options.details = true;
    options.health = Meter::Bar;
    rows = cardRows(mob, options);
    check(rows.size() == 3 && rows[0].meter == Meter::Bar && rows[1].label == "target.armor"
          && rows[2].valueIsKey, "details follow health in their original order");

    TargetInfo crop{"Wheat", "minecraft:wheat"};
    crop.blockPosition = TargetInfo::BlockPosition{1, 2, 3};
    crop.details.push_back({"target.growth", "5 / 7", false, 5.f / 7, DetailKind::Growth});
    crop.states.push_back("custom_state: 4");
    CardOptions cropOptions;
    cropOptions.coordinates = true;
    cropOptions.growth = Meter::Number;
    rows = cardRows(crop, cropOptions);
    check(rows.size() == 2 && rows[0].meter == Meter::Number && rows[1].label == "target.position"
          && rows[1].value == "1, 2, 3", "growth then coordinates; growth can be a plain number");
    cropOptions.details = true;
    rows = cardRows(crop, cropOptions);
    check(rows.back().label == "custom_state" && !rows.back().labelIsKey && rows.back().value == "4",
          "raw states split into a label and a value");
    check(cardRows(crop, cropOptions, 1).size() == 1, "the row limit holds");

    TargetInfo unranged{"Mystery", "minecraft:mystery"};
    unranged.details.push_back({"target.growth", "9", false, std::nullopt, DetailKind::Growth});
    check(cardRows(unranged, CardOptions{}).front().meter == Meter::Number, "values without a range stay numbers");

    auto full = hearts(1.f);
    check(full[0] == Heart::Full && full[9] == Heart::Full, "full health fills every heart");
    auto some = hearts(.75f); // 15 of 20 halves
    check(some[6] == Heart::Full && some[7] == Heart::Half && some[8] == Heart::Empty, "odd halves draw a half heart");
    check(hearts(0)[0] == Heart::Empty && hearts(2.f)[9] == Heart::Full, "hearts clamp to the range");
    check(spawnEggItem("minecraft:zombie") == "minecraft:zombie_spawn_egg" && spawnEggItem("").empty(),
          "mobs use their spawn egg as the icon");
    check(spawnEggItem("minecraft:villager_v2") == "minecraft:villager_spawn_egg"
          && spawnEggItem("minecraft:zombie_villager_v2") == "minecraft:zombie_villager_spawn_egg"
          && spawnEggItem("minecraft:evocation_illager") == "minecraft:evoker_spawn_egg",
          "renamed entities map to their egg");
    check(!rangeBlocks(0) && rangeBlocks(1) == 8.f && rangeBlocks(4) == 64.f && !rangeBlocks(9),
          "range choice 0 keeps the game's reach");
    check(morphProgress(0) == 0 && morphProgress(morphSeconds) == 1 && morphProgress(1) == 1
          && morphProgress(morphSeconds / 2) > .5f, "the card eases out and settles");
}

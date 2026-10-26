#include "features/information/TargetCard.h"
#include "features/information/SchematicTarget.h"
void check(bool, char const*);
void targetCardTests() {
    using namespace lamium::information;
    TargetInfo mob{"Zombie", "minecraft:zombie"};
    mob.details.push_back({"target.armor", "2"});
    mob.details.push_back({"target.health", "16 / 20", false, .8f, DetailKind::Health, 16, 20});
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
    auto count = [](std::vector<Heart> const& icons, Heart kind) {
        return static_cast<int>(std::count(icons.begin(), icons.end(), kind));
    };
    auto zombie = healthHearts(20, 20);
    check(zombie.size() == 10 && count(zombie, Heart::Full) == 10, "20/20 is ten full hearts");
    auto hurt = healthHearts(10, 20);
    check(count(hurt, Heart::Full) == 5 && count(hurt, Heart::Empty) == 5, "10/20 is five of ten");
    auto enderman = healthHearts(20, 40);
    check(enderman.size() == 20 && count(enderman, Heart::Full) == 10 && enderman[10] == Heart::Empty,
          "20/40 fills ten of twenty slots instead of looking full");
    auto odd = healthHearts(19, 20);
    check(count(odd, Heart::Full) == 9 && odd[9] == Heart::Half, "19/20 is nine and a half hearts");
    check(healthHearts(15, 15).size() == 8 && healthHearts(15, 15)[7] == Heart::Half, "an odd maximum ends on a half slot");
    check(healthHearts(-3, 20)[0] == Heart::Empty && count(healthHearts(30, 20), Heart::Full) == 10
          && healthHearts(5, 0).empty(), "health clamps to the slots");
    check(heartLines(20) == 1 && heartLines(40) == 2 && heartLines(100) == 5 && heartLines(101) == 6,
          "ten hearts per line");
    auto meterFor = [&](int current, int maximum) {
        TargetInfo target{"Mob", "minecraft:mob"};
        target.details.push_back({"target.health", "", false, 1.f, DetailKind::Health, current, maximum});
        return cardRows(target, CardOptions{}).front().meter;
    };
    check(meterFor(100, 100) == Meter::Hearts && meterFor(600, 600) == Meter::Bar && meterFor(1, 0) == Meter::Bar,
          "up to five lines stay hearts; bosses become a bar");
    auto zombieEggs = spawnEggCandidates("minecraft:zombie");
    check(zombieEggs.size() == 1 && zombieEggs[0] == "minecraft:zombie_spawn_egg" && spawnEggCandidates("").empty(),
          "mobs use their spawn egg as the icon");
    auto villagerEggs = spawnEggCandidates("minecraft:villager_v2");
    check(villagerEggs.size() == 2 && villagerEggs[0] == "minecraft:villager_v2_spawn_egg"
          && villagerEggs[1] == "minecraft:villager_spawn_egg"
          && spawnEggCandidates("minecraft:zombie_villager_v2")[1] == "minecraft:zombie_villager_spawn_egg"
          && spawnEggCandidates("minecraft:evocation_illager")[1] == "minecraft:evoker_spawn_egg"
          && spawnEggCandidates("minecraft:vindication_illager")[1] == "minecraft:vindicator_spawn_egg",
          "the exact egg is tried before the renamed entity's egg");
    auto tridentItem = entityItemCandidates("minecraft:thrown_trident");
    check(tridentItem.size() == 2 && tridentItem[0] == "minecraft:thrown_trident"
          && tridentItem[1] == "minecraft:trident" && entityItemCandidates("minecraft:snowball").size() == 1
          && entityItemCandidates("").empty(),
          "an actor named after its throw keeps the item and the stripped id");
    check(entityItemCandidates("minecraft:ender_crystal")[1] == "minecraft:end_crystal"
          && entityItemCandidates("minecraft:eye_of_ender_signal")[1] == "minecraft:ender_eye"
          && entityItemCandidates("minecraft:xp_bottle")[1] == "minecraft:experience_bottle",
          "the bounded aliases cover Bedrock ids the item no longer shares");
    auto pick = chooseBlockIcon("minecraft:stone", 3, "textures/blocks/stone");
    check(pick.kind == IconKind::Item && pick.name == "minecraft:stone" && pick.aux == 3,
          "a block with an item keeps its pick item even when a texture exists");
    auto texture = chooseBlockIcon("", 0, "textures/blocks/portal");
    check(texture.kind == IconKind::Texture && texture.name == "textures/blocks/portal",
          "a block without an item falls back to its own texture");
    check(chooseBlockIcon("", 0, "").kind == IconKind::None, "without either source the icon stays empty");
    auto portalFrame = fileFrameUv(TargetIcon{IconKind::Texture, "textures/blocks/portal", 0, .2812f, .7812f, .2969f,
                                               .8125f},
                                   1024, 512, 16, 512);
    check(portalFrame.u0 == 0 && portalFrame.v0 == 0 && portalFrame.u1 == 1
          && portalFrame.v1 > .03f && portalFrame.v1 < .032f,
          "an atlas tile becomes the first frame of the source file");
    auto plain = fileFrameUv(TargetIcon{IconKind::Texture, "textures/blocks/stone", 0, .5f, .25f, .75f, .5f}, 16, 16,
                             16, 16);
    check(plain.u0 == .5f && plain.u1 == .75f, "a uv set that already matches its file is unchanged");
    check(morphProgress(0) == 0 && morphProgress(morphSeconds) == 1 && morphProgress(1) == 1
          && morphProgress(morphSeconds / 2) > .5f, "the card eases out and settles");
    {
        TargetInfo block{"Oak Stairs", "minecraft:oak_stairs"};
        TargetInfo::DetailRow line{"target.schematic", "Oak Stairs (Wrong state)"};
        line.kind = DetailKind::Schematic;
        line.icon = "icon";
        TargetInfo::DetailRow state{"weirdo_direction", "2 (now 1)"};
        state.kind = DetailKind::Schematic;
        state.labelIsKey = false;
        block.details = {line, state};
        block.states = {"weirdo_direction: 1"};
        CardOptions plain;
        auto shown = cardRows(block, plain);
        check(shown.size() == 2 && shown[0].icon == "icon" && shown[1].label == "weirdo_direction" && !shown[1].labelIsKey,
              "schematic rows show without details, the first with the expected block's icon");
        TargetInfo crop{"Wheat", "minecraft:wheat"};
        crop.details = {{"target.growth", "3 / 7", false, .4f, DetailKind::Growth}, line};
        crop.blockPosition = TargetInfo::BlockPosition{1, 2, 3};
        CardOptions withPosition;
        withPosition.coordinates = true;
        auto ordered = cardRows(crop, withPosition, 1);
        check(ordered.size() == 1 && ordered[0].label == "target.schematic",
              "schematic rows come first, so a short card keeps them");
        auto ghost = ghostRow();
        check(ghost.label == "target.schematic" && ghost.value == "target.schematicMissing" && ghost.valueIsKey
              && ghost.tone == Tone::Missing, "a ghost reads 'Schematic: Not placed yet' without the placement name");
    }
    {
        // L-112: common states by name.
        auto facing = interpretBlockState("weirdo_direction", StateKind::Integer, 3, {}, "minecraft:stone_stairs");
        check(facing && facing->label == "target.facing" && facing->value == "target.dirNorth", "stairs facing is named");
        auto trapdoor = interpretBlockState("direction", StateKind::Integer, 0, {}, "minecraft:oak_trapdoor");
        auto other = interpretBlockState("direction", StateKind::Integer, 0, {}, "minecraft:bed");
        check(trapdoor && trapdoor->value == "target.dirEast" && !other, "only a trapdoor's direction is named");
        auto slab = interpretBlockState("minecraft:vertical_half", StateKind::Text, 0, "top", "minecraft:stone_slab");
        auto axis = interpretBlockState("pillar_axis", StateKind::Text, 0, "x", "minecraft:oak_log");
        auto flipped = interpretBlockState("upside_down_bit", StateKind::Integer, 1, {}, "minecraft:stone_stairs");
        check(slab && slab->value == "target.upper" && axis && axis->value == "X" && flipped && flipped->value == "target.yes",
              "slab half, axis and upside down are named");
        check(!interpretBlockState("minecraft:corner", StateKind::Text, 0, "none", "minecraft:stone_stairs"),
              "unknown states keep their raw rows");
        auto source = interpretBlockState("liquid_depth", StateKind::Integer, 0, {}, "minecraft:water");
        auto flowing = interpretBlockState("liquid_depth", StateKind::Integer, 3, {}, "minecraft:flowing_water");
        auto falling = interpretBlockState("liquid_depth", StateKind::Integer, 9, {}, "minecraft:flowing_water");
        check(source && source->value == "target.flowSource" && flowing && flowing->label == "target.flowSteps"
                  && flowing->value == "3 / 7" && falling && falling->value == "target.flowFalling",
              "water reads as a source, steps from one, or falling (L-264)");
        auto lava = interpretBlockState("liquid_depth", StateKind::Integer, 4, {}, "minecraft:flowing_lava");
        auto netherLava = interpretBlockState("liquid_depth", StateKind::Integer, 4, {}, "minecraft:flowing_lava", true);
        check(lava && lava->value == "2 / 3" && netherLava && netherLava->value == "4 / 7",
              "lava flows three blocks in two-step depths, seven in the Nether");
        auto lit = interpretBlockState("lit", StateKind::Integer, 1, {}, "minecraft:candle");
        auto powered = interpretBlockState("powered", StateKind::Integer, 0, {}, "minecraft:unpowered_repeater");
        check(lit && lit->label == "target.lit" && lit->value == "target.yes" && powered && powered->label == "target.powered"
                  && powered->value == "target.no", "lit and powered read as Lit / Powered: yes or no");
        // L-93: schematic rows say what to do.
        auto translate = [](std::string_view key) { return "<" + std::string(key) + ">"; };
        using lamium::schematic::CellState;
        auto wrong = schematicRows(CellState::Wrong, "Dirt", "icon", {}, "minecraft:stone", translate);
        check(wrong.size() == 2 && wrong[0].label == "target.schematic" && wrong[0].value == "target.schematicWrong"
              && wrong[0].valueIsKey && wrong[0].tone == Tone::Wrong,
              "a wrong block first reads 'Schematic: Wrong block' in red");
        check(wrong[1].label == "schematic.shouldBe" && wrong[1].value == "Dirt"
              && wrong[1].icon == "icon" && wrong[1].tone == Tone::Wrong, "then 'Should be' with its icon");
        auto stairs = schematicRows(CellState::State, "Stairs", "icon",
            {{"weirdo_direction", "0", "3"}, {"minecraft:corner", "none", "inner_left"}}, "minecraft:stone_stairs", translate);
        check(stairs.size() == 3 && stairs[0].value == "target.schematicState" && stairs[0].tone == Tone::State,
              "a wrong state first reads 'Schematic: Wrong state' in yellow");
        check(stairs[1].label == "target.facing" && stairs[1].labelIsKey
              && stairs[1].before == "<target.dirNorth>" && stairs[1].value == "<target.dirEast>" && stairs[1].tone == Tone::State,
              "a wrong state reads '<state>: now -> should be' by name");
        check(stairs[2].label == "minecraft:corner" && !stairs[2].labelIsKey && stairs[2].before == "inner_left"
              && stairs[2].value == "none",
              "an unknown state keeps its raw name and values");
        TargetInfo card{"Stairs", "minecraft:stone_stairs"};
        card.details = stairs;
        auto drawn = cardRows(card, CardOptions{});
        check(drawn.size() == 3 && drawn[0].valueIsKey && drawn[1].before == "<target.dirNorth>" && drawn[1].tone == Tone::State,
              "card rows keep the change and its color");
        auto extra = schematicRows(CellState::Extra, "", "", {}, "minecraft:dirt", translate);
        check(extra.size() == 2 && extra[0].value == "target.schematicExtra" && extra[1].value == "<schematic.air>"
              && extra[1].icon.empty(), "an extra block reads 'Extra block', then should be air");
    }
    check(cardContentFits(0, 0, 100, 50, 0, 0, 100, 50) && cardContentFits(-10, -5, 120, 60, 0, 0, 100, 50),
          "the card content shows when the background covers its box");
    check(!cardContentFits(0, 0, 80, 50, 0, 0, 100, 50) && !cardContentFits(0, 0, 100, 30, 0, 0, 100, 50)
          && !cardContentFits(5, 0, 100, 50, 0, 0, 100, 50),
          "the card content waits while a growing background is smaller than it");
}

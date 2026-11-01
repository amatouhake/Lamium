#include "features/inventory/FakeOffhandPlan.h"
void check(bool, char const*);
void fakeOffhandTests() {
    using lamium::inventory::fakeOffhand::instantUseSlot;
    using lamium::inventory::fakeOffhand::ItemTraits;
    using lamium::inventory::fakeOffhand::primaryPass;
    using lamium::inventory::fakeOffhand::secondaryInstant;
    using lamium::inventory::fakeOffhand::foodBlocked;
    auto item = [](char const* name) { return ItemTraits{.name = name}; };
    ItemTraits empty{.name = {}, .empty = true};
    check(primaryPass(empty,false,false) && primaryPass(empty,true,false),
        "an empty primary hand always passes");
    for (auto name : {"minecraft:totem_of_undying", "minecraft:stick", "minecraft:bone",
        "minecraft:diamond", "minecraft:iron_ingot", "minecraft:paper", "minecraft:arrow"}) {
        check(primaryPass(item(name),false,false) && primaryPass(item(name),true,false),
            "items without use properties pass in air and on ordinary blocks");
        check(!primaryPass(item(name),true,false,false),
            "materials keep vanilla priority on blocks with a block entity or no full solid shape");
    }
    ItemTraits dirt{.name = "minecraft:dirt", .block = true};
    ItemTraits cobble{.name = "minecraft:cobblestone", .block = true};
    check(primaryPass(dirt,false,false) && primaryPass(cobble,false,false)
        && !primaryPass(dirt,true,false) && !primaryPass(cobble,true,false),
        "every block item passes in air but keeps placement on block targets");
    check(!primaryPass({.name = "minecraft:waterlily", .block = true, .liquidClip = true},false,false),
        "liquid-placed blocks keep their air aim, which can target water");
    ItemTraits axe{.name = "minecraft:diamond_axe", .damageable = true};
    check(primaryPass(axe,false,false) && !primaryPass(axe,true,false),
        "damageable tools pass in air but keep their uncertain block actions");
    ItemTraits sword{.name = "minecraft:diamond_sword", .timed = true, .damageable = true, .idleTool = true};
    check(primaryPass(sword,false,false) && primaryPass(sword,true,false) && primaryPass(sword,true,false,false)
        && primaryPass(empty,true,false,false),
        "checked swords and pickaxes pass despite a reported use duration");
    ItemTraits flesh{.name = "minecraft:rotten_flesh", .food = true, .timed = true};
    ItemTraits carrot{.name = "minecraft:carrot", .food = true, .timed = true, .planter = true};
    check(primaryPass(flesh,false,true) && !primaryPass(flesh,false,false),
        "food passes in air only when it cannot be eaten");
    check(!primaryPass(carrot,true,true) && !primaryPass(carrot,true,true,true,true) && primaryPass(carrot,true,true,true,false),
        "a food that plants keeps a block at full hunger unless its planter cannot be used there");
    ItemTraits bread{.name = "minecraft:bread", .food = true, .timed = true};
    check(primaryPass(bread,true,true) && !primaryPass(bread,true,true,false) && !primaryPass(bread,true,false),
        "L-189: at full hunger bread passes on plain blocks; hungry, or on a special block, it keeps priority");
    for (auto traits : {ItemTraits{.name = "minecraft:spyglass", .timed = true},
        ItemTraits{.name = "minecraft:snowball"}, ItemTraits{.name = "minecraft:egg", .throwable = true},
        ItemTraits{.name = "minecraft:water_bucket", .timed = true},
        ItemTraits{.name = "minecraft:lava_bucket"}, ItemTraits{.name = "minecraft:glass_bottle", .liquidClip = true},
        ItemTraits{.name = "minecraft:iron_chestplate", .wearable = true},
        item("minecraft:fishing_rod"), item("minecraft:firework_rocket"), item("minecraft:elytra"),
        item("minecraft:empty_map"), item("minecraft:shield"), item("minecraft:carved_pumpkin")}) {
        check(!primaryPass(traits,false,false), "items with an air use keep vanilla priority in air");
    }
    for (auto traits : {item("minecraft:wheat_seeds"), item("minecraft:oak_door"), item("minecraft:oak_sign"),
        item("minecraft:redstone"), item("minecraft:string"), item("minecraft:cow_spawn_egg"),
        item("minecraft:minecart"), item("minecraft:honeycomb"), item("minecraft:ender_eye"),
        item("minecraft:music_disc_cat"), item("minecraft:flower_pot"),
        ItemTraits{.name = "minecraft:bone_meal", .fertilizer = true},
        ItemTraits{.name = "minecraft:sweet_berries", .planter = true}}) {
        check(!primaryPass(traits,true,false), "items with a block use keep vanilla priority on blocks");
    }
    // L-189: dyes and books act through block entities (signs, lecterns,
    // shelves), which are not plain targets.
    for (auto traits : {ItemTraits{.name = "minecraft:white_dye", .dye = true}, item("minecraft:ink_sac"),
        item("minecraft:glow_ink_sac"), item("minecraft:book"), item("minecraft:enchanted_book")}) {
        check(primaryPass(traits,true,false) && !primaryPass(traits,true,false,false),
            "dyes and books pass on plain blocks and keep blocks with a block entity");
    }
    // L-189: a target-specific use the game says cannot act here passes on a
    // plain block; one that can, or that could not be read, keeps priority.
    using lamium::inventory::fakeOffhand::TargetUse;
    using lamium::inventory::fakeOffhand::targetUseOf;
    for (auto traits : {item("minecraft:wheat_seeds"), ItemTraits{.name = "minecraft:wheat_seeds", .planter = true},
        ItemTraits{.name = "minecraft:bone_meal", .fertilizer = true}, item("minecraft:minecart"),
        item("minecraft:hopper_minecart"), item("minecraft:honeycomb"),
        ItemTraits{.name = "minecraft:iron_axe", .damageable = true}, ItemTraits{.name = "minecraft:iron_shovel", .damageable = true},
        ItemTraits{.name = "minecraft:shears", .damageable = true}, ItemTraits{.name = "minecraft:bucket", .bucket = true},
        ItemTraits{.name = "minecraft:glass_bottle", .liquidClip = true}}) {
        check(primaryPass(traits,true,false,true,false) && !primaryPass(traits,true,false,true,true)
                  && !primaryPass(traits,true,false,true) && !primaryPass(traits,true,false,false,false),
              "a target use that cannot act passes on plain blocks only");
    }
    ItemTraits seeds{.name = "minecraft:wheat_seeds", .planter = true}, bone{.name = "minecraft:bone_meal", .fertilizer = true};
    check(targetUseOf(seeds,false,false,false) == TargetUse::Planter && targetUseOf(bone,false,false,false) == TargetUse::Fertilizer
              && targetUseOf(item("minecraft:chest_minecart"),false,false,false) == TargetUse::Rail
              && targetUseOf(item("minecraft:honeycomb"),false,false,false) == TargetUse::Honeycomb
              && targetUseOf(item("minecraft:netherite_axe"),false,false,false) == TargetUse::Axe
              && targetUseOf(item("minecraft:diamond_pickaxe"),false,false,false) == TargetUse::None
              && targetUseOf(item("minecraft:iron_shovel"),true,false,false) == TargetUse::Shovel
              && targetUseOf(item("minecraft:iron_hoe"),false,true,false) == TargetUse::Hoe
              && targetUseOf(item("minecraft:shears"),false,false,false) == TargetUse::Shears
              && targetUseOf(item("minecraft:bucket"),false,false,false) == TargetUse::Fill
              && targetUseOf(item("minecraft:water_bucket"),false,false,false) == TargetUse::None
              && targetUseOf(item("custom:axe"),false,false,false) == TargetUse::None,
          "each item has the target use its kind, tags or name say");
    using namespace lamium::inventory::fakeOffhand;
    check(shovelChanges("grass_block") && shovelChanges("podzol") && !shovelChanges("stone") && hoeChanges("dirt")
              && !hoeChanges("podzol") && shearsChange("pumpkin") && !shearsChange("carved_pumpkin"),
          "the shovel, hoe and shears lists name the blocks they change");
    check(strippable("oak_log") && strippable("crimson_hyphae") && strippable("bamboo_block") && !strippable("stripped_oak_log")
              && !strippable("oak_planks"),
          "unstripped logs, woods, stems, hyphae and bamboo blocks can be stripped");
    check(plantsOn("wheat_seeds") == "farmland" && plantsOn("carrot") == "farmland" && plantsOn("pitcher_pod") == "farmland"
              && plantsOn("nether_wart") == "soul_sand" && plantsOn("sweet_berries").empty() && plantsOn("cocoa_beans").empty(),
          "the fallback names what crops and nether wart stand on, and leaves the rest unread");
    ItemTraits plainCarrot{.name = "minecraft:carrot", .food = true, .timed = true};
    check(targetUseOf(item("minecraft:wheat_seeds"),false,false,false) == TargetUse::Planter
              && targetUseOf(plainCarrot,false,false,false) == TargetUse::Planter
              && !primaryPass(plainCarrot,true,true) && primaryPass(plainCarrot,true,true,true,false),
          "vanilla planters count by name when the game reports no planter (wheat seeds, carrots)");
    // L-189: an air use that cannot act now (no arrows, Riptide out of water,
    // a firework while not gliding, an empty bucket aimed at no liquid).
    for (auto traits : {item("minecraft:firework_rocket"), ItemTraits{.name = "minecraft:bow", .timed = true},
        ItemTraits{.name = "minecraft:trident", .timed = true, .throwable = true}, ItemTraits{.name = "minecraft:bucket", .bucket = true}}) {
        check(primaryPass(traits,false,false,true,std::nullopt,true) && !primaryPass(traits,false,false),
              "an idle air use passes in air");
    }
    ItemTraits bow{.name = "minecraft:bow", .timed = true};
    check(primaryPass(bow,true,false,true,std::nullopt,true) && !primaryPass(bow,true,false,false,std::nullopt,true)
              && !primaryPass(bow,true,false),
          "a bow without arrows passes on plain blocks too, not on special ones");
    // L-290: a filled bucket is idle only where its use cannot act. A pour
    // bucket needs a block face to pour on; a fish bucket can also put its fish
    // into aimed liquid; milk is never idle, and a block face keeps every
    // bucket primary. `air`: no block face in reach.
    using lamium::inventory::fakeOffhand::bucketIdle;
    auto bucketPasses = [&](char const* name, bool air, bool liquid) {
        bool idle = bucketIdle(name, air, liquid);
        return primaryPass(item(name), !air, false, true, std::nullopt, idle);
    };
    for (auto name : {"minecraft:water_bucket", "minecraft:lava_bucket", "minecraft:powder_snow_bucket"}) {
        check(bucketIdle(name,true,false) && bucketIdle(name,true,true)
                  && !bucketIdle(name,false,false) && !bucketIdle(name,false,true)
                  && bucketPasses(name,true,false) && bucketPasses(name,true,true)
                  && !bucketPasses(name,false,false) && !bucketPasses(name,false,true),
              "L-290: a pour bucket is idle with no block face, whatever liquid lies along the ray");
    }
    for (auto name : {"minecraft:cod_bucket", "minecraft:salmon_bucket", "minecraft:tropical_fish_bucket",
        "minecraft:pufferfish_bucket", "minecraft:axolotl_bucket", "minecraft:tadpole_bucket"}) {
        check(bucketIdle(name,true,false) && !bucketIdle(name,true,true)
                  && !bucketIdle(name,false,false) && !bucketIdle(name,false,true)
                  && bucketPasses(name,true,false) && !bucketPasses(name,true,true)
                  && !bucketPasses(name,false,false) && !bucketPasses(name,false,true),
              "L-290: a fish bucket is idle only with no block face and no liquid aimed");
    }
    check(!bucketIdle("minecraft:milk_bucket",true,false) && !bucketIdle("minecraft:milk_bucket",true,true)
              && !bucketIdle("minecraft:milk_bucket",false,false) && !bucketIdle("minecraft:milk_bucket",false,true)
              && !bucketPasses("minecraft:milk_bucket",true,false) && !bucketPasses("minecraft:milk_bucket",true,true)
              && !bucketPasses("minecraft:milk_bucket",false,false) && !bucketPasses("minecraft:milk_bucket",false,true),
          "L-290: milk is never idle, because drinking works anywhere");
    check(bucketIdle("minecraft:bucket",true,false) && !bucketIdle("minecraft:bucket",true,true)
              && !bucketIdle("minecraft:bucket",false,false) && !bucketIdle("minecraft:bucket",false,true)
              && bucketPasses("minecraft:bucket",true,false) && !bucketPasses("minecraft:bucket",true,true),
          "L-290: the empty bucket keeps its rule, idle only at no liquid and no block face");
    ItemTraits bottle{.name = "minecraft:glass_bottle", .liquidClip = true};
    check(primaryPass(bottle,false,false,true,std::nullopt,true)
              && !primaryPass(bottle,false,false,true,std::nullopt,false)
              && !primaryPass(bottle,true,false,true,std::nullopt,false),
          "L-290: the empty glass bottle keeps its air and no-liquid rule");
    check(!bucketIdle("custom:water_bucket",true,false) && !bucketIdle("",true,false),
        "L-290: a bucket name the list does not know keeps vanilla priority");
    for (auto name : {"minecraft:water_bucket", "minecraft:lava_bucket", "minecraft:powder_snow_bucket",
        "minecraft:cod_bucket", "minecraft:salmon_bucket", "minecraft:tropical_fish_bucket",
        "minecraft:pufferfish_bucket", "minecraft:axolotl_bucket", "minecraft:tadpole_bucket",
        "minecraft:bucket", "minecraft:milk_bucket"}) {
        check(!primaryPass(item(name),false,false,true,std::nullopt,false),
            "L-290: an entity target reports no idle, so every bucket keeps vanilla priority");
    }
    check(!primaryPass(item("custom:stick"),false,false) && !primaryPass(ItemTraits{},false,false),
        "unknown and unreadable primary items remain vanilla");
    check(foodBlocked(false,false,20.f,20.f) && !foodBlocked(false,false,19.f,20.f),
        "only confirmed full hunger blocks eating");
    check(!foodBlocked(true,false,20.f,20.f) && !foodBlocked(false,true,20.f,20.f),
        "always-edible and creative food cannot be inferred blocked from hunger alone");
    check(!foodBlocked(false,false,{},20.f) && !foodBlocked(false,false,NAN,20.f)
        && !foodBlocked(false,false,20.f,NAN) && !foodBlocked(false,false,0.f,0.f)
        && !foodBlocked(false,false,21.f,20.f),
        "missing and invalid hunger data preserve primary behavior");
    for (auto traits : {item("minecraft:snowball"), item("minecraft:egg"), item("minecraft:ender_pearl"),
        ItemTraits{.name = "minecraft:splash_potion", .throwable = true},
        ItemTraits{.name = "minecraft:bucket", .timed = true},
        ItemTraits{.name = "minecraft:water_bucket", .timed = true}, item("minecraft:lava_bucket")}) {
        check(secondaryInstant(traits,false,false) && secondaryInstant(traits,false,true),
            "projectiles and buckets are instant secondary uses");
    }
    ItemTraits rocket = item("minecraft:firework_rocket");
    check(secondaryInstant(rocket,true,false) && secondaryInstant(rocket,false,true)
        && !secondaryInstant(rocket,false,false),
        "fireworks are used while gliding or on a block, never wasted in plain air");
    for (auto traits : {item("minecraft:milk_bucket"), item("minecraft:fishing_rod"),
        ItemTraits{.name = "minecraft:trident", .timed = true, .throwable = true},
        ItemTraits{.name = "minecraft:bow", .timed = true}, ItemTraits{.name = "minecraft:apple", .food = true, .timed = true},
        item("minecraft:stick"), item("custom:snowball"), ItemTraits{.name = {}, .empty = true}}) {
        check(!secondaryInstant(traits,true,true),
            "timed, tethered, passive and unknown items cannot enter the per-call borrow");
    }
    using lamium::inventory::fakeOffhand::undoesRestore;
    check(undoesRestore(0,8,8,0,true),
        "a server reselecting the borrowed slot right after a restore is undone");
    check(!undoesRestore(0,8,8,0,false) && !undoesRestore(0,3,8,0,true) && !undoesRestore(2,8,8,0,true),
        "late updates, other slots and selections made after the restore are kept");
    check(!undoesRestore(0,8,-1,0,true) && !undoesRestore(8,8,8,8,true),
        "invalid or unborrowed slot records never change selection");
    using lamium::inventory::fakeOffhand::ownsInstantHold;
    check(ownsInstantHold(0,8,0,8,true),
        "a native instant hold can repeat while its primary and target slots stay owned");
    check(!ownsInstantHold(0,8,1,8,true) && !ownsInstantHold(0,8,8,8,true),
        "manual selection cancels repetition rather than restoring over the chosen slot");
    check(!ownsInstantHold(0,8,0,7,true) && !ownsInstantHold(0,8,0,8,false),
        "target changes and lost gameplay eligibility cancel the native instant hold");
    for (int invalid : {-1, 9}) {
        check(!ownsInstantHold(invalid,8,invalid,8,true)
            && !ownsInstantHold(0,invalid,0,invalid,true),
            "released and invalid slot identities cannot own a repeat session");
    }
    using lamium::inventory::fakeOffhand::queuedPressDecides;
    check(queuedPressDecides(true,false) && !queuedPressDecides(true,true),
        "a queued right-click press decides only when the native handler has not yet");
    check(queuedPressDecides(false,false) && queuedPressDecides(false,true),
        "other activation bindings always decide in the queued press");
    using lamium::inventory::fakeOffhand::queuedPressAction;
    using lamium::inventory::fakeOffhand::QueuedPress;
    check(queuedPressAction(false, std::nullopt, false) == QueuedPress::Nothing
        && queuedPressAction(true, std::nullopt, false) == QueuedPress::Nothing
        && queuedPressAction(false, std::nullopt, true) == QueuedPress::Nothing,
        "L-288: a queued press that borrows nothing does nothing, whatever the binding");
    check(queuedPressAction(false, 8, true) == QueuedPress::Instant
        && queuedPressAction(true, 8, true) == QueuedPress::Instant,
        "L-288: an instant borrow runs from the queued press on any binding");
    check(queuedPressAction(false, 8, false) == QueuedPress::Placement,
        "L-288: another binding replays the use edge for a placement borrow");
    check(queuedPressAction(true, 8, false) == QueuedPress::Nothing,
        "L-288: a right-click binding leaves a placement borrow to vanilla");
    using lamium::inventory::fakeOffhand::queuedHoldEdge;
    using lamium::inventory::fakeOffhand::HoldEdge;
    check(queuedHoldEdge(true, false, false, true) == HoldEdge::Down
        && queuedHoldEdge(true, false, false, false) == HoldEdge::None,
        "L-288: a hold that starts off a block face sends the use edge once a tick has a placement slot");
    check(queuedHoldEdge(true, true, false, false) == HoldEdge::Up
        && queuedHoldEdge(true, true, false, true) == HoldEdge::None,
        "L-288: a placement hold that loses the slot releases the edge instead of using the primary");
    check(queuedHoldEdge(false, true, false, false) == HoldEdge::None
        && queuedHoldEdge(true, true, true, false) == HoldEdge::None,
        "L-288: right-click and instant holds keep their own edge lifecycle");
    using lamium::inventory::fakeOffhand::primaryYields;
    using lamium::inventory::fakeOffhand::interactSlot;
    using lamium::inventory::fakeOffhand::placementSlot;
    check(primaryYields(true, false) && !primaryYields(false, false) && primaryYields(false, true),
        "L-292: only a right-click binding keeps the primary priority");
    check(placementSlot(true,true,0,8,true,true,false,false, primaryYields(true,false), false) == 8
        && !placementSlot(true,true,0,8,true,true,false,false, primaryYields(true,false), true)
        && instantUseSlot(true,true,0,8, primaryYields(true,false), true,false,false,false) == 8,
        "L-292: another key borrows over a primary block, but never interrupts a primary in use");
    check(interactSlot(true,0,8,true,false,false) == 8,
        "L-292: another key opens an interactive block with the borrowed slot selected");
    check(!interactSlot(true,0,8,true,true,false) && !interactSlot(false,0,8,true,false,false)
        && !interactSlot(true,0,8,false,false,false) && !interactSlot(true,0,8,true,false,true)
        && !interactSlot(true,8,8,true,false,false) && !interactSlot(true,0,9,true,false,false),
        "L-292: sneaking, the right-click binding, plain blocks, a primary in use and invalid slots do not interact");
    check(!ownsInstantHold(8,8,8,8,true),
        "an ordinary selected target never becomes a borrowed repeat session");
    check(instantUseSlot(true,true,0,8,true,true,false,false,false) == 8,
        "an idle primary hand can borrow an instant-use item in air or on an ordinary block");
    check(!instantUseSlot(true,true,0,8,false,true,false,false,false),
        "applicable or uncertain primary-hand use keeps priority");
    check(!instantUseSlot(true,true,0,8,true,false,false,false,false),
        "empty target and timed items cannot start from a per-call borrow");
    check(!instantUseSlot(true,true,0,8,true,true,true,false,false),
        "entity interactions remain vanilla in the first instant-use step");
    check(!instantUseSlot(true,true,0,8,true,true,false,true,false)
        && instantUseSlot(true,true,0,8,true,true,false,true,true) == 8,
        "containers keep ordinary interaction unless sneaking");
    check(!instantUseSlot(false,true,0,8,true,true,false,false,false)
        && !instantUseSlot(true,false,0,8,true,true,false,false,false),
        "disabled or released activation never changes selection");
    for (int invalid : {-1, 9}) {
        check(!instantUseSlot(true,true,invalid,8,true,true,false,false,false)
            && !instantUseSlot(true,true,0,invalid,true,true,false,false,false),
            "only valid hotbar slots can be borrowed");
    }
    check(!instantUseSlot(true,true,8,8,true,true,false,false,false),
        "manually selecting the target leaves its ordinary use alone");
    using lamium::inventory::fakeOffhand::placementSlot;
    check(!placementSlot(true,true,0,8,true,true,false,false,primaryPass(dirt,true,false))
        && !placementSlot(true,true,0,8,true,true,false,false,primaryPass(bread,true,false)),
        "secondary blocks cannot preempt primary placement or hungry food");
    check(!placementSlot(true,true,0,8,true,true,false,false,true,true),
        "secondary placement never selects away from an active primary use");
    auto durationBlock = dirt;
    durationBlock.timed = true;
    check(primaryPass(durationBlock,false,false) && !primaryPass(durationBlock,true,false)
        && instantUseSlot(true,true,0,8,primaryPass(durationBlock,false,false),secondaryInstant(rocket,true,false),false,false,false) == 8,
        "a block with native duration passes to gliding fireworks only with air aim");
}

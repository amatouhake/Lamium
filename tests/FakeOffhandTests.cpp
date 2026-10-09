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
    ItemTraits carrot{.name = "minecraft:carrot", .food = true, .timed = true};
    check(primaryPass(flesh,false,true) && !primaryPass(flesh,false,false),
        "food passes in air only when it cannot be eaten");
    check(!primaryPass(carrot,true,true),
        "food keeps block targets at full hunger because some foods plant crops");
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
        item("minecraft:music_disc_cat"), item("minecraft:book"), item("minecraft:flower_pot"),
        ItemTraits{.name = "minecraft:bone_meal", .fertilizer = true},
        ItemTraits{.name = "minecraft:white_dye", .dye = true},
        ItemTraits{.name = "minecraft:sweet_berries", .planter = true}}) {
        check(!primaryPass(traits,true,false), "items with a block use keep vanilla priority on blocks");
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
}

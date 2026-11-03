#include "features/schematic/BlockEntityCheck.h"
#include <map>
#include <string>

void check(bool, char const*);

namespace {
using namespace lamium::schematic;
using entitycheck::differences;

nbt::Compound entity(std::string id) {
    nbt::Compound data;
    data.set("id", {std::move(id)});
    data.set("x", {std::int32_t{4}});
    return data;
}
nbt::Compound item(std::string name, std::int16_t damage = 0) {
    nbt::Compound stack;
    stack.set("Name", {std::move(name)});
    stack.set("Count", {std::int8_t{1}});
    if (damage) stack.set("Damage", {damage});
    return stack;
}
nbt::Compound side(std::string text, std::int32_t color = -16777216) {
    nbt::Compound c;
    c.set("Text", {std::move(text)});
    c.set("SignTextColor", {color});
    c.set("IgnoreLighting", {std::int8_t{0}});
    c.set("TextOwner", {std::string("someone")});
    return c;
}
}

void blockEntityCheckTests() {
    Placement still;
    // A note block: the note, by value whatever width it was saved in.
    {
        auto file = entity("Music"), world = entity("Music");
        file.set("note", {std::int8_t{5}});
        world.set("note", {std::int32_t{5}});
        check(differences(file, world, still, false).empty(), "the same note in another width matches");
        world.set("note", {std::int8_t{12}});
        auto d = differences(file, world, still, false);
        check(d.size() == 1 && d[0].field->key == "note", "another note is one difference");
        world.set("x", {std::int32_t{9}});
        check(differences(file, world, still, false).size() == 1, "the position is not compared");
    }
    // A field one side does not store says nothing, unless absent means none.
    {
        auto file = entity("Music"), world = entity("Music");
        world.set("note", {std::int8_t{3}});
        check(differences(file, world, still, false).empty(), "a file without the note does not judge it");
        auto frame = entity("ItemFrame"), filled = entity("ItemFrame");
        filled.set("Item", {item("minecraft:diamond")});
        filled.set("ItemRotation", {45.f});
        auto d = differences(frame, filled, still, false);
        check(d.size() == 1 && !d[0].expected && d[0].actual, "an item in a frame the file leaves empty differs");
        check(differences(frame, frame, still, false).empty(), "two empty frames match");
    }
    // An item frame: the item by name and data value, its count and tags aside;
    // the rotation only while both hold an item.
    {
        auto file = entity("ItemFrame"), world = entity("ItemFrame");
        file.set("Item", {item("minecraft:wool", 14)});
        file.set("ItemRotation", {90.f});
        auto worldItem = item("minecraft:wool", 14);
        worldItem.set("Count", {std::int8_t{3}});
        worldItem.set("WasPickedUp", {std::int8_t{1}});
        world.set("Item", {worldItem});
        world.set("ItemRotation", {90.f});
        check(differences(file, world, still, false).empty(), "the same item with another count matches");
        world.set("ItemRotation", {135.f});
        auto d = differences(file, world, still, false);
        check(d.size() == 1 && d[0].field->key == "ItemRotation", "a turned item differs");
        world.set("Item", {item("minecraft:wool", 1)});
        check(differences(file, world, still, false).size() == 2, "another colour of wool differs as well");
        auto plain = item("minecraft:stick"), zero = item("minecraft:stick");
        zero.set("Damage", {std::int16_t{0}});
        auto a = entity("GlowItemFrame"), b = entity("GlowItemFrame");
        a.set("Item", {plain});
        b.set("Item", {zero});
        check(differences(a, b, still, false).empty(), "a data value of 0 matches one left out");
    }
    // A lectern: the page counts while the file holds a book; without one in
    // the world the file's page still shows, against nothing.
    {
        auto file = entity("Lectern"), world = entity("Lectern");
        file.set("hasBook", {std::int8_t{1}});
        file.set("page", {std::int32_t{2}});
        world.set("hasBook", {std::int8_t{0}});
        world.set("page", {std::int32_t{0}});
        auto d = differences(file, world, still, false);
        check(d.size() == 2 && d[0].field->key == "hasBook" && d[1].field->key == "page" && d[1].expected && !d[1].actual,
              "a missing book shows the book and the page it should open at");
        world.set("hasBook", {std::int8_t{1}});
        d = differences(file, world, still, false);
        check(d.size() == 1 && d[0].field->key == "page", "another page differs once the book is there");
        file.set("hasBook", {std::int8_t{0}});
        world.set("page", {std::int32_t{5}});
        d = differences(file, world, still, false);
        check(d.size() == 1 && d[0].field->key == "hasBook", "without a book in the file the page is not judged");
    }
    // An empty frame in the world still shows the turn the file's item has.
    {
        auto file = entity("ItemFrame"), world = entity("ItemFrame");
        file.set("Item", {item("minecraft:clock")});
        file.set("ItemRotation", {90.f});
        world.set("ItemRotation", {0.f});
        auto d = differences(file, world, still, false);
        check(d.size() == 2 && d[1].field->key == "ItemRotation" && d[1].expected && !d[1].actual,
              "an empty frame shows the item and its turn");
    }
    // A sign: text, colour and glow on both sides; the owner aside; the
    // setting leaves all of it out.
    {
        auto file = entity("Sign"), world = entity("Sign");
        file.set("FrontText", {side("Storage")});
        file.set("BackText", {side("")});
        auto worldFront = side("Storage");
        worldFront.set("TextOwner", {std::string("another")});
        world.set("FrontText", {worldFront});
        world.set("BackText", {side("")});
        check(differences(file, world, still, false).empty(), "the same text by another writer matches");
        world.set("FrontText", {side("Storge", -1)});
        auto d = differences(file, world, still, false);
        check(d.size() == 2 && d[0].field->within == "FrontText", "other text and colour differ on the front");
        check(differences(file, world, still, true).empty(), "Ignore sign text passes the sign");
        check(entitycheck::compared(file, false) && !entitycheck::compared(file, true),
              "a sign is read only while its text is compared");
    }
    // A skull's rotation turns with the placement; a banner's patterns may be
    // left out when there are none.
    {
        auto file = entity("Skull"), world = entity("Skull");
        file.set("SkullType", {std::int8_t{3}});
        file.set("Rotation", {0.f});
        world.set("SkullType", {std::int8_t{3}});
        world.set("Rotation", {90.f});
        Placement turned;
        turned.rotation = 1;
        std::map<std::string, std::string> floor{{"facing_direction", "1"}}, wall{{"facing_direction", "3"}};
        check(differences(file, world, turned, false, &floor).empty(), "a skull turned with the placement matches");
        check(differences(file, world, still, false, &floor).size() == 1, "the same skull unturned differs");
        world.set("Rotation", {-270.f});
        check(differences(file, world, turned, false, &floor).empty(), "angles a full turn apart match");
        world.set("Rotation", {45.f});
        check(differences(file, world, turned, false, &wall).empty(),
              "a head on a wall faces by its state; its stored rotation is not compared");
        check(differences(file, world, turned, false).empty(), "without the file's states the rotation is not judged");

        // The game writes a half turn as 180, never -180; a ghost of a turned
        // placement reads its turned angle, as its model is drawn.
        check(entitycheck::gameAngle(-180.f) == 180.f && entitycheck::gameAngle(270.f) == -90.f
                  && entitycheck::gameAngle(180.f) == 180.f, "angles read as the game writes them");
        file.set("Rotation", {-180.f});
        auto shown = entitycheck::placedEntity(file, turned);
        auto const* turn = shown.find("Rotation");
        check(turn && turn->as<float>() && *turn->as<float>() == -90.f, "a ghost's head rotation reads turned");
        auto unturned = entitycheck::placedEntity(file, still);
        check(*unturned.find("Rotation")->as<float>() == 180.f, "an unturned half turn reads 180");

        auto banner = entity("Banner"), plain = entity("Banner");
        banner.set("Base", {std::int32_t{15}});
        plain.set("Base", {std::int32_t{15}});
        check(differences(banner, plain, still, false).empty(), "two banners without patterns match");
        nbt::Compound pattern;
        pattern.set("Color", {std::int32_t{1}});
        pattern.set("Pattern", {std::string("bri")});
        banner.set("Patterns", {nbt::List{nbt::Type::Compound, {nbt::Tag{pattern}}}});
        auto d = differences(banner, plain, still, false);
        check(d.size() == 1 && d[0].expected && !d[0].actual, "patterns the world lacks differ");
    }
    // A flower pot's plant by name and states, not the version; contents are
    // never read.
    {
        auto file = entity("FlowerPot"), world = entity("FlowerPot");
        nbt::Compound plant;
        plant.set("name", {std::string("minecraft:poppy")});
        plant.set("version", {std::int32_t{1}});
        file.set("PlantBlock", {plant});
        plant.set("version", {std::int32_t{2}});
        world.set("PlantBlock", {plant});
        check(differences(file, world, still, false).empty(), "the same plant of another version matches");
        auto chest = entity("Chest");
        chest.set("Items", {nbt::List{nbt::Type::Compound, {nbt::Tag{item("minecraft:dirt")}}}});
        check(!entitycheck::compared(chest, false), "a chest is never read");
        check(differences(chest, entity("Chest"), still, false).empty(), "a chest's contents are not compared");
        check(differences(entity("Music"), entity("Lectern"), still, false).empty(),
              "another kind of block entity is the block check's to judge");
    }
}

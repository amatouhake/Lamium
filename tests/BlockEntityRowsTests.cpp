#include "features/information/BlockEntityRows.h"
#include "features/information/NoteEvent.h"
#include "features/information/TargetCard.h"
#include "features/information/TargetInfo.h"
#include <format>
#include <string>
#include <string_view>
#include <vector>

void check(bool, char const*);

void blockEntityRowTests() {
    using namespace lamium::information;
    namespace nbt = lamium::schematic::nbt;
    auto integer = [](long long value) { return nbt::Tag{static_cast<std::int8_t>(value)}; };

    // The fields the card already covers never make a row.
    for (auto hidden : {std::string_view{"id"}, std::string_view{"x"}, std::string_view{"y"},
                        std::string_view{"z"}, std::string_view{"isMovable"}, std::string_view{"Findable"}})
        check(blockEntityFieldHidden(hidden), "identifying and engine fields make no row");
    for (auto shown : {std::string_view{"note"}, std::string_view{"FrontText"}, std::string_view{"Items"}})
        check(!blockEntityFieldHidden(shown), "a stored field makes a row");

    // Note 0 plays F#3 and the range ends at F#5; nothing else claims a name.
    check(notePitch(0) == "F#3" && notePitch(12) == "F#4" && notePitch(24) == "F#5",
          "note 0, 12 and 24 read as F#3, F#4 and F#5");
    check(notePitch(-1).empty() && notePitch(25).empty(), "a note outside 0-24 has no name");

    BlockEntityNames names;
    names.item = [](std::string_view id) -> std::string {
        return id == "minecraft:music_disc_cat" ? "Cat" : std::string{};
    };
    names.block = [](std::string_view id) -> std::string {
        return id == "minecraft:poppy" ? "Poppy" : std::string{};
    };
    names.label = [](std::string_view key) -> std::string {
        return key == "note" || key == "IsWaxed" ? std::string(key) : std::string{};
    };

    // L-300: a note block's pitch is stored, not a block state, and reads as
    // the note it plays.
    check(blockEntityValue("note", integer(12), names) == "12 / 24 (F#4)", "note 12 reads 12 / 24 (F#4)");
    check(blockEntityValue("note", integer(30), names) == "30", "a note past 24 keeps its number");

    {
        nbt::Tag text{std::string{"front\nback"}};
        check(blockEntityValue("FrontText", text, names) == "front / back", "a sign's lines read as one row");
        std::string longText(300, 'a');
        check(blockEntityValue("FrontText", nbt::Tag{longText}, names).ends_with("…"),
              "a long line is cut, not dropped");
        check(blockEntityValue("FrontText", nbt::Tag{std::string{}}, names).empty(),
              "an empty line stores nothing");
    }
    {
        nbt::Compound item;
        item.set("Name", {std::string("minecraft:music_disc_cat")});
        item.set("Count", {std::int8_t{1}});
        check(blockEntityValue("RecordItem", nbt::Tag{item}, names) == "Cat",
              "a stored item reads as the item's name");
        item.set("Count", {std::int8_t{3}});
        check(blockEntityValue("RecordItem", nbt::Tag{item}, names) == "Cat ×3", "a stack reads its count");

        nbt::Compound unknownItem;
        unknownItem.set("Name", {std::string("minecraft:unknown_thing")});
        check(blockEntityValue("RecordItem", nbt::Tag{unknownItem}, names) == "minecraft:unknown_thing",
              "an item the game cannot name keeps its id");

        nbt::Compound plant;
        plant.set("name", {std::string("minecraft:poppy")});
        check(blockEntityValue("PlantBlock", nbt::Tag{plant}, names) == "Poppy", "a potted plant reads its name");
    }
    {
        nbt::List sherds{nbt::Type::String, {nbt::Tag{std::string{"minecraft:brick"}}, nbt::Tag{std::string{"minecraft:brick"}}}};
        check(blockEntityValue("sherds", nbt::Tag{sherds}, names) == "minecraft:brick, minecraft:brick",
              "a pot's sherds read as one row");

        nbt::Compound book;
        book.set("Name", {std::string("minecraft:music_disc_cat")});
        nbt::List books{nbt::Type::Compound, {nbt::Tag{book}, nbt::Tag{book}}};
        check(blockEntityValue("Items", nbt::Tag{books}, names) == "Cat, Cat", "a shelf's items read as names");

        nbt::List empty{nbt::Type::String, {}};
        check(blockEntityValue("Items", nbt::Tag{empty}, names).empty(), "an empty list stores nothing");
    }
    check(blockEntityValue("Rotation", nbt::Tag{1.5f}, names) == "1.5", "a float keeps its value");

    // A sign's text compound carries its colour and glow: they become rows of
    // their own instead of hiding behind the text (L-300).
    {
        nbt::Compound front;
        front.set("Text", {std::string{"a\nb"}});
        front.set("IgnoreLighting", {std::int8_t{1}});
        front.set("SignTextColor", {std::int32_t{-16777216}});
        front.set("PersistFormatting", {std::int8_t{1}});
        nbt::Compound data;
        data.set("FrontText", {front});
        auto rows = blockEntityRows(data, names);
        check(rows.size() == 4, "the text and its three fields make four rows");
        check(rows[0].label == "FrontText" && rows[0].value == "a / b", "the text keeps its own row");
        check(rows[1].label == "IgnoreLighting" && rows[1].value == "target.yes" && rows[1].valueIsKey,
              "a nested flag reads as yes");
        check(rows[2].label == "SignTextColor" && rows[2].value == "#000000",
              "a nested colour reads as #RRGGBB");
        check(rows[3].label == "PersistFormatting" && rows[3].value == "1",
              "a nested field without a label keeps its own key");
    }
    // An item stack never spills its Name and Count into rows of their own.
    {
        nbt::Compound item;
        item.set("Name", {std::string("minecraft:music_disc_cat")});
        item.set("Count", {std::int8_t{1}});
        nbt::Compound data;
        data.set("RecordItem", {item});
        check(blockEntityRows(data, names).size() == 1, "an item stack stays one row");
    }
    // A container's item list is the world-load copy: the card leaves it out.
    {
        nbt::Compound coin;
        coin.set("Name", {std::string("minecraft:music_disc_cat")});
        nbt::List held{nbt::Type::Compound, {nbt::Tag{coin}}};
        nbt::Compound furnace;
        furnace.set("Items", {held});
        furnace.set("BurnTime", {std::int16_t{200}});
        auto shown = blockEntityRows(furnace, names);
        check(shown.size() == 2, "a container, asked for everything, shows its items");
        std::string_view const containerStale[] = {"Items"};
        auto skipped = blockEntityRows(furnace, names, containerStale);
        check(skipped.size() == 1 && skipped[0].label == "BurnTime",
              "a container's stale item list makes no row");
        // L-301: the sound event packs the instrument above the note. The probe
        // read these values under four different blocks.
        check(noteFromEventData(14) == 14, "under air: data 14 is note 14");
        check(noteFromEventData(257) == 1 && noteFromEventData(259) == 3, "under stone: 257-259 are notes 1-3");
        check(noteFromEventData(769) == 1, "under glass: 769 is note 1");
        check(noteFromEventData(1793) == 1, "under wool: 1793 is note 1");
        check(!noteFromEventData(-1) && !noteFromEventData(25) && !noteFromEventData(281),
              "a note past 24 or an unknown instrument makes no note");
        // The block stores the note the world loaded with, and the sound
        // events report the pitch the card should show.
        nbt::Compound music;
        music.set("note", {std::int8_t{8}});
        auto stored = blockEntityRows(music, names);
        check(stored.size() == 1 && stored[0].value == "8 / 24 (D4)", "the stored note is the row's value");
        auto heard = blockEntityRows(music, names, {}, 12);
        check(heard.size() == 1 && heard[0].value == "12 / 24 (F#4)", "a played note replaces the stored one");
        std::string_view const noteStale[] = {"note"};
        check(blockEntityRows(music, names, noteStale).empty(), "a field listed as stale makes no row");
    }
    // Every stored field makes one row, in the order the game saved them.
    {
        nbt::Compound data;
        data.set("id", {std::string("Music")});
        data.set("x", {std::int32_t{10}});
        data.set("isMovable", {std::int8_t{0}});
        data.set("note", {std::int8_t{12}});
        data.set("IsWaxed", {std::int8_t{1}});
        data.set("SkullType", {std::int8_t{3}});
        auto rows = blockEntityRows(data, names);
        check(rows.size() == 3, "three stored fields make three rows");
        check(rows[0].label == "note" && !rows[0].labelIsKey && rows[0].value == "12 / 24 (F#4)"
              && rows[0].kind == DetailKind::Other, "the pitch keeps its label and value");
        check(rows[1].label == "IsWaxed" && rows[1].value == "target.yes" && rows[1].valueIsKey,
              "a stored flag reads as yes");
        check(rows[2].label == "SkullType" && !rows[2].labelIsKey && rows[2].value == "3",
              "a field without a label keeps its own key");
    }
    // Only a byte reads as a flag; a wider integer with a flag-like name is a number.
    {
        nbt::Compound data;
        data.set("IsCount", {std::int32_t{1}});
        auto rows = blockEntityRows(data, names);
        check(rows.size() == 1 && rows[0].value == "1" && !rows[0].valueIsKey, "an int named like a flag keeps its number");
    }
}

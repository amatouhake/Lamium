#include "features/inspection/ItemDetails.h"
void check(bool, char const*);
void itemDetailsTests() {
    using namespace lamium::inspection::details;
    check(number(8) == "8" && number(0.5f) == "0.5" && number(1.25f) == "1.25" && number(10) == "10",
          "numbers drop their trailing zeros and keep whole tens");
    check(clip("abc", 3) == "abc" && clip("abcd", 3) == "abc\xE2\x80\xA6" && clip("\xE3\x81\x82\xE3\x81\x84", 1) == "\xE3\x81\x82\xE2\x80\xA6",
          "a value is cut by characters, not bytes");
    check(clip("\xC2\xA7" "cRed\nrose") == "&cRed rose", "a value cannot recolour or break the tooltip");
    {
        auto rows = wrapped({"aaaa", "bbbb", "cccc"}, 12, 4);
        check(rows.size() == 2 && rows[0] == "aaaa, bbbb, " && rows[1] == "cccc", "names wrap at the width");
        auto cut = wrapped({"aaaa", "bbbb", "cccc"}, 6, 2);
        check(cut.size() == 2 && cut[1] == "bbbb, \xE2\x80\xA6", "names past the last line become an ellipsis");
        check(wrapped({}).empty(), "no names, no lines");
    }
    check(lines({}).empty(), "an item with nothing to tell adds nothing");
    {
        Item chestplate{.id = "minecraft:netherite_chestplate", .armor = 8, .toughness = 3, .knockbackResistance = 0.1f,
                        .enchantability = 15, .maxStack = 1};
        auto out = lines(chestplate);
        check(out.size() == 5 && out[0] == Line{"itemDetails.armor", "8"} && out[1] == Line{"itemDetails.toughness", "3"}
                  && out[2] == Line{"itemDetails.knockbackResistance", "10"} && out[3] == Line{"itemDetails.enchantability", "15"}
                  && out[4] == Line{"", "minecraft:netherite_chestplate", true},
              "armour values come first, the id last, and a stack of one is not told");
    }
    {
        Item coal{.id = "minecraft:coal", .burnTicks = 1600};
        Item stick{.id = "minecraft:stick", .burnTicks = 100};
        Item pearl{.id = "minecraft:ender_pearl", .maxStack = 16, .cooldownTicks = 20};
        check(lines(coal)[0] == Line{"itemDetails.fuel", "8"} && lines(stick)[0] == Line{"itemDetails.fuel", "0.5"},
              "fuel is told as the items it smelts");
        auto out = lines(pearl);
        check(out[0] == Line{"itemDetails.maxStack", "16"} && out[1] == Line{"itemDetails.cooldown", "1"},
              "a small stack and a cooldown in seconds");
    }
    {
        Item potion{.id = "minecraft:potion", .aux = 21, .tags = {"minecraft:is_food"}};
        for (int i = 0; i < 10; ++i) potion.data.push_back({"k" + std::to_string(i), "v"});
        auto out = lines(potion);
        check(out[0] == Line{"", "minecraft:potion #21", true} && out[1] == Line{"itemDetails.tags", "minecraft:is_food", true},
              "a data value follows the id, tags follow both");
        check(out.size() == 2 + dataLines + 1 && out[2] == Line{"", "k0: v", true} && out.back() == Line{"itemDetails.more", "2", true},
              "user data is listed up to its limit, then counted");
    }
}

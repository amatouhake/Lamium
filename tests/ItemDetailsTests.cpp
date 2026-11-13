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
        Item sword{.id = "minecraft:netherite_sword", .enchantability = 15, .maxStack = 1};
        auto out = lines(sword);
        check(out.size() == 2 && out[0] == Line{"itemDetails.enchantability", "15"} && out[1] == Line{"", "minecraft:netherite_sword", true},
              "values come first, the id after them, and a stack of one is not told");
    }
    {
        Item stew{.id = "minecraft:suspicious_stew", .aux = 2, .effect = "Weakness (0:07)", .useTicks = 32};
        Item bow{.id = "minecraft:bow", .useTicks = 72000};
        auto out = lines(stew);
        check(out[0] == Line{"", "Weakness (0:07)"} && out[1] == Line{"itemDetails.useTime", "1.6"},
              "an effect the game left out leads, then the time the item takes to use");
        check(lines(bow).size() == 1, "an item held for as long as the button is has no use time");
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
        potion.data = {{"display", "", 0}, {"Name", "\"x\"", 1}, {"Lore", "[2]", 1}, {"0", "\"a\"", 2}, {"Damage", "3", 0}};
        auto out = lines(potion);
        check(out[0] == Line{"", "minecraft:potion #21", true} && out[1] == Line{"itemDetails.tags", "minecraft:is_food", true},
              "a data value follows the id, tags follow both");
        check(out.size() == 7 && out[2] == Line{"", "display:", true} && out[3] == Line{"", "  Name: \"x\"", true}
                  && out[4] == Line{"", "  Lore: [2]", true} && out[5] == Line{"", "    0: \"a\"", true} && out[6] == Line{"", "Damage: 3", true},
              "user data is told whole, each level indented under its compound or list");
        Item big{.id = "x"};
        for (size_t i = 0; i < dataLines + 5; ++i) big.data.push_back({"k", "v", 0});
        big.dataTotal = dataLines + 9;
        auto cut = lines(big);
        check(cut.size() == 1 + dataLines + 1 && cut.back() == Line{"itemDetails.more", "9", true},
              "past the limit the rest of the user data is counted, by the total the walk found");
    }
}

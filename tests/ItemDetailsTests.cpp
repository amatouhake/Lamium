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
        Item bread{.id = "minecraft:bread", .useTicks = 32};
        Item bow{.id = "minecraft:bow", .useTicks = 72000};
        check(lines(bread)[0] == Line{"itemDetails.useTime", "1.6"}, "the time the item takes to use, in seconds");
        check(lines(bow).size() == 1, "an item held for as long as the button is has no use time");
        check(effectTint(true) == 'c' && effectTint(false) == '9' && effectTint({}) == 'f',
              "a harmful effect is red, a helpful one blue, an unknown one white");
        check(modifierLine(-0.5f, 0, "Attack Damage") == "-0.5 Attack Damage" && modifierLine(0.2f, 2, "Speed") == "+20% Speed"
                  && modifierLine(3, 0, "Armor") == "+3 Armor",
              "an attribute change is an amount or a share, signed");
        check(plain("\xC2\xA7" "cRed\xC2\xA7") == "Red" && plain("plain") == "plain", "codes are dropped, a cut one too");
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
        potion.data = {{"display", "{Name:\"x\"}"}, {"Damage", "3"}};
        auto out = lines(potion);
        check(out[0] == Line{"", "minecraft:potion #21", true} && out[1] == Line{"itemDetails.tags", "minecraft:is_food", true},
              "a data value follows the id, tags follow both");
        check(out.size() == 3 && out[2] == Line{"itemDetails.data", "2", true}, "user data is only counted by default");
        potion.full = true;
        potion.data[0].value = std::string(dataWidth + 5, 'x');
        out = lines(potion);
        check(out.size() == 4 && out[2] == Line{"", "display: " + std::string(dataWidth, 'x') + "\xE2\x80\xA6", true}
                  && out[3] == Line{"", "Damage: 3", true},
              "the long form tells each top-level entry on one line, cut at the width");
        Item tagged{.tags = {std::string(30, 'a'), std::string(30, 'b')}};
        auto rows = lines(tagged);
        check(rows.size() == 3 && rows[0] == Line{"itemDetails.tags", "", true} && rows[1] == Line{"", "  " + std::string(30, 'a') + ", ", true}
                  && rows[2] == Line{"", "  " + std::string(30, 'b'), true},
              "tags that wrap go under their label, every row indented alike");
        auto grown = resized({100, 50}, 90, 4, 150, 9, 10), shrunk = resized({160, 100}, 150, 9, 90, 4, 10);
        check(grown.width == 160 && grown.height == 100 && shrunk.width == 100 && shrunk.height == 50 && lineCount("a\nb") == 2,
              "a tooltip's box follows the other text's lines and widest line, keeping its room around the text");
        check(keptInside(50, 100, 400) == 50 && keptInside(350, 100, 400) == 300 && keptInside(10, 500, 400) == 0,
              "a grown box moves back inside the screen, never past its start");
        check(stretched(0, 100, 40) == 0 && stretched(4, 100, 40) == 4 && stretched(96, 100, 40) == 136 && stretched(100, 100, 40) == 140,
              "a grown frame keeps its near corners and moves its far ones");
        Item big{.id = "x", .full = true};
        for (size_t i = 0; i < dataLines + 9; ++i) big.data.push_back({"k", "v"});
        auto cut = lines(big);
        check(cut.size() == 1 + dataLines + 1 && cut.back() == Line{"itemDetails.more", "9", true},
              "past the limit the rest of the user data is counted");
    }
}

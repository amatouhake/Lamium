#include "features/information/PlayerList.h"
void check(bool, char const*);
void playerListTests() {
    using namespace lamium::information::playerList;
    check(platformText(8) == "Win" && platformText(7) == "Win" && platformText(11) == "PS" && platformText(1) == "Android"
              && platformText(12) == "Switch" && platformText(-1) == "?" && platformText(42) == "?",
          "platforms read as short names, unknown values as ?");
    std::vector<Row> rows{{"zed"}, {"Alice"}, {"me", true}, {"bob"}};
    order(rows);
    check(rows[0].name == "me" && rows[1].name == "Alice" && rows[2].name == "bob" && rows[3].name == "zed",
          "you come first, then names ignoring case");
    auto one = plan(5, 100, 10, 400);
    check(one.columns == 1 && one.shown == 5, "a short list is one column");
    auto many = plan(75, 100, 10, 400);
    check(many.columns == 3 && many.shown == 60, "as many 20-row columns as fit, the rest left for 'and N more'");
    auto narrow = plan(75, 100, 10, 50);
    check(narrow.columns == 1 && narrow.shown == 20, "at least one column even when it does not fit");
    check(plan(0, 100, 10, 400).shown == 0, "no players, nothing shown");
    auto width = [](std::string const& s) { return static_cast<float>(s.size()); };
    check(fitName("short", 10, width) == "short", "a name that fits stays");
    check(fitName("Very_Long_Server_Nickname", 10, width) == "Very_Lo...", "a long name is cut with dots");
    auto glyphs = [](std::string const& s) {
        float n = 0;
        for (unsigned char c : s) n += (c & 0xC0) != 0x80;
        return n;
    };
    check(fitName("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a\xe3\x81\x8b", 5, glyphs) == "\xe3\x81\x82\xe3\x81\x84...",
          "cutting keeps whole UTF-8 characters");
    check(permissionTexture(2, false) == "textures/ui/permissions_op_crown"
              && permissionTexture(0, false) == "textures/ui/permissions_visitor_hand"
              && permissionTexture(3, false) == "textures/ui/permissions_custom_dots",
          "operators, visitors and custom permissions are always marked");
    check(permissionTexture(1, false).empty() && permissionTexture(1, true) == "textures/ui/permissions_member_star"
              && permissionTexture(std::nullopt, true).empty(),
          "members are marked only when asked; unknown levels never");
    check(distanceText(12.4) == "12 m" && distanceText(12.6) == "13 m" && distanceText(-1) == "-", "distance in whole meters");
}

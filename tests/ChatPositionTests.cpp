#include "features/information/ChatPosition.h"
#include "features/information/ScoreboardPosition.h"
#include "features/information/BossBarPosition.h"
#include "settings/SettingsStore.h"
#include "input/ToggleAction.h"
#include "app/PausedFeatures.h"
#include "settings/Options.h"
#include "ui/SettingsRows.h"
#include <algorithm>
#include <limits>
#include <chrono>
void check(bool, char const*);
void chatPositionTests() {
    using namespace lamium;
    using namespace ui;
    using information::chatPosition::placed;
    auto defaults = defaultHudElement(HudElementId::Chat);
    auto first = placed(640, 360, 256, 27, defaults);
    auto taller = placed(640, 360, 256, 72, defaults);
    check(first && taller && first->x == 0 && first->y + first->h == taller->y + taller->h,
          "chat grows upward without moving its bottom edge");
    using information::chatPosition::translated;
    auto before = translated(640, 360, {0, 60, 256, 27}, {2, 78}, defaults);
    auto after = translated(640, 360, {0, 60, 256, 18}, {2, 69}, defaults);
    check(before && after && before->x == after->x && before->y == after->y,
          "a surviving message stays in place when native layout removes an older line");
    auto background = translated(640, 360, {0, 60, 256, 18}, {0, 69}, defaults);
    check(after && background && after->x == background->x + 2 && after->y == background->y,
          "native text and background retain their offset after chat shrinks");
    check(!translated(640, 360, {0, 60, 256, 0}, {2, 69}, defaults)
          && !translated(640, 360, {0, 60, 256, 18}, {2, std::numeric_limits<float>::infinity()}, defaults),
          "incomplete native layout or invalid component coordinates are left alone");
    auto moved = placeAt(defaults, 190, 90, 256, 27, 640, 360);
    auto next = placed(640, 360, 256, 27, moved);
    check(next && next->x == 190 && next->y == 90, "a chat drop uses the shared HUD placement exactly");
    auto right = snapTo(defaults, Anchor::BottomRight);
    next = placed(800, 450, 320, 45, right);
    check(next && next->x + next->w == 800 - hudInset && next->y + next->h == 450 - hudInset,
          "chat edge anchors follow a resized screen and the game's new measured width");
    right.dx = 999; right.dy = -999;
    next = placed(640, 360, 256, 27, right);
    check(next && next->x == 384 && next->y == 0, "chat stays inside the screen after a stale offset");
    check(!placed(640, 360, 641, 20, defaults) && !placed(640, 360, 20, 361, defaults)
          && !placed(640, 360, 20, 0, defaults) && !placed(0, 360, 20, 20, defaults)
          && !placed(640, 360, std::numeric_limits<float>::quiet_NaN(), 20, defaults),
          "unmeasured, oversized or invalid chat geometry leaves vanilla placement intact");
    check(!decodeSettings("{}").information.placesChat() && input::defaultChord(input::Action::ToggleChatPosition).empty()
          && input::defaultChord(input::Action::ToggleGameHud).empty(),
          "older settings leave chat vanilla and the keys unbound");
    // Game HUD placement (L-321): one switch over the three parts.
    Settings grouped;
    check(!grouped.information.gameHud && grouped.information.chatPosition && grouped.information.paperDollPosition
          && grouped.information.scoreboardPosition && !grouped.information.placesChat(),
          "game HUD placement starts off with every part selected");
    auto legacyOn = decodeSettings(R"({"information":{"chatPosition":true}})");
    check(legacyOn.information.gameHud && legacyOn.information.placesChat() && !legacyOn.information.placesPaperDoll()
          && !legacyOn.information.placesScoreboard(),
          "a file from before the group keeps a moved chat and moves nothing else");
    auto legacyOff = decodeSettings(R"({"information":{"chatPosition":false}})");
    check(!legacyOff.information.gameHud && legacyOff.information.chatPosition && legacyOff.information.scoreboardPosition,
          "a file from before the group with the chat off gets the new defaults");
    auto partOff = decodeSettings(R"({"information":{"gameHud":true,"chatPosition":false}})");
    check(partOff.information.gameHud && !partOff.information.placesChat() && partOff.information.placesPaperDoll(),
          "a part left out of the group stays out");
    Settings value;
    value.hud.chat = moved;
    value.hud.info.dx = 19;
    check(input::toggleAction(value, input::Action::ToggleGameHud) && value.information.placesChat(), "the group has a toggle action");
    auto path = std::filesystem::temp_directory_path()
        / ("lamium-chat-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    writeSettings(path, value);
    auto restored = readSettings(path);
    std::filesystem::remove(path);
    check(restored.information.placesChat() && restored.hud.chat.anchor == moved.anchor
          && restored.hud.chat.dx == moved.dx && restored.hud.chat.dy == moved.dy && restored.hud.info.dx == 19,
          "chat enable and placement persist without changing another HUD element");
    auto active = paused::inUse(value);
    // The scoreboard is placed the same way (L-321).
    check(information::scoreboardPosition::sidebarControl("sidebar") && !information::scoreboardPosition::sidebarControl("main"),
          "the scoreboard is recognized by its sidebar panel");
    auto sidebar = defaultHudElement(HudElementId::Scoreboard);
    auto vanilla = placed(640, 360, 80, 50, sidebar);
    check(vanilla && vanilla->x == 560 && vanilla->y == 155, "the scoreboard's default place is the game's: the middle of the right edge");
    auto row = translated(640, 360, {560, 155, 80, 50}, {562, 165}, sidebar);
    check(row && row->x == 562 && row->y == 165, "at the default place a scoreboard part is not moved");
    auto shifted = translated(640, 360, {560, 155, 80, 50}, {562, 165}, placeAt(sidebar, 10, 20, 80, 50, 640, 360));
    check(shifted && shifted->x == 12 && shifted->y == 30, "a moved scoreboard carries its parts with it");
    check(hudGameDrawn(HudElementId::Scoreboard) && hudConditional(HudElementId::Scoreboard),
          "the scoreboard is the game's and shown as a frame in the editor");
    // Scale (L-321): the placed box and every part's place inside it scale together.
    auto big = defaults;
    big.scale = 150;
    auto grown = placed(640, 360, 256, 30, big);
    check(grown && grown->w == 384 && grown->h == 45 && grown->x == 0 && grown->y + grown->h == first->y + first->h,
          "a scaled chat grows from its anchored corner");
    auto part = translated(640, 360, {0, 60, 256, 30}, {2, 75}, big);
    check(part && part->x == 3 && part->y == grown->y + 22.5f, "a part keeps its place inside the scaled panel");
    big.scale = 200;
    check(!placed(640, 360, 400, 30, big) && !translated(640, 360, {0, 60, 400, 30}, {2, 75}, big),
          "a chat scaled past the screen is left as the game draws it");
    auto small = sidebar;
    small.scale = 50;
    auto shrunk = placed(640, 360, 80, 50, small);
    check(shrunk && shrunk->w == 40 && shrunk->x == 600 && shrunk->y == 167.5f, "a scaled scoreboard stays on its edge");
    check(information::chatPosition::zoom(defaults) == 1 && information::chatPosition::zoom(big) == 2, "the element's scale is a factor");
    Settings board;
    check(!board.information.placesScoreboard() && input::defaultChord(input::Action::ToggleScoreboardPosition).empty()
          && input::toggleAction(board, input::Action::ToggleGameHud) && board.information.placesScoreboard()
          && (settings::find("information.scoreboardPosition")->adjust(board, 1), board.information.hidesScoreboard())
          && (settings::find("information.scoreboardPosition")->adjust(board, 1), !board.information.placesScoreboard())
          && board.information.placesChat(),
          "the scoreboard follows the group, has an unbound key and can be left out alone");
    auto boardSaved = decodeSettings(R"({"information":{"scoreboardPosition":true},"hud":{"scoreboard":{"anchor":0,"dx":7,"dy":9,"scale":150}}})");
    check(boardSaved.information.placesScoreboard() && boardSaved.hud.scoreboard.dx == 7 && boardSaved.hud.scoreboard.scale == 150,
          "the scoreboard's switch, place and scale load");
    // The boss bars are placed the same way (L-323), by the box their shown
    // items fill rather than the grid's fixed 30 % height.
    {
        namespace boss = information::bossBarPosition;
        check(boss::gridControl("boss_health_grid") && !boss::gridControl("boss_health_panel") && !boss::gridControl("boss_hud_panel"),
              "the boss bars are recognized by their grid, not by the name the outer panel shares with each item");
        boss::Span two[] = {{2, 22}, {22, 42}};
        auto filled = boss::occupied({229, 2, 182, 108}, two);
        check(filled && filled->x == 229 && filled->y == 2 && filled->w == 182 && filled->h == 40,
              "the boss bar box runs from the grid's top to its lowest shown bar");
        check(!boss::occupied({229, 2, 182, 108}, {}), "with no bar shown there is nothing to place");
        auto bars = defaultHudElement(HudElementId::BossBars);
        auto vanillaBars = placed(640, 360, 182, 40, bars);
        check(vanillaBars && vanillaBars->x == 229 && vanillaBars->y == 2,
              "the boss bars' default place is the game's desktop one: top center, 2 units down");
        auto name = translated(640, 360, *filled, {229, 22}, bars);
        check(name && name->x == 229 && name->y == 22, "at the default place a boss bar part is not moved");
        auto low = bars;
        low.anchor = Anchor::BottomCenter;
        low.dy = -4;
        auto oneLow = placed(640, 360, 182, 20, low), threeLow = placed(640, 360, 182, 60, low);
        check(oneLow && threeLow && oneLow->y + oneLow->h == 356 && threeLow->y + threeLow->h == 356 && threeLow->y == 296,
              "boss bars anchored at the bottom stack upward as more appear");
        auto movedName = translated(640, 360, *filled, {229, 22}, low);
        check(movedName && movedName->x == 229 && movedName->y == 336, "moved boss bars carry each bar with them");
        auto bigBars = bars;
        bigBars.scale = 150;
        auto grownBars = placed(640, 360, 182, 20, bigBars);
        check(grownBars && grownBars->w == 273 && grownBars->h == 30 && grownBars->y == 2, "scaled boss bars grow from the top center");
        check(hudGameDrawn(HudElementId::BossBars) && hudConditional(HudElementId::BossBars),
              "the boss bars are the game's and shown as a frame in the editor");
        Settings withBars;
        check(withBars.information.bossBarPosition && !withBars.information.placesBossBars()
              && input::defaultChord(input::Action::HideBossBars).empty() && input::retired(input::Action::ToggleBossBarPosition)
              && input::toggleAction(withBars, input::Action::ToggleGameHud) && withBars.information.placesBossBars()
              && (settings::find("information.bossBarPosition")->adjust(withBars, -1), !withBars.information.placesBossBars())
              && withBars.information.placesScoreboard(),
              "the boss bars follow the group, keep the merged unbound key and can be left out alone");
        auto barsSaved = decodeSettings(R"({"information":{"gameHud":true,"bossBarPosition":false},"hud":{"bossBars":{"anchor":7,"dx":3,"dy":-5,"scale":125}}})");
        check(barsSaved.information.gameHud && !barsSaved.information.bossBarPosition && barsSaved.hud.bossBars.anchor == Anchor::BottomCenter
              && barsSaved.hud.bossBars.dy == -5 && barsSaved.hud.bossBars.scale == 125,
              "the boss bars' switch, place and scale load");
        auto older = decodeSettings(R"({"information":{"gameHud":true,"chatPosition":true}})");
        check(older.information.placesBossBars() && older.hud.bossBars.dy == 2,
              "a file from before the boss bars selects them at the game's place");
        writeSettings(path, barsSaved);
        auto roundTrip = readSettings(path);
        std::filesystem::remove(path);
        check(!roundTrip.information.bossBarPosition && roundTrip.hud.bossBars.scale == 125, "the boss bars' settings save");
    }
    // Showing and hiding from HUD layout (L-334).
    {
        Settings parts;
        auto const* chatRow = settings::find("information.chatPosition");
        auto mode = [&] { return std::get<settings::ChoiceValue>(chatRow->read(parts)).label; };
        check(mode() == "gameHudPart.placed", "a part is Placed by default");
        chatRow->adjust(parts, 1);
        bool hidden = mode() == "gameHudPart.hidden" && parts.information.chatPosition && !parts.information.showChat;
        chatRow->adjust(parts, 1);
        bool standard = mode() == "gameHudPart.vanilla" && !parts.information.chatPosition && parts.information.showChat;
        chatRow->adjust(parts, 1);
        check(hidden && standard && mode() == "gameHudPart.placed",
              "a part's row steps Placed, Hidden, Standard with its two saved switches");
        check(ui::optionAction("information.bossBarPosition") == input::Action::HideBossBars
              && ui::optionAction("information.chatPosition") == input::Action::ToggleChatPosition
              && !settings::find("information.showBossBars"),
              "each part is one row with one key; the boss bars keep the bound hide key");
    }
    {
        Settings shown;
        check(!settings::hudShown(shown, HudElementId::Chat) && !settings::hudShown(shown, HudElementId::BossBars)
              && !settings::hudShown(shown, HudElementId::PlayerList),
              "a game part not placed and the player list have no show/hide switch");
        check(settings::hudShown(shown, HudElementId::Info) == std::optional(false)
              && settings::hudShown(shown, HudElementId::Toast) == std::optional(true),
              "a Lamium element's show/hide is its own feature switch");
        shown.information.gameHud = true;
        *settings::hudShownField(shown, HudElementId::Scoreboard) = false;
        *settings::hudShownField(shown, HudElementId::Minimap) = true;
        check(shown.information.hidesScoreboard() && !shown.information.hidesChat() && shown.map.minimap
              && settings::hudShown(shown, HudElementId::Scoreboard) == std::optional(false),
              "the switch of a placed game part hides only that part");
        shown.information.scoreboardPosition = false;
        check(!shown.information.hidesScoreboard() && !settings::hudShown(shown, HudElementId::Scoreboard),
              "a part left out of the group is the game's again, shown");
        shown.information.scoreboardPosition = true;
        shown.information.gameHud = false;
        check(!shown.information.hidesScoreboard(), "the main switch off shows every part");
        auto hiddenSaved = decodeSettings(R"({"information":{"gameHud":true,"showChat":false,"showPaperDoll":false}})");
        check(hiddenSaved.information.hidesChat() && hiddenSaved.information.hidesPaperDoll()
              && !hiddenSaved.information.hidesScoreboard() && !hiddenSaved.information.hidesBossBars(),
              "the parts' show values load");
        auto migrated = decodeSettings(R"({"visuals":{"hideEffects":true}})");
        migrated.information.showBossBars = true;
        writeSettings(path, migrated);
        auto reread = readSettings(path);
        std::filesystem::remove(path);
        check(!reread.information.hidesBossBars() && reread.information.gameHud,
              "boss bars shown again after the move stay shown when saved and loaded");
        Settings standard;
        check(!settings::hudInEditor(standard, HudElementId::Chat) && settings::hudInEditor(standard, HudElementId::Info)
              && !settings::firstGameHudPart(standard),
              "with Game HUD placement off no game part is in the editor and its link selects none");
        standard.information.gameHud = true;
        standard.information.chatPosition = false;
        check(!settings::hudInEditor(standard, HudElementId::Chat) && settings::hudInEditor(standard, HudElementId::Scoreboard)
              && settings::firstGameHudPart(standard) == std::optional(HudElementId::PaperDoll),
              "a part left Standard is not in the editor; the link opens the first part that is");
        standard.information.showChat = false;
        standard.information.chatPosition = true;
        check(settings::hudInEditor(standard, HudElementId::Chat), "a hidden part stays in the editor");
        using hud_editor::hatch;
        auto lines = hatch({10, 20, 12, 6});
        check(!lines.empty() && std::all_of(lines.begin(), lines.end(), [](auto const& b) {
                  return b.x >= 10 && b.x + b.w <= 22 && b.y >= 20 && b.y + b.h <= 26;
              }), "the hatching stays inside the element");
        check(hatch({0, 0, 2000, 2000}).size() <= 400 && hatch({0, 0, 400, 200}).size() <= 400 && hatch({0, 0, 0, 5}).empty(),
              "the hatching costs about the same whatever the element's size");
    }
    check(paused::onUnverified("gameHud") == paused::Level::Partly
          && std::ranges::find(active, "gameHud") != active.end(),
          "game HUD placement is reported paused on an unverified executable");
}

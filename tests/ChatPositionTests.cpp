#include "features/information/ChatPosition.h"
#include "features/information/ScoreboardPosition.h"
#include "settings/SettingsStore.h"
#include "input/ToggleAction.h"
#include "app/PausedFeatures.h"
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
    check(!decodeSettings("{}").information.chatPosition && input::defaultChord(input::Action::ToggleChatPosition).empty(),
          "older settings leave chat vanilla and its new key unbound");
    Settings value;
    value.hud.chat = moved;
    value.hud.info.dx = 19;
    check(input::toggleAction(value, input::Action::ToggleChatPosition), "the chat switch has a toggle action");
    auto path = std::filesystem::temp_directory_path()
        / ("lamium-chat-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    writeSettings(path, value);
    auto restored = readSettings(path);
    std::filesystem::remove(path);
    check(restored.information.chatPosition && restored.hud.chat.anchor == moved.anchor
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
    check(hudGameDrawn(HudElementId::Scoreboard) && !hudScalable(HudElementId::Scoreboard) && hudConditional(HudElementId::Scoreboard),
          "the scoreboard is the game's, placed only, and shown as a frame in the editor");
    Settings board;
    check(!board.information.scoreboardPosition && input::defaultChord(input::Action::ToggleScoreboardPosition).empty()
          && input::toggleAction(board, input::Action::ToggleScoreboardPosition) && board.information.scoreboardPosition,
          "scoreboard position starts off, has an unbound key and toggles");
    auto boardSaved = decodeSettings(R"({"information":{"scoreboardPosition":true},"hud":{"scoreboard":{"anchor":0,"dx":7,"dy":9,"scale":150}}})");
    check(boardSaved.information.scoreboardPosition && boardSaved.hud.scoreboard.dx == 7 && boardSaved.hud.scoreboard.scale == 100,
          "the scoreboard's switch and place load; it has no scale");
    check(paused::onUnverified("scoreboardPosition") == paused::Level::Fully, "scoreboard position is reported paused on an unverified executable");
    check(paused::onUnverified("chatPosition") == paused::Level::Fully
          && std::ranges::find(active, "chatPosition") != active.end(),
          "chat position is reported paused on an unverified executable");
}

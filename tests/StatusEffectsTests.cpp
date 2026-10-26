#include "features/information/StatusEffects.h"
#include "settings/SettingsStore.h"
#include "ui/HudElement.h"
void check(bool, char const*);
void statusEffectsTests() {
    using namespace lamium::information::statusEffects;
    using Ids = std::vector<unsigned>;
    Effect speed{1, 3300, 1, false, false, "Speed"}, poison{19, 80, 0, false, true, "Poison"},
        vision{16, -1, 0, false, false, "Night Vision"}, beacon{3, 150, 1, true, false, "Haste"},
        regen{10, 160, 0, false, false, "Regeneration"};
    // A fresh HUD: shortest first so the next to run out is on top.
    check(rowIds(arrange({}, {vision, speed, poison, beacon, regen})) == Ids{19, 3, 10, 1, 16},
          "a new row list runs shortest first with infinite ones last");
    // L-247: beacon renewals must not trade rows. Poison 80 -> 40 and the
    // beacon's 150 -> 300 would put the beacon below regeneration's 160.
    std::vector<Effect> renewed = arrange(Ids{19, 3, 10},
        {Effect{19, 40, 0, false, true, "Poison"}, Effect{3, 300, 1, true, false, "Haste"},
         Effect{10, 160, 0, false, false, "Regeneration"}});
    check(rowIds(renewed) == Ids{19, 3, 10}, "a beacon's renewed duration keeps every row in place");
    check(renewed.size() == 3u && renewed[1].ticks == 300 && renewed[1].name == "Haste",
          "a row keeps its place and still shows the current countdown");
    check(rowIds(arrange(rowIds(renewed), renewed)) == Ids{19, 3, 10}, "arranging the same effects again moves nothing");
    // A longer potion of an effect already on screen replaces the time, not
    // the row, even when it now outlasts a row above it.
    check(rowIds(arrange(Ids{10, 1},
                  {Effect{1, 3300, 1, false, false, "Speed"}, Effect{10, 6000, 0, false, false, "Regeneration"}}))
              == Ids{10, 1},
          "a longer potion replaces the time but not the row");
    // New effects enter above the first row with a longer time, an ended
    // effect closes up, an infinite one goes last and nothing is hidden.
    std::vector<Effect> mixed = arrange(Ids{19, 3, 10, 1},
        {Effect{19, 40, 0, false, true, "Poison"}, Effect{3, 300, 1, true, false, "Haste"},
         Effect{10, 160, 0, false, false, "Regeneration"}, Effect{2, 120, 0, false, true, "Slowness"},
         Effect{16, -1, 0, false, false, "Night Vision"}});
    check(rowIds(mixed) == Ids{19, 2, 3, 10, 16} && mixed.size() == 5u,
          "a new effect enters by remaining time, an ended one closes up and an infinite one goes last");
    check(rowIds(arrange(Ids{19, 3}, {Effect{19, 40, 0, false, true, "Poison"}})) == Ids{19},
          "an effect that ended takes its row with it");
    check(timeText(3300) == "2:45" && timeText(80) == "0:04" && timeText(1) == "0:01" && timeText(0) == "0:00",
          "times read M:SS and round up while active");
    check(timeText(20 * 3725) == "1:02:05", "an hour or more reads H:MM:SS");
    check(timeText(-1).empty(), "an infinite effect has no time text (the HUD writes a word)");
    check(runningOut(poison) && runningOut(Effect{1, 199}) && !runningOut(Effect{1, 200}),
          "under 10 seconds is running out");
    check(!runningOut(beacon) && !runningOut(vision), "beacon and infinite effects never look like they run out");
    check(levelText(0) == "1" && levelText(-1) == "1" && levelText(1) == "2" && levelText(4) == "5"
              && levelText(10) == "11",
          "every level shows in Arabic numerals, level 1 included");
    check(iconPath(1) == "textures/ui/speed_effect" && iconPath(25) == "textures/ui/poison_effect"
              && iconPath(37) == "textures/ui/breath_of_the_nautilus_effect" && iconPath(6).empty() && iconPath(0).empty()
              && iconPath(38).empty(),
          "effect ids resolve to the vanilla icons; instant effects and unknown ids have none");

    using namespace lamium;
    Settings defaults;
    check(!defaults.information.effectsHud && defaults.information.effectsNames && defaults.information.effectsWarning
              && defaults.information.effectsHideVanilla
              && defaults.hud.statusEffects.anchor == ui::Anchor::MiddleRight,
          "the effects HUD starts off at the right middle, with names, warning color and vanilla icons hidden");
    auto loaded = decodeSettings(R"({"information":{"effectsHud":true,"effectsNames":false,"effectsWarning":false,
        "effectsHideVanilla":false},"hud":{"statusEffects":{"anchor":0,"background":2}}})");
    check(loaded.information.effectsHud && !loaded.information.effectsNames && !loaded.information.effectsWarning
              && !loaded.information.effectsHideVanilla && loaded.hud.statusEffects.anchor == ui::Anchor::TopLeft
              && loaded.hud.statusEffects.background == ui::ElementBackground::Line,
          "effects HUD options and its per-line background load");
}

#include "ui/HudLayout.h"
#include "features/information/PlayerInfo.h"
#include "features/information/NetworkInfo.h"
#include "features/information/TargetRows.h"
#include "features/information/DebugView.h"
#include <limits>
void check(bool, char const*);
void hudLayoutTests() {
    using lamium::ui::HudLayout;
    lamium::Settings::Information preferences;
    preferences.horizontal = 37;
    preferences.debug = true;
    auto debug = lamium::information::debugProfile(preferences);
    check(debug.hud && debug.target && debug.biome && debug.ping && debug.targetStates,
          "debug profile enables shared information providers");
    check(!preferences.hud && !preferences.target && preferences.horizontal == 37,
          "debug profile preserves normal HUD choices");
    preferences.debug = false;
    auto normal = lamium::information::debugProfile(preferences);
    check(!normal.hud && normal.horizontal == 37, "disabling debug restores normal rendering profile");
    for (float width : {80.f,320.f,640.f}) {
        auto columnWidth = std::min(230.f,width/2-8);
        auto first = HudLayout::fit(width,360,0,0,8,columnWidth);
        auto second = HudLayout::fit(width,360,100,0,9,columnWidth);
        check(first.x+first.width < second.x && second.x+second.width <= width-4,
              "debug columns remain separated on narrow screens");
    }
    lamium::information::TargetInfo target{"Stone","minecraft:stone",{"a: 0","b: 1","c: 2","d: 3","e: 4","f: 5","g: 6"}};
    using lamium::information::targetRows;
    check(targetRows(target,true,0).lines.empty(), "no space draws no target rows");
    auto compact = targetRows(target,true,4);
    check(compact.lines.size() == 3 && compact.lines.back() == "a: 0" && compact.showOmitted && compact.omittedStates == 6,
          "small target view reserves its last row for omitted state count");
    auto full = targetRows(target,true,9);
    check(full.lines.size() == 8 && full.showOmitted && full.omittedStates == 1, "target details cap at six states");
    target.states.resize(2);
    auto exact = targetRows(target,false,3);
    check(exact.lines.size() == 3 && !exact.showOmitted && exact.omittedStates == 0, "exact fit does not hide a state for an unnecessary marker");
    using lamium::information::facingKey;
    using lamium::information::measuredPing;
    check(!measuredPing(-1) && !measuredPing(std::numeric_limits<std::int64_t>::min()), "missing ping is unavailable");
    check(measuredPing(0) == 0 && measuredPing(125) == 125, "ping preserves measured milliseconds including zero");
    using lamium::information::lightLevels;
    check(lightLevels(0,15)->sky == 0 && lightLevels(0,15)->block == 15, "sky and block light retain independent values");
    check(lightLevels(15,0).has_value() && lightLevels(0,0).has_value(), "darkness is available data");
    check(!lightLevels(-1,0) && !lightLevels(0,-1) && !lightLevels(16,0) && !lightLevels(0,255),
          "invalid light stays unavailable instead of being clamped to darkness");
    check(facingKey(0) == "facing.south" && facingKey(90) == "facing.west"
        && facingKey(180) == "facing.north" && facingKey(-90) == "facing.east", "yaw maps cardinal axes");
    check(facingKey(360) == facingKey(0) && facingKey(-450) == facingKey(-90), "unwrapped yaw maps consistently");
    check(facingKey(44.9) == "facing.south" && facingKey(45) == "facing.west", "cardinal sector boundaries are deterministic");
    check(!facingKey(std::numeric_limits<double>::infinity()), "invalid yaw stays unavailable");
    for (float width : {120.f,640.f}) for (float height : {100.f,360.f})
        for (float horizontal : {0.f,50.f,100.f}) for (float vertical : {0.f,50.f,100.f}) {
            auto layout = HudLayout::fit(width,height,horizontal,vertical,2);
            check(layout.lines == 2 && layout.x >= 4 && layout.y >= 4, "HUD fits its requested lines");
            check(layout.x+layout.width <= width-4 && layout.y+layout.lines*14 <= height-4,
                  "HUD anchor preserves margins at every corner");
        }
    check(HudLayout::fit(640,20,0,0,2).lines == 0, "too-short HUD does not draw clipped lines");
    check(HudLayout::fit(640,360,0,0,0).lines == 0, "disabling every provider hides HUD content");
    auto left = HudLayout::fit(640,360,0,0,2), right = HudLayout::fit(640,360,100,100,2);
    check(right.x > left.x && right.y > left.y, "HUD is not fixed to the top-left corner");
}

#include "ui/HudLayout.h"
#include "features/information/PlayerInfo.h"
#include <limits>
void check(bool, char const*);
void hudLayoutTests() {
    using lamium::ui::HudLayout;
    using lamium::information::facingKey;
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

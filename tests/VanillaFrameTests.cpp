#include "ui/VanillaFrame.h"
#include <cmath>
#include <string>
#include <vector>
void check(bool, char const*);
namespace {
using namespace lamium::ui::vanilla_frame;
// Each unit cell as the last rect over it draws it: ' ' open, '.' fill, 'W' light, 'd' dark, '#' edge, 'P' border.
std::vector<std::string> raster(std::vector<Rect> const& rects, int w, int h) {
    std::vector<std::string> rows(static_cast<size_t>(h), std::string(static_cast<size_t>(w), ' '));
    for (auto const& r : rects)
        for (int y = static_cast<int>(r.y); y < static_cast<int>(r.y + r.h); ++y)
            for (int x = static_cast<int>(r.x); x < static_cast<int>(r.x + r.w); ++x) {
                if (x < 0 || y < 0 || x >= w || y >= h) continue;
                char const c = r.part == Part::Fill ? '.' : r.part == Part::Light ? 'W' : r.part == Part::Dark ? 'd' : r.part == Part::Edge ? '#' : 'P';
                rows[static_cast<size_t>(y)][static_cast<size_t>(x)] = c;
            }
    return rows;
}
}
void vanillaFrameTests() {
    // A 12x10 window, as the game's chest window measured unit by unit (2026-11-19).
    auto const window = raster(lamium::ui::vanilla_frame::window(0, 0, 12, 10), 12, 10);
    check(window == std::vector<std::string>{
              "   ######   ",
              "  #WWWWWW#  ",
              " #WWWWWWW.# ",
              "#WWW.....dd#",
              "#WW......dd#",
              "#WW......dd#",
              "#WW.....ddd#",
              " #ddddddddd#",
              "  #ddddddd# ",
              "   #######  ",
          }, "the window's edge, light, shadow and cut corners are the game's");
    // A 10x10 tooltip, as the game's measured (2026-11-19): upper right and lower left rounder.
    auto const tooltip = raster(lamium::ui::vanilla_frame::tooltip(0, 0, 10, 10), 10, 10);
    check(tooltip == std::vector<std::string>{
              "  .....   ",
              " .PPPPP.  ",
              ".P....PP. ",
              ".P.....PP.",
              ".P......P.",
              ".P......P.",
              ".PP.....P.",
              " .PP....P.",
              "  .PPPPP. ",
              "   .....  ",
          }, "the tooltip's fill, purple line and corners are the game's");
    // A 10-wide tab 6 high: the window's top corners and light, open below; the chosen one's shadow at its right.
    check(raster(lamium::ui::vanilla_frame::tab(0, 0, 10, 6, true), 10, 6) == std::vector<std::string>{
              "   ####   ",
              "  #WWWW#  ",
              " #WWWWW.# ",
              "#WWW...dd#",
              "#WW....dd#",
              "#WW....dd#",
          }, "a tab has the window's stepped top corners and two units of light; the chosen one is shadowed at its right");
    check(tooltipHeight(1) == 18 && tooltipWidth(10) == 20, "one line of tooltip text sits in the game's 18 units");

    // A 60x40 tooltip on a 400x300 screen: right of and below the pointer while it fits.
    auto at = tooltipAt(100, 100, 60, 40, 400, 300);
    check(at.x == 108 && at.y == 108, "a tooltip that fits sits 8 units right of and below the pointer");
    auto covers = [](TooltipAt a, float w, float h, float px, float py) { return px >= a.x && px < a.x + w && py >= a.y && py < a.y + h; };
    // Near the lower right corner it flips left and above instead of being pushed back over the pointer.
    at = tooltipAt(380, 290, 60, 40, 400, 300);
    check(at.x == 312 && at.y == 242, "near the lower right corner the tooltip flips left of and above the pointer");
    check(!covers(at, 60, 40, 380, 290), "a tooltip flipped at the lower right corner keeps off the pointer");
    at = tooltipAt(20, 290, 60, 40, 400, 300);
    check(at.x == 28 && at.y == 242, "near the lower left corner the tooltip stays right and flips above");
    at = tooltipAt(380, 10, 60, 40, 400, 300);
    check(at.x == 312 && at.y == 18, "near the upper right corner the tooltip flips left and stays below");
    // The margin of 2 counts: a box that ends exactly 2 units from the edge stays on its first side.
    check(tooltipAt(330, 100, 60, 40, 400, 300).x == 338, "a tooltip reaching just to the margin is not flipped");
    check(tooltipAt(331, 100, 60, 40, 400, 300).x == 263, "a tooltip one unit past the margin is flipped");
    // Taller than the room above and below: pushed onto the screen, but still beside the pointer, not over it.
    at = tooltipAt(380, 150, 60, 200, 400, 300);
    check(at.x == 312 && at.y == 98 && !covers(at, 60, 200, 380, 150), "a tooltip too tall for either side is pushed onto the screen beside the pointer");
    // Larger than the screen: kept from its top and left edge.
    at = tooltipAt(200, 150, 500, 400, 400, 300);
    check(at.x == 2 && at.y == 2, "a tooltip larger than the screen starts at the margin");
    // A pointer between units still gives a box on whole units.
    at = tooltipAt(100.4f, 100.6f, 60, 40, 400, 300);
    check(at.x == std::round(at.x) && at.y == std::round(at.y), "a tooltip lies on whole units");
}

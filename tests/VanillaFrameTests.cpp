#include "ui/VanillaFrame.h"
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
    auto const tooltip = raster(lamium::ui::vanilla_frame::tooltip(0, 0, 8, 7), 8, 7);
    check(tooltip == std::vector<std::string>{
              "  ....  ",
              " .PPPP. ",
              ".P....P.",
              ".P....P.",
              ".P....P.",
              " .PPPP. ",
              "  ....  ",
          }, "the tooltip's rounded fill and purple line are the game's");
    check(tooltipHeight(1) == 18 && tooltipWidth(10) == 22, "one line of tooltip text sits in the game's 18 units");
}

#include "features/map/WaypointStore.h"
#include "features/map/Waypoints.h"
#include "app/SessionIds.h"
#include "ui/WaypointPromptLayout.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <string>
void check(bool, char const*);
namespace {
using namespace lamium::map;
bool near(double a, double b) { return std::abs(a - b) < 1e-6; }
void basics() {
    check(nextColor(-1) == 0 && nextColor(0) == 1 && nextColor(11) == 0 && nextColor(99) == 0,
          "new waypoints take the next color, wrapping");
    std::vector<Waypoint> existing{{"Waypoint 1"}, {"Waypoint 3"}};
    auto name = [](int n) { return std::format("Waypoint {}", n); };
    check(defaultWaypointName(existing, name) == "Waypoint 2" && defaultWaypointName({}, name) == "Waypoint 1",
          "the default name takes the lowest free number");
    auto same = shownPosition(10, 64, -20, 0, 0, false);
    check(same && near(same->x, 10.5) && near(same->z, -19.5) && !same->scaled, "a waypoint shows at its block center");
    check(!shownPosition(10, 64, -20, 1, 0, false) && !shownPosition(10, 64, -20, 2, 0, true),
          "other dimensions are hidden, and the End never scales");
    auto inNether = shownPosition(80, 64, -160, 0, 1, true);
    auto inOverworld = shownPosition(10, 70, -20, 1, 0, true);
    check(inNether && near(inNether->x, 10.5) && near(inNether->z, -19.5) && inNether->scaled
          && inOverworld && near(inOverworld->x, 80.5) && near(inOverworld->z, -159.5) && inOverworld->y == 70,
          "with the option, Overworld waypoints show in the Nether at 1/8 and Nether ones at 8x");
}
void markers() {
    auto view = ViewTransform::northUp();
    auto inside = mapMarker(view, 0, 0, 10, 0, 128, 256, false, 6);
    check(inside.inside && near(inside.x, 148) && near(inside.y, 128), "a near waypoint sits where it is");
    auto east = mapMarker(view, 0, 0, 500, 0, 128, 256, false, 6);
    check(!east.inside && near(east.x, 250) && near(east.y, 128), "a far one sits on the edge toward it");
    auto corner = mapMarker(view, 0, 0, 500, 500, 128, 256, false, 6);
    check(near(corner.x, 250) && near(corner.y, 250), "a far diagonal one sits in the corner of a square map");
    auto round = mapMarker(view, 0, 0, 500, 500, 128, 256, true, 6);
    check(!round.inside && near(std::hypot(round.x - 128, round.y - 128), 122) && near(round.x, round.y),
          "on a round map it sits on the circle");
    std::vector<std::uint32_t> image(32 * 32, 0);
    drawDiamond(image, 32, 16, 16, 12, waypointColors[6]);
    check(image[16 * 32 + 16] == waypointColors[6] && channel(image[16 * 32 + 21], 3) > 0 && channel(image[16 * 32 + 21], 2) < 100
          && image[10 * 32 + 10] == 0, "a colored diamond with a black edge, corners clear");
    std::vector<std::uint32_t> cross(32 * 32, 0);
    drawCross(cross, 32, 16, 16, 12);
    check(channel(cross[16 * 32 + 16], 0) > 200 && channel(cross[16 * 32 + 16], 1) < 80 && cross[16 * 32 + 22] == 0
          && channel(cross[11 * 32 + 11], 3) > 0, "a red cross with a dark edge, gaps between its arms");
    CameraView camera; // At the origin looking south (+z), 90 degrees across.
    auto ahead = project(camera, 0, 0, 10, 400, 200);
    check(ahead && near(ahead->x, 200) && near(ahead->y, 100) && near(ahead->depth, 10), "straight ahead is the center");
    auto right = project(camera, 5, 5, 10, 400, 200);
    check(right && near(right->x, 300) && near(right->y, 50), "right and up of center");
    check(!project(camera, 0, 0, -10, 400, 200) && !project(camera, 30, 0, 10, 400, 200), "behind or outside is not drawn");
    check(project(camera, 10.5, 0, 10, 400, 200, 12).has_value(), "a margin keeps markers at the very edge");
    check(nearCrosshair(210, 110, 400, 200) && !nearCrosshair(240, 100, 400, 200), "names show near the crosshair");
    check(worldMarkersShown(0, false) && !worldMarkersShown(0, true) && !worldMarkersShown(1, false)
          && worldMarkersShown(1, true) && !worldMarkersShown(2, false) && !worldMarkersShown(2, true),
          "the key flips always-shown and held-only markers, and does nothing when off");
    auto rows = diamondRows(7);
    check(rows.size() == 7 && rows[0] == 0 && rows[3] == 3 && rows[6] == 0 && diamondRows(6).size() == 7,
          "a diamond is odd-sized, widest in the middle");
    DeathWatch watch;
    check(!watch.update(true) && watch.update(false) && !watch.update(false) && !watch.update(true) && watch.update(false),
          "a death is noticed once per death");
}
void storage() {
    check(serverFileName("Play.Example.com", 19132) == "play.example.com_19132.json", "server files use host and port");
    check(serverFileName("::1", 19132) == "__1_19132.json" || serverFileName("::1", 19132).empty(),
          "unsafe characters are replaced");
    check(serverFileName("", 19132).empty() && serverFileName("host", 0).empty() && serverFileName("..", 1).empty(),
          "nothing to identify gives no file");
    WaypointSet set;
    set.waypoints.push_back({"家", 6, -60, 70, 40, 0, true});
    set.waypoints.push_back({"Fortress", 0, 30, 72, -140, 1, false});
    set.death = DeathPoint{118, 61, -48, 0};
    set.lastColor = 6;
    check(decodeWaypoints(encodeWaypoints(set)) == set, "waypoints, the death point and the last color round-trip");
    auto partial = decodeWaypoints(R"({"version":1,"waypoints":[{"name":"a","x":1},{"x":2},{"name":"b","color":99,"dimension":7}]})");
    check(partial.waypoints.size() == 2 && partial.waypoints[0].visible && partial.waypoints[0].x == 1
          && partial.waypoints[1].color == 11 && partial.waypoints[1].dimension == 2 && !partial.death,
          "missing fields take defaults, entries without a name are dropped, values are clamped");
    bool rejected = false;
    try { decodeWaypoints(R"({"version":2})"); } catch (...) { rejected = true; }
    check(rejected, "another version is rejected");
    auto path = std::filesystem::temp_directory_path()
        / ("lamium-waypoints-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())) / "w.json";
    writeWaypoints(path, set);
    check(readWaypoints(path) == set, "waypoints survive the disk");
    std::error_code ignored;
    std::filesystem::remove_all(path.parent_path(), ignored);
}
}
void prompt() {
    using lamium::ui::WaypointPromptLayout;
    auto l = WaypointPromptLayout::at(400, 240);
    check(l.left == 93 && l.top == 72, "the prompt is centered");
    check(l.hit(l.left + 20, l.fieldY() + 5).part == WaypointPromptLayout::Part::Field, "the name field");
    auto swatch = l.hit(l.swatchX(5) + 3, l.swatchY() + 3);
    check(swatch.part == WaypointPromptLayout::Part::Swatch && swatch.swatch == 5, "the sixth color");
    check(l.hit(l.addX() + 5, l.buttonY() + 5).part == WaypointPromptLayout::Part::Add
          && l.hit(l.cancelX() + 5, l.buttonY() + 5).part == WaypointPromptLayout::Part::Cancel, "the two buttons");
    check(l.hit(l.left - 5, l.top - 5).part == WaypointPromptLayout::Part::None, "outside hits nothing");
    check(l.swatchX(11) + WaypointPromptLayout::swatch <= l.left + WaypointPromptLayout::width - WaypointPromptLayout::pad
          && l.addX() > l.left + WaypointPromptLayout::pad, "colors and buttons fit inside the panel");
    auto tiny = WaypointPromptLayout::at(100, 60);
    check(tiny.left == 0 && tiny.top == 0, "a tiny screen keeps the panel on screen");
}
void order() {
    std::vector<Waypoint> list{{"far", 0, 100, 64, 0, 0}, {"nether", 0, 1, 64, 1, 1}, {"near", 0, 5, 64, 0, 0},
                               {"end", 0, 0, 64, 0, 2}, {"alpha", 0, 9, 64, 9, 1}};
    auto order = waypointOrder(list, 0, 0, 0);
    check(order.size() == 5 && order[0] == 2 && order[1] == 0, "this dimension first, nearest first");
    check(order[2] == 4 && order[3] == 3 && order[4] == 1, "then the others by name");
}
void waypointTests() {

    {
        // Session ids (L-139): same names and places stay apart; deleting or
        // reordering others keeps an entry's id; copies get their own.
        using lamium::map::Waypoint;
        std::uint64_t next = 1;
        std::vector<Waypoint> list{{"Home", 0, 1, 2, 3}, {"Home", 0, 1, 2, 3}, {"Mine", 1, 9, 9, 9}};
        lamium::assignSessionIds(list, next);
        check(list[0].id && list[1].id && list[0].id != list[1].id && list[2].id != list[1].id,
              "entries with the same name and place get different ids");
        auto mine = list[2].id;
        list.erase(list.begin());
        std::swap(list[0], list[1]);
        lamium::assignSessionIds(list, next);
        check(lamium::indexOfId(list, mine) == 0 && list[0].id == mine, "deleting and reordering others keeps an entry's id");
        list.push_back(list[0]);
        list.push_back({"New", 0, 0, 0, 0});
        lamium::assignSessionIds(list, next);
        check(list[2].id != mine && list[3].id && list[3].id != list[2].id && list[0].id == mine,
              "a copy and a new entry get new ids; the original keeps its own");
        check(lamium::indexOfId(list, 999) == -1 && lamium::indexOfId(list, 0) == -1, "an id that no longer exists finds nothing");
        auto decoded = lamium::map::decodeWaypoints(lamium::map::encodeWaypoints({list, std::nullopt, -1}));
        check(decoded.waypoints.size() == list.size()
              && std::all_of(decoded.waypoints.begin(), decoded.waypoints.end(), [](Waypoint const& w) { return w.id == 0; }),
              "ids are not saved");
    }
    order();
    prompt();
    basics();
    markers();
    storage();
}

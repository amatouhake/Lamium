#include <map>
#include "features/map/MapThumbnail.h"
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
    check(hollowSize(5) == 3 && hollowSize(7) == 3 && hollowSize(25) == 13 && hollowSize(1) == 1,
          "a temporary mark's middle is about half the colored diamond, odd-sized");
    DeathWatch watch;
    check(!watch.update(true) && watch.update(false) && !watch.update(false) && !watch.update(true) && watch.update(false),
          "a death is noticed once per death");
}
void arrival() {
    std::vector<Waypoint> list{{"here", 0, 0, 64, 0, 0, true, true, 1}, {"far", 0, 100, 64, 0, 0, true, true, 2},
                               {"kept", 0, 3, 64, 0, 0, true, false, 3}, {"nether", 0, 0, 64, 0, 1, true, true, 4}};
    // One frame as arrival removal runs it: plan, then keep the state.
    auto step = [](ArrivalWatch& watch, std::vector<Waypoint> const& waypoints, int dimension, double x, double y,
                   double z, double distance) {
        auto plan = watch.plan(waypoints, dimension, x, y, z, distance);
        auto reached = plan.reached;
        watch.apply(std::move(plan));
        return reached;
    };
    ArrivalWatch watch;
    check(step(watch, list, 0, .5, 64, .5, 6).empty(), "nothing is reached where it was added");
    check(step(watch, list, 0, 50.5, 64, .5, 6).empty(), "one already near counts only after leaving it");
    auto reached = step(watch, list, 0, .5, 64, .5, 6);
    check(reached.size() == 1 && reached[0] == 1, "coming back within reach removes it");
    ArrivalWatch high;
    step(high, list, 0, 50, 64, 0, 6);
    check(step(high, list, 0, 100.5, 74, .5, 6).empty() && step(high, list, 0, 100.5, 69, .5, 6).size() == 1,
          "height counts: 10 blocks above is not there, 5 is");
    ArrivalWatch off;
    step(off, list, 0, 50, 64, 0, 0);
    check(step(off, list, 0, .5, 64, .5, 0).empty(), "0 never removes");
    ArrivalWatch others;
    step(others, list, 0, 50, 64, 0, 6);
    auto near = step(others, list, 0, 3.5, 64, .5, 6);
    check(near.size() == 1 && near[0] == 1, "normal waypoints and other dimensions are never reached");
}
// L-328: a death is a waypoint with a cross.
void deathWaypoints() {
    WaypointSet set;
    set.waypoints.push_back({"home", 3, 1, 64, 1, 0, true});
    check(recordDeath(set, DeathPoint{10, 70, -5, 1}, "Death point", true) && set.waypoints.size() == 2 && set.waypoints[1].death
              && set.waypoints[1].temporary && set.waypoints[1].color == deathColorIndex && set.waypoints[1].dimension == 1,
          "a death adds a red waypoint of the set kind where the player died");
    recordDeath(set, DeathPoint{20, 70, -5, 0}, "Death point", false);
    check(set.waypoints.size() == 3 && set.waypoints[1].x == 10 && set.waypoints[2].x == 20 && !set.waypoints[2].temporary
              && set.waypoints[0].name == "home",
          "the next death adds its own and leaves the earlier ones alone (L-332)");
    check(decodeWaypoints(encodeWaypoints(set)) == set, "the cross mark is saved");
    check(deathName("Death point", 3, 7, 9, 5) == "Death point 03-07 09:05", "a death's waypoint is named with its local date and time (L-333)");
    WaypointSet full;
    full.waypoints.resize(maxWaypoints);
    check(!recordDeath(full, DeathPoint{}, "x", true), "a full list takes no death waypoint");
}
// L-276: the watch keeps a reached waypoint armed until its removal is saved.
void arrivalSave() {
    WaypointSet set;
    set.waypoints = {{"here", 0, 0, 64, 0, 0, true, true, 7}, {"far", 0, 40, 64, 0, 0, true, true, 8}};
    // A save that fails first must not lose the waypoint: a later frame tries
    // again and announces it once the store works.
    struct Case { bool firstSave; size_t firstAnnounced; size_t keptAfterFirst; char const* what; };
    for (auto c : {Case{false, 0, 2, "a failed save keeps the waypoint, so a later frame announces it"},
                   Case{true, 1, 1, "a saved removal announces the waypoint once and keeps the other one"}}) {
        ArrivalWatch watch;
        WaypointSet live = set;
        bool saves = c.firstSave;
        auto save = [&](WaypointSet const& candidate) {
            if (saves) live = candidate;
            return saves;
        };
        auto step = [&](double x) { return arrivalStep(watch, live, 0, x, 64, .5, 6, save); };
        check(step(50.5).empty(), "nothing is reached while away, and leaving arms it");
        check(step(.5).size() == c.firstAnnounced && live.waypoints.size() == c.keptAfterFirst, c.what);
        saves = true; // The store works from now on.
        check(step(.5).size() == (c.firstSave ? 0u : 1u), "it is announced on the frame its removal is saved");
        check(live.waypoints.size() == 1 && live.waypoints[0].id == 8, "the waypoint that was not reached stays");
    }
}
void storage() {
    check(serverFileName("Play.Example.com", 19132) == "play.example.com_19132.json", "server files use host and port");
    check(serverFileName("::1", 19132) == "__1_19132.json" || serverFileName("::1", 19132).empty(),
          "unsafe characters are replaced");
    check(serverFileName("", 19132).empty() && serverFileName("host", 0).empty() && serverFileName("..", 1).empty(),
          "nothing to identify gives no file");
    {
        // The Saved data page's map picture (L-343): three regions across, eight blocks to a pixel.
        namespace th = lamium::map::thumbnail;
        th::Area area{{2, -1}};
        check(th::side == 96 && area.originX() == 256 && area.originZ() == -512 && area.region(0, 0) == lamium::map::RegionKey{1, -2}
              && area.region(2, 2) == lamium::map::RegionKey{3, 0}, "the picture is the center region and the ring around it");
        auto middle = area.at(256 + 384, -512 + 384);
        check(middle && std::abs(middle->first - .5f) < .01f && std::abs(middle->second - .5f) < .01f
              && !area.at(255, 0) && !area.at(256 + 768, 0) && area.at(256, -512), "a block finds its place in the picture, or is outside it");
        std::vector<std::uint32_t> shaded(static_cast<size_t>(lamium::map::regionColumns), 0);
        // One pixel's 8x8 blocks: half recorded as (200, 100, 0), half not recorded.
        for (int z = 0; z < 8; ++z) for (int x = 0; x < 4; ++x) shaded[static_cast<size_t>(z * 256 + x)] = lamium::map::packColor(200, 100, 0);
        auto image = th::blank();
        th::blit(image, shaded, 1, 2);
        auto const first = image[static_cast<size_t>((2 * 32) * 96 + 32)];
        check(first == lamium::map::packColor(200, 100, 0) && image[static_cast<size_t>((2 * 32) * 96 + 33)] == 0 && image[0] == 0,
              "a pixel is the average of its recorded blocks and clear where none is recorded");
        auto untouched = image;
        th::blit(image, shaded, 3, 0);
        th::blit(image, std::vector<std::uint32_t>(4, 0), 0, 0);
        check(image == untouched, "a region outside the picture or of the wrong size changes nothing");
    }
    {
        namespace sv = lamium::map::servers;
        check(sv::hostOf("http://198.51.100.20:19135") == "198.51.100.20" && sv::hostOf("https://featured.example.com:19132") == "featured.example.com"
              && sv::hostOf("Play.Example.com") == "Play.Example.com" && sv::hostOf("host:19132/path") == "host"
              && sv::hostOf("[::1]:19132") == "::1" && sv::hostOf("::1") == "::1" && sv::hostOf("").empty(),
              "the host is the address without scheme, port and path");
        check(sv::worldKey("AbCd1234EfG=") == "world_41624364313233344566473d" && sv::worldKey("").empty()
              && sv::worldKey("a/b") != sv::worldKey("a_b"), "a world without an address is named by its level id, exactly");
        std::map<std::string, long long> disk;
        int weighed = 0;
        auto held = [&](std::string const& key) { ++weighed; auto found = disk.find(key); return found == disk.end() ? -1 : found->second; };
        std::vector<sv::Entry> entries;
        // The names in use before L-343 keep their data.
        disk["http___198.51.100.20_19133_19133"] = 0;
        check(sv::choose(entries, "http://198.51.100.20:19133", "198.51.100.20", 19133, "local_world", held)
              == "http___198.51.100.20_19133_19133", "a join keeps using the name its data is already under");
        // The same server joined by its name: at the same IP and port, with more data under the name.
        disk["play.example.net_19133"] = 2'000'000'000'000;
        disk["http___play.example.net_19133_19133"] = 500;
        check(sv::choose(entries, "play.example.net", "198.51.100.20", 19133, "local_world", held) == "play.example.net_19133"
              && entries.size() == 1, "of the names of one server the one holding the most is used, as one record");
        check((entries[0].others == std::vector<std::string>{"http___198.51.100.20_19133_19133", "http___play.example.net_19133_19133"}),
              "the names passed over are remembered");
        check(sv::choose(entries, "http://198.51.100.20:19133", "198.51.100.20", 19133, "local_world", held)
              == "play.example.net_19133", "and from then on however the server is reached");
        // What is in use stays in use: a deleted map or a grown other folder changes nothing.
        disk["play.example.net_19133"] = 0;
        disk["http___198.51.100.20_19133_19133"] = 9'000'000'000'000;
        weighed = 0;
        check(sv::choose(entries, "play.example.net", "198.51.100.20", 19133, "local_world", held) == "play.example.net_19133"
              && sv::choose(entries, "http://198.51.100.20:19133", "198.51.100.20", 19133, "local_world", held)
                     == "play.example.net_19133" && weighed == 0, "a recorded server keeps its key without looking at the folders");
        check(sv::choose(entries, "play.example.net", "203.0.113.9", 19133, "local_world", held) == "play.example.net_19133"
              && entries[0].ip == "203.0.113.9", "a name finds its server at a new IP, and the folders passed over stay passed over");
        // The old IP now belongs to someone else: neither a new name there nor a bare join brings this data.
        check(sv::choose(entries, "other.example.net", "198.51.100.20", 19133, "other", held) == "other.example.net_19133"
              && sv::choose(entries, "third.example.net", "203.0.113.9", 19133, "local_world", held) == "third.example.net_19133",
              "another name is another server, even at a known IP and with the same level id");
        std::vector<sv::Entry> byIp;
        check(sv::choose(byIp, "http://198.51.100.7:19132", "198.51.100.7", 19132, "a", held) == "198.51.100.7_19132"
              && sv::choose(byIp, "http://198.51.100.7:19132", "198.51.100.7", 19132, "b", held) == "198.51.100.7_19132"
              && byIp.size() == 1, "one address keeps one folder whatever level id it sends");
        std::vector<sv::Entry> moved;
        check(sv::choose(moved, "named.example.net", "198.51.100.8", 19132, "a", held) == "named.example.net_19132"
              && sv::choose(moved, "http://198.51.100.8:19132", "198.51.100.8", 19132, "b", held) == "198.51.100.8_19132"
              && sv::choose(moved, "http://198.51.100.8:19132", "198.51.100.8", 19132, "a", held) == "named.example.net_19132",
              "a join by IP alone finds a named server there only when the level id agrees");
        check(sv::choose(entries, "play.example.net", "198.51.100.20", 19134, "farm", held) == "play.example.net_19134",
              "another port is another server, even with the same address");
        std::vector<sv::Entry> fresh;
        check(sv::choose(fresh, "https://featured.example.com:19132", "203.0.113.50", 19132, "Featured Network", held) == "featured.example.com_19132"
              && fresh.size() == 1 && fresh[0].hosts == std::vector<std::string>{"featured.example.com"}
              && fresh[0].ip == "203.0.113.50" && fresh[0].levelId == "Featured Network",
              "a server seen for the first time is named by host and port and remembered");
        check(sv::choose(fresh, "", "", 0, "id", held).empty() && sv::choose(fresh, "host", "1.2.3.4", 0, "id", held).empty(),
              "no address gives no server name");
        // The player's choice (part 3).
        std::vector<sv::Entry> picked;
        (void)sv::choose(picked, "http://198.51.100.30:19140", "198.51.100.30", 19140, "lv", held);
        check(sv::assign(picked, "http://198.51.100.30:19140", "198.51.100.30", 19140, "lv", "old.example.net_19140")
              && picked.size() == 1 && picked[0].key == "old.example.net_19140"
              && picked[0].others == std::vector<std::string>{"198.51.100.30_19140"}
              && sv::choose(picked, "http://198.51.100.30:19140", "198.51.100.30", 19140, "lv", held) == "old.example.net_19140",
              "a chosen key is used from then on, and the one in use before is remembered");
        check(sv::assign(picked, "http://198.51.100.30:19140", "198.51.100.30", 19140, "lv", "198.51.100.30_19140")
              && picked[0].key == "198.51.100.30_19140" && picked[0].others == std::vector<std::string>{"old.example.net_19140"},
              "choosing the earlier key again goes back");
        check(sv::maybe(picked, "198.51.100.30_19140", "old.example.net_19140") == sv::Maybe::UsedBefore
              && sv::maybe(picked, "198.51.100.30_19140", "unrelated_1") == sv::Maybe::No
              && !sv::assign(picked, "", "", 0, "lv", "x_1"), "a key passed over is a candidate; no address, no choice");
        // One data for every port of an address, with one port given other data.
        std::vector<sv::Entry> wide;
        (void)sv::choose(wide, "realm.example.net", "203.0.113.70", 30001, "r", held);
        check(sv::choose(wide, "realm.example.net", "203.0.113.71", 30002, "r", held) == "realm.example.net_30002"
              && sv::maybe(wide, "realm.example.net_30002", "realm.example.net_30001") == sv::Maybe::SameLevel,
              "another port is another server until the player says otherwise, and reads as a candidate");
        (void)sv::choose(wide, "elsewhere.example.net", "203.0.113.71", 30002, "r", held);
        check(sv::setAnyPort(wide, "realm.example.net_30001", true) == 1 && wide.size() == 2
              && sv::choose(wide, "realm.example.net", "203.0.113.71", 30002, "r", held) == "realm.example.net_30001"
              && sv::choose(wide, "realm.example.net", "203.0.113.72", 30003, "r", held) == "realm.example.net_30001"
              && sv::maybe(wide, "realm.example.net_30001", "realm.example.net_30002") == sv::Maybe::UsedBefore
              && sv::choose(wide, "elsewhere.example.net", "203.0.113.71", 30002, "r", held) == "elsewhere.example.net_30002",
              "with the switch on, the address's other ports give up their own data, which stays a candidate; other addresses keep theirs");
        check(sv::setAnyPort(wide, "nowhere_1", true) < 0, "no record, no switch");
        std::erase_if(wide, [](sv::Entry const& e) { return e.key == "elsewhere.example.net_30002"; });
        check(sv::assign(wide, "realm.example.net", "203.0.113.72", 30004, "other", "elsewhere_30004")
              && sv::choose(wide, "realm.example.net", "203.0.113.72", 30004, "other", held) == "elsewhere_30004"
              && sv::choose(wide, "realm.example.net", "203.0.113.72", 30005, "r", held) == "realm.example.net_30001",
              "a port given other data keeps it; the rest still follow the switch");
        check(sv::choose(wide, "stranger.example.net", "203.0.113.72", 30006, "r", held) == "stranger.example.net_30006",
              "the switch is for the address it was set on");
        {
            // The notice of fuller data: per kind, the fullest candidate that beats the data in use.
            auto best = sv::fuller({2, 0, 500}, {{12, 3, 100}, {1, 0, 9000}, {30, 3, 400}});
            check(best.waypoints == 2 && best.placements == 0 && best.map == 1, "each kind names the candidate holding the most of it");
            auto none = sv::fuller({12, 3, 9000}, {{12, 3, 100}, {1, 0, 9000}});
            check(none.waypoints < 0 && none.placements < 0 && none.map < 0, "data that holds as much is not second-guessed");
            auto keys = sv::candidates(wide, "realm.example.net_30001");
            check(std::find(keys.begin(), keys.end(), "realm.example.net_30002") != keys.end()
                  && std::find(keys.begin(), keys.end(), "realm.example.net_30001") == keys.end(),
                  "candidates are the keys passed over and those alike, never the one in use");
        }
        check(sv::shownName(wide, "realm.example.net_30001") == "realm.example.net:30001"
              && sv::shownName({}, "http___198.51.100.20_19133_19133") == "198.51.100.20:19133"
              && sv::shownName({}, "play.example.net_19133") == "play.example.net:19133"
              && sv::shownName({}, "world_41") == "world:41" , "a key reads as host and port");
        sv::List all{wide, {{"world_41", "A friend's world"}}};
        check(lamium::map::decodeServers(lamium::map::encodeServers(all)) == all, "the server list survives saving");
        sv::forget(all, "realm.example.net_30001");
        sv::forget(all, "world_41");
        check(all.worlds.empty() && std::none_of(all.servers.begin(), all.servers.end(), [](sv::Entry const& e) {
                  return e.key == "realm.example.net_30001"
                      || std::find(e.others.begin(), e.others.end(), "realm.example.net_30001") != e.others.end(); }),
              "deleted data leaves no record pointing at it");
        auto cleaned = lamium::map::decodeServers(
            R"({"version":1,"servers":[{"key":"../x","port":1},{"key":"ok_1","port":1,"hosts":["a",5]},{"key":"p","port":0}],
                "worlds":[{"key":"a/b","name":"n"},{"key":"world_1","name":"n"}]})");
        check(cleaned.servers.size() == 1 && cleaned.servers[0].key == "ok_1" && cleaned.servers[0].hosts == std::vector<std::string>{"a"}
              && cleaned.worlds.size() == 1, "entries that could leave the folder or make no sense are dropped");
        bool rejected = false;
        try { (void)lamium::map::decodeServers(R"({"version":2})"); } catch (...) { rejected = true; }
        check(rejected, "another version of the server list is rejected");
    }
    WaypointSet set;
    set.waypoints.push_back({"家", 6, -60, 70, 40, 0, true});
    set.waypoints.push_back({"Fortress", 0, 30, 72, -140, 1, false});
    set.death = DeathPoint{118, 61, -48, 0};
    set.waypoints.push_back({"Trip", 3, 5, 64, 5, 0, true, true});
    set.lastColor = 6;
    set.lastTemporary = true;
    check(decodeWaypoints(encodeWaypoints(set)) == set,
          "waypoints, temporary ones, an older file's death point and the last color and kind round-trip");
    auto partial = decodeWaypoints(R"({"version":1,"waypoints":[{"name":"a","x":1},{"x":2},{"name":"b","color":99,"dimension":7}]})");
    check(partial.waypoints.size() == 2 && partial.waypoints[0].visible && partial.waypoints[0].x == 1
          && partial.waypoints[1].color == 11 && partial.waypoints[1].dimension == 2 && !partial.death
          && !partial.waypoints[0].temporary && !partial.lastTemporary,
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
    check(l.left == 93 && l.top == 64.5f, "the prompt is centered");
    check(l.hit(l.left + 20, l.fieldY() + 5).part == WaypointPromptLayout::Part::Field, "the name field");
    auto swatch = l.hit(l.swatchX(5) + 3, l.swatchY() + 3);
    check(swatch.part == WaypointPromptLayout::Part::Swatch && swatch.swatch == 5, "the sixth color");
    check(l.hit(l.addX() + 5, l.buttonY() + 5).part == WaypointPromptLayout::Part::Add
          && l.hit(l.cancelX() + 5, l.buttonY() + 5).part == WaypointPromptLayout::Part::Cancel, "the two buttons");
    check(l.hit(l.left + 20, l.temporaryY() + 5).part == WaypointPromptLayout::Part::Temporary
          && l.hit(l.switchX() + 5, l.temporaryY() + 5).part == WaypointPromptLayout::Part::Temporary, "the temporary switch row");
    check(l.temporaryY() > l.swatchY() + WaypointPromptLayout::swatch && l.buttonY() >= l.temporaryY() + WaypointPromptLayout::rowHeight
          && l.hintY() + 10 <= l.top + WaypointPromptLayout::height, "rows do not overlap and the hint fits");
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
    arrival();
    deathWaypoints();
    arrivalSave();
    storage();
}

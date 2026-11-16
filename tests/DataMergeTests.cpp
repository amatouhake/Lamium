#include "features/map/DataMerge.h"

void check(bool, char const*);

namespace {
using namespace lamium::map;
Waypoint point(std::string name, int x, int dimension = 0) {
    Waypoint w;
    w.name = std::move(name);
    w.x = x; w.y = 64; w.z = -x;
    w.dimension = dimension;
    return w;
}
lamium::schematic::SavedPlacement placed(std::string file, int x, int rotation = 0) {
    lamium::schematic::SavedPlacement p;
    p.name = file;
    p.file = std::move(file);
    p.placement.origin = {x, 70, 0};
    p.placement.rotation = rotation;
    return p;
}
// A chunk's first column recorded with `color`, stamped at `time`.
void record(RegionData& region, size_t chunk, std::uint32_t color, std::uint64_t time, int column = 0) {
    for (size_t i = 0; i < static_cast<size_t>(regionColumns); ++i)
        if (chunkOfIndex(i) == chunk && column-- == 0) {
            region.colors[i] = color;
            region.heights[i] = 10;
            break;
        }
    region.chunks[chunk] = {time, 3};
}
std::uint32_t colorAt(RegionData const& region, size_t chunk, int column = 0) {
    for (size_t i = 0; i < static_cast<size_t>(regionColumns); ++i)
        if (chunkOfIndex(i) == chunk && column-- == 0) return region.colors[i];
    return 0;
}
constexpr std::uint32_t red = 0xFF0000FF, blue = 0xFFFF0000;
}

void dataMergeTests() {
    using namespace lamium::map::merge;
    {
        WaypointSet into;
        into.waypoints = {point("Home", 1), point("Mine", 2)};
        auto same = point("Home", 1);
        same.color = 5; same.visible = false; same.id = 99;
        auto counts = mergeWaypoints(into, {same, point("Home", 1, 1), point("Farm", 3)});
        check(counts.taken == 2 && counts.full == 0 && into.waypoints.size() == 4,
              "merging waypoints adds new ones and counts one equal in dimension, block and name once");
        check(into.waypoints.back().name == "Farm" && into.waypoints.back().id == 0, "a merged waypoint gets a new session id");
        auto again = mergeWaypoints(into, {same, point("Home", 1, 1), point("Farm", 3)});
        check(again.taken == 0 && into.waypoints.size() == 4, "merging the same waypoints again adds nothing");
    }
    {
        WaypointSet into;
        for (size_t i = 0; i + 1 < maxWaypoints; ++i) into.waypoints.push_back(point("w", static_cast<int>(i)));
        auto counts = mergeWaypoints(into, {point("a", -1), point("b", -2), point("c", -3)});
        check(counts.taken == 1 && counts.full == 2 && into.waypoints.size() == maxWaypoints,
              "a full waypoint list takes what fits and counts the rest");
    }
    {
        lamium::schematic::PlacementSet into;
        into.placements = {placed("a.mcstructure", 0)};
        auto renamed = placed("a.mcstructure", 0);
        renamed.name = "other"; renamed.visible = false;
        auto counts = mergePlacements(into, {renamed, placed("a.mcstructure", 0, 1), placed("b.mcstructure", 0)});
        check(counts.taken == 2 && into.placements.size() == 3,
              "merging placements counts one with the same file, dimension and placing once; name does not count");
        check(mergePlacements(into, {placed("b.mcstructure", 0)}).taken == 0, "merging the same placement again adds nothing");
        while (into.placements.size() < lamium::schematic::maxPlacements) into.placements.push_back(placed("f", static_cast<int>(into.placements.size()) * 100));
        auto full = mergePlacements(into, {placed("z", -5)});
        check(full.taken == 0 && full.full == 1, "a full placement list counts what was left out");
    }
    {
        RegionData into, from;
        record(into, 0, red, 100);  // newer in the data in use
        record(from, 0, blue, 50);
        record(into, 1, red, 50);   // newer in the other data
        record(from, 1, blue, 100);
        record(into, 2, red, 70);   // a tie
        record(from, 2, blue, 70);
        record(into, 3, red, 0);    // no time on either side
        record(from, 3, blue, 0);
        record(from, 4, blue, 10);  // nothing in use there
        from.chunks[5] = {500, 1};  // stamped but empty
        into.oldFile = OldFile{1, 2, 3};
        from.oldFile = OldFile{9, 9, 9};
        int changed = mergeRegion(into, from);
        check(colorAt(into, 0) == red, "a chunk the data in use saw later keeps its record");
        check(colorAt(into, 1) == blue && into.chunks[1].time == 100, "a chunk the other data saw later replaces it, with its time");
        check(colorAt(into, 2) == red && colorAt(into, 3) == red, "a tie or no time keeps the record in use");
        check(colorAt(into, 4) == blue && into.chunks[4].time == 10, "a chunk with nothing in use takes the other record");
        check(into.chunks[5].time == 0, "a stamped chunk without recorded columns takes nothing");
        check(changed == 2, "the merge counts the chunks it changed");
        check(into.oldFile == OldFile{1, 2, 3}, "the region keeps its own legacy facts");
        check(mergeRegion(into, from) == 0, "merging the same region again changes nothing");
    }
    {
        RegionData into, from;
        record(into, 0, red, 50, 0);
        record(into, 0, red, 50, 1);
        record(from, 0, blue, 90, 1); // newer, but only one column
        mergeRegion(into, from);
        check(colorAt(into, 0, 0) == red && colorAt(into, 0, 1) == blue,
              "a newer partial chunk lays its columns over and keeps the rest");
        check(into.chunks[0].time == 90 && into.chunks[0].revision == 3,
              "a mixed chunk keeps the later time and the lower revision");
        RegionData whole, higher;
        record(whole, 2, red, 10, 0);
        record(higher, 2, blue, 20, 0);
        higher.chunks[2].revision = 9;
        mergeRegion(whole, higher);
        check(whole.chunks[2].revision == 9 && whole.chunks[2].time == 20,
              "a newer chunk replacing everything there takes its whole stamp");
        RegionData older;
        record(older, 0, blue, 10, 2);
        mergeRegion(into, older);
        check(colorAt(into, 0, 2) == blue && into.chunks[0].time == 90, "an older chunk fills only empty columns and keeps the time");
    }
}

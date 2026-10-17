#include "features/map/MapCave.h"
#include "features/map/MapColors.h"
#include "features/map/MapImage.h"
#include "features/map/MapRadar.h"
#include "features/map/MapTiles.h"
#include "features/map/MapView.h"
#include "features/map/Teleport.h"
#include <cmath>
#include <array>
#include <vector>
void check(bool, char const*);
namespace {
using namespace lamium::map;
bool near(double a, double b, double epsilon = 1e-6) { return std::abs(a - b) <= epsilon; }
// The scan loop's shape (Minimap.cpp `scan()`), pure: a call walks at most
// `cells` cells and collects the chunks a scan would stop at, resuming at
// the walk, so several calls in a row walk on like the real scan path.
std::vector<ChunkKey> dueCells(TileCache const& cache, ScanWalk& walk, ChunkKey center, int radius, double now,
                               int cells, int layer = 0, int tolerance = 1 << 20) {
    std::vector<ChunkKey> due;
    walkAt(walk, center, radius);
    for (int i = 0; i < cells; ++i) {
        auto key = walkCell(walk);
        if (scanDue(cache.find(key), walk.ring, now, layer, tolerance)) due.push_back(key);
        if (!walkStep(walk, radius)) break;
    }
    return due;
}
void geometry() {
    check(chunkOf(0, 0) == ChunkKey{0, 0} && chunkOf(15, 15) == ChunkKey{0, 0} && chunkOf(16, -1) == ChunkKey{1, -1},
          "chunk of non-negative and just-negative blocks");
    check(chunkOf(-16, -17) == ChunkKey{-1, -2} && chunkOf(-1, -1) == ChunkKey{-1, -1}, "negative chunks round down");
    check(columnIndex(-1, -1) == 255 && columnIndex(0, 0) == 0 && columnIndex(-16, 1) == 16 && columnIndex(17, -17) == 15 * 16 + 1,
          "column index wraps negative coordinates into the chunk");
    check(packKey({-1, 0}) != packKey({0, -1}) && packKey({1, 2}) == packKey({1, 2}), "chunk keys are distinct");
    check(blockFloor(-.5) == -1 && blockFloor(2.9) == 2 && blockFloor(NAN) == 0, "block floor of positions");
    check(blocksAcross(defaultZoomIndex) == 128, "default zoom shows about 128 blocks");
    check(blocksAcross(stepZoom(defaultZoomIndex, 1)) == 96 && blocksAcross(stepZoom(defaultZoomIndex, -1)) == 192,
          "zooming in shows fewer blocks");
    check(zoomIndexFor(128) == defaultZoomIndex && zoomIndexFor(1000) == 10 && zoomIndexFor(0) == 0 && zoomIndexFor(100) == 5,
          "saved widths map to the nearest step");
    auto snapped = snapCenter(ViewTransform::northUp(), 10.7, -3.2, .5);
    check(near(snapped.x, 10.5) && near(snapped.z, -3.5), "north up, the center snaps to whole pixels");
    auto turnedView = ViewTransform::headingUp(30);
    auto t = snapCenter(turnedView, 10.7, -3.2, .5);
    auto onMap = turnedView.toMap(t.x, t.z);
    check(near(std::fmod(std::abs(onMap.x), .5), 0, 1e-9) || near(std::fmod(std::abs(onMap.x), .5), .5, 1e-9),
          "turned, it snaps along the map's own axes");
    check(blocksAcross(stepZoom(0, 1)) == 16 && blocksAcross(stepZoom(10, -1)) == 512 && blocksAcross(99) == 512,
          "zoom stops at its ends");
    auto south = heading(0), north = heading(180), west = heading(90), east = heading(-90);
    check(near(south.x, 0) && near(south.z, 1) && near(north.x, 0, 1e-9) && near(north.z, -1), "yaw 0 is south, 180 north");
    check(near(west.x, -1) && near(east.x, 1), "yaw 90 is west, -90 east");
    auto up = ViewTransform::headingUp(180);
    auto plain = ViewTransform::northUp();
    check(near(up.rightX, plain.rightX) && near(up.rightZ, plain.rightZ, 1e-9) && near(up.downX, plain.downX, 1e-9)
          && near(up.downZ, plain.downZ), "heading-up facing north is north-up");
    auto facingEast = ViewTransform::headingUp(-90);
    auto ahead = facingEast.toWorld(0, -1), right = facingEast.toWorld(1, 0);
    check(near(ahead.x, 1) && near(ahead.z, 0, 1e-9) && near(right.x, 0, 1e-9) && near(right.z, 1),
          "facing east, up is east and right is south");
    auto turned = ViewTransform::headingUp(37);
    auto back = turned.toMap(turned.toWorld(3, -5).x, turned.toWorld(3, -5).z);
    check(near(back.x, 3) && near(back.z, -5), "map and world transforms invert each other");
    auto p = worldToPixel(plain, 100, 100, 110, 90, 128, 256);
    check(near(p.x, 148) && near(p.y, 108) && p.inside, "a point east and north of the center");
    check(!worldToPixel(plain, 0, 0, 70, 0, 128, 256).inside && !worldToPixel(plain, 0, 0, 60, 0, 128, 256, 12).inside
          && worldToPixel(plain, 0, 0, 60, 0, 128, 256).inside, "edge test with a margin");
    check(chunkRadius(128, false) == 5 && chunkRadius(128, true) == 7 && chunkRadius(32, false) == 2,
          "scan radius covers the map and its turning corners");
}
// Recording follows the view distance (L-206): the floor is the old fixed
// range, the ceiling the game's own view-distance maximum.
void recording() {
    check(recordingBlocks(12, 96) == 384 && recordingBlocks(6, 96) == 384 && recordingBlocks(0, 96) == 384,
          "recording keeps the old fixed range as its floor");
    check(recordingBlocks(32, 96) == 1024 && recordingBlocks(48, 96) == 1536, "recording follows the view distance");
    check(recordingBlocks(120, 96) == 3072 && recordingBlocks(32, 24) == 768, "recording stops at the game's maximum");
    check(recordingBlocks(-4, 0) == 384 && recordingBlocks(200, -1) == 3072, "broken option values fall back");
}
// The scan walk (L-206 review): rings from the middle out, a bounded number
// of cells looked at per call, and progress that lives in the walk and not
// in the tiles, so the display cache's eviction cannot rewind it.
void scanning() {
    // A pass walks every cell of every ring once, nearer rings first.
    ScanWalk walk;
    walkAt(walk, {5, -3}, 2);
    std::vector<ChunkKey> cells;
    for (;;) {
        cells.push_back(walkCell(walk));
        if (!walkStep(walk, 2)) break;
    }
    auto ring = [](ChunkKey k) { return std::max(std::abs(k.x - 5), std::abs(k.z + 3)); };
    bool inOrder = true, onceEach = true;
    for (size_t i = 0; i < cells.size(); ++i) {
        if (i && ring(cells[i]) < ring(cells[i - 1])) inOrder = false;
        if (ring(cells[i]) > 2) onceEach = false;
        for (size_t j = 0; j < i; ++j)
            if (cells[j] == cells[i]) onceEach = false;
    }
    check(cells.size() == 25 && cells[0] == ChunkKey{5, -3}, "the walk starts at the center chunk");
    check(inOrder, "nearer rings come first");
    check(onceEach && walk.ring == 0 && walk.step == 0, "a pass covers each cell once and returns to the center");

    // A scanned area: nothing due, and a call still looks at a bounded
    // number of cells before the next one walks on.
    TileCache scanned;
    for (int z = -1; z <= 1; ++z)
        for (int x = -1; x <= 1; ++x) {
            auto& tile = scanned.put({x, z});
            tile.loaded = true;
            tile.scannedAt = 10;
        }
    ScanWalk quiet;
    check(dueCells(scanned, quiet, {0, 0}, 1, 10.5, 100).empty(), "a scanned area has nothing due");
    ScanWalk bounded;
    dueCells(scanned, bounded, {0, 0}, 1, 10.5, 4);
    check(bounded.ring == 1 && bounded.step == 3, "a call looks at at most the given number of cells, then resumes");

    // Progress does not depend on the display cache (L-206 review): every
    // call evicts the rings beyond `keep`, exactly like scan() does, and the
    // walk still reaches the far ring, looking at each cell of the pass once.
    TileCache far;
    ScanWalk record;
    int const radius = 12, keep = 3, perCall = 64;
    int examined = 0, laps = 0;
    bool reachedFar = false;
    for (int calls = 0; calls < 32 && laps == 0; ++calls) {
        walkAt(record, {0, 0}, radius);
        for (int i = 0; i < perCall; ++i) {
            auto key = walkCell(record);
            ++examined;
            if (scanDue(far.find(key), record.ring, 0)) {
                auto& tile = far.put(key);
                tile.loaded = true;
                tile.scannedAt = 0;
                if (record.ring == radius) reachedFar = true;
            }
            if (!walkStep(record, radius)) {
                ++laps;
                break;
            }
        }
        far.evict({0, 0}, keep); // the display cache drops the far rings
    }
    check(laps == 1 && reachedFar, "the walk reaches the far ring despite the display eviction");
    check(examined == 1 + 4 * radius * (radius + 1), "eviction does not make a pass look at a cell twice");

    // The walk follows the player: ordinary walking keeps its place; a jump
    // or a radius that no longer holds it starts over at the center.
    ScanWalk followed;
    walkAt(followed, {0, 0}, 5);
    for (int i = 0; i < 3; ++i) walkStep(followed, 5);
    auto keptRing = followed.ring, keptStep = followed.step;
    walkAt(followed, {-4, 2}, 5);
    check(followed.ring == keptRing && followed.step == keptStep && followed.center == ChunkKey{-4, 2},
          "walking keeps the walk's place");
    walkAt(followed, {60, 2}, 5);
    check(followed.ring == 0 && followed.step == 0, "a jump farther than the restart range starts the walk over");
    ScanWalk shrunk;
    walkAt(shrunk, {0, 0}, 5);
    for (int i = 0; i < 20; ++i) walkStep(shrunk, 5);
    walkAt(shrunk, {0, 0}, 1);
    check(shrunk.ring == 0 && shrunk.step == 0, "a radius that no longer holds the walk starts it over");
}
void scheduledScanning() {
    TileCache cache;
    for (int z = -4; z <= 4; ++z)
        for (int x = -4; x <= 4; ++x) {
            auto& tile = cache.put({x, z});
            tile.loaded = true;
            tile.layer = 64;
        }
    ScanSchedule schedule;
    schedule.sweep.ring = 97;
    schedule.nearNext = false;
    std::vector<ChunkKey> scanned;
    auto scan = [&](ChunkKey key) {
        scanned.push_back(key);
        auto& tile = cache.put(key);
        tile.loaded = true;
        tile.layer = 64;
        tile.scannedAt = 2;
        return true;
    };
    auto oneChunk = [] { return true; }; // The first scan uses the whole time budget.
    for (int call = 0; call < 2; ++call)
        scanBatch(cache, schedule, {0, 0}, 97, 2, 64, 0, scan, oneChunk);
    check(scanned.size() == 2 && scanned[1] == ChunkKey{0, 0},
          "an overdue center updates on the next near turn while the sweep is at ring 97");
    check(schedule.sweep.ring == 97 && schedule.sweep.step == 1,
          "near updates preserve the wide sweep's progress with a one-chunk budget");

    schedule.nearNext = true;
    scanBatch(cache, schedule, {1, 0}, 97, 2, 64, 0, scan, oneChunk);
    check(scanned.back() == ChunkKey{1, 0} && schedule.sweep.ring == 97,
          "crossing a chunk boundary updates the new center without waiting for the outer rings");
    schedule.nearNext = true;
    scanBatch(cache, schedule, {1, 0}, 97, 2, 80, 0, scan, oneChunk);
    check(scanned.size() == 4 && scanned.back() == ChunkKey{1, 0},
          "a changed cave layer updates near the player even when the old tile is fresh");

    TileCache missing;
    ScanSchedule busy;
    busy.sweep.ring = 97;
    int nearAttempts = 0, farAttempts = 0;
    for (int call = 0; call < 12; ++call)
        scanBatch(missing, busy, {0, 0}, 97, 0, 0, 0, [&](ChunkKey key) {
            if (key == ChunkKey{0, 0}) ++nearAttempts;
            else ++farAttempts;
            return true; // Near tiles stay due: the sweep must still get turns.
        }, oneChunk);
    check(nearAttempts == 6 && farAttempts == 6 && busy.sweep.step == 6,
          "a continuously due near area cannot starve the outer sweep");

    ScanSchedule waiting;
    waiting.sweep.ring = 97;
    int attempts = 0;
    auto resources = [&](ChunkKey key) {
        ++attempts;
        return key != ChunkKey{0, 0};
    };
    int completed = scanBatch(missing, waiting, {0, 0}, 97, 0, 0, 0, resources, oneChunk);
    completed += scanBatch(missing, waiting, {0, 0}, 97, 0, 0, 0, resources, oneChunk);
    completed += scanBatch(missing, waiting, {0, 0}, 97, 0, 0, 0, resources, oneChunk);
    check(completed == 1 && attempts == 3 && waiting.sweep.step == 1,
          "a near texture wait retries without blocking the wide sweep's next turn");

    TileCache fresh;
    for (int z = -12; z <= 12; ++z)
        for (int x = -12; x <= 12; ++x) {
            auto& tile = fresh.put({x, z});
            tile.loaded = true;
            tile.scannedAt = 10;
        }
    ScanSchedule bounded;
    int unexpected = 0;
    scanBatch(fresh, bounded, {0, 0}, 12, 10, 0, 0, [&](ChunkKey) { ++unexpected; return true; }, [] { return false; });
    ScanWalk expected;
    for (int i = 0; i < scanCellsPerCall - 81; ++i) walkStep(expected, 12);
    check(unexpected == 0 && bounded.sweep.ring == expected.ring && bounded.sweep.step == expected.step,
          "near and wide searches share one cell budget even when no chunks need scanning");

    TileCache evicted;
    ScanSchedule outer;
    bool reachedFar = false, leftCenter = false, finished = false;
    for (int call = 0; call < 2000 && !finished; ++call) {
        scanBatch(evicted, outer, {0, 0}, 12, 0, 0, 0, [&](ChunkKey key) {
            auto& tile = evicted.put(key);
            tile.loaded = true;
            if (std::max(std::abs(key.x), std::abs(key.z)) == 12) reachedFar = true;
            return true;
        }, oneChunk);
        evicted.evict({0, 0}, 4);
        leftCenter = leftCenter || outer.sweep.ring > 0;
        finished = leftCenter && outer.sweep.ring == 0;
    }
    check(reachedFar && finished, "scheduled near updates still let the wide sweep finish despite display eviction");
}
void tiles() {
    TileCache cache;
    ScanWalk firstWalk;
    auto first = dueCells(cache, firstWalk, {3, -4}, 2, 0, 100);
    check(first.size() == 25 && first[0] == ChunkKey{3, -4}, "the center chunk is scanned first");
    bool ringOrder = true;
    for (size_t i = 1; i < first.size(); ++i) {
        auto ring = [&](ChunkKey k) { return std::max(std::abs(k.x - 3), std::abs(k.z + 4)); };
        if (ring(first[i]) < ring(first[i - 1])) ringOrder = false;
    }
    check(ringOrder, "nearer rings come first");
    ScanWalk cappedWalk;
    auto capped = dueCells(cache, cappedWalk, {0, 0}, 3, 0, 5);
    check(capped.size() == 5, "a call looks at a bounded number of cells");
    auto resumed = dueCells(cache, cappedWalk, {0, 0}, 3, 0, 5);
    bool overlap = false;
    for (auto a : capped)
        for (auto b : resumed) if (a == b) overlap = true;
    check(resumed.size() == 5 && !overlap, "the next call resumes where the last stopped");
    auto& tile = cache.put({0, 0});
    tile.loaded = true;
    tile.scannedAt = 10;
    cache.put({1, 0}) = Tile{{}, true, 10};
    ScanWalk freshWalk;
    auto fresh = dueCells(cache, freshWalk, {0, 0}, 1, 10.5, 100);
    check(fresh.size() == 7, "fresh chunks are not scanned again");
    ScanWalk staleWalk;
    auto later = dueCells(cache, staleWalk, {0, 0}, 1, 11.5, 100);
    check(later.size() == 9 && later[0] == ChunkKey{0, 0}, "stale chunks are picked up again, nearest first");
    check(std::find(later.begin(), later.end(), ChunkKey{1, 0}) != later.end(),
          "a stale seen chunk comes back with the unseen ones");
    cache.put({1, 1}) = Tile{{}, false, 10};
    check(rescanAfter(5, false) < rescanAfter(0, true) && rescanAfter(9, true) > rescanAfter(2, true),
          "missing chunks retry soon, far chunks rescan rarely");
    tile.columns[static_cast<size_t>(columnIndex(5, 6))] = {packColor(10, 20, 30), 64};
    check(cache.column(5, 6) && cache.column(5, 6)->height == 64 && !cache.column(5, 7) && !cache.column(17, 17),
          "only known columns of loaded chunks are read");
    cache.put({40, 0});
    cache.put({-9, -9});
    cache.evict({0, 0}, 8);
    check(cache.find({40, 0}) == nullptr && cache.find({-9, -9}) == nullptr && cache.find({1, 0}) != nullptr,
          "far chunks are forgotten");
}
void image() {
    check(near(shadeFactor(64, 64, 64), 1) && shadeFactor(65, 64, 64) > 1 && shadeFactor(63, 64, 64) < 1,
          "higher than the neighbors is lighter");
    check(near(shadeFactor(100, 0, 0), 1.3f) && near(shadeFactor(0, 100, 100), .6f), "shading is bounded");
    check(over(0, 10, 20, 30, 1) == packColor(10, 20, 30), "opaque over transparent is the color");
    check(over(packColor(0, 0, 0), 255, 255, 255, .5f) == packColor(128, 128, 128), "half white over black");
    check(over(packColor(1, 2, 3), 9, 9, 9, 0) == packColor(1, 2, 3), "zero alpha leaves the pixel");

    TileCache cache;
    auto& tile = cache.put({-1, -1});
    tile.loaded = true;
    for (int i = 0; i < 256; ++i) tile.columns[static_cast<size_t>(i)] = {packColor(100, 100, 100), 60};
    // One column higher than its north and west neighbors.
    tile.columns[static_cast<size_t>(columnIndex(-8, -8))] = {packColor(100, 100, 100), 62};
    Frame frame;
    frame.pixels = 16;
    frame.centerX = -8;
    frame.centerZ = -8;
    frame.blocks = 16;
    std::vector<std::uint32_t> pixels;
    composeTerrain(cache, frame, pixels);
    check(pixels.size() == 256 && pixels[0] == packColor(100, 100, 100), "one block per pixel, flat ground unshaded");
    check(channel(pixels[8 * 16 + 8], 0) > 100 && channel(pixels[9 * 16 + 8], 0) < 100,
          "a raised column is lighter and the one south of it darker");
    // A chunk scanned later to the north reshades the border row south of it.
    auto& north = cache.put({-1, -2});
    north.loaded = true;
    for (int i = 0; i < 256; ++i) north.columns[static_cast<size_t>(i)] = {packColor(100, 100, 100), 70};
    composeTerrain(cache, frame, pixels);
    check(pixels[0] == packColor(100, 100, 100), "shading is kept until a neighbor changes");
    cache.changed({-1, -2});
    composeTerrain(cache, frame, pixels);
    check(channel(pixels[0], 0) < 100 && channel(pixels[16], 0) == 100, "a changed north neighbor darkens the border row");
    frame.centerX = 0;
    composeTerrain(cache, frame, pixels);
    check(pixels[0] != 0 && pixels[8 * 16 + 8] == 0, "columns of chunks never scanned stay transparent");
    frame.centerX = -8;
    frame.round = true;
    composeTerrain(cache, frame, pixels);
    check(pixels[0] == 0 && pixels[8 * 16 + 8] != 0, "a round map leaves the corners transparent");

    std::vector<std::uint32_t> arrow(64 * 64, 0);
    drawArrow(arrow, 64, 32, 32, 0, 16);
    auto at = [&](int x, int y) { return arrow[static_cast<size_t>(y) * 64 + x]; };
    check(at(32, 32) == packColor(255, 255, 255) && at(32, 26) != 0, "the arrow is white with its tip up");
    check(at(32, 38) == 0 && at(2, 2) == 0, "the tail notch and far pixels stay clear");
    check(channel(at(32, 22), 3) > 0 && channel(at(32, 22), 0) < 128, "the outline is black");
    std::vector<std::uint32_t> right(64 * 64, 0);
    drawArrow(right, 64, 32, 32, northUpArrowAngle(-90), 16);
    check(right[32 * 64 + 38] != 0 && right[32 * 64 + 26] == 0, "facing east points right");
    check(near(northUpArrowAngle(180), 0, 1e-9) && near(std::abs(northUpArrowAngle(0)), 3.14159265358979, 1e-9),
          "north is up, south down");

    std::vector<std::uint32_t> outlined(32 * 32, packColor(50, 50, 50));
    auto color = packColor(100, 210, 225);
    drawOutline(outlined, 32, {{{8.5, 8.5}, {23.5, 8.5}, {23.5, 23.5}, {8.5, 23.5}}}, color, 1, false);
    auto px = [&](int x, int y) { return outlined[static_cast<size_t>(y) * 32 + x]; };
    check(channel(px(16, 8), 2) > 150 && channel(px(16, 7), 2) < 50, "the outline is colored with a dark edge");
    check(px(16, 16) != packColor(50, 50, 50) && channel(px(16, 16), 2) < 150, "the inside gets a faint fill");
    check(px(2, 2) == packColor(50, 50, 50), "pixels away from the footprint stay untouched");
    std::vector<std::uint32_t> dotted(32 * 32, 0);
    drawOutline(dotted, 32, {{{16, 16}, {16.2, 16}, {16.2, 16.2}, {16, 16.2}}}, color, 1, false);
    check(channel(dotted[16 * 32 + 17], 3) > 0 && channel(dotted[16 * 32 + 14], 3) > 0, "a tiny footprint still shows");
    std::vector<std::uint32_t> clipped(32 * 32, 0);
    drawOutline(clipped, 32, {{{-10, -10}, {40, -10}, {40, 40}, {-10, 40}}}, color, 1, true);
    check(clipped[0] == 0 && clipped[16 * 32 + 16] != 0, "a round map clips the footprint to its circle");
    drawOutline(clipped, 32, {{{900, 900}, {910, 900}, {910, 910}, {900, 910}}}, color, 1, false);

    std::vector<std::uint32_t> framed(16 * 16, packColor(50, 50, 50));
    drawFrame(framed, 16, false, 1);
    check(channel(framed[0], 0) > 50 && channel(framed[17], 0) < 50 && framed[2 * 16 + 2] == packColor(50, 50, 50),
          "a light outer line, a dark inner line, the inside untouched");
    auto points = compassPoints(ViewTransform::northUp(), 100, 10, false);
    check(near(points[0].x, 50) && near(points[0].y, 10) && near(points[1].x, 90) && near(points[3].x, 10),
          "compass letters sit inside the edges");
    auto turnedPoints = compassPoints(ViewTransform::headingUp(-90), 100, 10, true);
    check(near(turnedPoints[1].y, 10) && near(turnedPoints[1].x, 50), "facing east, E is at the top");
}
void cave() {
    check(chooseView(ViewMode::Surface, true, false, 15) == ViewMode::Cave, "the Nether is always a cave");
    check(chooseView(ViewMode::Cave, false, false, 0) == ViewMode::Surface, "open sky is the surface");
    check(chooseView(ViewMode::Surface, false, true, 13) == ViewMode::Surface, "tree shade stays on the surface");
    check(chooseView(ViewMode::Surface, false, true, 0) == ViewMode::Cave, "a dark covered spot is a cave");
    check(chooseView(ViewMode::Surface, false, true, 8) == ViewMode::Surface
          && chooseView(ViewMode::Cave, false, true, 8) == ViewMode::Cave, "the view holds between the thresholds");
    check(applyForce(ViewForce::Surface, ViewMode::Cave) == ViewMode::Surface
          && applyForce(ViewForce::Auto, ViewMode::Cave) == ViewMode::Cave, "a forced view wins");
    check(pressForce(ViewForce::Auto, ViewMode::Cave) == ViewForce::Surface
          && pressForce(ViewForce::Auto, ViewMode::Surface) == ViewForce::Cave
          && pressForce(ViewForce::Cave, ViewMode::Cave) == ViewForce::Auto, "the key flips the view, then returns to auto");
    ViewSwitch view;
    check(view.update(ViewMode::Cave, 10) == ViewMode::Cave, "the first switch is immediate");
    check(view.update(ViewMode::Surface, 10.5) == ViewMode::Cave && view.update(ViewMode::Surface, 11) == ViewMode::Surface,
          "the next automatic switch waits a second");
    check(!coveredByMost(4) && coveredByMost(5), "most of the nine columns must have a roof");
    check(stableLayer(0, 64, true) == 64 && stableLayer(64, 67, false) == 64 && stableLayer(64, 61, false) == 64
          && stableLayer(64, 68, false) == 68 && stableLayer(64, 60, false) == 60,
          "the cave layer holds within the slack and then moves to the player");
    auto stone = packColor(100, 100, 100);
    // top = 66 for a player at 64: cells for y 66, 65, 64, 63, ...
    std::array<bool, 5> tunnel{true, false, false, true, true};
    auto floor = caveColumn(caveFloor(tunnel, 66, 64), 64, stone);
    check(floor.height == 63 && floor.color == stone, "the floor under the player's feet is at full brightness");
    std::array<bool, 5> rock{true, true, true, false, true};
    check(caveFloor(rock, 66, 64).kind == CaveHit::Kind::Wall && caveColumn(caveFloor(rock, 66, 64), 64, stone).color == caveWall,
          "rock at feet and head is a wall");
    std::array<bool, 27> pit{};
    pit[26] = true;
    auto deep = caveColumn(caveFloor(pit, 66, 64), 64, stone);
    check(deep.height == 40 && channel(deep.color, 0) < 100, "a lower floor is darker");
    check(caveFloor(std::array<bool, 5>{}, 66, 64).kind == CaveHit::Kind::Drop
          && caveColumn(caveFloor(std::array<bool, 5>{}, 66, 64), 64, stone).color == caveDeep,
          "open all the way is a drop");
    TileCache cache;
    auto& tile = cache.put({0, 0});
    tile.loaded = true;
    tile.layer = 64;
    ScanWalk atLayer, otherLayer, waiting, retried;
    check(dueCells(cache, atLayer, {0, 0}, 0, 0, 10, 64, 0).empty()
          && dueCells(cache, otherLayer, {0, 0}, 0, 0, 10, 65, 0).size() == 1,
          "a cave chunk scanned at another layer is scanned again");
    tile.scannedAt = 0;
    check(dueCells(cache, waiting, {0, 0}, 0, .6, 10, 64, 0).empty(), "a complete chunk waits");
    tile.partial = true;
    check(dueCells(cache, retried, {0, 0}, 0, .6, 10, 64, 0).size() == 1, "a chunk with missing blocks is retried soon");
    Frame frame;
    frame.pixels = 4;
    frame.blocks = 4;
    frame.centerX = 100;
    frame.unknown = packColor(16, 17, 19, 150);
    std::vector<std::uint32_t> pixels;
    composeTerrain(cache, frame, pixels);
    check(pixels[0] == frame.unknown, "unknown ground gets the fill");
}
void radar() {
    check(classify(true, false, false, true) == DotKind::Player && classify(false, true, false, false) == DotKind::Item
          && classify(false, false, true, true) == DotKind::Hostile && classify(false, false, false, true) == DotKind::Passive
          && !classify(false, false, false, false), "actors sort into players, items, hostile and passive mobs");
    RadarSwitches defaults;
    check(shown(DotKind::Player, defaults) && shown(DotKind::Hostile, defaults) && shown(DotKind::Passive, defaults)
          && !shown(DotKind::Item, defaults), "items are off by default");
    check(dotScale(64) == 1 && dotScale(128) == 1 && near(dotScale(512), .5) && dotScale(256) < 1 && dotScale(256) > .5,
          "dots shrink on wide maps, to half at 512 blocks");
    check(dotAlpha(7.9) == 1 && dotAlpha(-8) < 1 && dotAlpha(12) < 1, "dots 8 or more blocks above or below are fainter");
    std::vector<Dot> dots{{DotKind::Player, 10, 0, 0, "Alex"}, {DotKind::Hostile, 5, 0, 0, ""},
                          {DotKind::Passive, 200, 0, 0, ""}, {DotKind::Item, -5, 0, 0, ""}};
    auto placed = placeDots(dots, defaults, ViewTransform::northUp(), 0, 0, 128, 256, false, 4);
    check(placed.size() == 2 && placed[0].kind == DotKind::Hostile && placed[1].kind == DotKind::Player,
          "dots off the map or switched off are left out; players are drawn last");
    check(placed[1].px == 148 && placed[1].py == 128 && placed[1].name == "Alex", "a player 10 blocks east, with a name");
    auto limited = placeDots(dots, RadarSwitches{true, true, true, true}, ViewTransform::northUp(), 0, 0, 128, 256, false, 4, 1);
    check(limited.size() == 1 && limited[0].kind == DotKind::Hostile, "the nearest dots are kept at the limit");
    auto roundMap = placeDots({{DotKind::Hostile, 60, 60, 0, ""}}, defaults, ViewTransform::northUp(), 0, 0, 128, 256, true, 4);
    check(roundMap.empty(), "a round map leaves out its corners");
    auto distant = placeDots({{DotKind::Player, 10, 0, 0, "Far", -1, true}, {DotKind::Player, 0, 10, 20, "Low", -1, true}},
                             defaults, ViewTransform::northUp(), 0, 0, 128, 256, false, 4);
    check(distant.size() == 2 && distant[0].distant && distant[0].alpha == distantAlpha && distant[1].alpha == distantAlpha,
          "a distant player is drawn at the distant opacity, whatever its height");
    auto high = placeDots({{DotKind::Player, 10, 0, 20, "High"}, {DotKind::Hostile, 0, 10, 20, ""}}, defaults,
                          ViewTransform::northUp(), 0, 0, 128, 256, false, 4);
    check(high.size() == 2 && high[1].alpha == 1 && high[0].alpha < 1, "a loaded player far above stays opaque; a mob fades");
    std::vector<std::uint32_t> image(32 * 32, 0);
    drawDot(image, 32, 16, 16, 3, 2, dotColor(DotKind::Hostile), 1);
    check(image[16 * 32 + 16] == dotColor(DotKind::Hostile) && channel(image[16 * 32 + 20], 0) < 60
          && channel(image[16 * 32 + 20], 3) > 200 && image[2 * 32 + 2] == 0, "a colored dot inside a black ring");
}
void colors() {
    std::uint8_t gray[] = {200, 200, 200, 255, 100, 100, 100, 255};
    check(averageColor(gray, 2) == packColor(150, 150, 150), "plain average of opaque texels");
    std::uint8_t cutout[] = {0, 0, 0, 0, 0, 0, 0, 0, 60, 120, 30, 255};
    check(averageColor(cutout, 3) == packColor(60, 120, 30), "transparent texels do not count");
    std::uint8_t empty[] = {9, 9, 9, 0};
    check(!averageColor(empty, 1) && !averageColor(nullptr, 4), "an invisible texture has no color");
    check(tinted(packColor(200, 200, 200), .5f, 1, .25f) == packColor(100, 200, 50), "tints multiply");
    check(tinted(packColor(10, 20, 30), 2, NAN, -1) == packColor(10, 20, 0), "tints never brighten or break");
    check(usableTint(.596f, .765f, .357f) && !usableTint(0, 0, 0) && !usableTint(NAN, 1, 1),
          "a black or broken biome tint is not used");
}
void surface() {
    // The heightmap (100) holds the ground under a carpet (101): the scan
    // starts one above, and only a covering block wins there.
    check(columnBlockWins(false, true, 101, 100), "a covering block above the heightmap wins over the ground");
    check(columnBlockWins(false, true, 100, 100) && columnBlockWins(false, false, 99, 100),
          "from the heightmap down, covering and other visible blocks count");
    check(!columnBlockWins(false, false, 100, 100), "a non-covering block at the heightmap is looked through");
    check(!columnBlockWins(false, false, 101, 100) && !columnBlockWins(false, false, 102, 100),
          "plants and other non-covering blocks above the heightmap are looked through");
    check(!columnBlockWins(true, true, 101, 100) && !columnBlockWins(true, false, 99, 100),
          "skipped blocks never win");
    check(coversColumn(false, 1.f, 1.f) && !coversColumn(true, 1.f, 1.f) && !coversColumn(false, .5f, 1.f),
          "a covering shape spans the column and plants never cover");
}
void teleport() {
    check(canTeleport(true, false, 0, 0, 0),
          "a server listing /tp offers teleport even where the world's commands flag is off");
    check(!canTeleport(false, true, 4, 0, 0), "a server not listing /tp hides teleport");
    check(canTeleport(std::nullopt, true, 1, 0, 0) && !canTeleport(std::nullopt, false, 4, 0, 0)
          && !canTeleport(std::nullopt, true, 0, 0, 0),
          "before the list arrives, commands and permission decide");
    check(!canTeleport(true, true, 4, 0, 1), "another dimension's map never teleports across dimensions");
    check(teleportCommand(10, 64, -6) == "/tp @s 10.5 64 -5.5" && teleportCommand(-1, -59, 0) == "/tp @s -0.5 -59 0.5",
          "teleport targets the block center, also for negative coordinates");
}
}
void mapTests() {
    teleport();
    colors();
    surface();
    cave();
    radar();
    geometry();
    recording();
    scanning();
    scheduledScanning();
    tiles();
    image();
}

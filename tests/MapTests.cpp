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
// Every chunk one call of a scan path would pick (no batch limit).
std::vector<ChunkKey> dueNow(ScanGrid& grid, ChunkKey center, int radius, double now, bool round = false, int layer = 0,
                             int tolerance = 1 << 20, int staleRadius = 1 << 20) {
    grid.follow(center, radius, round);
    return grid.due(now, layer, tolerance, staleRadius, 100000);
}
void seeAll(ScanGrid& grid, std::vector<ChunkKey> const& keys, Seen seen, double now, int layer = 0) {
    for (auto key : keys) grid.saw(key, seen, layer, now);
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
// The scan grid (L-210): every call picks the due chunks nearest first from
// what the path has seen, so moving never leaves a chunk behind.
void scanning() {
    ScanGrid grid;
    auto first = dueNow(grid, {5, -3}, 2, 0);
    auto dist = [](ChunkKey k, ChunkKey c) { return (k.x - c.x) * (k.x - c.x) + (k.z - c.z) * (k.z - c.z); };
    bool nearestFirst = true, onceEach = true, inRange = true;
    for (size_t i = 0; i < first.size(); ++i) {
        if (i && dist(first[i], {5, -3}) < dist(first[i - 1], {5, -3})) nearestFirst = false;
        if (std::max(std::abs(first[i].x - 5), std::abs(first[i].z + 3)) > 2) inRange = false;
        for (size_t j = 0; j < i; ++j)
            if (first[j] == first[i]) onceEach = false;
    }
    check(first.size() == 25 && first[0] == ChunkKey{5, -3}, "the center chunk comes first");
    check(nearestFirst && onceEach && inRange, "every chunk in range once, nearest first");
    ScanGrid disc;
    auto round = dueNow(disc, {0, 0}, 3, 0, true);
    check(round.size() == 37 && std::find(round.begin(), round.end(), ChunkKey{3, 3}) == round.end()
              && std::find(round.begin(), round.end(), ChunkKey{3, 0}) != round.end(),
          "a round range leaves the square's corners out");

    // Rescans: complete chunks near the player soon, farther ones later,
    // incomplete ones sooner, and complete far ones only within staleRadius.
    seeAll(grid, first, Seen::Complete, 10);
    check(dueNow(grid, {5, -3}, 2, 10.5).empty(), "a scanned area has nothing due");
    check(dueNow(grid, {5, -3}, 2, 11.5).size() == 9, "the center and the first ring rescan after a second");
    check(dueNow(grid, {5, -3}, 2, 14.5).size() == 25, "the second ring after four");
    seeAll(grid, first, Seen::Complete, 20);
    grid.saw({5, -1}, Seen::Partial, 0, 20);
    grid.saw({7, -3}, Seen::Missing, 0, 20);
    check(dueNow(grid, {5, -3}, 2, 20.6).size() == 2, "incomplete and missing chunks retry within a second");
    check(rescanAfter(6, Seen::Missing) > rescanAfter(2, Seen::Missing)
              && rescanAfter(9, Seen::Complete) > rescanAfter(2, Seen::Complete),
          "far missing chunks retry less often, far complete ones rescan rarely");
    // Near rescans, never-seen chunks and far rescans take turns, the first
    // kind rotating per call; complete chunks beyond the stale radius wait
    // for the next visit.
    ScanGrid wide;
    seeAll(wide, dueNow(wide, {0, 0}, 8, 0), Seen::Complete, 0);
    wide.saw({8, 8}, Seen::Never, 0, 0);
    bool withinStale = true, turns = true;
    for (int call = 0; call < 3; ++call) {
        auto list = wide.due(100, 0, 1 << 20, 6, 1000);
        for (auto key : list)
            if (std::max(std::abs(key.x), std::abs(key.z)) > 6 && !(key == ChunkKey{8, 8})) withinStale = false;
        auto ringOf = [](ChunkKey k) { return std::max(std::abs(k.x), std::abs(k.z)); };
        // The first three picks are one of each kind.
        int nearKinds = 0, freshKinds = 0, farKinds = 0;
        for (size_t i = 0; i < 3 && i < list.size(); ++i) {
            if (list[i] == ChunkKey{8, 8}) ++freshKinds;
            else if (ringOf(list[i]) <= 4) ++nearKinds;
            else ++farKinds;
        }
        if (nearKinds != 1 || freshKinds != 1 || farKinds != 1) turns = false;
    }
    check(withinStale, "complete chunks beyond the stale radius are not rescanned");
    check(turns, "near rescans, never-seen chunks and far rescans take turns");

    // A one-chunk budget (in game: picking chunks among 30,000 cells) still
    // gives every kind its share, even with near chunks always due.
    ScanGrid shares;
    for (auto key : dueNow(shares, {0, 0}, 40, 0, true)) {
        int ring = std::max(std::abs(key.x), std::abs(key.z));
        shares.saw(key, ring > 30 ? Seen::Partial : Seen::Complete, 0, -10);
        if (ring >= 20 && ring <= 25) shares.saw(key, Seen::Never, 0, 0);
    }
    int nearScans = 0, freshScans = 0, farScans = 0;
    double at = 0;
    for (int call = 0; call < 600; ++call, at += 1.0 / 60) {
        scanBatch(shares, {0, 0}, 40, true, at, 0, 1 << 20, 16, [&](ChunkKey key) -> std::optional<Seen> {
            auto cell = shares.find(key);
            int ring = std::max(std::abs(key.x), std::abs(key.z));
            if (cell->seen == Seen::Never) ++freshScans;
            else if (ring <= 4) ++nearScans;
            else ++farScans;
            return ring <= 4 || ring > 30 ? Seen::Partial : Seen::Complete;
        }, [] { return true; });
    }
    check(freshScans >= 250 && nearScans >= 100 && farScans >= 50, "every kind gets turns with one chunk a call");

    ScanGrid backoff;
    seeAll(backoff, dueNow(backoff, {0, 0}, 12, 0), Seen::Complete, 0);
    for (double at : {0.0, 10.0, 20.0}) backoff.saw({10, 0}, Seen::Partial, 0, at);
    auto waitsLonger = [&](double now) {
        auto list = backoff.due(now, 0, 1 << 20, 0, 1000);
        return std::find(list.begin(), list.end(), ChunkKey{10, 0}) != list.end();
    };
    check(!waitsLonger(23) && waitsLonger(24.1), "a far chunk that stays partial is retried less often");
    backoff.saw({10, 0}, Seen::Complete, 0, 40);
    backoff.saw({10, 0}, Seen::Partial, 0, 50);
    check(waitsLonger(53.1), "a complete result resets the backoff");

    // Retries of incomplete chunks probe first, with a whole scan every
    // fourth time in a row, and stay frequent.
    ScanGrid probing;
    dueNow(probing, {0, 0}, 12, 0);
    bool probes = true;
    int wholeScans = 0;
    for (int i = 0; i < 40; ++i) {
        probing.saw({10, 0}, Seen::Partial, 0, i);
        bool expected = probing.find({10, 0})->tries % 4 != 3;
        if (probeFirst(*probing.find({10, 0})) != expected) probes = false;
        if (i >= 20 && !probeFirst(*probing.find({10, 0}))) ++wholeScans;
    }
    check(wholeScans == 5 && rescanAfter(10, Seen::Partial, probing.find({10, 0})->tries) == 4.0,
          "a chunk that stays incomplete keeps a whole scan every fourth retry at the longest wait");
    probing.saw({10, 0}, Seen::Complete, 0, 41);
    check(probes && !probeFirst(*probing.find({10, 0})) && !probeFirst(ScanCell{}),
          "incomplete retries probe first except every fourth; complete and unseen chunks are scanned");
    check(rescanAfter(10, Seen::Partial, 8) <= 4.0 && rescanAfter(10, Seen::Missing, 0) <= 1.0,
          "far incomplete chunks are retried within a few seconds");

    // A chunk that arrives makes its incomplete neighbors due at once, so a
    // patch the client sent fills together instead of as scattered specks.
    ScanGrid patch;
    seeAll(patch, dueNow(patch, {0, 0}, 20, 0), Seen::Partial, 0);
    patch.saw({10, 0}, Seen::Complete, 0, 0.5);
    auto soon = patch.due(0.6, 0, 1 << 20, 16, 1000);
    bool neighbors = true;
    for (ChunkKey key : {ChunkKey{9, -1}, ChunkKey{11, 0}, ChunkKey{10, 1}})
        if (std::find(soon.begin(), soon.end(), key) == soon.end()) neighbors = false;
    check(neighbors && std::find(soon.begin(), soon.end(), ChunkKey{15, 0}) == soon.end(),
          "an arrived chunk's incomplete neighbors are retried at once, others keep waiting");

    // Following the player: chunks still in range keep their state, new ones
    // are due, a jump starts over.
    ScanGrid moving;
    seeAll(moving, dueNow(moving, {0, 0}, 3, 0), Seen::Complete, 0);
    auto stepped = dueNow(moving, {1, 0}, 3, 0.5);
    bool newColumn = stepped.size() == 7;
    for (auto key : stepped)
        if (key.x != 4) newColumn = false;
    check(newColumn, "a step brings only the new column into due");
    check(moving.find({-2, 0}) && moving.find({-2, 0})->seen == Seen::Complete && !moving.find({-3, 0}),
          "chunks still in range keep what was seen, chunks left behind are dropped");
    check(dueNow(moving, {40, 0}, 3, 0.5).size() == 49, "a jump sees everything anew");
    seeAll(moving, dueNow(moving, {40, 0}, 3, 0.5), Seen::Complete, 0.5);
    check(dueNow(moving, {40, 0}, 5, 0.6).size() == 121 - 49 && moving.find({40, 0})->seen == Seen::Complete,
          "a wider radius keeps the middle");

    // Cave layers.
    ScanGrid caves;
    seeAll(caves, dueNow(caves, {0, 0}, 1, 0, false, 64, 0), Seen::Complete, 0, 64);
    check(dueNow(caves, {0, 0}, 1, 0.5, false, 64, 0).empty() && dueNow(caves, {0, 0}, 1, 0.5, false, 65, 0).size() == 9,
          "a cave chunk scanned at another layer is due again");
}
void scheduledScanning() {
    // The time budget: one chunk per call, nearest first, and a chunk
    // waiting for resources is first again on the next call.
    ScanGrid grid;
    std::vector<ChunkKey> scanned;
    auto scan = [&](ChunkKey key) -> std::optional<Seen> {
        scanned.push_back(key);
        return Seen::Complete;
    };
    auto oneChunk = [] { return true; };
    check(scanBatch(grid, {0, 0}, 4, false, 0, 0, 0, 1 << 20, scan, oneChunk) == 1 && scanned[0] == ChunkKey{0, 0},
          "a one-chunk budget scans the center first");
    scanBatch(grid, {0, 0}, 4, false, 0, 0, 0, 1 << 20, scan, oneChunk);
    check(scanned.size() == 2 && std::max(std::abs(scanned[1].x), std::abs(scanned[1].z)) == 1, "then the first ring");
    int tries = 0;
    ChunkKey firstTried{99, 99}, lastTried{99, 99};
    auto waiting = [&](ChunkKey key) -> std::optional<Seen> {
        if (++tries == 1) firstTried = key;
        lastTried = key;
        if (tries < 3) return std::nullopt;
        return Seen::Complete;
    };
    int done = scanBatch(grid, {0, 0}, 4, false, 0, 0, 0, 1 << 20, waiting, [] { return false; });
    done += scanBatch(grid, {0, 0}, 4, false, 0, 0, 0, 1 << 20, waiting, oneChunk);
    done += scanBatch(grid, {0, 0}, 4, false, 0, 0, 0, 1 << 20, waiting, oneChunk);
    check(done == 1 && tries == 3 && firstTried == lastTried && grid.find(lastTried)->seen == Seen::Complete,
          "a chunk waiting for resources is tried again first");

    // The range in game (L-210): at 96 chunks the client had chunks to about
    // 40, the edge stayed partial, and the chunks around the player stayed
    // partial too (provisional tints). Three full scans a frame (the worst
    // case: a missing chunk takes microseconds in game) must still look at
    // the whole range within six minutes, while retries keep their share.
    ScanGrid game;
    int calls = 0;
    auto gameScan = [&](ChunkKey key) -> std::optional<Seen> {
        double d = std::sqrt(double(key.x * key.x + key.z * key.z));
        return d > 40 ? Seen::Missing : d > 33 || d < 5 ? Seen::Partial : Seen::Complete;
    };
    double clock = 0;
    for (; calls < 60 * 360 && (calls == 0 || game.counts().seen[0] > 0); ++calls, clock += 1.0 / 60) {
        int left = 3;
        scanBatch(game, {0, 0}, 97, true, clock, 0, 1 << 20, 16, gameScan, [&] { return --left == 0; });
    }
    check(game.counts().seen[0] == 0 && calls < 60 * 360, "partial chunks near the player and at the edge cannot starve the range");
    check(game.counts().reach == 40, "the reach is the farthest chunk the client had");

    // Moving through the world (L-210): the old resumable walk left arcs of
    // chunks behind when the player moved during a pass. Stand, then walk
    // east one chunk every 30 calls with three chunks a call: every chunk that
    // was well inside the range at some point was scanned.
    ScanGrid trail;
    std::vector<ChunkKey> seen;
    auto record = [&](ChunkKey key) -> std::optional<Seen> {
        seen.push_back(key);
        return Seen::Complete;
    };
    int budget = 0;
    auto three = [&] { return ++budget % 3 == 0; };
    int const radius = 8;
    double time = 0;
    std::vector<ChunkKey> path;
    for (int step = 0; step <= 40; ++step) {
        ChunkKey at{step, 0};
        path.push_back(at);
        for (int call = 0; call < (step == 0 ? 120 : 30); ++call, time += 1.0 / 60)
            scanBatch(trail, at, radius, true, time, 0, 0, 4, record, three);
    }
    bool noHoles = true;
    for (auto at : path)
        for (int dz = -radius + 2; dz <= radius - 2; ++dz)
            for (int dx = -radius + 2; dx <= radius - 2; ++dx) {
                if (dx * dx + dz * dz > (radius - 2) * (radius - 2)) continue;
                ChunkKey key{at.x + dx, at.z + dz};
                if (std::find(seen.begin(), seen.end(), key) == seen.end()) noHoles = false;
            }
    check(noHoles, "moving leaves no chunk behind that was well inside the range");
}
void tiles() {
    TileCache cache;
    auto& tile = cache.put({0, 0});
    tile.loaded = true;
    tile.scannedAt = 10;
    cache.put({1, 0}) = Tile{{}, true, 10};
    cache.put({1, 1}) = Tile{{}, false, 10};
    check(seenOf(*cache.find({0, 0})) == Seen::Complete && seenOf(*cache.find({1, 1})) == Seen::Missing,
          "a tile tells what its scan saw");
    tile.partial = true;
    check(seenOf(tile) == Seen::Partial, "a chunk with blocks not received is partial");
    tile.partial = false;
    tile.columns[static_cast<size_t>(columnIndex(5, 6))] = {packColor(10, 20, 30), 64};
    check(cache.column(5, 6) && cache.column(5, 6)->height == 64 && !cache.column(5, 7) && !cache.column(17, 17),
          "only known columns of loaded chunks are read");
    cache.put({40, 0});
    cache.put({-9, -9});
    cache.put({70, 0});
    std::array<ChunkKey, 1> playerOnly{ChunkKey{0, 0}};
    std::array<ChunkKey, 2> playerAndCamera{ChunkKey{0, 0}, ChunkKey{64, 0}};
    cache.evict(playerAndCamera, 8);
    check(cache.find({70, 0}) != nullptr && cache.find({40, 0}) == nullptr && cache.find({1, 0}) != nullptr,
          "tiles near any scan path's center are kept");
    ScanGrid idle, moved;
    moved.follow({64, 0}, 3, false);
    check(!idle.following() && moved.following() && moved.center() == ChunkKey{64, 0},
          "only a grid that follows a center keeps tiles");
    cache.evict(playerOnly, 8);
    check(cache.find({70, 0}) == nullptr && cache.find({-9, -9}) == nullptr && cache.find({1, 0}) != nullptr,
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
    std::vector<std::uint32_t> green(64 * 64, 0);
    drawArrow(green, 64, 32, 32, 0, 16, cameraArrowColor);
    auto greenPixel = green[32 * 64 + 32];
    check(channel(greenPixel, 1) > channel(greenPixel, 0) && channel(greenPixel, 3) > 200,
          "the camera arrow uses the shared green fill");
    // The minimap draws it at the map center, where the map follows the camera;
    // a heading-up map turns the camera's own direction to the top (L-286).
    auto turned = ViewTransform::headingUp(-90).toMap(heading(-90).x, heading(-90).z);
    check(near(std::atan2(turned.x, -turned.z), 0, 1e-9), "the camera angle points up while the map turns with it");
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
          && applyForce(ViewForce::Cave, ViewMode::Surface) == ViewMode::Cave
          && applyForce(ViewForce::Auto, ViewMode::Cave) == ViewMode::Cave, "a chosen view wins");
    check(static_cast<int>(ViewForce::Auto) == 0 && static_cast<int>(ViewForce::Surface) == 1
          && static_cast<int>(ViewForce::Cave) == 2, "the saved view mode is the enum's index");
    check(cycleForce(ViewForce::Auto) == ViewForce::Surface
          && cycleForce(ViewForce::Surface) == ViewForce::Cave
          && cycleForce(ViewForce::Cave) == ViewForce::Auto, "the key cycles automatic, surface, cave");
    check(viewForceOf(-1) == ViewForce::Auto && viewForceOf(3) == ViewForce::Auto
          && viewForceOf(0) == ViewForce::Auto && viewForceOf(1) == ViewForce::Surface
          && viewForceOf(2) == ViewForce::Cave, "a corrupt saved mode reads as automatic");
    ViewSwitch view;
    check(view.update(ViewMode::Cave, 10) == ViewMode::Cave, "the first switch is immediate");
    check(view.update(ViewMode::Surface, 10.5) == ViewMode::Cave && view.update(ViewMode::Surface, 11) == ViewMode::Surface,
          "the next automatic switch waits a second");
    check(!coveredByMost(4) && coveredByMost(5), "most of the nine columns must have a roof");
    check(stableLayer(0, 64, true) == 64 && stableLayer(64, 67, false) == 64 && stableLayer(64, 61, false) == 64
          && stableLayer(64, 68, false) == 68 && stableLayer(64, 60, false) == 60,
          "the cave layer holds within the slack and then moves to the player");
    check(caveReferenceHeight(63, 64, true) == 63 && caveReferenceHeight(80, 78, true) == 80
              && caveReferenceHeight(0, 68, true) == 68 && caveReferenceHeight(0, 68, false) == 68,
          "the map opens at the recorder's stable band while it is close");
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
    // A lava lake 37 blocks under the player is still part of the cavern the
    // broad view shows; the layer view marks it as another height instead.
    std::array<bool, 54> cavern{};
    cavern[70 - 31] = true;
    auto lava = caveColumn(caveFloor(cavern, 70, 68), 68, packColor(200, 80, 20));
    check(lava.height == 31 && channel(lava.color, 0) > 80,
          "a floor deep in the same open space is still drawn, not turned into a drop");
    check(!caveAtSelectedHeight(31, 68) && caveAtSelectedHeight(62, 68) && caveAtSelectedHeight(76, 68),
          "the layer view keeps floors near the selected height only");
    int reads = 0;
    auto first = caveFloorFrom([&](int y) { ++reads; return y == 67; }, 70, 68, 17);
    check(first.kind == CaveHit::Kind::Floor && first.y == 67 && reads < 12,
          "the cave read stops at the first floor instead of reading the whole deep window");
    TileCache cache;
    auto& tile = cache.put({0, 0});
    tile.loaded = true;
    tile.layer = 64;
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

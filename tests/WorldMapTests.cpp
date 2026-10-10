#include "features/map/MapRegion.h"
#include "features/map/WorldMapView.h"
#include "features/map/SeedLink.h"
#include "features/map/MapFaces.h"
#include "features/map/SkinGeometry.h"
#include "features/map/MapMarks.h"
#include <cmath>
#include <format>
void check(bool, char const*);
namespace {
using namespace lamium::map;
std::array<Column, 256> chunkOf(std::uint32_t color, std::int16_t height) {
    std::array<Column, 256> columns{};
    for (auto& c : columns) c = {color, height};
    return columns;
}
void mapMarks() {
    using namespace lamium::map;
    // L-139: a waypoint over a placement over a shape; a small placement inside a large one.
    std::vector<ScreenMark> marks{{shapeKey(7), 0, 0, 100, 100}, {placementKey(3), 10, 10, 90, 90},
                                  {placementKey(4), 40, 40, 60, 60}, {waypointKey(9), 50, 50, 50, 50}, {deathKey, 80, 80, 80, 80}};
    check(markAt(marks, 52, 51) == waypointKey(9), "a waypoint over placements and shapes takes the cursor");
    check(markAt(marks, 45, 45) == placementKey(4), "of overlapping placements the smaller one is reached");
    check(markAt(marks, 20, 20) == placementKey(3) && markAt(marks, 5, 5) == shapeKey(7), "placements come before shapes");
    check(markAt(marks, 82, 81) == deathKey && !markAt(marks, 150, 150), "the death point is a point too; empty map is nothing");
    std::vector<ScreenMark> twins{{waypointKey(1), 10, 10, 10, 10}, {waypointKey(2), 10, 10, 10, 10}};
    check(markAt(twins, 10, 10) == waypointKey(1), "two waypoints in one place stay apart by id; the first drawn wins");
    check(present(marks, placementKey(4)) && !present(marks, placementKey(5)) && !present(marks, shapeKey(3)),
          "a selection on a mark that is gone is dropped");
}
void regions() {
    check(regionOfChunk({15, -1}) == RegionKey{0, -1} && regionOfChunk({16, -16}) == RegionKey{1, -1}
              && regionOfChunk({-17, 0}) == RegionKey{-2, 0}, "chunks fall into 16x16-chunk regions");
    check(regionOfBlock(-1, 256) == RegionKey{-1, 1} && regionIndex(-1, 256) == 255,
          "negative blocks index from the region's own corner");
    RegionData region;
    std::vector<bool> recorded(regionColumns, false);
    auto grass = packColor(90, 160, 60);
    check(mergeChunk(region, {-1, 2}, chunkOf(grass, 70), &recorded), "a new chunk changes the region");
    check(!mergeChunk(region, {-1, 2}, chunkOf(grass, 70), &recorded), "the same chunk again changes nothing");
    auto half = chunkOf(packColor(10, 20, 30), 64);
    for (size_t i = 0; i < 128; ++i) half[i].color = 0;
    mergeChunk(region, {-1, 2}, half);
    std::array<Column, 256> back{};
    check(copyChunk(region, {-1, 2}, back) && back[0].color == grass && back[200].height == 64,
          "unknown columns keep what was recorded; known ones replace it");
    check(!copyChunk(region, {0, 0}, back), "a chunk never recorded is unknown");
    auto partial = chunkOf(packColor(1, 2, 3), 60);
    partial[5] = {};
    partial[6] = {};
    std::array<Column, 256> saved{};
    std::array<bool, 256> provisional{};
    saved[5] = {grass, 70};
    saved[7] = {grass, 70};
    saved[8] = {grass, 70};
    saved[9] = {packColor(0, 0, 0), 70};
    provisional[8] = provisional[9] = true;
    fillFromSaved(partial, provisional, saved);
    check(partial[5].color == grass && partial[5].height == 70 && !(partial[6].color >> 24)
          && partial[7].color == packColor(1, 2, 3),
          "blocks not received yet show the saved column; received ones and unknown ones stay");
    check(partial[8].color == grass && partial[9].color == packColor(1, 2, 3),
          "stand-in tints yield to saved colors, but never to the black the tint bug saved");
    check(recorded[static_cast<size_t>(regionIndex(-16, 32))] && !recorded[0], "recording marks the columns it wrote");

    auto bytes = encodeRegion(region);
    auto decoded = decodeRegion(bytes);
    check(decoded && decoded->colors == region.colors && decoded->heights == region.heights, "a region survives saving");
    check(bytes.size() < 1000, "runs keep a mostly empty region small");
    check(!decodeRegion(bytes.substr(0, bytes.size() - 8)) && !decodeRegion("LMR1") && !decodeRegion("junk"),
          "a short or foreign file is not a region");
    auto wrong = bytes;
    wrong[4] = static_cast<char>(0xFF);
    wrong[5] = static_cast<char>(0xFF);
    check(!decodeRegion(wrong), "runs past the end are rejected");

    RegionData disk;
    mergeChunk(disk, {0, 0}, chunkOf(packColor(1, 2, 3), 5));
    mergeChunk(disk, {-1, 2}, chunkOf(packColor(4, 5, 6), 6));
    RegionData live;
    std::vector<bool> mask(regionColumns, false);
    mergeChunk(live, {-1, 2}, chunkOf(grass, 70), &mask);
    underlay(live, mask, disk);
    check(copyChunk(live, {0, 0}, back) && back[0].color == packColor(1, 2, 3), "the saved file fills what was not scanned");
    check(copyChunk(live, {-1, 2}, back) && back[0].color == grass, "what was scanned since wins over the file");
}
void images() {
    RegionData region;
    auto stone = packColor(100, 100, 100);
    mergeChunk(region, {0, 0}, chunkOf(stone, 64));
    region.heights[static_cast<size_t>(regionIndex(5, 5))] = 66;
    auto pixels = shadeRegion(region);
    check(pixels[0] == stone && pixels[static_cast<size_t>(regionIndex(20, 0))] == 0, "level ground keeps its color, unknown stays clear");
    check(channel(pixels[static_cast<size_t>(regionIndex(5, 5))], 0) > 100 && channel(pixels[static_cast<size_t>(regionIndex(6, 5))], 0) < 100,
          "a step up is lighter and the column after it darker");
    std::vector<std::uint32_t> parent(regionColumns, 0);
    downsampleInto(parent, pixels, 3);
    check(parent[static_cast<size_t>(128 * 256 + 128)] != 0 && parent[0] == 0 && parent[static_cast<size_t>(128 * 256 + 140)] == 0,
          "a quarter lands in its corner at half size");
    check(layerFolder({0, 0}) == "overworld" && layerFolder({1, 4}) == "nether/y64" && layerFolder({2, 0}) == "end",
          "each dimension and Nether layer has its folder");
    check(mapLayer(1, 70) == MapLayer{1, 4} && mapLayer(1, -1) == MapLayer{1, -1} && mapLayer(0, 70) == MapLayer{0, 0},
          "the Nether is layered by 16 blocks; other dimensions are not");
    check(regionFileName({-3, 7}) == "r.-3.7.lmr", "region files are named by position");
}
void view() {
    WorldView v{100, -50, defaultWorldScale, 400, 200};
    check(std::abs(v.screenX(100) - 200) < 1e-9 && std::abs(v.worldZ(100) + 50) < 1e-9, "the center is mid-screen");
    double wx = v.worldX(300), wz = v.worldZ(40);
    v.zoomAt(300, 40, 2);
    check(v.zoom == defaultWorldScale + 2 && std::abs(v.worldX(300) - wx) < 1e-9 && std::abs(v.worldZ(40) - wz) < 1e-9,
          "zooming keeps the point under the cursor");
    v.zoomAt(0, 0, 99);
    check(v.zoom == static_cast<int>(worldScales.size()) - 1, "zoom stops at the closest step");
    WorldView pan{0, 0, 6, 100, 100}; // One unit per block.
    pan.pan(10, -4);
    check(std::abs(pan.centerX + 10) < 1e-9 && std::abs(pan.centerZ - 4) < 1e-9, "dragging moves the map with the cursor");

    check(lodFor(2, 2) == 0 && lodFor(1 / 8., 2) == 2 && lodFor(1 / 8., 1) == 3 && lodFor(1 / 64., 1) == maxLod,
          "the image level follows screen pixels per block");
    check(parentTile({0, -1, 3}) == TileKey{1, -1, 1}, "a tile's parent covers it");
    WorldView wide{0, 0, defaultWorldScale, 1000, 600}; // 2 units per block: 500 x 300 blocks.
    auto tiles = visibleTiles(wide, 0);
    check(tiles.size() == 4 && tiles.front().x * 256 <= 0 && tiles.front().x * 256 + 256 >= 0, "the tiles on screen, center first");
    check(snapToPixel(10.3, 2) == 10.5 && snapToPixel(10.3, 0) == 10.3, "edges land on screen pixels");
    check(scaleBarBlocks(2, 30) == 20 && scaleBarBlocks(1 / 8., 30) == 500, "the scale bar takes a round length");
}
void seedLinks() {
    check(seedMapPlatform(1, 26, 51) == "bedrock_26_50" && seedMapPlatform(1, 26, 50) == "bedrock_26_50",
          "the game's 1.26.5x is ChunkBase's 26.50 map");
    check(seedMapPlatform(1, 26, 45) == "bedrock_26_30" && seedMapPlatform(1, 26, 23) == "bedrock_26_0",
          "a version takes the map that covers it");
    check(seedMapPlatform(1, 21, 132) == "bedrock_1_21_120" && seedMapPlatform(1, 21, 114) == "bedrock_1_21_110"
              && seedMapPlatform(1, 21, 50) == "bedrock_1_21_50" && seedMapPlatform(1, 21, 40) == "bedrock_1_21",
          "1.21 releases map by patch");
    check(seedMapPlatform(1, 26, 90) == "bedrock_26_50" && seedMapPlatform(1, 30, 0) == "bedrock_26_50",
          "a newer game takes the newest map, never a Java one");
    check(seedMapPlatform(1, 12, 0) == "bedrock_1_14", "an older game takes the oldest map");
    check(seedText(0xFFFFFFFFFFFFFFFFull) == "-1" && seedText(42) == "42", "seeds read as signed numbers");
    check(seedMapUrl(42, "bedrock_26_50", 1, -100, 250, 0.5)
              == "https://www.chunkbase.com/apps/seed-map#seed=42&platform=bedrock_26_50&dimension=nether&x=-100&z=250&zoom=0.5",
          "the link names seed, map, dimension, place and zoom");
    check(seedMapZoom(1) == 1 && seedMapZoom(.25) == .5 && seedMapZoom(1 / 8.) == .25 && seedMapZoom(8) == 1.75,
          "the zoom matches ChunkBase's measured scale");
    check(seedMapZoom(64) == 1.75 && seedMapZoom(1 / 64.) == 0 && seedMapZoom(0) == 1, "the zoom stays in range");
}
void radarFaces() {
    // An 8x4 texture: a 2x3 front at (1,0) in red tones and a 2x1 "nose"
    // front at (4,0) in green; the rest clear.
    std::vector<std::uint8_t> image(8 * 4 * 4, 0);
    auto paint = [&](int x, int y, std::uint8_t r, std::uint8_t g) {
        auto* p = &image[static_cast<size_t>((y * 8 + x) * 4)];
        p[0] = r; p[1] = g; p[2] = 0; p[3] = 255;
    };
    for (int y = 0; y < 3; ++y) for (int x = 1; x < 3; ++x) paint(x, y, static_cast<std::uint8_t>(x * 100), 0);
    paint(4, 0, 0, 200); paint(5, 0, 0, 250);
    // The head spans x 0-2, y 0-3 (y up), at depth 0; the nose 1 nearer, low.
    auto face = composeFace(image.data(), 8, 4, 1, {{0, 0, 2, 3, 0, 1, 0, 2, 3}, {0, 0, 2, 1, -1, 4, 0, 2, 1}});
    check(face && face->width == 2 && face->height == 3, "the front view spans every box");
    check(channel(face->pixels[0], 0) == 100 && channel(face->pixels[1], 0) == 200, "the head's texels keep their places");
    check(channel(face->pixels[4], 1) == 200 && channel(face->pixels[5], 1) == 250, "a nearer box covers the head where it sits");
    auto behind = composeFace(image.data(), 8, 4, 1, {{0, 0, 2, 1, 5, 4, 0, 2, 1}, {0, 0, 2, 3, 0, 1, 0, 2, 3}});
    check(behind && channel(behind->pixels[4], 0) == 100, "a box behind the face is covered by it");
    auto wide = composeFace(image.data(), 8, 4, 1, {{0, 0, 2, 3, 0, 1, 0, 2, 3}, {2, 2, 3, 3, 0, 4, 0, 1, 1}});
    check(wide && wide->width == 3 && channel(wide->pixels[0], 1) == 200 && !(wide->pixels[3] >> 24),
          "a box beside the head widens the face; seen from the front +x lies left; gaps stay clear");
    check(!composeFace(image.data(), 8, 4, 1, {{0, 0, 1, 1, 0, 7, 3, 1, 1}}), "a see-through front has no face");
    image[static_cast<size_t>((3 * 8 + 6) * 4 + 3)] = 40; // A mask alpha, as sheep and cats have.
    auto masked = composeFace(image.data(), 8, 4, 1, {{0, 0, 1, 1, 0, 6, 3, 1, 1}});
    check(masked && channel(masked->pixels[0], 3) == 255, "partial alpha in an entity texture is drawn opaque");
    check(!composeFace(image.data(), 8, 4, 1, {}) && !composeFace(nullptr, 8, 4, 1, {{0, 0, 1, 1, 0, 1, 0, 1, 1}}),
          "no boxes or no image, no face");
    std::vector<std::uint8_t> fine(64 * 64 * 4, 255);
    auto hd = composeFace(fine.data(), 64, 64, 4, {{0, 0, 8, 10, 0, 0, 0, 8, 10}});
    check(hd && hd->width == 8 && hd->height == 10, "a high-resolution face shrinks by a whole factor to fit 16");
    check(baseTexture("minecraft:villager_v2") == "textures/entity/villager2/villager" && baseTexture("minecraft:zombie").empty(),
          "only renderers whose default skin is an overlay use another texture");
    check(!faceLayer("hat") && !faceLayer("helmet") && faceLayer("nose") && faceLayer("head"), "worn layers are left out");
    std::vector<std::uint32_t> atlas;
    writeFace(atlas, 15, *face);
    auto cell = faceAtlasCell(15);
    auto at = [&](int x, int y) { return atlas[static_cast<size_t>(cell.y + y) * faceAtlasSide + cell.x + x]; };
    check(cell.x == 18 && cell.y == 18 && at(1, 1) == face->pixels[0], "faces sit in their own atlas cells inside an outline");
    check(at(0, 0) == faceOutline && at(3, 4) == faceOutline && outlinedWidth(*face) == 4, "a one-texel black outline surrounds the face");
    check(faceTexelPixels(8, 24) == 3 && faceTexelPixels(11, 24) == 3 && faceTexelPixels(8, 2) == 1 && faceTexelPixels(0, 24) == 1,
          "texels take whole screen pixels, the same for taller faces, at least one");
    check(faceTexelPixels(4, 24) == 4 && faceTexelPixels(2, 24) == 4, "tiny faces get bigger texels");
    for (double target = 4; target <= 40; target += .5)
        check(6 * faceTexelPixels(6, target) <= 8 * faceTexelPixels(8, target),
              "a 6-texel face is never drawn larger than an 8-texel one");
    check(faceTexelPixels(6, 24) == 4, "a 6-texel face still fills the target when it divides evenly");
}
void playerHeads() {
    auto skin = [](int width, int height) { return std::vector<std::uint8_t>(static_cast<size_t>(width) * height * 4, 0); };
    auto paint = [](std::vector<std::uint8_t>& image, int width, int x, int y, std::uint8_t r, std::uint8_t a) {
        auto* p = &image[static_cast<size_t>((y * width + x) * 4)];
        p[0] = r; p[3] = a;
    };
    auto classic = skin(64, 64);
    for (int y = 8; y < 16; ++y) for (int x = 8; x < 16; ++x) paint(classic, 64, x, y, 10, 255);
    paint(classic, 64, 9, 8, 20, 0);    // A clear texel in the base layer is still drawn.
    paint(classic, 64, 42, 10, 99, 255); // The outer layer, 2 right and 2 down.
    paint(classic, 64, 43, 10, 77, 60);  // Partial alpha in the outer layer covers too.
    auto head = playerHead(classic.data(), 64, 64);
    check(head && head->width == 8 && head->height == 8, "a classic skin gives an 8x8 head");
    check(channel(head->pixels[0], 0) == 10 && channel(head->pixels[1], 0) == 20 && channel(head->pixels[1], 3) == 255,
          "the base layer is drawn whole");
    check(channel(head->pixels[2 * 8 + 2], 0) == 99 && channel(head->pixels[2 * 8 + 3], 0) == 77,
          "the outer layer covers the base where it has any alpha");
    auto legacy = skin(64, 32);
    paint(legacy, 64, 8, 8, 5, 255);
    check(playerHead(legacy.data(), 64, 32).has_value(), "a legacy 64x32 skin has a head");
    auto fine = skin(256, 256);
    paint(fine, 256, 32, 32, 50, 255);
    auto hd = playerHead(fine.data(), 256, 256);
    check(hd && hd->width == 16 && channel(hd->pixels[0], 0) == 50, "a 256-pixel skin shrinks its 32-texel head to 16");
    check(!playerHead(skin(64, 64).data(), 64, 64), "a clear head is no head");
    check(!playerHead(classic.data(), 48, 64) && !playerHead(classic.data(), 64, 48) && !playerHead(nullptr, 64, 64),
          "other layouts have no head");
}
void skinGeometry() {
    // The current format, box UV: the head's front sits at uv + depth.
    auto head = skinHead(R"({"format_version":"1.12.0","minecraft:geometry":[{"description":{"identifier":"geometry.other",
        "texture_width":64,"texture_height":64},"bones":[{"name":"body","cubes":[{"origin":[0,0,0],"size":[1,1,1],"uv":[0,0]}]}]},
        {"description":{"identifier":"geometry.persona_x","texture_width":256,"texture_height":256},"bones":[
        {"name":"root"},{"name":"head","parent":"root","cubes":[{"origin":[-4,24,-4],"size":[8,8,8],"uv":[100,40]}]},
        {"name":"hat","parent":"head","cubes":[{"origin":[-4,24,-4],"size":[8,8,8],"uv":[140,40],"inflate":0.5}]},
        {"name":"helmet","parent":"head","cubes":[{"origin":[-4,24,-4],"size":[8,8,8],"uv":[0,0]}]}]}]})",
                         {"geometry.persona_x", ""});
    check(head && head->boxes.size() == 2 && head->textureWidth == 256, "the named geometry's head and hat, armor left out");
    check(head->boxes[0].u == 108 && head->boxes[0].v == 48 && head->boxes[0].w == 8 && head->boxes[1].u == 148,
          "box UV puts the front at uv plus the depth");
    check(head->boxes[1].z < head->boxes[0].z, "the outer layer is nearer than the face");
    // The legacy format, per-face UV, a geometry key with its parent.
    auto legacy = skinHead(R"({"geometry.custom:geometry.humanoid":{"texturewidth":128,"bones":[{"name":"Head",
        "cubes":[{"origin":[-4,24,-4],"size":[8,8,8],"uv":{"north":{"uv":[16,16],"uv_size":[16,16]}}}]}]}})",
                           {"geometry.custom", ""});
    check(legacy && legacy->boxes.size() == 1 && legacy->boxes[0].u == 16 && legacy->boxes[0].w == 16
              && legacy->textureWidth == 128,
          "legacy geometry and per-face UV are read");
    check(skinHead(R"("{\"geometry.a\":{\"bones\":[{\"name\":\"head\",\"cubes\":[{\"origin\":[0,0,0],\"size\":[8,8,8],\"uv\":[0,0]}]}]}}")", {})
              .has_value(),
          "geometry kept as text in a string is read");
    check(!skinHead("null", {}) && !skinHead("{", {}) && !skinHead(R"({"minecraft:geometry":[{"bones":[{"name":"body"}]}]})", {}),
          "no geometry or no head, no head boxes");
    auto patch = patchGeometry(R"({"geometry":{"animated_face":"geometry.face_x","default":"geometry.persona_x"}})");
    check(patch.geometry == "geometry.persona_x" && patch.animatedFace == "geometry.face_x" && patchGeometry("").geometry.empty(),
          "the resource patch names the geometry and the animated face");
    // A character-creator skin: no head in the default geometry; in the
    // animated face geometry a mesh head and a half-unit larger hat, only
    // their fronts (normal 0,0,-1) taken, v counted from the bottom.
    auto mesh = [](double r, double v0, double v1) {
        return std::format(R"({{"normalized_uvs":true,"normals":[[0,1,0],[0,0,-1]],
            "positions":[[-{0},24,-{0}],[{0},24,-{0}],[{0},{1},-{0}],[-{0},{1},-{0}]],
            "uvs":[[0.25,{2}],[0.5,{2}],[0.5,{3}],[0.25,{3}]],
            "polys":[[[0,0,0],[1,0,1],[2,0,2]],[[0,1,0],[1,1,1],[2,1,2],[3,1,3]]]}})", r, 24 + 2 * r, v0, v1);
    };
    auto persona = skinHead(std::format(R"({{"minecraft:geometry":[
        {{"description":{{"identifier":"geometry.persona_x","texture_width":256,"texture_height":256}},
          "bones":[{{"name":"head","parent":"body"}},{{"name":"body","poly_mesh":{0}}}]}},
        {{"description":{{"identifier":"geometry.face_x","texture_width":32,"texture_height":64}},
          "bones":[{{"name":"head","poly_mesh":{0}}},{{"name":"hat","parent":"head","poly_mesh":{1}}}]}}]}})",
        mesh(4, .75, .875), mesh(4.5, .5, .625)), patch);
    check(persona && persona->animatedFace && persona->boxes.size() == 2 && persona->textureWidth == 32,
          "a character-creator head comes from the animated face geometry");
    auto const& face0 = persona->boxes[0];
    check(face0.u == 8 && face0.v == 8 && face0.w == 8 && face0.h == 8 && face0.x0 == -4 && face0.y1 == 32,
          "the mesh front's UVs, v from the bottom, in texture units");
    check(persona->boxes[1].v == 24 && persona->boxes[1].x0 == -4 && persona->boxes[1].y1 == 32
              && persona->boxes[1].z < face0.z,
          "the larger hat is laid over the face at its size, nearer");
    // Together with the face composer: a persona-like layout.
    std::vector<std::uint8_t> image(256 * 256 * 4, 0);
    for (int y = 48; y < 56; ++y) for (int x = 108; x < 116; ++x) image[static_cast<size_t>((y * 256 + x) * 4) + 3] = 255;
    image[static_cast<size_t>((48 * 256 + 148) * 4)] = 200;
    image[static_cast<size_t>((48 * 256 + 148) * 4) + 3] = 255;
    auto face = composeFace(image.data(), 256, 256, 256 / head->textureWidth, head->boxes);
    check(face && face->width == 8 && face->height == 8 && channel(face->pixels[0], 0) == 200 && (face->pixels[63] >> 24),
          "the face comes from the geometry's place, the outer layer over it");
}
}
void worldMapTests() {
    radarFaces();
    playerHeads();
    skinGeometry();
    seedLinks();
    regions();
    mapMarks();
    images();
    view();
}

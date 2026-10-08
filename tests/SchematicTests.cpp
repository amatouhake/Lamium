#include <cmath>
#include "features/schematic/Structure.h"
#include "features/schematic/Verify.h"
#include "features/schematic/PlacementStore.h"
#include "features/schematic/Verification.h"
#include "features/schematic/SaveArea.h"
#include "ui/SavePromptLayout.h"
#include "ui/RadialLayout.h"
#include "ui/SchematicFiles.h"
#include "features/schematic/MaterialAmount.h"
#include "features/schematic/MenuModel.h"
#include "features/schematic/RestPose.h"
#include "features/schematic/PreviewView.h"
#include "features/schematic/GhostFaces.h"
#include <array>
#include <set>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
void check(bool, char const*);
namespace {
using namespace lamium::schematic;
std::span<std::uint8_t const> span(std::string const& bytes) {
    return {reinterpret_cast<std::uint8_t const*>(bytes.data()), bytes.size()};
}
bool throws(std::string const& bytes) {
    try { parseStructure(span(bytes)); } catch (std::runtime_error const&) { return true; }
    return false;
}

Structure sample() {
    Structure s;
    s.size = {2, 3, 4};
    s.worldOrigin = {100, 64, -20};
    nbt::Compound stairs;
    stairs.set("weirdo_direction", {std::int32_t{1}});
    stairs.set("upside_down_bit", {std::int8_t{0}});
    s.palette = {{"minecraft:air", {}, 18168865}, {"minecraft:stone", {}, 18168865},
                 {"minecraft:oak_stairs", stairs, 18168865}, {"minecraft:chest", {}, 18168865},
                 {"minecraft:water", {}, 18168865}};
    s.blocks.assign(s.cells(), 0);
    s.blocks[s.cell(0, 0, 0)] = 1;
    s.blocks[s.cell(1, 2, 3)] = 2;
    s.blocks[s.cell(0, 1, 2)] = 3;
    s.blocks[s.cell(1, 0, 1)] = voidCell;
    s.liquids.assign(s.cells(), voidCell);
    s.liquids[s.cell(1, 2, 3)] = 4;
    nbt::Compound chest;
    chest.set("id", {std::string{"Chest"}});
    chest.set("Items", {nbt::List{nbt::Type::Compound, {}}});
    s.blockEntities[s.cell(0, 1, 2)] = chest;
    nbt::Compound stand;
    stand.set("identifier", {std::string{"minecraft:armor_stand"}});
    nbt::List pos{nbt::Type::Float, {{101.5f}, {64.f}, {-17.5f}}};
    stand.set("Pos", {pos});
    s.entities.push_back({"minecraft:armor_stand", 1.5, 0, 2.5, stand});
    return s;
}

void nbtBasics() {
    nbt::Root root{"", {}};
    root.compound.set("b", {std::int8_t{-3}});
    root.compound.set("s", {std::string{"hé"}});
    root.compound.set("l", {nbt::List{nbt::Type::Short, {{std::int16_t{7}}, {std::int16_t{-8}}}}});
    root.compound.set("a", {std::vector<std::int32_t>{1, 2, 3}});
    auto bytes = nbt::write(root);
    check(bytes[0] == 10 && bytes[1] == 0 && bytes[2] == 0, "the root is an unnamed compound");
    auto back = nbt::read(span(bytes));
    check(nbt::text({back.compound}) == R"({b:-3b,s:"hé",l:[7s,-8s],a:[1,2,3]})",
          "NBT round-trips little-endian with entry order kept");
    std::int64_t value = 0;
    check(back.compound.find("b")->integer(value) && value == -3 && !back.compound.find("s")->integer(value),
          "integers of any width read as integers; strings do not");
    bool truncated = false;
    try { nbt::read(span(bytes.substr(0, bytes.size() - 3))); } catch (std::runtime_error const&) { truncated = true; }
    check(truncated, "truncated NBT is rejected");
    std::string huge = bytes.substr(0, 3) + std::string("\x0b\x01\x00\x61\xff\xff\xff\x7f", 8);
    bool tooLong = false;
    try { nbt::read(span(huge)); } catch (std::runtime_error const&) { tooLong = true; }
    check(tooLong, "an array length past the end is rejected before allocating");
}

void structureRoundTrip() {
    auto original = sample();
    check(original.cell(0, 0, 1) == 1 && original.cell(0, 1, 0) == 4 && original.cell(1, 0, 0) == 12,
          "cells run z fastest, then y, then x");
    auto at = original.position(original.cell(1, 2, 3));
    check(at[0] == 1 && at[1] == 2 && at[2] == 3, "a cell index maps back to its position");
    auto parsed = parseStructure(span(writeStructure(original)));
    check(parsed.size == original.size && parsed.worldOrigin == original.worldOrigin, "size and origin survive");
    check(parsed.blocks == original.blocks && parsed.liquids == original.liquids, "both block layers survive");
    check(parsed.palette.size() == 5 && parsed.palette[2].key() == "minecraft:oak_stairs[upside_down_bit=0b,weirdo_direction=1]",
          "palette keys sort states by name");
    check(parsed.palette[0].isAir() && !parsed.palette[1].isAir(), "air is recognized by name");
    check(parsed.blockEntities.size() == 1 && parsed.blockEntities.contains(original.cell(0, 1, 2))
          && *parsed.blockEntities.at(original.cell(0, 1, 2)).find("id")->as<std::string>() == "Chest",
          "block entity data is keyed by cell");
    check(parsed.entities.size() == 1 && parsed.entities[0].identifier == "minecraft:armor_stand"
          && parsed.entities[0].x == 1.5 && parsed.entities[0].y == 0 && parsed.entities[0].z == 2.5,
          "entity positions become relative to the structure corner");

    auto dry = original;
    dry.liquids.clear();
    check(parseStructure(span(writeStructure(dry))).liquids.empty(), "an all-void second layer reads as empty");

    // Older exports store each layer as a list of ints.
    nbt::Root legacy = nbt::read(span(writeStructure(dry)));
    auto& body = *legacy.compound.entries[2].tag.as<nbt::Compound>();
    nbt::List layers{nbt::Type::List, {}};
    nbt::List ints{nbt::Type::Int, {}};
    for (auto index : dry.blocks) ints.items.push_back({index});
    layers.items.push_back({ints});
    body.set("block_indices", {layers});
    check(parseStructure(span(nbt::write(legacy))).blocks == dry.blocks, "list-of-int layers read too");
}

void structureRejects() {
    check(throws("") && throws("\x0a\x00\x00\x00"), "empty input and an empty compound are rejected");
    auto bad = sample();
    bad.blocks[0] = 9;
    bool caught = false;
    try { parseStructure(span(writeStructure(bad))); } catch (std::runtime_error const&) { caught = true; }
    check(caught, "a block index outside the palette is rejected");
    auto mismatched = sample();
    mismatched.blocks.pop_back();
    caught = false;
    try { writeStructure(mismatched); } catch (std::runtime_error const&) { caught = true; }
    check(caught, "writing a layer that does not match the size is refused");
}

// Optional: LAMIUM_SAMPLE_STRUCTURES names a folder of real exports to parse.
void sampleFiles() {
    char* folder = nullptr;
    size_t length = 0;
    if (_dupenv_s(&folder, &length, "LAMIUM_SAMPLE_STRUCTURES") || !folder) return;
    std::unique_ptr<char, decltype(&std::free)> owned(folder, &std::free);
    for (auto const& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.path().extension() != ".mcstructure") continue;
        std::ifstream file(entry.path(), std::ios::binary);
        std::string bytes{std::istreambuf_iterator<char>(file), {}};
        auto parsed = parseStructure(span(bytes));
        check(parsed.blocks.size() == parsed.cells() && !parsed.palette.empty(), "a real export parses");
        auto again = parseStructure(span(writeStructure(parsed)));
        check(again.blocks == parsed.blocks && again.palette.size() == parsed.palette.size()
              && again.blockEntities.size() == parsed.blockEntities.size() && again.entities.size() == parsed.entities.size(),
              "a real export survives writing back");
    }
}

void placementTransforms() {
    Size size{3, 2, 5};
    for (int rotation = 0; rotation < 4; ++rotation)
        for (auto mirror : {Mirror::None, Mirror::X, Mirror::Z}) {
            Placement placement{{10, 64, -7}, rotation, mirror};
            Size placed = placedSize(size, rotation);
            std::set<std::tuple<int, int, int>> seen;
            bool inside = true, inverse = true;
            for (int x = 0; x < size.x; ++x) for (int y = 0; y < size.y; ++y) for (int z = 0; z < size.z; ++z) {
                auto world = toWorld(size, placement, {x, y, z});
                int dx = world.x - 10, dy = world.y - 64, dz = world.z + 7;
                inside = inside && dx >= 0 && dy >= 0 && dz >= 0 && dx < placed.x && dy < placed.y && dz < placed.z;
                seen.insert({world.x, world.y, world.z});
                auto back = toLocal(size, placement, world);
                inverse = inverse && back && *back == Point{x, y, z};
            }
            check(inside && seen.size() == static_cast<size_t>(size.x * size.y * size.z),
                  "every turn and mirror fills exactly the placed box");
            check(inverse, "toLocal undoes toWorld");
        }
    check(footprint(size, {{10, 64, -7}, 0, Mirror::None}) == Footprint{10, -7, 13, -2}
          && footprint(size, {{10, 64, -7}, 1, Mirror::X}) == Footprint{10, -7, 15, -4},
          "the footprint covers the placed box from above, turned");
    {
        using namespace lamium::ui::schematic_files;
        std::vector<std::string> paths{"farms/sugarcane.mcstructure", "hut.mcstructure", "farms/iron.mcstructure",
                                       "a/b/deep.mcstructure", "well.mcstructure"};
        auto list = rows(paths);
        check(list.size() == 8 && list[0].file == -1 && list[0].folder.empty() && list[1].file == 1 && list[2].file == 4
              && list[3].folder == "a/b/" && list[4].file == 3 && list[5].folder == "farms/" && list[6].file == 2 && list[7].file == 0,
              "files group under folder headings, the top level first");
        auto flat = rows({"b.mcstructure", "a.mcstructure"});
        check(flat.size() == 2 && flat[0].file == 1 && flat[1].file == 0, "without subfolders there are no headings");
        check(rowOf(list, 3) == 4 && rowOf(list, 9) == -1, "a file's row is found");
    }
    check(amountOf(2000, 64) == Amount{1, 4, 16} && amountOf(52, 64) == Amount{0, 0, 52}
          && amountOf(1728, 64) == Amount{1, 0, 0} && amountOf(40, 16) == Amount{0, 2, 8},
          "amounts split into chests of 27 stacks, stacks and items, by the item's stack size");
    check(calculatorName("minecraft:oak_planks") == "oakplanks" && calculatorName("minecraft:stonebrick") == "stonebricks",
          "calculator names drop underscores and fix the ids the site names differently");
    check(calculatorUrl({{"minecraft:oak_planks", 32}, {"", 4}, {"minecraft:glass", 0}, {"minecraft:oak_stairs", 5}})
              == "https://resourcecalculator.com/minecraft/#oakplanks=32&oakstairs=5",
          "the calculator link carries the remaining items and skips empty ones");
    {
        std::map<std::string, std::string> expected{{"direction", "2"}, {"half", "top"}, {"open", "0"}};
        std::map<std::string, std::string> actual{{"direction", "1"}, {"open", "0"}, {"powered", "1"}};
        auto diff = stateDifferences(expected, actual);
        check(diff.size() == 3 && diff[0] == StateDifference{"direction", "2", "1"}
              && diff[1] == StateDifference{"half", "top", "-"} && diff[2] == StateDifference{"powered", "-", "1"},
              "differing states in key order, one-sided ones with a dash, equal ones left out");
        check(stateDifferences(expected, actual, 1).size() == 1 && stateDifferences(expected, expected).empty(),
              "state differences respect the limit and are empty for equal states");
    }
    Placement turned{{0, 0, 0}, 1, Mirror::None};
    // A box 3 wide (x) and 5 deep (z): after one clockwise turn it is 5 wide and 3 deep,
    // and its north-east corner moves to the south-east.
    check(placedSize(size, 1) == Size{5, 2, 3} && toWorld(size, turned, {2, 0, 0}) == Point{4, 0, 2},
          "a clockwise turn takes the north-east corner to the south-east");
    check(toWorld(size, {{0, 0, 0}, 0, Mirror::X}, {0, 0, 0}) == Point{2, 0, 0}
          && toWorld(size, {{0, 0, 0}, 0, Mirror::Z}, {0, 0, 0}) == Point{0, 0, 4},
          "mirror X flips east-west and mirror Z flips north-south");
    check(!toLocal(size, turned, {5, 0, 0}) && !toLocal(size, turned, {0, 2, 0}) && !toLocal(size, turned, {-1, 0, 0}),
          "cells outside the placed box have no local cell");
    check(quarterTurns(-1) == 3 && quarterTurns(5) == 1, "turn counts wrap");

    bool centers = true;
    for (int rotation = 0; rotation < 4; ++rotation)
        for (auto mirror : {Mirror::None, Mirror::X, Mirror::Z}) {
            Placement placement{{10, 64, -7}, rotation, mirror};
            for (int x = 0; x < size.x; ++x) for (int z = 0; z < size.z; ++z) {
                auto cell = toWorld(size, placement, Point{x, 1, z});
                auto free = toWorldPosition(size, placement, Position{x + .5, 1.25, z + .5});
                centers = centers && free == Position{cell.x + .5, cell.y + .25, cell.z + .5};
            }
        }
    check(centers, "an entity in a cell's center stays in that cell's center after any turn and mirror");

    bool facings = true;
    constexpr double toRadians = 3.14159265358979 / 180;
    for (int rotation = 0; rotation < 4; ++rotation)
        for (auto mirror : {Mirror::None, Mirror::X, Mirror::Z})
            for (float yaw : {0.f, 90.f, -45.f, 170.f}) {
                Placement placement{{10, 64, -7}, rotation, mirror};
                Position from{1.5, 0, 2.5}, ahead{from.x - std::sin(yaw * toRadians), 0, from.z + std::cos(yaw * toRadians)};
                auto a = toWorldPosition(size, placement, from), b = toWorldPosition(size, placement, ahead);
                double turned = toWorldYaw(yaw, placement) * toRadians;
                facings = facings && std::abs(b.x - a.x + std::sin(turned)) < 1e-4 && std::abs(b.z - a.z - std::cos(turned)) < 1e-4;
            }
    check(facings, "an entity keeps facing the same way relative to the structure after any turn and mirror");

    auto near = [](std::optional<float> v, float want) { return v && std::abs(*v - want) < 1e-4f; };
    check(near(constantMolang("90"), 90) && near(constantMolang(" 90 - this "), 90) && near(constantMolang("-this + 22.5"), 22.5f)
          && near(constantMolang("(1 + 2) * -3 / 2"), -4.5f) && near(constantMolang("1.5f"), 1.5f),
          "rest-pose Molang: numbers, this as 0, arithmetic and parentheses");
    check(near(constantMolang("-3.0 - this", 2), -5) && near(constantMolang("-14 - this", -14), 0),
          "rest-pose Molang: this is the bone's rest value, so the result is the offset from it");
    check(!constantMolang("query.is_sitting ? 0 : 90") && !constantMolang("variable.tcos0 * 57.3") && !constantMolang("thisx")
          && !constantMolang("") && !constantMolang("1 / 0") && !constantMolang("(1"),
          "rest-pose Molang: anything needing the entity is not a constant");
}

void previewRules() {
    using namespace lamium::schematic::preview;
    View view{35, 30};
    check(drawOrder(view) == Order{1, 1, 1} && drawOrder(View{-145, 30}) == Order{-1, 1, -1},
          "the preview walks each axis from the end away from the viewer");
    auto order = drawOrder(view);
    check(facesViewer(order, 0, 1, 0) && !facesViewer(order, 0, -1, 0) && facesViewer(order, 1, 0, 0) && !facesViewer(order, 0, 0, -1),
          "only faces turned to the viewer are kept");
    auto e = eye(view);
    auto center = project(view, e.x, e.y, e.z);
    auto up = project(View{0, 0}, 0, 1, 0);
    check(std::abs(center.right) < 1e-4f && std::abs(center.down) < 1e-4f && std::abs(center.toward - 1) < 1e-4f
          && std::abs(up.down + 1) < 1e-4f && std::abs(up.right) < 1e-4f,
          "the eye direction projects onto the middle, toward the viewer, and up is up on screen");
    auto full = [](int, int, int) { return true; };
    auto cells = visibleCells(3, 3, 3, order, full, full);
    auto glassMiddle = visibleCells(3, 3, 3, order, full, [](int x, int y, int z) { return !(x == 1 && y == 1 && z == 2); });
    check(glassMiddle.size() == 27, "a block that does not cover its neighbors (glass, stairs) shows the cell behind it");
    bool farFirst = true;
    for (size_t i = 1; i < cells.size(); ++i) {
        auto depth = [&](std::uint32_t c) { int x = int(c) / 9, y = int(c) / 3 % 3, z = int(c) % 3; return project(view, x, y, z).toward; };
        // A later cell is never wholly behind an earlier one along all axes.
        int a = int(cells[i - 1]), b = int(cells[i]);
        bool behind = a / 9 >= b / 9 && a / 3 % 3 >= b / 3 % 3 && a % 3 >= b % 3 && a != b;
        farFirst = farFirst && !(behind && depth(cells[i]) < depth(cells[i - 1]));
    }
    check(cells.size() == 26 && cells.front() == 0 && cells.back() == 26 && farFirst,
          "the hidden middle cell is left out and cells come far to near");
    check(fitScale(4, 4, 4, 100, 60) > 0 && fitScale(4, 4, 4, 100, 60) * std::sqrt(48.f) <= 60,
          "the preview fits the structure's diagonal in the smaller side");
    float most = maxZoom(view, 10, 2, 10, 200, 120);
    bool inside = true;
    for (int k = 0; k < 8; ++k) {
        auto p = project(view, k & 1 ? 5 : -5, k & 2 ? 1 : -1, k & 4 ? 5 : -5);
        float s = fitScale(10, 2, 10, 200, 120) * most;
        inside = inside && std::abs(p.right * s) <= 100 && std::abs(p.down * s) <= 60;
    }
    check(most >= 1 && inside, "the largest zoom keeps every corner inside the box");
    auto top = cutFor(View{35, 60}, 5, 8, 5, 3), side = cutFor(View{90, 10}, 5, 8, 5, 2);
    check(top == Cut{1, 1, 4} && top.keeps(0, 4, 0) && !top.keeps(0, 5, 0) && side.axis == 0 && side.keeps(2, 0, 0) && !side.keeps(3, 0, 0)
          && cutFor(view, 5, 8, 5, 0).axis < 0 && cutFor(View{35, 60}, 5, 8, 5, 99).limit == 0,
          "peeling takes layers off the side the view looks down on, never the last one");
    View turned{90, 10};
    turned.peelAxis = 1;
    check(cutFor(turned, 5, 8, 5, 3) == Cut{1, 1, 4}, "a cut started from above stays a height cut when the view turns to the side");
    auto above = pick(View{0, 89}, 3, 3, 3, 0, 0, full);
    auto front = pick(View{0, 0}, 3, 3, 3, 0, 0, full);
    auto left = pick(View{0, 0}, 3, 3, 3, -1, 0, full);
    auto none = pick(View{0, 0}, 3, 3, 3, 5, 0, full);
    auto hollow = pick(View{0, 0}, 3, 3, 3, 0, 0, [](int, int, int z) { return z == 0; });
    check(above == Cell{1, 2, 1} && front == Cell{1, 1, 2} && left == Cell{0, 1, 2} && !none && hollow == Cell{1, 1, 0},
          "a click picks the nearest block along the line of sight, and nothing beside the build");
}

void saveRules() {
    Area area{{5, 70, -2}, {3, 64, 1}};
    check(area.low() == Point{3, 64, -2} && area.size() == Size{3, 7, 4} && area.cells() == 84,
          "an area spans both corner blocks in any order");
    check(schematicFileName("hut") == "hut.mcstructure" && schematicFileName("hut.mcstructure") == "hut.mcstructure"
          && schematicFileName("  a/b:c?. ") == "abc.mcstructure" && schematicFileName(" .. ").empty() && schematicFileName("").empty(),
          "file names drop characters Windows forbids and do not double the extension");
    check(schematicFileName("小屋") == "小屋.mcstructure", "file names keep non-ASCII text");
    auto longName = schematicFileName(std::string(70, 'a'));
    auto longJapanese = schematicFileName(std::string(30, 'x') + "あいうえおかきくけこさしすせそ");
    check(longName.size() == maxSaveName + 12 && longJapanese.size() <= maxSaveName + 12
          && (static_cast<unsigned char>(longJapanese[longJapanese.size() - 13]) & 0xc0) != 0xc0,
          "long names are cut without splitting a character");

    check(compassOctant(0, -5) == 0 && compassOctant(5, -5) == 1 && compassOctant(5, 0) == 2 && compassOctant(0, 5) == 4
          && compassOctant(-5, 0) == 6 && compassOctant(-5, -5) == 7, "directions name north as -z, clockwise");
    check(chunkOf(0) == 0 && chunkOf(15) == 0 && chunkOf(16) == 1 && chunkOf(-1) == -1 && chunkOf(-16) == -1 && chunkOf(-17) == -2,
          "blocks map to chunks with floor division");
    Area wide{{-20, 60, 5}, {17, 62, 40}};
    auto columns = chunkColumns(wide);
    std::uint64_t covered = 0;
    for (auto const& c : columns) covered += c.cells(wide.size().y);
    check(columns.size() == 4 * 3 && covered == wide.cells() && columns.front().lowX == -20 && columns.front().highX == -17
          && columns.back().lowX == 16 && columns.back().highX == 17 && columns.back().highZ == 40,
          "chunk columns cover the area exactly, cut at chunk borders");

    StructureBuilder builder({2, 1, 2}, {10, 64, 20});
    PaletteBlock stone{"minecraft:stone", {}, 1}, air{"minecraft:air", {}, 1}, water{"minecraft:water", {}, 1};
    nbt::Compound west;
    west.set("weirdo_direction", {std::int32_t{1}});
    PaletteBlock stairs{"minecraft:oak_stairs", west, 1};
    builder.setBlock(0, stone);
    builder.setBlock(1, air);
    builder.setBlock(2, stone);
    builder.setBlock(3, stairs);
    builder.setLiquid(3, water);
    nbt::Compound standData;
    standData.set("identifier", {std::string("minecraft:armor_stand")});
    nbt::List pos{nbt::Type::Float, {}};
    for (float v : {11.5f, 64.f, 21.5f}) pos.items.push_back({v});
    standData.set("Pos", {pos});
    builder.addEntity({"minecraft:armor_stand", 1.5, 0, 1.5, standData});
    auto const& built = builder.structure();
    check(built.palette.size() == 4 && built.blocks[0] == built.blocks[2] && built.liquids.size() == 4 && built.liquids[0] == voidCell,
          "equal blocks share a palette entry and the liquid layer is filled only where set");
    auto parsed = parseStructure(span(writeStructure(built)));
    check(parsed.size == built.size && parsed.blocks == built.blocks && parsed.liquids == built.liquids
          && parsed.palette[2].key() == stairs.key() && parsed.entities.size() == 1 && parsed.entities[0].x == 1.5
          && parsed.entities[0].z == 1.5, "a saved area reads back with its blocks, states, water and entities");
}

void savePromptHits() {
    auto l = lamium::ui::SavePromptLayout::at(480, 270);
    using Part = lamium::ui::SavePromptLayout::Part;
    auto minus = l.hit(l.cellX(2) + 2, l.cornerY(1) + 2), plus = l.hit(l.cellX(0) + l.cellWidth() - 2, l.cornerY(0) + 2);
    check(minus.part == Part::Minus && minus.corner == 1 && minus.axis == 2 && plus.part == Part::Plus && plus.corner == 0
          && plus.axis == 0, "the save prompt's steppers name their corner and axis");
    check(l.hit(l.saveX() + 1, l.buttonY() + 1).part == Part::Save && l.hit(l.cancelX() + 1, l.buttonY() + 1).part == Part::Cancel
          && l.hit(l.left + 20, l.fieldY() + 2).part == Part::Field && l.hit(l.clearX() + 1, l.buttonY() + 1).part == Part::Clear
          && l.clearX() + lamium::ui::SavePromptLayout::buttonWidth < l.saveX() && l.keysY() + 10 <= l.top + l.height(),
          "the save prompt's buttons and name field are where they are drawn, inside the panel");
}

void entityRules() {
    std::vector<EntitySpot> expected{{"minecraft:armor_stand", 1.5, 64, 1.5}, {"minecraft:armor_stand", 3.5, 64, 1.5},
                                     {"minecraft:pig", 5.5, 64, 5.5}};
    std::vector<EntitySpot> actual{{"minecraft:armor_stand", 1.8, 64, 1.4}, {"minecraft:pig", 9, 64, 9},
                                   {"minecraft:cow", 5.5, 64, 5.5}};
    auto placed = matchEntities(expected, actual);
    check(placed == std::vector<bool>{true, false, false}, "an entity counts near its spot, by type, and only once");
    std::vector<EntitySpot> high{{"minecraft:pig", 5.5, 66, 5.5}};
    check(matchEntities(std::span(expected).subspan(2), high) == std::vector<bool>{false}, "an entity two blocks up is not near");
    std::vector<EntitySpot> beside{{"minecraft:armor_stand", 2.5, 64, 1.5}};
    check(matchEntities(std::span(expected).first(1), beside) == std::vector<bool>{false},
          "an entity of the same type on the next block does not fill the spot");
    check(entityNameKey("minecraft:armor_stand") == "entity.armor_stand.name" && entityNameKey("mod:thing") == "entity.mod:thing.name",
          "entity name keys drop the vanilla namespace only");

    std::vector<MaterialLine> lines(3);
    lines[0] = {"minecraft:armor_stand", "Armor Stand", "", 2, 0, true};
    lines[1] = {"minecraft:stone", "Stone", "", 1, 1, false};
    lines[2] = {"minecraft:dirt", "Dirt", "", 5, 0, false};
    sortMaterials(lines);
    check(lines[0].name == "Dirt" && lines[1].name == "Stone" && lines[2].entity, "entity lines come after all block lines");
}

void layerRules() {
    Size placed{4, 6, 3};
    Layers layers;
    check(layerShown(layers, placed, {3, 5, 2}), "all layers show everything");
    layers = {LayerAxis::UpFromBottom, LayerMode::Only, 2};
    check(layerShown(layers, placed, {0, 2, 0}) && !layerShown(layers, placed, {0, 3, 0}), "only one height layer");
    layers = {LayerAxis::DownFromTop, LayerMode::UpTo, 1};
    check(layerShown(layers, placed, {0, 5, 0}) && layerShown(layers, placed, {0, 4, 0}) && !layerShown(layers, placed, {0, 3, 0}),
          "from the top, up to the second layer");
    layers = {LayerAxis::WestFromEast, LayerMode::Only, 0};
    check(layerShown(layers, placed, {3, 0, 0}) && !layerShown(layers, placed, {0, 0, 0}), "side layers count from the chosen side");
    check(layerCount(placed, LayerAxis::SouthFromNorth) == 3 && layerCount(placed, LayerAxis::EastFromWest) == 4,
          "layer counts follow the axis");
    Layers bottom{LayerAxis::UpFromBottom, LayerMode::Only, 1};
    auto top = withAxis(bottom, placed, LayerAxis::DownFromTop);
    check(top.index == 4 && layerShown(top, placed, {0, 1, 0}) && !layerShown(top, placed, {0, 2, 0}),
          "turning to the opposite side keeps the same layer");
    check(withAxis(bottom, placed, LayerAxis::SouthFromNorth).index == 1 && withAxis({LayerAxis::UpFromBottom, LayerMode::Only, 5}, placed,
          LayerAxis::SouthFromNorth).index == 2, "another axis keeps the number, clamped");
}

void verifyRules() {
    PaletteBlock air{"minecraft:air", {}, 0}, stone{"minecraft:stone", {}, 0};
    nbt::Compound east;
    east.set("weirdo_direction", {std::int32_t{0}});
    PaletteBlock stairs{"minecraft:oak_stairs", east, 0};
    auto key = stairs.key();
    WorldBlock worldAir{"minecraft:air", "minecraft:air"}, worldStone{"minecraft:stone", "minecraft:stone"};
    WorldBlock worldStairs{"minecraft:oak_stairs", key}, turnedStairs{"minecraft:oak_stairs", "minecraft:oak_stairs[weirdo_direction=2]"};
    check(classify(nullptr, "", worldStone, true) == CellState::Ignored, "structure void is never checked");
    check(classify(&stone, stone.key(), std::nullopt, true) == CellState::Unknown, "unloaded cells are unknown");
    check(classify(&stone, stone.key(), worldStone, true) == CellState::Correct
          && classify(&stone, stone.key(), worldAir, true) == CellState::Missing
          && classify(&stone, stone.key(), worldStairs, true) == CellState::Wrong,
          "correct, missing and wrong blocks");
    check(classify(&stairs, key, worldStairs, true) == CellState::Correct
          && classify(&stairs, key, turnedStairs, true) == CellState::State, "the same block facing another way is a state mismatch");
    check(classify(&air, air.key(), worldStone, true) == CellState::Extra
          && classify(&air, air.key(), worldStone, false) == CellState::Ignored
          && classify(&air, air.key(), worldAir, true) == CellState::Correct,
          "extra blocks count only when the placement counts them");

    Tally tally;
    tally.add(CellState::Correct, true);
    tally.add(CellState::Correct, false);
    tally.add(CellState::Missing, true);
    tally.add(CellState::Wrong, true);
    tally.add(CellState::State, true);
    tally.add(CellState::Extra, false);
    tally.add(CellState::Unknown, true);
    check(tally.correct == 1 && tally.total() == 5 && tally.mistakes() == 3,
          "correct air is not counted; unknown cells stay in the total but are not placed");

    Materials materials;
    addMaterial(materials, stone, CellState::Correct);
    addMaterial(materials, stone, CellState::Missing);
    addMaterial(materials, air, CellState::Correct);
    addMaterial(materials, stairs, CellState::State);
    check(materials.size() == 2 && materials["minecraft:stone"].needed == 2 && materials["minecraft:stone"].remaining() == 1
          && materials["minecraft:oak_stairs"].placed == 0, "materials count needed and correctly placed blocks, not air");
}

void ghostFaces() {
    using lamium::schematic::faces::Vertex;
    using lamium::schematic::faces::sideOf;
    std::array<Vertex, 4> west{{{5, 2, 3}, {5, 3, 3}, {5, 3, 4}, {5, 2, 4}}};
    std::array<Vertex, 4> top{{{5, 3, 3}, {6, 3, 3}, {6, 3, 4}, {5, 3, 4}}};
    std::array<Vertex, 4> stairStep{{{5, 2.5f, 3}, {6, 2.5f, 3}, {6, 2.5f, 4}, {5, 2.5f, 4}}};
    std::array<Vertex, 4> slab{{{5, 2, 3}, {5, 2.5f, 3}, {5, 2.5f, 4}, {5, 2, 4}}};
    check(sideOf(west, 5, 2, 3) == 0 && sideOf(top, 5, 2, 3) == 3, "quads on a cell side name that side");
    check(sideOf(stairStep, 5, 2, 3) == -1 && sideOf(slab, 5, 2, 3) == 0, "inner quads have no side; part of a side still is that side");
    using lamium::schematic::faces::beyond;
    // Cells 4 and 5 touch at x = 5; the camera at x 3.5 sees cell 5's west face, not cell 4's east face.
    check(beyond(0, 5, 2, 3, 3.5, 2.5, 3.5) && !beyond(1, 4, 2, 3, 3.5, 2.5, 3.5) && beyond(1, 4, 2, 3, 6.2, 2.5, 3.5),
          "of two touching faces only the one facing the camera has it beyond");
}

void menuRules() {
    using namespace lamium::schematic::menu;
    bool sized = true, labeled = true;
    for (auto const& c : categories) {
        sized = sized && !c.items.empty() && c.items.size() <= 8;
        for (auto const& item : c.items) labeled = labeled && item.label.starts_with("schematic.menu.");
    }
    check(sized && labeled && categories[moveCategory].label == "schematic.menu.cat.move",
          "every menu category has one to eight labeled items, Move first");
    Target target = Target::Placement;
    check(choosesTarget(Command::MoveCorner2, target) && target == Target::Corner2 && !choosesTarget(Command::ClearArea, target),
          "only the move commands choose a target");
    check(openAt(false, 3) == -1 && openAt(true, 3) == 3 && openAt(true, -1) == -1 && openAt(true, 99) == -1,
          "the menu opens at the list unless set to reopen where it was closed");

    using lamium::ui::RadialLayout;
    RadialLayout::Sizes sizes{100, 26, 140, 62};
    auto ring = RadialLayout::at(640, 360, 8, sizes, false);
    bool round = true;
    for (int i = 0; i < 8; ++i) round = round && ring.hit(ring.itemX(i), ring.itemY(i)) == i;
    check(round && ring.hit(ring.cx, ring.cy) == -1 && ring.itemY(0) < ring.cy, "the ring starts at the top and each item is hit by its direction");
    auto apart = [&](float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
        return std::abs(ax - bx) >= (aw + bw) / 2 || std::abs(ay - by) >= (ah + bh) / 2;
    };
    bool clear = true;
    for (int count : {3, 5, 7, 8}) {
        auto r = RadialLayout::at(640, 360, count, sizes, false);
        for (int i = 0; i < count; ++i) {
            clear = clear && apart(r.itemX(i), r.itemY(i), sizes.itemWidth, sizes.itemHeight, r.cx, r.cy, sizes.centerWidth, sizes.centerHeight);
            int j = (i + 1) % count;
            clear = clear && apart(r.itemX(i), r.itemY(i), sizes.itemWidth, sizes.itemHeight, r.itemX(j), r.itemY(j), sizes.itemWidth, sizes.itemHeight);
        }
    }
    check(clear, "no item overlaps its neighbor or the center, at any count");
    check(RadialLayout::spread(0) == 0 && RadialLayout::spread(RadialLayout::duration) == 1 && RadialLayout::spread(1) == 1
          && RadialLayout::spread(RadialLayout::duration / 2) > .5f && ring.itemX(2, 1) == ring.itemX(2) && ring.itemX(2, 0) < ring.itemX(2),
          "opening spreads items out from the center, fast first, and settles on the ring");
    auto small = RadialLayout::at(640, 360, 8, sizes, true), fewer = RadialLayout::at(640, 360, 3, sizes, true);
    check(small.cx > ring.cx && small.cy > ring.cy && small.cx + small.rx + sizes.itemWidth / 2 <= 640
          && fewer.cx == small.cx && fewer.cy == small.cy, "the small menu sits in the lower right, inside the screen, and stays put");
    check(repeatable(Stepper::Layer) && !repeatable(Stepper::Target), "the adjust key does not repeat choosing the target");
}

void drawKeys() {
    SavedPlacement a;
    a.name = "hut";
    a.file = "hut.mcstructure";
    a.placement.origin = {1, 2, 3};
    auto renamed = a, hidden = a, moved = a, layered = a;
    renamed.name = "other";
    hidden.visible = false;
    moved.placement.origin.x = 2;
    layered.layers.index = 4;
    check(drawKey(a) == drawKey(renamed) && drawKey(a) == drawKey(hidden), "renaming or hiding keeps a placement's ghosts");
    check(drawKey(a) != drawKey(moved) && drawKey(a) != drawKey(layered), "moving or changing layers rebuilds a placement's ghosts");
}

void placementDocuments() {
    PlacementSet set;
    SavedPlacement hut;
    hut.name = "hut";
    hut.file = "small/hut.mcstructure";
    hut.dimension = 1;
    hut.placement = {{124, 64, -38}, 3, Mirror::Z};
    hut.layers = {LayerAxis::WestFromEast, LayerMode::UpTo, 2};
    hut.visible = false;
    hut.countExtras = false;
    set.placements.push_back(hut);
    set.selected = 0;
    auto back = decodePlacements(encodePlacements(set));
    auto const& p = back.placements.at(0);
    check(back.selected == 0 && p.name == "hut" && p.file == "small/hut.mcstructure" && p.dimension == 1
          && p.placement.origin == Point{124, 64, -38} && p.placement.rotation == 3 && p.placement.mirror == Mirror::Z
          && p.layers == hut.layers && !p.visible && !p.countExtras && p.entities,
          "placements round-trip");
    auto tolerant = decodePlacements(R"({"version":1,"placements":[{"file":"a.mcstructure"},{"file":"../x.mcstructure"},
        {"file":"C:/x.mcstructure"},{"name":"no file"}],"selected":7})");
    check(tolerant.placements.size() == 1 && tolerant.placements[0].name == "a.mcstructure" && tolerant.placements[0].visible
          && tolerant.placements[0].countExtras && tolerant.selected == -1,
          "missing fields take defaults, unsafe paths are dropped, a stale selection clears");
    bool rejected = false;
    try { decodePlacements(R"({"version":2})"); } catch (std::exception const&) { rejected = true; }
    check(rejected, "other document versions are rejected");
    check(safeSchematicPath("farms/iron.mcstructure") && !safeSchematicPath("") && !safeSchematicPath("/abs")
          && !safeSchematicPath("a//b") && !safeSchematicPath("a/../b") && !safeSchematicPath(R"(a\b)"),
          "schematic paths stay inside the schematics folder");
}

void verificationOrder() {
    std::vector<Mismatch> list{{CellState::Missing, {1, 0, 0}}, {CellState::Wrong, {9, 0, 0}}, {CellState::State, {2, 0, 0}},
                               {CellState::Missing, {0, 0, 0}}};
    sortMismatches(list, 0, 0, 0);
    check(list[0].state == CellState::State && list[1].state == CellState::Wrong && list[2].position == Point{0, 0, 0}
          && list[3].position == Point{1, 0, 0}, "mistakes come before missing blocks, each nearest first");
    std::vector<MaterialLine> lines{{"a", "Stone", "", 10, 10}, {"b", "Planks", "", 5, 1}, {"c", "Glass", "", 9, 1}};
    sortMaterials(lines);
    check(lines[0].name == "Glass" && lines[1].name == "Planks" && lines[2].name == "Stone" && lines[2].remaining() == 0,
          "materials list the most remaining first and finished lines last");
    check(itemsPerBlock("minecraft:oak_double_slab", false) == 2 && itemsPerBlock("minecraft:wooden_door", true) == 0
          && itemsPerBlock("minecraft:stone", false) == 1, "double slabs need two items; second halves none");
}
}
void schematicTests() {
    verificationOrder();
    placementDocuments();
    drawKeys();
    menuRules();
    ghostFaces();
    placementTransforms();
    entityRules();
    previewRules();
    saveRules();
    savePromptHits();
    layerRules();
    verifyRules();
    nbtBasics();
    structureRoundTrip();
    structureRejects();
    sampleFiles();
}

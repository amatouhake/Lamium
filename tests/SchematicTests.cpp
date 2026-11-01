#include "features/schematic/RayCells.h"
#include <cmath>
#include "features/schematic/LiquidShape.h"
#include "features/schematic/Structure.h"
#include "features/schematic/Verify.h"
#include "features/schematic/CheckRules.h"
#include "features/schematic/PlacementStore.h"
#include "features/schematic/Verification.h"
#include "features/schematic/SaveArea.h"
#include "ui/SavePromptLayout.h"
#include "ui/RadialLayout.h"
#include "ui/Translations.h"
#include "ui/SchematicFiles.h"
#include "features/schematic/MaterialAmount.h"
#include "features/schematic/MenuModel.h"
#include "features/schematic/MoveDirection.h"
#include "features/schematic/MoveTarget.h"
#include "features/schematic/TargetRay.h"
#include "features/schematic/PickRay.h"
#include "features/schematic/UnreadableNotice.h"
#include "features/schematic/RestPose.h"
#include "features/schematic/PreviewView.h"
#include "features/schematic/GhostFaces.h"
#include "features/schematic/PlacementOverlap.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <span>
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

nbt::List& layersOf(nbt::Root& root) {
    auto& body = *const_cast<nbt::Tag*>(root.compound.find("structure"))->as<nbt::Compound>();
    return *const_cast<nbt::Tag*>(body.find("block_indices"))->as<nbt::List>();
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
    auto badLiquid = sample();
    badLiquid.liquids[0] = 7;
    std::string reason;
    try { parseStructure(span(writeStructure(badLiquid))); } catch (std::runtime_error const& error) { reason = error.what(); }
    check(reason.find("second block layer") != std::string::npos, "a second layer naming a missing palette entry is rejected by name");
    nbt::Root shortLayer = nbt::read(span(writeStructure(sample())));
    auto& layers = *layersOf(shortLayer).items[1].as<std::vector<std::int32_t>>();
    layers.pop_back();
    reason.clear();
    try { parseStructure(span(nbt::write(shortLayer))); } catch (std::runtime_error const& error) { reason = error.what(); }
    check(reason.find("second block layer has") != std::string::npos, "a second layer shorter than the volume is rejected by name");
}

// The shape a save writes (SCHEMATIC.md A): version 2; the second layer only
// when some cell holds a liquid, -1 elsewhere.
void structureWrittenShape() {
    auto wet = sample();
    nbt::Root root = nbt::read(span(writeStructure(wet)));
    std::int64_t version = 0;
    check(root.compound.find("format_version")->integer(version) && version == 2, "saves say format_version 2");
    check(layersOf(root).element == nbt::Type::IntArray && layersOf(root).items.size() == 2,
          "a structure with a waterlogged cell writes two int-array layers");
    auto const& second = *layersOf(root).items[1].as<std::vector<std::int32_t>>();
    check(second[wet.cell(1, 2, 3)] == 4 && std::count(second.begin(), second.end(), voidCell) == static_cast<long long>(wet.cells()) - 1,
          "the second layer is -1 except the waterlogged cell");

    auto dry = sample();
    dry.liquids.clear();
    root = nbt::read(span(writeStructure(dry)));
    check(layersOf(root).items.size() == 1, "a structure without liquids writes one layer");
    dry.liquids.assign(dry.cells(), voidCell);
    root = nbt::read(span(writeStructure(dry)));
    check(layersOf(root).items.size() == 1, "an all-void second layer is not written");

    auto old = sample();
    old.formatVersion = 1;
    root = nbt::read(span(writeStructure(old)));
    check(root.compound.find("format_version")->integer(version) && version == 2, "a version 1 file read in is written back as 2");

    // Vanilla exports hold water over an air cell and palette entries no cell uses.
    auto odd = sample();
    odd.palette.push_back({"minecraft:jungle_door", {}, 18168865});
    odd.liquids[odd.cell(0, 0, 1)] = 4;
    auto parsed = parseStructure(span(writeStructure(odd)));
    check(parsed.palette.size() == 6 && parsed.blocks[odd.cell(0, 0, 1)] == 0 && parsed.liquids[odd.cell(0, 0, 1)] == 4,
          "water in an air cell and an unused palette entry read as a valid file");
}

// Optional: LAMIUM_SAMPLE_STRUCTURES names real exports to read and write
// back: a folder, or files separated by ';' (e.g. the vanilla
// submerged.mcstructure and water_and_lava.mcstructure). Not in the repository.
void sampleFiles() {
    char* value = nullptr;
    size_t length = 0;
    if (_dupenv_s(&value, &length, "LAMIUM_SAMPLE_STRUCTURES") || !value) return;
    std::unique_ptr<char, decltype(&std::free)> owned(value, &std::free);
    std::vector<std::filesystem::path> files;
    std::string list = value;
    for (size_t start = 0; start <= list.size();) {
        auto end = std::min(list.find(';', start), list.size());
        std::filesystem::path path = list.substr(start, end - start);
        start = end + 1;
        if (path.empty()) continue;
        if (std::filesystem::is_directory(path)) {
            for (auto const& entry : std::filesystem::directory_iterator(path))
                if (entry.path().extension() == ".mcstructure") files.push_back(entry.path());
        } else files.push_back(path);
    }
    for (auto const& path : files) {
        std::ifstream file(path, std::ios::binary);
        check(file.good(), "a named sample file opens");
        std::string bytes{std::istreambuf_iterator<char>(file), {}};
        auto parsed = parseStructure(span(bytes));
        check(parsed.blocks.size() == parsed.cells() && !parsed.palette.empty(), "a real export parses");
        auto written = writeStructure(parsed);
        auto again = parseStructure(span(written));
        check(again.blocks == parsed.blocks && again.liquids == parsed.liquids && again.palette.size() == parsed.palette.size()
              && again.blockEntities.size() == parsed.blockEntities.size() && again.entities.size() == parsed.entities.size(),
              "a real export survives writing back, both layers included");
        nbt::Root root = nbt::read(span(written));
        check(layersOf(root).items.size() == (parsed.liquids.empty() ? 1u : 2u), "a written export has a second layer only with liquids");
        std::int64_t version = 0;
        if (parsed.formatVersion == writtenFormatVersion) {
            nbt::Root original = nbt::read(span(bytes));
            check(layersOf(original).items.size() == layersOf(root).items.size(),
                  "a version 2 export is written back with as many layers as it had");
        }
        check(root.compound.find("format_version")->integer(version) && version == writtenFormatVersion, "a written export says version 2");
    }
}

// Optional: LAMIUM_MIGRATE_PLACEMENTS names a world's schematics.json and
// LAMIUM_MIGRATE_SCHEMATICS the folder its file paths resolve in. Compares the
// old six-direction view and the new structure-local view cell by cell, and
// checks that the dual-written old fields survive back through fromLegacy.
void migrationFiles() {
    char* docValue = nullptr;
    char* folderValue = nullptr;
    size_t length = 0;
    if (_dupenv_s(&docValue, &length, "LAMIUM_MIGRATE_PLACEMENTS") || !docValue) return;
    std::unique_ptr<char, decltype(&std::free)> docOwned(docValue, &std::free);
    if (_dupenv_s(&folderValue, &length, "LAMIUM_MIGRATE_SCHEMATICS") || !folderValue) return;
    std::unique_ptr<char, decltype(&std::free)> folderOwned(folderValue, &std::free);
    std::ifstream docStream(std::filesystem::path(docValue), std::ios::binary);
    check(docStream.good(), "a migration placement document opens");
    if (!docStream) return;
    std::string doc{std::istreambuf_iterator<char>(docStream), {}};
    PlacementSet set;
    try { set = decodePlacements(doc); }
    catch (std::exception const&) { check(false, "a migration placement document decodes"); return; }
    int totalMismatch = 0, totalMissing = 0, totalDual = 0;
    std::cout << "migration check: " << set.placements.size() << " placements\n";
    for (auto const& p : set.placements) {
        auto path = std::filesystem::path(folderValue) / std::filesystem::path(p.file);
        std::ifstream file(path, std::ios::binary);
        if (!file) { ++totalMissing; std::cout << "  missing: " << p.name << " (" << p.file << ")\n"; continue; }
        std::string bytes{std::istreambuf_iterator<char>(file), {}};
        Structure structure;
        try { structure = parseStructure(span(bytes)); }
        catch (std::exception const&) { ++totalMissing; std::cout << "  unreadable: " << p.name << "\n"; continue; }
        Layers newView = p.layers.legacy ? fromLegacy(p.layers, structure.size, p.placement) : p.layers;
        Layers oldView;
        if (p.layers.legacy) {
            oldView = p.layers;
        } else {
            oldView.legacy = true;
            oldView.legacyAxis = p.layers.legacyAxis;
            oldView.legacyMode = p.layers.legacyMode;
            oldView.legacyIndex = p.layers.legacyIndex;
        }
        int visibleOld = 0, visibleNew = 0, mismatch = 0;
        for (int x = 0; x < structure.size.x; ++x)
            for (int y = 0; y < structure.size.y; ++y)
                for (int z = 0; z < structure.size.z; ++z) {
                    Point placed = localToPlaced(structure.size, p.placement, {x, y, z});
                    bool oldShown = layerShown(oldView, structure.size, p.placement, placed);
                    bool newShown = layerShown(newView, structure.size, p.placement, placed);
                    visibleOld += oldShown;
                    visibleNew += newShown;
                    mismatch += oldShown != newShown;
                }
        Layers copy = newView;
        dualWrite(copy, structure.size, p.placement);
        Layers back = fromLegacy(copy, structure.size, p.placement);
        bool dual = back.axis == newView.axis && back.mode == newView.mode && back.keep == newView.keep && back.cut == newView.cut;
        totalMismatch += mismatch;
        totalDual += !dual;
        std::cout << "  " << p.name << " [" << (p.layers.legacy ? "old" : "new") << "] "
                  << structure.size.x << "x" << structure.size.y << "x" << structure.size.z << " visible "
                  << visibleOld << " -> " << visibleNew << " mismatches " << mismatch << " dual " << (dual ? "ok" : "FAIL") << "\n";
    }
    std::cout << "migration check: " << totalMismatch << " mismatched cells, " << totalDual
              << " bad dual writes, " << totalMissing << " missing structures\n";
    check(totalMismatch == 0 && totalDual == 0 && totalMissing == 0,
          "actual placements keep their shown cells and dual-write");
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
    // L-179: a face stored in block entity data turns like the cell it points to.
    bool faces = true;
    constexpr int dx[6] = {0, 0, 0, 0, -1, 1}, dy[6] = {-1, 1, 0, 0, 0, 0}, dz[6] = {0, 0, -1, 1, 0, 0};
    Size cube{3, 3, 3};
    for (int rotation = 0; rotation < 4; ++rotation)
        for (auto mirror : {Mirror::None, Mirror::X, Mirror::Z})
            for (int face = 0; face < 6; ++face) {
                Placement p{{10, 0, -4}, rotation, mirror};
                auto from = toWorld(cube, p, {1, 1, 1});
                auto to = toWorld(cube, p, {1 + dx[face], 1 + dy[face], 1 + dz[face]});
                int turned = toWorldFace(face, p);
                faces = faces && to.x - from.x == dx[turned] && to.y - from.y == dy[turned] && to.z - from.z == dz[turned];
            }
    check(faces && toWorldFace(2, {{0, 0, 0}, 1, Mirror::None}) == 5 && toWorldFace(7, {{0, 0, 0}, 1, Mirror::None}) == 7,
          "a stored face follows the placement's mirror and turns; unknown values stay");

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
                // Mirrored entities face the other way, as the structure block places them.
                double back = mirror == Mirror::None ? 1 : -1;
                facings = facings && std::abs(b.x - a.x + back * std::sin(turned)) < 1e-4 && std::abs(b.z - a.z - back * std::cos(turned)) < 1e-4;
            }
    check(facings, "an entity keeps its facing relative to the structure after any turn, and is turned half round when mirrored");

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

void layerModeMemory() {
    Layers layers;
    layers.cut = 2;
    check(leaveAll(layers) && layers.mode == LayerMode::Only && layers.cut == 2,
          "leaving All defaults to Only and keeps the cut");
    layers.mode = LayerMode::UpTo;
    auto upTo = layers;
    check(!leaveAll(layers) && layers == upTo, "leaving a non-All mode changes nothing");
    layers.mode = LayerMode::All;
    check(leaveAll(layers) && layers.mode == LayerMode::Only,
          "a mode only viewed does not replace the default memory");

    ++layers.cut;
    noteLayerEdit(layers);
    for (int i = 0; i < 2; ++i)
        layers.mode = static_cast<LayerMode>((static_cast<int>(layers.mode) + 1) % 3);
    check(layers.mode == LayerMode::All && layers.lastShown == LayerMode::Only
              && leaveAll(layers) && layers.mode == LayerMode::Only && layers.cut == 3,
          "an Only cut edit survives stepping forward through UpTo to All");
    auto only = layers;
    check(!leaveAll(layers) && layers == only, "leaving Only changes nothing");

    for (bool editKeep : {false, true}) {
        layers = {};
        layers.mode = LayerMode::UpTo;
        if (editKeep) layers.keep = LayerKeep::High;
        else layers.cut = 3;
        noteLayerEdit(layers);
        auto edited = layers;
        layers.mode = LayerMode::Only;
        layers.mode = LayerMode::All;
        check(layers.lastShown == LayerMode::UpTo && leaveAll(layers) && layers == edited,
              "an UpTo Keep or cut edit survives viewing Only before All");
        layers.mode = LayerMode::All;
        leaveAll(layers);
        noteLayerEdit(layers);
        check(layers == edited, "a cut operation leaving All notes the restored mode without changing it");
    }

    layers.mode = LayerMode::Only;
    layers.axis = LayerAxis::X;
    noteLayerEdit(layers);
    layers.mode = LayerMode::All;
    check(layers.lastShown == LayerMode::Only && leaveAll(layers) && layers.mode == LayerMode::Only,
          "an Axis edit replaces the previous UpTo memory with Only");

    layers.mode = LayerMode::Only;
    layers.keep = LayerKeep::Low;
    layers.lastShown = LayerMode::UpTo;
    noteLayerEdit(layers);
    check(layers.lastShown == LayerMode::Only, "a Keep edit counts even under Only");

    for (auto remembered : {LayerMode::Only, LayerMode::UpTo}) {
        layers = {};
        layers.lastShown = remembered;
        layers.cut = 4;
        auto edited = layers;
        noteLayerEdit(layers);
        check(layers == edited, "a cut edit under All does not change the mode memory");
        layers.keep = LayerKeep::High;
        edited = layers;
        noteLayerEdit(layers);
        check(layers == edited, "a Keep edit under All does not change the mode memory");
        layers.axis = LayerAxis::Z;
        edited = layers;
        noteLayerEdit(layers);
        check(layers == edited, "an Axis edit under All does not change the mode memory");
        check(leaveAll(layers) && layers.mode == remembered,
              "All edits preserve the mode restored by leaveAll");
    }
    Layers old;
    old.legacy = true;
    old.legacyMode = LayerMode::UpTo;
    old.lastShown = LayerMode::UpTo;
    check(fromLegacy(old, {4, 6, 3}, {}).lastShown == LayerMode::Only,
          "legacy conversion starts with the default mode memory");
}

void layerRules() {
    Size structure{4, 6, 3};
    Placement identity{};
    Layers layers;
    check(layerShown(layers, structure, identity, {3, 5, 2}), "all layers show everything");
    layers = {LayerAxis::Y, LayerMode::Only, LayerKeep::Low, 2};
    check(layerShown(layers, structure, identity, {0, 2, 0}) && !layerShown(layers, structure, identity, {0, 3, 0}),
          "only one Y layer at the cut");
    layers = {LayerAxis::Y, LayerMode::UpTo, LayerKeep::High, 4};
    check(layerShown(layers, structure, identity, {0, 5, 0}) && layerShown(layers, structure, identity, {0, 4, 0})
              && !layerShown(layers, structure, identity, {0, 3, 0}),
          "up to with the high side keeps the top");
    layers = {LayerAxis::X, LayerMode::Only, LayerKeep::Low, 3};
    check(layerShown(layers, structure, identity, {3, 0, 0}) && !layerShown(layers, structure, identity, {0, 0, 0}),
          "the X cut is structure-local");
    layers = {LayerAxis::Z, LayerMode::UpTo, LayerKeep::Low, 1};
    check(layerShown(layers, structure, identity, {0, 0, 0}) && layerShown(layers, structure, identity, {0, 0, 1})
              && !layerShown(layers, structure, identity, {0, 0, 2}),
          "Z up to the low side");
    check(layerCount(structure, LayerAxis::Z) == 3 && layerCount(structure, LayerAxis::X) == 4, "layer counts follow the structure");
    check(withAxis({LayerAxis::X, LayerMode::Only, LayerKeep::Low, 3}, structure, LayerAxis::Z).cut == 2,
          "an axis change keeps the cut and clamps it to the new extent");
    check(cutAlong(structure, identity, LayerAxis::Y, {2, 24, 1}) == 5
              && cutAlong(structure, identity, LayerAxis::X, {99, 20, 30}) == 3,
          "the cut comes from the world position and clamps in structure space");

    // The cut is attached to the build: the same structure cells are shown
    // for every rotation and mirror.
    bool attached = true;
    Layers cut{LayerAxis::X, LayerMode::UpTo, LayerKeep::Low, 2};
    for (int r = 0; r < 4; ++r)
        for (int m = 0; m < 3; ++m) {
            Placement p{{10, 20, 30}, r, static_cast<Mirror>(m)};
            for (int x = 0; x < structure.x; ++x)
                for (int y = 0; y < structure.y; ++y)
                    for (int z = 0; z < structure.z; ++z) {
                        bool expected = x <= 2;
                        attached = attached && layerShown(cut, structure, p, localToPlaced(structure, p, {x, y, z})) == expected;
                    }
        }
    check(attached, "the cut follows the placement through rotation and mirror");
    bool roundTrip = true;
    for (int r = 0; r < 4; ++r)
        for (int m = 0; m < 3; ++m) {
            Placement p{{10, 20, 30}, r, static_cast<Mirror>(m)};
            for (int x = 0; x < structure.x; ++x)
                for (int y = 0; y < structure.y; ++y)
                    for (int z = 0; z < structure.z; ++z) {
                        Point local{x, y, z};
                        roundTrip = roundTrip && placedToLocal(structure, p, localToPlaced(structure, p, local)) == local;
                    }
        }
    check(roundTrip, "placed and structure-local offsets round-trip under every rotation and mirror");

    auto oldShown = [&](Layers const& old, Size placed, Point offset) {
        int count = layerAxisOf(old.legacyAxis) == LayerAxis::Y ? placed.y
            : layerAxisOf(old.legacyAxis) == LayerAxis::X ? placed.x : placed.z;
        int layer = legacyHighSide(old.legacyAxis) ? count - 1 - axisValue(offset, layerAxisOf(old.legacyAxis))
                                                   : axisValue(offset, layerAxisOf(old.legacyAxis));
        return old.legacyMode == LayerMode::All
            || (old.legacyMode == LayerMode::Only ? layer == old.legacyIndex : layer <= old.legacyIndex);
    };
    bool same = true;
    for (int r = 0; r < 4; ++r)
        for (int m = 0; m < 3; ++m) {
            Placement p{{10, 20, 30}, r, static_cast<Mirror>(m)};
            Size placed = placedSize(structure, r);
            for (int a = 0; a < 6; ++a) {
                auto axis = static_cast<LegacyLayerAxis>(a);
                int count = layerAxisOf(axis) == LayerAxis::Y ? placed.y
                    : layerAxisOf(axis) == LayerAxis::X ? placed.x : placed.z;
                for (int mode = 0; mode < 3; ++mode)
                    for (int index = 0; index < count; ++index) {
                        Layers old;
                        old.legacy = true;
                        old.legacyAxis = axis;
                        old.legacyMode = static_cast<LayerMode>(mode);
                        old.legacyIndex = index;
                        Layers converted = fromLegacy(old, structure, p);
                        for (int x = 0; x < structure.x; ++x)
                            for (int y = 0; y < structure.y; ++y)
                                for (int z = 0; z < structure.z; ++z) {
                                    Point placedOffset = localToPlaced(structure, p, {x, y, z});
                                    same = same && layerShown(converted, structure, p, placedOffset) == oldShown(old, placed, placedOffset);
                                }
                    }
            }
        }
    check(same, "every old direction and mode converts under every rotation and mirror");
    bool slabSame = true;
    for (int r = 0; r < 4; ++r)
        for (int m = 0; m < 3; ++m) {
            Placement p{{10, 20, 30}, r, static_cast<Mirror>(m)};
            std::vector<Layers> views;
            for (int a = 0; a < 3; ++a)
                for (int mode = 0; mode < 3; ++mode)
                    for (int keep = 0; keep < 2; ++keep)
                        for (int cut = 0; cut < 4; ++cut)
                            views.push_back({static_cast<LayerAxis>(a), static_cast<LayerMode>(mode), static_cast<LayerKeep>(keep), cut});
            for (int a = 0; a < 6; ++a)
                for (int mode = 0; mode < 3; ++mode)
                    for (int index = 0; index < 3; ++index) {
                        Layers old;
                        old.legacy = true;
                        old.legacyAxis = static_cast<LegacyLayerAxis>(a);
                        old.legacyMode = static_cast<LayerMode>(mode);
                        old.legacyIndex = index;
                        views.push_back(old);
                    }
            for (auto const& view : views) {
                auto slab = placedSlab(view, structure, p);
                for (int x = 0; x < structure.x; ++x)
                    for (int y = 0; y < structure.y; ++y)
                        for (int z = 0; z < structure.z; ++z) {
                            Point placedOffset = localToPlaced(structure, p, {x, y, z});
                            slabSame = slabSame && slab.shown(placedOffset) == layerShown(view, structure, p, placedOffset);
                        }
            }
        }
    check(slabSame, "the placed slab shows exactly the cells layerShown shows, in both forms");
    Placement rotated{{1, 2, 3}, 1, Mirror::X};
    Layers view{LayerAxis::X, LayerMode::UpTo, LayerKeep::High, 1};
    dualWrite(view, structure, rotated);
    Layers old;
    old.legacy = true;
    old.legacyAxis = view.legacyAxis;
    old.legacyMode = view.legacyMode;
    old.legacyIndex = view.legacyIndex;
    Layers back = fromLegacy(old, structure, rotated);
    check(back.axis == view.axis && back.mode == view.mode && back.keep == view.keep && back.cut == view.cut,
          "a structure-local view survives the old fields and back under a rotation");
    Placement turned{{0, 0, 0}, 0, Mirror::None};
    Layers legacyCut;
    legacyCut.legacy = true;
    legacyCut.legacyAxis = LegacyLayerAxis::EastFromWest;
    legacyCut.legacyMode = LayerMode::Only;
    legacyCut.legacyIndex = 1;
    std::vector<bool> before;
    for (int x = 0; x < structure.x; ++x)
        for (int y = 0; y < structure.y; ++y)
            for (int z = 0; z < structure.z; ++z)
                before.push_back(layerShown(legacyCut, structure, turned, localToPlaced(structure, turned, {x, y, z})));
    turnPlacement(turned, legacyCut, &structure, [](Placement& t) { t.rotation = 1; t.mirror = Mirror::Z; });
    bool kept = !legacyCut.legacy;
    size_t i = 0;
    for (int x = 0; x < structure.x; ++x)
        for (int y = 0; y < structure.y; ++y)
            for (int z = 0; z < structure.z; ++z)
                kept = kept && layerShown(legacyCut, structure, turned, localToPlaced(structure, turned, {x, y, z})) == before[i++];
    check(kept, "turning a legacy view with the structure known converts it and keeps the build cells");
    Layers unknown;
    unknown.legacy = true;
    Placement plain{};
    turnPlacement(plain, unknown, nullptr, [](Placement& t) { t.rotation = 2; });
    check(unknown.legacy && plain.rotation == 2, "without the structure a legacy view turns unconverted");
    Layers allView{LayerAxis::X, LayerMode::All, LayerKeep::High, 2}, onlyView{LayerAxis::X, LayerMode::Only, LayerKeep::Low, 2};
    check(!turnNeedsStructure(allView) && turnNeedsStructure(onlyView) && !turnNeedsStructure(unknown)
              && showsAllLayers(allView) && showsAllLayers(unknown) && !showsAllLayers(onlyView),
          "only a set cut plane needs the structure to turn; All in either form does not");
    Layers far{LayerAxis::X, LayerMode::Only, LayerKeep::Low, 99};
    check(resolvedLayers(far, structure, plain).cut == structure.x - 1
              && resolvedLayers(legacyCut, structure, turned) == legacyCut,
          "a resolved view clamps the cut and leaves a converted view alone");
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
    using lamium::schematic::faces::sideArea;
    using lamium::schematic::faces::coveredBy;
    // An upside-down stair's back: the upper and the lower box each half the side.
    std::array<Vertex, 4> upperBack{{{5, 2.5f, 3}, {5, 3, 3}, {5, 3, 4}, {5, 2.5f, 4}}};
    check(coveredBy(sideArea(slab, 0) + sideArea(upperBack, 0)) && !coveredBy(sideArea(slab, 0))
          && coveredBy(sideArea(top, 3)) && std::abs(sideArea(slab, 0) - .5f) < 1e-5f,
          "quads on one side add up to the part of it they cover");
    using lamium::schematic::faces::sameQuad;
    std::array<Vertex, 4> westBack{{{5, 2, 4}, {5, 3, 4}, {5, 3, 3}, {5, 2, 3}}};
    check(sameQuad(west, westBack) && sameQuad(west, west) && !sameQuad(west, slab) && !sameQuad(west, top),
          "a face emitted again with the other winding is the same quad; other faces are not");
    using lamium::schematic::faces::dropFace;
    check(dropFace(true, false, false, true, true) && dropFace(false, true, true, false, true),
          "a real opaque block, or far from the camera an opaque ghost, hides the face against it");
    check(!dropFace(false, true, true, true, true) && dropFace(false, true, true, true, false),
          "next to the camera a shared plane keeps only the face the camera sees");
    check(!dropFace(false, false, true, false, false) && dropFace(false, false, true, true, false)
          && !dropFace(false, false, false, true, false), "a see-through ghost's shared plane is paired only next to the camera");
    using lamium::schematic::faces::facingToward;
    check(facingToward(2) == 1 && facingToward(3) == 0 && facingToward(0) == 5 && facingToward(1) == 4 && facingToward(4) == 3
              && facingToward(5) == 2,
          "the neighbor below shows its top face, the one to the west its east face, and so on");
    using lamium::schematic::faces::dropBlendedFace;
    check(dropBlendedFace(true, false, false, 0) && dropBlendedFace(true, true, true, 1) && !dropBlendedFace(false, false, false, 0),
          "a see-through ghost's face goes against a solid ghost's face and between two of its kind");
    bool onePerPlane = true;
    for (int side = 0; side < 6; ++side)
        onePerPlane = onePerPlane && dropBlendedFace(true, true, false, side) != dropBlendedFace(true, true, false, side ^ 1);
    check(onePerPlane, "two different see-through ghosts keep exactly one face between them");
    using lamium::schematic::faces::facesAway;
    Vertex up{0, 1, 0}, center{0, 0, 0};
    check(facesAway(up, center, Vertex{0, -1, 0}) && !facesAway(up, center, Vertex{0, 1, 0})
          && !facesAway(Vertex{0, 0, 0}, center, Vertex{0, -1, 0}), "a face is seen from behind only past its plane, and only with a normal");
    using lamium::schematic::faces::farToNear;
    std::array<Vertex, 12> three{{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},     // near
                                  {0, 0, 9}, {1, 0, 9}, {1, 1, 9}, {0, 1, 9},     // far
                                  {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}};  // as near as the first
    auto order = farToNear(std::span<Vertex const>(three), Vertex{.5f, .5f, -1});
    check(order == std::vector<std::uint32_t>{1, 0, 2}, "blended quads are drawn far to near, equally far ones in their order");
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
    check(choosesTarget(Command::MoveArea, target) && target == Target::Area && !choosesTarget(Command::ClearArea, target),
          "only the move commands choose a target");
    // L-238: every schematic key has a menu item and every menu item a key,
    // except accelerators of one path (opening the menu or screen, Repeat;
    // choosing a target to move, which keys do by looking or cycling).
    std::set<lamium::input::Action> inMenu;
    bool keyed = true, sizedWell = true;
    for (auto const& c : categories) {
        sizedWell = sizedWell && c.items.size() >= 5 && c.items.size() <= 6;
        for (auto const& item : c.items) {
            for (auto key : {item.key, item.keyBack}) if (key != noKey) keyed = keyed && inMenu.insert(key).second;
            Target ignored{};
            keyed = keyed && (item.key != noKey || (!item.stepper() && choosesTarget(std::get<Command>(item.what), ignored)));
        }
    }
    bool covered = true;
    for (size_t i = 0; i < lamium::input::actions.size(); ++i) {
        auto action = static_cast<lamium::input::Action>(i);
        bool schematicKey = lamium::input::actions[i].feature.starts_with("schematic");
        covered = covered && (!schematicKey || inMenu.contains(action) || keyOnlyAccelerator(action));
    }
    check(keyed && covered, "every schematic key has a menu item and every menu item a key, accelerators aside");
    check(categories.size() == 6 && sizedWell, "six menu categories of five or six items");
    check(openAt(false, 3) == -1 && openAt(true, 3) == 3 && openAt(true, -1) == -1 && openAt(true, 99) == -1,
          "the menu opens at the list unless set to reopen where it was closed");
    bool switchThere = false;
    for (auto const& item : categories[showCategory].items)
        switchThere = switchThere || (!item.stepper() && std::get<Command>(item.what) == Command::ToggleFeature);
    check(switchThere && worksWhileOff({"", Command::ToggleFeature}) && worksWhileOff({"", Command::CheckTab})
              && !worksWhileOff({"", Command::Corner1Here}) && !worksWhileOff({"", Command::SaveArea})
              && !worksWhileOff({"", Stepper::Rotate}),
          "with Schematics off the menu opens at its switch, and only the switch and screens run");
    using S = Stepper;
    check(repeatable(S::ForwardBack) && repeatable(S::Rotate) && repeatable(S::Layer)
              && !repeatable(S::LayerAxis) && !repeatable(S::LayerMode) && !repeatable(S::Target) && !repeatable(S::Placement),
          "the adjust key repeats movement, rotation/mirror and the layer index only");
    check(repeatableAll(S::LayerAxis) && repeatableAll(S::LayerMode) && !repeatableAll(S::Target)
              && !repeatableAll(S::Placement),
          "the option adds the layer axis and mode but not target or placement selection");

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

    // Long names: every plate takes the widest label and the ring grows with
    // it, unclamped. Estimate the text like the game font does (ASCII about
    // 6 units, CJK about 9) and size the menu as renderSchematicMenu does; the
    // ring must fit a small GUI screen (1920x1012 at GUI scale 4) in every
    // language.
    auto estimate = [](std::string_view text) {
        float w = 0;
        for (size_t i = 0; i < text.size();) {
            auto c = static_cast<unsigned char>(text[i]);
            int length = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
            w += c >= 0x80 ? 9.f : c == ' ' ? 4.f : std::string_view("il.,:;!|'").find(static_cast<char>(c)) != std::string_view::npos ? 3.f : 6.f;
            i += static_cast<size_t>(length);
        }
        return w;
    };
    using lamium::ui::translations::find;
    bool fits = true;
    std::string tooWide;
    for (auto locale : {"en_US", "ja_JP", "zh_CN"}) {
        float widest = 72, centerWide = 100;
        for (auto const& c : categories) {
            widest = std::max(widest, estimate(find(c.label, locale)) + 16);
            centerWide = std::max(centerWide, estimate(find(c.label, locale)) + 16);
            for (auto const& item : c.items) widest = std::max(widest, estimate(find(item.label, locale)) + 16);
        }
        for (auto key : {"schematic.menu.hintList", "schematic.menu.hintClose", "schematic.menu.hintStep",
                 "schematic.menu.hintStepClick", "schematic.menu.hintRun", "schematic.menu.hintBack"})
            centerWide = std::max(centerWide, estimate(find(key, locale)) + 16);
        // The hovered item's keys are drawn above the ring, not in the center.
        RadialLayout::Sizes sized{widest, RadialLayout::itemHeight(1), centerWide, 16 + 3 + 4 * 11 + 3};
        for (bool corner : {false, true}) {
            auto r = RadialLayout::at(480, 253, 6, sized, corner);
            bool inside = r.cx - r.rx - widest / 2 >= 0 && r.cx + r.rx + widest / 2 <= 480
                && r.cy - r.ry - sized.itemHeight / 2 >= 0 && r.cy + r.ry + sized.itemHeight / 2 <= 253;
            if (!inside) tooWide += std::string(locale) + (corner ? " small " : " ") + std::to_string(static_cast<int>(widest)) + "/"
                + std::to_string(static_cast<int>(centerWide)) + "; ";
            fits = fits && inside;
        }
    }
    if (!fits) std::cout << "menu too wide: " << tooWide << '\n';
    check(fits, "the menu's ring fits a 480x253 GUI screen in English, Japanese and Chinese");
}

void drawKeys() {
    SavedPlacement a;
    a.name = "hut";
    a.file = "hut.mcstructure";
    a.placement.origin = {1, 2, 3};
    auto renamed = a, hidden = a, moved = a, allCut = a, only = a, onlyKeep = a, upTo = a, upToKeep = a;
    renamed.name = "other";
    hidden.visible = false;
    moved.placement.origin.x = 2;
    allCut.layers.cut = 1;
    only.layers.mode = LayerMode::Only;
    only.layers.cut = 1;
    onlyKeep = only;
    onlyKeep.layers.keep = LayerKeep::High;
    upTo = only;
    upTo.layers.mode = LayerMode::UpTo;
    upToKeep = upTo;
    upToKeep.layers.keep = LayerKeep::High;
    check(drawKey(a) == drawKey(renamed) && drawKey(a) == drawKey(hidden), "renaming or hiding keeps a placement's ghosts");
    check(drawKey(a) != drawKey(moved), "moving rebuilds a placement's ghosts");
    check(drawKey(a) == drawKey(allCut), "the cut under All does not rebuild");
    check(drawKey(a) != drawKey(only), "switching to Only rebuilds");
    check(drawKey(only) == drawKey(onlyKeep), "the kept side under Only does not rebuild");
    auto onlyCut = only;
    onlyCut.layers.cut = 2;
    check(drawKey(only) != drawKey(onlyCut), "moving the cut under Only rebuilds");
    check(drawKey(only) != drawKey(upTo) && drawKey(upTo) != drawKey(upToKeep),
          "UpTo uses its own key and the kept side matters there");
    for (auto const& placement : {a, only, upTo}) {
        auto remembered = placement;
        remembered.layers.lastShown = LayerMode::UpTo;
        check(drawKey(placement) == drawKey(remembered), "mode memory does not rebuild ghosts in any mode");
    }
    auto legacy = a;
    legacy.layers.legacy = true;
    legacy.layers.legacyMode = LayerMode::All;
    legacy.layers.legacyIndex = 2;
    auto legacyAll = legacy;
    legacyAll.layers.legacyIndex = 5;
    check(drawKey(legacy) == drawKey(legacyAll), "an old All view ignores its old index");
    auto legacyOnly = legacy;
    legacyOnly.layers.legacyMode = LayerMode::Only;
    legacyOnly.layers.legacyIndex = 1;
    auto legacyOnly2 = legacyOnly;
    legacyOnly2.layers.legacyIndex = 2;
    check(drawKey(legacyOnly) != drawKey(legacyOnly2), "an old Only view follows its old index");
}

// L-141: the earlier placement draws the shared cells.
void placementOverlap() {
    auto structure = std::make_shared<Structure>();
    structure->size = {2, 1, 1};
    structure->palette = {PaletteBlock{"minecraft:stone", {}, 0}, PaletteBlock{"minecraft:air", {}, 0}};
    structure->blocks = {0, 1}; // stone at x 0, air at x 1
    session::Shown first{{}, structure}, second{{}, structure}, far{{}, structure};
    first.placement.file = second.placement.file = far.placement.file = "a.mcstructure";
    first.placement.placement.origin = {0, 0, 0};
    second.placement.placement.origin = {0, 0, 0};
    far.placement.placement.origin = {10, 0, 0};
    std::vector<session::Shown> list{first, second, far};
    check(overlap::drawsCell(first, 0, {0, 0, 0}) && !overlap::drawsCell(first, 0, {1, 0, 0}),
          "a placement draws its blocks, not its air");
    auto owned = [&](size_t index, int dimension, Point cell) {
        auto earlier = overlap::earlierMeeting(list, index);
        return overlap::ownedEarlier(list, earlier, dimension, cell);
    };
    check(owned(1, 0, {0, 0, 0}) && !owned(0, 0, {0, 0, 0}), "the earlier placement owns a shared cell");
    check(!owned(1, 0, {1, 0, 0}), "an air cell of the earlier one does not take the cell");
    check(!owned(1, 1, {0, 0, 0}), "a placement in another dimension owns nothing here");
    check(overlap::earlierMeeting(list, 2).empty(), "a distant placement has no earlier one to look at");
    list[0].placement.visible = false;
    check(!owned(1, 0, {0, 0, 0}), "a hidden placement owns nothing");
    std::string hiddenKey = overlap::earlierKey(list, 1);
    list[0].placement.visible = true;
    check(overlap::earlierKey(list, 1) != hiddenKey && overlap::earlierKey(list, 2).empty(),
          "the earlier overlapping placements are part of the build key, distant ones are not");
    auto before = overlap::earlierKey(list, 1);
    list[0].structure = std::make_shared<Structure>(*structure); // its file read again
    check(overlap::earlierKey(list, 1) != before, "a new structure for the earlier placement rebuilds the overlap");
    // Border faces: placements that touch, earlier or later, not distant ones.
    session::Shown beside{{}, structure};
    beside.placement.file = "a.mcstructure";
    beside.placement.placement.origin = {2, 0, 0}; // right after the first's x 0..1
    std::vector<session::Shown> row{first, beside, far};
    check(overlap::touching(row, 0) == std::vector<size_t>{1} && overlap::touching(row, 1) == std::vector<size_t>{0}
              && overlap::touching(row, 2).empty() && !overlap::touchingKey(row, 0).empty() && overlap::touchingKey(row, 2).empty(),
          "a placement right next to another touches it both ways; a distant one touches none");
    check(overlap::paletteAt(first, 0, {0, 0, 0}) == 0 && !overlap::paletteAt(first, 0, {1, 0, 0}), "the palette index of a drawn cell");
}

void placementDocuments() {
    PlacementSet set;
    SavedPlacement hut;
    hut.name = "hut";
    hut.file = "small/hut.mcstructure";
    hut.dimension = 1;
    hut.placement = {{124, 64, -38}, 3, Mirror::Z};
    hut.layers = {LayerAxis::X, LayerMode::UpTo, LayerKeep::High, 1};
    dualWrite(hut.layers, Size{4, 6, 3}, hut.placement);
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
    auto remembered = set;
    remembered.placements[0].layers.lastShown = LayerMode::UpTo;
    check(encodePlacements(remembered) == encodePlacements(set), "mode memory is not written to the placement document");
    auto all = remembered;
    all.placements[0].layers.mode = LayerMode::All;
    dualWrite(all.placements[0].layers, Size{4, 6, 3}, hut.placement);
    auto rejoined = decodePlacements(encodePlacements(all));
    check(rejoined.placements[0].layers.lastShown == LayerMode::Only
              && leaveAll(rejoined.placements[0].layers) && rejoined.placements[0].layers.mode == LayerMode::Only,
          "reloading a placement in All forgets UpTo and leaves All for Only");
    auto suppliedMemory = decodePlacements(R"({"version":1,"placements":[{"file":"a.mcstructure","lastShown":2,
        "layerView":{"space":"structure","mode":"all","lastShown":"upTo"}}]})");
    check(suppliedMemory.placements[0].layers.lastShown == LayerMode::Only,
          "placement documents cannot supply mode memory");
    auto legacy = decodePlacements(R"({"version":1,"placements":[{"file":"a.mcstructure","layerAxis":3,"layerMode":2,"layer":1}]})");
    check(legacy.placements.size() == 1 && legacy.placements[0].layers.legacy
              && legacy.placements[0].layers.legacyAxis == LegacyLayerAxis::WestFromEast
              && legacy.placements[0].layers.legacyMode == LayerMode::UpTo && legacy.placements[0].layers.legacyIndex == 1,
          "an old document keeps its six-direction view until the size is known");
    auto converted = decodePlacements(R"({"version":1,"placements":[{"file":"a.mcstructure","layerAxis":0,"layerMode":0,"layer":0,
        "layerView":{"space":"structure","axis":"z","cut":2,"keep":"high","mode":"upTo"}}]})");
    check(converted.placements.size() == 1 && !converted.placements[0].layers.legacy
              && converted.placements[0].layers.axis == LayerAxis::Z && converted.placements[0].layers.cut == 2
              && converted.placements[0].layers.keep == LayerKeep::High && converted.placements[0].layers.mode == LayerMode::UpTo,
          "a structure-space layerView loads the cut plane");
    auto unmarked = decodePlacements(R"({"version":1,"placements":[{"file":"a.mcstructure","layerAxis":0,"layerMode":0,"layer":0,
        "layerView":{"axis":"z","cut":2,"keep":"high","mode":"upTo"}}]})");
    check(unmarked.placements.size() == 1 && unmarked.placements[0].layers.legacy,
          "an unmarked layerView falls back to the old fields");
    auto mistyped = decodePlacements(R"({"version":1,"placements":[{"file":"a.mcstructure","layerView":{"space":1}},
        {"file":"b.mcstructure","layerView":{"space":"structure","axis":0,"cut":"2","keep":[],"mode":{}}}]})");
    check(mistyped.placements.size() == 2 && mistyped.placements[0].layers.legacy && !mistyped.placements[1].layers.legacy
              && mistyped.placements[1].layers.axis == LayerAxis::Y && mistyped.placements[1].layers.cut == 0
              && mistyped.placements[1].layers.keep == LayerKeep::Low && mistyped.placements[1].layers.mode == LayerMode::All,
          "mistyped layerView fields take defaults instead of failing the document");
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
    // L-289: a liquid source needs a bucket, a flowing cell is filled by the game.
    auto count = [](std::string_view name, std::optional<int> depth, bool secondHalf = false) {
        PaletteBlock entry{std::string(name), {}, 18168865};
        if (depth) entry.states.set("liquid_depth", {static_cast<std::int8_t>(*depth)});
        return materialsCount(entry, secondHalf);
    };
    check(count("minecraft:water", 0) == 1 && count("minecraft:water", 8) == 0
          && count("minecraft:flowing_water", 8) == 0 && count("minecraft:water", std::nullopt) == 1
          && count("minecraft:lava", 0) == 1 && count("minecraft:lava", 2) == 0
          && count("minecraft:flowing_lava", 6) == 0,
          "a liquid source needs one item; a flowing liquid cell needs none");
    PaletteBlock waterlogged{"minecraft:oak_stairs", {}, 18168865};
    waterlogged.states.set("waterlogged_bit", {std::int8_t{1}});
    check(materialsCount(waterlogged, false) == 1 && count("minecraft:stone", std::nullopt) == 1
          && count("minecraft:oak_double_slab", std::nullopt) == 2 && count("minecraft:wooden_door", std::nullopt, true) == 0,
          "waterlogged and ordinary blocks keep their item count");
}
}
void otherHalves() {
    PaletteBlock door{"minecraft:jungle_door", {}, 18168865};
    door.states.set("door_hinge_bit", {std::int8_t{0}});
    door.states.set("upper_block_bit", {std::int8_t{0}});
    auto upper = otherHalf(door);
    check(upper && upper->step == 1 && upper->block.name == door.name && upper->block.version == door.version,
          "a lower door half has its other half one block above");
    std::int64_t bit = 0, hinge = 1;
    check(upper && upper->block.states.find("upper_block_bit")->integer(bit) && bit == 1
          && upper->block.states.find("door_hinge_bit")->integer(hinge) && hinge == 0
          && upper->block.states.entries.size() == door.states.entries.size(),
          "the other half flips only upper_block_bit");
    auto lower = upper ? otherHalf(upper->block) : std::nullopt;
    check(lower && lower->step == -1 && lower->block.key() == door.key(), "the upper half's other half is the lower half again");
    check(!otherHalf(PaletteBlock{"minecraft:stone", {}, 0}), "a one-block-tall block has no other half");
}
void liquidCorners() {
    using liquids::Cell;
    auto near = [](float a, float b) { return std::abs(a - b) < 1e-4f; };
    check(near(liquids::surface(0), 8.f / 9) && near(liquids::surface(7), 1.f / 9) && near(liquids::surface(8), 8.f / 9),
          "a liquid's surface: source 8/9, depth 7 one ninth, falling like a source");
    // A pool of sources walled in on every side: flat at the source height.
    auto pool = [](int dx, int dy, int dz) {
        if (dy != 0) return Cell{};
        return dx == 0 && dz == 0 ? Cell{1, 0, false} : Cell{0, 0, true};
    };
    check(near(liquids::corner(1, pool, 0, 0), 8.f / 9) && near(liquids::corner(1, pool, 1, 1), 8.f / 9),
          "a source between solid walls keeps its height at every corner");
    auto under = [](int dx, int dy, int dz) { return dy == 1 && dx == 0 && dz == 0 ? Cell{1, 0, false} : Cell{1, 3, false}; };
    check(near(liquids::corner(1, under, 0, 0), 1.f), "the same liquid above raises the corner to the full block");
    // Flowing water (depth 3) next to open air on the -x side: lower there.
    auto flow = [](int dx, int dy, int dz) {
        if (dy != 0) return Cell{};
        return dx < 0 ? Cell{0, 0, false} : Cell{1, 3, false};
    };
    check(liquids::corner(1, flow, 0, 0) < liquids::corner(1, flow, 1, 0) && near(liquids::corner(1, flow, 1, 0), liquids::surface(3)),
          "a flowing liquid slopes down toward open air");
    auto lava = [](int dx, int dy, int dz) { return dy == 0 && dx == 0 && dz == 0 ? Cell{2, 0, false} : Cell{1, 0, false}; };
    check(near(liquids::corner(2, lava, 0, 0), 8.f / 9 * 10 / 13), "another liquid around counts as open, not as the same liquid");
    auto still = [](liquids::Flow f) { return std::abs(f.x) < 1e-4f && std::abs(f.z) < 1e-4f; };
    check(still(liquids::flow(1, pool)), "a source between walls does not flow");
    // A pool of sources edged by solid stairs on +x and open ground (no water below) on -x.
    auto edged = [](int dx, int dy, int dz) {
        if (dy != 0) return Cell{0, 0, true};
        return dx > 0 ? Cell{0, 0, true} : dx < 0 ? Cell{0, 0, false} : Cell{1, 0, false};
    };
    check(still(liquids::flow(1, edged)) && liquids::corner(1, edged, 0, 0) < 8.f / 9,
          "a source sloping toward open ground still does not flow");
    // A source at the edge of a hole: open on -x with water below it.
    auto ledge = [](int dx, int dy, int dz) {
        if (dx < 0 && dy == -1) return Cell{1, 0, false};
        if (dy != 0) return Cell{0, 0, true};
        return dx < 0 ? Cell{0, 0, false} : dx == 0 && dz == 0 ? Cell{1, 0, false} : Cell{0, 0, true};
    };
    check(liquids::flow(1, ledge).x < 0, "a source flows toward an open cell with water below it");
    auto stream = [](int dx, int dy, int dz) {
        if (dy != 0) return Cell{0, 0, true};
        return dz != 0 ? Cell{0, 0, true} : Cell{1, dx + 2, false};
    };
    check(liquids::flow(1, stream).x > 0, "water flows toward its deeper (lower) neighbor");
}
void blockEntitySaves() {
    StructureBuilder builder(Size{2, 1, 1}, Point{0, 0, 0});
    nbt::Compound bed;
    bed.set("id", {std::string("Bed")});
    bed.set("color", {std::int8_t{13}});
    builder.setBlockEntity(1, bed);
    auto bytes = writeStructure(builder.structure());
    auto back = parseStructure({reinterpret_cast<std::uint8_t const*>(bytes.data()), bytes.size()});
    std::int64_t color = 0;
    auto found = back.blockEntities.find(1);
    check(found != back.blockEntities.end() && found->second.find("color") && found->second.find("color")->integer(color) && color == 13
          && back.blockEntities.size() == 1, "a saved area keeps a cell's block entity data through write and read");
}
void liquidChecks() {
    check(withLiquid(CellState::Correct, true) == CellState::Correct && withLiquid(CellState::Correct, false) == CellState::State,
          "a right block with the wrong waterlogging is a state mistake");
    check(withLiquid(CellState::Wrong, false) == CellState::Wrong && withLiquid(CellState::Missing, false) == CellState::Missing,
          "waterlogging is judged only once the block itself is right");
    using lamium::schematic::classifyReading;
    using lamium::schematic::CellReading;
    check(classifyReading({.placeholder = true}, true) == CellState::Unknown
          && classifyReading({.placeholder = true, .expectsAir = true}, true) == CellState::Unknown,
          "a placeholder block while the chunk arrives is never judged");
    check(classifyReading({.same = true}, false) == CellState::Correct && classifyReading({.actualAir = true}, false) == CellState::Missing
          && classifyReading({.sameType = true}, false) == CellState::State && classifyReading({}, false) == CellState::Wrong,
          "a block is correct, missing, in another state or wrong");
    check(classifyReading({.same = true, .liquidsMatch = false}, false) == CellState::State
          && classifyReading({.known = false, .liquidsMatch = false}, false) == CellState::Unknown,
          "a right block with the wrong water is a state mistake; an unknown block stays unknown");
    check(classifyReading({.expectsAir = true, .actualAir = true}, true) == CellState::Correct
          && classifyReading({.expectsAir = true}, true) == CellState::Extra && classifyReading({.expectsAir = true}, false) == CellState::Ignored,
          "air is correct when empty; something there is extra only when extras count");
    check(classifyReading({.expectsAir = true, .expectsLiquid = true, .same = true}, false) == CellState::Correct
          && classifyReading({.expectsAir = true, .expectsLiquid = true, .actualAir = true}, false) == CellState::Missing
          && classifyReading({.expectsAir = true, .expectsLiquid = true, .sameType = true}, false) == CellState::State
          && classifyReading({.expectsAir = true, .expectsLiquid = true, .liquidsMatch = false}, true) == CellState::Wrong,
          "air with water in the second layer is judged against the water");
    auto row = liquidDifference(true, false);
    check(row.key == "waterlogged" && row.expected == "true" && row.actual == "false", "the waterlogging row names both sides");
    auto face = faceDifference(2, 5);
    check(face.key == "facing" && face.expected == "north" && face.actual == "east" && faceDifference(9, 0).expected == "9",
          "a shulker box's opening row names both faces");
}
// L-184: the cells a ray passes, nearest first.
void rayCells() {
    auto cells = cellsAlong(.5, .5, .5, 1, 0, 0, 3.2);
    check(cells.size() == 4 && cells.front() == Point{0, 0, 0} && cells.back() == Point{3, 0, 0}, "a ray along x steps cell by cell");
    auto diagonal = cellsAlong(.5, .5, .5, 1, 1, 0, 2);
    bool adjacent = true;
    for (size_t i = 1; i < diagonal.size(); ++i)
        adjacent = adjacent && std::abs(diagonal[i].x - diagonal[i - 1].x) + std::abs(diagonal[i].y - diagonal[i - 1].y) == 1;
    check(adjacent && diagonal.size() >= 3, "a diagonal ray enters one face at a time");
    auto back = cellsAlong(-.5, 2.5, 0.5, 0, -1, 0, 1.6);
    check(back.size() == 3 && back[1] == Point{-1, 1, 0} && back[2] == Point{-1, 0, 0}, "negative directions and coordinates floor");
    check(cellsAlong(0, 0, 0, 0, 0, 0, 5).empty(), "no direction, no cells");
}
// L-186: tint by setting, outline copies by width.
void ghostLook() {
    using lamium::schematic::faces::ghostTint;
    check(std::abs(ghostTint(100)[0] - .62f) < 1e-6f && std::abs(ghostTint(100)[1] - .85f) < 1e-6f
              && ghostTint(0)[0] == 1.f && ghostTint(0)[1] == 1.f && ghostTint(40)[0] > ghostTint(100)[0]
              && ghostTint(40)[0] < 1.f && ghostTint(150)[0] == ghostTint(100)[0],
          "the tint runs from none at 0% to the earlier strong look at 100%");
    using lamium::schematic::faces::barHalfWidth;
    using lamium::schematic::faces::barQuads;
    check(barHalfWidth(0) == 0.f && barHalfWidth(2) > barHalfWidth(1) && barHalfWidth(1) > 0.f,
          "thin is a game line, normal and thick are bars of growing width");
    auto bar = barQuads({0, 0, 0}, {1, 0, 0}, .01f);
    bool around = bar.size() == 32;
    for (auto const& v : bar) around = around && (v[0] == 0.f || v[0] == 1.f) && std::abs(std::abs(v[1]) - .01f) < 1e-6f
                                      && std::abs(std::abs(v[2]) - .01f) < 1e-6f;
    check(around, "a bar is four sides around the edge, both windings, ends on the edge's ends");
    using lamium::schematic::faces::outlineRank;
    using lamium::schematic::faces::rankedBarHalfWidth;
    int wrong = outlineRank(1.f, .25f, .2f), unknown = outlineRank(1.f, .55f, .1f), state = outlineRank(1.f, .8f, .2f),
        missing = outlineRank(.35f, .85f, 1.f);
    check(wrong > unknown && unknown > state && state > missing, "wrong over unknown over wrong state over missing");
    check(rankedBarHalfWidth(1, wrong) > rankedBarHalfWidth(1, state) && rankedBarHalfWidth(1, state) > rankedBarHalfWidth(1, missing)
          && rankedBarHalfWidth(2, missing) == barHalfWidth(2) && rankedBarHalfWidth(0, wrong) == 0.f,
          "a stronger mistake's bar encloses a weaker neighbor's; thin stays a game line");
    using lamium::schematic::faces::edgeKey;
    check(edgeKey({0, 1, 0}, {1, 1, 0}) == edgeKey({0.001f, 1.f, 0.f}, {1.f, 1.001f, 0.f})
              && edgeKey({0, 1, 0}, {1, 1, 0}) != edgeKey({0, 1, 0}, {1, 1, 1}),
          "two boxes' copies of one edge share a key, other edges do not");
}
void moveRules() {
    using namespace lamium::schematic;
    auto forward = [](double x, double y, double z) { return moveAxes(x, y, z).forward; };
    auto right = [](double x, double y, double z) { return moveAxes(x, y, z).right; };
    auto up = [](double x, double y, double z) { return moveAxes(x, y, z).up; };
    constexpr double pi = 3.14159265358979323846;
    auto down = [&](double degrees) {
        double p = degrees * pi / 180;
        return forward(std::cos(p), -std::sin(p), 0);
    };
    check(forward(1, 0, 0) == Point{1, 0, 0} && right(1, 0, 0) == Point{0, 0, 1},
          "level view east steps east, right steps south");
    check(forward(0, 0, 1) == Point{0, 0, 1} && right(0, 0, 1) == Point{-1, 0, 0},
          "level view south steps south, right steps west");
    check(down(30) == Point{1, 0, 0} && down(59) == Point{1, 0, 0},
          "below 60 degrees a step stays on the horizontal axis");
    check(down(61) == Point{0, -1, 0}, "past 60 degrees down a forward step moves down");
    check(forward(std::cos(61 * pi / 180), std::sin(61 * pi / 180), 0) == Point{0, 1, 0},
          "past 60 degrees up a forward step moves up");
    check(forward(std::cos(60 * pi / 180), -std::sin(60 * pi / 180), 0) == Point{1, 0, 0},
          "exactly 60 degrees stays horizontal");
    // Sin and cos of 45 degrees are equal, so the step is decided per event:
    // 44 degrees faces south-dominant, 46 degrees east-dominant.
    check(forward(std::sin(44 * pi / 180), 0, std::cos(44 * pi / 180)) == Point{0, 0, 1}
              && forward(std::sin(46 * pi / 180), 0, std::cos(46 * pi / 180)) == Point{1, 0, 0},
          "the horizontal axis follows the view at once, with no hysteresis");
    auto steep = moveAxes(0, -1, 0);
    check(steep.forward == Point{0, -1, 0} && steep.right == Point{-1, 0, 0},
          "looking straight down keeps a fixed horizontal fallback for left and right");
    auto none = moveAxes(0, 0, 0);
    check(none.forward == Point{0, 0, 1} && none.right == Point{-1, 0, 0},
          "a zero view direction falls back to south");
    check(scaled(moveAxes(1, 0, 0).forward, -2) == Point{-2, 0, 0} && scaled(moveAxes(1, 0, 0).right, 2) == Point{0, 0, 2},
          "wheel amounts scale both axes");
    check(up(1, 0, 0) == Point{0, 1, 0} && up(std::cos(30 * pi / 180), -std::sin(30 * pi / 180), 0) == Point{0, 1, 0},
          "a level-ish view keeps up on world Y");
    check(up(std::cos(61 * pi / 180), -std::sin(61 * pi / 180), 0) == Point{1, 0, 0},
          "looking steeply down moves up to the horizon ahead, not to world Y");
    check(up(std::cos(61 * pi / 180), std::sin(61 * pi / 180), 0) == Point{-1, 0, 0},
          "looking steeply up moves up behind the view");
    auto orthogonal = [&](double x, double y, double z) {
        auto a = moveAxes(x, y, z);
        auto dot = [](Point p, Point q) { return p.x * q.x + p.y * q.y + p.z * q.z; };
        auto length = [&](Point p) { return dot(p, p); };
        return dot(a.forward, a.right) == 0 && dot(a.forward, a.up) == 0 && dot(a.right, a.up) == 0
            && length(a.forward) == 1 && length(a.right) == 1 && length(a.up) == 1;
    };
    check(orthogonal(0, 0, 1) && orthogonal(1, 0, 0) && orthogonal(0, -1, 0) && orthogonal(0, 1, 0)
              && orthogonal(std::cos(45 * pi / 180), -std::sin(45 * pi / 180), 0),
          "the six directions always stay one orthogonal unit-axis frame");
}
void moveTargetRules() {
    using namespace lamium::schematic;
    TargetValidity none{};
    check(!valid(MoveTarget::Placement, none) && !valid(MoveTarget::Corner1, none)
              && !valid(MoveTarget::Corner2, none) && !valid(MoveTarget::Area, none) && valid(MoveTarget::None, none),
          "no target is valid until one exists");
    check(fallback(none) == MoveTarget::None && nextValid(MoveTarget::None, none) == MoveTarget::None,
          "an empty set falls back to and cycles to no target");
    TargetValidity corners{false, true, true, true};
    check(!valid(MoveTarget::None, corners), "no target stops being valid once a candidate exists");
    check(fallback(corners) == MoveTarget::Corner1, "the first valid target is the fallback");
    check(nextValid(MoveTarget::None, corners) == MoveTarget::Corner1
              && nextValid(MoveTarget::Corner1, corners) == MoveTarget::Corner2
              && nextValid(MoveTarget::Corner2, corners) == MoveTarget::Area
              && nextValid(MoveTarget::Area, corners) == MoveTarget::Corner1,
          "the cycle skips invalid targets and wraps");
    check(previousValid(MoveTarget::Corner1, corners) == MoveTarget::Area
              && previousValid(MoveTarget::Area, corners) == MoveTarget::Corner2,
          "the reverse cycle also skips invalid targets and wraps");
    TargetValidity placementOnly{true, false, false, false};
    check(nextValid(MoveTarget::Area, placementOnly) == MoveTarget::Placement
              && nextValid(MoveTarget::Placement, placementOnly) == MoveTarget::Placement,
          "a single valid target stays selected while cycling");
    TargetValidity areaOnly{false, false, false, true};
    check(fallback(areaOnly) == MoveTarget::Area, "an area alone is a valid target");
}
void targetRayRules() {
    using namespace lamium::schematic;
    TargetRay through{0.5, 0.5, -2, 0, 0, 1, 256};
    auto cell = cellEntry(through, 0, 0, 0);
    check(cell && std::abs(*cell - 2) < 1e-9, "a ray through a cell reports its entry distance");
    TargetRay inside{0.5, 0.5, 0.5, 0, 0, 1, 256};
    check(cellEntry(inside, 0, 0, 0) == 0, "an origin inside the cell enters at 0");
    TargetRay outside{2, 0.5, -2, 0, 0, 1, 256};
    check(!cellEntry(outside, 0, 0, 0), "a ray beside the cell misses it");
    TargetRay behind{0.5, 0.5, 2, 0, 0, 1, 256};
    check(!cellEntry(behind, 0, 0, 0), "a cell behind the ray misses");
    TargetRay shortReach{0.5, 0.5, -2, 0, 0, 1, 1};
    check(!cellEntry(shortReach, 0, 0, 0), "a cell past the reach misses");
    auto area = boxEntry(through, 0, 0, 0, 4, 4, 4);
    check(area && std::abs(*area - 2) < 1e-9, "an area box uses the same entry distance");
}
// L-285: what the game's pick means for a ghost pick. The block under the
// player's feet, which the game picks when the ray meets nothing, is off the
// ray and must not shorten the ghost search; a block the player aimed at
// still does.
void pickRayRules() {
    using namespace lamium::schematic;
    // Eye at the origin; `d` is the game's ray direction and `p` its hit point.
    struct Case {
        double px, py, pz, dx, dy, dz;
        bool on;
        char const* what;
    };
    Case const cases[] = {
        {0, 0, -1.5, 0, 0, -1, true, "a block face on the view ray is a hit the player aimed at"},
        {0, -1.12, 0, 0, 0, -1, false, "the block under the feet is not on the view ray"},
        {0, 0, 3, 0, 0, -1, false, "a hit behind the player is not on the view ray"},
        {0, pickRayTolerance, 0, 1, 0, 0, true, "a hit exactly at the tolerance is on the ray"},
        {0, pickRayTolerance * 1.002, 0, 1, 0, 0, false, "a hit just past the tolerance is off the ray"},
        {0, .04, -1.5, 0, 0, -1, true, "a hit inside the tolerance a few blocks out is on the ray"},
        {0, 0, -1.5, 0, 0, -3, true, "the direction's length does not matter"},
        {0, 0, -1.5, 0, 0, 0, false, "a ray with no direction has no point on it"},
        {0, 0, 0, 0, 0, -1, true, "the ray's own start is on it"},
        {2, 2.2, 0, 1, 1, 0, false, "the distance is measured across the ray, not along an axis"},
    };
    for (auto const& c : cases) check(onRay(0, 0, 0, c.dx, c.dy, c.dz, c.px, c.py, c.pz, pickRayTolerance) == c.on, c.what);
    check(onRay(0, 0, 0, 0, 0, -1, 0, 0, -1.5, 0) && !onRay(0, 0, 0, 0, 0, -1, pickRayTolerance, 0, -1.5, 0),
          "a tolerance of 0 keeps only the ray itself");
    check(ghostReach(true, 2, 5) > 1.9 && ghostReach(true, 2, 5) < 2,
          "a hit the player aimed at shortens the ghost search to just before it");
    check(ghostReach(false, 1.12, 5) == 5, "the feet fallback leaves the whole pick range");
    check(ghostReach(true, 3, 5) < 3.2 && 3.2 < ghostReach(false, 1.12, 5),
          "a ghost behind a block the player aimed at stays out of the shortened reach");
    check(!(ghostReach(true, .04, 5) > 0), "a hit closer than the clearance leaves nothing to search");
}
void unreadableNoticeRules() {
    using namespace lamium::schematic;
    UnreadableNotice notice;
    std::vector<UnreadablePlacement> failed{{1, "House"}, {2, "Tower"}, {3, "Bridge"}};
    std::vector<std::string> names{"House", "Tower", "Bridge"};
    check(notice.update(1, true, {}).empty(), "no unreadable placements means no announcement");
    check(notice.update(1, true, failed) == names, "new failures in one frame form one announcement");
    check(notice.update(1, true, failed).empty(), "a placement is announced only once per generation");
    notice.update(1, true, {});
    check(notice.update(1, true, failed).empty(), "recovery and failure again do not repeat the announcement");
    failed.push_back({4, "House"});
    check(notice.update(1, true, failed) == std::vector<std::string>{"House"}, "placement identity is its id, not its name");
    failed.pop_back();
    check(notice.update(2, true, failed) == names, "a new world generation forgets announcements");
    check(notice.update(3, false, failed).empty(), "Schematics off announces nothing");
    check(notice.update(3, true, failed) == names, "Schematics off does not mark failures as announced");
    check(notice.update(3, false, failed).empty() && notice.update(3, true, failed).empty(),
          "switching Schematics off and on keeps announcements in the same world");
    notice.update(4, false, {});
    check(notice.update(5, true, failed) == names, "session stop and rejoin forget announcements");
}
void placementSteps() {
    using namespace lamium::schematic;
    check(steppedPlacement(-1, 3, 1) == 0 && steppedPlacement(-1, 3, -1) == 2,
          "from no selection, Next starts at the first placement and Previous at the last");
    check(steppedPlacement(0, 3, -1) == 2 && steppedPlacement(2, 3, 1) == 0,
          "placement steps wrap in both directions");
    check(steppedPlacement(-1, 4, -2) == 2 && steppedPlacement(1, 4, 6) == 3,
          "menu placement steps keep the key start rule for multiple steps");
    check(steppedPlacement(-1, 0, -1) == -1 && steppedPlacement(-1, 1, -1) == 0,
          "placement steps handle an empty list or a single placement");
}
void verificationReadiness() {
    using namespace lamium::schematic;
    Verification result;
    result.placement = 2;
    check(!result.ready(2), "an unfinished verification is counting");
    result.complete = true;
    check(result.ready(2) && !result.ready(1) && !result.ready(-1),
          "only a completed verification for the selected placement is ready");
    result.stale = true;
    check(!result.ready(2), "a cut or move invalidates completed counts until the next pass");
    result.stale = false;
    check(result.ready(2), "a fresh completed pass makes counts ready again");
}
void checkRelaxations() {
    namespace r = lamium::schematic::rules;
    using States = std::map<std::string, std::string>;
    States wire{{"redstone_signal", "15"}}, wireOff{{"redstone_signal", "0"}};
    check(!r::sameBlock("minecraft:redstone_wire", wire, "minecraft:redstone_wire", wireOff, 0)
              && r::sameBlock("minecraft:redstone_wire", wire, "minecraft:redstone_wire", wireOff, r::Signal)
              && !r::sameBlock("minecraft:redstone_wire", wire, "minecraft:redstone_wire", wireOff, r::Growth),
          "a signal difference counts only while signals are compared");
    States repeater{{"direction", "1"}, {"repeater_delay", "2"}}, faced{{"direction", "3"}, {"repeater_delay", "2"}};
    check(!r::sameType("minecraft:unpowered_repeater", "minecraft:powered_repeater", 0)
              && r::sameType("minecraft:unpowered_repeater", "minecraft:powered_repeater", r::Lit)
              && !r::sameBlock("minecraft:unpowered_repeater", repeater, "minecraft:powered_repeater", repeater, r::Lit | r::Signal)
              && r::sameType("minecraft:redstone_torch", "minecraft:unlit_redstone_torch", r::Lit)
              && r::sameType("minecraft:lit_redstone_lamp", "minecraft:redstone_lamp", r::Lit),
          "on and off forms are one kind (a state mistake) with the rule on, never the same block");
    auto torch = r::switchedOn("minecraft:redstone_torch"), unlit = r::switchedOn("minecraft:unlit_redstone_torch");
    auto repeaterOn = r::switchedOn("minecraft:powered_repeater");
    check(torch && torch->on && torch->state == "lit" && unlit && !unlit->on && repeaterOn && repeaterOn->state == "powered"
              && !r::switchedOn("minecraft:stone"),
          "which form of a pair is on, named lit or powered as in Java Edition");
    check(!r::sameBlock("minecraft:unpowered_repeater", repeater, "minecraft:powered_repeater", faced, r::Lit | r::Signal)
              && !r::sameType("minecraft:redstone_lamp", "minecraft:glowstone", r::Lit),
          "other states and other blocks still count");
    States young{{"growth", "1"}}, grown{{"growth", "7"}}, sapling{{"age_bit", "0"}, {"sapling_type", "oak"}},
        grownSapling{{"age_bit", "1"}, {"sapling_type", "oak"}}, birch{{"age_bit", "1"}, {"sapling_type", "birch"}};
    check(r::sameBlock("minecraft:wheat", young, "minecraft:wheat", grown, r::Growth)
              && r::sameBlock("minecraft:sapling", sapling, "minecraft:sapling", grownSapling, r::Growth)
              && !r::sameBlock("minecraft:sapling", sapling, "minecraft:sapling", birch, r::Growth),
          "growth stages are ignored, the kind of plant is not");
    check(!r::sameBlock("minecraft:wheat", young, "minecraft:wheat", States{}, 0) && r::ignoredState("kelp_age", r::Growth)
              && !r::ignoredState("open_bit", r::Signal | r::Lit | r::Growth),
          "a state only one side has still counts; doors' open state is not a signal");
}
void schematicTests() {
    checkRelaxations();
    placementSteps();
    verificationReadiness();
    unreadableNoticeRules();
    rayCells();
    ghostLook();
    otherHalves();
    liquidChecks();
    blockEntitySaves();
    liquidCorners();
    verificationOrder();
    placementDocuments();
    placementOverlap();
    drawKeys();
    menuRules();
    moveRules();
    moveTargetRules();
    targetRayRules();
    pickRayRules();
    ghostFaces();
    placementTransforms();
    entityRules();
    previewRules();
    saveRules();
    savePromptHits();
    layerModeMemory();
    layerRules();
    verifyRules();
    nbtBasics();
    structureRoundTrip();
    structureRejects();
    structureWrittenShape();
    sampleFiles();
    migrationFiles();
}

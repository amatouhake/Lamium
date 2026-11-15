#include "features/schematic/CheckIgnores.h"
#include "features/schematic/PlacementStore.h"
#include <string>
void check(bool, char const*);
void checkIgnoresTests() {
    using namespace lamium::schematic;
    IgnoredDifference lever{{1, 0, 2}, "minecraft:lever", "open_bit"};
    IgnoredDifference trapdoor{{1, 0, 2}, "minecraft:lever", "lever_direction"};

    IgnoreList list;
    check(list.empty() && list.fingerprint().empty(), "a placement starts with nothing ignored");
    check(list.add(lever) == IgnoreList::Added::Added && list.size() == 1, "an entry is added");
    check(list.add(lever) == IgnoreList::Added::Already && list.size() == 1, "the same difference is added once");
    check(list.add({{0, 0, 0}, "minecraft:lever", "open bit"}) == IgnoreList::Added::Invalid
              && list.add({{-1, 0, 0}, "minecraft:lever", "open_bit"}) == IgnoreList::Added::Invalid
              && list.add({{0, 0, 0}, std::string(maxIgnoredBlockName + 1, 'a'), "open_bit"}) == IgnoreList::Added::Invalid,
          "names with spaces, negative cells and overlong names are refused");
    auto copy = list;
    check(list.add(trapdoor) == IgnoreList::Added::Added && copy.size() == 1 && list.size() == 2,
          "a copy is not changed by a later add");
    check(list.anyAt({1, 0, 2}) && !list.anyAt({2, 0, 1}), "a cell with entries is found, another is not");
    check(list.ignored({1, 0, 2}, "minecraft:lever", "open_bit") && !list.ignored({1, 0, 2}, "minecraft:stone_button", "open_bit")
              && !list.ignored({1, 0, 2}, "minecraft:lever", "powered_bit"),
          "an entry names one state of one block in one cell");
    auto before = list.fingerprint();
    check(list.remove(trapdoor) && !list.remove(trapdoor) && list.size() == 1 && list.fingerprint() != before,
          "removing one entry changes the fingerprint");
    list.clear();
    check(list.empty(), "everything can be undone at once");

    IgnoreList full;
    for (int i = 0; i < static_cast<int>(maxIgnored); ++i) full.add({{i, 0, 0}, "minecraft:lever", "open_bit"});
    check(full.size() == maxIgnored && full.add({{999, 0, 0}, "minecraft:lever", "open_bit"}) == IgnoreList::Added::Full,
          "the number of entries is capped");

    // Saved form.
    check(encodeIgnored(lever) == "1,0,2 minecraft:lever open_bit", "an entry is saved as one string");
    check(decodeIgnored("1,0,2 minecraft:lever open_bit") == lever, "and read back");
    check(!decodeIgnored("1,0 minecraft:lever open_bit") && !decodeIgnored("1,0,2,3 minecraft:lever open_bit")
              && !decodeIgnored("1,0,2 minecraft:lever") && !decodeIgnored("1,0,2 a b c") && !decodeIgnored("x,0,2 a b")
              && !decodeIgnored("-1,0,2 a b"),
          "broken entries are skipped");

    // Staleness: the entry applies only while the file has the named block there.
    Structure structure;
    structure.size = {2, 1, 3};
    structure.palette.resize(2);
    structure.palette[0].name = "minecraft:air";
    structure.palette[1].name = "minecraft:lever";
    structure.blocks.assign(6, 0);
    structure.blocks[static_cast<size_t>(structure.cell(1, 0, 2))] = 1;
    check(ignoreApplies(lever, structure), "an entry on the named block applies");
    check(!ignoreApplies({{0, 0, 2}, "minecraft:lever", "open_bit"}, structure), "one on another block is stale");
    check(!ignoreApplies({{5, 0, 0}, "minecraft:lever", "open_bit"}, structure), "one outside the structure is stale");
    structure.blocks[static_cast<size_t>(structure.cell(1, 0, 2))] = voidCell;
    check(!ignoreApplies(lever, structure), "one on a structure void is stale");
    structure.blocks[static_cast<size_t>(structure.cell(1, 0, 2))] = 1;
    IgnoreList counted({lever, {{0, 0, 0}, "minecraft:lever", "open_bit"}});
    check(counted.size() == 2 && appliedIgnores(counted, structure) == 1, "the count leaves out stale entries");

    // Verdicts and totals.
    IgnoreList one({lever});
    std::vector<StateDifference> openOnly{{"open_bit", "0", "1"}};
    auto passed = applyIgnores(CellState::State, openOnly, one, {1, 0, 2}, "minecraft:lever");
    check(passed.state == CellState::Correct && passed.shown.empty(), "a cell whose only difference is ignored is correct");
    std::vector<StateDifference> two{{"lever_direction", "east", "west"}, {"open_bit", "0", "1"}};
    auto partly = applyIgnores(CellState::State, two, one, {1, 0, 2}, "minecraft:lever");
    check(partly.state == CellState::State && partly.shown.size() == 1 && partly.shown[0].key == "lever_direction",
          "another difference keeps the cell a mistake and is the only one listed");
    check(applyIgnores(CellState::State, openOnly, one, {1, 0, 2}, "minecraft:stone_button").state == CellState::State,
          "another block in the cell is still a mistake");
    check(applyIgnores(CellState::Wrong, openOnly, one, {1, 0, 2}, "minecraft:lever").state == CellState::Wrong,
          "a wrong block is never ignored");
    check(applyIgnores(CellState::State, openOnly, one, {1, 0, 2}, "minecraft:lever", true).state == CellState::State,
          "a cause the reading could not name keeps the mistake");
    check(applyIgnores(CellState::State, {}, one, {1, 0, 2}, "minecraft:lever").state == CellState::State,
          "a mistake without named differences is kept");
    IgnoreList facing({{{1, 0, 2}, "minecraft:lever", "facing"}});
    check(applyIgnores(CellState::State, {faceDifference(2, 3)}, facing, {1, 0, 2}, "minecraft:lever").state == CellState::State,
          "block entity rows cannot be ignored");
    Tally tally;
    tally.add(passed.state, true);
    tally.add(partly.state, true);
    check(tally.correct == 1 && tally.state == 1 && tally.total() == 2 && tally.mistakes() == 1,
          "an ignored difference counts as matching in the totals");

    // The entry follows the placement: turned and mirrored, the world cell
    // holding it maps back to the same structure cell.
    Size size{2, 1, 3};
    for (int rotation = 0; rotation < 4; ++rotation)
        for (auto mirror : {Mirror::None, Mirror::X, Mirror::Z}) {
            Placement placement{{100, 64, -20}, rotation, mirror};
            auto world = ignoredWorldCell(lever, size, placement);
            auto local = toLocal(size, placement, world);
            check(local && *local == lever.cell && one.anyAt(*local), "an ignored cell moves with the placement's turn and mirror");
        }

    // Placement documents: optional key, older documents unchanged.
    PlacementSet set;
    SavedPlacement hut;
    hut.name = "hut";
    hut.file = "hut.mcstructure";
    set.placements.push_back(hut);
    auto plain = encodePlacements(set);
    check(plain.find("ignored") == std::string::npos, "a placement without ignores writes no key");
    auto old = decodePlacements(R"({"version":1,"placements":[{"file":"a.mcstructure"}]})");
    check(old.placements.size() == 1 && old.placements[0].ignored.empty(), "an older placement loads with nothing ignored");
    set.placements[0].ignored = IgnoreList({lever, trapdoor});
    auto back = decodePlacements(encodePlacements(set));
    check(back.placements[0].ignored == set.placements[0].ignored, "ignores round-trip");
    check(checkKey(back.placements[0]) != checkKey(decodePlacements(plain).placements[0])
              && drawKey(back.placements[0]) == drawKey(decodePlacements(plain).placements[0]),
          "ignores change the check key, not the draw key");
    // L-236: the mesh key leaves out only the layer view.
    {
        auto base = back.placements[0];
        auto layered = base;
        layered.layers.legacy = false;
        layered.layers.mode = LayerMode::UpTo;
        layered.layers.cut = 3;
        check(meshKey(layered) == meshKey(base) && drawKey(layered) != drawKey(base), "the mesh key ignores the layer view");
        auto changed = [&](auto&& apply) { auto p = base; apply(p); return meshKey(p) != meshKey(base); };
        check(changed([](SavedPlacement& p) { p.file = "other.mcstructure"; }) && changed([](SavedPlacement& p) { p.dimension = 1; })
                  && changed([](SavedPlacement& p) { p.placement.origin.x += 1; }) && changed([](SavedPlacement& p) { p.placement.origin.y += 1; })
                  && changed([](SavedPlacement& p) { p.placement.origin.z += 1; }) && changed([](SavedPlacement& p) { p.placement.rotation = 1; })
                  && changed([](SavedPlacement& p) { p.placement.mirror = Mirror::X; }) && changed([](SavedPlacement& p) { p.countExtras = false; })
                  && changed([](SavedPlacement& p) { p.entities = false; }),
              "and changes with every other field the ghosts depend on");
    }
    auto tolerant = decodePlacements(R"({"version":1,"placements":[{"file":"a.mcstructure",
        "ignored":["1,0,2 minecraft:lever open_bit", 5, "broken", "1,0,2 minecraft:lever open_bit"]}]})");
    check(tolerant.placements.size() == 1 && tolerant.placements[0].ignored.size() == 1,
          "unreadable and repeated entries are dropped, the placement kept");
    auto wrongType = decodePlacements(R"({"version":1,"placements":[{"file":"a.mcstructure","ignored":"x"}]})");
    check(wrongType.placements.size() == 1 && wrongType.placements[0].ignored.empty(), "an ignored key of another type is skipped");

    // The largest document still reads back: every placement full, longest names.
    PlacementSet largest;
    for (size_t p = 0; p < maxPlacements; ++p) {
        SavedPlacement big;
        big.name = std::string(maxPlacementName, 'n');
        big.file = std::string(200, 'f') + ".mcstructure";
        std::vector<IgnoredDifference> entries;
        for (int i = 0; i < static_cast<int>(maxIgnored); ++i)
            entries.push_back({{10000 + i, 10000, 10000}, std::string(maxIgnoredBlockName, 'b'), std::string(maxIgnoredStateName, 's')});
        big.ignored = IgnoreList(std::move(entries));
        largest.placements.push_back(std::move(big));
    }
    bool readBack = false;
    try {
        auto again = decodePlacements(encodePlacements(largest));
        readBack = again.placements.size() == maxPlacements && again.placements.back().ignored.size() == maxIgnored;
    } catch (...) {}
    check(readBack, "a document with every placement full of ignores stays under the size limit");
}

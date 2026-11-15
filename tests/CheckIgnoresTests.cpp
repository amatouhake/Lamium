#include "features/schematic/CheckIgnores.h"
#include "features/schematic/PlacementStore.h"
#include "features/schematic/Verification.h"
#include <string>
void check(bool, char const*);
void checkIgnoresTests() {
    using namespace lamium::schematic;
    auto lever = stateEntry({1, 0, 2}, "minecraft:lever", "open_bit");
    auto direction = stateEntry({1, 0, 2}, "minecraft:lever", "lever_direction");
    auto whole = blockEntry({1, 0, 2}, "minecraft:lever");
    auto everyLever = everyEntry("minecraft:lever", "open_bit");

    IgnoreList list;
    check(list.empty() && list.fingerprint().empty(), "a placement starts with nothing ignored");
    check(list.add(lever) == IgnoreList::Added::Added && list.size() == 1, "an entry is added");
    check(list.add(lever) == IgnoreList::Added::Already && list.size() == 1, "the same difference is added once");
    check(list.add(stateEntry({0, 0, 0}, "minecraft:lever", "open bit")) == IgnoreList::Added::Invalid
              && list.add(stateEntry({-1, 0, 0}, "minecraft:lever", "open_bit")) == IgnoreList::Added::Invalid
              && list.add(stateEntry({0, 0, 0}, std::string(maxIgnoredBlockName + 1, 'a'), "open_bit")) == IgnoreList::Added::Invalid
              && list.add({IgnoreKind::Block, {0, 0, 0}, "minecraft:lever", "open_bit"}) == IgnoreList::Added::Invalid
              && list.add({IgnoreKind::Every, {1, 0, 0}, "minecraft:lever", "open_bit"}) == IgnoreList::Added::Invalid
              && list.add(everyEntry("minecraft:lever", "")) == IgnoreList::Added::Invalid,
          "names with spaces, negative cells, overlong names and malformed kinds are refused");
    auto copy = list;
    check(list.add(direction) == IgnoreList::Added::Added && copy.size() == 1 && list.size() == 2,
          "a copy is not changed by a later add");
    check(list.touches({1, 0, 2}, "minecraft:lever") && !list.touches({2, 0, 1}, "minecraft:lever"),
          "a cell with entries is found, another is not");
    check(list.covers({1, 0, 2}, "minecraft:lever", "open_bit") == Covered::Here
              && list.covers({1, 0, 2}, "minecraft:stone_button", "open_bit") == Covered::None
              && list.covers({1, 0, 2}, "minecraft:lever", "powered_bit") == Covered::None && !list.wholeBlock({1, 0, 2}, "minecraft:lever"),
          "a state entry names one state of one block in one cell");
    auto before = list.fingerprint();
    check(list.remove(direction) && !list.remove(direction) && list.size() == 1 && list.fingerprint() != before,
          "removing one entry changes the fingerprint");
    list.clear();
    check(list.empty(), "everything can be undone at once");

    // The three kinds side by side.
    IgnoreList kinds({whole, lever, everyLever});
    check(kinds.wholeBlock({1, 0, 2}, "minecraft:lever") && !kinds.wholeBlock({1, 0, 2}, "minecraft:stone")
              && !kinds.wholeBlock({0, 0, 0}, "minecraft:lever"),
          "a whole-block entry names the block of one cell");
    check(kinds.covers({7, 7, 7}, "minecraft:lever", "open_bit") == Covered::Every
              && kinds.covers({1, 0, 2}, "minecraft:lever", "open_bit") == Covered::Here
              && kinds.covers({7, 7, 7}, "minecraft:stone_button", "open_bit") == Covered::None,
          "an every-same-block entry covers that state on every such block, a cell's own entry first");
    check(kinds.touches({7, 7, 7}, "minecraft:lever") && !kinds.touches({7, 7, 7}, "minecraft:stone"),
          "a cell with no entry of its own is still touched by an every-same-block entry for its block");
    auto removed = kinds.removeIf([](IgnoreEntry const& e) { return e.kind != IgnoreKind::Every && e.cell == Point{1, 0, 2}; });
    check(removed == 2 && kinds.size() == 1 && kinds.covers({1, 0, 2}, "minecraft:lever", "open_bit") == Covered::Every,
          "un-ignoring a cell removes its own entries and keeps every-same-block ones");

    // The cap: one at a time and all or nothing.
    IgnoreList full;
    for (int i = 0; i < static_cast<int>(maxIgnored); ++i) full.add(stateEntry({i, 0, 0}, "minecraft:lever", "open_bit"));
    check(full.size() == maxIgnored && full.add(blockEntry({999, 0, 0}, "minecraft:lever")) == IgnoreList::Added::Full,
          "the number of entries is capped across kinds");
    IgnoreList nearly;
    for (int i = 0; i < 62; ++i) nearly.add(blockEntry({i, 0, 0}, "minecraft:stone"));
    auto beforeBulk = nearly;
    std::vector<IgnoreEntry> three{blockEntry({100, 0, 0}, "minecraft:stone"), blockEntry({101, 0, 0}, "minecraft:stone"),
                                   blockEntry({102, 0, 0}, "minecraft:stone")};
    check(nearly.addAll(three) == IgnoreList::Added::Full && nearly == beforeBulk, "a bulk add over the cap changes nothing");
    std::vector<IgnoreEntry> twoNew{blockEntry({0, 0, 0}, "minecraft:stone"), blockEntry({100, 0, 0}, "minecraft:stone"),
                                    blockEntry({101, 0, 0}, "minecraft:stone")};
    check(nearly.addAll(twoNew) == IgnoreList::Added::Added && nearly.size() == maxIgnored,
          "entries already there do not count against the cap");

    // Saved form: the three kinds, and the per-state build's entries.
    check(encodeIgnored(lever) == "1,0,2 minecraft:lever open_bit" && encodeIgnored(whole) == "1,0,2 minecraft:lever"
              && encodeIgnored(everyLever) == "* minecraft:lever open_bit",
          "each kind is saved as one string");
    check(decodeIgnored("1,0,2 minecraft:lever open_bit") == lever && decodeIgnored("1,0,2 minecraft:lever") == whole
              && decodeIgnored("* minecraft:lever open_bit") == everyLever,
          "and read back; the per-state build's form is one state of a cell");
    check(!decodeIgnored("1,0 minecraft:lever open_bit") && !decodeIgnored("1,0,2,3 minecraft:lever open_bit")
              && !decodeIgnored("1,0,2") && !decodeIgnored("1,0,2 a b c") && !decodeIgnored("x,0,2 a b")
              && !decodeIgnored("-1,0,2 a b") && !decodeIgnored("* minecraft:lever") && !decodeIgnored("1,0,2  a"),
          "broken entries are skipped");

    // Staleness: an entry applies only while the file has the named block there.
    Structure structure;
    structure.size = {2, 1, 3};
    structure.palette.resize(3);
    structure.palette[0].name = "minecraft:air";
    structure.palette[1].name = "minecraft:lever";
    structure.palette[2].name = "minecraft:stone";
    structure.blocks.assign(6, 0);
    structure.blocks[static_cast<size_t>(structure.cell(1, 0, 2))] = 1;
    structure.blocks[static_cast<size_t>(structure.cell(0, 0, 0))] = 1;
    auto counts = blockCounts(structure);
    check(sameBlocks(counts, "minecraft:lever") == 2 && sameBlocks(counts, "minecraft:air") == 0
              && sameBlocks(counts, "minecraft:stone") == 0,
          "blocks are counted per name, air and absent blocks not");
    check(ignoreApplies(lever, structure, counts) && ignoreApplies(whole, structure, counts)
              && ignoreApplies(everyLever, structure, counts),
          "entries on the named block apply");
    check(!ignoreApplies(stateEntry({0, 0, 2}, "minecraft:lever", "open_bit"), structure, counts)
              && !ignoreApplies(blockEntry({1, 0, 1}, "minecraft:air"), structure, counts)
              && !ignoreApplies(stateEntry({5, 0, 0}, "minecraft:lever", "open_bit"), structure, counts)
              && !ignoreApplies(everyEntry("minecraft:stone", "facing"), structure, counts),
          "one on another block, on air, outside the structure, or for a block the file lacks is stale");
    structure.blocks[static_cast<size_t>(structure.cell(1, 0, 2))] = voidCell;
    check(!ignoreApplies(lever, structure, counts), "one on a structure void is stale");
    structure.blocks[static_cast<size_t>(structure.cell(1, 0, 2))] = 1;
    IgnoreList counted({lever, stateEntry({0, 0, 1}, "minecraft:lever", "open_bit")});
    auto tallied = countIgnores(counted, structure, counts);
    check(counted.size() == 2 && tallied.applied == 1 && tallied.stale == 1, "the count leaves out stale entries");
    check(ignoredButton({}) == IgnoredButton::Hidden && ignoredButton({2, 1}) == IgnoredButton::Count
              && ignoredButton({0, 3}) == IgnoredButton::CountStale,
          "the Ignored button is hidden with no entries and names stale ones when nothing else is left");

    // Verdicts.
    IgnoreList one({lever});
    std::vector<StateDifference> openOnly{{"open_bit", "0", "1"}};
    check(judgeIgnores(CellState::State, true, openOnly, one, {1, 0, 2}, "minecraft:lever") == IgnoreMark::States
              && openOnly[0].covered == Covered::Here,
          "a cell whose only difference is ignored counts as correct, and says what covers it");
    std::vector<StateDifference> two{{"lever_direction", "east", "west"}, {"open_bit", "0", "1"}};
    check(judgeIgnores(CellState::State, true, two, one, {1, 0, 2}, "minecraft:lever") == IgnoreMark::None
              && two[0].covered == Covered::None && two[1].covered == Covered::Here,
          "another difference keeps the cell a mistake; the ignored one is still marked");
    std::vector<StateDifference> again{{"open_bit", "0", "1"}};
    check(judgeIgnores(CellState::State, true, again, one, {1, 0, 2}, "minecraft:stone_button") == IgnoreMark::None,
          "another block in the cell is still a mistake");
    check(judgeIgnores(CellState::Wrong, true, again, one, {1, 0, 2}, "minecraft:lever") == IgnoreMark::None,
          "a state entry never passes a wrong block");
    check(judgeIgnores(CellState::State, true, again, one, {1, 0, 2}, "minecraft:lever", true) == IgnoreMark::None,
          "a cause the reading could not name keeps the mistake");
    std::vector<StateDifference> none;
    check(judgeIgnores(CellState::State, true, none, one, {1, 0, 2}, "minecraft:lever") == IgnoreMark::None,
          "a mistake without named differences is kept");
    IgnoreList facing({stateEntry({1, 0, 2}, "minecraft:lever", "facing")});
    std::vector<StateDifference> opening{faceDifference(2, 3)};
    check(judgeIgnores(CellState::State, true, opening, facing, {1, 0, 2}, "minecraft:lever") == IgnoreMark::None,
          "block entity rows cannot be ignored");
    IgnoreList wholeOnly({whole});
    std::vector<StateDifference> empty;
    check(judgeIgnores(CellState::Wrong, true, empty, wholeOnly, {1, 0, 2}, "minecraft:lever") == IgnoreMark::Block
              && judgeIgnores(CellState::Missing, true, empty, wholeOnly, {1, 0, 2}, "minecraft:lever") == IgnoreMark::Block
              && judgeIgnores(CellState::Correct, true, empty, wholeOnly, {1, 0, 2}, "minecraft:lever") == IgnoreMark::Block
              && judgeIgnores(CellState::State, true, opening, wholeOnly, {1, 0, 2}, "minecraft:lever") == IgnoreMark::Block,
          "a whole-block entry takes the cell out whatever is there, block entity differences too");
    check(judgeIgnores(CellState::Extra, false, empty, wholeOnly, {1, 0, 2}, "minecraft:air") == IgnoreMark::None,
          "a cell expecting air has no entries");
    IgnoreList allLevers({everyLever});
    std::vector<StateDifference> elsewhere{{"open_bit", "0", "1"}};
    check(judgeIgnores(CellState::State, true, elsewhere, allLevers, {9, 0, 9}, "minecraft:lever") == IgnoreMark::States
              && elsewhere[0].covered == Covered::Every,
          "an every-same-block entry passes any such block whose only difference it names");

    // Totals: a whole block is left out, ignored states count as correct.
    Tally tally;
    tally.add(CellState::State, true, IgnoreMark::States);
    tally.add(CellState::State, true, IgnoreMark::None);
    tally.add(CellState::Wrong, true, IgnoreMark::Block);
    tally.add(CellState::Missing, true, IgnoreMark::Block);
    tally.add(CellState::Correct, true, IgnoreMark::Block);
    tally.add(CellState::Missing, true, IgnoreMark::None);
    check(tally.correct == 1 && tally.state == 1 && tally.wrong == 0 && tally.missing == 1 && tally.total() == 3
              && tally.mistakes() == 1,
          "ignored states count as matching, whole-block cells are in no count");
    check(!countedState(CellState::Missing, IgnoreMark::Block) && countedState(CellState::State, IgnoreMark::States) == CellState::Correct
              && countedState(CellState::Wrong, IgnoreMark::None) == CellState::Wrong,
          "what a cell adds to the counts follows its mark");

    // Readers of the rows: the nearest mistake skips ignored rows and missing
    // blocks, a partly ignored row is still found (the in-game failure of the
    // per-state build: ignored differences stayed in the HUD and the key).
    auto row = [](CellState state, int x, IgnoreMark mark) {
        Mismatch m;
        m.state = state;
        m.position = {x, 0, 0};
        m.ignore = mark;
        return m;
    };
    std::vector<Mismatch> rows{row(CellState::State, 1, IgnoreMark::States), row(CellState::Wrong, 2, IgnoreMark::Block),
                               row(CellState::Missing, 3, IgnoreMark::None), row(CellState::State, 6, IgnoreMark::None),
                               row(CellState::Extra, 9, IgnoreMark::None)};
    double squared = 0;
    auto const* nearest = nearestMistake(rows, 0, 0, 0, &squared);
    check(nearest && nearest->position.x == 6 && squared > 40 && squared < 45,
          "the nearest mistake skips ignored rows and missing blocks");
    check(!countsAsMistake(rows[0]) && !countsAsMistake(rows[1]) && !countsAsMistake(rows[2]) && countsAsMistake(rows[3])
              && countsAsMistake(rows[4]),
          "only rows not ignored count as mistakes");
    std::vector<Mismatch> allIgnored{row(CellState::State, 1, IgnoreMark::States), row(CellState::Wrong, 2, IgnoreMark::Block)};
    check(!nearestMistake(allIgnored, 0, 0, 0), "with every mistake ignored there is no nearest one");

    // The list's order: every-same-block entries first, then the nearest cells, stale last.
    IgnoreList mixed({stateEntry({0, 0, 0}, "minecraft:lever", "open_bit"), blockEntry({1, 0, 2}, "minecraft:lever"),
                      everyLever, blockEntry({1, 0, 1}, "minecraft:stone")});
    auto order = listOrder(mixed, &structure, counts, Placement{}, {1, 0, 2});
    check(order.size() == 4 && order[0] == everyLever && order[1] == blockEntry({1, 0, 2}, "minecraft:lever")
              && order[3] == blockEntry({1, 0, 1}, "minecraft:stone"),
          "the Ignored list shows every-same-block entries, then the nearest cells, stale ones last");

    // The entry follows the placement: turned and mirrored, the world cell
    // holding it maps back to the same structure cell.
    Size size{2, 1, 3};
    for (int rotation = 0; rotation < 4; ++rotation)
        for (auto mirror : {Mirror::None, Mirror::X, Mirror::Z}) {
            Placement placement{{100, 64, -20}, rotation, mirror};
            auto world = ignoredWorldCell(lever, size, placement);
            auto local = toLocal(size, placement, world);
            check(local && *local == lever.cell && one.touches(*local, "minecraft:lever"),
                  "an ignored cell moves with the placement's turn and mirror");
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
    set.placements[0].ignored = IgnoreList({lever, direction, whole, everyLever});
    auto back = decodePlacements(encodePlacements(set));
    check(back.placements[0].ignored == set.placements[0].ignored, "every kind round-trips");
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
    auto perState = decodePlacements(R"({"version":1,"placements":[{"file":"a.mcstructure",
        "ignored":["1,0,2 minecraft:lever open_bit", 5, "broken", "1,0,2 minecraft:lever open_bit"]}]})");
    check(perState.placements.size() == 1 && perState.placements[0].ignored.size() == 1
              && perState.placements[0].ignored.entries()[0] == lever,
          "the per-state build's entries load as one state of a cell; unreadable and repeated ones are dropped");
    auto wrongType = decodePlacements(R"({"version":1,"placements":[{"file":"a.mcstructure","ignored":"x"}]})");
    check(wrongType.placements.size() == 1 && wrongType.placements[0].ignored.empty(), "an ignored key of another type is skipped");

    // The largest document still reads back: every placement full, longest names.
    PlacementSet largest;
    for (size_t p = 0; p < maxPlacements; ++p) {
        SavedPlacement big;
        big.name = std::string(maxPlacementName, 'n');
        big.file = std::string(200, 'f') + ".mcstructure";
        std::vector<IgnoreEntry> entries;
        for (int i = 0; i < static_cast<int>(maxIgnored); ++i)
            entries.push_back(stateEntry({10000 + i, 10000, 10000}, std::string(maxIgnoredBlockName, 'b'), std::string(maxIgnoredStateName, 's')));
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

#include "features/schematic/CheckIgnores.h"
#include "features/schematic/PlacementStore.h"
#include "features/schematic/Verification.h"
#include <algorithm>
#include <string>
void check(bool, char const*);
void checkIgnoresTests() {
    using namespace lamium::schematic;
    auto lever = cellItemEntry({1, 0, 2}, "minecraft:lever", "open_bit"); // the per-state build's form
    auto place = placeEntry({1, 0, 2}, "minecraft:lever");
    auto ruleOpen = ruleEntry("minecraft:lever", "open_bit");
    auto ruleDirection = ruleEntry("minecraft:lever", "lever_direction");

    IgnoreList list;
    check(list.empty() && list.fingerprint().empty(), "a placement starts with nothing ignored");
    check(list.add(place) == IgnoreList::Added::Added && list.size() == 1, "an entry is added");
    check(list.add(place) == IgnoreList::Added::Already && list.size() == 1, "the same place is added once");
    check(list.add(ruleEntry("minecraft:lever", "open bit")) == IgnoreList::Added::Invalid
              && list.add(placeEntry({-1, 0, 0}, "minecraft:lever")) == IgnoreList::Added::Invalid
              && list.add(placeEntry({0, 0, 0}, std::string(maxIgnoredBlockName + 1, 'a'))) == IgnoreList::Added::Invalid
              && list.add({IgnoreKind::Place, {0, 0, 0}, "minecraft:lever", "open_bit"}) == IgnoreList::Added::Invalid
              && list.add({IgnoreKind::Rule, {1, 0, 0}, "minecraft:lever", "open_bit"}) == IgnoreList::Added::Invalid
              && list.add(ruleEntry("minecraft:lever", "")) == IgnoreList::Added::Invalid,
          "names with spaces, negative cells, overlong names and malformed kinds are refused");
    auto copy = list;
    check(list.add(ruleOpen) == IgnoreList::Added::Added && copy.size() == 1 && list.size() == 2,
          "a copy is not changed by a later add");
    check(list.place({1, 0, 2}, "minecraft:lever") && !list.place({1, 0, 2}, "minecraft:stone") && !list.place({0, 0, 0}, "minecraft:lever"),
          "a place names the block of one cell");
    check(list.ruled("minecraft:lever", "open_bit") && !list.ruled("minecraft:lever", "lever_direction")
              && !list.ruled("minecraft:stone_button", "open_bit"),
          "a rule names one item of one block type");
    check(list.touches({7, 7, 7}, "minecraft:lever") && !list.touches({7, 7, 7}, "minecraft:stone") && list.touches({1, 0, 2}, "minecraft:lever"),
          "a cell is touched by its own place or by a rule for its block");
    auto before = list.fingerprint();
    check(list.remove(ruleOpen) && !list.remove(ruleOpen) && list.size() == 1 && list.fingerprint() != before,
          "removing one entry changes the fingerprint");
    list.clear();
    check(list.empty(), "everything can be undone at once");

    // The old one-item cell entry: kept, never applied, never widened.
    IgnoreList legacy({lever});
    check(legacy.size() == 1 && !legacy.place({1, 0, 2}, "minecraft:lever") && !legacy.ruled("minecraft:lever", "open_bit")
              && !legacy.touches({1, 0, 2}, "minecraft:lever"),
          "a one-item cell entry from the per-state build applies nowhere");
    IgnoreList kinds({place, lever, ruleOpen, ruleDirection});
    check(kinds.rules("minecraft:lever") == std::vector<std::string>{"lever_direction", "open_bit"} && kinds.rules("minecraft:stone").empty(),
          "a block's rules are listed");
    auto removed = kinds.removeIf([](IgnoreEntry const& e) { return e.kind != IgnoreKind::Rule && e.cell == Point{1, 0, 2}; });
    check(removed == 2 && kinds.size() == 2 && kinds.ruled("minecraft:lever", "open_bit"), "removing a cell's entries keeps the rules");

    // The cap: one at a time and all or nothing, places and rules together.
    IgnoreList full;
    for (int i = 0; i < static_cast<int>(maxIgnored) - 1; ++i) full.add(placeEntry({i, 0, 0}, "minecraft:lever"));
    full.add(ruleOpen);
    check(full.size() == maxIgnored && full.add(placeEntry({999, 0, 0}, "minecraft:lever")) == IgnoreList::Added::Full
              && full.add(ruleDirection) == IgnoreList::Added::Full,
          "the number of entries is capped across kinds");
    IgnoreList nearly;
    for (int i = 0; i < 62; ++i) nearly.add(placeEntry({i, 0, 0}, "minecraft:stone"));
    auto beforeBulk = nearly;
    std::vector<IgnoreEntry> three{placeEntry({100, 0, 0}, "minecraft:stone"), placeEntry({101, 0, 0}, "minecraft:stone"),
                                   placeEntry({102, 0, 0}, "minecraft:stone")};
    check(nearly.addAll(three) == IgnoreList::Added::Full && nearly == beforeBulk, "a bulk add over the cap changes nothing");
    std::vector<IgnoreEntry> twoNew{placeEntry({0, 0, 0}, "minecraft:stone"), placeEntry({100, 0, 0}, "minecraft:stone"),
                                    placeEntry({101, 0, 0}, "minecraft:stone")};
    check(nearly.addAll(twoNew) == IgnoreList::Added::Added && nearly.size() == maxIgnored,
          "entries already there do not count against the cap");

    // Saved form.
    check(encodeIgnored(lever) == "1,0,2 minecraft:lever open_bit" && encodeIgnored(place) == "1,0,2 minecraft:lever"
              && encodeIgnored(ruleOpen) == "* minecraft:lever open_bit"
              && encodeIgnored(ruleEntry("minecraft:oak_sign", "#FrontText.Text")) == "* minecraft:oak_sign #FrontText.Text",
          "each kind is saved as one string, a block entity item too");
    check(decodeIgnored("1,0,2 minecraft:lever open_bit") == lever && decodeIgnored("1,0,2 minecraft:lever") == place
              && decodeIgnored("* minecraft:lever open_bit") == ruleOpen && decodeIgnored("0,0,0 minecraft:air") == placeEntry({0, 0, 0}, "minecraft:air"),
          "and read back; the per-state build's form stays a one-item cell entry");
    check(!decodeIgnored("1,0 minecraft:lever open_bit") && !decodeIgnored("1,0,2,3 minecraft:lever open_bit")
              && !decodeIgnored("1,0,2") && !decodeIgnored("1,0,2 a b c") && !decodeIgnored("x,0,2 a b")
              && !decodeIgnored("-1,0,2 a b") && !decodeIgnored("* minecraft:lever") && !decodeIgnored("1,0,2  a"),
          "broken entries are skipped");

    // Block entity items: language-independent ids that never read as a state.
    StateDifference sign{"Front text", "a", "b", true, "#FrontText.Text"};
    check(itemId(sign) == "#FrontText.Text" && itemId(StateDifference{"open_bit", "0", "1"}) == "open_bit"
              && itemId(faceDifference(1, 2)) == openingItem,
          "a difference names its item: the state, or the field's id");
    check(entitycheck::itemId(entitycheck::fields[0]) == "#note"
              && std::all_of(std::begin(entitycheck::fields), std::end(entitycheck::fields), [](auto const& f) {
                     return IgnoreList::valid(ruleEntry("minecraft:x", entitycheck::itemId(f)));
                 }),
          "every compared field has an id a rule can save");

    // Staleness: an entry applies only while the file has the named block there.
    Structure structure;
    structure.size = {2, 1, 3};
    structure.palette.resize(4);
    structure.palette[0].name = "minecraft:air";
    structure.palette[1].name = "minecraft:lever";
    structure.palette[2].name = "minecraft:stone";
    structure.palette[3].name = "minecraft:noteblock";
    structure.palette[1].states.set("open_bit", nbt::Tag{std::int8_t{0}});
    structure.palette[1].states.set("lever_direction", nbt::Tag{std::string("east")});
    structure.blocks.assign(6, 0);
    structure.blocks[static_cast<size_t>(structure.cell(1, 0, 2))] = 1;
    structure.blocks[static_cast<size_t>(structure.cell(0, 0, 0))] = 1;
    structure.blocks[static_cast<size_t>(structure.cell(1, 0, 0))] = 3;
    nbt::Compound music;
    music.set("id", nbt::Tag{std::string("Music")});
    music.set("note", nbt::Tag{std::int8_t{3}});
    structure.blockEntities[structure.cell(1, 0, 0)] = music;
    auto counts = blockCounts(structure);
    check(sameBlocks(counts, "minecraft:lever") == 2 && sameBlocks(counts, "minecraft:air") == 0
              && sameBlocks(counts, "minecraft:stone") == 0,
          "blocks are counted per name, air and absent blocks not");
    check(ignoreApplies(place, structure, counts) && ignoreApplies(ruleOpen, structure, counts)
              && ignoreApplies(placeEntry({1, 0, 1}, "minecraft:air"), structure, counts),
          "entries on the named block apply, a place on the file's air (an extra) too");
    check(!ignoreApplies(placeEntry({0, 0, 2}, "minecraft:lever"), structure, counts)
              && !ignoreApplies(placeEntry({5, 0, 0}, "minecraft:lever"), structure, counts)
              && !ignoreApplies(ruleEntry("minecraft:stone", "facing"), structure, counts)
              && !ignoreApplies(lever, structure, counts),
          "one on another block, outside the structure, for a block the file lacks, or an old one-item entry is stale");
    structure.blocks[static_cast<size_t>(structure.cell(1, 0, 2))] = voidCell;
    check(!ignoreApplies(place, structure, counts), "one on a structure void is stale");
    structure.blocks[static_cast<size_t>(structure.cell(1, 0, 2))] = 1;

    // The items a rule can name, per block type.
    auto items = comparedItems(structure, false, false);
    check(items.size() == 2 && items["minecraft:lever"] == std::vector<std::string>{"open_bit", "lever_direction"}
              && items["minecraft:noteblock"] == std::vector<std::string>{"#note", std::string(instrumentItem)}
              && !items.contains("minecraft:air") && !items.contains("minecraft:stone"),
          "a block's items are its states and its compared block entity fields; air and unused blocks have none");
    check(!comparedItems(structure, false, true).contains("minecraft:noteblock"),
          "without block entity comparison a block with only entity items is not offered");
    check(inMaterialsOrder({{"minecraft:lever", "Lever"}, {"minecraft:noteblock", "Note Block"}, {"minecraft:x", "Apple"}},
                           {"Note Block", "Stone", "Lever"})
              == std::vector<std::string>{"minecraft:noteblock", "minecraft:lever", "minecraft:x"},
          "block types follow the Materials tab's order, the rest by name");

    // Verdicts.
    IgnoreList rules({ruleOpen});
    std::vector<StateDifference> openOnly{{"open_bit", "0", "1"}};
    check(judgeIgnores(CellState::State, openOnly, rules, {1, 0, 2}, "minecraft:lever") == IgnoreMark::Items && openOnly[0].ruled,
          "a cell whose only difference is ruled counts as correct, and says so");
    std::vector<StateDifference> two{{"lever_direction", "east", "west"}, {"open_bit", "0", "1"}};
    check(judgeIgnores(CellState::State, two, rules, {9, 0, 9}, "minecraft:lever") == IgnoreMark::None && !two[0].ruled && two[1].ruled,
          "another difference keeps the cell a mistake; the ruled one is still marked");
    std::vector<StateDifference> again{{"open_bit", "0", "1"}};
    check(judgeIgnores(CellState::State, again, rules, {1, 0, 2}, "minecraft:stone_button") == IgnoreMark::None,
          "another block type is still a mistake");
    check(judgeIgnores(CellState::Wrong, again, rules, {1, 0, 2}, "minecraft:lever") == IgnoreMark::None,
          "a rule never passes a wrong block");
    check(judgeIgnores(CellState::State, again, rules, {1, 0, 2}, "minecraft:lever", true) == IgnoreMark::None,
          "a cause the reading could not name keeps the mistake");
    std::vector<StateDifference> none;
    check(judgeIgnores(CellState::State, none, rules, {1, 0, 2}, "minecraft:lever") == IgnoreMark::None,
          "a mistake without named differences is kept");
    std::vector<StateDifference> signText{sign};
    check(judgeIgnores(CellState::State, signText, IgnoreList({ruleEntry("minecraft:oak_sign", "#FrontText.Text")}), {0, 0, 0},
                       "minecraft:oak_sign") == IgnoreMark::Items
              && judgeIgnores(CellState::State, signText, IgnoreList({ruleEntry("minecraft:oak_sign", "Front_text")}), {0, 0, 0},
                              "minecraft:oak_sign") == IgnoreMark::None,
          "a block entity difference is ruled by its id, not by its drawn label");
    std::vector<StateDifference> opening{faceDifference(2, 3)};
    check(judgeIgnores(CellState::State, opening, IgnoreList({ruleEntry("minecraft:lever", "facing")}), {1, 0, 2}, "minecraft:lever") == IgnoreMark::None,
          "a shulker box's opening is not the facing state");
    check(judgeIgnores(CellState::State, again, IgnoreList({lever}), {1, 0, 2}, "minecraft:lever") == IgnoreMark::None,
          "an old one-item cell entry passes nothing");
    IgnoreList placeOnly({place, placeEntry({1, 0, 1}, "minecraft:air")});
    std::vector<StateDifference> empty;
    check(judgeIgnores(CellState::Wrong, empty, placeOnly, {1, 0, 2}, "minecraft:lever") == IgnoreMark::Place
              && judgeIgnores(CellState::Missing, empty, placeOnly, {1, 0, 2}, "minecraft:lever") == IgnoreMark::Place
              && judgeIgnores(CellState::Correct, empty, placeOnly, {1, 0, 2}, "minecraft:lever") == IgnoreMark::Place
              && judgeIgnores(CellState::State, opening, placeOnly, {1, 0, 2}, "minecraft:lever") == IgnoreMark::Place,
          "a place takes the cell out whatever is there");
    check(judgeIgnores(CellState::Extra, empty, placeOnly, {1, 0, 1}, "minecraft:air") == IgnoreMark::Place,
          "an extra on the file's air can be ignored as a place");

    // Totals: a place is left out, ruled differences count as correct.
    Tally tally;
    tally.add(CellState::State, true, IgnoreMark::Items);
    tally.add(CellState::State, true, IgnoreMark::None);
    tally.add(CellState::Wrong, true, IgnoreMark::Place);
    tally.add(CellState::Missing, true, IgnoreMark::Place);
    tally.add(CellState::Correct, true, IgnoreMark::Place);
    tally.add(CellState::Extra, false, IgnoreMark::Place);
    tally.add(CellState::Missing, true, IgnoreMark::None);
    check(tally.correct == 1 && tally.state == 1 && tally.wrong == 0 && tally.extra == 0 && tally.missing == 1 && tally.total() == 3
              && tally.mistakes() == 1,
          "ruled differences count as matching, places are in no count");
    check(!countedState(CellState::Missing, IgnoreMark::Place) && countedState(CellState::State, IgnoreMark::Items) == CellState::Correct
              && countedState(CellState::Wrong, IgnoreMark::None) == CellState::Wrong,
          "what a cell adds to the counts follows its mark");

    // Readers of the rows: the nearest mistake skips ignored rows and missing
    // blocks, a partly ruled row is still found.
    auto row = [](CellState state, int x, IgnoreMark mark) {
        Mismatch m;
        m.state = state;
        m.position = {x, 0, 0};
        m.ignore = mark;
        return m;
    };
    std::vector<Mismatch> rows{row(CellState::State, 1, IgnoreMark::Items), row(CellState::Wrong, 2, IgnoreMark::Place),
                               row(CellState::Missing, 3, IgnoreMark::None), row(CellState::State, 6, IgnoreMark::None),
                               row(CellState::Extra, 9, IgnoreMark::None)};
    double squared = 0;
    auto const* nearest = nearestMistake(rows, 0, 0, 0, &squared);
    check(nearest && nearest->position.x == 6 && squared > 40 && squared < 45,
          "the nearest mistake skips ignored rows and missing blocks");
    check(!countsAsMistake(rows[0]) && !countsAsMistake(rows[1]) && !countsAsMistake(rows[2]) && countsAsMistake(rows[3])
              && countsAsMistake(rows[4]),
          "only rows not ignored count as mistakes");
    std::vector<Mismatch> allIgnored{row(CellState::State, 1, IgnoreMark::Items), row(CellState::Wrong, 2, IgnoreMark::Place)};
    check(!nearestMistake(allIgnored, 0, 0, 0), "with every mistake ignored there is no nearest one");

    // The Check list's filters: All takes every row, extras sit with wrong blocks.
    check(listedIn(CheckFilter::All, CellState::Missing) && listedIn(CheckFilter::All, CellState::Extra)
              && listedIn(CheckFilter::WrongBlock, CellState::Extra) && listedIn(CheckFilter::WrongBlock, CellState::Wrong)
              && !listedIn(CheckFilter::WrongBlock, CellState::State) && listedIn(CheckFilter::WrongState, CellState::State)
              && !listedIn(CheckFilter::WrongState, CellState::Missing) && listedIn(CheckFilter::NotPlaced, CellState::Missing)
              && !listedIn(CheckFilter::NotPlaced, CellState::Wrong),
          "each filter lists its kinds; All includes not placed");

    // Several selected rows: places to add and to take back.
    auto checkRow = [](Point local, std::string block, std::vector<StateDifference> states) {
        Mismatch m;
        m.state = states.empty() ? CellState::Wrong : CellState::State;
        m.local = local;
        m.fileBlock = std::move(block);
        m.identifier = m.fileBlock;
        m.states = std::move(states);
        return m;
    };
    auto a = checkRow({0, 0, 0}, "minecraft:lever", {{"open_bit", "0", "1"}});
    auto b = checkRow({1, 0, 0}, "minecraft:lever", {{"open_bit", "0", "1"}, {"lever_direction", "east", "west"}});
    auto d = checkRow({3, 0, 0}, "minecraft:stone", {});
    auto extra = checkRow({1, 0, 1}, "minecraft:air", {});
    extra.state = CellState::Extra;
    Mismatch entity;
    entity.entity = true;
    std::vector<Mismatch const*> picked{&a, &b, &d, &extra, &entity};
    IgnoreList chosen({placeEntry({3, 0, 0}, "minecraft:stone"), ruleOpen});
    auto adds = placesToAdd(picked, chosen);
    check(adds.size() == 3 && std::find(adds.begin(), adds.end(), placeEntry({3, 0, 0}, "minecraft:stone")) == adds.end()
              && std::find(adds.begin(), adds.end(), placeEntry({1, 0, 1}, "minecraft:air")) != adds.end(),
          "ignoring places adds the rows not yet ignored, an extra too, never an entity");
    auto drops = placesToRemove(picked, chosen);
    check(drops.size() == 1 && drops[0] == placeEntry({3, 0, 0}, "minecraft:stone"),
          "stopping takes the rows' places and leaves rules");

    // Right after an entries change the rows carry the last pass's verdicts;
    // the Check tab tags them by the live entries.
    auto lastPass = checkRow({0, 0, 0}, "minecraft:lever", {{"open_bit", "0", "1"}});
    check(liveMark(lastPass, IgnoreList({lever})) == IgnoreMark::None && liveMark(lastPass, rules) == IgnoreMark::Items
              && liveMark(lastPass, IgnoreList({placeEntry({0, 0, 0}, "minecraft:lever")})) == IgnoreMark::Place
              && liveMark(b, rules) == IgnoreMark::None,
          "a row is tagged by the entries as they are now; partly ruled rows carry no tag");
    check(differingRows({a, b, d}, "minecraft:lever", "open_bit") == 2 && differingRows({a, b, d}, "minecraft:lever", "lever_direction") == 1,
          "how many rows of a block differ in an item");

    // The ignore view's list.
    IgnoreList viewed({placeEntry({0, 0, 0}, "minecraft:lever"), place, lever, placeEntry({0, 0, 2}, "minecraft:lever"), ruleOpen,
                       ruleEntry("minecraft:torch", "torch_facing_direction"), ruleEntry("minecraft:stone", "facing")});
    auto view = ignoreRows(viewed, &structure, counts, {"minecraft:noteblock", "minecraft:lever"}, Placement{}, {1, 0, 2});
    using K = IgnoreRow::Kind;
    check(view.size() == 10 && view[0].kind == K::PlacesHeading && view[1].entry == place && !view[1].stale
              && view[2].entry == placeEntry({0, 0, 0}, "minecraft:lever") && view[3].stale && view[4].stale
              && view[5].kind == K::BlocksHeading && view[6].block == "minecraft:noteblock" && view[6].rules.empty()
              && view[7].block == "minecraft:lever" && view[7].rules == std::vector<std::string>{"open_bit"}
              && view[8].kind == K::StaleBlock && view[9].kind == K::StaleBlock,
          "the ignore view lists places nearest first and stale last, then the blocks with their rules, then gone blocks");
    auto emptyView = ignoreRows({}, &structure, counts, {"minecraft:lever"}, Placement{}, {});
    check(emptyView.size() == 4 && emptyView[1].kind == K::NoPlaces && !emptyView[1].selectable() && emptyView[3].selectable(),
          "without places it says so");
    Verification outdated;
    outdated.placement = 0;
    outdated.complete = true;
    check(outdated.ready(0), "a finished pass is ready");
    outdated.outdated = true;
    check(!outdated.ready(0), "after an entries change the last pass is not ready: its counts still hold the ignored cells");

    // The entry follows the placement: turned and mirrored, the world cell
    // holding it maps back to the same structure cell.
    Size size{2, 1, 3};
    for (int rotation = 0; rotation < 4; ++rotation)
        for (auto mirror : {Mirror::None, Mirror::X, Mirror::Z}) {
            Placement placement{{100, 64, -20}, rotation, mirror};
            auto world = ignoredWorldCell(place, size, placement);
            auto local = toLocal(size, placement, world);
            check(local && *local == place.cell && IgnoreList({place}).touches(*local, "minecraft:lever"),
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
    set.placements[0].ignored = IgnoreList({lever, place, ruleOpen, ruleDirection, placeEntry({0, 0, 0}, "minecraft:air")});
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
          "the per-state build's entries load unchanged as one-item cell entries; unreadable and repeated ones are dropped");
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
            entries.push_back(cellItemEntry({10000 + i, 10000, 10000}, std::string(maxIgnoredBlockName, 'b'), std::string(maxIgnoredStateName, 's')));
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

// Checks for the semantic ordering: sections, Shulker Boxes, custom names,
// enchantments and durability. Keys are built the way the game-side
// classifier builds them, but from synthetic data; what these tests prove is
// the comparator, not how the game reports item state.

#include "features/inventory/sort/SortKey.h"
#include "features/inventory/sort/SortPlanner.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using namespace lamium::inventory::sort;

namespace {

int gFailures = 0;

void check(bool ok, char const* what, int line) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL (line %d): %s\n", line, what);
    }
}

#define CHECK(expr) check((expr), #expr, __LINE__)

// Enchantment ids as the game numbers them (see Enchant::Type).
constexpr int kSharpness  = 9;
constexpr int kEfficiency = 15;
constexpr int kSilkTouch  = 16;
constexpr int kUnbreaking = 17;
constexpr int kFortune    = 18;
constexpr int kMending    = 26;

SortKey item(Section section, int creativeIndex, std::string const& typeName) {
    SortKey k;
    k.section       = section;
    k.creativeIndex = creativeIndex;
    k.typeName      = typeName;
    return k;
}

SortKey pickaxe(int damage = 0, std::vector<Enchantment> ench = {}, std::string const& name = "") {
    auto k         = item(Section::Equipment, 590060, "minecraft:diamond_pickaxe");
    k.enchantments = canonicalEnchantments(std::move(ench));
    k.variantRank  = k.enchantments.empty() ? 1 : 0;
    k.name         = normalizeName(name);
    k.nameRank     = k.name.empty() ? 1 : 0;
    k.damage       = damage;
    return k;
}

SortKey book(std::vector<Enchantment> ench) {
    auto k         = item(Section::Items, 440861, "minecraft:enchanted_book");
    k.enchantments = canonicalEnchantments(std::move(ench));
    k.variantRank  = 0;
    return k;
}

// A Shulker Box key the way the classifier builds it: contents become a
// signature, the colour moves to `tail`, filled boxes get index 0.
SortKey shulker(std::string const& colourId, std::vector<ContentEntry> contents, std::string const& name = "") {
    SortKey k;
    k.section       = Section::ShulkerBox;
    k.typeName      = "shulker_box";
    k.tail          = colourId;
    k.variantRank   = 0;
    k.contents      = contentSignature(std::move(contents));
    k.creativeIndex = k.contents.empty() ? 1 : 0;
    k.name          = normalizeName(name);
    k.nameRank      = k.name.empty() ? 1 : 0;
    return k;
}

ContentEntry cobble(int count) {
    return ContentEntry{item(Section::Construction, 130383, "minecraft:cobblestone"), count};
}
ContentEntry stone(int count) { return ContentEntry{item(Section::Nature, 270603, "minecraft:stone"), count}; }
ContentEntry egg(int count) { return ContentEntry{item(Section::Nature, 440861, "minecraft:egg"), count}; }
// An inner tool with the same variant state an inventory item would carry.
ContentEntry innerPickaxe(int damage = 0, std::vector<Enchantment> ench = {}, std::string const& name = "") {
    return ContentEntry{pickaxe(damage, std::move(ench), name), 1};
}

bool before(SortKey const& a, SortKey const& b) { return compareKeys(a, b) < 0; }
bool same(SortKey const& a, SortKey const& b) { return compareKeys(a, b) == 0; }

std::vector<SortKey> sortedKeys(std::vector<SortKey> keys) {
    std::stable_sort(keys.begin(), keys.end(), [](SortKey const& a, SortKey const& b) { return before(a, b); });
    return keys;
}

// Runs a region of unstackable items through the planner and returns the
// resulting type names, to prove the planner honours the key order.
std::vector<std::string> plannedOrder(std::vector<SortKey> const& keys) {
    std::vector<SlotStack> slots;
    for (size_t i = 0; i < keys.size(); ++i) {
        SlotStack s;
        s.count        = 1;
        s.maxStackSize = 1;
        s.group        = static_cast<int>(i);
        s.key          = keys[i];
        slots.push_back(s);
    }
    slots.resize(27);
    auto const               plan = planSort(slots);
    std::vector<std::string> out;
    for (auto const& s : plan.expected) {
        if (!s.empty()) out.push_back(s.key.tail.empty() ? s.key.typeName : s.key.tail);
    }
    return out;
}

// --- top-level sections ----------------------------------------------------

void testTopLevelSections() {
    // Deliberately mixed and shuffled: a Nature block, a sword, a Shulker
    // Box, a misc item, an unknown add-on item, a Construction block, a
    // pickaxe.
    auto const                     order = plannedOrder({
        item(Section::Nature, 250578, "minecraft:dirt"),
        item(Section::Equipment, 561039, "minecraft:diamond_sword"),
        shulker("minecraft:red_shulker_box", {cobble(3)}),
        item(Section::Items, 400001, "minecraft:string"),
        item(Section::Unknown, INT_MAX, "addon:widget"),
        item(Section::Construction, 130383, "minecraft:cobblestone"),
        item(Section::Equipment, 590060, "minecraft:diamond_pickaxe"),
    });
    std::vector<std::string> const expected{
        "minecraft:red_shulker_box",
        "minecraft:diamond_sword",
        "minecraft:diamond_pickaxe",
        "minecraft:string",
        "minecraft:cobblestone",
        "minecraft:dirt",
        "addon:widget",
    };
    CHECK(order == expected);
}

void testEquipmentDoesNotSplitBlocks() {
    auto const order = plannedOrder({
        item(Section::Construction, 130383, "minecraft:cobblestone"),
        item(Section::Equipment, 590060, "minecraft:diamond_pickaxe"),
        item(Section::Nature, 250578, "minecraft:dirt"),
        item(Section::Construction, 100010, "minecraft:oak_planks"),
        item(Section::Nature, 270603, "minecraft:stone"),
    });
    // All blocks are contiguous at the tail, Construction before Nature.
    std::vector<std::string> const expected{
        "minecraft:diamond_pickaxe",
        "minecraft:oak_planks",
        "minecraft:cobblestone",
        "minecraft:dirt",
        "minecraft:stone",
    };
    CHECK(order == expected);
}

void testUnknownItemsAreStableAndLast() {
    auto const a = item(Section::Unknown, INT_MAX, "addon:zeta");
    auto const b = item(Section::Unknown, INT_MAX, "addon:alpha");
    CHECK(before(b, a)); // identifier order among unknowns
    CHECK(before(item(Section::Nature, INT_MAX - 1, "minecraft:x"), b));
    CHECK(same(a, a));
}

// --- Shulker Boxes ---------------------------------------------------------

void testEveryShulkerBeforeNonShulkers() {
    auto const emptyBox = shulker("minecraft:undyed_shulker_box", {});
    for (auto const& other : {
             item(Section::Equipment, 1, "minecraft:diamond_sword"),
             item(Section::Items, 1, "minecraft:string"),
             item(Section::Construction, 1, "minecraft:cobblestone"),
             item(Section::Nature, 1, "minecraft:dirt"),
             item(Section::Unknown, INT_MAX, "addon:widget"),
         }) {
        CHECK(before(emptyBox, other));
    }
}

void testFilledBeforeEmptyNoInterleaving() {
    auto const filledA = shulker("minecraft:red_shulker_box", {cobble(32)});
    auto const emptyB  = shulker("minecraft:blue_shulker_box", {});
    auto const filledC = shulker("minecraft:yellow_shulker_box", {stone(64)});
    // input: filled -> empty -> filled
    auto const                     order = plannedOrder({filledA, emptyB, filledC});
    std::vector<std::string> const expected{
        "minecraft:red_shulker_box",
        "minecraft:yellow_shulker_box",
        "minecraft:blue_shulker_box",
    };
    CHECK(order == expected);
}

void testAllColoursStayInShulkerSection() {
    std::vector<SortKey> keys;
    for (auto const* colour :
         {"white",
          "orange",
          "magenta",
          "light_blue",
          "yellow",
          "lime",
          "pink",
          "gray",
          "light_gray",
          "cyan",
          "purple",
          "blue",
          "brown",
          "green",
          "red",
          "black",
          "undyed"}) {
        keys.push_back(shulker(std::string("minecraft:") + colour + "_shulker_box", {}));
    }
    keys.push_back(item(Section::Equipment, 1, "minecraft:diamond_sword"));
    keys.push_back(item(Section::Construction, 1, "minecraft:cobblestone"));
    auto const order = plannedOrder(keys);
    for (size_t i = 0; i < 17; ++i) CHECK(order[i].ends_with("shulker_box"));
    CHECK(order[17] == "minecraft:diamond_sword");
}

void testContentSignatureIgnoresSlotLayout() {
    // A: slot 0 cobble x32, slot 15 stone x64. B: slot 2 stone x64, slot 20
    // cobble x32. Slot positions are not part of the entries at all, and the
    // entry order must not matter either.
    auto const sigA = contentSignature({cobble(32), stone(64)});
    auto const sigB = contentSignature({stone(64), cobble(32)});
    CHECK(sigA == sigB);
    CHECK(!sigA.empty());
    // Split stacks of one kind are the same contents as one merged stack.
    CHECK(contentSignature({cobble(20), stone(64), cobble(12)}) == sigA);
    CHECK(same(
        shulker("minecraft:red_shulker_box", {cobble(32), stone(64)}),
        shulker("minecraft:red_shulker_box", {stone(64), cobble(32)})
    ));
}

void testDifferentContentsOrderDeterministically() {
    auto const cobbleBox = shulker("minecraft:red_shulker_box", {cobble(64)});
    auto const stoneBox  = shulker("minecraft:red_shulker_box", {stone(64)});
    auto const eggBox    = shulker("minecraft:red_shulker_box", {egg(16)});
    // Contents are ordered like an inventory: Construction before Nature,
    // then Creative order inside Nature.
    CHECK(before(cobbleBox, stoneBox));
    CHECK(before(stoneBox, eggBox));
    // More of the leading kind first.
    CHECK(before(shulker("minecraft:red_shulker_box", {cobble(64)}), shulker("minecraft:red_shulker_box", {cobble(3)}))
    );
    // Deterministic regardless of the order the comparison is made in.
    CHECK(!before(stoneBox, cobbleBox));
    CHECK(compareKeys(cobbleBox, stoneBox) == -compareKeys(stoneBox, cobbleBox));
}

void testShulkerInnerEnchantedVsPlainDiffer() {
    // Same base item inside, different variant state: the boxes must not
    // collapse to the same content key.
    auto const effBox = shulker(
        "minecraft:red_shulker_box",
        {
            innerPickaxe(0, {{kEfficiency, 5}}
             )
    }
    );
    auto const plainBox = shulker("minecraft:red_shulker_box", {innerPickaxe(0)});
    CHECK(!same(effBox, plainBox));
    CHECK(effBox.contents != plainBox.contents);
    // And they follow the inventory rule: the enchanted one leads.
    CHECK(before(effBox, plainBox));
}

void testShulkerInnerNamedVariants() {
    auto const namedBox = shulker("minecraft:red_shulker_box", {innerPickaxe(0, {}, "Old Faithful")});
    auto const plainBox = shulker("minecraft:red_shulker_box", {innerPickaxe(0)});
    CHECK(!same(namedBox, plainBox));
    CHECK(before(namedBox, plainBox)); // named inner item leads, as in an inventory
    // Formatting codes in the inner name do not matter.
    CHECK(same(namedBox, shulker("minecraft:red_shulker_box", {innerPickaxe(0, {}, "§bOld Faithful")})));
}

void testShulkerInnerDurabilityVariants() {
    auto const freshBox = shulker("minecraft:red_shulker_box", {innerPickaxe(10)});
    auto const wornBox  = shulker("minecraft:red_shulker_box", {innerPickaxe(1400)});
    CHECK(!same(freshBox, wornBox));
    CHECK(before(freshBox, wornBox));
    // The review example: Efficiency V + nearly full vs plain + nearly broken.
    auto const good = shulker(
        "minecraft:red_shulker_box",
        {
            innerPickaxe(10, {{kEfficiency, 5}}
             )
    }
    );
    auto const bad = shulker("minecraft:red_shulker_box", {innerPickaxe(1500)});
    CHECK(!same(good, bad));
    CHECK(before(good, bad));
}

void testShulkerInnerVariantsIgnoreSlotArrangement() {
    // Same three inner stacks, listed in different orders (i.e. different
    // internal slots) and with one kind split across two stacks.
    auto const a = shulker(
        "minecraft:red_shulker_box",
        {
            innerPickaxe(0, {{kEfficiency, 5}}
             ),
            cobble(20),
            stone(64),
            cobble(12)
    }
    );
    auto const b = shulker(
        "minecraft:red_shulker_box",
        {
            stone(64),
            cobble(32),
            innerPickaxe(0, {{kEfficiency, 5}}
             )
    }
    );
    CHECK(same(a, b));
    CHECK(a.contents == b.contents);
}

void testNamedShulkersOrderByReadableName() {
    auto const tools  = shulker("minecraft:red_shulker_box", {cobble(1)}, "§bTools");
    auto const blocks = shulker("minecraft:blue_shulker_box", {cobble(64)}, "Blocks");
    auto const plain  = shulker("minecraft:white_shulker_box", {cobble(64)});
    // Named boxes lead, in name order, ahead of the unnamed one.
    auto const                     order = plannedOrder({plain, tools, blocks});
    std::vector<std::string> const expected{
        "minecraft:blue_shulker_box",
        "minecraft:red_shulker_box",
        "minecraft:white_shulker_box",
    };
    CHECK(order == expected);
    CHECK(normalizeName("§bTools") == "tools");
    CHECK(normalizeName("  Blocks ") == "blocks");
    // An empty named box is still after every filled box.
    CHECK(before(plain, shulker("minecraft:red_shulker_box", {}, "AAA")));
}

void testHashDifferencesDoNotScrambleShulkers() {
    auto a   = shulker("minecraft:red_shulker_box", {cobble(32)});
    auto b   = shulker("minecraft:red_shulker_box", {cobble(32)});
    a.detail = "999999";
    b.detail = "111111";
    auto c   = shulker("minecraft:red_shulker_box", {stone(64)});
    c.detail = "000000";
    // The hash only breaks the tie between the two equal boxes; the box
    // with different contents keeps its semantic position.
    CHECK(before(b, a));
    CHECK(before(a, c) && before(b, c));
    CHECK(before(shulker("minecraft:blue_shulker_box", {cobble(32)}), a)); // colour before hash
}

void testEmptyShulkersOrderByNameThenColour() {
    auto const                     named = shulker("minecraft:white_shulker_box", {}, "Spare");
    auto const                     blue  = shulker("minecraft:blue_shulker_box", {});
    auto const                     red   = shulker("minecraft:red_shulker_box", {});
    auto const                     order = plannedOrder({red, blue, named});
    std::vector<std::string> const expected{
        "minecraft:white_shulker_box",
        "minecraft:blue_shulker_box",
        "minecraft:red_shulker_box",
    };
    CHECK(order == expected);
}

// --- same-item variants ----------------------------------------------------

void testEnchantedBeforeUnenchanted() {
    auto const plain     = pickaxe();
    auto const enchanted = pickaxe(
        0,
        {
            {kEfficiency, 3}
    }
    );
    CHECK(before(enchanted, plain));
    // Both still sit inside the pickaxe run, i.e. before the next item kind.
    auto next = item(Section::Equipment, 590061, "minecraft:diamond_shovel");
    CHECK(before(plain, next) && before(enchanted, next));
}

void testSameEnchantmentHigherLevelFirst() {
    CHECK(before(
        pickaxe(
            0,
            {
                {kEfficiency, 5}
    }
        ),
        pickaxe(0, {{kEfficiency, 3}})
    ));
    CHECK(before(
        pickaxe(
            0,
            {
                {kEfficiency, 3}
    }
        ),
        pickaxe(0, {{kEfficiency, 1}})
    ));
}

void testDifferentEnchantmentsGroupByIdentity() {
    auto const eff5 = pickaxe(
        0,
        {
            {kEfficiency, 5}
    }
    );
    auto const eff1 = pickaxe(
        0,
        {
            {kEfficiency, 1}
    }
    );
    auto const silk = pickaxe(
        0,
        {
            {kSilkTouch, 1}
    }
    );
    auto const fort3 = pickaxe(
        0,
        {
            {kFortune, 3}
    }
    );
    auto const order = plannedOrder({fort3, eff1, silk, eff5});
    // Efficiency (15) < Silk Touch (16) < Fortune (18); Efficiency V before I.
    std::vector<std::string> const names{
        "minecraft:diamond_pickaxe",
        "minecraft:diamond_pickaxe",
        "minecraft:diamond_pickaxe",
        "minecraft:diamond_pickaxe"
    };
    CHECK(order == names);
    auto const sorted = sortedKeys({fort3, eff1, silk, eff5});
    CHECK(sorted[0].enchantments == eff5.enchantments);
    CHECK(sorted[1].enchantments == eff1.enchantments);
    CHECK(sorted[2].enchantments == silk.enchantments);
    CHECK(sorted[3].enchantments == fort3.enchantments);
    // A summed "power" score must not decide: Fortune III (3) does not beat
    // Efficiency I (1) just because 3 > 1.
    CHECK(before(eff1, fort3));
}

void testMultipleEnchantmentsExtendTheirPrefix() {
    auto const eff = pickaxe(
        0,
        {
            {kEfficiency, 5}
    }
    );
    auto const effUnb = pickaxe(
        0,
        {
            {kUnbreaking, 3},
            {kEfficiency, 5}
    }
    );
    auto const effMend = pickaxe(
        0,
        {
            {kMending,    1},
            {kEfficiency, 5}
    }
    );
    auto const fortUnb = pickaxe(
        0,
        {
            {kFortune,    3},
            {kUnbreaking, 3}
    }
    );
    // Canonical order is by id, whatever order the NBT listed them in.
    CHECK(effUnb.enchantments[0].id == kEfficiency && effUnb.enchantments[1].id == kUnbreaking);
    // All Efficiency pickaxes stay together; the one with more enchantments
    // leads; Fortune pickaxes follow.
    auto const sorted = sortedKeys({fortUnb, eff, effMend, effUnb});
    CHECK(sorted[0].enchantments == effUnb.enchantments);
    CHECK(sorted[1].enchantments == effMend.enchantments);
    CHECK(sorted[2].enchantments == eff.enchantments);
    CHECK(sorted[3].enchantments == fortUnb.enchantments);
}

void testEnchantedBooks() {
    auto const sharp5 = book({
        {kSharpness, 5}
    });
    auto const sharp1 = book({
        {kSharpness, 1}
    });
    auto const mend   = book({
        {kMending, 1}
    });
    auto const effUnb = book({
        {kEfficiency, 4},
        {kUnbreaking, 3}
    });
    auto const sorted = sortedKeys({mend, sharp1, effUnb, sharp5});
    CHECK(sorted[0].enchantments == sharp5.enchantments); // Sharpness (9)
    CHECK(sorted[1].enchantments == sharp1.enchantments);
    CHECK(sorted[2].enchantments == effUnb.enchantments); // Efficiency (15)
    CHECK(sorted[3].enchantments == mend.enchantments);   // Mending (26)
    // Books stay with the enchanted_book kind, not mixed into pickaxes.
    CHECK(before(pickaxe(), sharp5));
}

void testCustomNamedVariantsLead() {
    auto const named  = pickaxe(500, {}, "Old Faithful");
    auto const namedB = pickaxe(
        0,
        {
            {kEfficiency, 5}
    },
        "Beacon Breaker"
    );
    auto const enchant = pickaxe(
        0,
        {
            {kEfficiency, 5}
    }
    );
    auto const plain  = pickaxe();
    auto const sorted = sortedKeys({plain, enchant, named, namedB});
    CHECK(sorted[0].name == "beacon breaker"); // named, by name
    CHECK(sorted[1].name == "old faithful");
    CHECK(sorted[2].enchantments == enchant.enchantments);           // then enchanted
    CHECK(sorted[3].name.empty() && sorted[3].enchantments.empty()); // then plain
    // Name comparison is textual and formatting-insensitive.
    CHECK(same(pickaxe(0, {}, "§lAlpha"), pickaxe(0, {}, "alpha")));
}

void testDurabilityBetterConditionFirst() {
    auto const fresh  = pickaxe(0);
    auto const used   = pickaxe(600);
    auto const nearly = pickaxe(1500);
    auto const sorted = sortedKeys({nearly, fresh, used});
    CHECK(sorted[0].damage == 0 && sorted[1].damage == 600 && sorted[2].damage == 1500);
    // Same enchantment state, different durability: still better first, but
    // the enchantment decides before durability.
    auto const effWorn = pickaxe(
        1500,
        {
            {kEfficiency, 5}
    }
    );
    auto const effFresh = pickaxe(
        0,
        {
            {kEfficiency, 5}
    }
    );
    CHECK(before(effFresh, effWorn));
    CHECK(before(effWorn, fresh)); // enchanted (worn) still leads plain (fresh)
}

void testDetailIsLastResortOnly() {
    auto a = pickaxe(
        0,
        {
            {kEfficiency, 5}
    }
    );
    auto b = pickaxe(
        0,
        {
            {kEfficiency, 5}
    }
    );
    a.detail = "9";
    b.detail = "1";
    CHECK(before(b, a));
    // Any semantic field outranks it.
    a.damage = 1;
    CHECK(before(b, a));
    b.damage = 2;
    CHECK(before(a, b));
}

void testCanonicalEnchantmentsDedupes() {
    auto const list = canonicalEnchantments({
        {kUnbreaking, 3},
        {kEfficiency, 5},
        {kUnbreaking, 3}
    });
    CHECK(list.size() == 2);
    CHECK(list[0].id == kEfficiency && list[1].id == kUnbreaking);
}

} // namespace

int runSortKeyTests() {
    testTopLevelSections();
    testEquipmentDoesNotSplitBlocks();
    testUnknownItemsAreStableAndLast();
    testEveryShulkerBeforeNonShulkers();
    testFilledBeforeEmptyNoInterleaving();
    testAllColoursStayInShulkerSection();
    testContentSignatureIgnoresSlotLayout();
    testDifferentContentsOrderDeterministically();
    testShulkerInnerEnchantedVsPlainDiffer();
    testShulkerInnerNamedVariants();
    testShulkerInnerDurabilityVariants();
    testShulkerInnerVariantsIgnoreSlotArrangement();
    testNamedShulkersOrderByReadableName();
    testHashDifferencesDoNotScrambleShulkers();
    testEmptyShulkersOrderByNameThenColour();
    testEnchantedBeforeUnenchanted();
    testSameEnchantmentHigherLevelFirst();
    testDifferentEnchantmentsGroupByIdentity();
    testMultipleEnchantmentsExtendTheirPrefix();
    testEnchantedBooks();
    testCustomNamedVariantsLead();
    testDurabilityBetterConditionFirst();
    testDetailIsLastResortOnly();
    testCanonicalEnchantmentsDedupes();
    if (gFailures == 0) {
        std::printf("SortKeyTest: all checks passed\n");
    } else {
        std::printf("SortKeyTest: %d check(s) failed\n", gFailures);
    }
    return gFailures;
}

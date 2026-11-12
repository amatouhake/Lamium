#include "features/inventory/EnchantPlan.h"
#include "features/inventory/EnchantOrder.h"
#include "ui/EnchantLayout.h"
#include <cstdint>
#include <vector>
void check(bool, char const*);
namespace {
using namespace lamium::inventory::enchantPlan;
struct Piece { int value, work; bool item; };
// Every order of every pairing, with no sharing between branches: slow and
// obviously right, for comparing against the planner on small inputs.
int bruteForce(std::vector<Piece> pieces, int refuseAt) {
    if (pieces.size() == 1) return 0;
    int best = -1;
    for (std::size_t kept = 0; kept < pieces.size(); ++kept)
        for (std::size_t gone = 0; gone < pieces.size(); ++gone) {
            if (kept == gone || pieces[gone].item) continue;
            int const cost = pieces[gone].value + priorWorkPenalty(pieces[kept].work) + priorWorkPenalty(pieces[gone].work);
            if (refuseAt > 0 && cost >= refuseAt) continue;
            auto next = pieces;
            next[kept] = {pieces[kept].value + pieces[gone].value, std::max(pieces[kept].work, pieces[gone].work) + 1,
                          pieces[kept].item};
            next.erase(next.begin() + static_cast<std::ptrdiff_t>(gone));
            int const rest = bruteForce(next, refuseAt);
            if (rest >= 0 && (best < 0 || cost + rest < best)) best = cost + rest;
        }
    return best;
}
// The same search for the other goal: {anvil uses on the result, total}, the
// uses compared first. {-1, -1} when no order exists.
struct Least { int work, total; };
Least bruteLeastWork(std::vector<Piece> pieces, int refuseAt) {
    if (pieces.size() == 1) return {pieces[0].work, 0};
    Least best{-1, -1};
    for (std::size_t kept = 0; kept < pieces.size(); ++kept)
        for (std::size_t gone = 0; gone < pieces.size(); ++gone) {
            if (kept == gone || pieces[gone].item) continue;
            int const cost = pieces[gone].value + priorWorkPenalty(pieces[kept].work) + priorWorkPenalty(pieces[gone].work);
            if (refuseAt > 0 && cost >= refuseAt) continue;
            auto next = pieces;
            next[kept] = {pieces[kept].value + pieces[gone].value, std::max(pieces[kept].work, pieces[gone].work) + 1,
                          pieces[kept].item};
            next.erase(next.begin() + static_cast<std::ptrdiff_t>(gone));
            auto const rest = bruteLeastWork(next, refuseAt);
            if (rest.work < 0) continue;
            int const total = cost + rest.total;
            if (best.work < 0 || rest.work < best.work || (rest.work == best.work && total < best.total))
                best = {rest.work, total};
        }
    return best;
}
}
// The screen's edits (L-170): a sword's enchantments with the game's
// frequencies and the damage group excluding each other.
void enchantOrderTests() {
    using namespace lamium::inventory::enchantOrder;
    std::vector<Offer> offers{{9, 5, 30}, {10, 5, 10}, {11, 5, 10}, {12, 2, 10}, {13, 2, 3}, {14, 3, 3}, {17, 3, 10}, {26, 1, 3}, {28, 1, 1, true}};
    Conflicts conflicts{{{9, 10}, {9, 11}, {10, 11}, {22, 26}}};
    check(conflicts.between(10, 9) && conflicts.between(9, 10) && !conflicts.between(9, 12), "a conflict holds in either order");
    std::vector<Enchantment> wanted, none;
    check(toggle(wanted, offers[0], none, conflicts) == Change::Added && wanted == std::vector<Enchantment>{{9, 5}},
          "an enchantment turns on at its highest level");
    check(toggle(wanted, offers[1], none, conflicts) == Change::Blocked && wanted.size() == 1 && blocker(10, none, wanted, conflicts) == 9,
          "a conflicting one stays off and names what blocks it");
    check(step(wanted, offers[0], -1) && wanted[0].level == 4 && step(wanted, offers[0], 5) && wanted[0].level == 5
              && !step(wanted, offers[0], 1) && !step(wanted, offers[3], 1),
          "levels move within 1 and the highest, only for what is on");
    check(toggle(wanted, offers[0], none, conflicts) == Change::Removed && wanted.empty(), "a second press turns it off");
    std::vector<Enchantment> base{{17, 3}, {10, 4}};
    check(toggle(wanted, offers[6], base, conflicts) == Change::Fixed && toggle(wanted, offers[0], base, conflicts) == Change::Blocked
              && blocker(9, base, wanted, conflicts) == 10 && wanted.empty(),
          "what the item has is fixed and blocks its conflicts");
    {
        std::vector<Offer> many;
        std::vector<Enchantment> full;
        for (int i = 0; i < maxBooks + 1; ++i) many.push_back({100 + i, 1, 30});
        for (int i = 0; i < maxBooks; ++i) check(toggle(full, many[i], none, conflicts) == Change::Added, "nine books fit");
        check(toggle(full, many[maxBooks], none, conflicts) == Change::Full && static_cast<int>(full.size()) == maxBooks, "a tenth book does not");
    }
    auto all = allHighest(offers, none, conflicts);
    check(all == std::vector<Enchantment>{{9, 5}, {12, 2}, {13, 2}, {14, 3}, {17, 3}, {26, 1}},
          "everything at its highest takes the most common of a group and no curse");
    {
        // A mace: three of the damage group are equally common; density goes on the mace alone.
        std::vector<Offer> mace{{10, 5, 10, false, 4}, {11, 5, 10, false, 4}, {39, 5, 10, false, 1}, {40, 4, 3, false, 1}, {38, 3, 3, false, 1}};
        Conflicts group{{{10, 11}, {10, 39}, {10, 40}, {11, 39}, {11, 40}, {39, 40}}};
        check(allHighest(mace, none, group) == std::vector<Enchantment>{{39, 5}, {38, 3}},
              "equally common ones are decided by how particular they are to the item");
        // Boots: depth strider and frost walker tie on both, so the first stays.
        std::vector<Offer> boots{{7, 3, 3, false, 1}, {25, 2, 3, false, 1}};
        check(allHighest(boots, none, Conflicts{{{7, 25}}}) == std::vector<Enchantment>{{7, 3}}, "a full tie keeps the game's order");
    }
    {
        std::vector<Offer> boots{{0, 4, 30}, {1, 4, 10}, {2, 4, 10}, {3, 4, 3}, {4, 4, 10}, {28, 1, 1, true}, {17, 3, 10}, {27, 1, 1, true}};
        Conflicts armor{{{0, 1}, {0, 3}, {0, 4}, {1, 3}, {1, 4}, {3, 4}}};
        auto order = grouped(boots, armor);
        std::vector<int> types;
        for (auto const& offer : order) types.push_back(offer.type);
        check(types == std::vector<int>{0, 1, 3, 4, 2, 17, 28, 27}, "enchantments that exclude each other sit together, curses last");
    }
    {
        std::vector<Enchantment> low{{12, 2}, {9, 1}};
        check(step(low, offers[3], -1) && low[0].level == 1 && step(low, offers[3], -1) && low == std::vector<Enchantment>{{9, 1}},
              "a level below 1 turns the enchantment off");
        std::vector<Enchantment> chosen{{14, 3}, {17, 2}}, onItem{{17, 3}, {9, 7}, {99, 1}};
        release(chosen, onItem, offers);
        check(chosen == std::vector<Enchantment>{{17, 3}, {9, 5}, {14, 3}},
              "leaving the held state keeps what the item carries, ahead of the rest and within the highest level");
    }
    {
        // Unbreaking 2 on the item: + adds a book for 3, - from there drops it, toggling it is refused.
        std::vector<Enchantment> onItem{{17, 2}}, raising;
        check(!step(raising, offers[6], -1, 2) && step(raising, offers[6], 1, 2) && raising == std::vector<Enchantment>{{17, 3}}
                  && !step(raising, offers[6], 1, 2) && step(raising, offers[6], -1, 2) && raising.empty(),
              "a carried level goes up by a wanted book and comes back down to itself");
        check(!step(raising, offers[6], 1, 3), "a carried level at its highest has nowhere to go");
        raising = {{17, 3}, {12, 2}};
        auto made = inputs(raising, offers, 1, true, std::nullopt, onItem);
        check(made && (*made)[1].value == 3 && (*made)[1].onItem == 1 && (*made)[2].onItem < 0, "a raising book is worth the difference on the item");
        auto pruned = raising;
        prune(pruned, offers, std::vector<Enchantment>{{17, 3}}, conflicts);
        check(pruned == std::vector<Enchantment>{{12, 2}}, "a wanted level the item already has is dropped");
        auto highest = allHighest(offers, onItem, conflicts);
        check(has(highest, 17) && levelOf(highest, 17) == 3, "everything at its highest raises what the item carries");
        std::vector<Enchantment> leaving{{17, 3}};
        release(leaving, onItem, offers);
        check(leaving == std::vector<Enchantment>{{17, 3}}, "leaving the held state keeps the level it was being raised to");
    }
    {
        using lamium::inventory::enchantPlan::Step;
        lamium::inventory::enchantPlan::Plan made;
        made.steps = {Step{2u, 4u, 3, 1}, Step{1u, 6u, 9, 2}, Step{7u, 8u, 5, 3}};
        std::vector<unsigned> fresh{1u, 2u, 4u, 8u}, merged{1u, 6u, 8u}, onSword{7u, 8u}, finished{15u};
        check(nextStep(made, fresh) == 0 && nextStep(made, merged) == 1 && nextStep(made, onSword) == 2 && nextStep(made, finished) == -1,
              "the next step is the first whose parts are not yet in one stack");
        check(done(made.steps[0], onSword) && !done(made.steps[2], onSword) && atHand(6u, merged) && !atHand(6u, fresh),
              "a later result counts for the steps inside it");
        std::vector<unsigned> second{15u, 1u, 2u}, spare{15u, 2u};
        check(nextStep(made, progress(second, made)) == 0 && nextStep(made, progress(spare, made)) == -1
                  && progress(finished, made) == finished,
              "a finished item does not end the plan while another item to enchant is at hand; a spare book does not restart it");
        {
            auto start = follow(fresh, made), middle = follow(merged, made), end = follow(finished, made);
            std::vector<unsigned> noSword{2u, 4u, 8u}, noBook{1u, 2u, 8u};
            auto swordless = follow(noSword, made), bookless = follow(noBook, made);
            check(start.next == 0 && start.ready && start.have == 4 && start.need == 4 && start.done == std::vector<bool>{false, false, false}
                      && middle.next == 1 && middle.ready && middle.done == std::vector<bool>{true, false, false} && middle.have == 4,
                  "following a plan: the next step is ready when its two sides are at hand, and merged stacks still count as materials");
            check(end.next == -1 && !end.ready && end.have == 4 && swordless.next == 0 && swordless.ready && swordless.have == 3
                      && swordless.lacks(1u) && !swordless.lacks(2u) && !swordless.lacks(16u)
                      && bookless.next == 0 && !bookless.ready && bookless.lacks(4u),
                  "a missing input is named by its bit; a step whose side is missing is not ready");
        }
        {
            // Step 1 joins the item (1) and the merged book (6).
            std::vector<unsigned> slots{0u, 6u, 0u, 1u, 6u, 8u};
            auto both = nextStacks(made.steps[1], slots, 0u, 0u), placed = nextStacks(made.steps[1], slots, 1u, 0u),
                 wrong = nextStacks(made.steps[1], slots, 8u, 6u);
            check(both.left == 3 && both.right == 1 && placed.left == -1 && placed.right == 1 && wrong.left == 3 && wrong.right == -1,
                  "the next step's stacks are the first slots holding its sides, unless a side already lies in its anvil slot");
            // Steps: 0 joins 2+4 (cost 3), 1 joins 1+6 (cost 9), 2 joins 7+8 (cost 5).
            auto has = [](std::vector<unsigned> stacks) { return [stacks](unsigned side) { return std::ranges::find(stacks, side) != stacks.end(); }; };
            auto empty = anvilSay(made, 1, false, 0u, false, 0u, 0, has({1u, 6u})), half = anvilSay(made, 1, true, 1u, false, 0u, 0, has({6u}));
            check(empty.step == 1 && empty.left == SlotSay::Wanted && empty.right == SlotSay::Wanted && empty.result == ResultSay::None
                      && half.left == SlotSay::Right && half.right == SlotSay::Wanted,
                  "empty anvil slots ask for the next step's stacks while those are at hand; a placed one is right");
            auto pair = anvilSay(made, 1, true, 1u, true, 6u, 9, has({})), dear = anvilSay(made, 1, true, 1u, true, 6u, 12, has({}));
            auto swapped = anvilSay(made, 1, true, 6u, true, 1u, 9, has({})), other = anvilSay(made, 1, true, 2u, true, 4u, 3, has({}));
            auto stranger = anvilSay(made, 1, true, 0u, false, 0u, 0, has({6u})), lacking = anvilSay(made, 1, false, 0u, false, 0u, 0, has({6u}));
            check(pair.result == ResultSay::Match && pair.left == SlotSay::Right && pair.right == SlotSay::Right && dear.result == ResultSay::CostDiffers
                      && swapped.result == ResultSay::NotAStep && swapped.left == SlotSay::Wrong && swapped.right == SlotSay::Wrong,
                  "a placed pair is held against the step it makes: its cost, or that it is none, the two the wrong way round included");
            check(other.step == 0 && other.result == ResultSay::Match && other.left == SlotSay::Right && stranger.left == SlotSay::Wrong
                      && stranger.right == SlotSay::Wanted && lacking.left == SlotSay::None && anvilSay(made, -1, false, 0u, false, 0u, 0, has({1u})).left == SlotSay::None,
                  "another step's pair counts as that step; a stack of no plan is wrong; nothing is asked for what is not at hand or when the plan is done");
            auto box = gridSlot(241, 177, 11);
            check(box.x == 241 + 2 * 18 && box.y == 177 + 18 && box.w == 18 && box.h == 18 && gridSlot(241, 177, -1).w == 0 && gridSlot(241, 234, 8).x == 385,
                  "a slot's box follows from its grid's corner, nine to a row");
        }
        // An item with enchantments 1 and 3 on it (mask 11) is nothing this plan makes: steps join 2+4, then 1+6, then 7+8.
        std::vector<unsigned> earlier{11u, 2u, 4u}, earlierOnly{11u};
        check(nextStep(made, progress(earlier, made)) == 0 && !atHand(1u, progress(earlierOnly, made)) && progress(onSword, made) == onSword
                  && progress(merged, made) == merged,
              "an item the plan never makes is not counted as part of it; the plan's own results are");
    }
    auto allOnItem = allHighest(offers, base, conflicts);
    check(!has(allOnItem, 17) && !has(allOnItem, 9) && has(allOnItem, 12), "it skips what the item has and what that blocks");
    std::vector<Enchantment> stale{{9, 5}, {12, 3}, {99, 1}, {17, 2}, {14, 3}};
    prune(stale, offers, base, conflicts);
    check(stale == std::vector<Enchantment>{{14, 3}}, "pruning drops the blocked, the too high, the unknown and the item's own");
    std::vector<Enchantment> pick{{9, 5}, {14, 3}, {28, 1}};
    auto made = inputs(pick, offers, 2);
    check(made && made->size() == 4 && (*made)[0].item && (*made)[0].work == 2 && (*made)[1].value == 5 && (*made)[2].value == 6
              && (*made)[3].value == 4 && !(*made)[1].item && (*made)[1].work == 0,
          "inputs are the item with its uses and a fresh book per enchantment at the book cost");
    std::vector<Offer> odd{{9, 5, 7}};
    check(!inputs(std::vector<Enchantment>{{9, 5}}, odd, 0) && !inputs(std::vector<Enchantment>{{50, 1}}, offers, 0),
          "an unknown frequency or enchantment gives no inputs");
    {
        // A held book with looting 3 and mending: 6 + 2 as the sacrificed side, and it may be sacrificed.
        std::vector<Enchantment> onBook{{14, 3}, {26, 1}};
        auto value = bookValue(onBook, offers);
        auto withBook = inputs(std::vector<Enchantment>{{9, 5}}, offers, 1, true, value);
        check(value == 8 && withBook && withBook->size() == 2 && (*withBook)[0].value == 8 && (*withBook)[0].work == 1 && !(*withBook)[0].item,
              "a held book is input 0 with its value and uses, free to be either side");
        check(!bookValue(std::vector<Enchantment>{{99, 1}}, offers) && bookValue(none, offers) == 0, "a book with an unknown enchantment has no value");
    }
}
void enchantLayoutTests() {
    using lamium::ui::EnchantLayout;
    using lamium::ui::SettingsTable;
    using Zone = EnchantLayout::Zone;
    auto table = SettingsTable::fit(640, 360, 60, 0, 13);
    auto layout = EnchantLayout::fit(table, 20, true, 12, 0, 9, 0);
    check(layout.usable() && layout.dividerX > layout.left && layout.dividerX < layout.left + layout.width,
          "the item column sits left of the steps");
    check(layout.kindColumns >= 6 && layout.kindY(layout.kindColumns) > layout.kindY(0) && layout.kindX(layout.kindColumns) == layout.kindX(0),
          "kinds wrap into rows of whole cells");
    check(layout.hit(layout.kindX(3) + 2, layout.kindY(3) + 2).zone == Zone::Kind && layout.hit(layout.kindX(3) + 2, layout.kindY(3) + 2).index == 3
              && layout.hit(layout.kindX(19) + 2, layout.kindY(19) + 2).index == 19,
          "a kind cell is hit by its index");
    check(layout.hit(layout.kindX(20) + 2, layout.kindY(20) + 2).zone == Zone::Held && layout.heldIndex() == 20,
          "the main-hand item's cell comes after the last kind");
    check(layout.hit(layout.setAllX + 2, layout.setsY + 2).zone == Zone::SetAll && layout.hit(layout.setLastX + 2, layout.setsY + 2).zone == Zone::SetLast
              && layout.hit(layout.setClearX + 2, layout.setsY + 2).zone == Zone::SetClear
              && layout.setClearX + layout.setNarrow <= layout.dividerX,
          "the three set buttons sit side by side inside the column");
    float const rowY = layout.rows.listRowY(1) + 2;
    check(layout.hit(layout.left + 30, rowY).zone == Zone::Row && layout.hit(layout.left + 30, rowY).index == 1
              && layout.hit(layout.minusX() + 1, rowY).zone == Zone::RowMinus && layout.hit(layout.plusX() + 1, rowY).zone == Zone::RowPlus,
          "a row is hit by its index, with its level buttons at the right");
    auto const& side = layout.side;
    check(side.usable() && side.rows.listCount == 18 && side.totalY > side.goalY && side.detailY > side.totalY && side.detailLines == 1
              && side.rows.rowsTop > side.detailY + 11 && side.rows.rowsTop + side.rows.listVisible * 14 <= side.footY
              && side.footY + lamium::ui::EnchantStepsLayout::footHeight == layout.footerTop,
          "the total and its detail sit under the goal, each step takes two list rows below them, and a last line closes the column");
    check(layout.hit(side.goalX() + 4, side.goalY + 2).zone == Zone::Goal && layout.hit(layout.backX + 2, layout.headTop + 2).zone == Zone::Back
              && layout.hit(side.left + 8, side.detailY + 2).zone == Zone::AtOnce
              && layout.hit(side.left + side.width - 8, side.detailY + 2).zone == Zone::Result,
          "the goal and Back take clicks; the two details are told apart for their help");
    {
        using lamium::ui::EnchantStepsLayout;
        // The anvil's panel: as tall as its plan needs, the detail on two lines where one does not hold it.
        float const tall = EnchantStepsLayout::neededHeight(4, 2);
        auto anvil = EnchantStepsLayout::fit(300, 20, 180, 20 + tall, 4, 0, 2);
        check(anvil.usable() && anvil.rows.listVisible == 8 && anvil.rows.listCount == 8 && anvil.detailLines == 2
                  && anvil.hit(310, anvil.detailY + 2) == EnchantStepsLayout::Zone::AtOnce
                  && anvil.hit(310, anvil.detailY + 13) == EnchantStepsLayout::Zone::Result
                  && anvil.goalX() >= anvil.left + 34 && anvil.goalX() + anvil.goalW() <= anvil.left + anvil.width,
              "a panel as tall as its plan shows every step without scrolling, with its detail on two lines");
        auto shortPanel = EnchantStepsLayout::fit(300, 20, 180, 20 + EnchantStepsLayout::neededHeight(2, 2), 6, 3, 2);
        check(shortPanel.rows.listVisible == 4 && shortPanel.rows.listCount == 12 && shortPanel.rows.listFirst == 3
                  && !EnchantStepsLayout::fit(300, 20, 60, 400, 4, 0, 1).usable() && !EnchantStepsLayout::fit(300, 20, 180, 70, 4, 0, 1).usable(),
              "a shorter panel scrolls its steps; one too narrow or too short is not laid out");
        auto wordy = EnchantStepsLayout::fit(300, 20, 180, 20 + EnchantStepsLayout::neededHeight(4, 2, 3), 4, 0, 2, 3);
        check(wordy.usable() && wordy.rows.listVisible == 8 && wordy.footLines == 3 && wordy.footY == anvil.footY
                  && wordy.bottom - wordy.footY == EnchantStepsLayout::footHeight + 2 * EnchantStepsLayout::footLine,
              "a last line that wraps takes its room below the steps, which stay where they were");
    }
    auto plain = EnchantLayout::fit(table, 20, false, 12, 0, 9, 0);
    check(plain.kindColumns == layout.kindColumns && plain.setsY == layout.setsY && plain.rows.rowsTop == layout.rows.rowsTop
              && plain.hit(plain.kindX(20) + 2, plain.kindY(20) + 2).zone == Zone::None,
          "without a main-hand item its place stays empty and nothing moves");
    check(layout.minusX() + EnchantLayout::stepWidth == layout.levelX() && layout.levelX() + EnchantLayout::levelWidth == layout.plusX()
              && layout.plusX() + EnchantLayout::stepWidth < layout.switchX() && layout.minusX() > layout.left + 60,
          "a row reads name, the level between its buttons, then the switch at the right edge");
    check(!EnchantLayout::fit(SettingsTable::fit(100, 100, 29, 0), 20, true, 12, 0, 9, 0).usable(), "too-small windows draw nothing");
    // The same column as a panel left of the anvil screen.
    auto panel = EnchantLayout::fitPanel(10, 20, 150, 240, 20, true, true, 12, 0);
    check(panel.usable() && panel.dividerX == 160 && panel.footerTop == 260 - EnchantLayout::statusHeight && panel.rows.listVisible >= 4
              && panel.rows.rowsTop + panel.rows.listVisible * 14 <= panel.footerTop,
          "the anvil's panel is the choosing column alone, with its status lines under the rows");
    check(panel.slotIndex() == 21 && panel.hit(panel.kindX(20) + 2, panel.kindY(20) + 2).zone == Zone::Held
              && panel.hit(panel.kindX(21) + 2, panel.kindY(21) + 2).zone == Zone::Slot,
          "the anvil slot's item gets a cell after the main hand's");
    auto empty = EnchantLayout::fitPanel(10, 20, 150, 240, 20, false, false, 12, 0);
    check(empty.setsY == panel.setsY && empty.rows.rowsTop == panel.rows.rowsTop && empty.hit(empty.kindX(21) + 2, empty.kindY(21) + 2).zone == Zone::None
              && empty.hit(empty.kindX(20) + 2, empty.kindY(20) + 2).zone == Zone::None,
          "without the items their places stay empty and nothing moves");
    float const panelRow = panel.rows.listRowY(1) + 2;
    check(panel.hit(panel.minusX() + 1, panelRow).zone == Zone::RowMinus && panel.hit(panel.plusX() + 1, panelRow).zone == Zone::RowPlus
              && panel.hit(panel.left + 20, panelRow).zone == Zone::Row && panel.hit(panel.left + 20, panel.footerTop + 3).zone == Zone::None
              && panel.minusX() > panel.left + 50,
          "the panel's rows take presses like the screen's, and its status lines take none");
    check(layout.slotIndex() == 21 && layout.hit(layout.kindX(21) + 2, layout.kindY(21) + 2).zone == Zone::None
              && !EnchantLayout::fitPanel(10, 20, 100, 240, 20, true, true, 12, 0).usable(),
          "the planner screen has no slot cell; a panel too narrow is not laid out");
}
void enchantPlanTests() {
    enchantOrderTests();
    enchantLayoutTests();
    check(priorWorkPenalty(0) == 0 && priorWorkPenalty(1) == 1 && priorWorkPenalty(3) == 7,
          "each earlier anvil use doubles the surcharge");
    check(priorUses(0) == 0 && priorUses(1) == 1 && priorUses(7) == 3 && priorUses(31) == 5,
          "a stored surcharge names the anvil uses behind it");
    check(!priorUses(2) && !priorUses(-1) && !priorUses(6), "a surcharge the rule cannot produce names no count");
    {
        std::vector<Input> one{{0, 0, true}};
        auto result = plan(one);
        check(result && result->steps.empty() && result->total == 0, "a lone item needs no step");
    }
    {
        std::vector<Input> inputs{{0, 0, true}, {1, 0, false}, {4, 0, false}};
        auto result = plan(inputs);
        check(result && result->total == 6 && result->steps.size() == 2, "two books go on one after the other");
        check(result->highest == 4 && result->steps[0].sacrifice == 4u && result->steps[0].cost == 4,
              "equal totals put the dearer book first, so no step is higher than it must be");
        check(result->steps[1] == Step{1u | 4u, 2u, 2, 2}, "the second step pays the item's one earlier use");
    }
    {
        std::vector<Input> inputs{{0, 0, true}, {1, 0, false}, {1, 0, false}, {1, 0, false}, {1, 0, false}};
        auto result = plan(inputs);
        check(result && result->total == 10 && result->steps.size() == 4,
              "four equal books cost 10 when paired up, not 15 one after the other");
        unsigned merged = 0;
        for (auto const& step : result->steps) {
            check(!(step.sacrifice & 1u), "the item is never the sacrificed side");
            check(!(step.target & step.sacrifice), "a step's two sides share no input");
            merged |= step.target | step.sacrifice;
        }
        check(merged == 31u && result->steps.back().target + result->steps.back().sacrifice == 31u,
              "the last step joins every input");
    }
    {
        std::vector<Input> inputs{{0, 0, true}, {30, 0, false}, {30, 0, false}};
        check(!plan(inputs, {31}), "no order exists when the second book must pay a surcharge over the limit");
        check(plan(inputs, {0}).has_value(), "without a limit the same inputs plan");
        std::vector<Input> near{{0, 0, true}, {20, 0, false}, {20, 0, false}};
        auto result = plan(near, {40});
        check(result && result->total == 41 && result->highest == 21, "a step just under the limit is allowed");
    }
    {
        std::vector<Input> used{{0, 2, true}, {3, 0, false}};
        auto result = plan(used);
        check(result && result->total == 6 && result->work == 3, "an item's earlier anvil uses are charged and counted");
    }
    {
        std::vector<Input> two{{0, 0, true}, {0, 0, true}}, none, many(maxInputs + 1), negative{{-1, 0, false}};
        check(!plan(two) && !plan(none) && !plan(many) && !plan(negative), "bad input sets are refused");
        std::vector<Input> books{{2, 0, false}, {5, 0, false}};
        auto result = plan(books);
        check(result && result->total == 2 && result->steps[0].sacrifice == 1u, "books alone merge onto the dearer one");
    }
    // The nine costs read from the anvil: sharpness, unbreaking, looting, fire
    // aspect, mending, knockback and vanishing as books, the last two as items.
    check(levelCost(30, true) == 1 && levelCost(10, true) == 1 && levelCost(3, true) == 2 && levelCost(1, true) == 4,
          "a book's level costs 1, 1, 2 or 4 by frequency");
    check(levelCost(30, false) == 1 && levelCost(10, false) == 2 && levelCost(3, false) == 4 && levelCost(1, false) == 8,
          "an item's level costs 1, 2, 4 or 8 by frequency");
    check(!levelCost(0, true) && !levelCost(5, false) && !levelCost(-1, true), "an unknown frequency has no cost");
    {
        // A sword with unbreaking 3 already on it; wanted: sharpness 5, looting 3, mending.
        std::vector<Enchantment> base{{17, 3}}, wanted{{9, 5}, {14, 3}, {26, 1}};
        std::vector<Enchantment> fresh{{17, 3}}, half{{9, 5}, {17, 3}}, other{{17, 3}, {12, 2}}, low{{17, 3}, {9, 4}};
        check(carried(true, fresh, base, wanted) == 1u, "the untouched item is input 0");
        check(carried(true, half, base, wanted) == (1u | 2u), "the item with one wanted enchantment carries that input too");
        check(!carried(true, other, base, wanted), "an item with an unplanned enchantment matches nothing");
        check(!carried(true, low, base, wanted), "a wanted enchantment at another level matches nothing");
        check(!carried(true, std::vector<Enchantment>{{9, 5}}, base, wanted), "an item without its base enchantments is another item");
        std::vector<Enchantment> book{{14, 3}}, merged{{14, 3}, {26, 1}}, blank, stray{{17, 3}};
        check(carried(false, book, base, wanted) == 4u && carried(false, merged, base, wanted) == (4u | 8u),
              "a book carries the wanted enchantments on it");
        check(!carried(false, blank, base, wanted) && !carried(false, stray, base, wanted),
              "an empty book and a book of a base enchantment match nothing");
        std::vector<Input> inputs{{0, 0, true}, {5, 0, false}, {6, 0, false}, {2, 0, false}};
        auto result = plan(inputs);
        check(result.has_value(), "the example plans");
        int found = 0;
        for (std::size_t i = 0; i < result->steps.size(); ++i)
            if (stepFor(*result, result->steps[i].target, result->steps[i].sacrifice) == static_cast<int>(i)) ++found;
        check(found == static_cast<int>(result->steps.size()), "every step is found by its two sides");
        check(stepFor(*result, result->steps[0].sacrifice, result->steps[0].target) == -1, "swapped sides are not that step");
        check(stepFor(*result, 2u, 1u) == -1, "the item on the sacrificed side is no step");
    }
    {
        // A sword with sharpness 3: a sharpness 5 book costs 5 onto a book but 2 onto the sword.
        std::vector<Input> raise{{0, 0, true}, {5, 0, false, 2}, {3, 0, false}};
        auto result = plan(raise);
        check(result && result->total == 6 && result->steps.size() == 2 && result->steps[0].cost == 3 && result->steps[1].cost == 3,
              "a raising book is charged the difference on the item: 3 for the plain book, then 2 + 1");
        std::vector<Input> merged{{0, 0, true}, {5, 0, false, 2}, {1, 0, false}, {1, 0, false}, {1, 0, false}};
        auto wide = plan(merged);
        int sum = 0;
        for (auto const& step : wide->steps) sum += step.cost;
        check(wide && sum == wide->total && wide->total == 11, "the listed costs still add up with a raising book among others");
        check(!plan(std::vector<Input>{{0, 0, true}, {2, 0, false, 3}}), "a value on the item above the plain one is refused");
        std::vector<Enchantment> had{{9, 3}, {17, 3}}, want{{9, 5}, {14, 3}};
        check(carried(true, std::vector<Enchantment>{{9, 3}, {17, 3}}, had, want) == 1u
                  && carried(true, std::vector<Enchantment>{{9, 5}, {17, 3}}, had, want) == (1u | 2u)
                  && carried(true, std::vector<Enchantment>{{9, 5}, {17, 3}, {14, 3}}, had, want) == (1u | 2u | 4u)
                  && !carried(true, std::vector<Enchantment>{{9, 5}}, had, want) && !carried(true, std::vector<Enchantment>{{9, 4}, {17, 3}}, had, want),
              "an item whose carried level was raised is still the item, with that book on it");
    }
    // Compare with the brute force over small pseudo-random sets.
    std::uint32_t seed = 12345;
    auto next = [&](int below) { seed = seed * 1664525u + 1013904223u; return static_cast<int>((seed >> 16) % below); };
    int goalsDiffer = 0;
    for (int round = 0; round < 300; ++round) {
        int const count = 1 + next(5);
        bool const withItem = next(4) != 0;
        int const refuseAt = next(3) == 0 ? 0 : 6 + next(30);
        std::vector<Input> inputs;
        std::vector<Piece> pieces;
        for (int i = 0; i < count; ++i) {
            Input input{next(9), next(3), withItem && i == 0};
            inputs.push_back(input);
            pieces.push_back({input.value, input.work, input.item});
        }
        auto result = plan(inputs, {refuseAt});
        int const expected = bruteForce(pieces, refuseAt);
        check((result ? result->total : -1) == expected, "the planner's total equals the exhaustive search");
        auto least = plan(inputs, {refuseAt, Goal::LeastPriorWork});
        auto const leastExpected = bruteLeastWork(pieces, refuseAt);
        check(least.has_value() == result.has_value(), "both goals agree on whether an order exists");
        check((least ? least->work : -1) == leastExpected.work && (least ? least->total : -1) == leastExpected.total,
              "the least-prior-work plan equals the exhaustive search");
        if (!result) continue;
        check(least->work <= result->work && least->total >= result->total, "each goal is at least as good on its own value");
        if (least->work < result->work) ++goalsDiffer;
        int leastSum = 0;
        for (auto const& step : least->steps) {
            leastSum += step.cost;
            check(refuseAt == 0 || step.cost < refuseAt, "no least-prior-work step reaches the limit");
        }
        check(leastSum == least->total && static_cast<int>(least->steps.size()) == count - 1 &&
                  (least->steps.empty() || least->steps.back().work == least->work),
              "the least-prior-work steps add up to the reported plan");
        int sum = 0, highest = 0;
        for (auto const& step : result->steps) {
            sum += step.cost;
            highest = std::max(highest, step.cost);
            check(refuseAt == 0 || step.cost < refuseAt, "no planned step reaches the limit");
        }
        check(sum == result->total && highest == result->highest && static_cast<int>(result->steps.size()) == count - 1,
              "the listed steps add up to the reported plan");
    }
    check(goalsDiffer > 0, "some sets have a cheaper order that leaves more anvil uses on the result");
    {
        using namespace lamium::inventory::enchantOrder;
        Box const button = changeButton(100, 20, 120, 90, 30);
        check(button.x + button.w == 216 && button.y + button.h == 106 && button.w == 40 && button.h == changeHeight,
              "the anvil list's Change button sits in the bottom right corner, inside the padding");
        check(button.has(180, 100) && !button.has(175, 100) && !button.has(180, 94) && !button.has(216, 100),
              "only presses inside the Change button count");
        std::vector<Enchantment> const one{{9, 3}, {12, 1}}, other{{12, 1}, {9, 3}}, raised{{9, 4}, {12, 1}};
        check(sameSet(one, other) && !sameSet(one, raised) && !sameSet(one, std::vector<Enchantment>{{9, 3}}) && sameSet({}, {}),
              "two items carry the same enchantments when types and levels agree, whatever the order");
        check(changeButton(100, 20, 30, 90, 80).w == 22 && !Box{}.has(0, 0), "a long word is cut to the list's width; no box, no press");
    }
}

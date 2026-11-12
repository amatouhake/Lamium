#include "features/inventory/EnchantPlan.h"
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
void enchantPlanTests() {
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
}

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
    // Compare with the brute force over small pseudo-random sets.
    std::uint32_t seed = 12345;
    auto next = [&](int below) { seed = seed * 1664525u + 1013904223u; return static_cast<int>((seed >> 16) % below); };
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
        if (!result) continue;
        int sum = 0, highest = 0;
        for (auto const& step : result->steps) {
            sum += step.cost;
            highest = std::max(highest, step.cost);
            check(refuseAt == 0 || step.cost < refuseAt, "no planned step reaches the limit");
        }
        check(sum == result->total && highest == result->highest && static_cast<int>(result->steps.size()) == count - 1,
              "the listed steps add up to the reported plan");
    }
}

#pragma once
#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <vector>

// L-170 groundwork: the order of anvil combinations that costs the fewest
// levels. Not wired to any screen, setting or action yet.
//
// The caller supplies what each input costs as a sacrifice. The prior-work
// rule and the limit of 40 were read from the anvil screen on 1.26.60
// (VALIDATION-LOG 2026-11-12).
namespace lamium::inventory::enchantPlan {
inline constexpr int maxInputs = 10, maxWork = 20;

struct Input {
    int value = 0;     // Levels this input adds when it is the sacrificed side.
    int work = 0;      // Times it has already been through an anvil.
    bool item = false; // The piece being enchanted: always the kept side.
};
// What the plan minimizes first. The other value breaks ties.
enum class Goal { FewestLevels, LeastPriorWork };
struct Rules {
    int refuseAt = 40; // A step costing this much or more is refused; 0 = no limit.
    Goal goal = Goal::FewestLevels;
};
// Each earlier anvil use doubles the surcharge.
inline constexpr int priorWorkPenalty(int work) noexcept { return (1 << std::clamp(work, 0, maxWork)) - 1; }
// The anvil uses behind a stored surcharge (L-319), or nothing when the
// value is not one the rule above produces.
inline constexpr std::optional<int> priorUses(int penalty) noexcept {
    for (int work = 0; work <= maxWork; ++work)
        if (priorWorkPenalty(work) == penalty) return work;
    return std::nullopt;
}

// Levels one enchantment level costs on the sacrificed side, from the game's
// `Enchant::Frequency` value (30, 10, 3, 1). Read from the anvil in nine
// cases (VALIDATION-LOG 2026-11-12); another value gives nothing, so the
// caller does not plan with a guess.
inline constexpr std::optional<int> levelCost(int frequency, bool book) noexcept {
    int const item = frequency == 30 ? 1 : frequency == 10 ? 2 : frequency == 3 ? 4 : frequency == 1 ? 8 : 0;
    if (!item) return std::nullopt;
    return book ? std::max(1, item / 2) : item;
}

struct Step {
    unsigned target = 0, sacrifice = 0; // Bit i = input i is part of that side.
    int cost = 0;
    int work = 0; // The result's anvil uses.
    bool operator==(Step const&) const = default;
};
struct Plan {
    std::vector<Step> steps;
    int total = 0, highest = 0, work = 0;
};

// Inputs are assumed to carry distinct, mutually compatible enchantments;
// merging two books of one enchantment into a higher level is not modelled.
// FewestLevels: fewest total levels, then the lowest most-expensive step, then
// the fewest anvil uses on the result. LeastPriorWork: fewest anvil uses on
// the result, then fewest total levels, then the lowest most-expensive step.
inline std::optional<Plan> plan(std::span<Input const> inputs, Rules rules = {}) {
    int const count = static_cast<int>(inputs.size());
    if (count < 1 || count > maxInputs) return std::nullopt;
    unsigned item = 0;
    int deepest = 0;
    for (int i = 0; i < count; ++i) {
        auto const& input = inputs[i];
        if (input.value < 0 || input.work < 0 || input.work > maxWork - maxInputs) return std::nullopt;
        if (input.item) { if (item) return std::nullopt; item = 1u << i; }
        deepest = std::max(deepest, input.work);
    }
    int const works = deepest + count; // Work counts 0 .. deepest + count - 1.
    unsigned const full = (1u << count) - 1;
    struct Cell {
        int total = -1, highest = 0; // total < 0: not reachable.
        unsigned target = 0;
        int targetWork = 0, sacrificeWork = 0;
    };
    std::vector<Cell> cells(static_cast<std::size_t>(full + 1) * works);
    auto cell = [&](unsigned mask, int work) -> Cell& { return cells[static_cast<std::size_t>(mask) * works + work]; };
    std::vector<int> value(full + 1, 0);
    for (unsigned mask = 1; mask <= full; ++mask) {
        int low = 0;
        while (!(mask >> low & 1u)) ++low;
        value[mask] = value[mask & (mask - 1)] + inputs[low].value;
        if (!(mask & (mask - 1))) { cell(mask, inputs[low].work).total = 0; continue; }
        // Every way to split the set into a kept side and a sacrificed side.
        for (unsigned kept = (mask - 1) & mask; kept; kept = (kept - 1) & mask) {
            unsigned const gone = mask ^ kept;
            if (gone & item) continue;
            for (int a = 0; a < works; ++a) {
                auto const& left = cell(kept, a);
                if (left.total < 0) continue;
                for (int b = 0; b < works; ++b) {
                    auto const& right = cell(gone, b);
                    if (right.total < 0) continue;
                    int const work = std::max(a, b) + 1;
                    if (work >= works) continue;
                    int const cost = value[gone] + priorWorkPenalty(a) + priorWorkPenalty(b);
                    if (rules.refuseAt > 0 && cost >= rules.refuseAt) continue;
                    int const total = left.total + right.total + cost;
                    int const highest = std::max({left.highest, right.highest, cost});
                    auto& best = cell(mask, work);
                    if (best.total < 0 || total < best.total || (total == best.total && highest < best.highest))
                        best = {total, highest, kept, a, b};
                }
            }
        }
    }
    int end = -1;
    for (int work = 0; work < works; ++work) {
        auto const& candidate = cell(full, work);
        if (candidate.total < 0) continue;
        if (end < 0) { end = work; continue; }
        if (rules.goal == Goal::LeastPriorWork) break; // Work counts are visited in rising order.
        auto const& best = cell(full, end);
        if (candidate.total < best.total || (candidate.total == best.total && candidate.highest < best.highest)) end = work;
    }
    if (end < 0) return std::nullopt;
    Plan result{{}, cell(full, end).total, cell(full, end).highest, end};
    // Kept side's steps, then the sacrificed side's, then the step joining them.
    auto emit = [&](auto&& self, unsigned mask, int work) -> void {
        if (!(mask & (mask - 1))) return;
        auto const& here = cell(mask, work);
        unsigned const gone = mask ^ here.target;
        self(self, here.target, here.targetWork);
        self(self, gone, here.sacrificeWork);
        result.steps.push_back({here.target, gone,
            value[gone] + priorWorkPenalty(here.targetWork) + priorWorkPenalty(here.sacrificeWork), work});
    };
    emit(emit, full, end);
    return result;
}

// Following a plan at the anvil. Input 0 is the item and input i + 1 the
// book of wanted[i], as the planner's caller lays them out.
struct Enchantment {
    int type = 0, level = 0;
    bool operator==(Enchantment const&) const = default;
};
// Which inputs a stack in an anvil slot stands for: bit 0 when it is the
// item (it then carries everything in `base`), bit i + 1 for each wanted
// enchantment on it. Nothing when it carries anything else, a wanted one at
// another level, or is a book with none of them.
inline std::optional<unsigned> carried(bool item, std::span<Enchantment const> on, std::span<Enchantment const> base,
    std::span<Enchantment const> wanted) {
    if (wanted.size() + 1 > static_cast<std::size_t>(maxInputs)) return std::nullopt;
    unsigned mask = item ? 1u : 0u;
    std::size_t fromBase = 0;
    for (auto const& enchantment : on) {
        if (item && std::ranges::find(base, enchantment) != base.end()) { ++fromBase; continue; }
        auto const found = std::ranges::find(wanted, enchantment);
        if (found == wanted.end()) return std::nullopt;
        mask |= 1u << (found - wanted.begin() + 1);
    }
    if (item ? fromBase != base.size() : mask == 0) return std::nullopt;
    return mask;
}
// The step that joins these two sides, or -1.
inline int stepFor(Plan const& plan, unsigned target, unsigned sacrifice) noexcept {
    for (std::size_t i = 0; i < plan.steps.size(); ++i)
        if (plan.steps[i].target == target && plan.steps[i].sacrifice == sacrifice) return static_cast<int>(i);
    return -1;
}
}

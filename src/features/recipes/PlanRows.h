#pragma once
#include "features/recipes/PlanTargets.h"
#include <map>
#include <set>
#include <string>
#include <vector>

// L-370: the rows of the plan's three lists, one source for the Notes window and the panel, so the two list
// the same things in the same order (L-387). Drawing is each view's. Pure.
namespace lamium::recipes {
// "To gather": what is left, most left first, then what is gathered. `key` names a wanted item the recipe
// table does not know (its line's id -1); `stopped`, an item the player set to gather rather than make.
struct GatherRow {
    GatherLine line;
    std::string key;
    bool stopped = false, done = false;
    bool operator==(GatherRow const&) const = default;
};
struct GatherRows {
    std::vector<GatherRow> left, finished;
};
inline GatherRows gatherRows(Index const& index, RawView const& view, std::vector<LooseLine> const& loose, std::set<Item> const& stops) {
    Item const items = static_cast<Item>(index.items.size());
    GatherRows out;
    for (auto const& line : view.gather) {
        bool const stopped = line.id < items && stops.contains(line.id);
        if (line.left > 0) out.left.push_back({line, {}, stopped});
        else if (stopped) out.finished.push_back({line, {}, true, true});
    }
    for (auto const& line : loose) {
        GatherRow row{{-1, line.need, line.held, line.left}, line.key, false, line.left <= 0};
        (row.done ? out.finished : out.left).push_back(std::move(row));
    }
    for (auto const& line : view.done) out.finished.push_back({line, {}, false, true});
    return out;
}

// "To make": steps made now, in part, after taking out of the stores, short of something (L-385), each in
// the plan's order; then what held stock covers whole (Held). `now`: of what the step makes, what can be made
// now from what is carried.
struct MakeRow {
    StepLine step;
    StepState state = StepState::Ready;
    Count now = 0;
    bool operator==(MakeRow const&) const = default;
};
inline std::vector<MakeRow> makeRows(Index const& index, Request const& request, RawView const& view, std::vector<StepLine> const& held,
                                     std::map<Item, Count> const& carried, std::map<Item, Count> const& all) {
    std::vector<std::pair<StepState, Count>> states;
    for (auto const& step : view.steps) {
        Count const each = std::max<Count>(1, index.recipes[static_cast<size_t>(step.recipe)].count);
        states.emplace_back(stepState(index, request, step, carried, all), std::min(step.made, stepNow(index, request, step, carried) * each));
    }
    std::vector<MakeRow> out;
    for (StepState state : {StepState::Ready, StepState::Partly, StepState::Stored, StepState::Missing})
        for (size_t i = 0; i < view.steps.size(); ++i)
            if (states[i].first == state) out.push_back({view.steps[i], state, states[i].second});
    for (auto const& step : held) out.push_back({step, StepState::Held, 0});
    return out;
}

// "Wanted": each entry's finished items held (a wanted item the table does not know: what is held of it).
inline std::vector<Count> wantedHeld(std::vector<WantedEntry> const& entries, std::vector<TargetPlan> const& targets,
                                     std::vector<LooseLine> const& loose) {
    std::vector<Count> out;
    for (size_t e = 0; e < entries.size(); ++e) {
        Count have = 0;
        if (e < targets.size() && targets[e].item >= 0) have = targets[e].finished;
        else for (auto const& line : loose) if (line.key == entries[e].key) { have = line.held; break; }
        out.push_back(have);
    }
    return out;
}
// The entries not all held, then those that are ("done"), each in the order added. While `holding` (the
// pointer over the list) a row keeps the place `kept` gave it, so stepping a count past what is held does
// not move the row from under the pointer; otherwise `kept` takes today's places.
struct WantedRow {
    size_t entry = 0;
    Count held = 0;
    bool done = false;
    bool operator==(WantedRow const&) const = default;
};
inline std::vector<WantedRow> wantedRows(std::vector<WantedEntry> const& entries, std::vector<Count> const& held,
                                         std::map<std::string, bool>& kept, bool holding) {
    if (!holding) kept.clear();
    std::vector<WantedRow> open, done;
    for (size_t e = 0; e < entries.size(); ++e) {
        Count const have = e < held.size() ? held[e] : 0;
        bool const now = entries[e].count > 0 && have >= entries[e].count;
        std::string const id = wantedEntryId(entries[e]);
        bool shown = now;
        if (holding) {
            if (auto found = kept.find(id); found != kept.end()) shown = found->second;
        } else kept[id] = now;
        (shown ? done : open).push_back({e, have, shown});
    }
    open.insert(open.end(), done.begin(), done.end());
    return open;
}
}

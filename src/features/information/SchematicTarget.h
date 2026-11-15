#pragma once
#include "features/information/TargetInfo.h"
#include "features/schematic/Verify.h"
#include <cstdlib>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// The target card's schematic rows (BACKLOG L-93, redesigned 2026-10-08):
// what to do about the block under the crosshair, in the verifier's colors.
// Pure; the caller supplies translation.
namespace lamium::information {
using Translate = std::function<std::string(std::string_view)>;
// A raw state value as the card shows it: by name when L-112 knows it.
struct StateName { std::string label, value; bool labelIsKey = false; };
inline StateName stateName(std::string const& key, std::string const& raw, std::string_view identifier,
                           Translate const& translate) {
    if (raw == "-") return {key, raw};
    std::optional<TargetInfo::DetailRow> row;
    char* end = nullptr;
    long long number = std::strtoll(raw.c_str(), &end, 10);
    if (!raw.empty() && end && *end == '\0') row = interpretBlockState(key, StateKind::Integer, number, {}, identifier);
    else row = interpretBlockState(key, StateKind::Text, 0, raw, identifier);
    if (!row || row->progress) return {key, raw};
    return {row->label, row->valueIsKey ? translate(row->value) : row->value, true};
}
// A state's name without a value (L-183's items): the label the card gives
// its values, probed with a number and with the text forms (named
// directions, halves, axes); the raw key when the card has no name for it.
inline StateName stateLabel(std::string const& key, std::string_view identifier) {
    if (auto row = interpretBlockState(key, StateKind::Integer, 0, {}, identifier)) return {row->label, {}, true};
    for (std::string_view text : {"north", "top", "x"})
        if (auto row = interpretBlockState(key, StateKind::Text, 0, text, identifier)) return {row->label, {}, true};
    return {key, {}};
}
// The card's row for a schematic ghost under the crosshair, in the ghost's
// light blue (L-184). The header already names the block to place.
inline TargetInfo::DetailRow ghostRow() {
    TargetInfo::DetailRow row{"target.schematic", "target.schematicMissing", true};
    row.kind = DetailKind::Schematic;
    row.tone = Tone::Missing;
    return row;
}
// The card's row for an ignored place of the selected placement (L-183),
// dimmed: the key's effect seen up close.
inline TargetInfo::DetailRow ignoredRow() {
    TargetInfo::DetailRow row{"target.schematic", "target.schematicIgnored", true};
    row.kind = DetailKind::Schematic;
    row.tone = Tone::Ignored;
    return row;
}
inline std::vector<TargetInfo::DetailRow> schematicRows(schematic::CellState state, std::string const& expectedName,
    std::string const& icon, std::vector<schematic::StateDifference> const& differences, std::string_view identifier,
    Translate const& translate) {
    using schematic::CellState;
    std::vector<TargetInfo::DetailRow> rows;
    auto row = [&](std::string label, std::string value, bool labelIsKey, Tone tone, std::string withIcon = {},
                   std::string before = {}) {
        TargetInfo::DetailRow r{std::move(label), std::move(value)};
        r.before = std::move(before);
        r.kind = DetailKind::Schematic;
        r.labelIsKey = labelIsKey;
        r.tone = tone;
        r.icon = std::move(withIcon);
        rows.push_back(std::move(r));
    };
    // First what is wrong, as "Schematic: <kind>", then what it should be.
    auto status = [&](std::string kind, Tone tone) {
        row("target.schematic", std::move(kind), true, tone);
        rows.back().valueIsKey = true;
    };
    switch (state) {
    case CellState::Wrong:
        status("target.schematicWrong", Tone::Wrong);
        row("schematic.shouldBe", expectedName, true, Tone::Wrong, icon);
        break;
    case CellState::Extra:
        status("target.schematicExtra", Tone::Wrong);
        row("schematic.shouldBe", translate("schematic.air"), true, Tone::Wrong);
        break;
    case CellState::Missing:
        status("target.schematicMissing", Tone::Missing);
        row("schematic.placeHere", expectedName, true, Tone::Missing, icon);
        break;
    case CellState::State:
        status("target.schematicState", Tone::State);
        // One row per differing state: "<state>: <now> -> <should be>", the
        // arrow drawn by the card (the fonts lack U+2192).
        for (auto const& d : differences) {
            auto now = stateName(d.key, d.actual, identifier, translate);
            auto want = stateName(d.key, d.expected, identifier, translate);
            auto const& named = now.labelIsKey ? now : want;
            row(named.label, want.value, named.labelIsKey, Tone::State, {}, now.value);
        }
        break;
    default: break;
    }
    return rows;
}
}

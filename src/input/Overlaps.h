#pragma once
#include "input/Binding.h"
#include "input/FixedPresses.h"
#include <algorithm>
#include <optional>
#include <vector>

// Every use a chord meets where it works (L-412): other bindings, fixed
// presses, the game's click and the game's keys. Hotkeys' key cells, the
// conflict tip, the capture line, the footer and Find by key all read this
// one list, so they agree on what a key shares.
namespace lamium::input {
enum class OverlapSource { Binding, Fixed, GameClick, GameKey };
struct Overlap {
    OverlapSource source;
    Relation relation; // GameKey: Shared for its whole chord (the game's action stops), else Overlap; GameClick: Overlap
    Link link = Link::Same;
    unsigned where = 0; // the layers both work in
    Action action = Action::Count; // Binding
    size_t index = 0; // Fixed: into fixedPresses; GameKey: into gameKeys
    GameClick click = GameClick::None; // GameClick: in play; None for an item screen's click
    bool both = false; // Fixed, and a binding in fixedOverlaps: a lone mouse button is not taken, so the press happens too
};
// A fixed press as a chord to compare with: the drop and inventory keys by
// the game's first key for them; none when the game has none.
inline std::optional<Chord> fixedChord(FixedPress const& press, GameKeyCodes const& game) {
    Chord chord;
    for (int i = 0; i < fixedCapCount(press); ++i) {
        auto const& cap = press.caps[i];
        if (cap.game()) {
            auto const index = gameKeyIndex(cap.drop ? "key.drop" : "key.inventory");
            if (!index || game[*index].empty()) return std::nullopt;
            chord.push_back({Device::Key, game[*index].front()});
        } else if (cap.matches()) chord.push_back(cap.token);
    }
    return chord;
}
// Fixed presses ignore the order of their inputs and the wheel's direction,
// and name the generic Shift, Ctrl and Alt.
inline Chord unordered(Chord chord) {
    for (auto& token : chord) {
        if (token.device == Device::Key) token.code = genericModifier(token.code);
        if (token.device == Device::Wheel) token.code = 1;
    }
    std::sort(chord.begin(), chord.end(), [](Token a, Token b) {
        return a.device != b.device ? a.device < b.device : a.code < b.code;
    });
    chord.erase(std::unique(chord.begin(), chord.end()), chord.end());
    return chord;
}
inline bool loneClick(Chord const& chord) {
    return chord.size() == 1 && chord[0].device == Device::Mouse && chord[0].code <= 3;
}
// `self`: the action whose binding this is, left out of the list.
inline std::vector<Overlap> overlaps(unsigned layers, Chord const& chord, Bindings const& bindings, GameKeyCodes const& game,
                                     std::optional<Action> self = {}) {
    std::vector<Overlap> result;
    if (chord.empty() || !layers) return result;
    for (size_t i = 0; i < actions.size(); ++i) {
        auto const other = static_cast<Action>(i);
        auto const where = layers & whereActs(other).layers;
        if (other == self || retired(other) || !where) continue;
        auto const otherChord = effectiveChord(bindings, other);
        auto const relation = bindingRelation(chord, otherChord);
        if (relation != Relation::None) result.push_back({OverlapSource::Binding, relation, linkOf(chord, otherChord, relation), where, other});
    }
    auto const mine = unordered(chord);
    for (size_t i = 0; i < fixedPresses.size(); ++i) {
        auto const where = layers & fixedPresses[i].where;
        auto const fixed = where ? fixedChord(fixedPresses[i], game) : std::nullopt;
        if (!fixed || fixed->empty()) continue;
        auto const theirs = unordered(*fixed);
        auto const relation = bindingRelation(mine, theirs) == Relation::None ? Relation::None
            : mine == theirs ? Relation::Shared : Relation::Overlap;
        if (relation == Relation::None) continue;
        Overlap entry{OverlapSource::Fixed, relation, linkOf(mine, theirs, relation), where};
        entry.index = i;
        entry.both = loneClick(chord);
        result.push_back(entry);
    }
    // The game's click: in play it is attack, use or pick block; on an item
    // screen the slot's click. A window over the screen gives it to Lamium's list.
    if (auto const click = gameClick(chord); click != GameClick::None) {
        if (layers & layer::gameplay) {
            Overlap entry{OverlapSource::GameClick, Relation::Overlap, Link::Same, layer::gameplay};
            entry.click = click;
            result.push_back(entry);
        }
        if (layers & layer::itemScreen) result.push_back({OverlapSource::GameClick, Relation::Overlap, Link::Same, layer::itemScreen});
    }
    for (size_t i = 0; i < gameKeys.size(); ++i) {
        auto const where = layers & gameKeys[i].layers;
        if (!where || !takesGameKey(chord, i, game)) continue;
        // Only the game's whole chord stops its action; inside a longer one (F3 + B) it stops only there.
        Chord whole;
        for (int modifier : gameKeys[i].modifiers) if (modifier) whole.push_back({Device::Key, modifier});
        auto const gameKey = std::find_if(chord.begin(), chord.end(), [&](Token t) {
            return t.device == Device::Key && std::find(game[i].begin(), game[i].end(), genericModifier(t.code)) != game[i].end(); });
        if (gameKey != chord.end()) whole.push_back({Device::Key, genericModifier(gameKey->code)});
        bool const exact = unordered(chord) == unordered(whole);
        Overlap entry{OverlapSource::GameKey, exact ? Relation::Shared : Relation::Overlap, exact ? Link::Same : Link::InsideThis, where};
        entry.index = i;
        result.push_back(entry);
    }
    std::stable_sort(result.begin(), result.end(), [](Overlap const& a, Overlap const& b) {
        return a.source != b.source ? a.source < b.source : a.link < b.link;
    });
    return result;
}
inline std::vector<Overlap> overlaps(Bindings const& bindings, Action action, GameKeyCodes const& game) {
    if (retired(action)) return {};
    return overlaps(whereActs(action).layers, effectiveChord(bindings, action), bindings, game, action);
}
// The strongest: a shared chord, a fixed press taken whole or a game key stops
// something; an overlap or the game's click only shares it.
inline Relation strongest(std::vector<Overlap> const& list) {
    auto relation = Relation::None;
    for (auto const& entry : list) relation = std::max(relation, entry.both ? Relation::Overlap : entry.relation);
    return relation;
}
// The bindings that take a fixed press where it works, for its row's tip.
inline std::vector<Overlap> fixedOverlaps(size_t press, Bindings const& bindings, GameKeyCodes const& game) {
    auto const chord = fixedChord(fixedPresses[press], game);
    if (!chord) return {};
    std::vector<Overlap> result;
    auto const mine = unordered(*chord);
    for (size_t i = 0; i < actions.size(); ++i) {
        auto const other = static_cast<Action>(i);
        auto const where = fixedPresses[press].where & whereActs(other).layers;
        if (retired(other) || !where) continue;
        auto const theirs = unordered(effectiveChord(bindings, other));
        if (theirs.empty()) continue;
        auto const relation = bindingRelation(mine, theirs) == Relation::None ? Relation::None
            : mine == theirs ? Relation::Shared : Relation::Overlap;
        if (relation == Relation::None) continue;
        Overlap entry{OverlapSource::Binding, relation, linkOf(mine, theirs, relation), where, other};
        entry.both = loneClick(effectiveChord(bindings, other)); // as overlaps(): the press happens too
        result.push_back(entry);
    }
    std::stable_sort(result.begin(), result.end(), [](Overlap const& a, Overlap const& b) { return a.link < b.link; });
    return result;
}
}

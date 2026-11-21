#pragma once
#include "features/recipes/RecipeState.h"
#include "ui/NumberInput.h"
#include <string>

// L-359: a wanted item's count being typed into its stepper, on the left band
// or in the window: one at a time, whichever stepper's number was pressed.
// The host feeds it the keys it keeps from the game and ends it; the steppers
// only start it and show it. Client thread only.
namespace lamium::recipes::countEdit {
inline std::string item; // empty: nothing is being typed
// Where the press started it: the panel's box and the window's of one item are two, and only that one types.
enum class Where { Panel, Window };
inline Where where = Where::Panel;
inline ui::NumberInput typed;
inline bool active() { return !item.empty(); }
inline ui::TextEdit const* editing(std::string const& key, Where at) { return active() && key == item && at == where ? &typed : nullptr; }
inline void begin(std::string key, Count now, Where at) {
    item = std::move(key);
    where = at;
    typed.beginPrecise(static_cast<double>(now));
}
inline void cancel() { item.clear(); }
// Keeps what was typed, as near as a count can be: 0 takes the item out, as minus at one does; what is
// no number changes nothing.
inline void commit() {
    if (!active()) return;
    if (auto value = typed.committedPrecise(0, static_cast<double>(Wanted::most), true); value && state::wanted().has(item)) {
        if (*value >= 1) state::setWanted(item, static_cast<Count>(*value));
        else state::removeWanted(item);
    }
    item.clear();
}
}

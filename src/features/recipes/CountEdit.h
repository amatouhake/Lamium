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
inline ui::NumberInput typed;
inline bool active() { return !item.empty(); }
inline ui::TextEdit const* editing(std::string const& key) { return active() && key == item ? &typed : nullptr; }
inline void begin(std::string key, Count now) {
    item = std::move(key);
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

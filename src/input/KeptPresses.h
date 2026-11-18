#pragma once
#include <algorithm>
#include <vector>

namespace lamium::input {
// Which presses are kept from the game. A key or button is decided once, at its first down: kept, it
// stays kept through its repeats and its release, whatever has closed meanwhile (a held Esc must not
// reach the screen under a view it closed); passed on, the game sees the rest of it too, release
// included (a key held while a view opens must not stay held in the game).
struct KeptPresses {
    std::vector<int> kept, passed;
    // True when the event is kept. `first`: this is the press itself, not a repeat of it.
    bool decide(int code, bool down, bool wanted, bool* first = nullptr) {
        auto const k = std::find(kept.begin(), kept.end(), code), p = std::find(passed.begin(), passed.end(), code);
        if (first) *first = false;
        if (down) {
            if (k != kept.end()) return true;
            if (p != passed.end()) return false;
            if (first) *first = true;
            auto& to = wanted ? kept : passed;
            if (to.size() >= 64) to.erase(to.begin());
            to.push_back(code);
            return wanted;
        }
        if (p != passed.end()) passed.erase(p);
        if (k == kept.end()) return false;
        kept.erase(k);
        return true;
    }
};
}

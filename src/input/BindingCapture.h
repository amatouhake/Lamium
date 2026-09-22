#pragma once
#include "input/Binding.h"

namespace lamium::input {
class BindingCapture {
    Chord chord;
    Chord ignored;
public:
    void clear() { chord.clear(); ignored.clear(); }
    void begin(Chord alreadyHeld) { chord.clear(); ignored = std::move(alreadyHeld); }
    Chord const& value() const { return chord; }
    std::optional<Chord> observe(Token token, bool down, Behavior behavior) {
        if (std::find(ignored.begin(), ignored.end(), token) != ignored.end()) {
            if (!down) std::erase(ignored, token);
            return {};
        }
        if (token.device == Device::Wheel) {
            auto candidate = chord;
            candidate.push_back(token);
            return canonicalChord(std::move(candidate), behavior);
        }
        if (!down) {
            // Ignore release of the click/Enter that opened the editor. Only
            // a key captured after opening can complete the new binding.
            if (std::find(chord.begin(), chord.end(), token) == chord.end()) return {};
            return canonicalChord(chord, behavior);
        }
        if (std::find(chord.begin(), chord.end(), token) == chord.end()) {
            auto candidate = chord;
            candidate.push_back(token);
            chord = canonicalChord(std::move(candidate), behavior);
        }
        return {};
    }
};
}

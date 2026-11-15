#pragma once

// L-153 groundwork: which of two opposing movement keys counts while both
// are held. Event sequences only; nothing reads game input from here yet.
namespace lamium::input {
// What happens when the winning key is released while the other is still held.
enum class OpposingResume {
    Held,   // The key still held takes over again.
    Repress // Nothing until that key is released and pressed again.
};
class OpposingKeys {
public:
    explicit constexpr OpposingKeys(OpposingResume resume = OpposingResume::Held) noexcept : resume(resume) {}
    // side: false = the negative key (S, A), true = the positive key (W, D).
    // A key-down repeated while held (auto-repeat) changes nothing.
    constexpr void press(bool side) noexcept {
        auto& key = side ? positive : negative;
        if (key.held) return;
        key.held = true;
        key.suppressed = false;
        key.order = ++counter;
    }
    constexpr void release(bool side) noexcept {
        auto& key = side ? positive : negative;
        auto& other = side ? negative : positive;
        if (!key.held) return;
        bool const won = !other.held || key.order > other.order;
        key = {};
        if (won && other.held && resume == OpposingResume::Repress) other.suppressed = true;
    }
    // Focus loss, a menu opening, world exit: no key may stay held.
    constexpr void clear() noexcept { negative = {}; positive = {}; }
    // -1, 0 or +1.
    constexpr int direction() const noexcept {
        bool const minus = negative.held && !negative.suppressed, plus = positive.held && !positive.suppressed;
        if (minus && plus) return positive.order > negative.order ? 1 : -1;
        return plus ? 1 : minus ? -1 : 0;
    }
    constexpr bool held(bool side) const noexcept { return (side ? positive : negative).held; }

private:
    struct Key { bool held = false, suppressed = false; unsigned long long order = 0; };
    Key negative, positive;
    unsigned long long counter = 0;
    OpposingResume resume;
};
}

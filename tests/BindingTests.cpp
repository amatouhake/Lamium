#include "input/Binding.h"
void check(bool, char const*);
void bindingTests() {
    using namespace lamium::input;
    Token z{Device::Key, 0x5a}, three{Device::Key, 0x33};
    auto chord = canonicalChord({z, three, z}, Behavior::Hold);
    check(chord == canonicalChord({three, z}, Behavior::Hold), "chord order and duplicate keys canonicalize");
    BindingState state;
    check(!state.update(chord, {z}).pressed, "partial chord does not trigger");
    check(state.update(chord, {three, z}).pressed, "arbitrary chord triggers when all inputs held");
    check(!state.update(chord, {three, z}).pressed, "key repeats do not repeat an action");
    check(state.update(chord, {three}).released, "releasing any chord member ends hold");
    check(!state.update(chord, {}).released, "release delivered only once");
    check(state.update(chord, {z, three}).pressed, "chord can be pressed again");
    check(state.reset().released && !state.reset().released, "focus reset releases exactly once");
    check(!state.update({}, {z}).pressed, "unbound action never triggers");
    Token shift{Device::Key, 0x10}, wheel{Device::Wheel, 1};
    Chord scroll{shift, wheel};
    check(!state.update(scroll, {}, wheel).pressed, "wheel requires modifier");
    check(!state.update(scroll, {shift}).pressed, "held modifier alone does not pulse");
    check(state.update(scroll, {shift}, wheel).pressed && state.update(scroll, {shift}, wheel).pressed,
          "each wheel impulse triggers even when modifier stays held");
    check(!state.update(scroll, {shift}, Token{Device::Wheel, -1}).pressed, "wheel direction matters");
    for (auto invalid : {Chord{{Device::Key, 0}}, Chord{{Device::Mouse, 6}},
                         Chord{{Device::Wheel, 0}}, Chord{{Device::Wheel, 1}, {Device::Wheel, -1}}}) {
        bool rejected = false;
        try { (void)canonicalChord(invalid, Behavior::Press); } catch (...) { rejected = true; }
        check(rejected, "invalid bindings rejected");
    }
    bool rejected = false;
    try { (void)canonicalChord(scroll, Behavior::Hold); } catch (...) { rejected = true; }
    check(rejected, "hold actions cannot use a wheel impulse");
}

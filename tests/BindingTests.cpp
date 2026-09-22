#include "input/Binding.h"
#include "input/BindingCapture.h"
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
    HeldInputs held;
    held.observe(z, true, true);
    held.observe(three, true, true);
    check(state.update(chord, held.value()).pressed, "tracked chord presses");
    held.observe(z, true, true);
    check(!state.update(chord, held.value()).pressed && state.isActive(), "tracked repeat stays held without firing");
    held.observe({Device::Wheel, 1}, true, false);
    check(!state.update(chord, held.value()).released, "consumed wheel does not cancel a held chord");
    held.invalidate();
    check(state.reset().released, "ownership change releases action");
    held.observe(z, true, true);
    held.observe(three, true, true);
    check(!state.update(chord, held.value()).pressed, "held repeats cannot reactivate after focus loss");
    held.observe(z, false, false);
    held.observe(three, false, false);
    held.observe(z, true, true);
    held.observe(three, true, true);
    check(state.update(chord, held.value()).pressed, "fresh press after release re-arms chord");
    held.observe(z, false, false);
    check(state.update(chord, held.value()).released, "cancelled key-up still releases chord");
    held.clear();
    held.observe(z, true, false);
    held.observe(z, true, true);
    check(held.value().empty(), "key pressed in a text field cannot activate on repeat after it closes");
    held.observe(z, false, false);
    held.observe(z, true, true);
    check(held.value() == Chord{z}, "text-field key re-arms after its release");
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
    BindingCapture capture;
    capture.begin({Token{Device::Key, 13}});
    check(!capture.observe({Device::Key, 13}, true, Behavior::Hold), "opening Enter repeat ignored");
    check(!capture.observe({Device::Key, 13}, false, Behavior::Hold), "opening Enter release ignored");
    check(!capture.observe(z, true, Behavior::Hold), "capture waits for release");
    check(!capture.observe(three, true, Behavior::Hold), "capture accumulates simultaneous keys");
    check(capture.observe(z, false, Behavior::Hold) == chord, "capture completes arbitrary chord on release");
    capture.clear();
    capture.observe(shift, true, Behavior::Press);
    check(capture.observe(wheel, true, Behavior::Press) == canonicalChord(scroll, Behavior::Press),
          "capture completes modified wheel immediately");
    capture.clear();
    capture.observe({Device::Mouse, 5}, true, Behavior::Press);
    check(capture.observe({Device::Mouse, 5}, false, Behavior::Press) == Chord{{Device::Mouse, 5}},
          "capture supports extra mouse buttons");
}

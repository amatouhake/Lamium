#include "input/Binding.h"
#include "input/BindingCapture.h"
#include "input/ToggleAction.h"
#include "settings/Options.h"
void check(bool, char const*);
void bindingTests() {
    using namespace lamium::input;
    for (size_t i=0; i<actions.size(); ++i) {
        lamium::Settings value;
        auto action = static_cast<Action>(i);
        bool changed = toggleAction(value,action);
        // Permanent Sneak toggles runtime intent, not a saved preference.
        bool persistentToggle = actions[i].behavior == Behavior::Toggle && action != Action::PermanentSneak
            && action != Action::PeriodicAttack && action != Action::PeriodicUse;
        check(changed == persistentToggle, "every persistent toggle action has one shared implementation");
        size_t count = 0;
        for (auto const& option : lamium::settings::options) {
            if (option.read(value) == option.read(lamium::Settings{})) continue;
            ++count;
            check(option.feature == actions[i].feature, "toggle changes only its owning feature");
        }
        check(count == (changed ? 1u : 0u), "press and hold actions cannot modify toggle settings");
        toggleAction(value,action);
        for (auto const& option : lamium::settings::options)
            check(option.read(value) == option.read(lamium::Settings{}), "second toggle restores original settings");
        check(actions[i].defaultKey >= 0 && actions[i].defaultKey <= 255, "native defaults use valid key codes or unbound");
    }
    check(actions[static_cast<size_t>(Action::Settings)].defaultKey == 0x4C
        && actions[static_cast<size_t>(Action::Zoom)].defaultKey == 0x43
        && actions[static_cast<size_t>(Action::NightVision)].defaultKey == 0x4a
        && actions[static_cast<size_t>(Action::Sort)].defaultKey == 0x52,
        "existing native defaults remain compatible");
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
    // Exercise event sequences, including mixed devices, rather than only
    // passing complete snapshots to the matcher. Any member may arrive last.
    Chord mixed{z, three, {Device::Mouse, 3}};
    auto ordered = canonicalChord(mixed, Behavior::Hold);
    do {
        for (auto released : mixed) {
            HeldInputs sequence;
            BindingState actionState;
            for (size_t index = 0; index < ordered.size(); ++index) {
                sequence.observe(ordered[index], true, true);
                auto edge = actionState.update(mixed, sequence.value());
                check(edge.pressed == (index + 1 == ordered.size()) && !edge.released,
                    "mixed chord activates only on the last member in every press order");
            }
            sequence.observe(released, false, false);
            check(actionState.update(mixed, sequence.value()).released,
                "each member can end a mixed-device hold");
            sequence.observe(released, true, true);
            check(actionState.update(mixed, sequence.value()).pressed,
                "repressing the released member re-arms a chord without releasing its other members");
            sequence.invalidate();
            check(actionState.reset().released, "focus loss ends mixed-device hold");
            sequence.observe(released, false, false);
            sequence.observe(released, true, true);
            for (auto token : ordered) sequence.observe(token, true, true);
            check(!actionState.update(mixed, sequence.value()).pressed,
                "partial release after focus loss cannot revive other stale held inputs");
            for (auto token : ordered) sequence.observe(token, false, false);
            for (auto token : ordered) sequence.observe(token, true, true);
            check(actionState.update(mixed, sequence.value()).pressed,
                "all fresh inputs restore mixed chord after focus loss");
        }
    } while (std::next_permutation(ordered.begin(), ordered.end()));
    HeldInputs wheelModifiers;
    BindingState wheelState;
    wheelModifiers.observe(shift, true, true);
    check(wheelState.update(scroll, wheelModifiers.value(), wheel).pressed,
        "modified wheel fires before focus loss");
    wheelModifiers.invalidate();
    wheelModifiers.observe(shift, true, true);
    check(!wheelState.update(scroll, wheelModifiers.value(), wheel).pressed,
        "wheel cannot revive a modifier held across focus loss");
    wheelModifiers.observe(shift, false, false);
    wheelModifiers.observe(shift, true, true);
    check(wheelState.update(scroll, wheelModifiers.value(), wheel).pressed,
        "fresh modifier re-arms wheel binding");
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

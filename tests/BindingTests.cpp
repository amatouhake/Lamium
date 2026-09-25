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
        // FreeCamera toggles a detached-camera session, not a saved preference.
        bool persistentToggle = actions[i].behavior == Behavior::Toggle && action != Action::PermanentSneak
            && action != Action::PeriodicAttack && action != Action::PeriodicUse
            && action != Action::FreeCamera;
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
    check(defaultChord(Action::Settings) == Chord{Token{Device::Key, 0x4C}}
        && defaultChord(Action::ChunkBorders).empty(), "defaults resolve to single-key chords or unbound");
    Bindings fresh;
    check(effectiveChord(fresh, Action::Settings) == Chord{Token{Device::Key, 0x4C}}
        && effectiveChord(fresh, Action::ChunkBorders).empty(), "absent bindings use Lamium defaults");
    fresh[static_cast<size_t>(Action::Settings)] = Chord{Token{Device::Key, 0x46}};
    fresh[static_cast<size_t>(Action::ChunkBorders)] = Chord{};
    check(effectiveChord(fresh, Action::Settings) == Chord{Token{Device::Key, 0x46}},
        "an override beats the default");
    check(effectiveChord(fresh, Action::ChunkBorders).empty()
        && effectiveChord(fresh, Action::Zoom) == Chord{Token{Device::Key, 0x43}},
        "explicit unbind stays unbound while other defaults apply");
    check(!canClear(Action::Settings) && canClear(Action::Zoom) && canClear(Action::ChunkBorders),
        "only the settings action refuses Clear");
    Token z{Device::Key, 0x5a}, three{Device::Key, 0x33};
    auto chord = canonicalChord({z, three, z}, Behavior::Hold);
    check(chord == Chord{z, three} && canonicalChord({three, z}, Behavior::Hold) == Chord{three, z},
        "chords keep press order and drop repeated inputs");
    Token shift{Device::Key, 0x10}, ctrl{Device::Key, 0x11}, alt{Device::Key, 0x12}, f3{Device::Key, 0x72},
        b{Device::Key, 0x42}, c{Device::Key, 0x43}, w{Device::Key, 0x57}, space{Device::Key, 0x20},
        wheel{Device::Wheel, 1}, left{Device::Mouse, 1}, middle{Device::Mouse, 3}, back{Device::Mouse, 4};
    check(canonicalChord({wheel, shift}, Behavior::Press) == Chord{shift, wheel}, "a wheel impulse always completes a chord");
    check(legacyChordOrder({b, f3}) == Chord{f3, b} && legacyChordOrder({z, shift}) == Chord{shift, z}
        && legacyChordOrder({wheel, back, z, ctrl}) == Chord{ctrl, z, back, wheel}
        && legacyChordOrder({three, z}) == Chord{three, z},
        "legacy sorted chords regain modifier-first press order");

    // Event-sequence harness: every input goes through HeldInputs and then the
    // dispatcher, as CustomInput does.
    struct Keys {
        ChordSet chords;
        HeldInputs held;
        ChordDispatch dispatch;
        void bind(Action action, Chord value) {
            chords[static_cast<size_t>(action)] = canonicalChord(std::move(value), actions[static_cast<size_t>(action)].behavior);
        }
        Dispatch press(Token token, bool accepted = true) {
            bool fresh = held.observe(token, true, accepted);
            return dispatch.update(chords, held.value(), accepted ? std::optional<Token>(token) : std::nullopt, fresh);
        }
        Dispatch release(Token token) {
            held.observe(token, false, false);
            return dispatch.update(chords, held.value());
        }
        void focusLost() { held.invalidate(); }
    };
    auto only = [](Dispatch const& result, std::vector<Transition> expected) { return result.transitions == expected; };
    auto on = [](Action action) { return Transition{static_cast<size_t>(action), true}; };
    auto off = [](Action action) { return Transition{static_cast<size_t>(action), false}; };
    auto const shortA = Action::NightVision, longA = Action::ChunkBorders, longestA = Action::Hitboxes;

    {
        Keys keys;
        keys.bind(shortA, {b});
        keys.bind(longA, {f3, b});
        check(only(keys.press(f3), {}), "F3 alone fires nothing");
        auto completed = keys.press(b);
        check(only(completed, {on(longA)}) && completed.consumed, "F3 then B fires only F3+B and consumes B");
        auto repeat = keys.press(b);
        check(only(repeat, {}) && repeat.consumed, "B repeats stay with F3+B and never fire B");
        check(only(keys.release(f3), {off(longA)}), "releasing F3 ends F3+B without starting B");
        check(only(keys.press(b), {}), "B repeats after F3 is up cannot fire B late");
        check(only(keys.release(b), {}), "the latched B releases silently");
        check(only(keys.press(b), {on(shortA)}), "a fresh B fires B");
        auto late = keys.press(f3);
        check(only(late, {}) && !late.consumed, "B then F3 does not complete F3+B and leaves F3 to the game");
        check(only(keys.release(f3), {}) && only(keys.release(b), {off(shortA)}), "B releases once");
        keys.press(f3);
        check(only(keys.press(b), {on(longA)}), "F3+B repeats after a full release");
        keys.release(b);
        check(only(keys.press(b), {on(longA)}), "re-pressing B while F3 stays held repeats F3+B");
    }
    {
        Keys keys;
        keys.bind(shortA, {b});
        keys.bind(longA, {b});
        check(only(keys.press(b), {on(shortA), on(longA)}), "identical chords fire together");
        check(only(keys.press(b), {}), "identical chords fire once per press");
        check(only(keys.release(b), {off(shortA), off(longA)}), "identical chords release together");
    }
    {
        Keys keys;
        keys.bind(shortA, {b});
        keys.bind(longA, {shift, b});
        keys.bind(longestA, {ctrl, shift, b});
        keys.press(ctrl); keys.press(shift);
        check(only(keys.press(b), {on(longestA)}), "Ctrl+Shift+B fires only the three-key action");
        keys.release(b); keys.release(ctrl);
        check(only(keys.press(b), {on(longA)}), "Shift+B fires only the two-key action");
        keys.release(b); keys.release(shift);
        check(only(keys.press(b), {on(shortA)}), "B alone fires the single-key action");
        keys.release(b);
        keys.press(shift); keys.press(ctrl);
        check(only(keys.press(b), {on(longA)}), "Shift, Ctrl, B is not Ctrl+Shift+B; Shift+B wins");
    }
    {
        Keys keys;
        keys.bind(Action::Zoom, {c});
        keys.press(w); keys.press(space); keys.press(shift);
        check(only(keys.press(c), {on(Action::Zoom)}), "modifier-like zoom starts while moving, jumping and sneaking");
        check(only(keys.release(w), {}) && only(keys.press(w), {}) && only(keys.release(space), {}),
            "movement keys never break a modifier-like hold");
        check(only(keys.release(c), {off(Action::Zoom)}), "zoom ends with its key");
        keys.release(shift); keys.release(w);
        keys.bind(shortA, {c});
        keys.press(w);
        check(only(keys.press(c), {on(Action::Zoom), on(shortA)}), "ordinary chords also tolerate unrelated held keys");
    }
    {
        Keys keys;
        keys.bind(Action::Zoom, {c});
        keys.bind(longA, {c, left});
        check(only(keys.press(c), {on(Action::Zoom)}), "zoom starts");
        check(only(keys.press(left), {on(longA)}), "a longer chord fires on top of zoom");
        check(keys.dispatch.isActive(Action::Zoom), "zoom keeps running under the longer chord");
        check(only(keys.release(c), {off(Action::Zoom), off(longA)}), "releasing the shared key ends both");
    }
    {
        Keys keys;
        keys.bind(shortA, {b});
        keys.bind(longA, {b, left});
        check(only(keys.press(b), {on(shortA)}), "ordinary B activates");
        check(only(keys.press(left), {off(shortA), on(longA)}), "a longer ordinary chord takes over from an active shorter one");
        check(only(keys.release(left), {off(longA)}) && !keys.dispatch.isActive(shortA),
            "the shorter ordinary chord does not resume when the longer one ends");
        check(only(keys.release(b), {}) && only(keys.press(b), {on(shortA)}), "it activates again on a fresh press");
    }
    {
        Keys keys;
        keys.bind(shortA, {back});
        keys.bind(longA, {shift, back});
        keys.press(shift);
        check(only(keys.press(back), {on(longA)}), "Shift + mouse button beats the bare mouse button");
        keys.release(back); keys.release(shift);
        check(only(keys.press(back), {on(shortA)}), "the bare mouse button still works alone");
    }
    {
        Keys keys;
        keys.bind(shortA, {wheel});
        keys.bind(longA, {shift, wheel});
        check(only(keys.press(wheel), {on(shortA)}), "a bare wheel impulse fires");
        keys.press(shift);
        auto first = keys.press(wheel);
        check(only(first, {on(longA)}) && first.consumed, "Shift + wheel beats the bare wheel");
        check(only(keys.press(wheel), {on(longA)}), "each wheel impulse fires while Shift stays held");
        check(only(keys.press(Token{Device::Wheel, -1}), {}), "wheel direction matters");
        check(only(keys.release(shift), {}), "wheel chords have no release edge");
        keys.press(shift);
        keys.focusLost();
        keys.dispatch.reset();
        keys.press(shift);
        check(only(keys.press(wheel), {on(shortA)}), "a modifier held across focus loss cannot back a wheel chord");
        keys.release(shift); keys.press(shift);
        check(only(keys.press(wheel), {on(longA)}), "a fresh modifier re-arms the wheel chord");
    }
    {
        Keys keys;
        keys.bind(shortA, {z, three});
        keys.press(z);
        check(only(keys.press(three), {on(shortA)}), "chord fires on its last input");
        keys.focusLost();
        check(keys.dispatch.reset() == std::vector<Transition>{off(shortA)} && keys.dispatch.reset().empty(), "focus reset releases exactly once");
        check(only(keys.press(z), {}) && only(keys.press(three), {}), "held repeats cannot reactivate after focus loss");
        keys.release(three);
        check(only(keys.press(three), {}), "a stale first input cannot back a fresh last input");
        keys.release(z); keys.release(three);
        keys.press(z);
        check(only(keys.press(three), {on(shortA)}), "fresh presses after release re-arm the chord");
        keys.held.observe(z, false, false);
        check(only(keys.dispatch.update(keys.chords, keys.held.value()), {off(shortA)}), "cancelled key-up still releases");
    }
    {
        Keys keys;
        keys.bind(shortA, {z});
        check(only(keys.press(z, false), {}) && only(keys.press(z), {}), "key pressed in a text field cannot activate on repeat after it closes");
        keys.release(z);
        check(only(keys.press(z), {on(shortA)}), "text-field key re-arms after its release");
        keys.release(z);
        keys.press(z, false);
        keys.chords = {};
        check(only(keys.dispatch.update(keys.chords, keys.held.value()), {}), "unbound actions never fire");
    }
    {
        // Every press order of a mixed-device chord: the ordinary binding fires
        // only in its own order, the modifier-like one in any order.
        Chord mixed{alt, z, middle};
        auto order = mixed;
        std::sort(order.begin(), order.end());
        do {
            Keys keys;
            keys.bind(shortA, mixed);
            keys.bind(Action::Freelook, mixed);
            for (size_t index = 0; index < order.size(); ++index) {
                auto result = keys.press(order[index]);
                bool last = index + 1 == order.size();
                std::vector<Transition> expected;
                if (last && order == mixed) expected.push_back(on(shortA));
                if (last) expected.push_back(on(Action::Freelook));
                std::sort(expected.begin(), expected.end(), [](Transition l, Transition r) { return l.action < r.action; });
                check(only(result, expected), "ordinary chords follow press order, modifier-like chords do not");
            }
            for (auto released : mixed) {
                bool wasActive = keys.dispatch.isActive(Action::Freelook);
                auto result = keys.release(released);
                check(!wasActive || std::find(result.transitions.begin(), result.transitions.end(), off(Action::Freelook))
                    != result.transitions.end(), "any member ends a mixed-device hold");
            }
        } while (std::next_permutation(order.begin(), order.end()));
    }
    {
        Bindings bindings;
        bindings[static_cast<size_t>(Action::NightVision)] = Chord{b};
        bindings[static_cast<size_t>(Action::Hitboxes)] = Chord{b};
        bindings[static_cast<size_t>(Action::ChunkBorders)] = Chord{f3, b};
        bindings[static_cast<size_t>(Action::DebugView)] = Chord{b, f3};
        bindings[static_cast<size_t>(Action::Sort)] = Chord{b};
        check(bindingRelation({b}, {b}) == Relation::Shared && bindingRelation({f3, b}, {b}) == Relation::Overlap
            && bindingRelation({f3, b}, {b, f3}) == Relation::Overlap && bindingRelation({b}, {c}) == Relation::None
            && bindingRelation({}, {}) == Relation::None, "binding relations classify shared and overlapping chords");
        auto conflicts = bindingConflicts(bindings, Action::NightVision);
        auto has = [&](Action action, Relation relation) {
            return std::any_of(conflicts.begin(), conflicts.end(), [&](Conflict x) { return x.action == action && x.relation == relation; });
        };
        check(has(Action::Hitboxes, Relation::Shared) && has(Action::ChunkBorders, Relation::Overlap)
            && has(Action::DebugView, Relation::Overlap) && !has(Action::Sort, Relation::Shared),
            "the hotkeys list reports shared and overlapping bindings, not container-only ones");
        check(std::none_of(conflicts.begin(), conflicts.end(), [](Conflict x) { return x.action == Action::Zoom; }),
            "unrelated default keys are not reported");
    }
    Chord scroll{shift, wheel};
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

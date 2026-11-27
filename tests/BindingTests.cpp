#include "input/Binding.h"
#include "input/BindingCapture.h"
#include "input/ToggleAction.h"
#include "settings/Options.h"
#include "input/Actions.h"
#include "ui/SettingsNavigation.h"
#include "ui/KeyLabel.h"
void check(bool, char const*);
void bindingTests() {
    using namespace lamium::input;
    for (size_t i=0; i<actions.size(); ++i) {
        lamium::Settings value;
        auto action = static_cast<Action>(i);
        bool changed = toggleAction(value,action);
        // Permanent Sneak/Sprint toggle runtime intent, not a saved preference.
        // FreeCamera toggles a detached-camera session, not a saved preference.
        bool persistentToggle = actions[i].behavior == Behavior::Toggle && action != Action::PermanentSneak
            && action != Action::PermanentSprint            && action != Action::FreeCamera;
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
        && actions[static_cast<size_t>(Action::NightVision)].defaultKey == 0
        && actions[static_cast<size_t>(Action::DebugView)].defaultKey == 0x72
        && actions[static_cast<size_t>(Action::Sort)].defaultKey == 0,
        "single-key defaults follow the chosen keymap");
    check(defaultChord(Action::Settings) == Chord{Token{Device::Key, 0x4C}}
        && defaultChord(Action::NightVision).empty()
        && defaultChord(Action::DebugView) == Chord{Token{Device::Key, 0x72}}
        && defaultChord(Action::ChunkBorders) == Chord{{Device::Key, 0x72}, {Device::Key, 0x47}}
        && defaultChord(Action::Hitboxes) == Chord{{Device::Key, 0x72}, {Device::Key, 0x42}}
        && defaultChord(Action::Transfer).empty(),
        "Java-style debug defaults and unbound NightVision resolve correctly");
    check(sharedLayers(Action::Transfer, Action::Sort) == layer::itemScreen
        && sharedLayers(Action::Transfer, Action::Zoom) == layer::gameplay
        && !sharedLayers(Action::Sort, Action::Zoom),
        "transfer toggle meets item-screen and gameplay bindings, sort and zoom never meet");
    check(actionAllowed(Action::Transfer, true, false) && actionAllowed(Action::Transfer, false, true)
        && !actionAllowed(Action::Transfer, false, false)
        && actionAllowed(Action::Sort, false, true) && !actionAllowed(Action::Sort, true, false),
        "transfer toggle works in gameplay and containers while sort remains container-only");
    check(actionAllowed(Action::ShowRecipes, false, true) && !actionAllowed(Action::ShowUses, true, false)
        && sharedLayers(Action::ShowRecipes, Action::Sort) && !sharedLayers(Action::ShowRecipes, Action::Zoom)
        && defaultChord(Action::ShowRecipes) == Chord{Token{Device::Key, 0x52}} && defaultChord(Action::ShowUses) == Chord{Token{Device::Key, 0x55}}
        && actionAllowed(Action::ToggleRecipes, true, false),
        "the recipe keys are R and U on item screens only, where they meet Sort");
    {
        InputState screen{currentLayers(false, true, false, false)}, window{currentLayers(false, true, true, false)};
        InputState off = screen, offWindow = window;
        off.recipes = false; offWindow.recipes = false;
        check(defaultChord(Action::RecipeBack) == Chord{Token{Device::Key, 0x08}}
            && actionAllowed(Action::ShowRecipes, screen) && actionAllowed(Action::ShowRecipes, window)
            && !actionAllowed(Action::RecipeBack, screen) && actionAllowed(Action::RecipeBack, window)
            && !actionAllowed(Action::ShowUses, off) && !actionAllowed(Action::RecipeBack, offWindow)
            && actionAllowed(Action::Sort, screen) && !actionAllowed(Action::Sort, window)
            && !actionAllowed(Action::MassCraft, window) && !actionAllowed(Action::Transfer, window),
            "back is a key only while a window is open, which lets no other action through; off, the recipe keys are the game's");
        check(!sharedLayers(Action::RecipeBack, Action::Sort) && sharedLayers(Action::RecipeBack, Action::ShowRecipes) == layer::window
            && currentLayers(false, false, true, false) == 0,
            "back and sort never fire in the same place, so one key for both is no conflict (L-372)");
        InputState noPlan = window;
        noPlan.plan = false;
        check(defaultChord(Action::RecipeFavorite) == Chord{Token{Device::Key, 0x41}} && defaultChord(Action::RecipeWanted) == Chord{Token{Device::Key, 0x57}}
            && actionAllowed(Action::RecipeFavorite, false, true) && !actionAllowed(Action::RecipeWanted, true, false)
            && actionAllowed(Action::RecipeFavorite, window) && !actionAllowed(Action::RecipeWanted, noPlan)
            && actionAllowed(Action::RecipeWanted, off),
            "A and W over an item, on item screens and in the open view: A while Recipes is on, W while the Plan is (L-373)");
    }
    {
        // A conflict is shown exactly where both bindings could fire: in some state the input can be in, built as
        // the listener builds it (one screen on top, a window open or not, each condition either way), both are allowed.
        bool agree = true;
        std::vector<InputState> states;
        for (int screen = 0; screen < 4; ++screen)
            for (bool window : {false, true})
                for (int conditions = 0; conditions < 8; ++conditions)
                    states.push_back({currentLayers(screen == 1, screen == 2, window, screen == 3),
                                      (conditions & 1) != 0, (conditions & 2) != 0, (conditions & 4) != 0});
        for (size_t i = 0; i < actions.size(); ++i)
            for (size_t j = 0; j < actions.size(); ++j) {
                auto a = static_cast<Action>(i), b = static_cast<Action>(j);
                if (retired(a) || retired(b)) continue;
                bool both = std::any_of(states.begin(), states.end(), [&](InputState const& s) { return actionAllowed(a, s) && actionAllowed(b, s); });
                agree = agree && both == (sharedLayers(a, b) != 0);
            }
        check(agree, "bindings conflict exactly where both actions can fire");
        check(sharedLayers(Action::Sort, Action::MassCraft) && sharedLayers(Action::ShowRecipes, Action::RecipeBack)
              && sharedLayers(Action::Transfer, Action::Sort) && sharedLayers(Action::Settings, Action::FreeCameraSpeedUp)
              && !sharedLayers(Action::Sort, Action::RecipeBack) && !sharedLayers(Action::Sort, Action::Settings)
              && !sharedLayers(Action::RecipeBack, Action::Transfer),
              "named pairs: two item screen keys and two window keys conflict; back and Sort, Sort and Settings do not (L-372)");
        Bindings bindings{};
        bindings[static_cast<size_t>(Action::Sort)] = Chord{Token{Device::Key, 0x08}};
        auto backConflicts = bindingConflicts(bindings, Action::RecipeBack);
        check(std::none_of(backConflicts.begin(), backConflicts.end(), [](Conflict c) { return c.action == Action::Sort; }),
              "Sort on Backspace is not reported against back in the recipe view");
        bindings[static_cast<size_t>(Action::Sort)] = Chord{Token{Device::Key, 0x52}};
        auto rConflicts = bindingConflicts(bindings, Action::ShowRecipes);
        check(rConflicts.size() == 1 && rConflicts[0].action == Action::Sort && rConflicts[0].where == layer::itemScreen,
              "a conflict names where the two bindings meet");
    }
    check(actionAllowed(Action::Settings, false, false, false, true) && !actionAllowed(Action::Zoom, false, false, false, true)
        && !actionAllowed(Action::Settings, false, false), "outside a world only the settings key opens (L-175)");
    check(lamium::outsideWorldMenu("ui://./hbui/index.html", false) && !lamium::outsideWorldMenu("ui://./hbui/index.html", true)
        && lamium::outsideWorldMenu("start_screen", false)
        && !lamium::outsideWorldMenu("world_loading_progress_screen - local_world_load", false)
        && !lamium::outsideWorldMenu("hud_screen", false), "the title screen's menus count, loading screens and play do not");
    check(lamium::ui::nav::needsWorld(lamium::ui::nav::waypoints) && lamium::ui::nav::needsWorld(lamium::ui::nav::hud)
        && !lamium::ui::nav::needsWorld(0) && !lamium::ui::nav::needsWorld(lamium::ui::nav::hotkeys), "settings lists and keys work outside a world; world views do not");
    Bindings fresh;
    check(effectiveChord(fresh, Action::Settings) == Chord{Token{Device::Key, 0x4C}}
        && effectiveChord(fresh, Action::ChunkBorders) == defaultChord(Action::ChunkBorders)
        && effectiveChord(fresh, Action::NightVision).empty(), "absent bindings use Lamium defaults");
    fresh[static_cast<size_t>(Action::Settings)] = Chord{Token{Device::Key, 0x46}};
    fresh[static_cast<size_t>(Action::ChunkBorders)] = Chord{};
    fresh[static_cast<size_t>(Action::NightVision)] = Chord{Token{Device::Key, 0x4a}};
    check(effectiveChord(fresh, Action::Settings) == Chord{Token{Device::Key, 0x46}},
        "an override beats the default");
    check(effectiveChord(fresh, Action::ChunkBorders).empty()
        && effectiveChord(fresh, Action::NightVision) == Chord{Token{Device::Key, 0x4a}}
        && effectiveChord(fresh, Action::Zoom) == Chord{Token{Device::Key, 0x43}},
        "explicit unbind and old custom J stay while other defaults apply");
    check(!canClear(Action::Settings) && canClear(Action::Zoom) && canClear(Action::ChunkBorders),
        "only the settings action refuses Clear");
    Chord const lKey{Token{Device::Key, 0x4c}};
    check(completesMenuBinding(Chord{Token{Device::Mouse, 5}}, Chord{Token{Device::Mouse, 5}}, Token{Device::Mouse, 5})
        && completesMenuBinding(lKey, lKey, Token{Device::Key, 0x4c}),
        "menu close matches keyboard and side-button completing presses");
    check(!completesMenuBinding(Chord{Token{Device::Mouse, 2}}, Chord{Token{Device::Mouse, 2}}, Token{Device::Mouse, 2})
        && !completesMenuBinding(Chord{Token{Device::Mouse, 1}}, Chord{Token{Device::Mouse, 1}}, Token{Device::Mouse, 1})
        && completesMenuBinding(Chord{Token{Device::Key, 0x11}, Token{Device::Mouse, 2}},
            Chord{Token{Device::Key, 0x11}, Token{Device::Mouse, 2}}, Token{Device::Mouse, 2}),
        "a lone left/right click stays a menu click; a modified one still closes");
    Chord mouseChord{Token{Device::Key, 0x11}, Token{Device::Mouse, 5}};
    check(completesMenuBinding(mouseChord, mouseChord, mouseChord.back())
        && !completesMenuBinding(mouseChord, Chord{mouseChord.back()}, mouseChord.back())
        && !completesMenuBinding(mouseChord, Chord{mouseChord.back(), mouseChord.front()}, mouseChord.back())
        && !completesMenuBinding({}, mouseChord, mouseChord.back()),
        "menu close requires all chord inputs in order and its completing token");
    check(closesSettings(lKey, 0x4c) && !closesSettings(lKey, 0x4d)
              && !closesSettings(Chord{Token{Device::Key, 0x11}, Token{Device::Key, 0x4c}}, 0x4c)
              && !closesSettings(Chord{Token{Device::Mouse, 3}}, 3) && !closesSettings(Chord{Token{Device::Key, 0x0d}}, 0x0d)
              && !closesSettings(Chord{Token{Device::Key, 0x28}}, 0x28),
          "the single Settings key closes the screen, unless the screen uses that key");
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
        keys.bind(longA, {shift, b, left});
        keys.press(b);
        check(only(keys.press(shift), {}), "Shift after B completes nothing");
        keys.release(b);
        check(only(keys.press(b), {on(shortA)}), "ordinary B activates with Shift held");
        check(only(keys.press(left), {off(shortA), on(longA)}), "a longer ordinary chord takes over from an active shorter one");
        check(only(keys.release(left), {off(longA)}) && !keys.dispatch.isActive(shortA),
            "the shorter ordinary chord does not resume when the longer one ends");
        check(only(keys.release(b), {}) && only(keys.press(b), {on(shortA)}), "it activates again on a fresh press");
    }
    {
        // Java's F3: a key that begins a longer chord acts on release unless
        // the longer chord was used while it was held.
        Keys keys;
        auto const debug = Action::DebugView;
        keys.bind(debug, {f3});
        keys.bind(longA, {f3, b});
        keys.bind(shortA, {Token{Device::Key, 0x4a}});
        auto pressed = keys.press(f3);
        check(only(pressed, {}) && pressed.consumed, "F3 does nothing on press but belongs to Lamium");
        check(only(keys.press(f3), {}), "F3 repeats keep waiting");
        check(only(keys.release(f3), {on(debug)}), "F3 alone fires on release");
        keys.press(f3);
        check(only(keys.press(b), {on(longA)}), "F3+B fires on B's press");
        check(only(keys.release(b), {off(longA)}) && only(keys.press(b), {on(longA)}), "F3+B repeats while F3 stays held");
        keys.release(b);
        check(only(keys.release(f3), {}), "F3 is silent after F3+B was used");
        keys.press(f3);
        keys.press(w);
        check(only(keys.press(Token{Device::Key, 0x4a}), {on(shortA)}), "an unrelated action still fires while F3 is held");
        keys.release(w);
        keys.release(Token{Device::Key, 0x4a});
        check(only(keys.release(f3), {on(debug)}), "walking or other actions do not cancel F3");
        keys.press(f3);
        keys.focusLost();
        check(keys.dispatch.reset().empty() && only(keys.release(f3), {}), "focus loss drops a waiting F3");
        keys.press(f3);
        auto elsewhere = keys.chords;
        keys.chords = {};
        check(only(keys.release(f3), {}), "a waiting F3 does not fire once its action is no longer allowed");
        keys.chords = elsewhere;
        keys.bind(longA, {});
        check(only(keys.press(f3), {on(debug)}), "without a longer chord F3 fires on press");
        keys.release(f3);
        keys.bind(Action::Zoom, {f3});
        keys.bind(longA, {f3, b});
        check(only(keys.press(f3), {on(Action::Zoom)}), "a hold action leading a longer chord still acts on press");
    }
    {
        Keys keys;
        for (size_t i = 0; i < actions.size(); ++i)
            keys.bind(static_cast<Action>(i), defaultChord(static_cast<Action>(i)));
        Token g{Device::Key, 0x47};
        check(only(keys.press(f3), {}) && only(keys.release(f3), {on(Action::DebugView)}),
            "default F3 toggles Debug View on release");
        keys.press(f3);
        check(only(keys.press(b), {on(Action::Hitboxes)}), "default F3+B toggles Hitboxes");
        check(only(keys.release(b), {off(Action::Hitboxes)}) && only(keys.release(f3), {}),
            "default F3+B does not also toggle Debug View");
        keys.press(f3);
        check(only(keys.press(g), {on(Action::ChunkBorders)}), "default F3+G toggles Chunk Borders");
        check(only(keys.release(g), {off(Action::ChunkBorders)}) && only(keys.release(f3), {}),
            "default F3+G does not also toggle Debug View");
    }
    {
        Bindings bindings;
        bindings[static_cast<size_t>(Action::DebugView)] = Chord{f3};
        bindings[static_cast<size_t>(Action::ChunkBorders)] = Chord{f3, b};
        bindings[static_cast<size_t>(Action::Zoom)] = Chord{f3};
        bindings[static_cast<size_t>(Action::NightVision)] = Chord{b};
        bindings[static_cast<size_t>(Action::Sort)] = Chord{Token{Device::Key, 0x4a}};
        bindings[static_cast<size_t>(Action::Hitboxes)] = Chord{{Device::Key, 0x4a}, b};
        check(firesOnRelease(bindings, Action::DebugView) == Action::ChunkBorders
            && !firesOnRelease(bindings, Action::ChunkBorders) && !firesOnRelease(bindings, Action::Zoom)
            && !firesOnRelease(bindings, Action::NightVision) && !firesOnRelease(bindings, Action::Sort),
            "the Hotkeys list names release-triggered actions; hold, trailing and container-only keys stay on press");
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
        Token const left{Device::Mouse, 1}, right{Device::Mouse, 2}, middle{Device::Mouse, 3}, side{Device::Mouse, 4};
        check(gameClick({left}) == GameClick::Attack && gameClick({right}) == GameClick::Use && gameClick({middle}) == GameClick::Pick,
              "a mouse button alone is the game's own click");
        check(gameClick({side}) == GameClick::None && gameClick({Token{Device::Key, 0x12}, left}) == GameClick::None
              && gameClick({b}) == GameClick::None && gameClick({}) == GameClick::None,
              "side buttons, chords with a modifier and keys are not the game's click");
        check(gameClick(Action::Zoom, {middle}) == GameClick::Pick && gameClick(Action::Sort, {middle}) == GameClick::None
              && gameClick(Action::Transfer, {right}) == GameClick::Use,
              "only an action that works in play is told it shares the game's click");
        GameScreenKeys const game{{'E'}, {'Q'}, {'1', '2', '3', '4', '5', '6', '7', '8', '9'}};
        Token const e{Device::Key, 'E'}, q{Device::Key, 'Q'}, three{Device::Key, '3'}, shift{Device::Key, 0x10};
        check(gameScreenKey(Action::Sort, {e}, game) == GameScreenKey::Inventory
              && gameScreenKey(Action::Sort, {shift, q}, game) == GameScreenKey::Drop
              && gameScreenKey(Action::RecipeBack, {three}, game) == GameScreenKey::Hotbar,
              "an item-screen or window binding on the game's inventory, drop or hotbar key is marked");
        check(gameScreenKey(Action::Zoom, {e}, game) == GameScreenKey::None
              && gameScreenKey(Action::Sort, {Token{Device::Key, 'R'}}, game) == GameScreenKey::None
              && gameScreenKey(Action::Sort, {Token{Device::Mouse, 'E'}}, game) == GameScreenKey::None
              && gameScreenKey(Action::Sort, {e}, GameScreenKeys{}) == GameScreenKey::None,
              "play-only actions, other keys, mouse buttons and unread game keys are not marked");
        Keys keys;
        keys.bind(shortA, {right});
        keys.bind(Action::DebugView, {alt, right});
        check(only(keys.press(right), {on(shortA)}) && !keys.dispatch.takes(keys.chords, right),
              "a mouse button bound alone leaves its click to the game");
        keys.release(right);
        keys.press(alt);
        check(only(keys.press(right), {on(Action::DebugView)}) && keys.dispatch.takes(keys.chords, right)
              && !keys.dispatch.takes(keys.chords, left), "a chord with a modifier takes its click");
        keys.release(right);
        keys.bind(longA, {alt, right, b});
        auto waiting = keys.press(right);
        check(only(waiting, {}) && waiting.consumed && keys.dispatch.takes(keys.chords, right),
              "a chord waiting for its release takes its click too");
        keys.dispatch.reset();
        check(!keys.dispatch.takes(keys.chords, right), "a reset dispatcher owns nothing");
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
        auto links = [](std::vector<Conflict> const& list) {
            std::vector<std::pair<Action, Link>> result;
            for (auto x : list) result.emplace_back(x.action, x.link);
            return result;
        };
        check(links(conflicts) == std::vector<std::pair<Action, Link>>{{Action::Hitboxes, Link::Same},
            {Action::DebugView, Link::StartsWithThis}, {Action::ChunkBorders, Link::ContainsThis}},
            "the tooltip groups same, leading and containing chords in that order");
        check(links(bindingConflicts(bindings, Action::ChunkBorders)) == std::vector<std::pair<Action, Link>>{
            {Action::NightVision, Link::InsideThis}, {Action::Hitboxes, Link::InsideThis}, {Action::DebugView, Link::Reordered}},
            "shorter chords inside this one and reordered chords are grouped last");
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
    check(lamium::ui::keyLabel(0x12, "ALTERNATIVE") == "ALT" && lamium::ui::keyLabel(0xA4, "ALTERNATIVE") == "ALT"
              && lamium::ui::keyLabel(0xA5, "ALTERNATIVE") == "ALT",
          "the long Alt name is shortened for the generic and both side codes");
    check(lamium::ui::keyLabel(0x11, "CONTROL") == "CTRL" && lamium::ui::keyLabel(0xA2, "CONTROL") == "CTRL"
              && lamium::ui::keyLabel(0xA3, "CONTROL") == "CTRL",
          "the long Ctrl name is shortened for the generic and both side codes");
    check(lamium::ui::keyLabel(0x10, "SHIFT") == "SHIFT" && lamium::ui::keyLabel(0x52, "R") == "R",
          "short names and ordinary keys keep the game's name");
}

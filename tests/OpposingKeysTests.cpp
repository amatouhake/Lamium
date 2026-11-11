#include "input/OpposingKeys.h"
void check(bool, char const*);
void opposingKeysTests() {
    using lamium::input::OpposingKeys;
    using lamium::input::OpposingResume;
    constexpr bool back = false, forward = true;
    {
        OpposingKeys keys;
        check(keys.direction() == 0, "no key, no direction");
        keys.press(forward);
        check(keys.direction() == 1, "one key moves its way");
        keys.press(back);
        check(keys.direction() == -1, "the key pressed last wins while both are held");
        keys.press(forward);
        check(keys.direction() == -1, "a repeated key-down of a held key does not retake the lead");
        keys.release(back);
        check(keys.direction() == 1, "releasing the winner hands back to the key still held");
        keys.press(back);
        keys.release(forward);
        check(keys.direction() == -1, "releasing the loser changes nothing");
        keys.release(back);
        check(keys.direction() == 0 && !keys.held(back) && !keys.held(forward), "all released is neutral");
        keys.release(back);
        check(keys.direction() == 0, "a stray release is ignored");
    }
    {
        OpposingKeys keys(OpposingResume::Repress);
        keys.press(forward);
        keys.press(back);
        keys.release(back);
        check(keys.direction() == 0 && keys.held(forward), "with re-press, the key still held does not resume");
        keys.press(back);
        check(keys.direction() == -1, "a new press of the other key still moves");
        keys.release(back);
        check(keys.direction() == 0, "and the suppressed key stays quiet after it");
        keys.release(forward);
        keys.press(forward);
        check(keys.direction() == 1, "pressing it again moves again");
        keys.press(back);
        keys.release(forward);
        check(keys.direction() == -1, "releasing the loser never suppresses the winner");
    }
    {
        OpposingKeys keys;
        keys.press(forward);
        keys.press(back);
        keys.clear();
        check(keys.direction() == 0 && !keys.held(forward) && !keys.held(back), "focus loss leaves no held key");
        keys.release(forward);
        keys.press(forward);
        check(keys.direction() == 1, "a release that arrives after the clear does not break the next press");
    }
}

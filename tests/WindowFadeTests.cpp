#include "ui/WindowFade.h"
#include <cmath>
void check(bool, char const*);
void windowFadeTests() {
    using lamium::ui::WindowFade;
    constexpr float screen = 300, height = 100; // centred, the window rests at 100
    {
        WindowFade fade;
        double t = 0;
        float const rest = fade.see("chest", 300, height, screen, t);
        check(rest == 100, "a window never seen is guessed centred");
        check(fade.opacity(300, screen) == 0, "at the screen's bottom the parts are not shown");
        fade.see("chest", 200, height, screen, t += 16);
        check(std::abs(fade.opacity(200, screen) - (1 - 100.f / 100)) < 1e-4f, "half way to the bottom is nothing");
        fade.see("chest", 150, height, screen, t += 16);
        check(std::abs(fade.opacity(150, screen) - .5f) < 1e-4f, "the parts fade by the distance from rest");
        fade.see("chest", 100, height, screen, t += 16);
        check(fade.rest() == 100 && fade.opacity(100, screen) == 1, "at rest the parts show whole");
        fade.see("chest", 140, height, screen, t += 16);
        check(fade.rest() == 100 && fade.opacity(140, screen) < 1, "sliding away keeps the rest and fades");
    }
    {
        WindowFade fade;
        double t = 0;
        fade.see("tall", 280, height, screen, t);
        for (int i = 0; i < 10; ++i) fade.see("tall", 280 - 20.f * i, height, screen, t += 16);
        for (int i = 0; i < 5; ++i) fade.see("tall", 120, height, screen, t += 16);
        check(fade.rest() == 100 && fade.opacity(120, screen) < 1, "a window resting below the guess is faded at first");
        for (int i = 0; i < 20; ++i) fade.see("tall", 120, height, screen, t += 16);
        check(fade.rest() == 120 && fade.opacity(120, screen) == 1, "standing still for 20 frames is rest");
        fade.see("tall", 290, height, screen, t += 1000);
        check(fade.rest() == 120, "reopened, a screen seen before starts from its own rest");
        fade.see("other", 290, height, screen, t += 1000);
        check(fade.rest() == 100, "another screen starts from its own guess");
        fade.see("tall", 60, height, screen, t += 1000);
        check(fade.rest() == 60, "a window above its rest rests there");
    }
}

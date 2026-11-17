#pragma once
// L-359 step 1: what the item list beside the game's item screens can rely
// on. Logs each container screen's name, its window's box and the controls
// around it (to find the recipe book), and draws 300 item icons beside the
// window to time them. The same option lets Lamium's screen open over any
// container screen. Only active with `xmake f --item_screen_probe=y`.
#include <string>
namespace lamium::researchTrace::itemScreen {
void start();
void stop();
// The probe's text field under a game screen (F9: focus; F10: focus with the
// platform keyboard claimed; Esc: leave). True when the probe took the text
// or the key, which then must not reach the game.
bool type(std::string const& text);
bool key(int code, bool down);
}

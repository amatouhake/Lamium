#pragma once
// L-359 steps 7 to 9 and L-165, read-only: what the game does when a recipe
// is picked in its recipe book and when the output is taken (the screen's
// slot calls, the chosen recipe and the grid whenever they change, the
// server's answers and, in a local world, the craft its server is asked
// for), and, once per item screen, its controls and its item collections.
// Not read-only: F7, F8 and F9 on a crafting screen lay a recipe into the
// grid and take the output through the slot calls Item transfer uses (see
// the .cpp). Only active with `xmake f --craft_trace=y`.
namespace lamium::researchTrace::craft {
void start();
void stop();
}

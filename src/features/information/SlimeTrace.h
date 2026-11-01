#pragma once
// L-232 check: compares the slime chunk rule against the game's own slime
// spawn rule. Only active with `xmake f --slime_trace=y`.
namespace lamium::information::slimeTrace {
void start();
void stop();
}

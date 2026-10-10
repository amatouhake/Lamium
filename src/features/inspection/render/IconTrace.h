#pragma once
// Icon research (L-91 leather/shield, L-119 fence gates): how vanilla
// inventory slots draw special items (render passes, UI materials, block
// routes, icon blits) next to Lamium's own icon calls. Only active with
// `xmake f --icon_trace=y`.
namespace lamium::inspection::iconTrace {
void start();
void stop();
}

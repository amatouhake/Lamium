#pragma once
// L-96 research spike: trim the border of glass faces on sides that touch the
// same glass, from the block tessellator's per-face calls, with a log. Only
// active with `xmake f --ctm_trace=y`.
namespace lamium::visuals::connectedTexturesTrace {
void start();
void stop();
}

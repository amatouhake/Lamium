#pragma once
// L-96 research: which chunks the newer block pipeline builds, against the
// glass the Connected Textures hooks see. Only active with
// `xmake f --ctm_trace=y`.
namespace lamium::visuals::connectedTexturesTrace {
void start();
void stop();
}

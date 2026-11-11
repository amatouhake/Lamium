#pragma once
// Read-only probes for L-249, L-33 and L-30 (2026-11-12).
// Only active in builds configured with `xmake f --world_probe=y`.
namespace lamium::researchTrace::worldProbe {
void start();
void stop();
}

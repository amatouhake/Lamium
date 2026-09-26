#pragma once
// Bounded diagnostics for BACKLOG research items (L-37, L-40; F9/F10 experiments).
// Only active in builds configured with `xmake f --research_trace=y`.
namespace lamium::researchTrace {
void start();
void stop();
}

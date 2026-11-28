#pragma once
// L-265 research (assist_trace builds only): what the server answers to the
// assist's placements and hand swaps, to find why a block carried only in
// the inventory sometimes reverts after it looked placed.
class Packet;
namespace lamium::schematic::assist::trace {
#ifdef LAMIUM_ASSIST_TRACE
void start();
void stop();
// An outgoing packet, from the assist's send hook: inventory transactions
// are logged.
void sent(Packet& packet) noexcept;
void line(char const* text) noexcept;
#endif
}

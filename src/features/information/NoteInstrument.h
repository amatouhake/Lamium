#pragma once
#include <optional>

namespace lamium::information {
// What a neighbor cell says about a note block's instrument: unknown when
// nothing says what stands there, otherwise the instrument the block there
// declares. `instrument` empty means it declares nothing for that side, air
// included.
template <class Instrument>
struct Declared {
    bool known = false;
    std::optional<Instrument> instrument;
};
// Some block types store an engaged `none` where others store nothing at all;
// both mean the block declares no instrument there. A caller reads a component
// through this, so an engaged none never counts as a decision.
template <class Instrument>
std::optional<Instrument> declaresNoInstrument(std::optional<Instrument> stored, Instrument none) {
    if (stored && *stored == none) return std::nullopt;
    return stored;
}
// L-184: the instrument a note block plays, from what its two neighbors
// declare. A block declares one instrument for a note block under it (a mob
// head) and one for a note block standing on it; the one above wins, and a
// note block with neither plays the harp. Checked in game against the game's
// own answer on 1.26.51.01. None while an unknown neighbor could still decide.
template <class Instrument>
std::optional<Instrument> noteInstrument(Declared<Instrument> above, Declared<Instrument> below, Instrument harp) {
    if (!above.known) return std::nullopt;
    if (above.instrument) return above.instrument;
    if (!below.known) return std::nullopt;
    return below.instrument ? *below.instrument : harp;
}
}

#pragma once
#include <optional>

namespace lamium::information {
// A note block's level sound event packs two things into its data: the
// instrument above the note in the high byte, the note itself below it. The
// probe of 2026-11-03 read 14-16 under air (harp), 257-259 under stone (bass
// drum), 769-771 under glass (hat) and 1793-1795 under wool (guitar) - the
// same order as NoteBlockInstrument.
inline std::optional<int> noteFromEventData(int data) {
    if (data < 0) return std::nullopt;
    int note = data % 256;
    int instrument = data / 256;
    // 0-24 is the note block's range; 25 is the last instrument (piglin).
    if (note > 24 || instrument > 25) return std::nullopt;
    return note;
}
}

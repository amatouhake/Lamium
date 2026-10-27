#pragma once
#include <string>
#include <string_view>

namespace lamium::inventory {
// L-263: whether a pick worked is read back from the slot on a later frame,
// never taken from the transaction being sent: a sent move only means the
// server was asked, and a server that refuses it puts the slot back to what
// it held before. The read waits first: right after the send the slot holds
// what the move predicted, and only the server's answer replaces it.
inline constexpr int pickConfirmFrames = 60; // a server answer needs a tick or two; 1 s at 60 fps
// The pick waiting for that read: the slot it asked for and the item it
// asked to find there.
struct PickPending {
    int slot = -1;
    int frame = 0;
    std::string name;
    short aux = 0;
    bool waiting() const { return slot >= 0 && !name.empty(); }
    // Whether the pick recorded on `frame` should be read at `now`: only
    // after the frame it happened on, and late enough for the server's
    // answer to have replaced the predicted slot.
    bool due(int now) const { return waiting() && now - frame >= pickConfirmFrames; }
    void clear() { *this = PickPending{}; }
};
// Whether the slot holds what the pick asked for. A variant counts only with
// its own data value: a skull or monster egg of another kind is not it.
inline bool pickConfirmed(std::string_view heldName, short heldAux, std::string_view wantedName, short wantedAux) {
    return !wantedName.empty() && heldName == wantedName && heldAux == wantedAux;
}
}

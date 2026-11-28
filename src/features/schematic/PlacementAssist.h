#pragma once
// Placement assist (L-265, docs/PLACEMENT-ASSIST.md): using a block on a
// schematic ghost places the ghost's block there in the ghost's state.
namespace lamium::schematic::assist {
void start();
void stop();
// Focus loss: a placement waiting for its faked look is dropped.
void focusLost();
// The ghost a press would place, worked out each tick while the assist is
// on (L-265: shown before the press): its drawn outline's box in the world
// and whether a press now would place it. Any thread.
struct Aim {
    int dimension = 0;
    double low[3]{}, high[3]{};
    bool placeable = false;
};
bool aimed(Aim& out);
}

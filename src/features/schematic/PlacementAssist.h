#pragma once
// Placement assist (L-265, docs/PLACEMENT-ASSIST.md): using a block on a
// schematic ghost places the ghost's block there in the ghost's state.
namespace lamium::schematic::assist {
void start();
void stop();
// Focus loss: a placement waiting for its faked look is dropped.
void focusLost();
}

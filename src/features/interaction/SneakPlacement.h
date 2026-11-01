#pragma once
#include <optional>

namespace lamium::interaction::sneakPlacement {
// Uses on interactive blocks act as if sneaking (BACKLOG L-293).
void start();
void stop();
// The sneak state a use with an item on an interactive block gets, given the
// player's own; nothing while the feature is off. Fake Offhand decides
// between opening the block and using its slot with it.
std::optional<bool> sneakForUse(bool userSneaking);
}

#pragma once
#include "features/recipes/Places.h"
#include <optional>
class ScreenView;

// L-359 step 8: the container that is open, as a place that can be counted:
// a block's storage (chest, barrel, shulker box) or the ender chest, with what
// it holds now. Read a few times a second, behind the CountedPlaces
// capability. Client thread only; the screen is the focused one of this frame.
namespace lamium::recipes::placesGame {
struct Open {
    bool ender = false;
    Spot spot;
    std::optional<Spot> pair; // a double chest's other half
    std::string block;
    std::vector<Stored> items;
};
// Nothing for a screen that is no such container (a minecart, the inventory, a furnace).
std::optional<Open> const& open(ScreenView& screen);
}

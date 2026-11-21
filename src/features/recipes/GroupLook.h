#pragma once
#include "features/recipes/RecipeIndex.h"
#include <map>

namespace lamium::recipes::view { struct Late; }

// L-369: an ingredient any of a group's members serves, as every recipe screen shows it. It holds still: the
// member "How to make" picked, else the one most held, else the one it is known by (groupLead). The wheel over
// its slot, once the pointer has rested there a moment, turns through the members to look at them; it changes
// nothing chosen. Client thread only; what was looked at lasts for the session.
namespace lamium::recipes::groupLook {
// Once a frame, before anything is drawn.
void beginFrame();
// The pointer is over a slot of `group` at (x, y) this frame.
void hover(int group, float x, float y);
// The group whose slot the pointer rested on last frame long enough for the wheel, or -1.
int rested();
// Turns what `group` shows by `steps` members.
void turn(int group, int steps);
// What `group` shows. `held`: what is held by item, or null where it is not known.
Item face(Index const& index, int group, std::map<int, Item> const& picks, std::map<Item, Count> const* held);
// The tip over a group's slot: its name, the member shown, whether one is chosen and the members as icons, the
// shown one framed white and the chosen one green over it; the wheel's use goes to the hint line. `held` only
// decides the member shown. The "*" marks a group's slot always, chosen or not (L-369): it says the slot is a
// kind; the picture says which.
void tip(view::Late& late, Index const& index, int group, std::map<int, Item> const& picks, std::map<Item, Count> const* held);
}

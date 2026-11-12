#pragma once
#include "features/inspection/render/ItemIcon.h"
#include "features/inventory/EnchantOrderGame.h"
#include "ui/EnchantLayout.h"
#include <string>
#include <vector>
class MinecraftUIRenderContext;
// The enchantment order's steps' side (BACKLOG L-170): the goal, the total
// with the player's level and the materials count, the detail, the steps and
// a last line saying what to do next. One drawing for the planner screen's
// right column and for the panel right of the anvil screen.
//
// Steps are told apart in three ways and no more (decided 2026-11-12): a name
// in the warning colour is not at hand, a faint step is done, a white bar
// marks the next step. At the anvil the step whose two stacks lie in the
// slots is tinted as well.
namespace lamium::ui::enchant_steps {
using Zone = EnchantStepsLayout::Zone;
// 1 when the two details fit side by side in `width`, else 2.
int detailLines(MinecraftUIRenderContext&, inventory::enchantOrder::game::StepsInfo const&, float width);
// How many lines the last line takes in `width`, up to three.
int footLines(MinecraftUIRenderContext&, inventory::enchantOrder::game::StepsInfo const&, float width);
// A side's names that were cut, shown whole in a box by the row while the pointer is on it, as the settings
// table does for a value that is cut. Drawn by the caller after its icons, so it is on top of them.
struct Tip {
    bool show = false;
    float x = 0, y = 0, width = 0;
    int lines = 1;
    std::string text;
};
void drawTip(MinecraftUIRenderContext&, Tip const&);
// `pointerX/Y`: where the pointer is, or outside the layout for none. `goalLive`: the goal can be switched here
// (the planner is open).
Tip draw(MinecraftUIRenderContext&, EnchantStepsLayout const&, inventory::enchantOrder::game::StepsInfo const&, float pointerX, float pointerY,
         bool goalLive, std::vector<inspection::render::IconAt>& icons);
// What the status line says about the zone under the pointer; empty for none.
std::string status(inventory::enchantOrder::game::StepsInfo const&, Zone hover);
}

#include "ui/EnchantView.h"
#include "ui/EnchantChooser.h"
#include "ui/EnchantLayout.h"
#include "ui/EnchantSteps.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/ScreenParts.h"
#include "ui/SettingsTableView.h"
#include "ui/Widgets.h"
#include "features/inspection/render/ItemIcon.h"
#include "features/inventory/EnchantOrderGame.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/world/item/ItemStack.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace lamium::ui::enchant_view {
namespace {
using Zone = EnchantLayout::Zone;
namespace order = inventory::enchantOrder;
namespace game = inventory::enchantOrder::game;
bool open = false;
int rowFirst = 0, stepFirst = 0;
enum class Drag { None, Rows, Steps } dragging = Drag::None;
EnchantLayout last;
glm::vec2 lastPointer{};

void back() { reset(); table_view::rebuild(true); }
// Icons are collected while the view draws and drawn in one call at the end,
// each from a stack of its own: that is how glints survive (the container
// previews do the same).
std::vector<inspection::render::IconAt> icons;
}
bool begin() {
    reset();
    auto* client = screen::client();
    if (!client) { screen::setMessage(translated("worldOnly")); return false; }
    table_view::endSearch();
    screen::finishEditing();
    std::string problem;
    if (!game::open(*client, problem)) { screen::setMessage(translated(problem)); return false; }
    open = true;
    return true;
}
bool opened() { return open; }
void reset() {
    if (open) game::close();
    open = false;
    rowFirst = stepFirst = 0;
    dragging = Drag::None;
    last = {};
}
bool click(float x, float y, bool right) {
    if (!last.usable() || x < last.left || x >= last.left + last.width || y < last.headTop || y >= last.footerTop) return false;
    if (right) return true;
    if (last.rows.onScrollbar(x, y)) { rowFirst = last.rows.firstAt(y); dragging = Drag::Rows; return true; }
    if (last.side.rows.onScrollbar(x, y)) { stepFirst = last.side.rows.firstAt(y); dragging = Drag::Steps; return true; }
    auto hit = last.hit(x, y);
    screen::clearMessage();
    switch (hit.zone) {
    case Zone::Back: back(); return true;
    case Zone::Goal: game::flipGoal(); stepFirst = 0; return true;
    default: {
        // The choosing part's zones, shared with the anvil's panel.
        std::string message;
        auto const pressed = enchant_chooser::press(hit, message);
        if (!message.empty()) screen::setMessage(message);
        if (pressed == enchant_chooser::Pressed::NewItem) rowFirst = 0;
        if (pressed != enchant_chooser::Pressed::No && hit.zone != Zone::RowMinus && hit.zone != Zone::RowPlus) stepFirst = 0;
        return true;
    }
    }
}
void key(int key) {
    if (key == 0x1b) back();
}
void wheel(int amount) {
    if (lastPointer.x < last.dividerX) rowFirst = SettingsTable::clampFirst(rowFirst + amount, last.rows.listCount, last.rows.listVisible);
    else stepFirst = SettingsTable::clampFirst(stepFirst + amount, last.side.rows.listCount, last.side.rows.listVisible);
}
void release() { dragging = Drag::None; }
void renderContent(MinecraftUIRenderContext& context, SettingsTable const& t, glm::vec2 pointer) {
    lastPointer = pointer;
    icons.clear();
    int itemsDrawn = 0;
    auto const& v = game::view();
    // Where the player stands with the plan, from the inventory as it is now.
    if (auto* client = screen::client()) game::followInventory(*client);
    auto const& info = game::steps();
    int const stepCount = info.planned ? static_cast<int>(info.steps.size()) : 0;
    auto fit = [&](int detailLines) {
        return EnchantLayout::fit(t, static_cast<int>(v.kinds.size()), v.heldCell, static_cast<int>(v.rows.size()), rowFirst, stepCount, stepFirst,
            detailLines);
    };
    last = fit(1);
    // The two details go on a line each where the column does not hold them side by side.
    if (last.usable() && last.side.usable())
        if (int const lines = enchant_steps::detailLines(context, info, last.side.width); lines != 1) last = fit(lines);
    if (!last.usable()) return;
    if (dragging == Drag::Rows) last.rows.listFirst = last.rows.firstAt(pointer.y);
    if (dragging == Drag::Steps) last.side.rows.listFirst = last.side.rows.firstAt(pointer.y);
    rowFirst = last.rows.listFirst = SettingsTable::clampFirst(last.rows.listFirst, last.rows.listCount, last.rows.listVisible);
    stepFirst = last.side.rows.listFirst = SettingsTable::clampFirst(last.side.rows.listFirst, last.side.rows.listCount, last.side.rows.listVisible);
    auto hover = last.hit(pointer.x, pointer.y);
    float const pad = SettingsTable::pad;

    // Heading row: where this screen belongs, and the way back to it.
    label(context, t.nameX, t.theadTop + 2, last.backX - t.nameX - SettingsTable::gap,
        translated("section.inventory") + "  >  " + translated("feature.enchantOrder"), palette::faint);
    drawSmallButton(context, last.backX, t.theadTop, ShapesLayout::closeWidth, 11, "< " + translated("section.inventory"), hover.zone == Zone::Back);
    fill(context, t.tableLeft, t.rowsTop - 1, t.tableWidth, 1, palette::white, .14f);
    fill(context, last.dividerX, t.rowsTop, 1, last.footerTop - t.rowsTop, palette::white, .14f);

    // Left: the item, the kinds and the main hand's cell, the sets, the rows.
    enchant_chooser::draw(context, last, v, hover, icons, itemsDrawn);

    // Right: the goal, the totals, the steps. The same drawing as right of the anvil screen.
    auto const sideZone = hover.zone == Zone::Goal ? enchant_steps::Zone::Goal : hover.zone == Zone::AtOnce ? enchant_steps::Zone::AtOnce
        : hover.zone == Zone::Result ? enchant_steps::Zone::Result : enchant_steps::Zone::None;
    enchant_steps::draw(context, last.side, info, sideZone, true, icons);

    // Footer, as in the table: status first, then what the screen is, then the limit.
    fill(context, t.left, t.footerTop, t.width, 1, palette::white, .14f);
    float const textLeft = t.left + pad, footWidth = t.width - 2 * pad;
    std::string status = screen::message();
    Rgb statusColor = palette::warning;
    if (status.empty()) {
        statusColor = palette::text;
        status = enchant_chooser::status(v, hover);
        if (status.empty()) status = enchant_steps::status(info, sideZone);
        if (status.empty()) status = translated("enchantOrder.fresh");
    }
    label(context, textLeft, t.footerTop + 3, footWidth, status, statusColor);
    label(context, textLeft, t.footerTop + 16, footWidth, translated("help.enchantOrderList"), palette::dim);
    label(context, textLeft, t.footerTop + 30, footWidth, translated(v.creative ? "enchantOrder.limit.creative" : "enchantOrder.limit"), palette::faint);
    context.flushText(0, std::nullopt);
    if (!icons.empty()) inspection::render::drawItemIcons(context, icons, 17);
    icons.clear();
}
}

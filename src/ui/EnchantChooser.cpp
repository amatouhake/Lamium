#include "ui/EnchantChooser.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/Widgets.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/world/item/ItemStack.h"
#include <algorithm>
#include <cmath>

namespace lamium::ui::enchant_chooser {
namespace {
using Zone = EnchantLayout::Zone;
namespace order = inventory::enchantOrder;
namespace game = inventory::enchantOrder::game;
void drawButton(MinecraftUIRenderContext& context, float x, float y, float w, std::string text, bool hovered, bool enabled) {
    if (enabled) drawSmallButton(context, x, y, w, EnchantLayout::buttonHeight, std::move(text), hovered);
    else drawSmallButton(context, x, y, w, EnchantLayout::buttonHeight, std::move(text), false, palette::keyFill, palette::keyEdge, palette::faint);
}
bool rowZone(Zone zone) { return zone == Zone::Row || zone == Zone::RowMinus || zone == Zone::RowPlus; }
}
void addIcon(std::vector<inspection::render::IconAt>& icons, ItemStack const* stack, float x, float y, float size) {
    if (!stack) return;
    float s = drawScale(); // icons draw in GUI units, outside the widgets' scale
    icons.push_back({stack, std::round(x * s), std::round(y * s), size * s / 16, 0, 1.f, true});
}
void draw(MinecraftUIRenderContext& context, EnchantLayout const& l, game::View const& v, EnchantLayout::Hit hover,
          std::vector<inspection::render::IconAt>& icons, int& itemsDrawn) {
    using L = EnchantLayout;
    float const pad = SettingsTable::pad;
    // The item, the kinds with the item cells last, the sets, the rows.
    float const leftRight = l.dividerX - pad, leftLeft = l.left + pad;
    addIcon(icons, game::itemIcon(itemsDrawn++), leftLeft, l.itemY + 2);
    label(context, leftLeft + 14, l.itemY + 3, leftRight - leftLeft - 14,
        v.useHeld ? translated("enchantOrder.kind.held", v.itemName) : v.useSlot ? translated("enchantOrder.kind.slot", v.itemName)
            : v.useItem ? translated("enchantOrder.kind.item", v.itemName) : v.itemName);

    for (int i = 0; i < l.kindCount; ++i) {
        float const x = l.kindX(i), y = l.kindY(i);
        bool const chosen = i == v.kind && !v.useItem, over = hover.zone == Zone::Kind && hover.index == i;
        fill(context, x, y, L::cell, L::cell, chosen ? palette::accent : palette::shade, chosen ? .16f : .3f);
        frame(context, x, y, L::cell, L::cell, chosen ? palette::accent : over ? palette::white : palette::keyEdge, chosen || over ? 1 : .6f);
        addIcon(icons, game::kindIcon(i), x + 2, y + 2, 16);
    }
    // An item itself, after the kinds: choosing it plans from its state. A
    // lighter fill and frame set these cells apart from the kinds (a mark in
    // a corner would sit on the item's picture).
    auto itemCell = [&](int index, bool chosen, bool over, ItemStack const* icon) {
        float const x = l.kindX(index), y = l.kindY(index);
        fill(context, x, y, L::cell, L::cell, chosen ? palette::accent : palette::white, chosen ? .16f : .08f);
        frame(context, x, y, L::cell, L::cell, chosen ? palette::accent : over ? palette::white : palette::dim);
        addIcon(icons, icon, x + 2, y + 2, 16);
    };
    if (l.heldCell) itemCell(l.heldIndex(), v.useHeld, hover.zone == Zone::Held, game::heldIcon());
    if (l.slotCell) itemCell(l.slotIndex(), v.useSlot, hover.zone == Zone::Slot, game::slotIcon());
    drawButton(context, l.setAllX, l.setsY, l.setWide, translated("enchantOrder.setAll"), hover.zone == Zone::SetAll, true);
    drawButton(context, l.setLastX, l.setsY, l.setWide, translated("enchantOrder.setLast"), hover.zone == Zone::SetLast, v.hasLast);
    drawButton(context, l.setClearX, l.setsY, l.setNarrow, translated("enchantOrder.setClear"), hover.zone == Zone::SetClear, v.wanted > 0);
    auto const& rc = l.rows;
    label(context, rc.listLeft + pad, l.rowsTitleY + 2, rc.listWidth - 2 * pad, translated("enchantOrder.wanted", v.wanted, order::maxBooks), palette::faint);
    for (int i = rc.listFirst; i < rc.listFirst + rc.listVisible && i < rc.listCount && i < static_cast<int>(v.rows.size()); ++i) {
        auto const& row = v.rows[static_cast<size_t>(i)];
        float const y = rc.listRowY(i);
        bool const rowHover = rowZone(hover.zone) && hover.index == i;
        bool const on = row.state == game::RowState::On, fixed = row.state == game::RowState::Fixed;
        bool const off = row.state == game::RowState::Blocked || row.state == game::RowState::Full;
        rowBackground(context, rc.listLeft + 1, y, rc.listWidth - 2, ShapesLayout::rowHeight, false, rowHover && !off && !fixed);
        // Name, the level as a stepper, then the switch at the right edge. What
        // replaces them (on the item, a conflict) is written in their place.
        float const nameLeft = rc.listLeft + pad;
        float right = l.switchX() + switchWidth;
        Rgb const nameColor = off ? palette::faint : row.curse ? palette::vanilla::curse : fixed ? palette::dim : palette::text;
        if (fixed) {
            // On the item: the switch stays on and cannot be pressed; the
            // level can go up from what the item has.
            toggleSwitch(context, l.switchX(), y + 3, true);
            fill(context, l.switchX(), y + 3, switchWidth, switchHeight, palette::panel, .55f);
            float nameRight = l.switchX() - 4;
            if (!row.levelText.empty()) {
                if (row.maxLevel > row.carried) {
                    bool const raised = row.level > row.carried;
                    drawSmallButton(context, l.minusX(), y + 1, L::stepWidth, 12, "-", raised && hover.zone == Zone::RowMinus && hover.index == i,
                        palette::keyFill, palette::keyEdge, raised ? palette::text : palette::faint);
                    drawSmallButton(context, l.plusX(), y + 1, L::stepWidth, 12, "+", hover.zone == Zone::RowPlus && hover.index == i);
                }
                label(context, l.levelX(), y + 3, L::levelWidth, row.levelText, row.level > row.carried ? palette::text : palette::dim, Align::Center);
                nameRight = l.minusX() - 4;
            }
            label(context, nameLeft, y + 3, nameRight - nameLeft, row.levelText.empty() ? row.label : row.name, nameColor);
        } else if (row.state == game::RowState::Blocked) {
            std::string why = translated("enchantOrder.conflict", row.reason);
            float const w = std::min(textWidth(context, why) + 2, (right - nameLeft) * .55f);
            label(context, right - w, y + 3, w, why, palette::faint, Align::Right);
            label(context, nameLeft, y + 3, right - w - 3 - nameLeft, row.name.empty() ? row.label : row.name, nameColor);
        } else {
            toggleSwitch(context, l.switchX(), y + 3, on);
            float nameRight = l.switchX() - 4;
            if (!row.levelText.empty()) {
                if (on) {
                    drawSmallButton(context, l.minusX(), y + 1, L::stepWidth, 12, "-", hover.zone == Zone::RowMinus && hover.index == i);
                    drawSmallButton(context, l.plusX(), y + 1, L::stepWidth, 12, "+", hover.zone == Zone::RowPlus && hover.index == i);
                }
                // Off: the level it would turn on at, without its buttons.
                label(context, l.levelX(), y + 3, L::levelWidth, row.levelText, on ? palette::text : palette::faint, Align::Center);
                nameRight = l.minusX() - 4;
            }
            label(context, nameLeft, y + 3, nameRight - nameLeft, row.levelText.empty() ? row.label : row.name, nameColor);
        }
    }
    drawListScrollbar(context, rc);
}
std::string status(game::View const& v, EnchantLayout::Hit hover) {
    if (hover.zone == Zone::Kind && hover.index >= 0 && hover.index < static_cast<int>(v.kinds.size()))
        return v.kinds[static_cast<size_t>(hover.index)].name;
    if (hover.zone == Zone::Held) return translated("enchantOrder.useHeld.help", v.heldName);
    if (hover.zone == Zone::Slot) return translated("enchantOrder.useSlot.help", v.slotName);
    if (hover.zone == Zone::SetAll) return translated("enchantOrder.setAll.help");
    if (hover.zone == Zone::SetLast) return translated("enchantOrder.setLast.help");
    if (rowZone(hover.zone) && hover.index >= 0 && hover.index < static_cast<int>(v.rows.size())) {
        auto const& row = v.rows[static_cast<size_t>(hover.index)];
        return row.state == game::RowState::Fixed ? translated("enchantOrder.onItem.help")
            : row.state == game::RowState::Blocked ? translated("enchantOrder.conflict", row.reason)
            : row.state == game::RowState::Full ? translated("enchantOrder.full", order::maxBooks) : row.label;
    }
    return {};
}
Pressed press(EnchantLayout::Hit hit, std::string& message) {
    switch (hit.zone) {
    case Zone::Kind: game::pickKind(hit.index); return Pressed::NewItem;
    case Zone::Held: game::pickHeld(); return Pressed::NewItem;
    case Zone::Slot: game::pickSlot(); return Pressed::NewItem;
    case Zone::SetAll: game::setAllHighest(); return Pressed::Yes;
    case Zone::SetLast: game::restoreLast(); return Pressed::Yes;
    case Zone::SetClear: game::clearWanted(); return Pressed::Yes;
    case Zone::Row:
        switch (game::toggleRow(hit.index)) {
        case order::Change::Full: message = translated("enchantOrder.full", order::maxBooks); break;
        // What the item carries cannot come off at an anvil; a fresh plan is
        // chosen by the kind's cell, not here.
        case order::Change::Fixed: message = translated("enchantOrder.onItem.help"); break;
        default: break;
        }
        return Pressed::Yes;
    case Zone::RowMinus: game::stepRow(hit.index, -1); return Pressed::Yes;
    case Zone::RowPlus: game::stepRow(hit.index, 1); return Pressed::Yes;
    default: return Pressed::No;
    }
}
}

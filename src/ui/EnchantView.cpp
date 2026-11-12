#include "ui/EnchantView.h"
#include "ui/EnchantLayout.h"
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
void drawIcon(MinecraftUIRenderContext& context, ItemStack const* stack, float x, float y, float size = 10) {
    if (!stack) return;
    float s = drawScale(); // icons draw in GUI units, outside the widgets' scale
    inspection::render::drawItemIcon(context, {stack, std::round(x * s), std::round(y * s), size * s / 16}, 17);
}
void drawButton(MinecraftUIRenderContext& context, float x, float y, float w, std::string text, bool hovered, bool enabled) {
    if (enabled) drawSmallButton(context, x, y, w, EnchantLayout::buttonHeight, std::move(text), hovered);
    else drawSmallButton(context, x, y, w, EnchantLayout::buttonHeight, std::move(text), false, palette::keyFill, palette::keyEdge, palette::faint);
}
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
    if (last.steps.onScrollbar(x, y)) { stepFirst = last.steps.firstAt(y); dragging = Drag::Steps; return true; }
    auto hit = last.hit(x, y);
    screen::clearMessage();
    switch (hit.zone) {
    case Zone::Back: back(); return true;
    case Zone::Kind: game::pickKind(hit.index); rowFirst = stepFirst = 0; return true;
    case Zone::Held: game::pickHeld(); rowFirst = stepFirst = 0; return true;
    case Zone::SetAll: game::setAllHighest(); stepFirst = 0; return true;
    case Zone::SetLast: game::restoreLast(); stepFirst = 0; return true;
    case Zone::SetClear: game::clearWanted(); stepFirst = 0; return true;
    case Zone::Row:
        switch (game::toggleRow(hit.index)) {
        case order::Change::Full: screen::setMessage(translated("enchantOrder.full", order::maxBooks)); break;
        // What the item carries cannot come off at an anvil; a fresh plan is
        // chosen by the kind's cell, not here.
        case order::Change::Fixed: screen::setMessage(translated("enchantOrder.onItem.help")); break;
        default: break;
        }
        stepFirst = 0;
        return true;
    case Zone::RowMinus: game::stepRow(hit.index, -1); return true;
    case Zone::RowPlus: game::stepRow(hit.index, 1); return true;
    case Zone::Goal: game::flipGoal(); stepFirst = 0; return true;
    default: return true;
    }
}
void key(int key) {
    if (key == 0x1b) back();
}
void wheel(int amount) {
    if (lastPointer.x < last.dividerX) rowFirst = SettingsTable::clampFirst(rowFirst + amount, last.rows.listCount, last.rows.listVisible);
    else stepFirst = SettingsTable::clampFirst(stepFirst + amount, last.steps.listCount, last.steps.listVisible);
}
void release() { dragging = Drag::None; }
void renderContent(MinecraftUIRenderContext& context, SettingsTable const& t, glm::vec2 pointer) {
    lastPointer = pointer;
    auto const& v = game::view();
    int const stepCount = static_cast<int>(v.steps.size());
    last = EnchantLayout::fit(t, static_cast<int>(v.kinds.size()), v.heldCell, static_cast<int>(v.rows.size()), rowFirst, stepCount, stepFirst);
    if (!last.usable()) return;
    if (dragging == Drag::Rows) last.rows.listFirst = last.rows.firstAt(pointer.y);
    if (dragging == Drag::Steps) last.steps.listFirst = last.steps.firstAt(pointer.y);
    rowFirst = last.rows.listFirst = SettingsTable::clampFirst(last.rows.listFirst, last.rows.listCount, last.rows.listVisible);
    stepFirst = last.steps.listFirst = SettingsTable::clampFirst(last.steps.listFirst, last.steps.listCount, last.steps.listVisible);
    auto hover = last.hit(pointer.x, pointer.y);
    float const pad = SettingsTable::pad;
    using L = EnchantLayout;

    // Heading row: where this screen belongs, and the way back to it.
    label(context, t.nameX, t.theadTop + 2, last.backX - t.nameX - SettingsTable::gap,
        translated("section.inventory") + "  >  " + translated("feature.enchantOrder"), palette::faint);
    drawSmallButton(context, last.backX, t.theadTop, ShapesLayout::closeWidth, 11, "< " + translated("section.inventory"), hover.zone == Zone::Back);
    fill(context, t.tableLeft, t.rowsTop - 1, t.tableWidth, 1, palette::white, .14f);
    fill(context, last.dividerX, t.rowsTop, 1, last.footerTop - t.rowsTop, palette::white, .14f);

    // Left: the item, the main hand's cell and the kinds, the sets, the rows.
    float const leftRight = last.dividerX - pad, leftLeft = last.left + pad;
    drawIcon(context, game::itemIcon(), leftLeft, last.itemY + 2);
    label(context, leftLeft + 14, last.itemY + 3, leftRight - leftLeft - 14,
        v.useHeld ? translated("enchantOrder.kind.held", v.itemName) : v.itemName);

    for (int i = 0; i < last.kindCount; ++i) {
        float const x = last.kindX(i), y = last.kindY(i);
        bool const chosen = i == v.kind && !v.useHeld, over = hover.zone == Zone::Kind && hover.index == i;
        fill(context, x, y, L::cell, L::cell, chosen ? palette::accent : Rgb{0, 0, 0}, chosen ? .16f : .3f);
        frame(context, x, y, L::cell, L::cell, chosen ? palette::accent : over ? palette::white : palette::keyEdge, chosen || over ? 1 : .6f);
        drawIcon(context, game::kindIcon(i), x + 2, y + 2, 16);
    }
    if (last.heldCell) {
        // The item in the main hand itself, after the kinds: choosing it plans
        // from its state. A lighter frame sets it apart from the kinds.
        float const x = last.kindX(last.heldIndex()), y = last.kindY(last.heldIndex());
        bool const over = hover.zone == Zone::Held;
        fill(context, x, y, L::cell, L::cell, v.useHeld ? palette::accent : palette::white, v.useHeld ? .16f : .08f);
        frame(context, x, y, L::cell, L::cell, v.useHeld ? palette::accent : over ? palette::white : palette::dim);
        drawIcon(context, game::heldIcon(), x + 2, y + 2, 16);
    }
    drawButton(context, last.setAllX, last.setsY, last.setWide, translated("enchantOrder.setAll"), hover.zone == Zone::SetAll, true);
    drawButton(context, last.setLastX, last.setsY, last.setWide, translated("enchantOrder.setLast"), hover.zone == Zone::SetLast, v.hasLast);
    drawButton(context, last.setClearX, last.setsY, last.setNarrow, translated("enchantOrder.setClear"), hover.zone == Zone::SetClear, v.wanted > 0);
    auto const& rc = last.rows;
    label(context, rc.listLeft + pad, last.rowsTitleY + 2, rc.listWidth - 2 * pad, translated("enchantOrder.wanted", v.wanted, order::maxBooks), palette::faint);
    for (int i = rc.listFirst; i < rc.listFirst + rc.listVisible && i < rc.listCount; ++i) {
        auto const& row = v.rows[static_cast<size_t>(i)];
        float const y = rc.listRowY(i);
        bool const rowHover = (hover.zone == Zone::Row || hover.zone == Zone::RowMinus || hover.zone == Zone::RowPlus) && hover.index == i;
        bool const on = row.state == game::RowState::On, fixed = row.state == game::RowState::Fixed;
        bool const off = row.state == game::RowState::Blocked || row.state == game::RowState::Full;
        rowBackground(context, rc.listLeft + 1, y, rc.listWidth - 2, ShapesLayout::rowHeight, false, rowHover && !off && !fixed);
        // Name, the level as a stepper, then the switch at the right edge. What
        // replaces them (on the item, a conflict) is written in their place.
        float const nameLeft = rc.listLeft + pad;
        float right = last.switchX() + switchWidth;
        Rgb const nameColor = off ? palette::faint : row.curse ? palette::curse : fixed ? palette::dim : palette::text;
        if (fixed) {
            std::string tag = translated("enchantOrder.onItem");
            float const w = textWidth(context, tag) + 2;
            label(context, right - w, y + 3, w, tag, palette::accent, Align::Right);
            // Its level in the level column like the others, moved left only if the tag is wider than the switch.
            float nameRight = right - w - 3;
            if (!row.levelText.empty()) {
                float const levelLeft = std::min(last.levelX(), right - w - 3 - L::levelWidth);
                label(context, levelLeft, y + 3, L::levelWidth, row.levelText, palette::dim, Align::Center);
                nameRight = levelLeft - 4;
            }
            label(context, nameLeft, y + 3, nameRight - nameLeft, row.levelText.empty() ? row.label : row.name, nameColor);
        } else if (row.state == game::RowState::Blocked) {
            std::string why = translated("enchantOrder.conflict", row.reason);
            float const w = std::min(textWidth(context, why) + 2, (right - nameLeft) * .55f);
            label(context, right - w, y + 3, w, why, palette::faint, Align::Right);
            label(context, nameLeft, y + 3, right - w - 3 - nameLeft, row.name.empty() ? row.label : row.name, nameColor);
        } else {
            toggleSwitch(context, last.switchX(), y + 3, on);
            float nameRight = last.switchX() - 4;
            if (!row.levelText.empty()) {
                if (on) {
                    drawSmallButton(context, last.minusX(), y + 1, L::stepWidth, 12, "-", hover.zone == Zone::RowMinus && hover.index == i);
                    drawSmallButton(context, last.plusX(), y + 1, L::stepWidth, 12, "+", hover.zone == Zone::RowPlus && hover.index == i);
                }
                // Off: the level it would turn on at, without its buttons.
                label(context, last.levelX(), y + 3, L::levelWidth, row.levelText, on ? palette::text : palette::faint, Align::Center);
                nameRight = last.minusX() - 4;
            }
            label(context, nameLeft, y + 3, nameRight - nameLeft, row.levelText.empty() ? row.label : row.name, nameColor);
        }
    }
    drawListScrollbar(context, rc);

    // Right: the goal, the steps, the totals.
    label(context, last.rightLeft + pad, last.goalY + 3, last.goalX() - last.rightLeft - pad - 4, translated("enchantOrder.goal"), palette::dim);
    {
        float const gx = last.goalX();
        bool const over = hover.zone == Zone::Goal;
        if (over) fill(context, gx, last.goalY + 1, L::goalWidth, L::barHeight - 2, palette::white, .07f);
        arrow(context, gx + 2, last.goalY + 4, true, over ? palette::text : palette::dim);
        arrow(context, gx + L::goalWidth - 8, last.goalY + 4, false, over ? palette::text : palette::dim);
        label(context, gx + 10, last.goalY + 3, L::goalWidth - 20,
            translated(v.goal == inventory::enchantPlan::Goal::FewestLevels ? "enchantOrder.goal.levels" : "enchantOrder.goal.work"),
            palette::text, Align::Center);
    }
    auto const& sc = last.steps;
    float const stepsLeft = sc.listLeft + pad, stepsWidth = sc.listWidth - 2 * pad;
    if (v.planned && !v.steps.empty()) {
        float y = last.summaryTop + 1;
        label(context, stepsLeft, y, stepsWidth, translated("enchantOrder.total", v.total));
        y += L::summaryLine;
        label(context, stepsLeft, y, stepsWidth,
            translated("enchantOrder.summary", v.highest, v.work, inventory::enchantPlan::priorWorkPenalty(v.work)), palette::dim);
        y += L::summaryLine;
        if (v.otherKnown)
            label(context, stepsLeft, y, stepsWidth,
                v.sameForBoth ? translated("enchantOrder.same")
                    : translated(v.goal == inventory::enchantPlan::Goal::FewestLevels ? "enchantOrder.otherWork" : "enchantOrder.otherLevels",
                          v.otherTotal, v.otherWork),
                palette::faint);
    }
    fill(context, last.rightLeft, last.stepsTitleY - 2, last.rightWidth, 1, palette::white, .14f);
    if (v.steps.empty()) {
        label(context, stepsLeft, last.stepsTitleY + 2, stepsWidth, translated("enchantOrder.steps.none"), palette::faint);
        std::string text = translated(v.costUnknown ? "enchantOrder.unknownCost" : v.refused ? "enchantOrder.refused"
            : v.planned ? "enchantOrder.noSteps" : "enchantOrder.empty");
        paragraph(context, stepsLeft, sc.rowsTop + 3, stepsWidth, text, 6, v.costUnknown || v.refused ? palette::warning : palette::faint);
    } else {
        label(context, stepsLeft, last.stepsTitleY + 2, stepsWidth, translated("enchantOrder.steps", v.steps.size()), palette::faint);
        label(context, stepsLeft, last.stepsTitleY + 2, stepsWidth - 2, translated("enchantOrder.levels"), palette::faint, Align::Right);
        constexpr float numberWidth = 12, costWidth = 20;
        float const textLeft = stepsLeft + numberWidth + 4, textRight = sc.listLeft + sc.listWidth - pad - costWidth - 2;
        for (int i = sc.listFirst; i < sc.listFirst + sc.listVisible && i < sc.listCount; ++i) {
            auto const& step = v.steps[static_cast<size_t>(i / L::stepRows)];
            float const y = sc.listRowY(i);
            if ((i / L::stepRows) % 2 == 1) fill(context, sc.listLeft + 1, y, sc.listWidth - 2, ShapesLayout::rowHeight, palette::white, .04f);
            if (i % L::stepRows == 0) {
                // A line above each step after the first: where one anvil use ends.
                if (i > 0) fill(context, sc.listLeft + 1, y, sc.listWidth - 2, 1, palette::white, .18f);
                label(context, stepsLeft, y + 3, numberWidth, std::to_string(i / L::stepRows + 1), palette::dim, Align::Right);
                drawIcon(context, step.keptIsItem ? game::itemIcon() : game::bookIcon(), textLeft, y + 2);
                label(context, textLeft + 13, y + 3, textRight - textLeft - 13, step.kept, step.keptCurse ? palette::curse : palette::text);
                label(context, textRight + 2, y + 3, costWidth, std::to_string(step.cost), palette::text, Align::Right);
            } else {
                label(context, textLeft - 8, y + 3, 7, "+", palette::faint, Align::Right);
                drawIcon(context, game::bookIcon(), textLeft, y + 2);
                float nameRight = textRight;
                if (step.penalty > 0) {
                    std::string penalty = translated("enchantOrder.penalty", step.penalty);
                    float const w = std::min(textWidth(context, penalty) + 2, (textRight + costWidth - textLeft) * .5f);
                    label(context, textRight + costWidth + 2 - w, y + 3, w, penalty, palette::faint, Align::Right);
                    nameRight = textRight + costWidth - w - 2;
                }
                label(context, textLeft + 13, y + 3, nameRight - textLeft - 13, step.gone, step.goneCurse ? palette::curse : palette::dim);
            }
        }
        drawListScrollbar(context, sc);
    }

    // Footer, as in the table: status first, then what the screen is, then the limit.
    fill(context, t.left, t.footerTop, t.width, 1, palette::white, .14f);
    float const textLeft = t.left + pad, footWidth = t.width - 2 * pad;
    std::string status = screen::message();
    Rgb statusColor = palette::warning;
    if (status.empty()) {
        statusColor = palette::text;
        if (hover.zone == Zone::Kind && hover.index >= 0 && hover.index < static_cast<int>(v.kinds.size())) {
            status = v.kinds[static_cast<size_t>(hover.index)].name;
        } else if (hover.zone == Zone::Held) status = translated("enchantOrder.useHeld.help", v.heldName);
        else if (hover.zone == Zone::SetAll) status = translated("enchantOrder.setAll.help");
        else if (hover.zone == Zone::SetLast) status = translated("enchantOrder.setLast.help");
        else if (hover.zone == Zone::Goal) status = translated("enchantOrder.goal.help");
        else if ((hover.zone == Zone::Row || hover.zone == Zone::RowMinus || hover.zone == Zone::RowPlus) && hover.index >= 0
                 && hover.index < static_cast<int>(v.rows.size())) {
            auto const& row = v.rows[static_cast<size_t>(hover.index)];
            status = row.state == game::RowState::Fixed ? translated("enchantOrder.onItem.help")
                : row.state == game::RowState::Blocked ? translated("enchantOrder.conflict", row.reason)
                : row.state == game::RowState::Full ? translated("enchantOrder.full", order::maxBooks) : row.label;
        } else status = translated("enchantOrder.fresh");
    }
    label(context, textLeft, t.footerTop + 3, footWidth, status, statusColor);
    label(context, textLeft, t.footerTop + 16, footWidth, translated("help.enchantOrderList"), palette::dim);
    label(context, textLeft, t.footerTop + 30, footWidth, translated(v.creative ? "enchantOrder.limit.creative" : "enchantOrder.limit"), palette::faint);
    context.flushText(0, std::nullopt);
}
}

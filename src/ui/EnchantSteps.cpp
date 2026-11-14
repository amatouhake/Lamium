#include "ui/EnchantSteps.h"
#include "ui/EnchantChooser.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/Widgets.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/world/item/ItemStack.h"
#include <algorithm>
#include <string>

namespace lamium::ui::enchant_steps {
namespace {
namespace game = inventory::enchantOrder::game;
using inventory::enchantPlan::Goal;
std::string atOnce(game::StepsInfo const& info) { return translated("enchantOrder.atOnce", info.highest); }
std::string result(game::StepsInfo const& info) {
    return translated("enchantOrder.resultPenalty", inventory::enchantPlan::priorWorkPenalty(info.work));
}
constexpr float detailGap = 12;
// A side's names one after another, each in its own colour, cut at `right`. True when they were cut.
bool names(MinecraftUIRenderContext& context, std::vector<game::NamePart> const& parts, std::string const& whole, float x, float y, float right,
           Rgb base, bool faint, inventory::enchantOrder::Following const& following) {
    if (parts.empty()) {
        label(context, x, y, right - x, whole, faint ? palette::faint : base);
        return textWidth(context, whole) > right - x;
    }
    bool cut = false;
    for (size_t i = 0; i < parts.size(); ++i) {
        std::string text = parts[i].text;
        if (i + 1 < parts.size()) text += ", ";
        float const w = textWidth(context, text);
        if (x >= right) { cut = true; break; }
        if (x + w > right + 1) cut = true;
        Rgb const color = faint ? palette::faint : following.lacks(parts[i].bit) ? palette::warning : base;
        label(context, x, y, std::min(w + 2, right - x), std::move(text), color);
        x += w;
    }
    return cut;
}
}
int detailLines(MinecraftUIRenderContext& context, game::StepsInfo const& info, float width) {
    if (!info.planned) return 1;
    return textWidth(context, atOnce(info)) + detailGap + textWidth(context, result(info)) <= width - 2 * SettingsTable::pad ? 1 : 2;
}
int footLines(MinecraftUIRenderContext& context, game::StepsInfo const& info, float width) {
    if (!info.planned || info.note.empty()) return 1;
    return std::clamp(static_cast<int>(paragraphLines(context, width - 2 * SettingsTable::pad, info.note, 3)), 1, 3);
}
void drawTip(MinecraftUIRenderContext& context, Tip const& tip) {
    if (!tip.show) return;
    constexpr float pad = 4, lineHeight = 12;
    float const height = static_cast<float>(tip.lines) * lineHeight + 2 * pad;
    // Queued text is flushed first so the box covers it.
    context.flushText(0, std::nullopt);
    fill(context, tip.x, tip.y, tip.width, height, palette::panel, .97f);
    frame(context, tip.x, tip.y, tip.width, height, palette::keyEdge);
    paragraph(context, tip.x + pad, tip.y + pad, tip.width - 2 * pad, tip.text, static_cast<size_t>(tip.lines), palette::text);
    context.flushText(0, std::nullopt);
}
Tip draw(MinecraftUIRenderContext& context, EnchantStepsLayout const& l, game::StepsInfo const& info, float pointerX, float pointerY,
         bool goalLive, std::vector<inspection::render::IconAt>& icons) {
    Tip tip;
    if (!l.usable()) return tip;
    using L = EnchantStepsLayout;
    Zone const hover = l.hit(pointerX, pointerY);
    float const pad = SettingsTable::pad, left = l.left + pad, inner = l.width - 2 * pad;
    // The goal: the stepper the lists and the settings table use, where it can be switched.
    {
        float const gx = l.goalX(), gw = l.goalW();
        std::string name = translated(info.goal == Goal::FewestLevels ? "enchantOrder.goal.levels" : "enchantOrder.goal.work");
        label(context, left, l.goalY + 3, gx - left - 4, translated("enchantOrder.goal"), palette::dim);
        if (goalLive) {
            drawStepperBox(context, gx, l.goalY, gw, false, std::move(name), nullptr, pointerX, pointerY);
            if (hover == Zone::Goal) fill(context, gx, l.goalY + 1, gw, ShapesLayout::rowHeight - 2, palette::white, .1f);
        } else label(context, gx, l.goalY + 3, gw, std::move(name), palette::dim, Align::Right);
    }
    fill(context, l.left, l.totalY - 3, l.width, 1, palette::white, .14f);
    auto const& rows = l.rows;
    if (!info.planned) {
        fill(context, l.left, l.titleY - 2, l.width, 1, palette::white, .14f);
        label(context, left, l.titleY + 2, inner, translated("enchantOrder.steps.none"), palette::faint);
        paragraph(context, left, rows.rowsTop + 3, inner, info.message, 6, info.warning ? palette::warning : palette::faint);
        return tip;
    }
    // A cut side's whole text, by its row.
    auto offer = [&](std::string const& text, float rowY) {
        if (pointerX < rows.listLeft || pointerX >= rows.listLeft + rows.listWidth || pointerY < rowY || pointerY >= rowY + ShapesLayout::rowHeight)
            return;
        constexpr float tipPad = 4, lineHeight = 12;
        float const room = l.width - 4;
        tip.show = true;
        tip.text = text;
        tip.width = std::min(room, textWidth(context, text) + 2 * tipPad + 2);
        tip.lines = std::clamp(static_cast<int>(paragraphLines(context, tip.width - 2 * tipPad, text, 3)), 1, 3);
        tip.x = l.left + l.width - 2 - tip.width;
        float const height = static_cast<float>(tip.lines) * lineHeight + 2 * tipPad, under = rowY + ShapesLayout::rowHeight + 1;
        tip.y = under + height <= l.bottom ? under : rowY - 1 - height;
    };
    auto const& following = info.following;
    // The total, what the player has beside it, then how much of the materials is at hand: one run of text.
    {
        std::string total = translated("enchantOrder.total", info.total);
        float x = left;
        float const right = left + inner;
        auto put = [&](std::string text, Rgb color) {
            if (x >= right) return;
            float const w = textWidth(context, text);
            label(context, x, l.totalY, std::min(w + 2, right - x), std::move(text), color);
            x += w + 6;
        };
        put(std::move(total), palette::text);
        if (info.level >= 0) put(translated("enchantOrder.now", info.level), info.level < info.highest ? palette::warning : palette::dim);
        put(translated("enchantOrder.materials", following.have, following.need), following.have < following.need ? palette::warning : palette::dim);
    }
    if (l.detailLines > 0) {
        std::string once = atOnce(info), after = result(info);
        Rgb const onceColor = hover == Zone::AtOnce ? palette::text : palette::dim, afterColor = hover == Zone::Result ? palette::text : palette::dim;
        if (l.detailLines == 1) {
            float const onceWidth = textWidth(context, once);
            label(context, left, l.detailY, inner, std::move(once), onceColor);
            label(context, left + onceWidth + detailGap, l.detailY, inner - onceWidth - detailGap, std::move(after), afterColor);
        } else {
            label(context, left, l.detailY, inner, std::move(once), onceColor);
            label(context, left, l.detailY + L::line, inner, std::move(after), afterColor);
        }
    }
    fill(context, l.left, l.titleY - 2, l.width, 1, palette::white, .14f);
    label(context, left, l.titleY + 2, inner, translated("enchantOrder.steps", info.steps.size()), palette::faint);
    label(context, left, l.titleY + 2, inner - 2, translated("enchantOrder.levels"), palette::faint, Align::Right);

    constexpr float numberWidth = 12, costWidth = 20;
    float const textLeft = left + numberWidth + 4, textRight = rows.listLeft + rows.listWidth - pad - costWidth - 2;
    int itemsDrawn = 0, booksDrawn = 0;
    for (int i = rows.listFirst; i < rows.listFirst + rows.listVisible && i < rows.listCount; ++i) {
        int const number = i / L::stepRows;
        if (number >= static_cast<int>(info.steps.size())) break;
        auto const& step = info.steps[static_cast<size_t>(number)];
        float const y = rows.listRowY(i);
        bool const done = static_cast<size_t>(number) < following.done.size() && following.done[static_cast<size_t>(number)];
        bool const next = number == following.next, inSlots = number == info.inSlots;
        if (number % 2 == 1) fill(context, rows.listLeft + 1, y, rows.listWidth - 2, ShapesLayout::rowHeight, palette::white, .04f);
        if (inSlots) fill(context, rows.listLeft + 1, y, rows.listWidth - 2, ShapesLayout::rowHeight, palette::accent, .16f);
        if (next || inSlots) fill(context, rows.listLeft + 1, y, 2, ShapesLayout::rowHeight, inSlots ? palette::accent : palette::white);
        if (i % L::stepRows == 0) {
            // A line above each step after the first: where one anvil use ends.
            if (i > 0) fill(context, rows.listLeft + 1, y, rows.listWidth - 2, 1, palette::white, .18f);
            label(context, left, y + 3, numberWidth, std::to_string(number + 1), done ? palette::faint : palette::dim, Align::Right);
            enchant_chooser::addIcon(icons, step.keptIsItem ? game::stepItemIcon(itemsDrawn++, step.keptEnchanted) : game::stepBookIcon(booksDrawn++),
                textLeft, y + 2);
            if (names(context, step.keptParts, step.kept, textLeft + 13, y + 3, textRight, step.keptCurse ? palette::vanilla::curse : palette::text, done,
                    following))
                offer(step.kept, y);
            label(context, textRight + 2, y + 3, costWidth, std::to_string(step.cost),
                done ? palette::faint : info.level >= 0 && info.level < step.cost ? palette::warning : palette::text, Align::Right);
        } else {
            label(context, textLeft - 8, y + 3, 7, "+", palette::faint, Align::Right);
            enchant_chooser::addIcon(icons, game::stepBookIcon(booksDrawn++), textLeft, y + 2);
            float nameRight = textRight;
            if (step.penalty > 0) {
                // The part of the cost that is prior work, short and under the cost: the names matter more. What
                // it is, is said in the status line.
                std::string penalty = translated("enchantOrder.penalty.short", step.penalty);
                float const w = textWidth(context, penalty) + 2;
                label(context, textRight + costWidth + 2 - w, y + 3, w, penalty, palette::faint, Align::Right);
                nameRight = textRight + costWidth - w - 2;
            }
            if (names(context, step.goneParts, step.gone, textLeft + 13, y + 3, nameRight, step.goneCurse ? palette::vanilla::curse : palette::dim, done,
                    following))
                offer(step.gone, y);
        }
    }
    drawListScrollbar(context, rows, rows.onScrollbar(pointerX, pointerY));
    // What to do next.
    fill(context, l.left, l.footY, l.width, 1, palette::white, .14f);
    paragraph(context, left, l.footY + 3, inner, info.note, static_cast<size_t>(l.footLines),
        info.noteTone == 2 ? palette::warning : info.noteTone == 1 ? palette::accent : palette::text);
    return tip;
}
std::string status(game::StepsInfo const& info, EnchantStepsLayout const& l, float x, float y) {
    if (!l.usable()) return {};
    if (auto text = status(info, l.hit(x, y)); !text.empty()) return text;
    // A step whose cost holds prior work.
    auto const& rows = l.rows;
    if (!info.planned || x < rows.listLeft || x >= rows.listLeft + rows.listWidth || y < rows.rowsTop
        || y >= rows.rowsTop + static_cast<float>(rows.listVisible) * ShapesLayout::rowHeight) return {};
    int const number = (static_cast<int>((y - rows.rowsTop) / ShapesLayout::rowHeight) + rows.listFirst) / EnchantStepsLayout::stepRows;
    if (number < 0 || number >= static_cast<int>(info.steps.size()) || info.steps[static_cast<size_t>(number)].penalty <= 0) return {};
    auto const& step = info.steps[static_cast<size_t>(number)];
    return translated("enchantOrder.penalty.help", number + 1, step.cost, step.penalty);
}
std::string status(game::StepsInfo const& info, Zone hover) {
    switch (hover) {
    case Zone::Goal:
        if (!info.planned || !info.otherKnown) return translated("enchantOrder.goal.help");
        if (info.sameForBoth) return translated("enchantOrder.same");
        return translated(info.goal == Goal::FewestLevels ? "enchantOrder.otherWork" : "enchantOrder.otherLevels", info.otherTotal, info.otherWork);
    case Zone::AtOnce: return info.planned ? translated("enchantOrder.atOnce.help") : std::string{};
    case Zone::Result: return info.planned ? translated("enchantOrder.resultPenalty.help", info.work) : std::string{};
    default: return {};
    }
}
}

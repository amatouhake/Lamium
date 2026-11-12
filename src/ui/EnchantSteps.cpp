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
// A side's names one after another, each in its own colour, cut at `right`.
void names(MinecraftUIRenderContext& context, std::vector<game::NamePart> const& parts, std::string const& whole, float x, float y, float right,
           Rgb base, bool faint, inventory::enchantOrder::Following const& following) {
    if (parts.empty()) { label(context, x, y, right - x, whole, faint ? palette::faint : base); return; }
    for (size_t i = 0; i < parts.size() && x < right; ++i) {
        std::string text = parts[i].text;
        if (i + 1 < parts.size()) text += ", ";
        float const w = textWidth(context, text);
        Rgb const color = faint ? palette::faint : following.lacks(parts[i].bit) ? palette::warning : base;
        label(context, x, y, std::min(w + 2, right - x), std::move(text), color);
        x += w;
    }
}
}
int detailLines(MinecraftUIRenderContext& context, game::StepsInfo const& info, float width) {
    if (!info.planned) return 1;
    return textWidth(context, atOnce(info)) + detailGap + textWidth(context, result(info)) <= width - 2 * SettingsTable::pad ? 1 : 2;
}
void draw(MinecraftUIRenderContext& context, EnchantStepsLayout const& l, game::StepsInfo const& info, Zone hover, bool goalLive,
          std::vector<inspection::render::IconAt>& icons) {
    if (!l.usable()) return;
    using L = EnchantStepsLayout;
    float const pad = SettingsTable::pad, left = l.left + pad, inner = l.width - 2 * pad;
    // The goal, as a stepper like the settings' own.
    {
        float const gx = l.goalX(), gw = l.goalW();
        bool const over = goalLive && hover == Zone::Goal;
        label(context, left, l.goalY + 3, gx - left - 4, translated("enchantOrder.goal"), palette::dim);
        if (over) fill(context, gx, l.goalY + 1, gw, L::barHeight - 2, palette::white, .07f);
        if (goalLive) {
            arrow(context, gx + 2, l.goalY + 4, true, over ? palette::text : palette::dim);
            arrow(context, gx + gw - 8, l.goalY + 4, false, over ? palette::text : palette::dim);
        }
        label(context, gx + 10, l.goalY + 3, gw - 20, translated(info.goal == Goal::FewestLevels ? "enchantOrder.goal.levels" : "enchantOrder.goal.work"),
            goalLive ? palette::text : palette::dim, Align::Center);
    }
    auto const& rows = l.rows;
    if (!info.planned) {
        fill(context, l.left, l.titleY - 2, l.width, 1, palette::white, .14f);
        label(context, left, l.titleY + 2, inner, translated("enchantOrder.steps.none"), palette::faint);
        paragraph(context, left, rows.rowsTop + 3, inner, info.message, 6, info.warning ? palette::warning : palette::faint);
        return;
    }
    auto const& following = info.following;
    // The total, what the player has beside it, and how much of the materials is at hand.
    {
        std::string total = translated("enchantOrder.total", info.total);
        std::string const materials = translated("enchantOrder.materials", following.have, following.need);
        float const materialsWidth = textWidth(context, materials) + 2, totalWidth = textWidth(context, total);
        label(context, left, l.totalY, inner - materialsWidth - 4, std::move(total));
        if (info.level >= 0)
            label(context, left + totalWidth + 4, l.totalY, inner - totalWidth - materialsWidth - 8, translated("enchantOrder.now", info.level),
                info.level < info.highest ? palette::warning : palette::dim);
        label(context, left + inner - materialsWidth, l.totalY, materialsWidth, materials,
            following.have < following.need ? palette::warning : palette::dim, Align::Right);
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
            names(context, step.keptParts, step.kept, textLeft + 13, y + 3, textRight, step.keptCurse ? palette::curse : palette::text, done, following);
            label(context, textRight + 2, y + 3, costWidth, std::to_string(step.cost),
                done ? palette::faint : info.level >= 0 && info.level < step.cost ? palette::warning : palette::text, Align::Right);
        } else {
            label(context, textLeft - 8, y + 3, 7, "+", palette::faint, Align::Right);
            enchant_chooser::addIcon(icons, game::stepBookIcon(booksDrawn++), textLeft, y + 2);
            float nameRight = textRight;
            if (step.penalty > 0) {
                std::string penalty = translated("enchantOrder.penalty", step.penalty);
                float const w = std::min(textWidth(context, penalty) + 2, (textRight + costWidth - textLeft) * .5f);
                label(context, textRight + costWidth + 2 - w, y + 3, w, penalty, palette::faint, Align::Right);
                nameRight = textRight + costWidth - w - 2;
            }
            names(context, step.goneParts, step.gone, textLeft + 13, y + 3, nameRight, step.goneCurse ? palette::curse : palette::dim, done, following);
        }
    }
    drawListScrollbar(context, rows);
    // What to do next.
    fill(context, l.left, l.footY, l.width, 1, palette::white, .14f);
    label(context, left, l.footY + 3, inner, info.note, info.noteTone == 2 ? palette::warning : info.noteTone == 1 ? palette::accent : palette::text);
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

#pragma once
#include "ui/SettingsTable.h"
#include "ui/ShapesLayout.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace lamium::ui {
struct LanguageChoice { std::string code, name; };
struct LanguageSelection {
    static constexpr size_t limit = 96;
    std::vector<LanguageChoice> choices;
    int selected = -1, first = 0;
    void add(std::string code, std::string name) {
        if (code.empty() || choices.size() >= limit) return;
        for (auto const& choice : choices) if (choice.code == code) return;
        choices.push_back({std::move(code), std::move(name)});
        if (choices.back().name.empty()) choices.back().name = choices.back().code;
    }
    void select(int index) {
        if (index >= 0 && index < static_cast<int>(choices.size())) selected = index;
    }
    void move(int amount, int visible) {
        if (choices.empty()) { selected = -1; return; }
        selected = std::clamp(selected + amount, 0, static_cast<int>(choices.size()) - 1);
        first = SettingsTable::reveal(first, selected, visible);
    }
    std::optional<std::string> request(std::string_view current, std::string_view saved) const {
        if (selected < 0 || selected >= static_cast<int>(choices.size())) return std::nullopt;
        auto const& code = choices[selected].code;
        return code == current && code == saved ? std::nullopt : std::optional(code);
    }
};
// In-world switching froze the game after 12 changes in 23 s while the
// title screen did not (L-167, 2026-10-14). Its mechanism was never identified,
// so this is the only guard: one change per interval, 2.5x the frozen pace.
struct LanguagePace {
    static constexpr double interval = 5;
    double last = -interval;
    // Seconds left before another change, 0 when one may start now.
    int wait(double now) const { return now - last >= interval ? 0 : static_cast<int>(std::ceil(interval - (now - last))); }
    void applied(double now) { last = now; }
};
struct LanguageContext {
    std::string code;
    int dimension = 0;
    bool active = true;
    bool permits(LanguageContext const& now, bool cancelled) const {
        return !cancelled && active && now.active && dimension == now.dimension && code == now.code;
    }
};
// The list takes the settings table's place (header, sidebar and footer
// stay): its heading row holds the path and Back, rows below it.
inline ShapesLayout languageLayout(SettingsTable const& t, int count, int first) {
    ShapesLayout l;
    if (!t.usable()) return l;
    l.left = l.listLeft = t.tableLeft; l.width = l.listWidth = t.tableWidth;
    l.top = t.theadTop; l.height = t.footerTop - t.theadTop;
    l.closeX = t.rowsRight() - SettingsTable::pad - ShapesLayout::closeWidth;
    l.rowsTop = t.rowsTop;
    l.footerTop = t.footerTop;
    l.listCount = std::max(0, count);
    l.listVisible = std::max(0, static_cast<int>((l.footerTop - l.rowsTop - 2) / SettingsTable::rowHeight));
    l.listFirst = SettingsTable::clampFirst(first, count, l.listVisible);
    return l;
}
}

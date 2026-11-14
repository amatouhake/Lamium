#include "ui/CalculatorView.h"
#include "app/Desktop.h"
#include "ui/Calculator.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/SearchQuery.h"
#include "ui/Widgets.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/input/RectangleArea.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <optional>
#include <string_view>
#include <vector>

namespace lamium::ui::calculator_view {
namespace {
// The layout decided 2026-10-13 (calculator-v3.html "B"): 460 x 300, left
// column 216 wide, a six-column key grid whose keys grow to fill it.
struct Layout {
    static constexpr float pad = 7, leftWidth = 216, gap = 3, cardHeight = 30, cardGap = 3, copyWidth = 34, arrowWidth = 12;
    float left = 0, top = 0, width = 460, height = 300;
    static Layout at(glm::vec2 size) {
        Layout l;
        l.width = std::min(460.f, std::max(300.f, size.x - 16));
        l.height = std::min(300.f, std::max(200.f, size.y - 16));
        l.left = std::max(0.f, (size.x - l.width) / 2);
        l.top = std::max(0.f, (size.y - l.height) / 2 - std::min(14.f, (size.y - l.height) / 2));
        return l;
    }
    float leftX() const { return left + pad; }
    float fieldY() const { return top + 23; }
    float resultY() const { return top + 42; }
    float amountY() const { return top + 60; }
    float gridY() const { return top + 74; }
    float bottom() const { return top + height - pad; }
    float cell() const { return (leftWidth - 5 * gap) / 6; }
    // Seven rows; the unit row sits 3 apart from the rest.
    float rowHeight() const { return std::max(12.f, (bottom() - gridY() - 6 * gap - 3) / 7); }
    float keyX(int column) const { return leftX() + column * (cell() + gap); }
    float keyY(int row) const { return gridY() + row * (rowHeight() + gap) + (row > 0 ? 3 : 0); }
    float copyX() const { return leftX() + leftWidth - copyWidth - arrowWidth - 1; }
    float rightX() const { return left + pad + leftWidth + 10; }
    float rightWidth() const { return width - leftWidth - 2 * pad - 10; }
    float cardsTop() const { return top + 34; }
    // Close as in the settings screen's header, help just before it.
    static constexpr float closeWidth = 58;
    float closeX() const { return left + width - pad - closeWidth; }
    float helpX() const { return closeX() - 4 - 12; }
};
enum class Kind { Digit, Operator, Unit, Variable, Function, Equals };
struct Key {
    std::string_view label, insert;
    Kind kind;
    std::string_view tip; // a translation key, or empty
};
constexpr std::array<std::array<Key, 6>, 7> grid{{
    {{{"st", "st", Kind::Unit, "calc.tip.st"}, {"ch", "ch", Kind::Unit, "calc.tip.ch"}, {"lc", "lc", Kind::Unit, "calc.tip.lc"},
      {"x", "x", Kind::Variable, "calc.tip.x"}, {"y", "y", Kind::Variable, "calc.tip.y"}, {"z", "z", Kind::Variable, "calc.tip.z"}}},
    {{{"sqrt", "sqrt(", Kind::Function, "calc.tip.sqrt"}, {"abs", "abs(", Kind::Function, "calc.tip.abs"}, {"(", "(", Kind::Operator, ""},
      {")", ")", Kind::Operator, ""}, {"C", "clear", Kind::Operator, "calc.tip.clear"}, {"\xe2\x8c\xab", "back", Kind::Operator, "calc.tip.back"}}},
    {{{"round", "round(", Kind::Function, "calc.tip.round"}, {"floor", "floor(", Kind::Function, "calc.tip.floor"}, {"7", "7", Kind::Digit, ""},
      {"8", "8", Kind::Digit, ""}, {"9", "9", Kind::Digit, ""}, {"\xc3\xb7", " / ", Kind::Operator, ""}}},
    {{{"ceil", "ceil(", Kind::Function, "calc.tip.ceil"}, {"min", "min(", Kind::Function, "calc.tip.min"}, {"4", "4", Kind::Digit, ""},
      {"5", "5", Kind::Digit, ""}, {"6", "6", Kind::Digit, ""}, {"\xc3\x97", " * ", Kind::Operator, ""}}},
    {{{"max", "max(", Kind::Function, "calc.tip.max"}, {"^", " ^ ", Kind::Operator, "calc.tip.power"}, {"1", "1", Kind::Digit, ""},
      {"2", "2", Kind::Digit, ""}, {"3", "3", Kind::Digit, ""}, {"\xe2\x88\x92", " - ", Kind::Operator, ""}}},
    {{{"nether", "nether(", Kind::Function, "calc.tip.nether"}, {"ow", "ow(", Kind::Function, "calc.tip.ow"}, {"0", "0", Kind::Digit, ""},
      {".", ".", Kind::Digit, ""}, {"ans", "ans", Kind::Variable, "calc.tip.ans"}, {"+", " + ", Kind::Operator, ""}}},
    {{{"ticks", "ticks(", Kind::Function, "calc.tip.ticks"}, {"secs", "secs(", Kind::Function, "calc.tip.secs"}, {",", ", ", Kind::Operator, "calc.tip.comma"},
      {"pi", "pi", Kind::Variable, ""}, {"%", " % ", Kind::Operator, "calc.tip.remainder"}, {"=", "equals", Kind::Equals, ""}}},
}};
Rgb inkOf(Kind kind) {
    switch (kind) {
    case Kind::Operator: return Rgb{.84f, .95f, .79f};
    case Kind::Unit: return Rgb{1.f, .85f, .54f};
    case Kind::Variable: return Rgb{.66f, .83f, 1.f};
    case Kind::Function: return Rgb{.89f, .77f, 1.f};
    default: return palette::text;
    }
}

SearchQuery line;
std::vector<calculator::Line> history; // the session's lines, oldest first; newest drawn at the bottom
int recalled = -1;                     // lines back from the newest shown in the field (Up/Down), -1 none
float scrolled = 0;                    // how far the cards are scrolled up from the newest, in GUI units
std::string draft;                     // the line typed before recalling
calculator::Variables variables;
calculator::Result result;
bool helpOpen = false;
// Which copy menu is open: -2 none, -1 the result's, else a history line.
int menuFor = -2;
std::string note;                      // "Copied: ..."
std::chrono::steady_clock::time_point noteUntil;
Layout last;                           // as last drawn, for the wheel
glm::vec2 lastPointer{-1, -1};
bool dragging = false;                 // the history scrollbar is held

void recalculate() {
    variables.ans = history.empty() ? std::nullopt : std::optional(history.back().value);
    result = calculator::evaluate(line.value(), variables);
}
void show(std::string const& text) {
    line.set(text);
    recalculate();
}
std::string chests(double value) {
    if (!calculator::restatable(value)) return {};
    auto count = static_cast<std::uint64_t>(std::llround(std::fabs(value)));
    return (value < 0 ? "-" : "") + amountText(count);
}
void copy(std::string const& text) {
    if (text.empty() || !copyText(text)) return;
    note = translated("calc.copied", text);
    noteUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
}
std::string errorText() {
    switch (result.error) {
    case calculator::Error::Incomplete: return translated("calc.incomplete");
    case calculator::Error::UnknownName: return translated("calc.unknown", result.name);
    case calculator::Error::Undefined: return translated("calc.undefined");
    default: return {};
    }
}
void enter() {
    if (!result.value) return;
    calculator::remember(history, {line.value(), *result.value});
    recalled = -1;
    scrolled = 0;
    line.selectAll(); // a digit starts a new line; an operator goes on from "ans"
    recalculate();
}
void press(Key const& key) {
    menuFor = -2;
    if (key.insert == "clear") line.clear();
    else if (key.insert == "back") line.eraseBefore(calculator::pieceStart(line.value(), line.caret()));
    else if (key.insert == "equals") { enter(); return; }
    else {
        // After Enter the line is selected: an operator continues from the result.
        if (line.selectedAll() && key.kind == Kind::Operator && key.insert.find_first_of("+-*/^%") != std::string_view::npos)
            line.set("ans");
        line.append(key.insert);
    }
    recalled = -1;
    recalculate();
}
// The card of history line `index` (oldest 0) as drawn, or none when it is
// scrolled wholly out of view; cards cut by the edges are clipped.
constexpr float step = Layout::cardHeight + Layout::cardGap;
float cardTop(Layout const& l, int index) {
    int fromNewest = static_cast<int>(history.size()) - 1 - index;
    return l.bottom() - (fromNewest + 1) * step + Layout::cardGap + scrolled;
}
std::optional<float> cardY(Layout const& l, int index) {
    float y = cardTop(l, index);
    if (y + Layout::cardHeight <= l.cardsTop() || y >= l.bottom()) return std::nullopt;
    return y;
}
// How far the history can scroll: all cards minus the space they have.
float scrollRange(Layout const& l) {
    return std::max(0.f, static_cast<float>(history.size()) * step - Layout::cardGap - (l.bottom() - l.cardsTop()));
}
void clampScroll(Layout const& l) { scrolled = std::clamp(scrolled, 0.f, scrollRange(l)); }
// Scrolls just enough to show the card of history line `index` whole.
void reveal(Layout const& l, int index) {
    float y = cardTop(l, index);
    if (y < l.cardsTop()) scrolled += l.cardsTop() - y;
    else if (y + Layout::cardHeight > l.bottom()) scrolled -= y + Layout::cardHeight - l.bottom();
    clampScroll(l);
}
bool inside(float x, float y, float left, float top, float w, float h) { return x >= left && x < left + w && y >= top && y < top + h; }
// A card's copy button, beside its expression line.
float cardCopyX(Layout const& l) { return l.rightX() + l.rightWidth() - 5 - 4 - Layout::copyWidth - Layout::arrowWidth - 1; }
// The history scrollbar's track, at the column's right edge.
bool onScrollbar(Layout const& l, float x, float y) {
    return x >= l.rightX() + l.rightWidth() - 6 && x <= l.rightX() + l.rightWidth() + 1 && y >= l.cardsTop() && y <= l.bottom();
}
// The scrollbar thumb follows the pointer smoothly (whole cards jumped, 2026-10-13).
float thumbLength(Layout const& l) {
    float track = l.bottom() - l.cardsTop(), range = scrollRange(l);
    return std::max(10.f, track * track / (track + range));
}
float grabOffset = 0; // where in the thumb it was taken
void scrollTo(Layout const& l, float y) {
    float track = l.bottom() - l.cardsTop(), thumb = thumbLength(l), range = scrollRange(l);
    if (range <= 0 || track <= thumb) { scrolled = 0; return; }
    float thumbTop = y - grabOffset;
    scrolled = std::clamp((l.bottom() - thumb - thumbTop) / (track - thumb), 0.f, 1.f) * range;
}
float thumbTop(Layout const& l) {
    float track = l.bottom() - l.cardsTop(), thumb = thumbLength(l), range = scrollRange(l);
    return l.bottom() - thumb - (range > 0 ? (track - thumb) * scrolled / range : 0);
}
// A small triangle pointing down, for the copy menu's button.
void downArrow(MinecraftUIRenderContext& context, float cx, float cy, Rgb color) {
    fill(context, cx - 2.5f, cy, 5, 1, color);
    fill(context, cx - 1.5f, cy + 1, 3, 1, color);
    fill(context, cx - .5f, cy + 2, 1, 1, color);
}
// The game font draws "×", "−" and "⌫" small and off center (2026-10-13), so
// those keys are drawn as shapes; long names shrink to fit their key.
void drawKeyLabel(MinecraftUIRenderContext& context, Key const& k, float x, float y, float w, float h) {
    float cx = std::floor(x + w / 2), cy = std::floor(y + h / 2);
    Rgb ink = inkOf(k.kind);
    if (k.insert == " * ") {
        for (int i = 0; i < 5; ++i) {
            fill(context, cx - 2 + i, cy - 2 + i, 1, 1, ink);
            fill(context, cx + 2 - i, cy - 2 + i, 1, 1, ink);
        }
        return;
    }
    if (k.insert == " - ") { fill(context, cx - 3, cy, 6, 1, ink); return; }
    if (k.insert == "back") {
        // A left arrow: head and shaft.
        for (int i = 0; i < 3; ++i) fill(context, cx - 3 + i, cy - i, 1, 1 + 2 * i, ink);
        fill(context, cx, cy, 4, 1, ink);
        return;
    }
    std::string text(k.label);
    float width = textWidth(context, text), room = w - 4;
    float scale = width > room ? std::max(.6f, room / width) : 1.f;
    labelScaled(context, x, y + (h - 10 * scale) / 2 + boxTextInset() * scale - scale, w, text, scale, ink, Align::Center);
}
void arrowButton(MinecraftUIRenderContext& context, float x, float y, float w, float h, bool over, bool open) {
    Rgb ink = drawButtonBox(context, x, y, w, h, over, open ? ButtonKind::Primary : ButtonKind::Plain);
    downArrow(context, x + w / 2, y + h / 2 - 1, ink);
}
}

void begin() {
    line.clear();
    recalled = -1;
    scrolled = 0;
    draft.clear();
    helpOpen = false;
    menuFor = -2;
    recalculate();
}
TextEdit& field() { return line; }
void type(std::string const& text) {
    if (line.type(text)) edited();
}
void edited() {
    recalled = -1;
    menuFor = -2;
    recalculate();
}
Result key(int key) {
    switch (key) {
    case 0x1b:
        if (menuFor != -2) { menuFor = -2; return Result::None; }
        if (helpOpen) { helpOpen = false; return Result::None; }
        return Result::Close;
    case 0x0d: enter(); return Result::None;
    case 0x26: // Up: an older line, higher in the list
        if (recalled + 1 < static_cast<int>(history.size())) {
            if (recalled < 0) draft = line.value();
            ++recalled;
            show(history[history.size() - 1 - static_cast<size_t>(recalled)].text);
            reveal(last, static_cast<int>(history.size()) - 1 - recalled);
        }
        return Result::None;
    case 0x28: // Down: a newer line, then back to the line being typed
        if (recalled >= 0) {
            --recalled;
            show(recalled >= 0 ? history[history.size() - 1 - static_cast<size_t>(recalled)].text : draft);
            if (recalled >= 0) reveal(last, static_cast<int>(history.size()) - 1 - recalled);
            else { scrolled = 0; }
        }
        return Result::None;
    default: return Result::None;
    }
}
void copyValue() {
    if (result.value) copy(calculator::format(*result.value));
}
void copyLine() {
    if (result.value) copy(line.value() + " = " + calculator::format(*result.value));
}
Result click(float x, float y, glm::vec2 size) {
    auto l = Layout::at(size);
    if (inside(x, y, l.closeX(), l.top + 4, Layout::closeWidth, 12)) { menuFor = -2; helpOpen = false; return Result::Close; }
    // An open copy menu takes the click first.
    if (menuFor != -2) {
        int owner = menuFor;
        menuFor = -2;
        float mx = (owner == -1 ? l.copyX() : cardCopyX(l)) + Layout::copyWidth + Layout::arrowWidth + 1 - 110;
        float my = (owner == -1 ? l.resultY() + 1 : cardY(l, owner).value_or(-100) + 3) + 13;
        double value = owner == -1 ? result.value.value_or(0) : history[static_cast<size_t>(owner)].value;
        std::string text = owner == -1 ? line.value() : history[static_cast<size_t>(owner)].text;
        bool ok = owner != -1 || result.value.has_value();
        if (ok && inside(x, y, mx, my, 110, 12)) { copy(text + " = " + calculator::format(value)); return Result::None; }
        if (ok && inside(x, y, mx, my + 12, 110, 12)) { copy(chests(value)); return Result::None; }
    }
    if (inside(x, y, l.helpX(), l.top + 4, 12, 12)) { helpOpen = !helpOpen; return Result::None; }
    if (helpOpen) { helpOpen = false; return Result::None; }
    if (scrollRange(l) > 0 && onScrollbar(l, x, y)) {
        dragging = true;
        // Taken on the thumb it keeps its grip; on the track the thumb centers there.
        float top = thumbTop(l), thumb = thumbLength(l);
        grabOffset = y >= top && y <= top + thumb ? y - top : thumb / 2;
        scrollTo(l, y);
        return Result::None;
    }
    // The result's copy button: the value, or the menu.
    if (result.value && inside(x, y, l.copyX(), l.resultY() + 1, Layout::copyWidth, 12)) { copyValue(); return Result::None; }
    if (result.value && inside(x, y, l.copyX() + Layout::copyWidth + 1, l.resultY() + 1, Layout::arrowWidth, 12)) { menuFor = -1; return Result::None; }
    for (int row = 0; row < 7; ++row)
        for (int column = 0; column < 6; ++column)
            if (inside(x, y, l.keyX(column), l.keyY(row), l.cell(), l.rowHeight())) { press(grid[row][column]); return Result::None; }
    for (int index = 0; index < static_cast<int>(history.size()); ++index) {
        auto top = cardY(l, index);
        if (!top || y < l.cardsTop() || y >= l.bottom() || !inside(x, y, l.rightX(), *top, l.rightWidth() - 5, Layout::cardHeight)) continue;
        float cx = cardCopyX(l);
        if (inside(x, y, cx, *top + 3, Layout::copyWidth, 10)) { copy(calculator::format(history[static_cast<size_t>(index)].value)); return Result::None; }
        if (inside(x, y, cx + Layout::copyWidth + 1, *top + 3, Layout::arrowWidth, 10)) { menuFor = index; return Result::None; }
        recalled = -1;
        show(history[static_cast<size_t>(index)].text);
        return Result::None;
    }
    return Result::None;
}
void wheel(int direction) {
    if (!inside(lastPointer.x, lastPointer.y, last.rightX(), last.cardsTop(), last.rightWidth() + 2, last.bottom() - last.cardsTop())) return;
    scrolled += direction > 0 ? step : -step; // wheel up shows older cards
    clampScroll(last);
}
void release() { dragging = false; }
glm::vec2 fieldPosition(glm::vec2 size) {
    auto l = Layout::at(size);
    return {l.leftX() + 4, l.fieldY()};
}
void render(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) {
    // The player's block position for x, y and z.
    if (auto* player = context.mClient.getLocalPlayer()) {
        auto feet = player->getFeetPos();
        if (std::isfinite(feet.x) && std::isfinite(feet.y) && std::isfinite(feet.z)) {
            variables.x = std::floor(feet.x);
            variables.y = std::floor(feet.y);
            variables.z = std::floor(feet.z);
        }
    }
    recalculate();
    auto l = Layout::at(size);
    last = l;
    lastPointer = pointer;
    if (dragging) scrollTo(l, pointer.y);
    clampScroll(l);
    float x = l.leftX(), lw = Layout::leftWidth;
    panel(context, l.left, l.top, l.width, l.height, .88f);
    frame(context, l.left, l.top, l.width, l.height, palette::white, .14f);
    label(context, x, l.top + 6, 120, translated("calc.title"));
    drawButton(context, l.helpX(), l.top + 4, 12, 12, "?", inside(pointer.x, pointer.y, l.helpX(), l.top + 4, 12, 12), helpOpen ? ButtonKind::Primary : ButtonKind::Plain);
    drawButton(context, l.closeX(), l.top + 4, Layout::closeWidth, 12, translated("closeButton"),
        inside(pointer.x, pointer.y, l.closeX(), l.top + 4, Layout::closeWidth, 12));
    fill(context, l.left, l.top + 18, l.width, 1, palette::white, .14f);

    // ---- Left: field, result, chests, keys ----
    drawTextField(context, x, l.fieldY(), lw, 14, true, &line, {});
    if (result.value) {
        labelScaled(context, x, l.resultY(), lw - Layout::copyWidth - Layout::arrowWidth - 6, "= " + calculator::format(*result.value), 1.5f);
        drawButton(context, l.copyX(), l.resultY() + 1, Layout::copyWidth, 12, translated("calc.copy"),
                        inside(pointer.x, pointer.y, l.copyX(), l.resultY() + 1, Layout::copyWidth, 12));
        float ax = l.copyX() + Layout::copyWidth + 1;
        arrowButton(context, ax, l.resultY() + 1, Layout::arrowWidth, 12, inside(pointer.x, pointer.y, ax, l.resultY() + 1, Layout::arrowWidth, 12),
                    menuFor == -1);
        if (auto amount = chests(*result.value); !amount.empty()) label(context, x, l.amountY(), lw, amount, palette::dim);
    } else if (result.error != calculator::Error::Empty) {
        label(context, x, l.resultY() + 3, lw, errorText(), palette::warning);
    }
    std::string_view tip;
    float tipX = 0, tipY = 0;
    float rh = l.rowHeight();
    for (int row = 0; row < 7; ++row)
        for (int column = 0; column < 6; ++column) {
            auto const& k = grid[row][column];
            float kx = l.keyX(column), ky = l.keyY(row);
            bool over = inside(pointer.x, pointer.y, kx, ky, l.cell(), rh);
            bool equals = k.kind == Kind::Equals;
            drawButtonBox(context, kx, ky, l.cell(), rh, over, equals ? ButtonKind::Primary : ButtonKind::Plain);
            drawKeyLabel(context, k, kx, ky, l.cell(), rh);
            if (over && !k.tip.empty()) { tip = k.tip; tipX = kx; tipY = ky - 13; }
        }

    // ---- Right: history cards, newest at the bottom ----
    float rx = l.rightX(), rw = l.rightWidth();
    fill(context, rx - 5, l.top + 19, 1, l.height - 19, palette::white, .14f);
    label(context, rx, l.top + 23, rw, translated("calc.history", history.size(), calculator::historyLimit), palette::faint);
    // Cards scroll by the unit and are clipped to their column.
    context.flushText(0, std::nullopt);
    context.saveCurrentClippingRectangle();
    context.setClippingRectangle(RectangleArea{rx - 1, rx + rw + 1, l.cardsTop(), l.bottom()});
    for (int index = 0; index < static_cast<int>(history.size()); ++index) {
        auto top = cardY(l, index);
        if (!top) continue;
        auto const& entry = history[static_cast<size_t>(index)];
        bool current = recalled >= 0 && index == static_cast<int>(history.size()) - 1 - recalled;
        bool over = !dragging && pointer.y >= l.cardsTop() && pointer.y < l.bottom() && inside(pointer.x, pointer.y, rx, *top, rw - 5, Layout::cardHeight);
        fill(context, rx, *top, rw - 5, Layout::cardHeight, current ? palette::accentDeep : palette::white, current ? .35f : over ? .08f : .04f);
        frame(context, rx, *top, rw - 5, Layout::cardHeight, current ? palette::accent : palette::white, current ? 1.f : .08f);
        bool buttons = over || menuFor == index;
        float textWidth = rw - 13 - (buttons ? Layout::copyWidth + Layout::arrowWidth + 6 : 0);
        label(context, rx + 4, *top + 3, textWidth, entry.text, palette::dim);
        label(context, rx + 4, *top + 12, rw - 13, "= " + calculator::format(entry.value));
        if (auto amount = chests(entry.value); !amount.empty()) labelScaled(context, rx + 4, *top + 22, rw - 13, amount, .75f, palette::faint);
        if (buttons) {
            float cx = cardCopyX(l);
            drawButton(context, cx, *top + 3, Layout::copyWidth, 10, translated("calc.copy"), inside(pointer.x, pointer.y, cx, *top + 3, Layout::copyWidth, 10));
            arrowButton(context, cx + Layout::copyWidth + 1, *top + 3, Layout::arrowWidth, 10,
                        inside(pointer.x, pointer.y, cx + Layout::copyWidth + 1, *top + 3, Layout::arrowWidth, 10), menuFor == index);
        }
    }
    context.flushText(0, std::nullopt);
    context.restoreSavedClippingRectangle();
    if (scrollRange(l) > 0) {
        float track = l.bottom() - l.cardsTop(), thumb = thumbLength(l), from = thumbTop(l);
        bool overBar = dragging || onScrollbar(l, pointer.x, pointer.y);
        drawScrollbar(context, rx + rw - 3, l.cardsTop(), track, from, thumb, overBar);
    }
    // Text is drawn when flushed: flush what lies under the overlays first, so
    // their backgrounds cover it (help showed the history through it).
    context.flushText(0, std::nullopt);
    // ---- Over everything: the copy menu, help, the tip and the note ----
    if (menuFor != -2 && (menuFor == -1 ? result.value.has_value() : cardY(l, menuFor).has_value())) {
        float mx = (menuFor == -1 ? l.copyX() : cardCopyX(l)) + Layout::copyWidth + Layout::arrowWidth + 1 - 110;
        float my = (menuFor == -1 ? l.resultY() + 1 : *cardY(l, menuFor) + 3) + 13;
        double value = menuFor == -1 ? *result.value : history[static_cast<size_t>(menuFor)].value;
        bool hasChests = !chests(value).empty();
        fill(context, mx, my, 110, 25, palette::panel, .98f);
        frame(context, mx, my, 110, 25, palette::accent);
        bool overLine = inside(pointer.x, pointer.y, mx, my, 110, 12), overChests = hasChests && inside(pointer.x, pointer.y, mx, my + 12, 110, 12);
        if (overLine) fill(context, mx + 1, my + 1, 108, 11, palette::white, .08f);
        if (overChests) fill(context, mx + 1, my + 13, 108, 11, palette::white, .08f);
        label(context, mx + 5, my + 2, 100, translated("calc.copyLine"));
        label(context, mx + 5, my + 14, 100, translated("calc.copyChests"), hasChests ? palette::text : palette::faint);
    }
    if (helpOpen) {
        float hx = rx - 2, hy = l.top + 22, hw = rw + 4, hh = l.height - 28;
        fill(context, hx, hy, hw, hh, palette::panel, .98f);
        frame(context, hx, hy, hw, hh, palette::accent);
        float y = hy + 5;
        for (auto keyName : {"calc.help.keys", "calc.help.copy", "calc.help.units", "calc.help.variables", "calc.help.functions", "calc.help.private"}) {
            auto text = translated(keyName);
            size_t lines = paragraphLines(context, hw - 10, text, 4);
            paragraph(context, hx + 5, y, hw - 10, text, 4, palette::dim);
            y += static_cast<float>(lines) * 12 + 4;
        }
    }
    if (!tip.empty()) {
        auto text = translated(tip);
        float w = textWidth(context, text) + 8;
        float tx = std::clamp(tipX, l.left + 2, l.left + l.width - w - 2);
        fill(context, tx, tipY, w, 12, palette::panel, .97f);
        frame(context, tx, tipY, w, 12, palette::keyEdge);
        label(context, tx + 4, tipY + 1 + boxTextInset() - 1, w - 8, text, palette::dim);
    }
    if (!note.empty() && std::chrono::steady_clock::now() < noteUntil) {
        float w = std::min(l.width, textWidth(context, note) + 12);
        float nx = l.left + (l.width - w) / 2, ny = std::min(l.top + l.height + 4, size.y - 14);
        fill(context, nx, ny, w, 12, palette::panel, .97f);
        frame(context, nx, ny, w, 12, palette::accent);
        label(context, nx, ny + 1 + boxTextInset() - 1, w, note, palette::text, Align::Center);
    }
    context.flushText(0, std::nullopt);
}
}

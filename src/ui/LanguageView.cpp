#include "ui/LanguageView.h"
#include "ui/GameLanguage.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/ScreenParts.h"
#include "ui/SettingsTableView.h"
#include "ui/Widgets.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include <chrono>

namespace lamium::ui::language_view {
namespace {
bool open = false, dragging = false;
LanguageSelection selection;
ShapesLayout last;
std::shared_ptr<game_language::Request> request;
std::string_view notice;
LanguagePace pace; // Kept across openings of the list.
int waitSeconds = 0;
double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
bool busy() { return request && request->result.load() == game_language::Result::Pending; }
void choose() {
    if (busy()) return;
    if (auto code = selection.request(game_language::currentCode(), game_language::savedCode())) {
        if (game_language::inWorld() && (waitSeconds = pace.wait(now())) > 0) { notice = "language.wait"; return; }
        request = game_language::apply(*code);
        notice = {};
    }
}
void back() { reset(); table_view::rebuild(true); }
void reveal() {
    selection.first = SettingsTable::reveal(selection.first, selection.selected, last.listVisible);
    selection.first = SettingsTable::clampFirst(selection.first, last.listCount, last.listVisible);
}
void poll() {
    if (!request) return;
    using R = game_language::Result;
    auto result = request->result.load();
    if (result == R::Pending) return;
    if (result == R::Applied) { notice = {}; if (game_language::inWorld()) pace.applied(now()); }
    else if (result == R::Saving) notice = "language.saving";
    else if (result == R::Cancelled) notice = "language.cancelled";
    else if (result == R::Unavailable) notice = "language.unavailable";
    else notice = "language.failed";
    request.reset();
}
bool overBack(float x, float y) {
    return x >= last.closeX && x < last.closeX + ShapesLayout::closeWidth && y >= last.top && y < last.rowsTop;
}
}
bool begin() {
    reset();
    selection = game_language::choices();
    if (selection.choices.empty()) { screen::setMessage(translated("language.unavailable")); return false; }
    table_view::endSearch();
    screen::finishEditing();
    open = true;
    return true;
}
bool opened() { return open; }
void reset() {
    if (request) request->cancelled.store(true);
    request.reset();
    selection = {}; last = {}; notice = {};
    open = false; dragging = false;
}
bool click(float x, float y, bool right) {
    if (last.width <= 0 || x < last.left || x >= last.left + last.width || y < last.top || y >= last.footerTop) return false;
    if (right) return true;
    if (overBack(x, y)) { back(); return true; }
    if (last.onScrollbar(x, y)) { selection.first = last.firstAt(y); dragging = true; return true; }
    if (y < last.rowsTop) return true;
    int offset = static_cast<int>((y - last.rowsTop) / ShapesLayout::rowHeight);
    if (offset >= last.listVisible || offset + last.listFirst >= last.listCount || busy()) return true;
    selection.select(offset + last.listFirst);
    choose();
    return true;
}
void key(int key) {
    if (key == 0x1b || key == 0x08) { back(); return; }
    if (busy()) return;
    switch (key) {
    case 0x26: selection.move(-1, last.listVisible); break;
    case 0x28: selection.move(1, last.listVisible); break;
    case 0x21: selection.move(-std::max(1, last.listVisible - 1), last.listVisible); break;
    case 0x22: selection.move(std::max(1, last.listVisible - 1), last.listVisible); break;
    case 0x24: selection.select(0); reveal(); break;
    case 0x23: selection.select(static_cast<int>(selection.choices.size()) - 1); reveal(); break;
    case 0x0d: case 0x20: choose(); break;
    }
}
void wheel(int amount) {
    selection.first = SettingsTable::clampFirst(selection.first + amount, static_cast<int>(selection.choices.size()), last.listVisible);
}
void release() { dragging = false; }
void renderContent(MinecraftUIRenderContext& context, SettingsTable const& t, glm::vec2 pointer) {
    poll();
    last = languageLayout(t, static_cast<int>(selection.choices.size()), selection.first);
    if (last.width <= 0) return;
    if (dragging) selection.first = last.firstAt(pointer.y);
    selection.first = last.listFirst = SettingsTable::clampFirst(selection.first, last.listCount, last.listVisible);
    auto current = game_language::currentCode();

    // Heading row: where this list belongs, and the way back to it.
    label(context, t.nameX, t.theadTop + 2, last.closeX - t.nameX - SettingsTable::gap,
        translated("section.interface") + "  >  " + translated("feature.gameLanguage"), palette::faint);
    drawButton(context, last.closeX, t.theadTop, ShapesLayout::closeWidth, 11,
        "< " + translated("section.interface"), overBack(pointer.x, pointer.y));
    fill(context, t.tableLeft, t.rowsTop - 1, t.tableWidth, 1, palette::white, .14f);

    float codeWidth = t.keyWidth;
    for (int i = last.listFirst; i < last.listFirst + last.listVisible && i < last.listCount; ++i) {
        auto const& choice = selection.choices[i];
        float y = last.listRowY(i);
        bool hovered = pointer.x >= last.listLeft && pointer.x < last.listLeft + last.listWidth
            && pointer.y >= y && pointer.y < y + ShapesLayout::rowHeight;
        rowBackground(context, last.listLeft + 1, y, last.listWidth - 2, ShapesLayout::rowHeight, selection.selected == i, hovered);
        label(context, t.nameX, y + 3, t.keyX - t.nameX - SettingsTable::gap, choice.name,
            choice.code == current ? palette::accent : palette::text);
        label(context, t.keyX, y + 3, codeWidth, choice.code, choice.code == current ? palette::accent : palette::faint);
    }
    drawListScrollbar(context, last);

    // Footer, as in the table: status first, hints below.
    fill(context, t.left, t.footerTop, t.width, 1, palette::white, .14f);
    float textLeft = t.left + SettingsTable::pad, textWidth = t.width - 2 * SettingsTable::pad;
    auto text = busy() ? translated("language.applying") : notice == "language.wait" ? translated(notice, waitSeconds)
        : !notice.empty() ? translated(notice)
        : translated("language.current", game_language::currentName());
    label(context, textLeft, t.footerTop + 3, textWidth, text, notice.empty() ? palette::text : palette::warning);
    label(context, textLeft, t.footerTop + 16, textWidth, translated("help.gameLanguage"), palette::dim);
    label(context, textLeft, t.footerTop + 30, textWidth, translated("language.hint"), palette::faint);
    context.flushText(0, std::nullopt);
}
}

#include "ui/SavedDataView.h"
#include "features/map/MapColors.h"
#include "features/map/SavedData.h"
#include "features/map/Waypoints.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/SavedDataLayout.h"
#include "ui/SavedDataThumb.h"
#include "ui/ScreenParts.h"
#include "ui/SettingsTableView.h"
#include "ui/Widgets.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include <chrono>
#include <ctime>

namespace lamium::ui::saved_data_view {
namespace {
using Data = map::waypoints::SavedData;
using Zone = SavedDataLayout::Zone;
bool open = false, dragging = false;
std::vector<Data> items;
int selected = 0, first = 0;
std::string armed; // The key whose Delete was pressed once.
std::string notice;
bool noticeWarns = false;
SavedDataLayout last;
// The name's cache for the settings row, which is drawn every frame.
std::string rowName;
double rowNameAt = -1;
double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

void back() { reset(); table_view::rebuild(true); }
Data const* chosen() { return selected >= 0 && selected < static_cast<int>(items.size()) ? &items[static_cast<size_t>(selected)] : nullptr; }
std::string shown(Data const& d) { return d.name.empty() ? translated("savedData.unnamed") : d.name; }
void reload(std::string const& keep = {}) {
    items = map::waypoints::savedData();
    selected = 0;
    for (size_t i = 0; i < items.size(); ++i) if (!keep.empty() && items[i].key == keep) selected = static_cast<int>(i);
    armed.clear();
    rowNameAt = -1;
}
void reveal() {
    first = SettingsTable::reveal(first, selected, last.list.listVisible);
    first = SettingsTable::clampFirst(first, last.list.listCount, last.list.listVisible);
}
void say(std::string text, bool warns = false) { notice = std::move(text); noticeWarns = warns; }
void use() {
    auto const* d = chosen();
    if (!d || !d->canUse) return;
    std::string const key = d->key, name = shown(*d);
    if (map::waypoints::useData(key)) { reload(key); say(translated("savedData.switched", name)); }
    else say(translated("savedData.failed"), true);
}
void remove() {
    auto const* d = chosen();
    if (!d || !d->canDelete) return;
    if (armed != d->key) { armed = d->key; return; }
    std::string const name = shown(*d);
    if (map::waypoints::removeData(*d)) { reload(); say(translated("savedData.deleted", name)); }
    else { armed.clear(); say(translated("savedData.failed"), true); }
}
void anyPort() {
    auto const* d = chosen();
    if (!d || d->kind != Data::Kind::Server || d->host.empty()) return;
    std::string const key = d->key, host = d->host;
    bool const on = !d->anyPort;
    int const released = map::waypoints::setDataAnyPort(key, on);
    if (released < 0) { say(translated("savedData.failed"), true); return; }
    reload(key);
    if (on && released > 0) say(translated("savedData.anyPortOn", released, host));
    else notice.clear();
}
std::string kindText(Data const& d) {
    return translated(d.kind == Data::Kind::Server ? "savedData.kind.server"
        : d.kind == Data::Kind::Friend ? "savedData.kind.friend" : "savedData.kind.local");
}
std::string dateText(std::int64_t seconds) {
    if (seconds <= 0) return "-";
    std::time_t const time = static_cast<std::time_t>(seconds);
    std::tm local{};
    if (localtime_s(&local, &time) != 0) return "-";
    char text[32];
    std::snprintf(text, sizeof text, "%04d-%02d-%02d", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
    return text;
}
std::string deleteText(Data const& d) {
    return armed == d.key ? translated("savedData.deleteArmed", d.waypoints, sizeText(d.mapBytes, d.mapMore)) : translated("savedData.delete");
}
// Why it may be the joined server's data, as the row's mark.
std::string maybeText(map::servers::Maybe maybe) {
    using M = map::servers::Maybe;
    return maybe == M::No ? std::string()
        : translated(maybe == M::UsedBefore ? "savedData.why.before" : maybe == M::SameLevel ? "savedData.why.level" : "savedData.why.address");
}
// Two rows that read the same get what tells them apart: a server's last use, a local world's folder.
std::string apart(Data const& d) {
    int same = 0;
    for (auto const& other : items) if (other.kind == d.kind && shown(other) == shown(d)) ++same;
    if (same < 2) return {};
    return d.kind == Data::Kind::Local ? d.key.substr(0, 8) : dateText(d.used);
}
Rgb markRgb(int color) {
    auto c = map::waypointColors[static_cast<size_t>(map::clampColor(color))];
    return {map::channel(c, 0) / 255.f, map::channel(c, 1) / 255.f, map::channel(c, 2) / 255.f};
}
}
bool begin() {
    reset();
    table_view::endSearch();
    screen::finishEditing();
    reload();
    open = true;
    return true;
}
bool opened() { return open; }
void reset() {
    data_thumb::reset();
    items.clear(); armed.clear(); notice.clear();
    selected = first = 0; last = {};
    open = false; dragging = false; noticeWarns = false;
}
std::string currentName() {
    double const time = now();
    if (rowNameAt < 0 || time - rowNameAt > 2) { rowName = map::waypoints::dataName(); rowNameAt = time; }
    return rowName;
}
bool click(float x, float y, bool right) {
    if (!last.usable() || x < last.left || x >= last.left + last.width || y < last.headTop || y >= last.footerTop) return false;
    if (right) return true;
    if (last.list.onScrollbar(x, y)) { first = last.list.firstAt(y); dragging = true; return true; }
    auto hit = last.hit(x, y);
    if (hit.zone != Zone::Delete) armed.clear();
    switch (hit.zone) {
    case Zone::Back: back(); break;
    case Zone::Row: selected = hit.index; notice.clear(); break;
    case Zone::AnyPort: anyPort(); break;
    case Zone::Use: use(); break;
    case Zone::Delete: remove(); break;
    case Zone::None: break;
    }
    return true;
}
void key(int key) {
    if (key == 0x1b || key == 0x08) {
        if (!armed.empty()) { armed.clear(); return; }
        back();
        return;
    }
    armed.clear();
    int const count = static_cast<int>(items.size());
    if (!count) return;
    auto move = [&](int amount) { selected = std::clamp(selected + amount, 0, count - 1); notice.clear(); reveal(); };
    switch (key) {
    case 0x26: move(-1); break;
    case 0x28: move(1); break;
    case 0x21: move(-std::max(1, last.list.listVisible - 1)); break;
    case 0x22: move(std::max(1, last.list.listVisible - 1)); break;
    case 0x24: move(-count); break;
    case 0x23: move(count); break;
    case 0x0d: use(); break;
    }
}
void wheel(int amount) { first = SettingsTable::clampFirst(first + amount, static_cast<int>(items.size()), last.list.listVisible); }
void release() { dragging = false; }
void calm() { dragging = false; armed.clear(); }
void renderContent(MinecraftUIRenderContext& context, SettingsTable const& t, glm::vec2 pointer) {
    auto const* d = chosen();
    bool const server = d && d->kind == Data::Kind::Server;
    std::string const useText = translated("savedData.use"), removeText = d ? deleteText(*d) : std::string();
    last = SavedDataLayout::fit(t, static_cast<int>(items.size()), first, server && !d->host.empty(), d && d->canUse,
        d && d->canDelete, textWidth(context, useText) + 12, textWidth(context, removeText) + 12);
    if (!last.usable()) return;
    if (dragging) first = last.list.firstAt(pointer.y);
    first = last.list.listFirst = SettingsTable::clampFirst(first, last.list.listCount, last.list.listVisible);
    auto hover = last.hit(pointer.x, pointer.y);

    // Heading row: where this page belongs, the total, and the way back.
    long long total = 0;
    for (auto const& item : items) total += item.mapBytes;
    std::string const totalText = translated("savedData.total", sizeText(total));
    float const totalWidth = textWidth(context, totalText);
    label(context, t.nameX, t.theadTop + 2, last.backX - t.nameX - totalWidth - 2 * SettingsTable::gap,
        translated("section.interface") + "  >  " + translated("feature.savedData"), palette::faint);
    label(context, last.backX - SettingsTable::gap - totalWidth, t.theadTop + 2, totalWidth, totalText, palette::faint);
    drawSmallButton(context, last.backX, t.theadTop, ShapesLayout::closeWidth, 11, "< " + translated("section.interface"),
        hover.zone == Zone::Back);
    fill(context, t.tableLeft, t.rowsTop - 1, t.tableWidth, 1, palette::white, .14f);

    // The list: kind, name, and what it is to the joined server.
    auto const& list = last.list;
    float const kindWidth = 46, pad = SettingsTable::pad;
    if (items.empty()) label(context, list.listLeft + pad, list.rowsTop + 3, list.listWidth - 2 * pad, translated("savedData.empty"), palette::faint);
    for (int i = list.listFirst; i < list.listFirst + list.listVisible && i < list.listCount; ++i) {
        auto const& item = items[static_cast<size_t>(i)];
        float const y = list.listRowY(i);
        rowBackground(context, list.listLeft + 1, y, list.listWidth - 2, ShapesLayout::rowHeight, selected == i,
            hover.zone == Zone::Row && hover.index == i);
        std::string const tag = item.inUse ? translated("savedData.inUse") : maybeText(item.maybe);
        float const tagWidth = tag.empty() ? 0 : textWidth(context, tag) + SettingsTable::gap;
        float const nameX = list.listLeft + pad + kindWidth, nameRoom = list.listWidth - 2 * pad - kindWidth - tagWidth - 6;
        std::string const name = shown(item), extra = apart(item);
        label(context, list.listLeft + pad, y + 3, kindWidth, kindText(item), palette::faint);
        label(context, nameX, y + 3, nameRoom, name, item.inUse ? palette::accent : palette::text);
        if (!extra.empty()) {
            float const used = std::min(nameRoom, textWidth(context, name) + SettingsTable::gap);
            if (nameRoom - used > 20) label(context, nameX + used, y + 3, nameRoom - used, extra, palette::faint);
        }
        if (!tag.empty())
            label(context, list.listLeft + list.listWidth - pad - 6 - tagWidth, y + 3, tagWidth, tag,
                item.inUse ? palette::accent : palette::warning, Align::Right);
    }
    drawListScrollbar(context, list);
    fill(context, last.dividerX, last.bodyTop, 1, last.footerTop - last.bodyTop, palette::white, .14f);

    // The details: enough to tell whose data it is.
    if (d) {
        float const x = last.detailLeft + pad, width = last.detailWidth - 2 * pad, line = SavedDataLayout::lineHeight;
        float y = last.bodyTop + 3;
        float const bottom = last.noteY - 2;
        auto row = [&](std::string text, Rgb color = palette::dim) {
            if (y + line > bottom) return;
            label(context, x, y, width, std::move(text), color);
            y += line;
        };
        row(shown(*d), palette::text);
        // The map picture with the waypoints on it, and their names beside it: whose data this is, at a glance.
        constexpr float thumbSide = 60;
        bool const pictured = y + thumbSide + 2 <= bottom && width >= thumbSide + 60;
        float const namesX = pictured ? x + thumbSide + 6 : x, namesWidth = width - (namesX - x), thumbTop = y + 1;
        if (pictured) {
            data_thumb::show(d->mapFolder, d->key);
            fill(context, x, thumbTop, thumbSide, thumbSide, Rgb{0, 0, 0}, .45f);
            if (data_thumb::draw(context, {x, thumbTop, thumbSide, thumbSide})) {
                for (auto const& dot : d->dots)
                    if (auto at = data_thumb::at(dot.x, dot.z)) {
                        float const cx = x + at->first * thumbSide, cy = thumbTop + at->second * thumbSide;
                        fill(context, cx - 2, cy - 2, 4, 4, Rgb{0, 0, 0});
                        fill(context, cx - 1, cy - 1, 2, 2, markRgb(dot.color));
                    }
            } else label(context, x, thumbTop + thumbSide / 2 - 5, thumbSide, translated("savedData.noMap"), palette::faint, Align::Center);
            frame(context, x, thumbTop, thumbSide, thumbSide, palette::keyEdge);
        }
        float const below = pictured ? thumbTop + thumbSide + 3 : y;
        auto beside = [&](std::string text, Rgb color, float indent = 0) {
            if (y + line > (pictured ? below : bottom)) return false;
            label(context, namesX + indent, y, namesWidth - indent, std::move(text), color);
            y += line;
            return true;
        };
        beside(translated("savedData.waypoints", d->waypoints), palette::dim);
        size_t named = 0;
        for (auto const& mark : d->marks) {
            float const markY = y;
            if (!beside(mark.name, palette::text, 10)) break;
            fill(context, namesX + 2, markY + 3, 5, 5, markRgb(mark.color));
            ++named;
        }
        if (d->waypoints > static_cast<int>(named) && named > 0)
            beside(translated("savedData.more", d->waypoints - static_cast<int>(named)), palette::faint, 10);
        if (pictured) y = below;
        row(translated("savedData.placements", d->placements));
        row(translated("savedData.map", sizeText(d->mapBytes, d->mapMore)));
        row(translated("savedData.used", dateText(d->used)));
        if (!d->levelId.empty() && d->kind == Data::Kind::Server) row(translated("savedData.levelId", d->levelId), palette::faint);

        std::string note = d->inUse ? translated("savedData.note.inUse")
            : d->kind != Data::Kind::Server ? translated("savedData.note.world")
            : !d->canUse ? translated("savedData.note.noServer")
            : translated("savedData.note.use", currentName());
        paragraph(context, x, last.noteY, width, note, static_cast<size_t>(SavedDataLayout::noteLines), palette::dim);
        if (last.anyPort) {
            // As a settings row: the words left, the switch at the right edge.
            label(context, x, last.anyPortY, width - switchWidth - 4, translated("savedData.anyPort", d->host),
                hover.zone == Zone::AnyPort ? palette::text : palette::dim);
            toggleSwitch(context, x + width - switchWidth, last.anyPortY + (line - switchHeight) / 2, d->anyPort);
        }
        if (last.use)
            drawSmallButton(context, last.useX, last.useY(), last.useWidth, SavedDataLayout::buttonHeight, useText,
                hover.zone == Zone::Use, palette::accentDeep, palette::accent);
        if (last.remove) {
            // The same red as the other press-twice deletes (L-341 collects them).
            bool const isArmed = armed == d->key;
            drawSmallButton(context, last.deleteX, last.deleteY, last.deleteWidth, SavedDataLayout::buttonHeight, removeText,
                hover.zone == Zone::Delete, isArmed ? Rgb{.54f,.18f,.16f} : palette::keyFill, Rgb{.54f,.23f,.2f},
                isArmed ? palette::text : Rgb{1.f,.7f,.68f});
        }
    }

    // Footer, as in the table: status first, hints below.
    fill(context, t.left, t.footerTop, t.width, 1, palette::white, .14f);
    float const textLeft = t.left + SettingsTable::pad, textWidth_ = t.width - 2 * SettingsTable::pad;
    std::string const current = currentName();
    label(context, textLeft, t.footerTop + 3, textWidth_,
        !notice.empty() ? notice : current.empty() ? translated("savedData.noCurrent") : translated("savedData.current", current),
        !notice.empty() && noticeWarns ? palette::warning : palette::text);
    label(context, textLeft, t.footerTop + 16, textWidth_, translated("help.savedData"), palette::dim);
    label(context, textLeft, t.footerTop + 30, textWidth_, translated("savedData.hint"), palette::faint);
    context.flushText(0, std::nullopt);
}
}

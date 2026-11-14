#include "ui/HighlightView.h"
#include "ui/HighlightLayout.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/ScreenParts.h"
#include "ui/SearchQuery.h"
#include "ui/SettingsTableView.h"
#include "ui/Widgets.h"
#include "app/Capabilities.h"
#include "app/Runtime.h"
#include "features/inspection/render/ItemIcon.h"
#include "features/visuals/BlockHighlight.h"
#include "features/visuals/BlockHighlightHooks.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/block/registry/BlockTypeRegistry.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lamium::ui::highlight_view {
namespace {
using Zone = HighlightLayout::Zone;
namespace hl = visuals::highlight;
// Every registered block type, read once per opening: its name and its
// display name in the current language.
struct Choice { std::string name, display; };
std::vector<Choice> catalog;
std::unordered_map<std::string, size_t> catalogIndex;
std::vector<size_t> results; // indices into `catalog`
bool open = false;
SearchQuery query;
int listedFirst = 0, resultFirst = 0, selected = -1;
enum class Drag { None, Listed, Results } dragging = Drag::None;
std::string armed;   // a listed name whose Remove was pressed once
HighlightLayout last;
glm::vec2 lastPointer{};
// Item stacks for icons, made from each type's default block; they point at
// the world's items, so they are dropped with the view (SchematicItems.h).
std::map<std::string, ItemStack, std::less<>> stacks;
// Nearby counts from the recorded marks, refreshed while open.
std::map<std::string, size_t, std::less<>> nearby;
// A pass over the marks reads the world, so it runs a slice per frame and
// replaces `nearby` only when it finishes.
std::vector<hl::Mark> marks;
std::uint64_t markRevision = 0;
double countedAt = -1;
bool countCapped = false;
std::map<std::string, size_t, std::less<>> counting;
size_t countNext = 0, countEnd = 0;
bool countingCapped = false, countPass = false;
constexpr size_t maxCountedMarks = 200000;
constexpr size_t countsPerFrame = 20000;

double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
std::vector<hl::Entry> listed() { return Runtime::instance().preferences().visuals.highlightBlocks; }
bool isListed(std::vector<hl::Entry> const& list, std::string_view name) {
    return std::any_of(list.begin(), list.end(), [&](hl::Entry const& e) { return e.name == name; });
}
std::string displayOf(std::string const& name) {
    auto found = catalogIndex.find(name);
    return found != catalogIndex.end() && !catalog[found->second].display.empty() ? catalog[found->second].display
        : std::string(hl::shortName(name));
}
void buildCatalog() {
    catalog.clear(); catalogIndex.clear();
    try {
        for (auto const& [key, type] : *BlockTypeRegistry::get().mBlockLookupMap) {
            if (!type) continue;
            std::string name = key.getString();
            if (name.empty()) continue;
            std::string display;
            try { if (Block const* block = type->mDefaultState; block) display = block->buildDescriptionName(); } catch (...) {}
            catalog.push_back({std::move(name), std::move(display)});
        }
    } catch (...) { catalog.clear(); }
    std::sort(catalog.begin(), catalog.end(), [](Choice const& a, Choice const& b) { return hl::shortName(a.name) < hl::shortName(b.name); });
    for (size_t i = 0; i < catalog.size(); ++i) catalogIndex.emplace(catalog[i].name, i);
}
void search() {
    results.clear();
    if (query.value().find_first_not_of(' ') == std::string::npos) {
        for (auto name : hl::suggestedNames)
            if (auto found = catalogIndex.find(std::string(name)); found != catalogIndex.end()) results.push_back(found->second);
    } else {
        for (size_t i = 0; i < catalog.size(); ++i)
            if (hl::queryMatches(query.value(), catalog[i].name, catalog[i].display)) results.push_back(i);
    }
    resultFirst = 0;
    selected = results.empty() ? -1 : 0;
}
ItemStack const* stackFor(std::string const& name) {
    auto found = stacks.find(name);
    if (found == stacks.end()) {
        ItemStack stack;
        try {
            if (auto const* type = BlockTypeRegistry::get().lookupByName(HashedString{name}); type) {
                Block const* block = type->mDefaultState;
                if (block) stack.reinit(*block, 1);
            }
        } catch (...) { stack = ItemStack(); }
        stack.mShowPickUp = false;
        stack.mWasPickedUp = false;
        found = stacks.emplace(name, std::move(stack)).first;
    }
    return found->second.isNull() ? nullptr : &found->second;
}
void drawIcon(MinecraftUIRenderContext& context, std::string const& name, float x, float y) {
    auto const* stack = stackFor(name);
    if (!stack) return;
    float s = drawScale(); // icons draw in GUI units, outside the widgets' scale
    inspection::render::drawItemIcon(context, {stack, std::round(x * s), std::round(y * s), 10 * s / 16}, 17);
}
bool highlightOn() {
    return Runtime::instance().preferences().visuals.blockHighlight
        && capabilities::registry().status(capabilities::Id::BlockHighlight).available();
}
// The marks name cells only; the type comes from the block there now.
void refreshCounts() {
    if (!highlightOn()) { nearby.clear(); countPass = false; return; }
    auto* client = screen::client();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return;
    if (!countPass) {
        double t = now();
        if (t - countedAt < .5) return;
        countedAt = t;
        if (!hl::marks(static_cast<int>(player->getDimensionId()), markRevision, marks)) return;
        counting.clear();
        for (auto const& e : listed()) counting[e.name] = 0;
        countingCapped = marks.size() > maxCountedMarks;
        countNext = 0;
        countEnd = std::min(marks.size(), maxCountedMarks);
        countPass = true;
    }
    auto& region = player->getDimensionBlockSource();
    size_t stop = std::min(countEnd, countNext + countsPerFrame);
    try {
        for (; countNext < stop; ++countNext) {
            auto const& cell = marks[countNext].cell;
            BlockPos at{cell.x, cell.y, cell.z};
            if (!region.getChunkAt(at)) continue;
            auto found = counting.find(region.getBlock(at).getTypeName());
            if (found != counting.end()) ++found->second;
        }
    } catch (...) { countNext = countEnd; }
    if (countNext < countEnd) return;
    nearby = std::move(counting);
    counting.clear();
    countCapped = countingCapped;
    countPass = false;
}
void forceRecount() { countedAt = -1; markRevision = 0; countPass = false; }
void toggle(std::string const& name) {
    auto& runtime = Runtime::instance();
    auto value = runtime.preferences();
    auto change = hl::toggle(value.visuals.highlightBlocks, name);
    if (change == hl::Change::Invalid) return;
    if (change == hl::Change::Full) { screen::setMessage(translated("highlight.full")); return; }
    // Adding a type means wanting to see it, as with the key.
    if (change == hl::Change::Added) value.visuals.blockHighlight = true;
    if (!runtime.save(value)) { screen::setMessage(translated("saveError")); return; }
    screen::clearMessage();
    armed.clear();
    forceRecount();
}
void back() { screen::endDirectTool(); reset(); table_view::rebuild(true); }
void revealSelected() {
    if (selected < 0) return;
    resultFirst = SettingsTable::reveal(resultFirst, selected, last.results.listVisible);
    resultFirst = SettingsTable::clampFirst(resultFirst, static_cast<int>(results.size()), last.results.listVisible);
}
void moveSelection(int delta) {
    if (results.empty()) return;
    selected = std::clamp(selected < 0 ? 0 : selected + delta, 0, static_cast<int>(results.size()) - 1);
    revealSelected();
}
}
bool begin() {
    reset();
    auto* client = screen::client();
    if (!client || !client->getLocalPlayer()) { screen::setMessage(translated("worldOnly")); return false; }
    table_view::endSearch();
    screen::finishEditing();
    buildCatalog();
    if (catalog.empty()) { screen::setMessage(translated("highlight.noCatalog")); return false; }
    open = true;
    search();
    return true;
}
bool opened() { return open; }
void reset() {
    open = false;
    catalog.clear(); catalogIndex.clear(); results.clear(); stacks.clear(); nearby.clear(); marks.clear();
    query.clear();
    listedFirst = resultFirst = 0; selected = -1;
    dragging = Drag::None;
    armed.clear();
    last = {};
    forceRecount();
}
TextEdit& field() { return query; }
void edited() { armed.clear(); search(); }
void type(std::string const& text) { if (query.type(text)) edited(); }
bool click(float x, float y, bool right) {
    if (!last.usable() || x < last.left || x >= last.left + last.width || y < last.headTop || y >= last.footerTop) return false;
    if (right) return true;
    if (last.listed.onScrollbar(x, y)) { listedFirst = last.listed.firstAt(y); dragging = Drag::Listed; return true; }
    if (last.results.onScrollbar(x, y)) { resultFirst = last.results.firstAt(y); dragging = Drag::Results; return true; }
    auto hit = last.hit(x, y);
    auto list = listed();
    switch (hit.zone) {
    case Zone::Back: back(); return true;
    case Zone::Remove:
        if (hit.index >= 0 && hit.index < static_cast<int>(list.size())) {
            auto const& name = list[static_cast<size_t>(hit.index)].name;
            if (armed == name) toggle(name);
            else armed = name;
        }
        return true;
    case Zone::Result:
        if (hit.index >= 0 && hit.index < static_cast<int>(results.size())) {
            selected = hit.index;
            toggle(catalog[results[static_cast<size_t>(hit.index)]].name);
        }
        return true;
    default: armed.clear(); return true;
    }
}
void key(int key) {
    switch (key) {
    case 0x1b:
        if (!query.value().empty()) { query.clear(); edited(); }
        else if (!screen::leaveDirectTool()) back();
        return;
    case 0x26: moveSelection(-1); return;
    case 0x28: moveSelection(1); return;
    case 0x21: moveSelection(-std::max(1, last.results.listVisible - 1)); return;
    case 0x22: moveSelection(std::max(1, last.results.listVisible - 1)); return;
    case 0x0d:
        if (selected >= 0 && selected < static_cast<int>(results.size())) toggle(catalog[results[static_cast<size_t>(selected)]].name);
        return;
    }
}
void wheel(int amount) {
    if (lastPointer.x < last.dividerX) listedFirst = SettingsTable::clampFirst(listedFirst + amount, last.listed.listCount, last.listed.listVisible);
    else resultFirst = SettingsTable::clampFirst(resultFirst + amount, static_cast<int>(results.size()), last.results.listVisible);
}
void release() { dragging = Drag::None; }
void renderContent(MinecraftUIRenderContext& context, SettingsTable const& t, glm::vec2 pointer) {
    lastPointer = pointer;
    refreshCounts();
    auto list = listed();
    if (!armed.empty() && !isListed(list, armed)) armed.clear();
    last = HighlightLayout::fit(t, static_cast<int>(list.size()), listedFirst, static_cast<int>(results.size()), resultFirst);
    if (!last.usable()) return;
    if (dragging == Drag::Listed) last.listed.listFirst = last.listed.firstAt(pointer.y);
    if (dragging == Drag::Results) last.results.listFirst = last.results.firstAt(pointer.y);
    listedFirst = last.listed.listFirst = SettingsTable::clampFirst(last.listed.listFirst, last.listed.listCount, last.listed.listVisible);
    resultFirst = last.results.listFirst = SettingsTable::clampFirst(last.results.listFirst, last.results.listCount, last.results.listVisible);
    auto hover = last.hit(pointer.x, pointer.y);
    float pad = SettingsTable::pad;
    bool on = highlightOn();

    // Heading row: where this list belongs, and the way back to it.
    label(context, t.nameX, t.theadTop + 2, last.backX - t.nameX - SettingsTable::gap,
        translated("section.camera") + "  >  " + translated("feature.blockHighlight"), palette::faint);
    drawSmallButton(context, last.backX, t.theadTop, ShapesLayout::closeWidth, 11, "< " + translated("section.camera"), hover.zone == Zone::Back);
    fill(context, t.tableLeft, t.rowsTop - 1, t.tableWidth, 1, palette::white, .14f);
    fill(context, last.dividerX, t.rowsTop, 1, last.footerTop - t.rowsTop, palette::white, .14f);

    // Left: the listed types, in the order added.
    auto const& lc = last.listed;
    float countRight = last.removeX() - 4;
    label(context, lc.listLeft + pad, last.bodyTop + 2, lc.listWidth - 2 * pad,
        translated("highlight.listed", list.size(), hl::maxEntries), palette::faint);
    if (on && !list.empty()) label(context, lc.listLeft + pad, last.bodyTop + 2, countRight - lc.listLeft - pad, translated("highlight.nearby"), palette::faint, Align::Right);
    if (list.empty())
        paragraph(context, lc.listLeft + pad, lc.rowsTop + 3, lc.listWidth - 2 * pad, translated("highlight.empty"), 6, palette::faint);
    for (int i = lc.listFirst; i < lc.listFirst + lc.listVisible && i < lc.listCount; ++i) {
        auto const& name = list[static_cast<size_t>(i)].name;
        float y = lc.listRowY(i);
        bool rowHover = (hover.zone == Zone::Listed || hover.zone == Zone::Remove) && hover.index == i;
        rowBackground(context, lc.listLeft + 1, y, lc.listWidth - 2, ShapesLayout::rowHeight, false, rowHover);
        drawIcon(context, name, lc.listLeft + pad, y + 2);
        float nameLeft = lc.listLeft + pad + 14;
        std::string count;
        if (on) if (auto found = nearby.find(name); found != nearby.end())
            count = std::to_string(found->second) + (countCapped ? "+" : "");
        float countWidth = count.empty() ? 0 : textWidth(context, count) + 4;
        label(context, nameLeft, y + 3, countRight - countWidth - nameLeft, displayOf(name));
        if (!count.empty()) label(context, countRight - countWidth, y + 3, countWidth, count, count == "0" ? palette::faint : palette::dim, Align::Right);
        if (rowHover || armed == name) {
            bool isArmed = armed == name;
            drawDangerButton(context, last.removeX(), y + 1, HighlightLayout::removeWidth, 12,
                translated(isArmed ? "highlight.removeConfirm" : "highlight.remove"), hover.zone == Zone::Remove && hover.index == i, isArmed);
        }
    }
    drawListScrollbar(context, lc);

    // Right: the search field, then the results.
    float fx = last.searchX(), fw = last.searchWidth();
    fill(context, fx, last.searchY, fw, HighlightLayout::fieldHeight, palette::shade, .45f);
    frame(context, fx, last.searchY, fw, HighlightLayout::fieldHeight, palette::accent);
    if (query.value().empty())
        label(context, fx + 4, last.searchY + 1 + boxTextInset(), fw - 8, translated("highlight.searchPlaceholder"), palette::faint);
    drawEditField(context, fx + 4, last.searchY, HighlightLayout::fieldHeight, fw - 8, query);
    auto const& rc = last.results;
    bool searching = query.value().find_first_not_of(' ') != std::string::npos;
    label(context, rc.listLeft + pad, rc.rowsTop - HighlightLayout::titleHeight + 2, rc.listWidth - 2 * pad,
        searching ? translated("highlight.found", results.size()) : translated("highlight.suggested"), palette::faint);
    if (searching && results.empty())
        label(context, rc.listLeft + pad, rc.rowsTop + 3, rc.listWidth - 2 * pad, translated("highlight.noMatch"), palette::faint);
    float tagLeft = last.addX();
    // The ID column takes a share of what is left beside the tag, so it
    // stays readable at the narrowest window.
    float idLeft = rc.listLeft + pad + 14 + (tagLeft - rc.listLeft - pad - 14) * .55f;
    for (int i = rc.listFirst; i < rc.listFirst + rc.listVisible && i < rc.listCount; ++i) {
        auto const& choice = catalog[results[static_cast<size_t>(i)]];
        float y = rc.listRowY(i);
        bool rowHover = hover.zone == Zone::Result && hover.index == i;
        bool listedNow = isListed(list, choice.name);
        rowBackground(context, rc.listLeft + 1, y, rc.listWidth - 2, ShapesLayout::rowHeight, selected == i, rowHover);
        drawIcon(context, choice.name, rc.listLeft + pad, y + 2);
        float nameLeft = rc.listLeft + pad + 14;
        label(context, nameLeft, y + 3, idLeft - nameLeft - 4, choice.display.empty() ? std::string(hl::shortName(choice.name)) : choice.display,
            listedNow ? palette::accent : palette::text);
        label(context, idLeft, y + 3, tagLeft - idLeft - 4, std::string(hl::shortName(choice.name)), palette::faint);
        if (listedNow) label(context, tagLeft, y + 3, HighlightLayout::addWidth, translated("highlight.tagListed"), palette::accent, Align::Center);
        else if (rowHover) label(context, tagLeft, y + 3, HighlightLayout::addWidth, translated("highlight.add"), palette::dim, Align::Center);
    }
    drawListScrollbar(context, rc);

    // Footer, as in the table: status first, then what the list is, then keys.
    fill(context, t.left, t.footerTop, t.width, 1, palette::white, .14f);
    float textLeft = t.left + SettingsTable::pad, footWidth = t.width - 2 * SettingsTable::pad;
    std::string status = screen::message();
    Rgb statusColor = palette::warning;
    if (status.empty() && !on) status = translated("highlight.offNote");
    if (status.empty()) {
        statusColor = palette::text;
        std::string hovered = hover.zone == Zone::Result && hover.index >= 0 ? catalog[results[static_cast<size_t>(hover.index)]].name
            : (hover.zone == Zone::Listed || hover.zone == Zone::Remove) && hover.index >= 0 && hover.index < static_cast<int>(list.size())
                ? list[static_cast<size_t>(hover.index)].name : std::string{};
        status = hovered.empty() ? translated("highlight.nearbyHelp") : displayOf(hovered) + "  " + hovered;
    }
    label(context, textLeft, t.footerTop + 3, footWidth, status, statusColor);
    label(context, textLeft, t.footerTop + 16, footWidth, translated("help.highlightList"), palette::dim);
    label(context, textLeft, t.footerTop + 30, footWidth, translated("highlight.hint"), palette::faint);
    context.flushText(0, std::nullopt);
}
}

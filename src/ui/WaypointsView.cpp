#include "ui/DataNotice.h"
#include "ui/WaypointsView.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/RowSelection.h"
#include "ui/ScreenParts.h"
#include "ui/SearchQuery.h"
#include "ui/ShapesLayout.h"
#include "features/map/WaypointSession.h"
#include "features/map/Waypoints.h"
#include "features/camera/CameraSessions.h"
#include "app/Runtime.h"
#include "app/SessionIds.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <functional>
#include <vector>

namespace lamium::ui::waypoints_view {
namespace {
using ShapeZone = ShapesLayout::Zone;
// Waypoints view (L-60 step 5c): built like Shapes, from a copy of the
// current world's waypoints taken each frame.
bool waypointsDocked = false;
std::optional<map::MarkKey> waypointSelected; // the death point or a waypoint, by its session id
// What the list has selected (L-190); waypointSelected is the row the editor
// works on while it is alone, none while several show the selection pane.
RowSelection<map::MarkKey> waypointRows;
BulkArm waypointBulk;
bool severalSelected() { return waypointRows.several(); }
void syncSelected() { waypointSelected = waypointRows.single(); }
map::WaypointSet waypointSet;
std::vector<size_t> waypointList; // Display order, indices into the set.
ShapesLayout waypointsDisplayed;
int waypointListFirst = 0, waypointFieldFirst = 0, waypointFieldSelected = -1;
bool waypointDeleteArmed = false;
int editingWaypointField = -1;
bool editingWaypointName = false, waypointNameDirty = false;
SearchQuery waypointNameInput;
void applyWaypointName();
map::Waypoint const* selectedWaypoint();
void changeSelected(std::function<void(map::Waypoint&)> const& apply);
bool fromMapFlag = false; // opened from the world map: Close goes back to it
// ---- Waypoints view ----
void refreshWaypoints() {
    waypointSet = map::waypoints::current();
    double x = 0, y = 0, z = 0;
    if (auto* player = screen::client() ? screen::client()->getLocalPlayer() : nullptr) {
        auto feet = player->getFeetPos();
        x = feet.x;
        y = feet.y;
        z = feet.z;
    }
    waypointList = map::waypointOrder(waypointSet.waypoints, screen::playerDimension(), x, y, z, screen::waypointsHorizontal());
    waypointRows.retain([](map::MarkKey const& key) { return indexOfId(waypointSet.waypoints, key.id) >= 0; });
    syncSelected();
}
int waypointRowCount() { return static_cast<int>(waypointList.size()); }
// The selection a list row stands for.
std::optional<map::MarkKey> waypointAtRow(int row) {
    if (row < 0 || row >= static_cast<int>(waypointList.size())) return std::nullopt;
    return map::waypointKey(waypointSet.waypoints[waypointList[static_cast<size_t>(row)]].id);
}
map::Waypoint const* selectedWaypoint() {
    if (!waypointSelected || waypointSelected->layer != map::MarkLayer::Waypoint) return nullptr;
    int index = indexOfId(waypointSet.waypoints, waypointSelected->id);
    return index < 0 ? nullptr : &waypointSet.waypoints[static_cast<size_t>(index)];
}
// A change to one waypoint, found by its id when the change runs.
bool changeWaypoint(std::optional<map::MarkKey> mark, std::function<void(map::Waypoint&)> const& apply) {
    return mark && mark->layer == map::MarkLayer::Waypoint && map::waypoints::change([&](map::WaypointSet& set) {
        int index = indexOfId(set.waypoints, mark->id);
        if (index < 0) return false;
        apply(set.waypoints[static_cast<size_t>(index)]);
        return true;
    });
}
void selectWaypoint(std::optional<map::MarkKey> value) {
    screen::finishEditing();
    if (value) waypointRows.only(*value);
    else waypointRows.clear();
    syncSelected();
    waypointFieldFirst = 0;
    waypointFieldSelected = -1;
    waypointDeleteArmed = false;
    waypointBulk.disarm();
}
void applyWaypointName() {
    if (!editingWaypointName || !std::exchange(waypointNameDirty, false)) return;
    auto name = waypointNameInput.value();
    if (name.find_first_not_of(' ') == std::string::npos) return; // An empty name keeps the old one.
    if (!changeWaypoint(waypointSelected, [&](map::Waypoint& w) { w.name = name; })) screen::setMessage(translated("waypoint.saveError"));
    refreshWaypoints();
}
// Waypoints placed "here" follow FreeCamera's position; schematics keep the body's.
std::optional<screen::Place> waypointPlace() {
    auto place = screen::standingPlace();
    if (!place || !screen::client()) return place;
    if (auto feet = CameraSessions::instance().freeCameraPose(*screen::client());
        feet && std::isfinite(feet->x) && std::isfinite(feet->y) && std::isfinite(feet->z))
        return screen::Place{static_cast<int>(std::floor(feet->x)), static_cast<int>(std::floor(feet->y)),
                     static_cast<int>(std::floor(feet->z)), place->dimension};
    return place;
}
void addWaypointHere() {
    auto place = waypointPlace();
    if (!place) return;
    map::Waypoint w;
    w.x = place->x; w.y = place->y; w.z = place->z; w.dimension = place->dimension;
    w.color = map::nextColor(waypointSet.lastColor);
    w.temporary = waypointSet.lastTemporary; // as the key and map prompts start
    w.name = map::defaultWaypointName(waypointSet.waypoints, [](int n) { return translated("waypoint.defaultName", n); });
    if (auto id = map::waypoints::add(w)) {
        refreshWaypoints();
        selectWaypoint(map::waypointKey(id));
        screen::clearMessage();
    } else screen::setMessage(translated("waypoint.saveError"));
}
// A change to the selected waypoint; reports only a failed save.
void changeSelected(std::function<void(map::Waypoint&)> const& apply) {
    if (!changeWaypoint(waypointSelected, apply)) screen::setMessage(translated("waypoint.saveError"));
    else screen::clearMessage();
    refreshWaypoints();
}
// Click or key activation of an editor row. part: -1/1 step, 0 value, 2 label.
void activateWaypointField(int index, int part) {
    auto const* w = selectedWaypoint();
    if (!w || index < 0 || index >= static_cast<int>(map::waypointFields.size())) return;
    waypointFieldSelected = index;
    if (part == 2) return;
    int direction = part == -1 ? -1 : 1;
    switch (map::waypointFields[static_cast<size_t>(index)]) {
    case map::WaypointField::X: case map::WaypointField::Y: case map::WaypointField::Z: {
        int value = index == 0 ? w->x : index == 1 ? w->y : w->z;
        if (part == 0) {
            editingWaypointField = index;
            screen::number().beginPrecise(value);
            screen::clearMessage();
            return;
        }
        changeSelected([&](map::Waypoint& t) {
            int& c = index == 0 ? t.x : index == 1 ? t.y : t.z;
            c = std::clamp(c + direction * (screen::heldShift() ? 10 : 1), -map::coordinateLimit, map::coordinateLimit);
        });
        return;
    }
    case map::WaypointField::MoveHere:
        if (auto place = waypointPlace())
            changeSelected([&](map::Waypoint& t) { t.x = place->x; t.y = place->y; t.z = place->z; t.dimension = place->dimension; });
        return;
    case map::WaypointField::Visible: changeSelected([](map::Waypoint& t) { t.visible = !t.visible; }); return;
    case map::WaypointField::Temporary: changeSelected([](map::Waypoint& t) { t.temporary = !t.temporary; }); return;
    case map::WaypointField::Color: {
        int count = static_cast<int>(map::waypointColors.size());
        changeSelected([&](map::Waypoint& t) { t.color = (map::clampColor(t.color) + direction + count) % count; });
        return;
    }
    }
}
void moveWaypointField(int step) {
    if (!selectedWaypoint()) return;
    int count = static_cast<int>(map::waypointFields.size());
    waypointFieldSelected = std::clamp(waypointFieldSelected + step, 0, count - 1);
    int visible = waypointsDisplayed.fieldVisible;
    if (visible > 0) {
        if (waypointFieldSelected < waypointFieldFirst) waypointFieldFirst = waypointFieldSelected;
        if (waypointFieldSelected >= waypointFieldFirst + visible) waypointFieldFirst = waypointFieldSelected - visible + 1;
    }
}
void openWaypointKeySettings() { screen::showFeatureKeys("waypoints"); }
void deleteSelectedWaypoint() {
    auto selection = waypointSelected;
    if (!selection) return;
    int row = 0;
    for (int i = 0; i < waypointRowCount(); ++i) if (waypointAtRow(i) == selection) row = i;
    bool saved = map::waypoints::change([&](map::WaypointSet& set) {
        int index = indexOfId(set.waypoints, selection->id);
        if (index < 0) return false;
        set.waypoints.erase(set.waypoints.begin() + index);
        return true;
    });
    if (!saved) { screen::setMessage(translated("waypoint.saveError")); return; }
    screen::clearMessage();
    refreshWaypoints();
    int rowsLeft = waypointRowCount();
    selectWaypoint(rowsLeft ? waypointAtRow(std::min(row, rowsLeft - 1)) : std::nullopt);
}
std::vector<map::MarkKey> waypointOrder() {
    std::vector<map::MarkKey> order;
    order.reserve(waypointList.size());
    for (size_t index : waypointList) order.push_back(map::waypointKey(waypointSet.waypoints[index].id));
    return order;
}
// Ctrl + click toggles a row, Shift + click takes a range, Ctrl + A takes all.
void pickRows(map::MarkKey const& value) {
    screen::finishEditing();
    auto before = waypointSelected;
    waypointRows.click(value, screen::heldCtrl(), screen::heldShift(), waypointOrder());
    syncSelected();
    if (waypointSelected != before) { waypointFieldFirst = 0; waypointFieldSelected = -1; }
    waypointDeleteArmed = false;
    waypointBulk.disarm();
}
void bulkWaypoints(BulkAction action) {
    auto const keys = waypointRows.keys;
    bool saved = map::waypoints::change([&](map::WaypointSet& set) {
        for (auto const& key : keys) {
            int index = indexOfId(set.waypoints, key.id);
            if (index < 0) continue;
            if (action == BulkAction::Delete) set.waypoints.erase(set.waypoints.begin() + index);
            else set.waypoints[static_cast<size_t>(index)].visible = action == BulkAction::Show;
        }
        return true;
    });
    if (!saved) { screen::setMessage(translated("waypoint.saveError")); return; }
    screen::clearMessage();
    if (action == BulkAction::Delete) selectWaypoint(std::nullopt);
    refreshWaypoints();
}
void handleWaypointClick(float x, float y, bool right) {
    screen::finishEditing();
    // A right click steps a stepper down and presses nothing else (L-346).
    if (right) {
        if (auto hit = waypointsDisplayed.hit(x, y); hit.zone == ShapeZone::Field) activateWaypointField(hit.index, -1);
        return;
    }
    if (auto pressed = right ? data_notice::Pressed::Outside : data_notice::press(x, y); pressed != data_notice::Pressed::Outside) {
        if (pressed == data_notice::Pressed::Open) screen::showSavedData();
        else refresh();
        if (auto done = data_notice::takeResult()) done->warns ? screen::setMessage(done->text) : screen::setInfo(done->text);
        return;
    }
    if (!right && screen::pressScrollbar(waypointsDisplayed, waypointListFirst, x, y)) return;
    if (!waypointsDocked && screen::navClick(x, y)) return;
    auto hit = waypointsDisplayed.hit(x, y);
    if (!(hit.zone == ShapeZone::Action && hit.index == 1)) waypointDeleteArmed = false;
    if (hit.zone != ShapeZone::Bulk) waypointBulk.disarm();
    switch (hit.zone) {
    case ShapeZone::Close: if (fromMapFlag) screen::returnToMap(); else screen::close(); return;
    case ShapeZone::Dock: waypointsDocked = !waypointsDocked; return;
    case ShapeZone::Keys: openWaypointKeySettings(); return;
    case ShapeZone::DrawAll:
        screen::toggleOption("map.waypoints");
        return;
    case ShapeZone::NewShape: addWaypointHere(); return;
    case ShapeZone::ListRow: {
        auto value = waypointAtRow(hit.index);
        auto const& l = waypointsDisplayed;
        if (value && value->layer == map::MarkLayer::Waypoint && listColumns(l.listLeft, l.listWidth, 0, switchWidth).onShown(x)) {
            if (!changeWaypoint(value, [](map::Waypoint& t) { t.visible = !t.visible; })) screen::setMessage(translated("waypoint.saveError"));
            else screen::clearMessage();
            refreshWaypoints();
            return;
        }
        if (value && (screen::heldCtrl() || screen::heldShift())) {
            pickRows(*value);
            return;
        }
        if (value && (value != waypointSelected || severalSelected())) selectWaypoint(value);
        return;
    }
    case ShapeZone::Name:
        if (auto const* w = selectedWaypoint()) {
            editingWaypointName = true;
            waypointNameInput.clear();
            waypointNameInput.append(w->name);
            waypointNameInput.selectAll();
            screen::clearMessage();
        }
        return;
    case ShapeZone::Field:
        if (!right && hit.part != 2 && hit.index >= 0 && hit.index < static_cast<int>(map::waypointFields.size())
            && map::waypointFields[static_cast<size_t>(hit.index)] == map::WaypointField::Color) {
            waypointFieldSelected = hit.index;
            int count = static_cast<int>(map::waypointColors.size());
            if (int swatch = waypointsDisplayed.swatchAt(x, count); swatch >= 0)
                changeSelected([&](map::Waypoint& t) { t.color = swatch; });
            return;
        }
        activateWaypointField(hit.index, right ? -1 : hit.part);
        return;
    case ShapeZone::Bulk: {
        auto const actions = bulkActions(SelectionList::Waypoints);
        int at = hit.index;
        if (severalSelected() && at >= 0 && at < static_cast<int>(actions.size()) && waypointBulk.press(actions[static_cast<size_t>(at)]))
            bulkWaypoints(actions[static_cast<size_t>(at)]);
        return;
    }
    case ShapeZone::Action:
        if (hit.index == 0) return;
        if (!waypointSelected) return;
        // A temporary waypoint goes on the first click (L-261).
        if (auto const* w = selectedWaypoint(); !waypointDeleteArmed && !(w && w->temporary)) { waypointDeleteArmed = true; return; }
        deleteSelectedWaypoint();
        return;
    default: return;
    }
}
void handleWaypointKey(int key) {
    if (editingWaypointName) {
        switch (key) {
        case 0x08: if (waypointNameInput.backspace()) waypointNameDirty = true; break;
        case 0x41: if (screen::heldCtrl()) waypointNameInput.selectAll(); break;
        case 0x1b: screen::cancelEditing(); break;
        case 0x0d: case 0x09: screen::finishEditing(); break;
        }
        return;
    }
    if (editingWaypointField >= 0) {
        switch (key) {
        case 0x08: if (screen::number().backspace()) screen::numberTyped(); break;
        case 0x41: if (screen::heldCtrl()) screen::number().selectAll(); break;
        case 0x1b: screen::cancelEditing(); break;
        case 0x0d: case 0x09: screen::finishEditing(); break;
        }
        return;
    }
    switch (selectionKey(key, screen::heldCtrl(), severalSelected())) {
    case SelectionKey::SelectAll:
        waypointRows.selectAll(waypointOrder());
        syncSelected();
        waypointFieldFirst = 0;
        waypointFieldSelected = -1;
        waypointDeleteArmed = false;
        waypointBulk.disarm();
        return;
    case SelectionKey::Clear: selectWaypoint(std::nullopt); return;
    case SelectionKey::None: break;
    }
    switch (key) {
    case 0x1b: if (fromMapFlag) screen::returnToMap(); else screen::close(); break;
    case 0x26: moveWaypointField(-1); break;
    case 0x28: moveWaypointField(1); break;
    case 0x25: activateWaypointField(waypointFieldSelected, -1); break;
    case 0x27: activateWaypointField(waypointFieldSelected, 1); break;
    case 0x0d: case 0x20: activateWaypointField(waypointFieldSelected, 0); break;
    case 0x21: case 0x22: {
        int count = waypointRowCount();
        if (!count) break;
        int row = 0;
        for (int i = 0; i < count; ++i) if (waypointSelected && waypointAtRow(i) == waypointSelected) row = i;
        row = std::clamp(row + (key == 0x22 ? 1 : -1), 0, count - 1);
        selectWaypoint(waypointAtRow(row));
        break;
    }
    case 0x09: screen::nextNav(screen::heldShift()); break;
    }
}
Rgb waypointRgb(int color) {
    auto c = map::waypointColors[static_cast<size_t>(map::clampColor(color))];
    return {map::channel(c, 0) / 255.f, map::channel(c, 1) / 255.f, map::channel(c, 2) / 255.f};
}
// Small pixel glyphs for list rows and the editor.
void drawDiamondGlyph(MinecraftUIRenderContext& context, float cx, float cy, int size, Rgb color, float opacity = 1) {
    auto rows = map::diamondRows(size);
    float top = cy - static_cast<float>(rows.size()) / 2;
    for (size_t i = 0; i < rows.size(); ++i)
        fill(context, cx - rows[i] - .5f, top + i, 2.f * rows[i] + 1, 1, color, opacity);
}
// A death's waypoint (L-328): a cross of `reach` units each way, `thick` wide.
void drawCrossGlyph(MinecraftUIRenderContext& context, float cx, float cy, Rgb color, int reach = 2, float thick = 1,
                    float opacity = 1) {
    for (int i = -reach; i <= reach; ++i) {
        fill(context, cx + i - thick / 2, cy + i - thick / 2, thick, thick, color, opacity);
        fill(context, cx + i - thick / 2, cy - i - thick / 2, thick, thick, color, opacity);
    }
}
std::string waypointDescription() {
    if (severalSelected()) return translated("list.selectedCount", waypointRows.keys.size());
    auto const* w = selectedWaypoint();
    if (!w) return translated(waypointSet.waypoints.empty() ? "waypoint.empty" : "waypoint.selectHint");
    if (editingWaypointField >= 0) return translated("integerRange", -map::coordinateLimit, map::coordinateLimit);
    if (w->dimension != screen::playerDimension()) return w->name + ": " + translated("waypoint.elsewhere");
    return w->name + ": " + translated(w->visible ? "waypoint.shownState" : "waypoint.hiddenState");
}
void drawWaypointsBody(MinecraftUIRenderContext& context, ShapesLayout const& l, glm::vec2 pointer) {
    if (l.noticeHeight > 0)
        data_notice::draw(context, l.listLeft + ShapesLayout::pad, l.noticeTop + 2, l.listWidth - 2 * ShapesLayout::pad,
            data_notice::Kind::Waypoints, pointer);
    else data_notice::hide();
    auto const preferences = Runtime::instance().preferences();
    auto hover = l.hit(pointer.x, pointer.y);
    auto over = [&](ShapeZone zone, int index = -1) { return hover.zone == zone && (index < 0 || hover.index == index); };
    drawToolHeader(context,l,translated("waypoint.showAll"),preferences.map.waypoints,waypointsDocked,over(ShapeZone::DrawAll),over(ShapeZone::Keys),over(ShapeZone::Dock));

    // List pane.
    drawButton(context,l.listLeft+ShapesLayout::pad,l.toolbarTop+2,ShapesLayout::newWidth,12,translated("waypoint.addHere"),
        over(ShapeZone::NewShape),ButtonKind::Primary);
    fill(context,l.listLeft,l.theadTop-1,l.listWidth,1,palette::white,.14f);
    auto const columns = listColumns(l.listLeft, l.listWidth, 0, switchWidth);
    drawListHeadings(context,columns,l.theadTop);
    fill(context,l.listLeft,l.rowsTop-1,l.listWidth,1,palette::white,.14f);
    if (l.listCount == 0)
        paragraph(context,l.listLeft+ShapesLayout::pad,l.rowsTop+3,l.listWidth-2*ShapesLayout::pad,translated("waypoint.empty"),3,palette::faint);
    for (int i = l.listFirst; i < l.listFirst + l.listVisible && i < l.listCount; ++i) {
        float y = l.listRowY(i);
        auto value = waypointAtRow(i);
        if (i % 2) fill(context,l.listLeft+1,y,l.rowWidth(),ShapesLayout::rowHeight,palette::white,.025f);
        rowBackground(context,l.listLeft+1,y,l.rowWidth(),ShapesLayout::rowHeight,value && waypointRows.contains(*value),over(ShapeZone::ListRow,i));
        float gx = columns.markX + 4, gy = y + ShapesLayout::rowHeight / 2;
        int index = value ? indexOfId(waypointSet.waypoints, value->id) : -1;
        if (index < 0) continue;
        auto const& w = waypointSet.waypoints[static_cast<size_t>(index)];
        bool here = w.dimension == screen::playerDimension();
        if (w.death) drawCrossGlyph(context, gx, gy, waypointRgb(w.color), 2, 1, w.visible && here ? 1.f : .35f);
        else {
            drawDiamondGlyph(context, gx, gy, 7, waypointRgb(w.color), w.visible && here ? 1.f : .35f);
            if (w.temporary) drawDiamondGlyph(context, gx, gy, map::hollowSize(7), palette::shade, .85f); // hollow: temporary (L-261)
        }
        label(context,columns.nameX,y+3,columns.nameWidth,w.name,here ? palette::text : palette::faint);
        if (columns.hasDistance())
            label(context,columns.distanceX,y+3,columns.distanceWidth,listDistanceText(w.x, w.y, w.z, w.dimension),palette::dim,Align::Right);
        toggleSwitch(context,columns.shownX,y+(ShapesLayout::rowHeight-switchHeight)/2,w.visible);
    }
    drawListScrollbar(context,l,screen::scrollHeld(&waypointListFirst) || l.onScrollbar(pointer.x,pointer.y));
    if (l.docked) fill(context,l.left,l.detailTop-1,l.width,1,palette::white,.14f);
    else fill(context,l.detailLeft-1,l.toolbarTop,1,l.footerTop-l.toolbarTop,palette::white,.14f);

    // Editor pane.
    float dx = l.detailLeft + ShapesLayout::pad, dw = l.detailWidth - 2 * ShapesLayout::pad;
    auto const* w = selectedWaypoint();
    if (severalSelected()) {
        auto const chosen = waypointRows.inOrder(waypointOrder());
        auto const pane = l.selectionPane(static_cast<int>(chosen.size()));
        std::vector<SelectedName> names;
        std::vector<map::Waypoint const*> marks;
        size_t shown = 0;
        for (auto const& key : chosen) {
            int index = indexOfId(waypointSet.waypoints, key.id);
            if (index < 0) continue;
            auto const& c = waypointSet.waypoints[static_cast<size_t>(index)];
            if (c.visible) ++shown;
            if (names.size() >= static_cast<size_t>(pane.fit.names)) continue;
            names.push_back({c.name, c.dimension != screen::playerDimension(), listDistanceText(c.x, c.y, c.z, c.dimension)});
            marks.push_back(&c);
        }
        drawSelectionPane(context,pane,chosen.size(),names,[&](size_t i, float gx, float gy) {
            auto const& c = *marks[i];
            float opacity = c.visible && c.dimension == screen::playerDimension() ? 1.f : .35f;
            if (c.death) drawCrossGlyph(context, gx, gy, waypointRgb(c.color), 2, 1, opacity);
            else {
                drawDiamondGlyph(context, gx, gy, 7, waypointRgb(c.color), opacity);
                if (c.temporary) drawDiamondGlyph(context, gx, gy, map::hollowSize(7), palette::shade, .85f);
            }
        },l.selecting,waypointBulk,pointer.x,pointer.y,shown);
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
    } else if (!w) {
        paragraph(context,dx,l.detailTop+6,dw,translated(waypointSet.waypoints.empty() ? "waypoint.empty"
            : "waypoint.selectHint"),3,palette::faint);
    } else {
        // Name field with the waypoint's diamond.
        drawTextField(context,dx,l.nameY,dw,ShapesLayout::rowHeight-1,editingWaypointName,&waypointNameInput,w->name,{},17);
        if (w->death) drawCrossGlyph(context, dx + 7, l.nameY + 6.5f, waypointRgb(w->color));
        else {
            drawDiamondGlyph(context, dx + 7, l.nameY + 6.5f, 7, waypointRgb(w->color));
            if (w->temporary) drawDiamondGlyph(context, dx + 7, l.nameY + 6.5f, map::hollowSize(7), palette::shade, .85f);
        }
        // The preview area: a large diamond and where the waypoint is.
        float size = ShapesLayout::previewSize;
        fill(context,dx,l.previewY,size,size,palette::shade,.35f);
        frame(context,dx,l.previewY,size,size,palette::white,.14f);
        if (w->death) {
            drawCrossGlyph(context, dx + size / 2, l.previewY + size / 2, palette::shade, 10, 6, .8f);
            drawCrossGlyph(context, dx + size / 2, l.previewY + size / 2, waypointRgb(w->color), 9, 3);
        } else {
            drawDiamondGlyph(context, dx + size / 2, l.previewY + size / 2, 31, palette::shade, .8f);
            drawDiamondGlyph(context, dx + size / 2, l.previewY + size / 2, 25, waypointRgb(w->color));
            if (w->temporary) drawDiamondGlyph(context, dx + size / 2, l.previewY + size / 2, map::hollowSize(25), palette::shade, .85f);
        }
        float ix = dx + size + 8, iw = dw - size - 8;
        label(context,ix,l.previewY+1,iw,std::format("{}, {}, {}", w->x, w->y, w->z));
        label(context,ix,l.previewY+12,iw,screen::dimensionName(w->dimension),palette::dim);
        if (w->dimension == screen::playerDimension()) {
            int const away = screen::distanceTo(w->x, w->y, w->z);
            label(context,ix,l.previewY+23,iw,translated("waypoint.distance", away),palette::dim);
        }
        if (w->temporary) paragraph(context,ix,l.previewY+36,iw,translated("waypoint.temporaryNote"),2,palette::faint);
        // Fields.
        for (int i = l.fieldFirst; i < l.fieldFirst + l.fieldVisible && i < static_cast<int>(map::waypointFields.size()); ++i) {
            float y = l.fieldY(i);
            auto field = map::waypointFields[static_cast<size_t>(i)];
            rowBackground(context,l.detailLeft+1,y,l.detailWidth-2,ShapesLayout::rowHeight,waypointFieldSelected == i,over(ShapeZone::Field,i));
            static constexpr std::array<std::string_view, 7> labels{"X", "Y", "Z", "waypoint.moveHere", "waypoint.shown",
                                                                   "waypoint.temporary", "waypoint.color"};
            std::string text = i < 3 ? std::string(labels[static_cast<size_t>(i)]) : translated(labels[static_cast<size_t>(i)]);
            label(context,dx,y+3,l.stepperX()-dx-4,text,palette::dim);
            switch (field) {
            case map::WaypointField::Visible:
                toggleSwitch(context,l.stepperX()+l.stepperWidth()-switchWidth,y+(ShapesLayout::rowHeight-switchHeight)/2,w->visible);
                break;
            case map::WaypointField::Temporary:
                toggleSwitch(context,l.stepperX()+l.stepperWidth()-switchWidth,y+(ShapesLayout::rowHeight-switchHeight)/2,w->temporary);
                break;
            case map::WaypointField::MoveHere:
                drawButton(context,l.stepperX(),y+1,l.stepperWidth(),ShapesLayout::rowHeight-2,translated("waypoint.moveHere"),
                    over(ShapeZone::Field,i));
                break;
            case map::WaypointField::Color:
                drawSwatchRow(context,l,y,static_cast<int>(map::waypointColors.size()),map::clampColor(w->color),
                    [](int c) { return waypointRgb(c); });
                break;
            default: {
                int value = i == 0 ? w->x : i == 1 ? w->y : w->z;
                drawShapeStepper(context,l,y,true,std::to_string(value),editingWaypointField == i ? &screen::number() : nullptr,pointer.x,pointer.y);
                break;
            }
            }
        }
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
    }
    if (waypointSelected) {
        drawDangerButton(context,l.deleteX(),l.actionsY+2,ShapesLayout::deleteWidth,12,
            armedLabel(context,translated("shape.delete"),confirmDelete(),waypointDeleteArmed,ShapesLayout::deleteWidth),over(ShapeZone::Action,1), waypointDeleteArmed);
    }

    // Footer.
    fill(context,l.left,l.footerTop,l.width,1,palette::white,.14f);
    float textLeft = l.left + ShapesLayout::pad, available = l.width - 2 * ShapesLayout::pad;
    bool shortFooter = l.docked;
    std::string text = !screen::message().empty() ? screen::message() : waypointDescription();
    if (shortFooter) label(context,textLeft,l.footerTop+3,available,std::move(text),screen::message().empty() ? palette::text : screen::messageColor());
    else {
        paragraph(context,textLeft,l.footerTop+3,available,text,2,screen::message().empty() ? palette::text : screen::messageColor());
        label(context,textLeft,l.footerTop+30,available,translated(editingWaypointName || editingWaypointField >= 0
            ? "shape.numberHint" : "waypoint.screenHint"),palette::faint);
    }
}
ShapesLayout fitWaypoints(SettingsTable const& t, glm::vec2 size, bool docked) {
    int fieldCount = selectedWaypoint() ? static_cast<int>(map::waypointFields.size()) : 0;
    auto l = ShapesLayout::fit(t, size.x, size.y, docked, waypointRowCount(), waypointListFirst, fieldCount,
        waypointFieldFirst, false, false, false, data_notice::height(data_notice::Kind::Waypoints));
    if (severalSelected()) l.selecting = bulkActions(SelectionList::Waypoints);
    waypointsDisplayed = l;
    waypointListFirst = l.listFirst;
    waypointFieldFirst = l.fieldFirst;
    return l;
}
void renderWaypointsContent(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer, SettingsTable const& t) {
    auto l = fitWaypoints(t, size, false);
    if (l.usable()) drawWaypointsBody(context, l, pointer);
    context.flushText(0,std::nullopt);
}
void renderWaypointsDocked(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) {
    auto l = fitWaypoints(screen::table(), size, true);
    if (l.width <= 0) {
        label(context, 4, 4, std::max(1.0f, size.x - 8), translated("smallWindow"));
        context.flushText(0, std::nullopt);
        return;
    }
    drawDockedFrame(context,l,translated("nav.waypoints"),translated(fromMapFlag ? "worldMap.back" : "closeButton"),
        l.hit(pointer.x, pointer.y).zone == ShapeZone::Close);
    if (l.usable()) drawWaypointsBody(context, l, pointer);
    else drawDockedTooShort(context, l, pointer.x, pointer.y);
    context.flushText(0,std::nullopt);
}

}

void refresh() { refreshWaypoints(); }
void select(std::optional<map::MarkKey> mark) { selectWaypoint(mark); }
void click(float x, float y, bool right) { handleWaypointClick(x, y, right); }
void key(int key) { handleWaypointKey(key); }
void wheel(int step, glm::vec2 pointer) {
    auto const& l = waypointsDisplayed;
    bool overList = pointer.x >= l.listLeft && pointer.x < l.listLeft + l.listWidth && (!l.docked || pointer.y < l.detailTop);
    if (overList) waypointListFirst = std::max(0, waypointListFirst + step);
    else waypointFieldFirst = std::max(0, waypointFieldFirst + step);
}
bool editingName() { return editingWaypointName; }
TextEdit* nameField() { return editingWaypointName ? &waypointNameInput : nullptr; }
void nameEdited() { waypointNameDirty = true; }
bool editingNumber() { return editingWaypointField >= 0; }
void type(std::string const& text) { if (waypointNameInput.type(text)) waypointNameDirty = true; }
void applyNumber() {
    if (editingWaypointField < 0) return;
    auto parsed = screen::number().parsedPrecise(-map::coordinateLimit, map::coordinateLimit, true);
    if (!parsed) { screen::warnRange(translated("integerRange", -map::coordinateLimit, map::coordinateLimit)); return; }
    int field = editingWaypointField, value = static_cast<int>(*parsed);
    auto const* w = selectedWaypoint();
    if (!w || (field == 0 ? w->x : field == 1 ? w->y : w->z) == value) { screen::clearMessage(); return; }
    changeSelected([&](map::Waypoint& t) { (field == 0 ? t.x : field == 1 ? t.y : t.z) = value; });
}
void applyName() { applyWaypointName(); }
void endEditing() { editingWaypointField = -1; editingWaypointName = false; waypointNameDirty = false; }
glm::vec2 caret() {
    return {waypointsDisplayed.stepperX(), editingWaypointName ? waypointsDisplayed.nameY
        : waypointsDisplayed.fieldY(std::max(0, editingWaypointField))};
}
bool docked() { return waypointsDocked; }
void renderDocked(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) { renderWaypointsDocked(context, size, pointer); }
void renderContent(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer, SettingsTable const& table) {
    renderWaypointsContent(context, size, pointer, table);
}
void reset() {
    endEditing();
    if (waypointRows.clearSeveral()) syncSelected(); // a closed screen keeps one row, not several
    waypointBulk.disarm();
    waypointDeleteArmed = false;
    fromMapFlag = false;
}
void setFromMap(bool fromMap) { fromMapFlag = fromMap; }
bool fromMap() { return fromMapFlag; }
}

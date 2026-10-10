#include "ui/WaypointsView.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
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
    double x = 0, z = 0;
    if (auto* player = screen::client() ? screen::client()->getLocalPlayer() : nullptr) {
        auto feet = player->getFeetPos();
        x = feet.x;
        z = feet.z;
    }
    waypointList = map::waypointOrder(waypointSet.waypoints, screen::playerDimension(), x, z);
    if (waypointSelected && (waypointSelected->layer == map::MarkLayer::Death ? !waypointSet.death
                                 : indexOfId(waypointSet.waypoints, waypointSelected->id) < 0))
        waypointSelected.reset();
}
int waypointRowCount() { return static_cast<int>(waypointList.size()) + (waypointSet.death ? 1 : 0); }
// The selection a list row stands for: the death point first, if any.
std::optional<map::MarkKey> waypointAtRow(int row) {
    if (waypointSet.death) {
        if (row == 0) return map::deathKey;
        --row;
    }
    if (row < 0 || row >= static_cast<int>(waypointList.size())) return std::nullopt;
    return map::waypointKey(waypointSet.waypoints[waypointList[static_cast<size_t>(row)]].id);
}
bool deathSelected() { return waypointSelected == map::deathKey; }
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
    waypointSelected = value;
    waypointFieldFirst = 0;
    waypointFieldSelected = -1;
    waypointDeleteArmed = false;
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
            c = std::clamp(c + direction, -map::coordinateLimit, map::coordinateLimit);
        });
        return;
    }
    case map::WaypointField::MoveHere:
        if (auto place = waypointPlace())
            changeSelected([&](map::Waypoint& t) { t.x = place->x; t.y = place->y; t.z = place->z; t.dimension = place->dimension; });
        return;
    case map::WaypointField::Visible: changeSelected([](map::Waypoint& t) { t.visible = !t.visible; }); return;
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
        if (selection->layer == map::MarkLayer::Death) { set.death.reset(); return true; }
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
void keepDeathPoint() {
    if (!waypointSet.death) return;
    auto death = *waypointSet.death;
    map::Waypoint w{translated("waypoint.deathName"), map::nextColor(waypointSet.lastColor), death.x, death.y, death.z,
                    death.dimension, true};
    bool saved = map::waypoints::change([&](map::WaypointSet& set) {
        set.waypoints.push_back(w);
        set.lastColor = w.color;
        set.death.reset();
        return true;
    });
    if (!saved) { screen::setMessage(translated("waypoint.saveError")); return; }
    screen::clearMessage();
    refreshWaypoints();
    if (!waypointSet.waypoints.empty()) selectWaypoint(map::waypointKey(waypointSet.waypoints.back().id));
}
void handleWaypointClick(float x, float y, bool right) {
    screen::finishEditing();
    if (!right && screen::pressScrollbar(waypointsDisplayed, waypointListFirst, x, y)) return;
    if (!waypointsDocked && screen::navClick(x, y)) return;
    auto hit = waypointsDisplayed.hit(x, y);
    if (!(hit.zone == ShapeZone::Action && hit.index == 1)) waypointDeleteArmed = false;
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
        if (value && value->layer == map::MarkLayer::Waypoint && x >= l.listLeft + l.listWidth - ShapesLayout::pad - switchWidth - 2) {
            if (!changeWaypoint(value, [](map::Waypoint& t) { t.visible = !t.visible; })) screen::setMessage(translated("waypoint.saveError"));
            else screen::clearMessage();
            refreshWaypoints();
            return;
        }
        if (value && value != waypointSelected) selectWaypoint(value);
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
    case ShapeZone::Action:
        if (hit.index == 0) { if (deathSelected()) keepDeathPoint(); return; }
        if (!waypointSelected) return;
        if (!waypointDeleteArmed) { waypointDeleteArmed = true; return; }
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
        case 0x1b: case 0x0d: case 0x09: screen::finishEditing(); break;
        }
        return;
    }
    if (editingWaypointField >= 0) {
        switch (key) {
        case 0x08: if (screen::number().backspace()) screen::numberTyped(); break;
        case 0x41: if (screen::heldCtrl()) screen::number().selectAll(); break;
        case 0x1b: case 0x0d: case 0x09: screen::finishEditing(); break;
        }
        return;
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
constexpr Rgb deathRgb{230 / 255.f, 46 / 255.f, 46 / 255.f};
// Small pixel glyphs for list rows and the editor.
void drawDiamondGlyph(MinecraftUIRenderContext& context, float cx, float cy, int size, Rgb color, float opacity = 1) {
    auto rows = map::diamondRows(size);
    float top = cy - static_cast<float>(rows.size()) / 2;
    for (size_t i = 0; i < rows.size(); ++i)
        fill(context, cx - rows[i] - .5f, top + i, 2.f * rows[i] + 1, 1, color, opacity);
}
void drawCrossGlyph(MinecraftUIRenderContext& context, float cx, float cy, Rgb color) {
    for (int i = -2; i <= 2; ++i) {
        fill(context, cx + i - .5f, cy + i - .5f, 1, 1, color);
        fill(context, cx + i - .5f, cy - i - .5f, 1, 1, color);
    }
}
std::string waypointDescription() {
    if (deathSelected() && waypointSet.death) return translated("waypoint.deathNote");
    auto const* w = selectedWaypoint();
    if (!w) return translated(waypointSet.waypoints.empty() && !waypointSet.death ? "waypoint.empty" : "waypoint.selectHint");
    if (editingWaypointField >= 0) return translated("integerRange", -map::coordinateLimit, map::coordinateLimit);
    if (w->dimension != screen::playerDimension()) return w->name + ": " + translated("waypoint.elsewhere");
    return w->name + ": " + translated(w->visible ? "waypoint.shownState" : "waypoint.hiddenState");
}
void drawWaypointsBody(MinecraftUIRenderContext& context, ShapesLayout const& l, glm::vec2 pointer) {
    auto const preferences = Runtime::instance().preferences();
    auto hover = l.hit(pointer.x, pointer.y);
    auto over = [&](ShapeZone zone, int index = -1) { return hover.zone == zone && (index < 0 || hover.index == index); };
    float top = l.top + 4;
    label(context,l.drawAllX,l.drawAllY+1+boxTextInset(),l.drawAllWidth-switchWidth-4,translated("waypoint.showAll"),
        over(ShapeZone::DrawAll) ? palette::text : palette::dim,Align::Right);
    toggleSwitch(context,l.drawAllX+l.drawAllWidth-switchWidth,l.drawAllY+2,preferences.map.waypoints);
    label(context,l.keysX,top+1+boxTextInset(),ShapesLayout::keysWidth,translated("shape.keys"),
        over(ShapeZone::Keys) ? palette::text : palette::accent,Align::Center);
    fill(context,l.keysX+6,top+11,ShapesLayout::keysWidth-12,1,palette::accent,over(ShapeZone::Keys) ? 1.f : .5f);
    drawSmallButton(context,l.dockX,top,ShapesLayout::dockWidth,12,translated(waypointsDocked ? "shape.undock" : "shape.dock"),over(ShapeZone::Dock));

    // List pane.
    float listRight = l.listLeft + l.listWidth;
    drawSmallButton(context,l.listLeft+ShapesLayout::pad,l.toolbarTop+2,ShapesLayout::newWidth,12,translated("waypoint.addHere"),
        over(ShapeZone::NewShape),palette::accentDeep,palette::accent);
    fill(context,l.listLeft,l.theadTop-1,l.listWidth,1,palette::white,.14f);
    float nameX = l.listLeft + ShapesLayout::pad + 14;
    float shownX = listRight - ShapesLayout::pad - switchWidth - 2;
    float distanceX = shownX - 56;
    label(context,nameX,l.theadTop+2,distanceX-nameX-4,translated("shape.columnName"),palette::faint);
    label(context,distanceX,l.theadTop+2,52,translated("waypoint.columnDistance"),palette::faint,Align::Right);
    label(context,shownX-6,l.theadTop+2,switchWidth+12,translated("shape.columnShown"),palette::faint,Align::Center);
    fill(context,l.listLeft,l.rowsTop-1,l.listWidth,1,palette::white,.14f);
    if (l.listCount == 0)
        paragraph(context,l.listLeft+ShapesLayout::pad,l.rowsTop+3,l.listWidth-2*ShapesLayout::pad,translated("waypoint.empty"),3,palette::faint);
    for (int i = l.listFirst; i < l.listFirst + l.listVisible && i < l.listCount; ++i) {
        float y = l.listRowY(i);
        auto value = waypointAtRow(i);
        if (i % 2) fill(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,palette::white,.025f);
        rowBackground(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,value && value == waypointSelected,over(ShapeZone::ListRow,i));
        float gx = l.listLeft + ShapesLayout::pad + 4, gy = y + ShapesLayout::rowHeight / 2;
        if (value == map::deathKey) {
            auto const& d = *waypointSet.death;
            bool here = d.dimension == screen::playerDimension();
            drawCrossGlyph(context, gx, gy, deathRgb);
            label(context,nameX,y+3,distanceX-nameX-4,translated("waypoint.death"),here ? palette::text : palette::faint);
            label(context,distanceX,y+3,52,here ? translated("waypoint.meters", screen::distanceTo(d.x, d.z)) : screen::dimensionName(d.dimension),
                palette::dim,Align::Right);
            continue;
        }
        int index = value ? indexOfId(waypointSet.waypoints, value->id) : -1;
        if (index < 0) continue;
        auto const& w = waypointSet.waypoints[static_cast<size_t>(index)];
        bool here = w.dimension == screen::playerDimension();
        drawDiamondGlyph(context, gx, gy, 7, waypointRgb(w.color), w.visible && here ? 1.f : .35f);
        label(context,nameX,y+3,distanceX-nameX-4,w.name,here ? palette::text : palette::faint);
        label(context,distanceX,y+3,52,here ? translated("waypoint.meters", screen::distanceTo(w.x, w.z)) : screen::dimensionName(w.dimension),
            palette::dim,Align::Right);
        toggleSwitch(context,shownX,y+(ShapesLayout::rowHeight-switchHeight)/2,w.visible);
    }
    drawListScrollbar(context,l);
    if (l.docked) fill(context,l.left,l.detailTop-1,l.width,1,palette::white,.14f);
    else fill(context,l.detailLeft-1,l.toolbarTop,1,l.footerTop-l.toolbarTop,palette::white,.14f);

    // Editor pane.
    float dx = l.detailLeft + ShapesLayout::pad, dw = l.detailWidth - 2 * ShapesLayout::pad;
    auto const* w = selectedWaypoint();
    if (deathSelected() && waypointSet.death) {
        auto const& d = *waypointSet.death;
        drawCrossGlyph(context, dx + 5, l.nameY + 7, deathRgb);
        label(context,dx+14,l.nameY+1+boxTextInset(),dw-14,translated("waypoint.death"));
        label(context,dx,l.previewY+1,dw,std::format("{}, {}, {}", d.x, d.y, d.z));
        label(context,dx,l.previewY+12,dw,screen::dimensionName(d.dimension),palette::dim);
        if (d.dimension == screen::playerDimension())
            label(context,dx,l.previewY+23,dw,translated("waypoint.distance", screen::distanceTo(d.x, d.z)),palette::dim);
        paragraph(context,dx,l.previewY+36,dw,translated("waypoint.deathNote"),3,palette::faint);
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
        drawSmallButton(context,l.actionX(0),l.actionsY+2,l.firstActionWidth,12,translated("waypoint.keep"),
            over(ShapeZone::Action,0),palette::accentDeep,palette::accent);
    } else if (!w) {
        paragraph(context,dx,l.detailTop+6,dw,translated(waypointSet.waypoints.empty() && !waypointSet.death ? "waypoint.empty"
            : "waypoint.selectHint"),3,palette::faint);
    } else {
        // Name field with the waypoint's diamond.
        fill(context,dx,l.nameY,dw,ShapesLayout::rowHeight-1,Rgb{0,0,0},.4f);
        frame(context,dx,l.nameY,dw,ShapesLayout::rowHeight-1,editingWaypointName ? palette::accent : palette::keyEdge);
        drawDiamondGlyph(context, dx + 7, l.nameY + 6.5f, 7, waypointRgb(w->color));
        if (editingWaypointName) drawEditText(context,dx+17,l.nameY,ShapesLayout::rowHeight-1,dw-20,waypointNameInput);
        else label(context,dx+17,l.nameY+1+boxTextInset(),dw-20,w->name);
        // The preview area: a large diamond and where the waypoint is.
        float size = ShapesLayout::previewSize;
        fill(context,dx,l.previewY,size,size,Rgb{0,0,0},.35f);
        frame(context,dx,l.previewY,size,size,palette::white,.14f);
        drawDiamondGlyph(context, dx + size / 2, l.previewY + size / 2, 31, Rgb{0,0,0}, .8f);
        drawDiamondGlyph(context, dx + size / 2, l.previewY + size / 2, 25, waypointRgb(w->color));
        float ix = dx + size + 8, iw = dw - size - 8;
        label(context,ix,l.previewY+1,iw,std::format("{}, {}, {}", w->x, w->y, w->z));
        label(context,ix,l.previewY+12,iw,screen::dimensionName(w->dimension),palette::dim);
        if (w->dimension == screen::playerDimension())
            label(context,ix,l.previewY+23,iw,translated("waypoint.distance", screen::distanceTo(w->x, w->z)),palette::dim);
        // Fields.
        for (int i = l.fieldFirst; i < l.fieldFirst + l.fieldVisible && i < static_cast<int>(map::waypointFields.size()); ++i) {
            float y = l.fieldY(i);
            auto field = map::waypointFields[static_cast<size_t>(i)];
            rowBackground(context,l.detailLeft+1,y,l.detailWidth-2,ShapesLayout::rowHeight,waypointFieldSelected == i,over(ShapeZone::Field,i));
            static constexpr std::array<std::string_view, 6> labels{"X", "Y", "Z", "waypoint.moveHere", "waypoint.shown", "waypoint.color"};
            std::string text = i < 3 ? std::string(labels[static_cast<size_t>(i)]) : translated(labels[static_cast<size_t>(i)]);
            label(context,dx,y+3,l.stepperX()-dx-4,text,palette::dim);
            switch (field) {
            case map::WaypointField::Visible:
                toggleSwitch(context,l.stepperX()+l.stepperWidth()-switchWidth,y+(ShapesLayout::rowHeight-switchHeight)/2,w->visible);
                break;
            case map::WaypointField::MoveHere:
                drawSmallButton(context,l.stepperX(),y+1,l.stepperWidth(),ShapesLayout::rowHeight-2,translated("waypoint.moveHere"),
                    over(ShapeZone::Field,i));
                break;
            case map::WaypointField::Color:
                drawSwatchRow(context,l,y,static_cast<int>(map::waypointColors.size()),map::clampColor(w->color),
                    [](int c) { return waypointRgb(c); });
                break;
            default: {
                int value = i == 0 ? w->x : i == 1 ? w->y : w->z;
                drawShapeStepper(context,l,y,true,std::to_string(value),editingWaypointField == i ? &screen::number() : nullptr);
                break;
            }
            }
        }
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
    }
    if (waypointSelected) {
        drawSmallButton(context,l.deleteX(),l.actionsY+2,ShapesLayout::deleteWidth,12,
            translated(waypointDeleteArmed ? "shape.deleteConfirm" : "shape.delete"),over(ShapeZone::Action,1),
            waypointDeleteArmed ? Rgb{.54f,.18f,.16f} : palette::keyFill,Rgb{.54f,.23f,.2f},
            waypointDeleteArmed ? palette::text : Rgb{1.f,.7f,.68f});
    }

    // Footer.
    fill(context,l.left,l.footerTop,l.width,1,palette::white,.14f);
    float textLeft = l.left + ShapesLayout::pad, available = l.width - 2 * ShapesLayout::pad;
    bool shortFooter = l.docked || screen::table().shortFooter;
    std::string text = !screen::message().empty() ? screen::message() : waypointDescription();
    if (shortFooter) label(context,textLeft,l.footerTop+3,available,std::move(text),screen::message().empty() ? palette::text : palette::warning);
    else {
        paragraph(context,textLeft,l.footerTop+3,available,text,2,screen::message().empty() ? palette::text : palette::warning);
        label(context,textLeft,l.footerTop+30,available,translated(editingWaypointName || editingWaypointField >= 0
            ? "shape.numberHint" : "waypoint.screenHint"),palette::faint);
    }
}
ShapesLayout fitWaypoints(SettingsTable const& t, glm::vec2 size, bool docked) {
    int fieldCount = selectedWaypoint() ? static_cast<int>(map::waypointFields.size()) : 0;
    auto l = ShapesLayout::fit(t, size.x, size.y, docked, waypointRowCount(), waypointListFirst, fieldCount,
        waypointFieldFirst, false, false);
    // Keeping the death point needs a wider first button.
    if (deathSelected()) l.firstActionWidth = 110;
    waypointsDisplayed = l;
    waypointListFirst = l.listFirst;
    waypointFieldFirst = l.fieldFirst;
    return l;
}
void renderWaypointsContent(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer, SettingsTable const& t) {
    auto l = fitWaypoints(t, size, false);
    if (l.width > 0) drawWaypointsBody(context, l, pointer);
    context.flushText(0,std::nullopt);
}
void renderWaypointsDocked(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) {
    auto l = fitWaypoints(screen::table(), size, true);
    if (l.width <= 0) {
        label(context, 4, 4, std::max(1.0f, size.x - 8), translated("smallWindow"));
        context.flushText(0, std::nullopt);
        return;
    }
    panel(context,l.left,l.top,l.width,l.height,.82f);
    frame(context,l.left,l.top,l.width,l.height,palette::white,.14f);
    label(context,l.left+ShapesLayout::pad,l.top+6,l.drawAllX-l.left-10,translated("nav.waypoints"));
    bool closeHover = l.hit(pointer.x, pointer.y).zone == ShapeZone::Close;
    drawSmallButton(context,l.closeX,l.top+4,ShapesLayout::closeWidth,12,translated(fromMapFlag ? "worldMap.back" : "closeButton"),
        closeHover,palette::keyFill,palette::keyEdge,closeHover ? palette::text : palette::dim);
    fill(context,l.left,l.top+ShapesLayout::headerHeight-1,l.width,1,palette::white,.14f);
    drawWaypointsBody(context, l, pointer);
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
    waypointDeleteArmed = false;
    fromMapFlag = false;
}
void setFromMap(bool fromMap) { fromMapFlag = fromMap; }
bool fromMap() { return fromMapFlag; }
}

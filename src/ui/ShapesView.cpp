#include "ui/ShapesView.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/RowSelection.h"
#include "ui/ScreenParts.h"
#include "ui/SearchQuery.h"
#include "ui/ShapeEditor.h"
#include "ui/ShapesLayout.h"
#include "overlay/ShapeSession.h"
#include "features/camera/CameraSessions.h"
#include "features/camera/ViewFeet.h"
#include "app/Runtime.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/world/phys/HitResult.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <vector>

namespace lamium::ui::shapes_view {
namespace {
// Shapes view. The collection lives in the overlay session; this is only what
// the editor shows: the selected shape or an unsaved draft, and scroll state.
using ShapeZone = ShapesLayout::Zone;
bool shapesDocked = false;
std::optional<overlay::ShapeId> shapeSelected; // shapeRows' row while it is alone, which the editor works on
// What the list has selected (L-190); the draft row never takes part.
RowSelection<overlay::ShapeId> shapeRows;
BulkArm shapeBulk;
void chooseShape(std::optional<overlay::ShapeId> id) {
    if (id) shapeRows.only(*id);
    else shapeRows.clear();
    shapeSelected = id;
    shapeBulk.disarm();
}
std::optional<overlay::ShapeDefinition> shapeDraft;
bool shapePicking = false, shapeDeleteArmed = false;
int shapeListFirst = 0, shapeFieldFirst = 0, shapeFieldSelected = -1, shapeLayer = 0;
shape::Reference shapeReference = shape::Reference::StandingBlock;
std::vector<overlay::shapes::Summary> shapeList;
ShapesLayout shapesDisplayed;
int editingShapeField = -1;
bool editingShapeName = false;
SearchQuery shapeNameInput;
bool shapeNameDirty = false;
std::string shapeNameOrigin; // the name before typing, for Esc (names apply as typed)
// Applies an edited definition: a draft only updates its world preview; an
// existing shape is saved through the session, reporting failures in the footer.
void applyShape(overlay::ShapeDefinition definition) {
    try {
        if (shapeDraft) { overlay::shapes::setDraft(definition); shapeDraft = std::move(definition); }
        else if (shapeSelected) overlay::shapes::edit(*shapeSelected, std::move(definition));
        screen::clearMessage();
    } catch (overlay::ShapeSaveError const&) { screen::setMessage(translated("shape.saveError")); }
    catch (std::exception const&) { screen::setMessage(translated("shape.editError")); }
}
std::optional<overlay::ShapeDefinition> currentShape() {
    if (shapeDraft) return shapeDraft;
    if (shapeSelected) return overlay::shapes::find(*shapeSelected);
    return {};
}
void applyShapeName() {
    if (!editingShapeName || !std::exchange(shapeNameDirty, false)) return;
    try {
        if (shapeDraft) {
            auto definition = *shapeDraft;
            definition.name = shapeNameInput.value();
            overlay::ShapeCollection{}.add(definition); // Validates the name only.
            shapeDraft = std::move(definition);
            overlay::shapes::setDraft(shapeDraft);
        } else if (shapeSelected) overlay::shapes::rename(*shapeSelected, shapeNameInput.value());
        screen::clearMessage();
    }
    catch (overlay::ShapeSaveError const&) { screen::setMessage(translated("shape.saveError")); }
    catch (std::exception const&) { screen::setMessage(translated("shape.nameError")); }
}
// ---- Shapes view ----
// Where the drawn view stands: the body's feet, or the camera's virtual feet
// while Freelook or FreeCamera detaches it (L-248). The schematic placement
// actions use the same rule, so a shape lands where the crosshair is drawn.
std::optional<overlay::Point> viewPoint() {
    auto* client = screen::client();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return {};
    auto ray = CameraSessions::instance().detachedViewRay(*client);
    std::optional<camera::Position> detached;
    if (ray) detached = camera::Position{ray->x, ray->y, ray->z};
    auto eye = player->getEyePos();
    auto body = player->getFeetPos();
    auto point = camera::viewFeet(detached, {body.x, body.y, body.z}, {eye.x, eye.y, eye.z});
    if (!point) return {};
    return overlay::Point{point->x, point->y, point->z};
}
// Reference point for new or moved shapes, and the reference actually used: a
// missing target falls back to the standing block with a notice. A targeted
// block is taken before the view point is asked for, so a frame without a
// usable pose does not drop the block the crosshair picked.
std::optional<std::pair<overlay::Point, shape::Reference>> referencePoint() {
    if (shapeReference == shape::Reference::TargetBlock) {
        if (auto* client = screen::client()) {
            auto const& hit = client->getLatestHitResult();
            if (hit.mType == HitResultType::Tile)
                return std::pair{overlay::Point{hit.mBlock.x + .5, hit.mBlock.y + .5, hit.mBlock.z + .5}, shapeReference};
        }
        auto point = viewPoint();
        if (!point) return {};
        screen::setMessage(translated("shape.targetMissing"));
        return std::pair{*point, shape::Reference::StandingBlock};
    }
    auto point = viewPoint();
    if (!point) return {};
    return std::pair{*point, shapeReference};
}
overlay::Point centerOf(overlay::ShapeDefinition const& definition) {
    if (auto spec = std::get_if<overlay::ShapeSpec>(&definition.geometry)) return spec->center;
    auto const& origin = std::get<overlay::PlaneSpec>(definition.geometry).origin;
    return {double(origin.x), double(origin.y), double(origin.z)};
}
void resetShapeEditor() { shapeFieldFirst = 0; shapeFieldSelected = -1; shapeLayer = 0; shapeDeleteArmed = false; }
void cancelDraft() {
    if (shapeDraft) { shapeDraft.reset(); overlay::shapes::setDraft({}); }
    shapePicking = false;
}
void beginDraft(int type) {
    auto point = referencePoint();
    if (!point) return;
    overlay::ShapeDefinition definition;
    definition.name = translated(shape::types[type].name);
    definition.dimension = screen::playerDimension();
    shapeDraft = shape::withType(std::move(definition), type, point->first, point->second);
    shapePicking = false;
    chooseShape(std::nullopt);
    resetShapeEditor();
    try { overlay::shapes::setDraft(shapeDraft); }
    catch (std::exception const&) { screen::setMessage(translated("shape.editError")); }
}
void createDraft() {
    if (!shapeDraft) return;
    try {
        auto id = overlay::shapes::add(*shapeDraft);
        cancelDraft();
        chooseShape(id);
        resetShapeEditor();
        screen::clearMessage();
    } catch (overlay::ShapeSaveError const&) { screen::setMessage(translated("shape.saveError")); }
    catch (std::exception const&) { screen::setMessage(translated("shape.editError")); }
}
void selectShape(std::optional<overlay::ShapeId> id) {
    cancelDraft();
    chooseShape(id);
    resetShapeEditor();
}
std::vector<overlay::ShapeId> shapeOrder() {
    std::vector<overlay::ShapeId> order;
    order.reserve(shapeList.size());
    for (auto const& item : shapeList) order.push_back(item.id);
    return order;
}
// Ctrl + click toggles a row, Shift + click takes a range, Ctrl + A takes all.
// A draft or type choice ends first.
void pickShapes(overlay::ShapeId id) {
    cancelDraft();
    auto before = shapeSelected;
    shapeRows.click(id, screen::heldCtrl(), screen::heldShift(), shapeOrder());
    shapeSelected = shapeRows.single();
    shapeBulk.disarm();
    if (shapeSelected != before) resetShapeEditor();
    shapeDeleteArmed = false;
}
void bulkShapes(BulkAction action) {
    try {
        if (action == BulkAction::Delete) overlay::shapes::remove(shapeRows.keys);
        else overlay::shapes::setVisible(shapeRows.keys, action == BulkAction::Show);
        screen::clearMessage();
    } catch (overlay::ShapeSaveError const&) { screen::setMessage(translated("shape.saveError")); return; }
    catch (std::exception const&) { screen::setMessage(translated("shape.editError")); return; }
    if (action == BulkAction::Delete) selectShape(std::nullopt);
    shapeList = overlay::shapes::list();
}
// Click or key activation of an editor row. part: -1/1 step, 0 value, 2 label.
void activateShapeField(int index, int part) {
    auto definition = currentShape();
    if (!definition) return;
    auto fields = shape::rows(*definition, shapeDraft.has_value());
    if (index < 0 || index >= static_cast<int>(fields.size()) || fields[index].kind == shape::Row::Kind::Group) return;
    shapeFieldSelected = index;
    if (part == 2) return;
    int direction = part == -1 ? -1 : 1;
    auto const& row = fields[index];
    switch (row.field) {
    case shape::Field::Type: {
        if (!shapeDraft) return;
        int count = static_cast<int>(shape::types.size());
        int old = shape::typeIndex(*shapeDraft), type = (old + direction + count) % count;
        auto next = shape::withType(*shapeDraft, type, centerOf(*shapeDraft), shapeReference);
        if (next.name == translated(shape::types[old].name)) next.name = translated(shape::types[type].name);
        applyShape(std::move(next));
        return;
    }
    case shape::Field::Reference: {
        shapeReference = static_cast<shape::Reference>((static_cast<int>(shapeReference) + direction + 3) % 3);
        if (shapeDraft) if (auto point = referencePoint()) {
            auto moved = *shapeDraft;
            shape::place(moved, point->first, point->second);
            applyShape(std::move(moved));
        }
        return;
    }
    case shape::Field::Follow: {
        // Off keeps the shape where it was last drawn: the body's block, not a
        // detached camera's. From another dimension, where the player left its
        // own; the saved place stands only if the player was not there this session.
        auto* player = screen::client() ? screen::client()->getLocalPlayer() : nullptr;
        std::optional<overlay::Cell> feet = overlay::shapes::lastFeet(definition->dimension);
        if (player && definition->dimension == screen::playerDimension()) {
            auto const body = player->getFeetPos();
            feet = overlay::Cell{static_cast<int>(std::floor(body.x)), static_cast<int>(std::floor(body.y)),
                                 static_cast<int>(std::floor(body.z))};
        }
        if (definition->follow && feet) { applyShape(overlay::settledAt(*definition, *feet)); return; }
        break;
    }
    case shape::Field::MoveHere: {
        if (auto point = referencePoint()) {
            auto moved = *definition;
            shape::place(moved, point->first, point->second);
            moved.dimension = screen::playerDimension();
            applyShape(std::move(moved));
        }
        return;
    }
    default: break;
    }
    if (auto range = shape::numeric(*definition, row.field); range && part == 0) {
        editingShapeField = index;
        screen::number().beginPrecise(range->value);
        screen::clearMessage();
        return;
    }
    // Shift steps a number ten of its own steps; a choice steps once.
    int const times = shape::numeric(*definition, row.field) && screen::heldShift() ? 10 : 1;
    auto stepped = *definition;
    for (int i = 0; i < times; ++i) stepped = shape::adjust(stepped, row.field, direction);
    applyShape(std::move(stepped));
}
void moveShapeField(int step) {
    auto definition = currentShape();
    if (!definition) return;
    auto fields = shape::rows(*definition, shapeDraft.has_value());
    int index = shapeFieldSelected;
    for (size_t tries = 0; tries < fields.size(); ++tries) {
        index = std::clamp(index + step, 0, static_cast<int>(fields.size()) - 1);
        if (fields[index].kind != shape::Row::Kind::Group) break;
        if (index == 0 || index == static_cast<int>(fields.size()) - 1) step = -step;
    }
    shapeFieldSelected = index;
    int visible = shapesDisplayed.fieldVisible;
    if (visible > 0) {
        if (index < shapeFieldFirst) shapeFieldFirst = index;
        if (index >= shapeFieldFirst + visible) shapeFieldFirst = index - visible + 1;
    }
}
void openShapeKeySettings() {
    // Show the Shape rendering row, expanded, in its category.
    cancelDraft();
    screen::showFeatureKeys("shapes");
}
void handleShapeClick(float x, float y, bool right) {
    screen::finishEditing();
    // A right click steps a stepper down and presses nothing else (L-346).
    if (right) {
        auto hit = shapesDisplayed.hit(x, y);
        if (hit.zone == ShapeZone::Field) activateShapeField(hit.index, -1);
        else if (hit.zone == ShapeZone::LayerDown || hit.zone == ShapeZone::LayerUp) --shapeLayer;
        return;
    }
    if (!right && screen::pressScrollbar(shapesDisplayed, shapeListFirst, x, y)) return;
    if (!shapesDocked && screen::navClick(x, y)) return;
    auto hit = shapesDisplayed.hit(x, y);
    if (!(hit.zone == ShapeZone::Action && hit.index == 1 && !shapeDraft)) shapeDeleteArmed = false;
    if (hit.zone != ShapeZone::Bulk) shapeBulk.disarm();
    switch (hit.zone) {
    case ShapeZone::Close: screen::close(); return;
    case ShapeZone::Dock: shapesDocked = !shapesDocked; return;
    case ShapeZone::Keys: openShapeKeySettings(); return;
    case ShapeZone::DrawAll:
        screen::toggleOption("overlays.shapes");
        return;
    case ShapeZone::NewShape:
        cancelDraft();
        chooseShape(std::nullopt);
        shapePicking = true;
        resetShapeEditor();
        return;
    case ShapeZone::ListRow: {
        int index = hit.index - (shapeDraft ? 1 : 0);
        if (index < 0 || index >= static_cast<int>(shapeList.size())) return;
        auto const& item = shapeList[index];
        auto const& l = shapesDisplayed;
        if (listColumns(l.listLeft, l.listWidth, 0, switchWidth).onShown(x)) {
            try { overlay::shapes::setVisible(item.id, !item.definition.visible); screen::clearMessage(); }
            catch (overlay::ShapeSaveError const&) { screen::setMessage(translated("shape.saveError")); }
            catch (std::exception const&) { screen::setMessage(translated("shape.editError")); }
            return;
        }
        if (screen::heldCtrl() || screen::heldShift()) { pickShapes(item.id); return; }
        if (shapeSelected != item.id || shapeDraft || shapeRows.several()) selectShape(item.id);
        return;
    }
    case ShapeZone::Bulk: {
        auto const actions = bulkActions(SelectionList::Shapes);
        int at = hit.index;
        if (shapeRows.several() && at >= 0 && at < static_cast<int>(actions.size()) && shapeBulk.press(actions[static_cast<size_t>(at)]))
            bulkShapes(actions[static_cast<size_t>(at)]);
        return;
    }
    case ShapeZone::Name:
        if (auto definition = currentShape()) {
            editingShapeName = true;
            shapeNameOrigin = definition->name;
            shapeNameInput.clear();
            shapeNameInput.append(definition->name);
            shapeNameInput.selectAll();
            screen::clearMessage();
        }
        return;
    case ShapeZone::LayerDown: shapeLayer -= screen::heldShift() ? 10 : 1; return;
    case ShapeZone::LayerUp: shapeLayer += screen::heldShift() ? 10 : 1; return;
    case ShapeZone::Field:
        if (!right && hit.part != 2) if (auto definition = currentShape()) {
            auto fields = shape::rows(*definition, shapeDraft.has_value());
            if (hit.index >= 0 && hit.index < static_cast<int>(fields.size()) && fields[hit.index].field == shape::Field::Color) {
                shapeFieldSelected = hit.index;
                if (int swatch = shapesDisplayed.swatchAt(x, 4); swatch >= 0) {
                    definition->color = static_cast<overlay::ShapeColor>(swatch);
                    applyShape(std::move(*definition));
                }
                return;
            }
        }
        activateShapeField(hit.index, right ? -1 : hit.part);
        return;
    case ShapeZone::Pick: beginDraft(hit.index); return;
    case ShapeZone::Action:
        if (shapeDraft) { if (hit.index == 0) createDraft(); else cancelDraft(); return; }
        if (!shapeSelected) return;
        if (hit.index == 0) {
            if (auto definition = currentShape()) {
                definition->name = translated("shape.copyName", definition->name);
                if (definition->name.size() > 128) definition->name.resize(128);
                try { selectShape(overlay::shapes::add(*definition)); screen::clearMessage(); }
                catch (overlay::ShapeSaveError const&) { screen::setMessage(translated("shape.saveError")); }
                catch (std::exception const&) { screen::setMessage(translated("shape.editError")); }
            }
            return;
        }
        if (!shapeDeleteArmed) { shapeDeleteArmed = true; return; }
        try {
            size_t position = 0;
            for (; position < shapeList.size() && shapeList[position].id != *shapeSelected; ++position) {}
            overlay::shapes::remove(*shapeSelected);
            auto remaining = overlay::shapes::list();
            selectShape(remaining.empty() ? std::nullopt
                : std::optional(remaining[std::min(position, remaining.size() - 1)].id));
            screen::clearMessage();
        } catch (overlay::ShapeSaveError const&) { screen::setMessage(translated("shape.saveError")); }
        catch (std::exception const&) { screen::setMessage(translated("shape.editError")); }
        return;
    default: return;
    }
}
void handleShapeKey(int key) {
    if (editingShapeName) {
        switch (key) {
        case 0x08: if (shapeNameInput.backspace()) shapeNameDirty = true; break;
        case 0x41: if (screen::heldCtrl()) shapeNameInput.selectAll(); break;
        case 0x1b: screen::cancelEditing(); break;
        case 0x0d: case 0x09: screen::finishEditing(); break;
        }
        return;
    }
    if (editingShapeField >= 0) {
        switch (key) {
        case 0x08: if (screen::number().backspace()) screen::numberTyped(); break;
        case 0x41: if (screen::heldCtrl()) screen::number().selectAll(); break;
        case 0x1b: screen::cancelEditing(); break;
        case 0x0d: case 0x09: screen::finishEditing(); break;
        }
        return;
    }
    // Esc ends a draft or type choice first, then several rows, then closes;
    // Ctrl + A never drops a draft.
    if (!shapePicking && !shapeDraft)
        switch (selectionKey(key, screen::heldCtrl(), shapeRows.several())) {
        case SelectionKey::SelectAll:
            shapeRows.selectAll(shapeOrder());
            if (shapeRows.single() != shapeSelected) resetShapeEditor();
            shapeSelected = shapeRows.single();
            shapeDeleteArmed = false;
            shapeBulk.disarm();
            return;
        case SelectionKey::Clear: selectShape(std::nullopt); return;
        case SelectionKey::None: break;
        }
    switch (key) {
    case 0x1b:
        if (shapePicking || shapeDraft) cancelDraft();
        else screen::close();
        break;
    case 0x26: moveShapeField(-1); break;
    case 0x28: moveShapeField(1); break;
    case 0x25: activateShapeField(shapeFieldSelected, -1); break;
    case 0x27: activateShapeField(shapeFieldSelected, 1); break;
    case 0x0d: case 0x20: activateShapeField(shapeFieldSelected, 0); break;
    case 0x21: case 0x22: {
        // Previous / next shape in the list.
        if (shapeList.empty()) break;
        int index = 0;
        for (size_t i = 0; i < shapeList.size(); ++i) if (shapeSelected == shapeList[i].id) index = static_cast<int>(i);
        index = std::clamp(index + (key == 0x22 ? 1 : -1), 0, static_cast<int>(shapeList.size()) - 1);
        selectShape(shapeList[index].id);
        break;
    }
    case 0x09: screen::nextNav(screen::heldShift()); break;
    }
}

// Preview is regenerated only when the geometry or layer changes.
struct PreviewCache {
    std::optional<overlay::ShapeDefinition> definition;
    int layer = 0;
    std::optional<shape::Preview> preview;
} previewCache;
bool sameGeometry(overlay::ShapeDefinition const& a, overlay::ShapeDefinition const& b) {
    auto sa = std::get_if<overlay::ShapeSpec>(&a.geometry), sb = std::get_if<overlay::ShapeSpec>(&b.geometry);
    if (sa && sb) return sa->shape == sb->shape && sa->center == sb->center && sa->snap == sb->snap
        && sa->radius == sb->radius && sa->height == sb->height;
    auto pa = std::get_if<overlay::PlaneSpec>(&a.geometry), pb = std::get_if<overlay::PlaneSpec>(&b.geometry);
    return pa && pb && pa->origin == pb->origin && pa->width == pb->width && pa->depth == pb->depth
        && pa->spacing == pb->spacing && pa->plane == pb->plane;
}
shape::Preview const* previewFor(overlay::ShapeDefinition const& definition) {
    if (!previewCache.definition || !sameGeometry(*previewCache.definition, definition) || previewCache.layer != shapeLayer) {
        previewCache.definition = definition;
        previewCache.layer = shapeLayer;
        try { previewCache.preview = shape::preview(definition, shapeLayer); }
        catch (std::exception const&) { previewCache.preview.reset(); }
        if (previewCache.preview) {
            // Keep the shown layer inside the shape, then rebuild once for it.
            int layer = std::clamp(shapeLayer, previewCache.preview->lowLayer, previewCache.preview->highLayer);
            if (layer != shapeLayer) {
                shapeLayer = previewCache.layer = layer;
                try { previewCache.preview = shape::preview(definition, shapeLayer); }
                catch (std::exception const&) { previewCache.preview.reset(); }
            }
        }
    }
    return previewCache.preview ? &*previewCache.preview : nullptr;
}
Rgb shapeRgb(overlay::ShapeColor color) {
    switch (color) {
    case overlay::ShapeColor::Yellow: return palette::shapeYellow;
    case overlay::ShapeColor::Pink: return palette::shapePink;
    case overlay::ShapeColor::White: return palette::shapeWhite;
    default: return palette::shapeCyan;
    }
}
// The distance column (L-190): from the player to the shape's center or a
// plane's origin, as the Waypoints list measures; a following shape is
// drawn around the player, so it has none.
std::string shapeDistanceText(overlay::ShapeDefinition const& definition) {
    if (definition.follow && definition.dimension == screen::playerDimension()) return "-";
    if (auto spec = std::get_if<overlay::ShapeSpec>(&definition.geometry))
        return listDistanceText(static_cast<int>(std::floor(spec->center.x)), static_cast<int>(std::floor(spec->center.z)), definition.dimension);
    auto const& origin = std::get<overlay::PlaneSpec>(definition.geometry).origin;
    return listDistanceText(origin.x, origin.z, definition.dimension);
}
void drawTypeIcon(MinecraftUIRenderContext& context, float x, float y, int type, Rgb color) {
    auto pattern = shape::typeGlyphs[static_cast<size_t>(std::clamp(type, 0, static_cast<int>(shape::typeGlyphs.size()) - 1))];
    for (int i = 0; i < 25 && i < static_cast<int>(pattern.size()); ++i)
        if (pattern[i] == '#') fill(context, x + (i % 5) * 2, y + (i / 5) * 2, 2, 2, color);
}
std::string shapeDescription(std::optional<overlay::ShapeDefinition> const& definition) {
    if (shapePicking) return translated("shape.pickType");
    if (shapeRows.several()) return translated("list.selectedCount", shapeRows.keys.size());
    if (!definition) return translated(shapeList.empty() ? "shape.empty" : "shape.selectHint");
    if (shapeDraft) return translated("shape.draftNote");
    if (definition->dimension != screen::playerDimension()) return definition->name + ": " + translated("shape.elsewhere");
    if (editingShapeField >= 0) {
        auto fields = shape::rows(*definition, false);
        if (editingShapeField < static_cast<int>(fields.size()))
            if (auto range = shape::numeric(*definition, fields[editingShapeField].field))
                return translated(range->integer ? "integerRange" : "numberRange", range->minimum, range->maximum);
    }
    return definition->name + ": " + translated(definition->visible ? "shape.shownState" : "shape.hiddenState");
}
// List, editor, header controls and footer of the Shapes view.
void drawShapesBody(MinecraftUIRenderContext& context, ShapesLayout const& l, glm::vec2 pointer,
                    std::optional<overlay::ShapeDefinition> const& definition) {
    auto const preferences = Runtime::instance().preferences();
    auto hover = l.hit(pointer.x, pointer.y);
    auto over = [&](ShapeZone zone, int index = -1) { return hover.zone == zone && (index < 0 || hover.index == index); };
    // Header controls.
    drawToolHeader(context,l,translated("shape.drawAll"),preferences.overlays.shapes,shapesDocked,over(ShapeZone::DrawAll),over(ShapeZone::Keys),over(ShapeZone::Dock));

    // List pane.
    drawButton(context,l.listLeft+ShapesLayout::pad,l.toolbarTop+2,ShapesLayout::newWidth,12,translated("shape.new"),
        over(ShapeZone::NewShape) || shapePicking,ButtonKind::Primary); // lit while picking a type
    fill(context,l.listLeft,l.theadTop-1,l.listWidth,1,palette::white,.14f);
    auto const columns = listColumns(l.listLeft, l.listWidth, 0, switchWidth);
    drawListHeadings(context,columns,l.theadTop);
    fill(context,l.listLeft,l.rowsTop-1,l.listWidth,1,palette::white,.14f);
    int draftOffset = shapeDraft ? 1 : 0;
    if (l.listCount == 0)
        paragraph(context,l.listLeft+ShapesLayout::pad,l.rowsTop+3,l.listWidth-2*ShapesLayout::pad,translated("shape.empty"),3,palette::faint);
    for (int i = l.listFirst; i < l.listFirst + l.listVisible && i < l.listCount; ++i) {
        float y = l.listRowY(i);
        bool isDraft = shapeDraft && i == 0;
        auto const* item = isDraft ? nullptr : &shapeList[i - draftOffset];
        auto const& shown = isDraft ? *shapeDraft : item->definition;
        bool chosen = isDraft || (!shapeDraft && item && shapeRows.contains(item->id));
        if (i % 2) fill(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,palette::white,.025f);
        rowBackground(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,chosen && !isDraft,over(ShapeZone::ListRow,i));
        if (isDraft) frame(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,palette::info);
        bool elsewhere = shown.dimension != screen::playerDimension();
        // The same type glyph and color as the opened shape; dimmed when hidden or elsewhere.
        Rgb glyph = isDraft ? palette::info : shapeRgb(shown.color);
        if (!shown.visible || elsewhere) glyph = {glyph.r * .4f, glyph.g * .4f, glyph.b * .4f};
        drawTypeIcon(context,columns.markX,y+2,shape::typeIndex(shown),glyph);
        std::string suffix = isDraft ? translated("shape.draftTag") : "";
        float suffixWidth = suffix.empty() ? 0 : textWidth(context, suffix) + 6;
        float nameEnd = columns.nameX + columns.nameWidth;
        label(context,columns.nameX,y+3,columns.nameWidth-suffixWidth,shown.name,isDraft ? palette::info : elsewhere ? palette::faint : palette::text);
        if (!suffix.empty()) label(context,nameEnd-suffixWidth+2,y+3,suffixWidth,suffix,palette::info);
        if (columns.hasDistance()) label(context,columns.distanceX,y+3,columns.distanceWidth,shapeDistanceText(shown),palette::dim,Align::Right);
        if (!isDraft) toggleSwitch(context,columns.shownX,y+(ShapesLayout::rowHeight-switchHeight)/2,shown.visible);
    }
    drawListScrollbar(context,l,screen::scrollDragging() || l.onScrollbar(pointer.x,pointer.y));
    // Divider between list and editor.
    if (l.docked) fill(context,l.left,l.detailTop-1,l.width,1,palette::white,.14f);
    else fill(context,l.detailLeft-1,l.toolbarTop,1,l.footerTop-l.toolbarTop,palette::white,.14f);

    // Editor pane.
    float dx = l.detailLeft + ShapesLayout::pad, dw = l.detailWidth - 2 * ShapesLayout::pad;
    if (shapePicking) {
        label(context,dx,l.nameY+2,dw,translated("shape.group.basic"),palette::accent);
        for (int i = l.fieldFirst; i < l.fieldFirst + l.fieldVisible && i < static_cast<int>(shape::types.size()); ++i) {
            float y = l.fieldY(i);
            if (over(ShapeZone::Pick, i)) fill(context,l.detailLeft+1,y,l.detailWidth-2,ShapesLayout::pickHeight,palette::white,.07f);
            drawTypeIcon(context,dx+1,y+6,i,palette::dim);
            label(context,dx+16,y+2,dw-16,translated(shape::types[i].name));
            label(context,dx+16,y+11,dw-16,translated(shape::types[i].description),palette::faint);
        }
    } else if (!l.selecting.empty()) {
        auto const chosen = shapeRows.inOrder(shapeOrder());
        auto const pane = l.selectionPane(static_cast<int>(chosen.size()));
        std::vector<SelectedName> names;
        std::vector<overlay::ShapeDefinition const*> marks;
        size_t shown = 0;
        for (auto const& item : shapeList) {
            if (!shapeRows.contains(item.id)) continue;
            if (item.definition.visible) ++shown;
            if (names.size() >= static_cast<size_t>(pane.fit.names)) continue;
            names.push_back({item.definition.name, item.definition.dimension != screen::playerDimension(), shapeDistanceText(item.definition)});
            marks.push_back(&item.definition);
        }
        drawSelectionPane(context,pane,chosen.size(),names,[&](size_t i, float gx, float gy) {
            auto const& d = *marks[i];
            Rgb glyph = shapeRgb(d.color);
            if (!d.visible || d.dimension != screen::playerDimension()) glyph = {glyph.r * .4f, glyph.g * .4f, glyph.b * .4f};
            drawTypeIcon(context,gx-4,gy-5,shape::typeIndex(d),glyph);
        },l.selecting,shapeBulk,pointer.x,pointer.y,shown);
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
    } else if (!definition) {
        paragraph(context,dx,l.detailTop+6,dw,translated(shapeList.empty() ? "shape.empty" : "shape.selectHint"),3,palette::faint);
    } else {
        if (shapeDraft) label(context,dx,l.detailTop+4,dw,translated("shape.draftNote"),palette::info);
        // Name field.
        drawTextField(context,dx,l.nameY,dw,ShapesLayout::rowHeight-1,editingShapeName,&shapeNameInput,definition->name,{},17);
        drawTypeIcon(context,dx+3,l.nameY+1.5f,shape::typeIndex(*definition),shapeDraft ? palette::info : shapeRgb(definition->color));
        // Preview: one layer seen from above; plane seen along its normal.
        float px = dx, py = l.previewY, size = ShapesLayout::previewSize;
        fill(context,px,py,size,size,palette::shade,.35f);
        frame(context,px,py,size,size,palette::white,.14f);
        auto const* preview = previewFor(*definition);
        if (preview) {
            float spanU = float(preview->maxU - preview->minU + 1), spanV = float(preview->maxV - preview->minV + 1);
            float scale = std::min((size - 4) / spanU, (size - 4) / spanV);
            float ox = px + (size - spanU * scale) / 2 - preview->minU * scale;
            float oy = py + (size - spanV * scale) / 2 - preview->minV * scale;
            Rgb color = shapeDraft ? palette::info : shapeRgb(definition->color);
            for (auto const& run : preview->runs)
                fill(context,ox + run.u * scale,oy + run.v * scale,std::max(1.0f, run.length * scale),std::max(1.0f, scale),color,.9f);
            fill(context,ox,oy,std::max(1.0f, scale),std::max(1.0f, scale),palette::accent);
        }
        float ix = px + size + 8, iw = dw - size - 8;
        label(context,ix,py+1,iw,translated(shape::types[shape::typeIndex(*definition)].name));
        if (preview) label(context,ix,py+12,iw,translated("shape.cellCount",preview->cells),palette::dim);
        if (auto* player = screen::client() ? screen::client()->getLocalPlayer() : nullptr) {
            auto feet = player->getFeetPos();
            auto center = centerOf(*definition);
            int distance = static_cast<int>(std::round(std::hypot(center.x - feet.x, center.z - feet.z)));
            label(context,ix,py+22,iw,distance == 0 ? translated("distance.here") : translated("shape.distance",distance),palette::dim);
        }
        if (preview && preview->lowLayer != preview->highLayer) {
            float lx = l.layerStepperX(), ly = l.layerY(), lw = 64, aw = ShapesLayout::arrowWidth;
            drawStepper(context,lx,ly+1,lw,ShapesLayout::rowHeight-2,true,
                translated("shape.layer",(shapeLayer >= 0 ? "+" : "") + std::to_string(shapeLayer)),
                {.cell = aw, .pointerX = pointer.x, .pointerY = pointer.y});
        }
        paragraph(context,ix,py+45,iw,translated("shape.previewHint"),1,palette::faint);
        // Fields.
        auto fields = shape::rows(*definition, shapeDraft.has_value());
        for (int i = l.fieldFirst; i < l.fieldFirst + l.fieldVisible && i < static_cast<int>(fields.size()); ++i) {
            float y = l.fieldY(i);
            auto const& row = fields[i];
            if (row.kind == shape::Row::Kind::Group) {
                label(context,dx,y+4,dw,translated(row.label),palette::faint);
                continue;
            }
            rowBackground(context,l.detailLeft+1,y,l.detailWidth-2,ShapesLayout::rowHeight,shapeFieldSelected == i,over(ShapeZone::Field,i));
            label(context,dx,y+3,l.stepperX()-dx-4,translated(row.label),palette::dim);
            switch (row.kind) {
            case shape::Row::Kind::Switch:
                toggleSwitch(context,l.stepperX()+l.stepperWidth()-switchWidth,y+(ShapesLayout::rowHeight-switchHeight)/2,
                    row.field == shape::Field::Follow ? definition->follow : definition->visible);
                break;
            case shape::Row::Kind::Button:
                drawButton(context,l.stepperX(),y+1,l.stepperWidth(),ShapesLayout::rowHeight-2,translated(row.label),over(ShapeZone::Field,i));
                break;
            default: {
                if (row.field == shape::Field::Color) {
                    drawSwatchRow(context,l,y,4,static_cast<int>(definition->color),
                        [](int c) { return shapeRgb(static_cast<overlay::ShapeColor>(c)); });
                    break;
                }
                auto range = shape::numeric(*definition, row.field);
                std::string value;
                if (range) {
                    value = range->integer ? std::to_string(static_cast<long long>(range->value)) : shape::numberText(range->value);
                    if (row.field == shape::Field::X || row.field == shape::Field::Y || row.field == shape::Field::Z)
                        value = range->integer ? value : std::format("{:.3f}", range->value);
                    else value = translated("shape.blocks", value);
                } else value = translated(shape::choiceLabel(*definition, row.field, shapeReference));
                drawShapeStepper(context,l,y,range.has_value(),std::move(value),editingShapeField == i ? &screen::number() : nullptr,pointer.x,pointer.y);
                break;
            }
            }
        }
        // Actions.
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
        if (shapeDraft) {
            drawButton(context,l.actionX(0),l.actionsY+2,ShapesLayout::actionWidth,12,translated("shape.create"),
                over(ShapeZone::Action,0),ButtonKind::Primary);
            drawButton(context,l.actionX(1),l.actionsY+2,ShapesLayout::actionWidth,12,translated("shape.cancel"),over(ShapeZone::Action,1));
        } else {
            drawButton(context,l.actionX(0),l.actionsY+2,ShapesLayout::actionWidth,12,translated("shape.duplicateShort"),over(ShapeZone::Action,0));
            drawDangerButton(context,l.deleteX(),l.actionsY+2,ShapesLayout::deleteWidth,12,
                againLabel(translated("shape.delete"), shapeDeleteArmed),over(ShapeZone::Action,1), shapeDeleteArmed);
        }
    }

    // Footer.
    fill(context,l.left,l.footerTop,l.width,1,palette::white,.14f);
    float textLeft = l.left + ShapesLayout::pad, available = l.width - 2 * ShapesLayout::pad;
    bool shortFooter = l.docked;
    // Simple graphics draw faces poorly (additive, order dependent); say so
    // rather than tuning that mode further.
    bool simple = screen::client() && screen::client()->getOptions().getGraphicsMode() == GraphicsMode::Simple;
    bool warn = !screen::message().empty() || simple;
    std::string text = !screen::message().empty() ? screen::message() : simple && shortFooter ? translated("shape.simpleWarning") : shapeDescription(definition);
    if (shortFooter) label(context,textLeft,l.footerTop+3,available,std::move(text),
        !screen::message().empty() ? screen::messageColor() : warn ? palette::warning : palette::text);
    else {
        paragraph(context,textLeft,l.footerTop+3,available,text,2,screen::message().empty() ? palette::text : screen::messageColor());
        label(context,textLeft,l.footerTop+30,available,translated(simple ? "shape.simpleWarning"
            : editingShapeName || editingShapeField >= 0 ? "shape.numberHint" : "shape.hint"),simple ? palette::warning : palette::faint);
    }
}
std::optional<overlay::ShapeDefinition> visibleShape() {
    auto definition = currentShape();
    if (!definition && shapeSelected) { // Removed elsewhere.
        shapeRows.retain([](overlay::ShapeId id) { return overlay::shapes::find(id).has_value(); });
        shapeSelected = shapeRows.single();
        definition = currentShape();
    }
    return definition;
}
void renderShapesContent(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer,
                         SettingsTable const& t) {
    auto definition = visibleShape();
    int fieldCount = shapePicking ? static_cast<int>(shape::types.size())
        : definition ? static_cast<int>(shape::rows(*definition, shapeDraft.has_value()).size()) : 0;
    auto l = ShapesLayout::fit(t, size.x, size.y, false, static_cast<int>(shapeList.size()) + (shapeDraft ? 1 : 0),
        shapeListFirst, fieldCount, shapeFieldFirst, shapeDraft.has_value(), shapePicking);
    if (shapeRows.several() && !shapeDraft && !shapePicking) l.selecting = bulkActions(SelectionList::Shapes);
    shapesDisplayed = l;
    shapeListFirst = l.listFirst; shapeFieldFirst = l.fieldFirst;
    if (l.usable()) drawShapesBody(context, l, pointer, definition);
    context.flushText(0,std::nullopt);
}
void renderShapesDocked(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) {
    auto definition = visibleShape();
    int fieldCount = shapePicking ? static_cast<int>(shape::types.size())
        : definition ? static_cast<int>(shape::rows(*definition, shapeDraft.has_value()).size()) : 0;
    auto l = ShapesLayout::fit(screen::table(), size.x, size.y, true, static_cast<int>(shapeList.size()) + (shapeDraft ? 1 : 0),
        shapeListFirst, fieldCount, shapeFieldFirst, shapeDraft.has_value(), shapePicking);
    if (shapeRows.several() && !shapeDraft && !shapePicking) l.selecting = bulkActions(SelectionList::Shapes);
    shapesDisplayed = l;
    shapeListFirst = l.listFirst; shapeFieldFirst = l.fieldFirst;
    if (l.width <= 0) {
        label(context, 4, 4, std::max(1.0f, size.x - 8), translated("smallWindow"));
        context.flushText(0, std::nullopt);
        return;
    }
    // Docked: the world stays visible; only the panel is drawn.
    drawDockedFrame(context,l,translated("nav.shapes"),translated("closeButton"),l.hit(pointer.x, pointer.y).zone == ShapeZone::Close);
    if (l.usable()) drawShapesBody(context, l, pointer, definition);
    else drawDockedTooShort(context, l, pointer.x, pointer.y);
    context.flushText(0,std::nullopt);
}

}

void refresh() {
    shapeList = overlay::shapes::list();
    shapeRows.retain([](overlay::ShapeId id) {
        return std::any_of(shapeList.begin(), shapeList.end(), [&](auto const& item) { return item.id == id; });
    });
    if (!shapeDraft) shapeSelected = shapeRows.single();
}
void click(float x, float y, bool right) { handleShapeClick(x, y, right); }
void key(int key) { handleShapeKey(key); }
void select(overlay::ShapeId id) {
    if (overlay::shapes::find(id)) selectShape(id);
}
void wheel(int step, glm::vec2 pointer) {
    auto const& l = shapesDisplayed;
    bool overList = pointer.x >= l.listLeft && pointer.x < l.listLeft + l.listWidth && (!l.docked || pointer.y < l.detailTop);
    if (overList) shapeListFirst = std::max(0, shapeListFirst + step);
    else shapeFieldFirst = std::max(0, shapeFieldFirst + step);
}
bool editingName() { return editingShapeName; }
TextEdit* nameField() { return editingShapeName ? &shapeNameInput : nullptr; }
void nameEdited() { shapeNameDirty = true; }
bool editingNumber() { return editingShapeField >= 0; }
void type(std::string const& text) { if (shapeNameInput.type(text)) shapeNameDirty = true; }
void applyNumber() {
    if (editingShapeField < 0) return;
    auto definition = currentShape();
    auto fields = definition ? shape::rows(*definition, shapeDraft.has_value()) : std::vector<shape::Row>{};
    if (!definition || editingShapeField >= static_cast<int>(fields.size())) { editingShapeField = -1; return; }
    auto field = fields[editingShapeField].field;
    auto range = shape::numeric(*definition, field);
    if (!range) { editingShapeField = -1; return; }
    auto parsed = screen::number().parsedPrecise(range->minimum,range->maximum,range->integer);
    if (!parsed) {
        screen::warnRange(translated(range->integer ? "integerRange" : "numberRange",range->minimum,range->maximum));
        return;
    }
    if (*parsed == range->value) { screen::clearMessage(); return; }
    applyShape(shape::setNumber(*definition, field, *parsed));
}
void applyName() { applyShapeName(); }
void revertName() {
    if (!editingShapeName) return;
    shapeNameInput.clear();
    shapeNameInput.append(shapeNameOrigin);
    shapeNameDirty = true;
    applyShapeName();
}
void endEditing() { editingShapeField = -1; editingShapeName = false; shapeNameDirty = false; }
glm::vec2 caret() {
    return {shapesDisplayed.stepperX(), editingShapeName ? shapesDisplayed.nameY : shapesDisplayed.fieldY(std::max(0, editingShapeField))};
}
bool docked() { return shapesDocked; }
void renderDocked(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) { renderShapesDocked(context, size, pointer); }
void renderContent(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer, SettingsTable const& table) {
    renderShapesContent(context, size, pointer, table);
}
void leave() { shapeDraft.reset(); shapePicking = false; overlay::shapes::setDraft({}); }
void reset() {
    if (shapeDraft) { shapeDraft.reset(); overlay::shapes::setDraft({}); }
    shapePicking = false; shapeDeleteArmed = false;
    if (shapeRows.clearSeveral()) shapeSelected.reset(); // a closed screen keeps one row, not several
    shapeBulk.disarm();
    endEditing();
}
int fieldSelected() { return shapeFieldSelected; }
std::optional<overlay::ShapeId> selected() { return shapeSelected; }
}

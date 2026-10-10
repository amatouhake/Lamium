#include "ui/ShapesView.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/ScreenParts.h"
#include "ui/SearchQuery.h"
#include "ui/ShapeEditor.h"
#include "ui/ShapesLayout.h"
#include "overlay/ShapeSession.h"
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
std::optional<overlay::ShapeId> shapeSelected;
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
// Reference point for new or moved shapes, and the reference actually used: a
// missing target falls back to the standing block with a notice.
std::optional<std::pair<overlay::Point, shape::Reference>> referencePoint() {
    auto* player = screen::client() ? screen::client()->getLocalPlayer() : nullptr;
    if (!player) return {};
    if (shapeReference == shape::Reference::TargetBlock) {
        auto const& hit = screen::client()->getLatestHitResult();
        if (hit.mType == HitResultType::Tile)
            return std::pair{overlay::Point{hit.mBlock.x + .5, hit.mBlock.y + .5, hit.mBlock.z + .5}, shapeReference};
        screen::setMessage(translated("shape.targetMissing"));
        auto feet = player->getFeetPos();
        return std::pair{overlay::Point{feet.x, feet.y, feet.z}, shape::Reference::StandingBlock};
    }
    auto feet = player->getFeetPos();
    return std::pair{overlay::Point{feet.x, feet.y, feet.z}, shapeReference};
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
    shapeSelected.reset();
    resetShapeEditor();
    try { overlay::shapes::setDraft(shapeDraft); }
    catch (std::exception const&) { screen::setMessage(translated("shape.editError")); }
}
void createDraft() {
    if (!shapeDraft) return;
    try {
        auto id = overlay::shapes::add(*shapeDraft);
        cancelDraft();
        shapeSelected = id;
        resetShapeEditor();
        screen::clearMessage();
    } catch (overlay::ShapeSaveError const&) { screen::setMessage(translated("shape.saveError")); }
    catch (std::exception const&) { screen::setMessage(translated("shape.editError")); }
}
void selectShape(std::optional<overlay::ShapeId> id) {
    cancelDraft();
    shapeSelected = id;
    resetShapeEditor();
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
    applyShape(shape::adjust(*definition, row.field, direction));
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
    if (!right && screen::pressScrollbar(shapesDisplayed, shapeListFirst, x, y)) return;
    if (!shapesDocked && screen::navClick(x, y)) return;
    auto hit = shapesDisplayed.hit(x, y);
    if (!(hit.zone == ShapeZone::Action && hit.index == 1 && !shapeDraft)) shapeDeleteArmed = false;
    switch (hit.zone) {
    case ShapeZone::Close: screen::close(); return;
    case ShapeZone::Dock: shapesDocked = !shapesDocked; return;
    case ShapeZone::Keys: openShapeKeySettings(); return;
    case ShapeZone::DrawAll:
        screen::toggleOption("overlays.shapes");
        return;
    case ShapeZone::NewShape:
        cancelDraft();
        shapeSelected.reset();
        shapePicking = true;
        resetShapeEditor();
        return;
    case ShapeZone::ListRow: {
        int index = hit.index - (shapeDraft ? 1 : 0);
        if (index < 0 || index >= static_cast<int>(shapeList.size())) return;
        auto const& item = shapeList[index];
        auto const& l = shapesDisplayed;
        if (x >= l.listLeft + l.listWidth - ShapesLayout::pad - switchWidth - 2) {
            try { overlay::shapes::setVisible(item.id, !item.definition.visible); screen::clearMessage(); }
            catch (overlay::ShapeSaveError const&) { screen::setMessage(translated("shape.saveError")); }
            catch (std::exception const&) { screen::setMessage(translated("shape.editError")); }
            return;
        }
        if (shapeSelected != item.id || shapeDraft) selectShape(item.id);
        return;
    }
    case ShapeZone::Name:
        if (auto definition = currentShape()) {
            editingShapeName = true;
            shapeNameInput.clear();
            shapeNameInput.append(definition->name);
            shapeNameInput.selectAll();
            screen::clearMessage();
        }
        return;
    case ShapeZone::LayerDown: --shapeLayer; return;
    case ShapeZone::LayerUp: ++shapeLayer; return;
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
        case 0x1b: case 0x0d: case 0x09: screen::finishEditing(); break;
        }
        return;
    }
    if (editingShapeField >= 0) {
        switch (key) {
        case 0x08: if (screen::number().backspace()) screen::numberTyped(); break;
        case 0x41: if (screen::heldCtrl()) screen::number().selectAll(); break;
        case 0x1b: case 0x0d: case 0x09: screen::finishEditing(); break;
        }
        return;
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
    case overlay::ShapeColor::Yellow: return {.95f,.8f,.24f};
    case overlay::ShapeColor::Pink: return {.94f,.5f,.75f};
    case overlay::ShapeColor::White: return {.95f,.95f,.95f};
    default: return {.25f,.82f,.88f};
    }
}
constexpr Rgb draftRgb{.62f,.83f,1.f};
void drawTypeIcon(MinecraftUIRenderContext& context, float x, float y, int type, Rgb color) {
    auto pattern = shape::typeGlyphs[static_cast<size_t>(std::clamp(type, 0, static_cast<int>(shape::typeGlyphs.size()) - 1))];
    for (int i = 0; i < 25 && i < static_cast<int>(pattern.size()); ++i)
        if (pattern[i] == '#') fill(context, x + (i % 5) * 2, y + (i / 5) * 2, 2, 2, color);
}
std::string shapeDescription(std::optional<overlay::ShapeDefinition> const& definition) {
    if (shapePicking) return translated("shape.pickType");
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
    float top = l.top + 4;
    // Header controls.
    label(context,l.drawAllX,l.drawAllY+1+boxTextInset(),l.drawAllWidth-switchWidth-4,translated("shape.drawAll"),
        over(ShapeZone::DrawAll) ? palette::text : palette::dim,Align::Right);
    toggleSwitch(context,l.drawAllX+l.drawAllWidth-switchWidth,l.drawAllY+2,preferences.overlays.shapes);
    label(context,l.keysX,top+1+boxTextInset(),ShapesLayout::keysWidth,translated("shape.keys"),
        over(ShapeZone::Keys) ? palette::text : palette::accent,Align::Center);
    fill(context,l.keysX+6,top+11,ShapesLayout::keysWidth-12,1,palette::accent,over(ShapeZone::Keys) ? 1.f : .5f);
    drawSmallButton(context,l.dockX,top,ShapesLayout::dockWidth,12,translated(shapesDocked ? "shape.undock" : "shape.dock"),over(ShapeZone::Dock));

    // List pane.
    float listRight = l.listLeft + l.listWidth;
    drawSmallButton(context,l.listLeft+ShapesLayout::pad,l.toolbarTop+2,ShapesLayout::newWidth,12,translated("shape.new"),
        over(ShapeZone::NewShape),shapePicking ? palette::accent : palette::accentDeep,palette::accent);
    fill(context,l.listLeft,l.theadTop-1,l.listWidth,1,palette::white,.14f);
    float nameX = l.listLeft + ShapesLayout::pad + 14;
    float shownX = listRight - ShapesLayout::pad - switchWidth - 2;
    float typeX = shownX - 50;
    label(context,nameX,l.theadTop+2,typeX-nameX-4,translated("shape.columnName"),palette::faint);
    label(context,typeX,l.theadTop+2,48,translated("shape.columnType"),palette::faint);
    label(context,shownX-6,l.theadTop+2,switchWidth+12,translated("shape.columnShown"),palette::faint,Align::Center);
    fill(context,l.listLeft,l.rowsTop-1,l.listWidth,1,palette::white,.14f);
    int draftOffset = shapeDraft ? 1 : 0;
    if (l.listCount == 0)
        paragraph(context,l.listLeft+ShapesLayout::pad,l.rowsTop+3,l.listWidth-2*ShapesLayout::pad,translated("shape.empty"),3,palette::faint);
    for (int i = l.listFirst; i < l.listFirst + l.listVisible && i < l.listCount; ++i) {
        float y = l.listRowY(i);
        bool isDraft = shapeDraft && i == 0;
        auto const* item = isDraft ? nullptr : &shapeList[i - draftOffset];
        auto const& shown = isDraft ? *shapeDraft : item->definition;
        bool chosen = isDraft || (!shapeDraft && item && shapeSelected == item->id);
        if (i % 2) fill(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,palette::white,.025f);
        rowBackground(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,chosen && !isDraft,over(ShapeZone::ListRow,i));
        if (isDraft) frame(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,draftRgb);
        bool elsewhere = shown.dimension != screen::playerDimension();
        // The same type glyph and color as the opened shape; dimmed when hidden or elsewhere.
        Rgb glyph = isDraft ? draftRgb : shapeRgb(shown.color);
        if (!shown.visible || elsewhere) glyph = {glyph.r * .4f, glyph.g * .4f, glyph.b * .4f};
        drawTypeIcon(context,l.listLeft+ShapesLayout::pad,y+2,shape::typeIndex(shown),glyph);
        std::string suffix = isDraft ? translated("shape.draftTag") : elsewhere ? translated("shape.otherDimension", shown.dimension) : "";
        float suffixWidth = suffix.empty() ? 0 : textWidth(context, suffix) + 6;
        label(context,nameX,y+3,typeX-nameX-4-suffixWidth,shown.name,isDraft ? draftRgb : elsewhere ? palette::faint : palette::text);
        if (!suffix.empty())
            label(context,typeX-4-suffixWidth+2,y+3,suffixWidth,suffix,isDraft ? draftRgb : palette::faint);
        label(context,typeX,y+3,48,translated(shape::types[shape::typeIndex(shown)].name),palette::dim);
        if (!isDraft) toggleSwitch(context,shownX,y+(ShapesLayout::rowHeight-switchHeight)/2,shown.visible);
    }
    drawListScrollbar(context,l);
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
    } else if (!definition) {
        paragraph(context,dx,l.detailTop+6,dw,translated(shapeList.empty() ? "shape.empty" : "shape.selectHint"),3,palette::faint);
    } else {
        if (shapeDraft) label(context,dx,l.detailTop+4,dw,translated("shape.draftNote"),draftRgb);
        // Name field.
        fill(context,dx,l.nameY,dw,ShapesLayout::rowHeight-1,Rgb{0,0,0},.4f);
        frame(context,dx,l.nameY,dw,ShapesLayout::rowHeight-1,editingShapeName ? palette::accent : palette::keyEdge);
        drawTypeIcon(context,dx+3,l.nameY+1.5f,shape::typeIndex(*definition),shapeDraft ? draftRgb : shapeRgb(definition->color));
        if (editingShapeName) drawEditText(context,dx+17,l.nameY,ShapesLayout::rowHeight-1,dw-20,shapeNameInput);
        else label(context,dx+17,l.nameY+1+boxTextInset(),dw-20,definition->name);
        // Preview: one layer seen from above; plane seen along its normal.
        float px = dx, py = l.previewY, size = ShapesLayout::previewSize;
        fill(context,px,py,size,size,Rgb{0,0,0},.35f);
        frame(context,px,py,size,size,palette::white,.14f);
        auto const* preview = previewFor(*definition);
        if (preview) {
            float spanU = float(preview->maxU - preview->minU + 1), spanV = float(preview->maxV - preview->minV + 1);
            float scale = std::min((size - 4) / spanU, (size - 4) / spanV);
            float ox = px + (size - spanU * scale) / 2 - preview->minU * scale;
            float oy = py + (size - spanV * scale) / 2 - preview->minV * scale;
            Rgb color = shapeDraft ? draftRgb : shapeRgb(definition->color);
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
            label(context,ix,py+22,iw,translated("shape.distance",distance),palette::dim);
        }
        if (preview && preview->lowLayer != preview->highLayer) {
            float lx = l.layerStepperX(), ly = l.layerY(), lw = 64, aw = ShapesLayout::arrowWidth;
            fill(context,lx,ly+1,aw,ShapesLayout::rowHeight-2,palette::keyFill);
            fill(context,lx+lw-aw,ly+1,aw,ShapesLayout::rowHeight-2,palette::keyFill);
            frame(context,lx,ly+1,lw,ShapesLayout::rowHeight-2,palette::keyEdge);
            label(context,lx,ly+2+boxTextInset(),aw,"-",palette::dim,Align::Center);
            label(context,lx+lw-aw,ly+2+boxTextInset(),aw,"+",palette::dim,Align::Center);
            label(context,lx+aw,ly+2+boxTextInset(),lw-2*aw,translated("shape.layer",(shapeLayer >= 0 ? "+" : "") + std::to_string(shapeLayer)),palette::text,Align::Center);
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
                toggleSwitch(context,l.stepperX()+l.stepperWidth()-switchWidth,y+(ShapesLayout::rowHeight-switchHeight)/2,definition->visible);
                break;
            case shape::Row::Kind::Button:
                drawSmallButton(context,l.stepperX(),y+1,l.stepperWidth(),ShapesLayout::rowHeight-2,translated(row.label),over(ShapeZone::Field,i));
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
                    value = range->integer ? std::to_string(static_cast<long long>(range->value)) : std::format("{:.3g}", range->value);
                    if (row.field == shape::Field::X || row.field == shape::Field::Y || row.field == shape::Field::Z)
                        value = range->integer ? value : std::format("{:.3f}", range->value);
                    else value = translated("shape.blocks", value);
                } else value = translated(shape::choiceLabel(*definition, row.field, shapeReference));
                drawShapeStepper(context,l,y,range.has_value(),std::move(value),editingShapeField == i ? &screen::number() : nullptr);
                break;
            }
            }
        }
        // Actions.
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
        if (shapeDraft) {
            drawSmallButton(context,l.actionX(0),l.actionsY+2,ShapesLayout::actionWidth,12,translated("shape.create"),
                over(ShapeZone::Action,0),palette::accentDeep,palette::accent);
            drawSmallButton(context,l.actionX(1),l.actionsY+2,ShapesLayout::actionWidth,12,translated("shape.cancel"),over(ShapeZone::Action,1));
        } else {
            drawSmallButton(context,l.actionX(0),l.actionsY+2,ShapesLayout::actionWidth,12,translated("shape.duplicateShort"),over(ShapeZone::Action,0));
            drawSmallButton(context,l.deleteX(),l.actionsY+2,ShapesLayout::deleteWidth,12,
                translated(shapeDeleteArmed ? "shape.deleteConfirm" : "shape.delete"),over(ShapeZone::Action,1),
                shapeDeleteArmed ? Rgb{.54f,.18f,.16f} : palette::keyFill,Rgb{.54f,.23f,.2f},
                shapeDeleteArmed ? palette::text : Rgb{1.f,.7f,.68f});
        }
    }

    // Footer.
    fill(context,l.left,l.footerTop,l.width,1,palette::white,.14f);
    float textLeft = l.left + ShapesLayout::pad, available = l.width - 2 * ShapesLayout::pad;
    bool shortFooter = l.docked || screen::table().shortFooter;
    // Simple graphics draw faces poorly (additive, order dependent); say so
    // rather than tuning that mode further.
    bool simple = screen::client() && screen::client()->getOptions().getGraphicsMode() == GraphicsMode::Simple;
    bool warn = !screen::message().empty() || simple;
    std::string text = !screen::message().empty() ? screen::message() : simple && shortFooter ? translated("shape.simpleWarning") : shapeDescription(definition);
    if (shortFooter) label(context,textLeft,l.footerTop+3,available,std::move(text),warn ? palette::warning : palette::text);
    else {
        paragraph(context,textLeft,l.footerTop+3,available,text,2,screen::message().empty() ? palette::text : palette::warning);
        label(context,textLeft,l.footerTop+30,available,translated(simple ? "shape.simpleWarning"
            : editingShapeName || editingShapeField >= 0 ? "shape.numberHint" : "shape.hint"),simple ? palette::warning : palette::faint);
    }
}
std::optional<overlay::ShapeDefinition> visibleShape() {
    auto definition = currentShape();
    if (!definition && shapeSelected) shapeSelected.reset(); // Removed elsewhere.
    return definition;
}
void renderShapesContent(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer,
                         SettingsTable const& t) {
    auto definition = visibleShape();
    int fieldCount = shapePicking ? static_cast<int>(shape::types.size())
        : definition ? static_cast<int>(shape::rows(*definition, shapeDraft.has_value()).size()) : 0;
    auto l = ShapesLayout::fit(t, size.x, size.y, false, static_cast<int>(shapeList.size()) + (shapeDraft ? 1 : 0),
        shapeListFirst, fieldCount, shapeFieldFirst, shapeDraft.has_value(), shapePicking);
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
    shapesDisplayed = l;
    shapeListFirst = l.listFirst; shapeFieldFirst = l.fieldFirst;
    if (!l.usable()) {
        label(context, 4, 4, std::max(1.0f, size.x - 8), translated("smallWindow"));
        context.flushText(0, std::nullopt);
        return;
    }
    // Docked: the world stays visible; only the panel is drawn.
    panel(context,l.left,l.top,l.width,l.height,.82f);
    frame(context,l.left,l.top,l.width,l.height,palette::white,.14f);
    label(context,l.left+ShapesLayout::pad,l.top+6,l.drawAllX-l.left-10,translated("nav.shapes"));
    bool closeHover = l.hit(pointer.x, pointer.y).zone == ShapeZone::Close;
    drawSmallButton(context,l.closeX,l.top+4,ShapesLayout::closeWidth,12,translated("closeButton"),closeHover,
        palette::keyFill,palette::keyEdge,closeHover ? palette::text : palette::dim);
    fill(context,l.left,l.top+ShapesLayout::headerHeight-1,l.width,1,palette::white,.14f);
    drawShapesBody(context, l, pointer, definition);
    context.flushText(0,std::nullopt);
}

}

void refresh() { shapeList = overlay::shapes::list(); }
void click(float x, float y, bool right) { handleShapeClick(x, y, right); }
void key(int key) { handleShapeKey(key); }
void wheel(int step, glm::vec2 pointer) {
    auto const& l = shapesDisplayed;
    bool overList = pointer.x >= l.listLeft && pointer.x < l.listLeft + l.listWidth && (!l.docked || pointer.y < l.detailTop);
    if (overList) shapeListFirst = std::max(0, shapeListFirst + step);
    else shapeFieldFirst = std::max(0, shapeFieldFirst + step);
}
bool editingName() { return editingShapeName; }
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
    endEditing();
}
int fieldSelected() { return shapeFieldSelected; }
std::optional<overlay::ShapeId> selected() { return shapeSelected; }
}

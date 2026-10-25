#include "ui/ShapeEditor.h"
#include "ui/ShapesLayout.h"
#include "ui/Translations.h"
#include <format>
void check(bool, char const*);
void shapeEditorTests() {
    using namespace lamium;
    using ui::shape::Field;
    using ui::shape::Reference;
    overlay::Point feet{10.7, 64, -3.2};

    check(ui::shape::typeGlyphs.size() == ui::shape::types.size(), "every shape type has its own glyph");
    for (size_t a = 0; a < ui::shape::typeGlyphs.size(); ++a) {
        auto glyph = ui::shape::typeGlyphs[a];
        check(glyph.size() == 25 && glyph.find_first_not_of(".#") == std::string_view::npos,
              "type glyphs are complete 5x5 patterns");
        for (size_t b = a + 1; b < ui::shape::typeGlyphs.size(); ++b)
            check(glyph != ui::shape::typeGlyphs[b], "each type glyph is distinct");
    }

    // Every type builds a valid definition, placed by the chosen reference.
    for (size_t type = 0; type < ui::shape::types.size(); ++type) {
        overlay::ShapeDefinition base;
        base.name = "Draft";
        auto definition = ui::shape::withType(base, static_cast<int>(type), feet, Reference::StandingBlock);
        check(ui::shape::typeIndex(definition) == static_cast<int>(type), "type index follows the created type");
        overlay::ShapeCollection collection;
        check(collection.add(definition) > 0, "every offered type produces valid geometry");
        for (auto locale : {"en_US", "ja_JP", "zh_CN", "zh_TW"}) {
            check(!ui::translations::find(ui::shape::types[type].name, locale).empty()
                && !ui::translations::find(ui::shape::types[type].description, locale).empty(), "type names and descriptions are localized");
        }
        for (bool draft : {true, false}) {
            auto rows = ui::shape::rows(definition, draft);
            check(rows.front().kind == ui::shape::Row::Kind::Group, "editor rows start with a group");
            bool hasType = false, hasCoordinates = false, hasMove = false;
            for (auto const& row : rows) {
                hasType = hasType || row.field == Field::Type && row.kind != ui::shape::Row::Kind::Group;
                hasCoordinates = hasCoordinates || row.field == Field::X;
                hasMove = hasMove || row.field == Field::MoveHere;
                if (row.kind == ui::shape::Row::Kind::Value && !ui::shape::numeric(definition, row.field))
                    check(!ui::shape::choiceLabel(definition, row.field, Reference::StandingBlock).empty(),
                        "every non-numeric value has a choice label");
                if (row.label.size() > 1) for (auto locale : {"en_US", "ja_JP", "zh_CN", "zh_TW"})
                    check(!ui::translations::find(row.label, locale).empty(), "editor labels are localized");
            }
            check(hasType == draft && hasCoordinates != draft && hasMove != draft,
                "drafts choose type and reference; existing shapes edit coordinates and move");
        }
    }

    // References choose snapping for round shapes; planes use the block.
    overlay::ShapeDefinition sphere = ui::shape::withType({"S"}, 2, feet, Reference::ExactPosition);
    auto spec = std::get<overlay::ShapeSpec>(sphere.geometry);
    check(spec.snap == overlay::Snap::Off && spec.center == feet, "exact position keeps the precise center");
    ui::shape::place(sphere, feet, Reference::StandingBlock);
    check(std::get<overlay::ShapeSpec>(sphere.geometry).snap == overlay::Snap::BlockCenter, "standing block snaps to its center");
    auto plane = ui::shape::withType({"P"}, 9, feet, Reference::ExactPosition);
    check(std::get<overlay::PlaneSpec>(plane.geometry).origin == overlay::Cell{10, 64, -4}, "planes start at the block containing the point");

    // Numbers step within range; choices cycle; invalid values are rejected.
    auto bigger = ui::shape::adjust(sphere, Field::Radius, 1);
    check(std::get<overlay::ShapeSpec>(bigger.geometry).radius == 4.5, "radius steps by half a block");
    auto smallest = ui::shape::setNumber(sphere, Field::Radius, 0);
    check(std::get<overlay::ShapeSpec>(ui::shape::adjust(smallest, Field::Radius, -1).geometry).radius == 0, "steps clamp at the minimum");
    bool rejected = false;
    try { (void)ui::shape::setNumber(plane, Field::Width, 2.5); } catch (std::invalid_argument const&) { rejected = true; }
    check(rejected, "integer fields reject fractions");
    auto restyled = ui::shape::adjust(sphere, Field::Style, 1);
    check(restyled.style == overlay::ShapeStyle::Line && ui::shape::adjust(restyled, Field::Style, 1).style == overlay::ShapeStyle::Face,
        "style cycles between faces and lines");
    check(ui::shape::adjust(sphere, Field::Color, -1).color == overlay::ShapeColor::White, "color cycles backwards");
    check(!ui::shape::adjust(sphere, Field::Visible, 1).visible, "visibility toggles");

    // Preview: layers of a sphere, rings of a circle, runs of a filled plane.
    auto ball = ui::shape::withType({"B"}, 2, {0.5, 0.5, 0.5}, Reference::StandingBlock);
    auto equator = ui::shape::preview(ball, 0);
    check(equator.lowLayer == -4 && equator.highLayer == 4 && equator.cells > 0, "sphere preview spans its layers");
    check(ui::shape::preview(ball, 9).cells == 0, "layers outside the shape are empty");
    auto ring = ui::shape::preview(ui::shape::withType({"C"}, 0, {0.5, 0.5, 0.5}, Reference::StandingBlock), 0);
    bool hollow = true;
    for (auto const& run : ring.runs) hollow = hollow && !(run.v == 0 && run.u <= 0 && run.u + run.length > 0);
    check(hollow && ring.lowLayer == 0 && ring.highLayer == 0, "circle preview is a ring on one layer");
    auto grid = ui::shape::withType({"G"}, 9, {0, 0, 0}, Reference::StandingBlock);
    auto filled = ui::shape::preview(grid, 0);
    check(filled.cells == 81 && filled.runs.size() == 9, "a filled plane previews as one run per row");

    // Layout: side by side inside the panel; docked keeps the world visible.
    auto table = ui::SettingsTable::fit(640, 360, 20, 0);
    auto inside = ui::ShapesLayout::fit(table, 640, 360, false, 3, 0, 16, 0, false, false);
    check(inside.usable() && inside.listLeft + inside.listWidth == inside.detailLeft
        && inside.detailLeft + inside.detailWidth == table.rowsRight(), "list and editor share the table area");
    auto many = ui::ShapesLayout::fit(table, 640, 360, false, 80, 0, 16, 0, false, false);
    float bar = many.listLeft + many.listWidth - 2;
    check(many.onScrollbar(bar, many.rowsTop + 1) && !many.onScrollbar(many.listLeft + 10, many.rowsTop + 1) && !inside.onScrollbar(bar, inside.rowsTop + 1)
          && many.firstAt(many.rowsTop) == 0 && many.firstAt(many.rowsTop + many.listVisible * ui::ShapesLayout::rowHeight) == 80 - many.listVisible,
          "the scrollbar takes presses only while rows overflow, and maps its track to the first row");
    check(inside.hit(inside.listLeft + 10, inside.listRowY(1) + 3).zone == ui::ShapesLayout::Zone::ListRow
        && inside.hit(inside.listLeft + 10, inside.listRowY(1) + 3).index == 1, "list rows are hit by index");
    auto field = inside.hit(inside.stepperX() + 2, inside.fieldY(2) + 3);
    check(field.zone == ui::ShapesLayout::Zone::Field && field.index == 2 && field.part == -1, "stepper decrease part");
    check(inside.hit(inside.stepperX() + inside.stepperWidth() - 2, inside.fieldY(2) + 3).part == 1, "stepper increase part");
    check(inside.hit(inside.detailLeft + 8, inside.fieldY(2) + 3).part == 2, "labels select without changing");
    for (int count : {4, 12}) {
        float size = inside.swatchSize(count);
        check(size >= 6 && inside.swatchX(count - 1, count) + size <= inside.stepperX() + inside.stepperWidth() + .01f,
              "every color swatch fits in the value column");
        bool each = true;
        for (int i = 0; i < count; ++i) each = each && inside.swatchAt(inside.swatchX(i, count) + size / 2, count) == i;
        check(each, "a click on a swatch picks that color");
    }
    check(inside.swatchAt(inside.stepperX() - 5, 12) == -1, "left of the swatches picks nothing");
    check(inside.hit(inside.closeX + 2, inside.top + 8).zone == ui::ShapesLayout::Zone::Close, "close button");
    check(inside.hit(inside.listLeft + 10, inside.toolbarTop + 6).zone == ui::ShapesLayout::Zone::NewShape, "new shape button");
    auto docked = ui::ShapesLayout::fit(table, 640, 360, true, 8, 0, 16, 0, true, false);
    check(docked.usable() && docked.left > 640 * .5f && docked.detailTop > docked.rowsTop, "docked panel sits right, list above editor");
    check(docked.hit(docked.drawAllX + 2, docked.drawAllY + 4).zone == ui::ShapesLayout::Zone::DrawAll,
        "docked draw-all switch moves to the toolbar");
    auto narrow = ui::ShapesLayout::fit(table, 480, 253, true, 8, 0, 16, 0, false, false, true);
    check(narrow.left + ui::ShapesLayout::pad + narrow.dockedTitleWidth() <= narrow.keysX,
        "a docked title stops before the key settings link");
    // Schematics fills the toolbar with its tabs: docked, the switch gets its own row
    // below the toolbar, clear of the tabs and above the column headings.
    auto tabs = ui::ShapesLayout::fit(table, 640, 360, true, 8, 0, 16, 0, false, false, true);
    check(tabs.usable() && tabs.drawAllY >= tabs.toolbarTop + ui::ShapesLayout::toolbarHeight
        && tabs.drawAllY + 12 <= tabs.theadTop, "docked draw-all switch below a full toolbar");
    check(tabs.hit(tabs.drawAllX + 2, tabs.drawAllY + 4).zone == ui::ShapesLayout::Zone::DrawAll
        && tabs.hit(tabs.drawAllX + 2, tabs.toolbarTop + 6).zone != ui::ShapesLayout::Zone::DrawAll,
        "the switch is hit in its own row, not in the toolbar");
    check(tabs.detailTop > tabs.rowsTop && tabs.rowsTop - docked.rowsTop == ui::ShapesLayout::toolbarHeight,
        "the list moves down by one toolbar row");
    // A short screen (1920x1012 at GUI scale 4 is 480x253): the docked list gives
    // up rows so the editor keeps some, and the header works even with none left.
    auto shortScreen = ui::ShapesLayout::fit(table, 480, 253, true, 8, 0, 13, 0, false, false, true);
    check(shortScreen.width > 0 && shortScreen.listVisible < ui::ShapesLayout::dockedListRows
        && shortScreen.listVisible >= 2 && shortScreen.fieldVisible >= 1, "a short docked panel shares its height");
    auto cramped = ui::ShapesLayout::fit(table, 480, 150, true, 8, 0, 13, 0, false, false, true);
    check(cramped.width > 0 && cramped.fieldVisible == 0, "a very short docked panel has no editor rows");
    check(cramped.hit(cramped.dockX + 2, cramped.top + 8).zone == ui::ShapesLayout::Zone::Dock
        && cramped.hit(cramped.closeX + 2, cramped.top + 8).zone == ui::ShapesLayout::Zone::Close,
        "it can still be undocked or closed");
    // Nothing else is drawn then (drawDockedTooShort), so nothing else answers.
    bool nothingElse = cramped.hit(cramped.keysX + 2, cramped.top + 8).zone == ui::ShapesLayout::Zone::None
        && cramped.hit(cramped.drawAllX + 2, cramped.drawAllY + 4).zone == ui::ShapesLayout::Zone::None
        && cramped.hit(cramped.listLeft + 10, cramped.rowsTop + 3).zone == ui::ShapesLayout::Zone::None
        && cramped.hit(cramped.deleteX() + 2, cramped.actionsY + 4).zone == ui::ShapesLayout::Zone::None
        && cramped.hit(cramped.left + 10, cramped.footerTop + 4).zone == ui::ShapesLayout::Zone::None
        && !cramped.onScrollbar(cramped.listLeft + cramped.listWidth - 2, cramped.rowsTop + 3);
    check(nothingElse, "a panel too short for its body answers only Close and Expand");
    // Whenever a layout is usable, its parts stack without overlapping: the list
    // above the detail pane, the preview above the fields, the fields above the
    // actions, the actions above the footer. The Check tab's 60-unit preview and
    // one text line also fit above the actions.
    int usableLayouts = 0;
    for (bool dockedView : {true, false})
        for (bool full : {false, true})
            for (bool draft : {false, true})
                for (float h : {150.f, 180.f, 200.f, 253.f, 300.f, 360.f, 480.f}) {
                    auto l = ui::ShapesLayout::fit(table, 480, h, dockedView, 12, 0, 13, 0, draft, false, full);
                    if (!l.usable()) continue;
                    ++usableLayouts;
                    bool listAbove = !dockedView || l.rowsTop + l.listVisible * ui::ShapesLayout::rowHeight <= l.detailTop;
                    bool stacked = listAbove && l.previewY + ui::ShapesLayout::previewSize <= l.fieldsTop
                        && l.fieldY(l.fieldFirst + l.fieldVisible) <= l.actionsY - 2
                        && l.actionsY + ui::ShapesLayout::actionsHeight <= l.footerTop
                        && l.actionsY - 6 >= l.previewY - 2 + 60 + 5 + 10;
                    check(stacked, std::format("usable layout stacks without overlap (docked={} full={} draft={} h={})",
                        dockedView, full, draft, h).c_str());
                }
    check(usableLayouts >= 40, "most of those sizes fit their body");
    check(docked.hit(docked.actionX(1) + 2, docked.actionsY + 4).zone == ui::ShapesLayout::Zone::Action
        && docked.hit(docked.actionX(1) + 2, docked.actionsY + 4).index == 1, "draft cancel action");
    auto picking = ui::ShapesLayout::fit(table, 640, 360, false, 0, 0, 4, 0, false, true);
    check(picking.hit(picking.detailLeft + 20, picking.fieldY(3) + 5).zone == ui::ShapesLayout::Zone::Pick
        && picking.hit(picking.detailLeft + 20, picking.fieldY(3) + 5).index == 3, "type picker rows");
}

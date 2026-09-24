#include "ui/ShapeEditor.h"
#include "ui/ShapesLayout.h"
#include "ui/Translations.h"
void check(bool, char const*);
void shapeEditorTests() {
    using namespace lamium;
    using ui::shape::Field;
    using ui::shape::Reference;
    overlay::Point feet{10.7, 64, -3.2};

    // Every type builds a valid definition, placed by the chosen reference.
    for (size_t type = 0; type < ui::shape::types.size(); ++type) {
        overlay::ShapeDefinition base;
        base.name = "Draft";
        auto definition = ui::shape::withType(base, static_cast<int>(type), feet, Reference::StandingBlock);
        check(ui::shape::typeIndex(definition) == static_cast<int>(type), "type index follows the created type");
        overlay::ShapeCollection collection;
        check(collection.add(definition) > 0, "every offered type produces valid geometry");
        for (auto locale : {"en_US", "ja_JP"}) {
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
                if (row.label.size() > 1) for (auto locale : {"en_US", "ja_JP"})
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
    check(inside.hit(inside.listLeft + 10, inside.listRowY(1) + 3).zone == ui::ShapesLayout::Zone::ListRow
        && inside.hit(inside.listLeft + 10, inside.listRowY(1) + 3).index == 1, "list rows are hit by index");
    auto field = inside.hit(inside.stepperX() + 2, inside.fieldY(2) + 3);
    check(field.zone == ui::ShapesLayout::Zone::Field && field.index == 2 && field.part == -1, "stepper decrease part");
    check(inside.hit(inside.stepperX() + inside.stepperWidth() - 2, inside.fieldY(2) + 3).part == 1, "stepper increase part");
    check(inside.hit(inside.detailLeft + 8, inside.fieldY(2) + 3).part == 2, "labels select without changing");
    check(inside.hit(inside.closeX + 2, inside.top + 8).zone == ui::ShapesLayout::Zone::Close, "close button");
    check(inside.hit(inside.listLeft + 10, inside.toolbarTop + 6).zone == ui::ShapesLayout::Zone::NewShape, "new shape button");
    auto docked = ui::ShapesLayout::fit(table, 640, 360, true, 8, 0, 16, 0, true, false);
    check(docked.usable() && docked.left > 640 * .5f && docked.detailTop > docked.rowsTop, "docked panel sits right, list above editor");
    check(docked.hit(docked.drawAllX + 2, docked.drawAllY + 4).zone == ui::ShapesLayout::Zone::DrawAll,
        "docked draw-all switch moves to the toolbar");
    check(docked.hit(docked.actionX(1) + 2, docked.actionsY + 4).zone == ui::ShapesLayout::Zone::Action
        && docked.hit(docked.actionX(1) + 2, docked.actionsY + 4).index == 1, "draft cancel action");
    auto picking = ui::ShapesLayout::fit(table, 640, 360, false, 0, 0, 4, 0, false, true);
    check(picking.hit(picking.detailLeft + 20, picking.fieldY(3) + 5).zone == ui::ShapesLayout::Zone::Pick
        && picking.hit(picking.detailLeft + 20, picking.fieldY(3) + 5).index == 3, "type picker rows");
}

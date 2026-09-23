#pragma once
#include "overlay/ShapeCollection.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lamium::ui::shape {
using overlay::ShapeDefinition;

// The shape types offered for new shapes. Adding a type means adding an entry
// here and its fields below; the screen lists and edits types generically.
struct TypeInfo { std::string_view id, name, description, group; };
inline constexpr auto types = std::to_array<TypeInfo>({
    {"circle", "shape.circleName", "shape.type.circle", "shape.group.basic"},
    {"cylinder", "shape.cylinderName", "shape.type.cylinder", "shape.group.basic"},
    {"sphere", "shape.sphereName", "shape.type.sphere", "shape.group.basic"},
    {"plane", "shape.planeName", "shape.type.plane", "shape.group.basic"},
});
inline int typeIndex(ShapeDefinition const& definition) {
    if (auto spec = std::get_if<overlay::ShapeSpec>(&definition.geometry))
        return spec->shape == overlay::Shape::Circle ? 0 : spec->shape == overlay::Shape::Cylinder ? 1 : 2;
    return 3;
}

// Where a new shape's center comes from. It also chooses how the center snaps:
// the standing and targeted blocks use block centers, the exact position none.
enum class Reference { StandingBlock, ExactPosition, TargetBlock };
inline constexpr auto referenceLabels = std::to_array<std::string_view>({
    "shape.ref.standing", "shape.ref.exact", "shape.ref.target"});
inline overlay::Cell blockOf(overlay::Point p) {
    return {overlay::checkedCoordinate(std::floor(p.x)), overlay::checkedCoordinate(std::floor(p.y)),
            overlay::checkedCoordinate(std::floor(p.z))};
}
// Moves a definition to a reference point, keeping its type and dimensions.
inline void place(ShapeDefinition& definition, overlay::Point point, Reference reference) {
    if (auto spec = std::get_if<overlay::ShapeSpec>(&definition.geometry)) {
        spec->center = point;
        spec->snap = reference == Reference::ExactPosition ? overlay::Snap::Off : overlay::Snap::BlockCenter;
    } else {
        auto& plane = std::get<overlay::PlaneSpec>(definition.geometry);
        plane.origin = blockOf(point);
    }
}
// A type's defaults, keeping name, dimension and appearance of the source.
inline ShapeDefinition withType(ShapeDefinition definition, int type, overlay::Point point, Reference reference) {
    if (type == 3) definition.geometry = overlay::PlaneSpec{{}, 9, 9, 1, overlay::Plane::XZ};
    else definition.geometry = overlay::ShapeSpec{type == 0 ? overlay::Shape::Circle : type == 1
        ? overlay::Shape::Cylinder : overlay::Shape::Sphere, {}, overlay::Snap::BlockCenter, 4, 5};
    place(definition, point, reference);
    return definition;
}

enum class Field { Type, Radius, Height, Snap, Width, Depth, Spacing, Orientation, X, Y, Z,
    Reference, MoveHere, Visible, Style, Color };
struct Row {
    enum class Kind { Group, Value, Switch, Button } kind;
    std::string_view label;
    Field field = Field::Type;
};
// Editor rows grouped as shape, position and display. A draft adds its type and
// takes the center reference instead of coordinates and a move action.
inline std::vector<Row> rows(ShapeDefinition const& definition, bool draft) {
    using Kind = Row::Kind;
    std::vector<Row> result;
    if (draft) {
        result.push_back({Kind::Group, "shape.group.type"});
        result.push_back({Kind::Value, "shape.field.type", Field::Type});
    }
    result.push_back({Kind::Group, "shape.group.shape"});
    if (auto spec = std::get_if<overlay::ShapeSpec>(&definition.geometry)) {
        result.push_back({Kind::Value, "shape.field.radius", Field::Radius});
        if (spec->shape == overlay::Shape::Cylinder) result.push_back({Kind::Value, "shape.field.height", Field::Height});
        result.push_back({Kind::Value, "shape.field.snap", Field::Snap});
    } else {
        result.push_back({Kind::Value, "shape.field.width", Field::Width});
        result.push_back({Kind::Value, "shape.field.depth", Field::Depth});
        result.push_back({Kind::Value, "shape.field.spacing", Field::Spacing});
        result.push_back({Kind::Value, "shape.field.orientation", Field::Orientation});
    }
    result.push_back({Kind::Group, "shape.group.position"});
    if (!draft) {
        result.push_back({Kind::Value, "X", Field::X});
        result.push_back({Kind::Value, "Y", Field::Y});
        result.push_back({Kind::Value, "Z", Field::Z});
    }
    result.push_back({Kind::Value, "shape.field.reference", Field::Reference});
    if (!draft) result.push_back({Kind::Button, "shape.field.moveHere", Field::MoveHere});
    result.push_back({Kind::Group, "shape.group.display"});
    if (!draft) result.push_back({Kind::Switch, "shape.field.visible", Field::Visible});
    result.push_back({Kind::Value, "shape.field.style", Field::Style});
    result.push_back({Kind::Value, "shape.field.color", Field::Color});
    return result;
}

struct Numeric { double value, minimum, maximum; bool integer = false; double step = 1; };
inline std::optional<Numeric> numeric(ShapeDefinition const& definition, Field field) {
    if (auto spec = std::get_if<overlay::ShapeSpec>(&definition.geometry)) {
        switch (field) {
        case Field::X: return Numeric{spec->center.x, -30000000, 30000000};
        case Field::Y: return Numeric{spec->center.y, -30000000, 30000000};
        case Field::Z: return Numeric{spec->center.z, -30000000, 30000000};
        case Field::Radius: return Numeric{spec->radius, 0, 512, false, .5};
        case Field::Height: return Numeric{double(spec->height), 1, 512, true};
        default: return {};
        }
    }
    auto const& plane = std::get<overlay::PlaneSpec>(definition.geometry);
    switch (field) {
    case Field::X: return Numeric{double(plane.origin.x), -30000000, 30000000, true};
    case Field::Y: return Numeric{double(plane.origin.y), -30000000, 30000000, true};
    case Field::Z: return Numeric{double(plane.origin.z), -30000000, 30000000, true};
    case Field::Width: return Numeric{double(plane.width), 1, 512, true};
    case Field::Depth: return Numeric{double(plane.depth), 1, 512, true};
    case Field::Spacing: return Numeric{double(plane.spacing), 1, 512, true};
    default: return {};
    }
}
// Throws std::invalid_argument when outside the field's range.
inline ShapeDefinition setNumber(ShapeDefinition definition, Field field, double number) {
    auto range = numeric(definition, field);
    if (!range || !std::isfinite(number) || number < range->minimum || number > range->maximum
        || (range->integer && std::trunc(number) != number)) throw std::invalid_argument("Invalid shape value");
    if (auto spec = std::get_if<overlay::ShapeSpec>(&definition.geometry)) {
        switch (field) {
        case Field::X: spec->center.x = number; break;
        case Field::Y: spec->center.y = number; break;
        case Field::Z: spec->center.z = number; break;
        case Field::Radius: spec->radius = number; break;
        case Field::Height: spec->height = static_cast<int>(number); break;
        default: break;
        }
    } else {
        auto& plane = std::get<overlay::PlaneSpec>(definition.geometry);
        int value = static_cast<int>(number);
        switch (field) {
        case Field::X: plane.origin.x = value; break;
        case Field::Y: plane.origin.y = value; break;
        case Field::Z: plane.origin.z = value; break;
        case Field::Width: plane.width = value; break;
        case Field::Depth: plane.depth = value; break;
        case Field::Spacing: plane.spacing = value; break;
        default: break;
        }
    }
    return definition;
}
// Steps a number or cycles a choice. Type and reference are owned by the caller.
inline ShapeDefinition adjust(ShapeDefinition definition, Field field, int direction) {
    int step = direction < 0 ? -1 : 1;
    auto cycle = [&](auto value, int count) {
        return static_cast<decltype(value)>((static_cast<int>(value) + step + count) % count);
    };
    if (auto range = numeric(definition, field))
        return setNumber(definition, field, std::clamp(range->value + step * range->step, range->minimum, range->maximum));
    switch (field) {
    case Field::Snap:
        if (auto spec = std::get_if<overlay::ShapeSpec>(&definition.geometry)) spec->snap = cycle(spec->snap, 3);
        break;
    case Field::Orientation:
        if (auto plane = std::get_if<overlay::PlaneSpec>(&definition.geometry)) plane->plane = cycle(plane->plane, 3);
        break;
    case Field::Style: definition.style = cycle(definition.style, 2); break;
    case Field::Color: definition.color = cycle(definition.color, 4); break;
    case Field::Visible: definition.visible = !definition.visible; break;
    default: break;
    }
    return definition;
}
inline constexpr auto snapLabels = std::to_array<std::string_view>({"shape.blockCenter", "shape.blockCorner", "shape.noSnap"});
inline constexpr auto orientationLabels = std::to_array<std::string_view>({"shape.plane.xz", "shape.plane.xy", "shape.plane.yz"});
inline constexpr auto styleLabels = std::to_array<std::string_view>({"shape.style.face", "shape.style.line"});
inline constexpr auto colorLabels = std::to_array<std::string_view>({"shape.color.cyan", "shape.color.yellow", "shape.color.pink", "shape.color.white"});
// The translation key of a choice value, or empty for numbers and actions.
inline std::string_view choiceLabel(ShapeDefinition const& definition, Field field, Reference reference) {
    switch (field) {
    case Field::Type: return types[typeIndex(definition)].name;
    case Field::Snap:
        if (auto spec = std::get_if<overlay::ShapeSpec>(&definition.geometry)) return snapLabels[static_cast<size_t>(spec->snap)];
        return {};
    case Field::Orientation:
        if (auto plane = std::get_if<overlay::PlaneSpec>(&definition.geometry)) return orientationLabels[static_cast<size_t>(plane->plane)];
        return {};
    case Field::Reference: return referenceLabels[static_cast<size_t>(reference)];
    case Field::Style: return styleLabels[static_cast<size_t>(definition.style)];
    case Field::Color: return colorLabels[static_cast<size_t>(definition.color)];
    default: return {};
    }
}

// Top-down block layout for the preview: one layer of a round shape relative
// to its center block, or a plane seen along its normal. Rows are merged into
// runs so a filled grid draws as few rectangles as possible.
struct PreviewRun { int u, v, length; };
struct Preview {
    std::vector<PreviewRun> runs;
    int cells = 0;
    int minU = 0, maxU = 0, minV = 0, maxV = 0; // Bounds over all layers.
    int lowLayer = 0, highLayer = 0;
};
inline Preview preview(ShapeDefinition const& definition, int layer) {
    Preview result;
    std::set<overlay::Cell> cells;
    overlay::Cell center{};
    auto spec = std::get_if<overlay::ShapeSpec>(&definition.geometry);
    if (spec) {
        // Round shapes: only the requested layer, from the column model, so
        // large radii stay cheap. Bounds cover every column.
        auto columns = overlay::roundColumns(*spec);
        center = blockOf(overlay::snapped(spec->center, spec->snap));
        bool any = false;
        for (int x = columns.x0; x < columns.x0 + columns.width; ++x)
            for (int z = columns.z0; z < columns.z0 + columns.depth; ++z) {
                auto [lo, hi] = columns.at(x, z);
                if (lo > hi) continue;
                int u = x - center.x, v = z - center.z;
                if (!any) { result.minU = result.maxU = u; result.minV = result.maxV = v; any = true; }
                result.minU = std::min(result.minU, u); result.maxU = std::max(result.maxU, u);
                result.minV = std::min(result.minV, v); result.maxV = std::max(result.maxV, v);
            }
        if (!any) return result;
        result.lowLayer = columns.low - center.y;
        result.highLayer = columns.high - center.y;
        std::map<std::pair<int,int>, bool> layerCells;
        for (auto const& c : overlay::roundLayer(columns, center.y + layer, spec->shape == overlay::Shape::Sphere))
            layerCells[{c.z - center.z, c.x - center.x}] = true;
        result.cells = static_cast<int>(layerCells.size());
        for (auto it = layerCells.begin(); it != layerCells.end();) {
            auto [v, u] = it->first;
            int length = 1;
            auto next = std::next(it);
            while (next != layerCells.end() && next->first.first == v && next->first.second == u + length) { ++length; ++next; }
            result.runs.push_back({u, v, length});
            it = next;
        }
        return result;
    } else {
        auto const& plane = std::get<overlay::PlaneSpec>(definition.geometry);
        cells = overlay::shapeCells(definition);
        center = plane.origin;
    }
    auto project = [&](overlay::Cell c) -> std::array<int,3> { // u, v, layer
        if (spec) return {c.x - center.x, c.z - center.z, c.y - center.y};
        auto orientation = std::get<overlay::PlaneSpec>(definition.geometry).plane;
        if (orientation == overlay::Plane::XY) return {c.x - center.x, center.y - c.y, 0};
        if (orientation == overlay::Plane::YZ) return {c.z - center.z, center.y - c.y, 0};
        return {c.x - center.x, c.z - center.z, 0};
    };
    bool first = true;
    std::map<std::pair<int,int>, bool> layerCells;
    for (auto const& c : cells) {
        auto [u, v, l] = project(c);
        if (first) { result.minU = result.maxU = u; result.minV = result.maxV = v; result.lowLayer = result.highLayer = l; first = false; }
        result.minU = std::min(result.minU, u); result.maxU = std::max(result.maxU, u);
        result.minV = std::min(result.minV, v); result.maxV = std::max(result.maxV, v);
        result.lowLayer = std::min(result.lowLayer, l); result.highLayer = std::max(result.highLayer, l);
        if (l == layer) layerCells[{v, u}] = true;
    }
    result.cells = static_cast<int>(layerCells.size());
    for (auto it = layerCells.begin(); it != layerCells.end();) {
        auto [v, u] = it->first;
        int length = 1;
        auto next = std::next(it);
        while (next != layerCells.end() && next->first.first == v && next->first.second == u + length) { ++length; ++next; }
        result.runs.push_back({u, v, length});
        it = next;
    }
    return result;
}
}

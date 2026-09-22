#pragma once
#include "overlay/ShapeSession.h"
#include "ui/Localization.h"

namespace lamium::ui {
// Dedicated tool content; the host supplies focus, navigation and list drawing.
class ShapePanel {
    enum class Control { Back, Sphere, Circle, Cylinder, Plane, Select, Visible, X, Y, Z,
        Radius, Height, Snap, Width, Depth, Spacing, Orientation, Duplicate, Remove, Name };
    struct Row { Control control; std::string text; overlay::ShapeId id = 0; };
    std::vector<Row> rows;
    std::optional<overlay::ShapeId> editing;
    std::optional<overlay::ShapeDefinition> definition;
    void row(Control control, std::string text, overlay::ShapeId id = 0) { rows.push_back({control,std::move(text),id}); }
public:
    std::optional<std::string> nameAt(int index) const {
        return definition && rows.at(index).control == Control::Name ? std::optional(definition->name) : std::nullopt;
    }
    void rename(std::string name) {
        if (!editing) throw std::out_of_range("No shape selected");
        overlay::shapes::rename(*editing,std::move(name)); refresh();
    }
    struct Numeric { double value, minimum, maximum; bool integer = false; };
    std::optional<Numeric> numeric(int index) const {
        if (!definition) return {};
        auto control = rows.at(index).control;
        if (auto spec = std::get_if<overlay::ShapeSpec>(&definition->geometry)) {
            switch (control) {
            case Control::X: return Numeric{spec->center.x,-30000000,30000000};
            case Control::Y: return Numeric{spec->center.y,-30000000,30000000};
            case Control::Z: return Numeric{spec->center.z,-30000000,30000000};
            case Control::Radius: return Numeric{spec->radius,0,512};
            case Control::Height: return Numeric{double(spec->height),1,512,true};
            default: return {};
            }
        }
        auto const& plane = std::get<overlay::PlaneSpec>(definition->geometry);
        switch (control) {
        case Control::X: return Numeric{double(plane.origin.x),-30000000,30000000,true};
        case Control::Y: return Numeric{double(plane.origin.y),-30000000,30000000,true};
        case Control::Z: return Numeric{double(plane.origin.z),-30000000,30000000,true};
        case Control::Width: return Numeric{double(plane.width),1,512,true};
        case Control::Depth: return Numeric{double(plane.depth),1,512,true};
        case Control::Spacing: return Numeric{double(plane.spacing),1,512,true};
        default: return {};
        }
    }
    void setNumber(int index, double number) {
        auto range = numeric(index);
        if (!range || !std::isfinite(number) || number < range->minimum || number > range->maximum
            || (range->integer && std::trunc(number) != number)) throw std::invalid_argument("Invalid shape value");
        if (number == range->value) return;
        auto value = overlay::shapes::find(*editing);
        if (!value) throw std::out_of_range("Shape no longer exists");
        auto control = rows.at(index).control;
        if (auto spec = std::get_if<overlay::ShapeSpec>(&value->geometry)) {
            switch (control) {
            case Control::X: spec->center.x=number; break;
            case Control::Y: spec->center.y=number; break;
            case Control::Z: spec->center.z=number; break;
            case Control::Radius: spec->radius=number; break;
            case Control::Height: spec->height=static_cast<int>(number); break;
            default: break;
            }
        } else {
            auto& plane = std::get<overlay::PlaneSpec>(value->geometry);
            auto integer = static_cast<int>(number);
            switch (control) {
            case Control::X: plane.origin.x=integer; break;
            case Control::Y: plane.origin.y=integer; break;
            case Control::Z: plane.origin.z=integer; break;
            case Control::Width: plane.width=integer; break;
            case Control::Depth: plane.depth=integer; break;
            case Control::Spacing: plane.spacing=integer; break;
            default: break;
            }
        }
        overlay::shapes::edit(*editing,std::move(*value)); refresh();
    }
    void open() { editing.reset(); refresh(); }
    bool isEditing() const { return editing.has_value(); }
    int count() const { return static_cast<int>(rows.size()); }
    std::string label(int index) const { return rows.at(index).text; }
    std::string title() const { return translated(editing ? "shape.editor" : "shape.manager"); }
    std::string subtitle() const { return definition ? "#" + std::to_string(*editing) + " " + definition->name : translated("shape.session"); }
    void refresh() {
        rows.clear();
        definition = editing ? overlay::shapes::find(*editing) : std::nullopt;
        if (!definition) editing.reset();
        row(Control::Back, translated(editing ? "shape.backList" : "shape.backSettings"));
        if (!editing) {
            row(Control::Sphere, translated("shape.addSphere"));
            row(Control::Circle, translated("shape.addCircle"));
            row(Control::Cylinder, translated("shape.addCylinder"));
            row(Control::Plane, translated("shape.addPlane"));
            for (auto const& shape : overlay::shapes::list())
                row(Control::Select, translated("shape.entry", "#" + std::to_string(shape.id) + " " + shape.definition.name,
                    translated(shape.definition.visible ? "on" : "off"), shape.definition.dimension), shape.id);
        } else {
            row(Control::Name,translated("shape.name",definition->name));
            row(Control::Visible, translated("shape.visible", translated(definition->visible ? "on" : "off")));
            auto point = [&](double x, double y, double z) {
                row(Control::X, translated("shape.x", x)); row(Control::Y, translated("shape.y", y));
                row(Control::Z, translated("shape.z", z));
            };
            if (auto spec = std::get_if<overlay::ShapeSpec>(&definition->geometry)) {
                point(spec->center.x,spec->center.y,spec->center.z);
                row(Control::Radius, translated("shape.radius",spec->radius));
                if (spec->shape == overlay::Shape::Cylinder) row(Control::Height,translated("shape.height",spec->height));
                row(Control::Snap, translated("shape.snap", translated(spec->snap == overlay::Snap::BlockCenter
                    ? "shape.blockCenter" : spec->snap == overlay::Snap::BlockCorner ? "shape.blockCorner" : "shape.noSnap")));
            } else {
                auto const& plane = std::get<overlay::PlaneSpec>(definition->geometry);
                point(plane.origin.x,plane.origin.y,plane.origin.z);
                row(Control::Width,translated("shape.width",plane.width));
                row(Control::Depth,translated("shape.depth",plane.depth));
                row(Control::Spacing,translated("shape.spacing",plane.spacing));
                row(Control::Orientation,translated("shape.plane",plane.plane == overlay::Plane::XZ ? "XZ"
                    : plane.plane == overlay::Plane::XY ? "XY" : "YZ"));
            }
            row(Control::Duplicate,translated("shape.duplicate"));
            row(Control::Remove,translated("shape.remove"));
        }
        row(Control::Back,translated(editing ? "shape.backList" : "shape.backSettings"));
    }
    // True asks the host to return to settings. Edits apply immediately; failed
    // generation leaves the collection unchanged and propagates to its error UI.
    bool activate(int index, int direction, overlay::Point position, int dimension) {
        auto item = rows.at(index);
        if (direction != 0 && (item.control == Control::Back || item.control == Control::Select
            || item.control == Control::Duplicate || item.control == Control::Remove || !editing)) return false;
        int step = direction < 0 ? -1 : 1;
        if (item.control == Control::Back) {
            if (!editing) return true;
            open(); return false;
        }
        if (item.control == Control::Select) { editing=item.id; refresh(); return false; }
        if (!editing) {
            overlay::ShapeDefinition value;
            value.dimension = dimension;
            if (item.control == Control::Plane) {
                value.name = translated("shape.planeName");
                value.geometry = overlay::PlaneSpec{{overlay::checkedCoordinate(std::floor(position.x)),
                    overlay::checkedCoordinate(std::floor(position.y)),overlay::checkedCoordinate(std::floor(position.z))}};
            } else {
                auto type = item.control == Control::Sphere ? overlay::Shape::Sphere
                    : item.control == Control::Circle ? overlay::Shape::Circle : overlay::Shape::Cylinder;
                value.name = translated(type == overlay::Shape::Sphere ? "shape.sphereName"
                    : type == overlay::Shape::Circle ? "shape.circleName" : "shape.cylinderName");
                value.geometry = overlay::ShapeSpec{type,position,overlay::Snap::BlockCenter,4,5};
            }
            editing = overlay::shapes::add(std::move(value)); refresh(); return false;
        }
        auto value = overlay::shapes::find(*editing);
        if (!value) { open(); return false; }
        if (item.control == Control::Remove) { overlay::shapes::remove(*editing); open(); return false; }
        if (item.control == Control::Duplicate) { editing = overlay::shapes::add(*value); refresh(); return false; }
        if (item.control == Control::Visible) {
            overlay::shapes::setVisible(*editing,!value->visible); refresh(); return false;
        }
        if (auto spec = std::get_if<overlay::ShapeSpec>(&value->geometry)) {
            switch (item.control) {
            case Control::X: spec->center.x += step; break;
            case Control::Y: spec->center.y += step; break;
            case Control::Z: spec->center.z += step; break;
            case Control::Radius: spec->radius = std::max(0.0,spec->radius + step*.5); break;
            case Control::Height: spec->height = std::clamp(spec->height+step,1,512); break;
            case Control::Snap: spec->snap = static_cast<overlay::Snap>((static_cast<int>(spec->snap)+step+3)%3); break;
            default: break;
            }
        } else {
            auto& plane = std::get<overlay::PlaneSpec>(value->geometry);
            switch (item.control) {
            case Control::X: plane.origin.x = overlay::checkedCoordinate(double(plane.origin.x)+step); break;
            case Control::Y: plane.origin.y = overlay::checkedCoordinate(double(plane.origin.y)+step); break;
            case Control::Z: plane.origin.z = overlay::checkedCoordinate(double(plane.origin.z)+step); break;
            case Control::Width: plane.width = std::clamp(plane.width+step,1,512); break;
            case Control::Depth: plane.depth = std::clamp(plane.depth+step,1,512); break;
            case Control::Spacing: plane.spacing = std::clamp(plane.spacing+step,1,512); break;
            case Control::Orientation: plane.plane = static_cast<overlay::Plane>((static_cast<int>(plane.plane)+step+3)%3); break;
            default: break;
            }
        }
        overlay::shapes::edit(*editing,std::move(*value)); refresh(); return false;
    }
};
}

#pragma once
#include "overlay/Geometry.h"
#include <map>
#include <string>
#include <type_traits>
#include <variant>

namespace lamium::overlay {
struct PlaneSpec {
    Cell origin;
    int width = 9, depth = 9, spacing = 1;
    Plane plane = Plane::XZ;
};
struct ShapeDefinition {
    std::string name;
    int dimension = 0;
    bool visible = true;
    std::variant<ShapeSpec, PlaneSpec> geometry = ShapeSpec{};
};
using ShapeId = uint64_t;
struct ManagedShape {
    ShapeDefinition definition;
    std::vector<Line> lines;
};

// A world-session collection. The owner must clear it on world exit, even when
// the next world's dimension has the same numeric ID. No game pointers live here.
// Mutations belong outside drawing; readers borrow immutable cached geometry.
class ShapeCollection {
    std::map<ShapeId, ManagedShape> shapes;
    ShapeId nextId = 1;
    size_t totalLines = 0;
    size_t maximumShapes, maximumLines;

    ManagedShape prepare(ShapeDefinition definition, size_t replacedLines = 0) const {
        if (definition.name.empty() || definition.name.size() > 128)
            throw std::invalid_argument("Shape name must contain 1 to 128 bytes");
        auto cells = std::visit([](auto const& spec) {
            using Spec = std::decay_t<decltype(spec)>;
            if constexpr (std::is_same_v<Spec, ShapeSpec>) {
                if (spec.shape != Shape::Circle && spec.shape != Shape::Cylinder && spec.shape != Shape::Sphere)
                    throw std::invalid_argument("Unknown shape type");
                if (spec.snap != Snap::BlockCenter && spec.snap != Snap::BlockCorner && spec.snap != Snap::Off)
                    throw std::invalid_argument("Unknown shape snapping");
                return rasterize(spec);
            } else {
                if (spec.plane != Plane::XZ && spec.plane != Plane::XY && spec.plane != Plane::YZ)
                    throw std::invalid_argument("Unknown plane orientation");
                return gridPlane(spec.origin, spec.width, spec.depth, spec.spacing, spec.plane);
            }
        }, definition.geometry);
        auto lines = gridSurfaceLines(cells, maximumLines - (totalLines - replacedLines));
        return {std::move(definition), std::move(lines)};
    }
public:
    explicit ShapeCollection(size_t shapeLimit = 32, size_t lineLimit = 200000)
        : maximumShapes(shapeLimit), maximumLines(lineLimit) {}
    auto const& entries() const { return shapes; }
    ManagedShape const* find(ShapeId id) const {
        auto found = shapes.find(id);
        return found == shapes.end() ? nullptr : &found->second;
    }
    ShapeId add(ShapeDefinition definition) {
        if (shapes.size() >= maximumShapes || nextId == std::numeric_limits<ShapeId>::max())
            throw std::length_error("Shape collection is full");
        auto prepared = prepare(std::move(definition));
        auto count = prepared.lines.size();
        auto id = nextId;
        shapes.emplace(id, std::move(prepared));
        ++nextId;
        totalLines += count;
        return id;
    }
    void edit(ShapeId id, ShapeDefinition definition) {
        auto& existing = shapes.at(id);
        auto prepared = prepare(std::move(definition), existing.lines.size());
        auto count = totalLines - existing.lines.size() + prepared.lines.size();
        std::swap(existing, prepared);
        totalLines = count;
    }
    void setVisible(ShapeId id, bool visible) { shapes.at(id).definition.visible = visible; }
    bool remove(ShapeId id) {
        auto found = shapes.find(id);
        if (found == shapes.end()) return false;
        totalLines -= found->second.lines.size();
        shapes.erase(found);
        return true;
    }
    void clear() { shapes.clear(); totalLines = 0; }
    template<class Draw> void forVisible(int dimension, Draw draw) const {
        for (auto const& [id, shape] : shapes)
            if (shape.definition.visible && shape.definition.dimension == dimension) draw(id, shape);
    }
};
}

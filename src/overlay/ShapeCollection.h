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
// How a shape is drawn. Faces are translucent block faces with a faint outline;
// lines are the block edges only. Further styles may be appended.
enum class ShapeStyle { Face, Line };
enum class ShapeColor { Cyan, Yellow, Pink, White };
struct ShapeDefinition {
    std::string name;
    int dimension = 0;
    bool visible = true;
    std::variant<ShapeSpec, PlaneSpec> geometry = ShapeSpec{};
    ShapeStyle style = ShapeStyle::Face;
    ShapeColor color = ShapeColor::Cyan;
};
using ShapeId = uint64_t;
struct ManagedShape {
    ShapeDefinition definition;
    std::vector<Line> lines;
    std::vector<CellFace> faces;
    // Changes whenever the geometry or its appearance is regenerated, so a
    // renderer can keep uploaded meshes until then.
    uint64_t revision = 0;
};
// Display blocks for any definition: rings, sphere volume or plane grid.
inline std::set<Cell> shapeCells(ShapeDefinition const& definition) {
    return std::visit([](auto const& spec) {
        using Spec = std::decay_t<decltype(spec)>;
        if constexpr (std::is_same_v<Spec, ShapeSpec>) {
            if (spec.shape != Shape::Circle && spec.shape != Shape::Cylinder && spec.shape != Shape::Sphere)
                throw std::invalid_argument("Unknown shape type");
            if (spec.snap != Snap::BlockCenter && spec.snap != Snap::BlockCorner && spec.snap != Snap::Off)
                throw std::invalid_argument("Unknown shape snapping");
            std::set<Cell> cells;
            for (auto const& face : roundFaces(spec)) cells.insert(face.cell);
            return cells;
        } else {
            if (spec.plane != Plane::XZ && spec.plane != Plane::XY && spec.plane != Plane::YZ)
                throw std::invalid_argument("Unknown plane orientation");
            return gridPlane(spec.origin, spec.width, spec.depth, spec.spacing, spec.plane);
        }
    }, definition.geometry);
}

// A world-session collection. The owner must clear it on world exit, even when
// the next world's dimension has the same numeric ID. No game pointers live here.
// Mutations belong outside drawing; readers borrow immutable cached geometry.
class ShapeCollection {
    std::map<ShapeId, ManagedShape> shapes;
    ShapeId nextId = 1;
    uint64_t nextRevision = 1;
    size_t totalLines = 0;
    size_t maximumShapes, maximumLines;

    static void validateName(std::string const& name) {
        if (name.empty() || name.size() > 128 || name.find_first_not_of(' ') == std::string::npos)
            throw std::invalid_argument("Shape name must contain 1 to 128 bytes of visible text");
        for (unsigned char c : name) if (c < 32 || c == 127)
            throw std::invalid_argument("Shape name cannot contain control characters");
    }

    ManagedShape prepare(ShapeDefinition definition, size_t replacedLines = 0) {
        validateName(definition.name);
        if (definition.style != ShapeStyle::Face && definition.style != ShapeStyle::Line)
            throw std::invalid_argument("Unknown shape style");
        if (static_cast<unsigned>(definition.color) > static_cast<unsigned>(ShapeColor::White))
            throw std::invalid_argument("Unknown shape color");
        size_t available = maximumLines - (totalLines - replacedLines);
        std::vector<Line> lines;
        std::vector<CellFace> faces;
        if (auto spec = std::get_if<ShapeSpec>(&definition.geometry)) {
            // Round shapes come straight from their columns, never the volume.
            if (spec->shape != Shape::Circle && spec->shape != Shape::Cylinder && spec->shape != Shape::Sphere)
                throw std::invalid_argument("Unknown shape type");
            if (spec->snap != Snap::BlockCenter && spec->snap != Snap::BlockCorner && spec->snap != Snap::Off)
                throw std::invalid_argument("Unknown shape snapping");
            faces = roundFaces(*spec, available);
            lines = faceLines(faces, available);
        } else {
            auto cells = shapeCells(definition);
            lines = gridSurfaceLines(cells, available);
            faces = boundaryFaces(cells);
        }
        return {std::move(definition), std::move(lines), std::move(faces), nextRevision++};
    }
public:
    explicit ShapeCollection(size_t shapeLimit = 32, size_t lineLimit = 4000000)
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
    void rename(ShapeId id, std::string name) {
        validateName(name);
        shapes.at(id).definition.name = std::move(name);
    }
    bool remove(ShapeId id) {
        auto found = shapes.find(id);
        if (found == shapes.end()) return false;
        totalLines -= found->second.lines.size();
        shapes.erase(found);
        return true;
    }
    void clear() { shapes.clear(); totalLines = 0; }
    void replace(std::vector<ShapeDefinition> definitions) {
        ShapeCollection replacement(maximumShapes,maximumLines);
        replacement.nextId = nextId;
        replacement.nextRevision = nextRevision;
        for (auto& definition : definitions) replacement.add(std::move(definition));
        shapes.swap(replacement.shapes);
        nextId = replacement.nextId;
        nextRevision = replacement.nextRevision;
        totalLines = replacement.totalLines;
    }
    template<class Draw> void forVisible(int dimension, Draw draw) const {
        for (auto const& [id, shape] : shapes)
            if (shape.definition.visible && shape.definition.dimension == dimension) draw(id, shape);
    }
};
}

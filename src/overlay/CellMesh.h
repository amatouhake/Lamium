#pragma once
// Uploading and drawing a cell overlay (BACKLOG L-138): the faces and
// outline from CellSurface.h with the overlay face material, colored and
// pulled toward the eye by the depth rules (Depth.h). Shapes, the breaking
// restriction and the schematic save area draw through it; each keeps its
// own colors and decides when it is shown.
#include "overlay/CellSurface.h"
#include "overlay/Depth.h"
#include "overlay/FaceMaterial.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include <glm/vec3.hpp>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
class ScreenContext;
class Vec3;
namespace lamium::overlay {
// How an outline is colored: by its vertices on the debug material (fades
// with alpha; Vibrant Visuals draws it black), or by the shader color on the
// block selection material (keeps its color there; ignores alpha).
enum class LineColoring { Vertex, Shader };
struct CellStyle {
    float r = 1, g = 1, b = 1;
    bool faces = true; // false: outline only
    LineColoring lines = LineColoring::Vertex;
    bool operator==(CellStyle const&) const = default;
};
// A cell overlay uploaded once, relative to `origin`, and reused while the
// camera moves. Owned by the render thread.
struct CellMesh {
    std::uint64_t key = 0; // what it was built from (the caller's revision)
    Cell origin{};
    std::optional<mce::Mesh> faces, lines;
    std::uint32_t faceVertices = 0, lineVertices = 0;
    int variant = -1; // the FaceMaterial variant it was built for
    CellStyle style{};
    // Built from something else, for another material, or its upload was lost.
    bool stale(std::uint64_t key, CellStyle const& style, FaceMaterial const& material) const;
    // Drops the uploads (meshes cannot be assigned); the next draw rebuilds.
    void release() {
        faces.reset(); lines.reset();
        faceVertices = lineVertices = 0;
        key = 0;
        variant = -1;
    }
};
void buildCellMesh(ScreenContext& screen, CellMesh& mesh, std::span<CellFace const> faces, std::span<Line const> lines,
                   CellStyle const& style, FaceMaterial const& material, std::uint64_t key);
// Draws the faces, then the outline, pulled toward the eye by `pull`.
void drawCellMesh(ScreenContext& screen, Vec3 const& camera, CellMesh const& mesh, FaceMaterial const& material,
                  float pull = depth::facePull);
// Runs `draw` with the world matrix moved to `offset` (camera-relative) and
// scaled toward the eye by `pull` (Depth.h rule 2); popped again also when
// the draw throws.
void drawPulled(ScreenContext& screen, glm::vec3 offset, float pull, std::function<void()> const& draw);
}

#include "overlay/CellMesh.h"
#include "overlay/LineColor.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/client/renderer/SupplementaryFieldAutoGenerationMode.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/core_graphics/enums/PrimitiveMode.h"
#include "mc/deps/minecraft_renderer/framebuilder/dragon/RenderMetadata.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureDescription.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/deps/renderer/Camera.h"
#include "mc/deps/renderer/MatrixStack.h"
#include <glm/gtc/matrix_transform.hpp>

namespace lamium::overlay {
bool CellMesh::stale(std::uint64_t wanted, CellStyle const& wantedStyle, FaceMaterial const& material) const {
    return key != wanted || style != wantedStyle || variant != material.variant || (faces && !faces->isValid())
        || (lines && !lines->isValid());
}
void buildCellMesh(ScreenContext& screen, CellMesh& mesh, std::span<CellFace const> faces, std::span<Line const> lines,
                   CellStyle const& style, FaceMaterial const& material, std::uint64_t key) {
    mesh.faces.reset(); mesh.lines.reset();
    mesh.faceVertices = mesh.lineVertices = 0;
    mesh.key = key;
    mesh.style = style;
    mesh.variant = material.variant;
    mesh.origin = surfaceOrigin(faces, lines);
    auto relative = [&](Tessellator& batch, Point p) {
        batch.vertex(static_cast<float>(p.x - mesh.origin.x), static_cast<float>(p.y - mesh.origin.y),
            static_cast<float>(p.z - mesh.origin.z));
    };
    if (style.faces && !faces.empty()) {
        // On the cells' own planes: the pull toward the eye keeps them in
        // front of the block faces there (depth rules, Depth.h).
        auto corners = faceCorners(faces, material.twoSided);
        Tessellator batch(screen.tessellator.mBufferResourceService);
        batch.begin({}, mce::PrimitiveMode::QuadList, static_cast<int>(corners.size()), false);
        batch.color(style.r, style.g, style.b, material.alpha);
        for (auto p : corners) relative(batch, p);
        mesh.faces.emplace(batch.end(Tessellator::UploadMode::Buffered, "Lamium cell faces", SupplementaryFieldAutoGenerationMode{}));
        mesh.faceVertices = static_cast<std::uint32_t>(corners.size());
    }
    if (!lines.empty()) {
        Tessellator batch(screen.tessellator.mBufferResourceService);
        batch.begin({}, mce::PrimitiveMode::LineList, static_cast<int>(lines.size() * 2), false);
        // Written for the vertex-colored material and as the fallback of the shader-colored one.
        batch.color(style.r, style.g, style.b, outlineAlpha(style.faces, material.strongLines));
        for (auto const& line : lines) { relative(batch, line.from); relative(batch, line.to); }
        mesh.lines.emplace(batch.end(Tessellator::UploadMode::Buffered, "Lamium cell lines", SupplementaryFieldAutoGenerationMode{}));
        mesh.lineVertices = static_cast<std::uint32_t>(lines.size() * 2);
    }
}
void drawCellMesh(ScreenContext& screen, Vec3 const& camera, CellMesh const& mesh, FaceMaterial const& material, float pull) {
    if (!mesh.faces && !mesh.lines) return;
    bool shaded = mesh.style.lines == LineColoring::Shader;
    mce::MaterialPtr lineMaterial = shaded ? lines::material() : mce::MaterialPtr(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    glm::vec3 offset{static_cast<float>(mesh.origin.x - camera.x), static_cast<float>(mesh.origin.y - camera.y),
                     static_cast<float>(mesh.origin.z - camera.z)};
    drawPulled(screen, offset, pull, [&] {
        if (mesh.faces && material.material.mRenderMaterialInfoPtr)
            mesh.faces->renderMesh(screen, material.material, gsl::span<mce::ClientTexture const*>{}, 0, mesh.faceVertices,
                OffscreenCaptureDescription{}, nullptr);
        if (!mesh.lines || !lineMaterial.mRenderMaterialInfoPtr) return;
        auto draw = [&] {
            mesh.lines->renderMesh(screen, lineMaterial, gsl::span<mce::ClientTexture const*>{}, 0, mesh.lineVertices,
                OffscreenCaptureDescription{}, nullptr);
        };
        if (shaded) lines::colored(screen, mesh.style.r, mesh.style.g, mesh.style.b, draw);
        else draw();
    });
}
void drawPulled(ScreenContext& screen, glm::vec3 offset, float pull, std::function<void()> const& draw) {
    auto ref = screen.camera.worldMatrixStack->push(false);
    ref.stack->_isDirty = true;
    ref.mat->_m = glm::scale(glm::translate(ref.mat->_m.get(), offset * pull), glm::vec3{pull});
    // Pop manually, the pattern proven for this stack, also when the draw
    // throws: a pushed matrix left behind would shift the whole world.
    auto pop = [&] {
        ref.stack->_isDirty = true;
        if (ref.stack->sortOrigin->has_value() && (ref.stack->stack->size() - 1) <= ref.stack->sortOrigin->value())
            ref.stack->sortOrigin->reset();
        ref.stack->stack->pop_back();
        ref.mat = nullptr;
        ref.stack = nullptr;
    };
    try { draw(); } catch (...) { pop(); throw; }
    pop();
}
}

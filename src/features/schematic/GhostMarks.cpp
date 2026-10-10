#include "features/schematic/GhostMarks.h"
#include "features/schematic/AreaSave.h"
#include "features/schematic/EntityModels.h"
#include "features/schematic/GhostRenderer.h"
#include "overlay/CellMesh.h"
#include "overlay/LineColor.h"
#include "features/schematic/Selection.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/FontRepository.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/BaseActorRenderer.h"
#include "mc/client/renderer/SupplementaryFieldAutoGenerationMode.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/deps/minecraft_renderer/framebuilder/dragon/RenderMetadata.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/common/client/renderer/helpers/MeshHelpers.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core_graphics/enums/PrimitiveMode.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureDescription.h"
#include "mc/deps/renderer/Camera.h"
#include "mc/deps/renderer/MatrixStack.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/phys/HitResult.h"
#include "mc/world/level/ShapeType.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <format>
#include <mutex>
#include <set>

namespace lamium::schematic::ghosts {
namespace {
// The dashed frame of an entity without a model: mob-sized, except for
// dropped items and experience orbs, whose real box is a quarter block.
struct FrameSize { float width, height; };
FrameSize entityFrame(std::string_view identifier) {
    if (identifier == "minecraft:item" || identifier == "minecraft:xp_orb") return {.25f, .25f};
    return {.8f, 1.8f};
}
constexpr float nameTagScale = .025f; // blocks per font pixel
// Names over missing entities' frames, collected with the frames and drawn
// as name tags in the same pass.
std::vector<std::pair<Position, std::string>> labels;
// "Show in world": a marked cell until `pointUntil`.
std::mutex pointMutex;
std::optional<Point> pointAt;
Clock::time_point pointUntil{};
// The placement frames as one mesh, built again only when the placements,
// the selection or the dimension change: built each frame, the dashed
// frames of large placements took about 1.3 ms (C, measured). Vertices are
// relative to `anchor`, a placement corner, for float precision.
struct FrameMesh {
    std::optional<mce::Mesh> mesh;
    std::uint32_t vertices = 0;
    glm::dvec3 anchor{};
    std::uint64_t revision = 0;
    int selected = -2, dimension = -1;
} frameMesh;
// The save area's outline and corners, rebuilt when the selection changes.
selection::State drawnSelection;
std::uint64_t selectionRevision = 1;
overlay::CellMesh areaMesh, cornerMeshes[2];
}

void translated(ScreenContext& screen, glm::vec3 offset, std::function<void()> const& draw) {
    overlay::drawPulled(screen, offset, towardEye, draw);
}
// The area chosen for saving: a white outline, and corner 1 red and corner
// 2 blue as tinted cells with their outline, drawn over it with a nearer
// pull (Depth.h rule 3) so a full block still shows which corner it is.
void drawSelection(ScreenContext& screen, Vec3 const& camera, int dimension, overlay::FaceMaterial const& material) {
    auto state = selection::current();
    if (state.dimension != dimension || (!state.first && !state.second)) return;
    if (state.first != drawnSelection.first || state.second != drawnSelection.second || state.dimension != drawnSelection.dimension) {
        drawnSelection = state;
        ++selectionRevision;
    }
    Area area = state.area().value_or(Area{state.first ? *state.first : *state.second, state.first ? *state.first : *state.second});
    auto cell = [](Point p) { return overlay::Cell{p.x, p.y, p.z}; };
    Point low = area.low();
    Size size = area.size();
    overlay::CellStyle white{1.f, 1.f, 1.f, false, overlay::LineColoring::Shader};
    if (areaMesh.stale(selectionRevision, white, material)) {
        auto outline = overlay::boxOutline(cell(low), {low.x + size.x - 1, low.y + size.y - 1, low.z + size.z - 1});
        overlay::buildCellMesh(screen, areaMesh, {}, outline, white, material, selectionRevision);
    }
    overlay::drawCellMesh(screen, camera, areaMesh, material, overlay::depth::facePull);
    constexpr float cornerColors[2][3]{{1.f, .25f, .2f}, {.25f, .5f, 1.f}};
    for (int i = 0; i < 2; ++i) {
        auto const& corner = i == 0 ? state.first : state.second;
        if (!corner) continue;
        overlay::CellStyle style{cornerColors[i][0], cornerColors[i][1], cornerColors[i][2], true, overlay::LineColoring::Shader};
        auto& mesh = cornerMeshes[i];
        if (mesh.stale(selectionRevision, style, material)) {
            auto surface = overlay::cellSurface({cell(*corner)});
            overlay::buildCellMesh(screen, mesh, surface.faces, surface.lines, style, material, selectionRevision);
        }
        overlay::drawCellMesh(screen, camera, mesh, material, overlay::depth::markPull);
    }
}

// While a save waits, the chunk columns it still has to read: yellow frames
// standing on the area's floor, nearest first.
void drawWaitingColumns(ScreenContext& screen, Vec3 const& camera, mce::MaterialPtr const& faceMaterial, bool twoSided) {
    auto columns = waitingColumns(camera.x, camera.z, 256);
    auto const& waiting = columns.nearest;
    if (waiting.empty()) return;
    int height = columns.height, lowY = columns.lowY;
    mce::MaterialPtr lineMaterial = overlay::lines::material();
    if (!lineMaterial.mRenderMaterialInfoPtr) return;
    Tessellator lines(screen.tessellator.mBufferResourceService), faces(screen.tessellator.mBufferResourceService);
    lines.begin({}, mce::PrimitiveMode::LineList, static_cast<int>(waiting.size() * 24), false);
    faces.begin({}, mce::PrimitiveMode::QuadList, static_cast<int>(waiting.size() * 8), false);
    lines.color(1.f, .8f, .25f, 1.f);
    faces.color(1.f, .8f, .25f, .18f);
    constexpr int edges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    for (auto const& column : waiting) {
        glm::vec3 a{static_cast<float>(column.lowX - camera.x) + .05f, static_cast<float>(lowY - camera.y),
                    static_cast<float>(column.lowZ - camera.z) + .05f};
        glm::vec3 b{static_cast<float>(column.highX + 1 - camera.x) - .05f, static_cast<float>(lowY + height - camera.y),
                    static_cast<float>(column.highZ + 1 - camera.z) - .05f};
        glm::vec3 c[8];
        for (int k = 0; k < 8; ++k) c[k] = {k & 1 ? b.x : a.x, k & 2 ? b.y : a.y, k & 4 ? b.z : a.z};
        for (auto [p, q] : edges) { lines.vertex(c[p].x, c[p].y, c[p].z); lines.vertex(c[q].x, c[q].y, c[q].z); }
        // The floor, seen from both sides, so the column reads from above too.
        glm::vec3 floor[4]{c[0], c[1], c[5], c[4]};
        for (int k = 0; k < 4; ++k) faces.vertex(floor[k].x, floor[k].y + .02f, floor[k].z);
        if (twoSided) for (int k = 3; k >= 0; --k) faces.vertex(floor[k].x, floor[k].y + .02f, floor[k].z);
    }
    translated(screen, glm::vec3{0}, [&] {
        if (faceMaterial.mRenderMaterialInfoPtr) MeshHelpers::renderMeshImmediately(screen, faces, faceMaterial, OffscreenCaptureDescription{});
        overlay::lines::colored(screen, 1.f, .8f, .25f, [&] { MeshHelpers::renderMeshImmediately(screen, lines, lineMaterial, OffscreenCaptureDescription{}); });
    });
}

// Missing entities: a dashed frame where each should stand, in the ghost
// color, with its name drawn by the HUD. Entities already there show nothing.
// Every placement's box in the ghosts' light blue (L-93 screen review): the
// selected one solid, the others dashed. The line material ignores alpha
// (checked 2026-10-08), so the shape tells them apart.
void drawPlacementFrames(ScreenContext& screen, session::Snapshot const& snapshot, int dimension, Vec3 const& camera) {
    mce::MaterialPtr lineMaterial = overlay::lines::material();
    if (!lineMaterial.mRenderMaterialInfoPtr) return;
    bool stale = !frameMesh.mesh || !frameMesh.mesh->isValid() || frameMesh.revision != snapshot.revision
        || frameMesh.selected != snapshot.selected || frameMesh.dimension != dimension;
    if (stale) {
        frameMesh.mesh.reset();
        frameMesh.vertices = 0;
        frameMesh.revision = snapshot.revision;
        frameMesh.selected = snapshot.selected;
        frameMesh.dimension = dimension;
        constexpr int edges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
        Tessellator lines(screen.tessellator.mBufferResourceService);
        bool begun = false;
        for (size_t i = 0; i < snapshot.placements.size(); ++i) {
            auto const& shown = snapshot.placements[i];
            bool selected = static_cast<int>(i) == snapshot.selected;
            if (!shown.structure || shown.placement.dimension != dimension || (!shown.placement.visible && !selected)) continue;
            Size size = placedSize(shown.structure->size, shown.placement.placement.rotation);
            auto const& o = shown.placement.placement.origin;
            if (!begun) {
                frameMesh.anchor = {o.x, o.y, o.z};
                lines.begin({}, mce::PrimitiveMode::LineList, 4096, false);
                lines.color(.35f, .85f, 1.f, 1.f);
                begun = true;
            }
            glm::vec3 a{static_cast<float>(o.x - frameMesh.anchor.x) - .02f, static_cast<float>(o.y - frameMesh.anchor.y) - .02f,
                        static_cast<float>(o.z - frameMesh.anchor.z) - .02f};
            glm::vec3 b = a + glm::vec3{static_cast<float>(size.x) + .04f, static_cast<float>(size.y) + .04f,
                                        static_cast<float>(size.z) + .04f};
            glm::vec3 c[8];
            for (int k = 0; k < 8; ++k) c[k] = {k & 1 ? b.x : a.x, k & 2 ? b.y : a.y, k & 4 ? b.z : a.z};
            for (auto [p, q] : edges) {
                if (selected) {
                    lines.vertex(c[p].x, c[p].y, c[p].z);
                    lines.vertex(c[q].x, c[q].y, c[q].z);
                    frameMesh.vertices += 2;
                    continue;
                }
                constexpr float dash = .5f, gap = .5f;
                glm::vec3 from = c[p], to = c[q];
                float length = glm::length(to - from);
                glm::vec3 step = (to - from) / length;
                for (float t = 0; t < length; t += dash + gap) {
                    glm::vec3 s0 = from + step * t, s1 = from + step * std::min(length, t + dash);
                    lines.vertex(s0.x, s0.y, s0.z);
                    lines.vertex(s1.x, s1.y, s1.z);
                    frameMesh.vertices += 2;
                }
            }
        }
        if (!begun) return;
        auto mesh = lines.end(Tessellator::UploadMode::Buffered, "Lamium schematic frames", SupplementaryFieldAutoGenerationMode{});
        if (frameMesh.vertices) frameMesh.mesh.emplace(std::move(mesh));
    }
    if (!frameMesh.mesh) return;
    glm::vec3 offset{static_cast<float>(frameMesh.anchor.x - camera.x), static_cast<float>(frameMesh.anchor.y - camera.y),
                     static_cast<float>(frameMesh.anchor.z - camera.z)};
    translated(screen, offset, [&] {
        overlay::lines::colored(screen, .35f, .85f, 1.f, [&] {
            frameMesh.mesh->renderMesh(screen, lineMaterial, gsl::span<mce::ClientTexture const*>{}, 0, frameMesh.vertices,
                OffscreenCaptureDescription{}, nullptr);
        });
    });
}
// Missing entities: their game model with part outlines (L-115), or a dashed
// frame when the entity has no model.
void drawEntities(ScreenContext& screen, IClientInstance& client, session::Snapshot const& snapshot, int dimension, Vec3 const& camera,
                  std::vector<Resolved> const& resolved) {
    std::vector<models::Spot> spots;
    std::vector<std::pair<size_t, std::string>> names; // spot index, name
    for (size_t i = 0; i < snapshot.placements.size() && i < resolved.size(); ++i) {
        auto const& shown = snapshot.placements[i];
        if (!shown.structure || !shown.placement.visible || !shown.placement.entities || shown.placement.dimension != dimension) continue;
        Size placed = placedSize(shown.structure->size, shown.placement.placement.rotation);
        auto const& r = resolved[i];
        for (size_t e = 0; e < r.entities.size(); ++e) {
            if (r.entityPlaced[e] != false || !layerShown(shown.placement.layers, placed, r.entities[e].offset)) continue;
            auto const& at = r.entities[e].at;
            double dx = at.x - camera.x, dy = at.y - camera.y, dz = at.z - camera.z, distance = dx * dx + dy * dy + dz * dz;
            if (distance > drawDistance * drawDistance) continue;
            spots.push_back({at, r.entities[e].identifier, r.entities[e].yaw});
            if (distance < 32 * 32) names.push_back({spots.size() - 1, r.entities[e].name});
        }
    }
    auto modelled = models::draw(screen, client, camera, spots, [&](std::function<void()> const& draw) { translated(screen, glm::vec3{0}, draw); });
    // A model says what the entity is; only the frames get name tags.
    std::vector<std::pair<Position, std::string>> named;
    for (auto& [index, name] : names) {
        if (modelled[index] || named.size() >= 64) continue;
        auto const& at = spots[index].at;
        named.push_back({{at.x, at.y + entityFrame(spots[index].identifier).height + .3, at.z}, std::move(name)});
    }
    labels = std::move(named);
    std::vector<std::pair<Position, FrameSize>> frames;
    for (size_t i = 0; i < spots.size(); ++i)
        if (!modelled[i]) frames.push_back({spots[i].at, entityFrame(spots[i].identifier)});
    mce::MaterialPtr lineMaterial = overlay::lines::material();
    if (frames.empty() || !lineMaterial.mRenderMaterialInfoPtr) return;
    // Each edge as dashes. The frame does not claim the entity's real size,
    // which the client cannot know without the entity.
    constexpr int edges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    Tessellator lines(screen.tessellator.mBufferResourceService);
    lines.begin({}, mce::PrimitiveMode::LineList, static_cast<int>(frames.size() * 12 * 12), false);
    lines.color(.35f, .85f, 1.f, 1.f);
    for (auto const& [at, size] : frames) {
        // Shorter dashes on small frames so every edge still shows a dash.
        float dash = size.height < 1 ? .06f : .2f, gap = size.height < 1 ? .04f : .15f, half = size.width / 2;
        glm::vec3 base{static_cast<float>(at.x - camera.x), static_cast<float>(at.y - camera.y), static_cast<float>(at.z - camera.z)};
        glm::vec3 a = base + glm::vec3{-half, 0, -half}, b = base + glm::vec3{half, size.height, half}, c[8];
        for (int k = 0; k < 8; ++k) c[k] = {k & 1 ? b.x : a.x, k & 2 ? b.y : a.y, k & 4 ? b.z : a.z};
        for (auto [p, q] : edges) {
            glm::vec3 from = c[p], to = c[q];
            float length = glm::length(to - from);
            glm::vec3 step = (to - from) / length;
            for (float t = 0; t < length; t += dash + gap) {
                glm::vec3 s0 = from + step * t, s1 = from + step * std::min(length, t + dash);
                lines.vertex(s0.x, s0.y, s0.z);
                lines.vertex(s1.x, s1.y, s1.z);
            }
        }
    }
    translated(screen, glm::vec3{0}, [&] {
        overlay::lines::colored(screen, .35f, .85f, 1.f, [&] { MeshHelpers::renderMeshImmediately(screen, lines, lineMaterial, OffscreenCaptureDescription{}); });
    });
}

// The names of missing entities like a named entity's tag: a dark plate with
// the text, facing the camera, a fixed size in the world. Drawn with the
// game's name tag materials (the both-sides variants).
void drawNameTags(ScreenContext& screen, IClientInstance& client, BlockSource& region, BaseActorRenderer& renderer, Vec3 const& camera) {
    if (labels.empty()) return;
    auto const& backgroundMaterial = renderer.mNameTagBackgroundWithBackfaceMat.get();
    auto const& textMaterial = renderer.mNameTagTextWithBackfaceMat.get();
    if (!backgroundMaterial.mRenderMaterialInfoPtr || !textMaterial.mRenderMaterialInfoPtr) return;
    if (screen.camera.viewMatrixStack->stack->empty()) return;
    auto view = *screen.camera.viewMatrixStack->top()._m;
    // The view's rotation rows are the camera axes.
    glm::vec3 right{view[0][0], view[1][0], view[2][0]}, up{view[0][1], view[1][1], view[2][1]};
    glm::vec3 across = glm::cross(right, -up);
    auto& font = client.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default").getFont();
    auto background = BaseActorRenderer::NAME_TAG_BACKGROUND_COLOR();
    // Smooth fonts (Japanese, Chinese) keep their own material, which reads
    // text constants the name tag material does not set: without them the
    // glyphs got colored fringes (L-117).
    bool smoothFont = !font.materialCanBeOverridden();
    for (auto const& [at, name] : labels) {
        // Not through walls: a block between the camera and the tag hides it.
        Vec3 to{static_cast<float>(at.x), static_cast<float>(at.y), static_cast<float>(at.z)};
        auto hit = region.clip(camera, to, false, ShapeType::Outline, 64, false, false, nullptr,
            [](BlockSource const&, Block const&, bool) { return true; }, false);
        if (hit.mType == HitResultType::Tile) continue;
        float width = static_cast<float>(font.getLineLength(name, 1.f, false));
        // The glyph sheet of the first character past ASCII: its sheet may be
        // a smooth (multi-channel) one whose material must not be replaced.
        int sheet = 0, first = 0;
        for (size_t i = 0; i < name.size();) {
            auto c = static_cast<unsigned char>(name[i]);
            int length = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
            if (c >= 0x80 && i + length <= name.size()) {
                first = c < 0xE0 ? (c & 0x1F) : c < 0xF0 ? (c & 0x0F) : (c & 0x07);
                for (int k = 1; k < length; ++k) first = (first << 6) | (static_cast<unsigned char>(name[i + k]) & 0x3F);
                sheet = first >> 8;
                break;
            }
            i += length;
        }
        bool smooth = smoothFont || sheet != 0;
        // Glyphs of scaled sheets (Japanese: 1.333) are measured at their
        // sheet's scale but drawn at 1, so the plate came out too wide.
        if (sheet != 0) {
            width = 0;
            for (size_t i = 0; i < name.size();) {
                auto c = static_cast<unsigned char>(name[i]);
                size_t length = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
                length = std::min(length, name.size() - i);
                int point = length == 1 ? c : length == 2 ? (c & 0x1F) : length == 3 ? (c & 0x0F) : (c & 0x07);
                for (size_t k = 1; k < length; ++k) point = (point << 6) | (static_cast<unsigned char>(name[i + k]) & 0x3F);
                float scale = font.getScaleFactor(point);
                width += static_cast<float>(font.getLineLength(std::string_view(name).substr(i, length), 1.f, false)) / (scale > 0 ? scale : 1.f);
                i += length;
            }
        }
        static std::set<int> described;
        if (described.insert(sheet).second) {
            auto shift = font.getTranslationFactor();
            log(std::format("name tags: sheet {} type {} overridable {} scale {:.3f} / char {:.3f}, shift {:.2f},{:.2f}, \"{}\" {} px", sheet,
                static_cast<int>(font.getType(sheet)), font.materialCanBeOverridden(), font.getScaleFactor(), font.getScaleFactor(first), shift.x, shift.y,
                name, width));
        }
        glm::vec3 offset{static_cast<float>(at.x - camera.x), static_cast<float>(at.y - camera.y), static_cast<float>(at.z - camera.z)};
        // Font pixels: x to the camera's right, y downward.
        glm::mat4 model{glm::vec4(right * nameTagScale, 0), glm::vec4(-up * nameTagScale, 0), glm::vec4(across * nameTagScale, 0),
                        glm::vec4(offset, 1)};
        auto ref = screen.camera.worldMatrixStack->push(false);
        ref.stack->_isDirty = true;
        ref.mat->_m = ref.mat->_m.get() * model;
        Tessellator plate(screen.tessellator.mBufferResourceService);
        plate.begin({}, mce::PrimitiveMode::QuadList, 8, false);
        plate.color(background.r, background.g, background.b, background.a);
        float x0 = -width / 2 - 1, x1 = width / 2 + 1, y0 = -1, y1 = 8;
        glm::vec2 quad[4]{{x0, y0}, {x0, y1}, {x1, y1}, {x1, y0}};
        for (int k = 0; k < 4; ++k) plate.vertex(quad[k].x, quad[k].y, .01f);
        for (int k = 3; k >= 0; --k) plate.vertex(quad[k].x, quad[k].y, .01f);
        MeshHelpers::renderMeshImmediately(screen, plate, backgroundMaterial, OffscreenCaptureDescription{});
        mce::Color white{1.f, 1.f, 1.f, 1.f}, black{0.f, 0.f, 0.f, 1.f};
        // The smooth sheet's edge softness follows the on-screen size of a
        // font pixel (the UI passes its GUI scale): about nameTagScale times
        // 1080 px over the view height at that distance (70 degrees). At 1
        // the edges spread over the glyphs and darkened them.
        float onScreen = nameTagScale * 1080.f / (2 * std::max(glm::length(offset), .5f) * .7f);
        if (smooth) font.setTextConstantsInScreenContext(screen, sheet, std::clamp(onScreen, .5f, 8.f), white, false);
        font.drawCached(screen, name, -width / 2, 0, white, false, false, false, smooth ? nullptr : &textMaterial, -1, false, 0, white, black, 0, 0,
            OffscreenCaptureDescription{}, false);
        ref.stack->_isDirty = true;
        if (ref.stack->sortOrigin->has_value() && (ref.stack->stack->size() - 1) <= ref.stack->sortOrigin->value())
            ref.stack->sortOrigin->reset();
        ref.stack->stack->pop_back();
        ref.mat = nullptr;
        ref.stack = nullptr;
    }
}
// The cell chosen with "Show in world": a pulsing tinted box with outlines
// and a tall beam of crossed faces above it, readable from far away.
void drawPoint(ScreenContext& screen, Vec3 const& camera, mce::MaterialPtr const& faceMaterial, bool twoSided) {
    std::optional<Point> at;
    {
        std::lock_guard lock(pointMutex);
        if (pointAt && Clock::now() > pointUntil) pointAt.reset();
        at = pointAt;
    }
    if (!at) return;
    mce::MaterialPtr lineMaterial = overlay::lines::material();
    float pulse = .5f + .5f * std::sin(std::chrono::duration<float>(Clock::now().time_since_epoch()).count() * 6.f);
    glm::vec3 offset{static_cast<float>(at->x - camera.x), static_cast<float>(at->y - camera.y), static_cast<float>(at->z - camera.z)};
    constexpr float grow = .04f, beam = 64.f, half = .12f;
    if (faceMaterial.mRenderMaterialInfoPtr) {
        Tessellator faces(screen.tessellator.mBufferResourceService);
        faces.begin({}, mce::PrimitiveMode::QuadList, 48 + 16, false);
        faces.color(1.f, 1.f, 1.f, .25f + .3f * pulse);
        glm::vec3 a{-grow}, b{1 + grow}, c[8];
        for (int i = 0; i < 8; ++i) c[i] = {i & 1 ? b.x : a.x, i & 2 ? b.y : a.y, i & 4 ? b.z : a.z};
        constexpr int sides[6][4] = {{0,2,6,4},{1,5,7,3},{0,4,5,1},{2,3,7,6},{0,1,3,2},{4,6,7,5}};
        for (auto const& side : sides) {
            for (int k = 0; k < 4; ++k) faces.vertex(c[side[k]].x, c[side[k]].y, c[side[k]].z);
            if (twoSided) for (int k = 3; k >= 0; --k) faces.vertex(c[side[k]].x, c[side[k]].y, c[side[k]].z);
        }
        faces.color(1.f, 1.f, 1.f, .45f);
        // Two crossed faces make the beam visible from every side.
        glm::vec3 beamQuads[2][4] = {{{.5f - half, 1, .5f}, {.5f + half, 1, .5f}, {.5f + half, beam, .5f}, {.5f - half, beam, .5f}},
                                     {{.5f, 1, .5f - half}, {.5f, 1, .5f + half}, {.5f, beam, .5f + half}, {.5f, beam, .5f - half}}};
        for (auto const& quad : beamQuads) {
            for (int k = 0; k < 4; ++k) faces.vertex(quad[k].x, quad[k].y, quad[k].z);
            if (twoSided) for (int k = 3; k >= 0; --k) faces.vertex(quad[k].x, quad[k].y, quad[k].z);
        }
        translated(screen, offset, [&] { MeshHelpers::renderMeshImmediately(screen, faces, faceMaterial, OffscreenCaptureDescription{}); });
    }
    if (!lineMaterial.mRenderMaterialInfoPtr) return;
    Tessellator lines(screen.tessellator.mBufferResourceService);
    lines.begin({}, mce::PrimitiveMode::LineList, 26, false);
    lines.color(1.f, 1.f, 1.f, 1.f);
    glm::vec3 a{-grow}, b{1 + grow}, c[8];
    for (int i = 0; i < 8; ++i) c[i] = {i & 1 ? b.x : a.x, i & 2 ? b.y : a.y, i & 4 ? b.z : a.z};
    constexpr int edges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    for (auto [i, j] : edges) { lines.vertex(c[i].x, c[i].y, c[i].z); lines.vertex(c[j].x, c[j].y, c[j].z); }
    lines.vertex(.5f, 1.f, .5f);
    lines.vertex(.5f, beam, .5f);
    translated(screen, offset, [&] {
        overlay::lines::colored(screen, 1.f, 1.f, 1.f, [&] { MeshHelpers::renderMeshImmediately(screen, lines, lineMaterial, OffscreenCaptureDescription{}); });
    });
}
void point(Point cell) {
    std::lock_guard lock(pointMutex);
    pointAt = cell;
    pointUntil = Clock::now() + std::chrono::seconds(30);
}
void resetMarks() {
    frameMesh.mesh.reset();
    areaMesh.release();
    for (auto& mesh : cornerMeshes) mesh.release();
    labels.clear();
}
}

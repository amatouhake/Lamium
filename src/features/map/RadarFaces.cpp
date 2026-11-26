#include "features/map/RadarFaces.h"
#include "features/map/SkinGeometry.h"
#include "features/visuals/PackImage.h"
#include "app/Runtime.h"
#include "ui/Widgets.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/container/Blob.h"
#include "mc/deps/core/image/Image.h"
#include "mc/client/model/geom/Cube.h"
#include "mc/client/model/geom/ModelPart.h"
#include "mc/client/model/models/Model.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/client/renderer/TextureGroup.h"
#include "mc/client/renderer/actor/ActorRenderDispatcher.h"
#include "mc/client/renderer/actor/DataDrivenRenderer.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core_graphics/ImageBuffer.h"
#include "mc/deps/core_graphics/ImageDescription.h"
#include "mc/deps/core_graphics/enums/TextureFormat.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorFlags.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/player/SerializedSkinImpl.h"
#include "mc/world/actor/player/SerializedSkinRef.h"
#include "mc/util/ThreadOwner.h"
#include "mc/deps/json/Value.h"
#include "mc/world/actor/player/AnimatedImageData.h"
#include "mc/world/actor/player/persona/AnimatedTextureType.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace lamium::map::faces {
namespace {
std::unordered_map<std::string, int> byRenderer; // -1: this kind has no face.
std::vector<Face> found;
// Every face in one runtime texture, uploaded again when one is added.
std::vector<std::uint32_t> atlas;
bool atlasDirty = false, atlasUploaded = false;
ResourceLocation const& atlasLocation() {
    // Never destroyed: its destructor is game code, which must not run while
    // the process tears down after the game.
    static auto const* location = new ResourceLocation(Core::PathView("lamium/radar-faces"), ResourceFileSystem::Raw);
    return *location;
}
bool upload(IClientInstance& client) {
    auto group = client.getTextureGroup();
    if (!group) return false;
    mce::Image image(faceAtlasSide, faceAtlasSide, mce::ImageFormat::RGBA8Unorm, mce::ImageUsage::SRGB);
    image.mAlphaUsage = mce::AlphaUsage::Transparent;
    image.setRawImage(mce::Blob(reinterpret_cast<std::uint8_t const*>(atlas.data()), atlas.size() * sizeof(std::uint32_t)));
    cg::ImageBuffer buffer(std::move(image));
    if (atlasUploaded) return group->updateTextureInPlace(atlasLocation(), std::move(buffer));
    group->uploadTexture(atlasLocation(), std::move(buffer));
    return true;
}
// New kinds looked up per frame; each may load a texture.
constexpr int loadsPerFrame = 2;
int loadsLeft = 0;
int logged = 0;
void log(std::string const& text) {
    if (++logged > 12) return;
    try { Runtime::instance().self().getLogger().info("Radar faces: {}", text); } catch (...) {}
}
// The parts a kind's face is seen from (faceRecipe), as boxes with all six
// faces and the turns that place them. Cube origins and pivots are model
// coordinates, y up (checked in game 2026-10-02). A part keeps its pivot as
// its offset from its parent's, y the other way, and a root's from y 24
// (matched against the game's model files 2026-11-26, L-330).
Vec3d pivotOf(ModelPart const& part, int depth = 0) {
    auto const& pos = *part.mPos;
    if (!part.mParent || depth > 16) return {pos.x, 24 - pos.y, pos.z};
    Vec3d const parent = pivotOf(*part.mParent, depth + 1);
    return {parent.x + pos.x, parent.y - pos.y, parent.z + pos.z};
}
std::vector<Turn> partTurns(ModelPart const& part) {
    std::vector<Turn> turns;
    int depth = 0;
    for (ModelPart const* at = &part; at && depth < 16; at = at->mParent, ++depth) {
        auto const& rot = *at->mRot;
        if (rot.x != 0 || rot.y != 0 || rot.z != 0) turns.push_back({pivotOf(*at), {rot.x, rot.y, rot.z}});
    }
    return turns;
}
void addPart(ModelPart const& part, FaceRecipe const& recipe, std::vector<ModelBox>& out, int depth) {
    if (depth > 4 || !drawnPart(recipe, part.mName->getString())) return;
    std::vector<Turn> const turns = partTurns(part);
    for (auto const& cube : *part.mCubes) {
        auto const origin = *cube.mOrigin, size = *cube.mSize;
        ModelBox box{{origin.x, origin.y, origin.z}, {origin.x + size.x, origin.y + size.y, origin.z + size.z}, cube.mMirrored, {}, {},
                     drawnOnTop(recipe, part.mName->getString())};
        for (int side = 0; side < 6; ++side) {
            auto const& face = (*cube.mFaceData)[static_cast<size_t>(side)];
            box.faces[side] = {face.mUV->x, face.mUV->y, face.mUVSize->x, face.mUVSize->y, face.mFaceValid};
        }
        auto const& rot = *cube.mRotation;
        if (rot.x != 0 || rot.y != 0 || rot.z != 0) box.turns.push_back({{cube.mCubePivot->x, cube.mCubePivot->y, cube.mCubePivot->z}, {rot.x, rot.y, rot.z}});
        box.turns.insert(box.turns.end(), turns.begin(), turns.end());
        out.push_back(std::move(box));
    }
    if (recipe.children)
        for (auto const* child : *part.mChildren)
            if (child) addPart(*child, recipe, out, depth + 1);
}
double partVolume(ModelPart const& part, int depth = 0) {
    double volume = 0;
    for (auto const& cube : *part.mCubes) volume += std::abs(cube.mSize->x * cube.mSize->y * cube.mSize->z);
    if (depth < 4)
        for (auto const* child : *part.mChildren)
            if (child) volume += partVolume(*child, depth + 1);
    return volume;
}
struct ModelBoxes {
    std::vector<ModelBox> boxes;
    float textureWidth = 0;
};
// A renderer holds its adult's and its baby's geometry, or its variants' (a cow's per climate): of the parts
// with a wanted name, the first near the largest, so the adult's, and of variants the default skin's.
std::optional<ModelBoxes> modelBoxes(Model const& model, FaceRecipe const& recipe) {
    ModelBoxes out;
    for (auto name : recipe.parts) {
        double largest = 0;
        for (auto const* part : *model.mAllParts)
            if (part && part->mName->getString() == name) largest = std::max(largest, partVolume(*part));
        ModelPart const* best = nullptr;
        for (auto const* part : *model.mAllParts)
            if (part && part->mName->getString() == name && partVolume(*part) >= largest * .75) { best = part; break; }
        if (!best) continue;
        if (out.textureWidth <= 0) out.textureWidth = best->mTexSize->x;
        addPart(*best, recipe, out.boxes, 0);
    }
    if (out.boxes.empty()) return std::nullopt;
    return out;
}
std::string faceTexture(DataDrivenRenderer& dataDriven, std::string const& renderer) {
    std::string texture(baseTexture(renderer));
    if (texture.empty())
        if (auto const& location = dataDriven.mDefaultSkin->mResourceLocationPtr.get(); location)
            texture = location->mPath->value;
    return texture;
}
// A picture used whole (FaceRecipe::image), at eight texels a side as a head.
std::optional<Face> pictureFace(IClientInstance& client, std::string_view path) {
    auto image = visuals::readPackImage(client.getResourcePackManager(), std::string(path));
    if (!image || image->width <= 0 || image->height <= 0) return std::nullopt;
    if (image->rgba.size() < static_cast<size_t>(image->width) * image->height * 4) return std::nullopt;
    return reduceFace(packedPicture(image->rgba.data(), image->width, image->height), image->width, image->height, image->width / 8.0);
}
// `variant`: the texture the mob's variant picks (variantTexture), or empty for the kind's own.
std::optional<Face> load(IClientInstance& client, DataDrivenRenderer& dataDriven, std::string const& renderer,
                         std::string const& variant = {}, bool sheared = false) {
    FaceRecipe const recipe = faceRecipe(renderer, sheared);
    if (!recipe.image.empty()) return pictureFace(client, recipe.image);
    auto model = dataDriven.mModel.get();
    if (!model) return std::nullopt;
    auto boxes = modelBoxes(*model, recipe);
    if (!boxes) return std::nullopt;
    std::string texture = variant.empty() ? faceTexture(dataDriven, renderer) : variant;
    if (texture.empty()) return std::nullopt;
    auto image = visuals::readPackImage(client.getResourcePackManager(), texture);
    if (!image || image->width <= 0 || image->height <= 0) return std::nullopt;
    int width = image->width, height = image->height;
    // Textures may be finer than the model's texture units.
    double texels = boxes->textureWidth > 0 ? width / boxes->textureWidth : 1;
    return projectFace(image->rgba.data(), width, height, texels, boxes->boxes, {recipe.yaw, recipe.pitch, recipe.rows, recipe.columns});
}
std::optional<Face> load(IClientInstance& client, Actor& actor, std::string const& renderer, std::string const& variant, bool sheared) {
    auto dispatcher = client.getEntityRenderDispatcher();
    if (!dispatcher) return std::nullopt;
    auto dataDriven = dispatcher->getDataDrivenRenderer(actor.getActorRendererId());
    if (!dataDriven) return std::nullopt;
    return load(client, *dataDriven, renderer, variant, sheared);
}
#ifdef LAMIUM_RADAR_ICON_PROBE
// The research pictures' background, behind clear pixels.
constexpr std::uint32_t probeGray = packColor(96, 96, 96);
void writeBmp(std::string const& name, std::uint32_t const* px, int width, int height) {
    try {
        std::ofstream out(Runtime::instance().self().getModDir() / "logs" / name, std::ios::binary);
        auto put = [&](std::uint32_t v, int bytes) { for (int b = 0; b < bytes; ++b) out.put(static_cast<char>((v >> (8 * b)) & 0xff)); };
        std::uint32_t data = static_cast<std::uint32_t>(width * height * 4);
        out.put('B'); out.put('M'); put(54 + data, 4); put(0, 4); put(54, 4);
        put(40, 4); put(static_cast<std::uint32_t>(width), 4); put(static_cast<std::uint32_t>(-height), 4); put(1, 2); put(32, 2);
        put(0, 4); put(data, 4); put(2835, 4); put(2835, 4); put(0, 4); put(0, 4);
        for (int i = 0; i < width * height; ++i) {
            auto c = px[i];
            out.put(static_cast<char>(channel(c, 2))); out.put(static_cast<char>(channel(c, 1)));
            out.put(static_cast<char>(channel(c, 0))); out.put(static_cast<char>(0xff));
        }
    } catch (...) {}
}
// Research builds only: every face built so far, outlined and x4, to
// logs/radar-faces.bmp, in the order logged with "face #N".
void dumpFaces() {
    constexpr int zoom = 4, cell = faceCellSide * zoom + 4, perRow = 8;
    int count = static_cast<int>(found.size()), rows = (count + perRow - 1) / perRow;
    int width = perRow * cell, height = std::max(1, rows) * cell;
    std::vector<std::uint32_t> px(static_cast<size_t>(width) * height, probeGray);
    for (int i = 0; i < count; ++i) {
        auto at = faceAtlasCell(i);
        int ox = (i % perRow) * cell + 2, oy = (i / perRow) * cell + 2;
        for (int y = 0; y < faceCellSide * zoom; ++y)
            for (int x = 0; x < faceCellSide * zoom; ++x) {
                auto c = atlas[static_cast<size_t>(at.y + y / zoom) * faceAtlasSide + at.x + x / zoom];
                if (c >> 24) px[static_cast<size_t>(oy + y) * width + ox + x] = c;
            }
    }
    writeBmp("radar-faces.bmp", px.data(), width, height);
}
// An unread skin (L-87): its image (clear pixels shown grey) and every
// geometry text, as logs/skin-N.*, N in the order logged.
void dumpSkin(int n, std::uint8_t const* rgba, int width, int height, std::initializer_list<std::string> texts) {
    std::vector<std::uint32_t> px(static_cast<size_t>(width) * height);
    for (size_t i = 0; i < px.size(); ++i) {
        auto const* p = rgba + i * 4;
        px[i] = p[3] ? packColor(p[0], p[1], p[2]) : probeGray;
    }
    writeBmp(std::format("skin-{}.bmp", n), px.data(), width, height);
    int part = 0;
    for (auto const& text : texts)
        try {
            std::ofstream(Runtime::instance().self().getModDir() / "logs" / std::format("skin-{}-{}.json", n, part++)) << text;
        } catch (...) {}
}
// Research builds only (L-330): once per world, every data-driven renderer's face built apart from the radar's
// own atlas, to logs/radar-sweep.bmp (cells in reading order, eight a row), and what each was built from to
// logs/radar-sweep.txt: the head part or every part's name, each head cube's rotation, pivot and mirror, the
// front face's UV and its rotation, the texture and its size against the model's.
bool swept = false;
std::string vec(Vec3 const& v) { return std::format("({:g},{:g},{:g})", v.x, v.y, v.z); }
void describePart(ModelPart const& part, std::string& out, int depth) {
    if (depth > 4) return;
    out += std::format("    part {}{} pos {} rot {} mirror {} cubes {} meshes {}\n", std::string(static_cast<size_t>(depth) * 2, ' '),
                       part.mName->getString(), vec(*part.mPos), vec(*part.mRot), static_cast<bool>(part.mMirror), part.mCubes->size(),
                       part.mTextureMeshes->size());
    for (auto const& cube : *part.mCubes) {
        auto const& face = (*cube.mFaceData)[2];
        out += std::format("      cube origin {} size {} rot {} pivot {} mirrored {} front uv ({:g},{:g}) size ({:g},{:g}) uvrot {} valid {}\n",
                           vec(*cube.mOrigin), vec(*cube.mSize), vec(*cube.mRotation), vec(*cube.mCubePivot),
                           static_cast<bool>(cube.mMirrored), face.mUV->x, face.mUV->y, face.mUVSize->x, face.mUVSize->y,
                           static_cast<int>(face.mUvRotation), static_cast<bool>(face.mFaceValid));
    }
    for (auto const* child : *part.mChildren)
        if (child) describePart(*child, out, depth + 1);
}
void sweepAll(IClientInstance& client) {
    auto dispatcher = client.getEntityRenderDispatcher();
    if (!dispatcher) return;
    std::vector<std::pair<std::string, DataDrivenRenderer*>> renderers;
    for (auto const& [name, renderer] : *dispatcher->mDataDrivenRenderers)
        if (renderer) renderers.emplace_back(name.getString(), renderer.get());
    std::sort(renderers.begin(), renderers.end(), [](auto const& a, auto const& b) { return a.first < b.first; });
    std::string text = std::format("{} data-driven renderers, {} others\n", renderers.size(), dispatcher->mNonDataDrivenRenderers->size());
    for (auto const& entry : *dispatcher->mNonDataDrivenRenderers) text += "  not data-driven: " + entry.first.getString() + "\n";
    std::vector<Face> faces;
    for (size_t i = 0; i < renderers.size(); ++i) {
        auto const& [name, renderer] = renderers[i];
        text += std::format("#{} {}\n", i, name);
        try {
            std::string texture = faceTexture(*renderer, name);
            text += std::format("  texture {} ({})", texture.empty() ? "none" : texture, baseTexture(name).empty() ? "default skin" : "baseTexture");
            if (!texture.empty())
                if (auto image = visuals::readPackImage(client.getResourcePackManager(), texture))
                    text += std::format(" {}x{}", image->width, image->height);
            text += "\n";
            auto model = renderer->mModel.get();
            if (!model) { text += "  no model\n"; continue; }
            int heads = 0;
            std::string names;
            for (auto const* part : *model->mAllParts) {
                if (!part) continue;
                names += " " + part->mName->getString();
                if (part->mName->getString() != "head") continue;
                if (heads++ == 0) text += std::format("  head texSize ({:g},{:g})\n", part->mTexSize->x, part->mTexSize->y);
                describePart(*part, text, 0);
            }
            if (heads != 1) text += std::format("  {} head parts; all parts:{}\n", heads, names);
            if (auto face = load(client, *renderer, name)) {
                text += std::format("  face {}x{} in cell {}\n", face->width, face->height, faces.size());
                faces.push_back(std::move(*face));
            } else text += "  no face\n";
            for (int v = 1; !variantTexture(name, v).empty(); ++v)
                if (auto face = load(client, *renderer, name, variantTexture(name, v))) {
                    text += std::format("  variant {} face {}x{} in cell {}\n", v, face->width, face->height, faces.size());
                    faces.push_back(std::move(*face));
                }
        } catch (...) { text += "  failed\n"; }
    }
    try { std::ofstream(Runtime::instance().self().getModDir() / "logs" / "radar-sweep.txt") << text; } catch (...) {}
    int const count = std::min(static_cast<int>(faces.size()), faceAtlasCapacity);
    std::vector<std::uint32_t> sheet;
    for (int i = 0; i < count; ++i) writeFace(sheet, i, faces[static_cast<size_t>(i)]);
    if (sheet.empty()) return;
    constexpr int zoom = 4, cell = faceCellSide * zoom + 4, perRow = 8;
    int const rows = (count + perRow - 1) / perRow, width = perRow * cell, height = std::max(1, rows) * cell;
    std::vector<std::uint32_t> px(static_cast<size_t>(width) * height, probeGray);
    for (int i = 0; i < count; ++i) {
        auto at = faceAtlasCell(i);
        int ox = (i % perRow) * cell + 2, oy = (i / perRow) * cell + 2;
        for (int y = 0; y < faceCellSide * zoom; ++y)
            for (int x = 0; x < faceCellSide * zoom; ++x) {
                auto c = sheet[static_cast<size_t>(at.y + y / zoom) * faceAtlasSide + at.x + x / zoom];
                if (c >> 24) px[static_cast<size_t>(oy + y) * width + ox + x] = c;
            }
    }
    writeBmp("radar-sweep.bmp", px.data(), width, height);
    Runtime::instance().self().getLogger().info("Radar faces: swept {} renderers, {} faces, to logs/radar-sweep.*", renderers.size(), count);
}
#endif
int add(std::optional<Face> face, std::string const& name) {
    if (!face || static_cast<int>(found.size()) >= faceAtlasCapacity) return -1;
    found.push_back(std::move(*face));
    int index = static_cast<int>(found.size()) - 1;
    writeFace(atlas, index, found.back());
    atlasDirty = true;
#ifdef LAMIUM_RADAR_ICON_PROBE
    Runtime::instance().self().getLogger().info("Radar faces: face #{} is {} ({}x{})", index, name, found.back().width,
                                                found.back().height);
    dumpFaces();
#else
    (void)name;
#endif
    return index;
}
}
int faceOf(IClientInstance& client, Actor& actor) {
    std::string renderer = actor.getActorRendererId().getString();
    if (renderer.empty()) return -1;
    // A kind whose look follows the mob (a horse's coat, an elder guardian, a sheared snow golem) has a face
    // per look.
    bool const guardian = renderer == "minecraft:guardian" || renderer == "minecraft:elder_guardian";
    std::string variant = lookTexture(renderer, variantTexture(renderer, 0).empty() ? -1 : actor.getVariant(),
                                      guardian && actor.getStatusFlag(ActorFlags::Elder));
    bool const sheared = renderer == "minecraft:snow_golem" && actor.getStatusFlag(ActorFlags::Sheared);
    std::string key = variant.empty() ? renderer : renderer + "#" + variant;
    if (sheared) key += "#sheared";
    if (auto at = byRenderer.find(key); at != byRenderer.end()) return at->second;
    if (loadsLeft <= 0) return -1;
    --loadsLeft;
    int index = -1;
    try { index = add(load(client, actor, renderer, variant, sheared), key); } catch (...) {}
    if (index < 0) log(std::format("no face for {}; it stays a dot", key));
    byRenderer.emplace(std::move(key), index);
    return index;
}
int headOf(Actor& actor) {
    std::unique_ptr<SerializedSkinRef> const& ref = static_cast<Player&>(actor).mSkin;
    return ref ? headOf(*ref) : -1;
}
int headOf(SerializedSkinRef const& ref) {
    std::shared_ptr<Bedrock::Application::ThreadOwner<SerializedSkinImpl>> const& owner = ref.mSkinImpl;
    if (!owner) return -1;
    auto const& skin = owner->mObject;
    // Skins may arrive after the player: one not here yet is not remembered.
    std::string key = skin.mFullId->empty() ? *skin.mId : *skin.mFullId;
    if (key.empty()) return -1;
    key.insert(0, "skin:");
    if (auto at = byRenderer.find(key); at != byRenderer.end()) return at->second;
    auto const& image = static_cast<mce::Image const&>(*skin.mSkinImage);
    int width = static_cast<int>(image.mWidth), height = static_cast<int>(image.mHeight);
    if (width <= 0 || height <= 0 || loadsLeft <= 0) return -1;
    --loadsLeft;
    int index = -1;
    std::string source = "nothing";
    try {
        auto const& bytes = image.mImageBytes;
        if (image.imageFormat == mce::ImageFormat::RGBA8Unorm && bytes.size() >= static_cast<size_t>(width) * height * 4) {
            // The skin's own geometry first: character-creator skins and
            // custom models keep the face elsewhere than the classic layout.
            auto patch = patchGeometry(*skin.mResourcePatch);
            if (patch.geometry.empty()) patch.geometry = *skin.mDefaultGeometryName;
            for (auto const* geometry : {&*skin.mGeometryData, &*skin.mGeometryDataMutable}) {
                if (index >= 0) break;
                auto head = skinHead(geometry->toStyledString(), patch);
                if (!head) continue;
                // A character-creator head is painted on the animated face image.
                mce::Image const* texture = &image;
                if (head->animatedFace) {
                    texture = nullptr;
                    for (auto const& animated : *skin.mSkinAnimatedImages)
                        if (static_cast<persona::AnimatedTextureType>(animated.mType) == persona::AnimatedTextureType::Face) {
                            texture = &static_cast<mce::Image const&>(*animated.mImage);
                            break;
                        }
                }
                if (!texture || texture->imageFormat != mce::ImageFormat::RGBA8Unorm || texture->mWidth == 0) continue;
                int w = static_cast<int>(texture->mWidth), h = static_cast<int>(texture->mHeight);
                if (texture->mImageBytes.size() < static_cast<size_t>(w) * h * 4) continue;
                index = add(composeFace(texture->mImageBytes.data(), w, h, w / head->textureWidth, std::move(head->boxes)), key);
                source = head->animatedFace ? "its animated face" : "its geometry";
            }
            if (index < 0) {
#ifdef LAMIUM_RADAR_ICON_PROBE
                static int skins = 0;
                Runtime::instance().self().getLogger().info("Radar faces: skin #{} has no head in its geometry; dumped", skins);
                dumpSkin(skins++, bytes.data(), width, height,
                         {skin.mGeometryData->toStyledString(), skin.mGeometryDataMutable->toStyledString(), *skin.mResourcePatch,
                          *skin.mAnimationData});
#endif
                index = add(playerHead(bytes.data(), width, height), key);
                source = "the classic layout";
            }
        }
    } catch (...) {}
    log(std::format("{} for a {}x{} skin from {} (persona {}, geometry {})", index < 0 ? "no head" : "a head", width, height,
                    source, static_cast<bool>(skin.mIsPersona), std::string(*skin.mDefaultGeometryName)));
    byRenderer.emplace(std::move(key), index);
    return index;
}
void frame() { loadsLeft = loadsPerFrame; }
void sweep(IClientInstance& client) {
#ifdef LAMIUM_RADAR_ICON_PROBE
    if (swept) return;
    swept = true;
    try { sweepAll(client); } catch (...) {}
#else
    (void)client;
#endif
}
bool draw(MinecraftUIRenderContext& context, int index, float x, float y, float size, float alpha) {
    if (index < 0 || index >= static_cast<int>(found.size())) return false;
    auto& client = context.mClient;
    if (atlasDirty || !atlasUploaded) {
        try { atlasUploaded = upload(client); } catch (...) { atlasUploaded = false; }
        atlasDirty = false;
        if (!atlasUploaded) return false;
    }
    auto const& face = found[static_cast<size_t>(index)];
    double inverse = client.getGuiData()->mInvGuiScale;
    double scale = std::isfinite(inverse) && inverse > 0 ? 1 / inverse : 1; // Screen pixels per GUI unit.
    // Whole screen pixels per texel, placed on the screen's pixel grid: the
    // face keeps its texture's shape. The outline is part of the image.
    int texel = faceTexelPixels(std::max(face.width, face.height), size * scale);
    int cols = outlinedWidth(face), rows = outlinedHeight(face);
    double w = cols * texel, h = rows * texel;
    double left = std::round(x * scale - w / 2), top = std::round(y * scale - h / 2);
    auto gui = [&](double pixels) { return static_cast<float>(pixels / scale); };
    auto cell = faceAtlasCell(index);
    float span = 1.f / faceAtlasSide;
    if (!ui::runtimeImage(context, atlasLocation(), {gui(left), gui(top), gui(w), gui(h)}, cell.x * span, cell.y * span,
                          cols * span, rows * span, alpha)) {
        // Resource reloads drop runtime textures; upload again next time.
        atlasUploaded = false;
        return false;
    }
    return true;
}
void forget(IClientInstance* client) {
    byRenderer.clear();
    found.clear();
    atlas.clear();
    if (atlasUploaded && client)
        try {
            if (auto group = client->getTextureGroup()) group->unloadTexture(atlasLocation(), false);
        } catch (...) {}
    atlasUploaded = false;
    atlasDirty = false;
#ifdef LAMIUM_RADAR_ICON_PROBE
    swept = false;
#endif
}
}

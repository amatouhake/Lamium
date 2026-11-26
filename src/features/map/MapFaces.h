#pragma once
#include "features/map/MapTiles.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lamium::map {
// Mob faces on the radar (BACKLOG L-85, docs/demos/radar-icons.html, look
// B). Built at runtime from the texture the game already has; nothing from
// the game is stored. Pure; RadarFaces.cpp reads the model and texture,
// Minimap.cpp draws the faces on screen pixels.

// A face at its texture's own resolution (at most maxFaceSide a side; a
// larger one is halved, maintainer 2026-11-26).
inline constexpr int maxFaceSide = 15;
// A warden's and an elder guardian's are 16 a side and stay whole, twice a
// guardian's as they are in the world (FaceRecipe::side, L-411).
inline constexpr int wholeFaceSide = 16;
// The widest face: a phantom with the roots of its wings.
inline constexpr int widestFaceSide = 17;
struct Face {
    int width = 0, height = 0;
    std::vector<std::uint32_t> pixels; // RGBA, row-major; alpha 0 is see-through.
};

// Renderers whose default skin is not the mob's own look (an overlay, an
// armor layer or a baby's): the texture to build the face from instead.
inline std::string_view baseTexture(std::string_view renderer) {
    if (renderer == "minecraft:villager_v2") return "textures/entity/villager2/villager";
    if (renderer == "minecraft:horse") return "textures/entity/horse2/horse_white";
    if (renderer == "minecraft:donkey") return "textures/entity/horse2/donkey";
    if (renderer == "minecraft:mule") return "textures/entity/horse2/mule";
    if (renderer == "minecraft:skeleton_horse") return "textures/entity/horse2/horse_skeleton";
    if (renderer == "minecraft:zombie_horse") return "textures/entity/horse2/horse_zombie";
    if (renderer == "minecraft:llama" || renderer == "minecraft:trader_llama") return "textures/entity/llama/llama_creamy";
    if (renderer == "minecraft:fox") return "textures/entity/fox/fox";
    if (renderer == "minecraft:happy_ghast") return "textures/entity/happy_ghast/happy_ghast";
    if (renderer == "minecraft:elder_guardian") return "textures/entity/guardian_elder";
    // Layers the game draws over the skin, where these kinds' eyes are (L-411).
    if (renderer == "minecraft:bogged") return "textures/entity/skeleton/bogged_clothes";
    if (renderer == "minecraft:sulfur_cube") return "textures/entity/sulfur_cube/sulfur_cube_outer";
    return {};
}
// Parts drawn over a head that are not the face itself (worn layers and tack).
inline bool faceLayer(std::string_view part) {
    for (auto skip : {"hat", "helmet", "brim", "armor", "jacket", "bridle", "rein", "saddle"})
        if (part.find(skip) != std::string_view::npos) return false;
    return part != "bitl" && part != "bitr";
}

// How a kind's face is seen (L-330, maintainer 2026-11-26): from the front of its "head" with the parts
// hanging from it by default; a kind without a head shows the part where its face is; long necks are cut
// below the face; kinds whose eyes are on their sides (fish, parrots, horses, turtles, camels) are seen from
// the side, horses, armadillos and goats from an angle (a turtle's eye, squeezed at an angle, showed only its
// white). The kinds changed after the faces were looked at one by one are L-411's (maintainer 2026-11-26).
struct FaceRecipe {
    std::vector<std::string_view> parts{"head"}; // none: the kind stays a dot
    bool children = true;  // the parts hanging from those, as a head's nose and ears
    double yaw = 0;        // degrees turned before it is seen: 90 shows its +x side, its front on the right
    double pitch = 0;      // degrees tipped toward the viewer, to see its top
    int rows = 0, columns = 0; // at most this many model units from the top, and from the front (0: all)
    std::vector<std::string_view> leftOut;           // parts the game hides on this kind (a horse's mule ears)
    std::vector<std::string_view> leftOutContaining; // and by part of their name (a guardian's spikes)
    std::vector<std::string_view> onTop;             // drawn over the rest: an eye set inside the head
    std::string_view image; // a picture used whole instead of the model (the snow golem's carved pumpkin)
    int cubes = 0;          // of each named part, only its first this many boxes (0: all)
    bool aligned = false, oneSided = false, picked = false, centered = false; // FaceShot's
    int side = maxFaceSide; // the longer side the face is reduced to fit
    double scale = 1;       // the boxes' units scaled: a box stretched over its texture is seen at the texture's size
};
// `sheared`: a snow golem without its pumpkin shows the head under it. `elder`: a guardian with the elder's look.
inline FaceRecipe faceRecipe(std::string_view renderer, bool sheared = false, bool elder = false) {
    std::string_view const kind = renderer.substr(renderer.starts_with("minecraft:") ? 10 : 0);
    FaceRecipe r;
    auto only = [&](std::initializer_list<std::string_view> parts) { r.parts = parts; r.children = false; };
    auto side = [&] { r.yaw = 90; };
    // From 30° off the front a horse shows an eye, its muzzle and both ears (maintainer 2026-11-26).
    if (kind == "horse" || kind == "skeleton_horse" || kind == "zombie_horse") r.leftOut = {"muleearl", "muleearr"}, r.yaw = 30;
    else if (kind == "donkey" || kind == "mule") r.leftOut = {"earl", "earr"}, r.yaw = 30;
    else if (kind == "llama" || kind == "trader_llama") r.rows = 10;
    // The head without the neck under it: smaller, and nothing of it halved.
    else if (kind == "camel" || kind == "camel_husk") side(), r.rows = 8, r.columns = 10;
    else if (kind == "turtle" || kind == "parrot") side();
    // At 45° a goat shows a horn, an eye and its beard at once.
    else if (kind == "armadillo" || kind == "goat") r.yaw = 45;
    // At 60° a dolphin's eye has its white and its pupil; a sniffer shows its red coat behind its yellow beak.
    else if (kind == "dolphin") r.yaw = 60;
    else if (kind == "sniffer") r.yaw = 30, r.picked = true;
    // The head with its ears is 17 wide on half units: halved about its middle it stays even left and right.
    else if (kind == "panda") r.aligned = r.centered = r.picked = true;
    // From the front with the roots of its wings: its eyes show, and its shape.
    else if (kind == "phantom") r.parts = {"body"}, r.leftOutContaining = {"wingtip", "tail"}, r.side = widestFaceSide;
    else if (kind == "bogged") r.leftOut = {"mushrooms"};
    // The outer box, whose skin (baseTexture) has the eyes and the mouth.
    else if (kind == "sulfur_cube") only({"sulfur_cube"}), r.cubes = 1;
    // Boxes of 72 and 64 units under a face 16 texture units wide: seen at that size, even left and right.
    else if (kind == "ghast") only({"body"}), r.scale = 16.0 / 72, r.side = wholeFaceSide;
    else if (kind == "happy_ghast") only({"body"}), r.cubes = 1, r.scale = 16.0 / 64, r.side = wholeFaceSide;
    // Whole from the side: the shell's spiral, the eye and the tentacles.
    else if (kind == "nautilus" || kind == "zombie_nautilus") r.parts = {"nautilus"}, side();
    // Its head is the trunk's two boxes, then flat boards of branches to both sides.
    else if (kind == "creaking") r.cubes = 2;
    // Heads built on half units; a wolf's snout is three wide on a head six wide and keeps its one-texel nose.
    else if (kind == "wolf") r.aligned = r.oneSided = true;
    else if (kind == "cat" || kind == "ocelot" || kind == "allay" || kind == "vex" || kind == "polar_bear") r.aligned = true;
    else if (kind == "warden") r.leftOutContaining = {"tendril"}, r.side = wholeFaceSide;
    else if (kind == "frog") r.parts = {"body"}; // the whole frog: its face is its eyes over its mouth and feet
    else if (kind == "squid" || kind == "glow_squid" || kind == "bee" || kind == "strider") only({"body"});
    else if (kind == "slime") only({"cube", "eye0", "eye1", "mouth"}), r.onTop = {"eye0", "eye1", "mouth"};
    else if (kind == "magma_cube") r.parts = {"insidecube"};
    else if (kind == "shulker") only({"lid", "base", "head"});
    else if (kind == "guardian" || kind == "elder_guardian") {
        r.leftOutContaining = {"spikepart"}, r.onTop = {"eye"}, r.picked = true;
        if (elder || kind == "elder_guardian") r.side = wholeFaceSide;
    }
    else if (kind == "silverfish") only({"bodypart_0", "bodypart_1"});
    else if (kind == "endermite") only({"section_0", "section_1"});
    else if (kind == "wither") r.parts = {"head1"};
    else if (kind == "pufferfish") r.parts = {"body"};
    else if (kind == "tadpole") r.parts = {"body", "tail"}, side();
    else if (kind == "cod" || kind == "tropicalfish") r.parts = {"body"}, side();
    else if (kind == "salmon") r.parts = {"body_front"}, side();
    else if (kind == "snow_golem" && !sheared) r.image = "textures/blocks/pumpkin_face_off";
    else if (kind == "armor_stand") r.parts.clear();
    return r;
}
inline bool drawnPart(FaceRecipe const& recipe, std::string_view part) {
    if (!faceLayer(part) || std::find(recipe.leftOut.begin(), recipe.leftOut.end(), part) != recipe.leftOut.end()) return false;
    for (auto piece : recipe.leftOutContaining)
        if (part.find(piece) != std::string_view::npos) return false;
    return true;
}
inline bool drawnOnTop(FaceRecipe const& recipe, std::string_view part) {
    return std::find(recipe.onTop.begin(), recipe.onTop.end(), part) != recipe.onTop.end();
}
// Kinds whose look the game picks by the mob's variant (from its render controllers): the texture for this
// variant, or empty for a kind without them or a variant out of range (the default skin is used then).
inline std::string variantTexture(std::string_view renderer, int variant) {
    std::string_view const kind = renderer.substr(renderer.starts_with("minecraft:") ? 10 : 0);
    auto pick = [&](std::initializer_list<std::string_view> textures) -> std::string {
        if (variant < 0 || variant >= static_cast<int>(textures.size())) return {};
        return std::string("textures/entity/") + std::string(textures.begin()[variant]);
    };
    if (kind == "horse") return pick({"horse2/horse_white", "horse2/horse_creamy", "horse2/horse_chestnut", "horse2/horse_brown", "horse2/horse_black", "horse2/horse_gray", "horse2/horse_darkbrown"});
    if (kind == "llama" || kind == "trader_llama") return pick({"llama/llama_creamy", "llama/llama_white", "llama/llama_brown", "llama/llama_gray"});
    if (kind == "parrot") return pick({"parrot/parrot_red_blue", "parrot/parrot_blue", "parrot/parrot_green", "parrot/parrot_yellow_blue", "parrot/parrot_grey"});
    if (kind == "cat") return pick({"cat/white", "cat/tuxedo", "cat/redtabby", "cat/siamesecat", "cat/britishshorthair", "cat/calico", "cat/persian", "cat/ragdoll", "cat/tabby", "cat/allblackcat", "cat/jellie"});
    // The skins of the model in use (1.26.10's): the files of the one before it are still in the packs, laid out otherwise.
    if (kind == "rabbit")
        return pick({"rabbit/rabbit_brown", "rabbit/rabbit_white", "rabbit/rabbit_black", "rabbit/rabbit_white_splotched", "rabbit/rabbit_gold",
                     "rabbit/rabbit_salt"});
    if (kind == "axolotl") return pick({"axolotl/axolotl_lucy", "axolotl/axolotl_cyan", "axolotl/axolotl_gold", "axolotl/axolotl_wild", "axolotl/axolotl_blue"});
    if (kind == "fox") return pick({"fox/fox", "fox/arctic_fox"});
    if (kind == "frog") return pick({"frog/temperate_frog", "frog/cold_frog", "frog/warm_frog"});
    if (kind == "wolf") return pick({"wolf/wolf", "wolf/wolf_ashen", "wolf/wolf_black", "wolf/wolf_chestnut", "wolf/wolf_rusty", "wolf/wolf_snowy", "wolf/wolf_spotted", "wolf/wolf_striped", "wolf/wolf_woods"});
    if (kind == "mooshroom") return pick({"cow/mooshroom_v2", "cow/brown_mooshroom_v2"});
    if (kind == "panda") return pick({"panda/panda", "panda/panda_lazy", "panda/panda_worried", "panda/panda_playful", "panda/panda_brown", "panda/panda_sneezy", "panda/panda_aggressive"});
    if (kind == "shulker")
        return pick({"shulker/shulker_black", "shulker/shulker_red", "shulker/shulker_green", "shulker/shulker_brown", "shulker/shulker_blue",
                     "shulker/shulker_purple", "shulker/shulker_cyan", "shulker/shulker_silver", "shulker/shulker_gray", "shulker/shulker_pink",
                     "shulker/shulker_lime", "shulker/shulker_yellow", "shulker/shulker_light_blue", "shulker/shulker_magenta",
                     "shulker/shulker_orange", "shulker/shulker_white", "shulker/shulker_undyed"});
    return {};
}
// The texture this mob's state picks: an elder guardian shares the guardian's renderer and takes its own skin
// by a flag; else its variant's (variantTexture). Empty: the kind's own.
inline std::string lookTexture(std::string_view renderer, int variant, bool elder) {
    if (elder && (renderer == "minecraft:guardian" || renderer == "minecraft:elder_guardian")) return "textures/entity/guardian_elder";
    return variantTexture(renderer, variant);
}

// A face picture at its image's resolution, reduced to fit `side` (larger ones halved, maintainer
// 2026-11-26): a high-resolution face drops to one texel per model unit when that fits, else to the
// smallest whole factor that does. Each texel is the mean of the visible ones it covers, visible when at
// least half are, so eyes are not dropped; `picked`, it is the one at the middle of those instead, which keeps
// a dark eye dark where the mean greys it (guardian, panda, L-411). None when nothing shows.
inline std::optional<Face> reduceFace(std::vector<std::uint32_t> const& full, int fullW, int fullH, double texelsPerUnit,
                                      int side = maxFaceSide, bool picked = false) {
    if (fullW <= 0 || fullH <= 0 || side <= 0 || full.size() < static_cast<size_t>(fullW) * fullH) return std::nullopt;
    int step = std::max(1, (std::max(fullW, fullH) + side - 1) / side);
    if (int unit = static_cast<int>(std::lround(texelsPerUnit)); unit > step && std::max(fullW, fullH) / unit <= side) step = unit;
    Face face{std::max(1, fullW / step), std::max(1, fullH / step), {}};
    face.pixels.assign(static_cast<size_t>(face.width) * face.height, 0);
    bool any = false;
    for (int y = 0; y < face.height; ++y)
        for (int x = 0; x < face.width; ++x) {
            if (picked) {
                auto const c = full[static_cast<size_t>(std::min(fullH - 1, y * step + step / 2)) * fullW + std::min(fullW - 1, x * step + step / 2)];
                if (!(c >> 24)) continue;
                face.pixels[static_cast<size_t>(y * face.width + x)] = c;
                any = true;
                continue;
            }
            // A picture thinner than the step has fewer texels to cover than step x step.
            int covered = 0, seen = 0, r = 0, g = 0, b = 0;
            for (int dy = 0; dy < step && y * step + dy < fullH; ++dy)
                for (int dx = 0; dx < step && x * step + dx < fullW; ++dx) {
                    auto c = full[static_cast<size_t>(y * step + dy) * fullW + x * step + dx];
                    ++covered;
                    if (!(c >> 24)) continue;
                    ++seen, r += channel(c, 0), g += channel(c, 1), b += channel(c, 2);
                }
            if (!seen || seen * 2 < covered) continue;
            face.pixels[static_cast<size_t>(y * face.width + x)] = packColor(r / seen, g / seen, b / seen);
            any = true;
        }
    if (!any) return std::nullopt;
    return face;
}

// An RGBA image as face pixels: alpha 0 clear, any other drawn opaque (composeFace).
inline std::vector<std::uint32_t> packedPicture(std::uint8_t const* rgba, int width, int height) {
    std::vector<std::uint32_t> out(static_cast<size_t>(std::max(0, width)) * std::max(0, height));
    for (size_t i = 0; i < out.size(); ++i) {
        auto const* p = rgba + i * 4;
        out[i] = p[3] ? packColor(p[0], p[1], p[2]) : 0;
    }
    return out;
}

// One cube's front: where it covers the head (model units, y up), how near
// it is (smaller z is nearer, the mob facing -z), and its front face in the
// texture (texture units).
struct FaceBox {
    double x0, y0, x1, y1, z;
    double u, v, w, h;
};
// The head seen from the front: every box's front face painted far to
// near, at `texelsPerUnit` image pixels per model unit. A finer texture (a
// high-resolution pack) is reduced by a whole factor to fit maxFaceSide.
// None when nothing is visible.
inline std::optional<Face> composeFace(std::uint8_t const* rgba, int width, int height, double texelsPerUnit,
                                       std::vector<FaceBox> boxes) {
    if (!rgba || width <= 0 || height <= 0 || !(texelsPerUnit > 0) || boxes.empty()) return std::nullopt;
    double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    for (auto const& b : boxes) {
        minX = std::min(minX, b.x0); maxX = std::max(maxX, b.x1);
        minY = std::min(minY, b.y0); maxY = std::max(maxY, b.y1);
    }
    int fullW = static_cast<int>(std::lround((maxX - minX) * texelsPerUnit));
    int fullH = static_cast<int>(std::lround((maxY - minY) * texelsPerUnit));
    if (fullW <= 0 || fullH <= 0 || fullW > 512 || fullH > 512) return std::nullopt;
    std::vector<std::uint32_t> full(static_cast<size_t>(fullW) * fullH, 0);
    std::stable_sort(boxes.begin(), boxes.end(), [](FaceBox const& a, FaceBox const& b) { return a.z > b.z; });
    for (auto const& b : boxes) {
        // A mob faces north (-z): seen from the front, +x is on the viewer's
        // left, as the front texture is laid out.
        int px0 = static_cast<int>(std::lround((maxX - b.x1) * texelsPerUnit));
        int px1 = static_cast<int>(std::lround((maxX - b.x0) * texelsPerUnit));
        int py0 = static_cast<int>(std::lround((maxY - b.y1) * texelsPerUnit));
        int py1 = static_cast<int>(std::lround((maxY - b.y0) * texelsPerUnit));
        if (px1 <= px0 || py1 <= py0) continue;
        for (int y = py0; y < py1; ++y)
            for (int x = px0; x < px1; ++x) {
                double fx = (x - px0 + .5) / (px1 - px0), fy = (y - py0 + .5) / (py1 - py0);
                int sx = static_cast<int>(std::floor((b.u + fx * b.w) * texelsPerUnit));
                int sy = static_cast<int>(std::floor((b.v + fy * b.h) * texelsPerUnit));
                if (sx < 0 || sy < 0 || sx >= width || sy >= height) continue;
                auto const* p = rgba + (static_cast<size_t>(sy) * width + sx) * 4;
                // Entity textures use partial alpha as a mask (tinting,
                // glow) on pixels the game draws opaque: only 0 is clear.
                if (!p[3]) continue;
                full[static_cast<size_t>(y) * fullW + x] = packColor(p[0], p[1], p[2]);
            }
    }
    return reduceFace(full, fullW, fullH, texelsPerUnit);
}

// A mob's model seen as a picture (L-330): its boxes with each face's
// texture rectangle, turned by the box's own rotation and then each part's
// up to the model's root, painted near over far from the front or the side.
struct Vec3d { double x = 0, y = 0, z = 0; };
// A rotation as the game keeps it (radians, its signs) about a pivot in model units, y up.
struct Turn { Vec3d pivot, angles; };
struct FaceRect { double u = 0, v = 0, w = 0, h = 0; bool valid = false; }; // texture units
enum FaceSide : int { sideDown, sideUp, sideNorth, sideSouth, sideWest, sideEast }; // the game's order of a cube's faces
struct ModelBox {
    Vec3d from, to;
    bool mirrored = false;
    FaceRect faces[6]{};
    std::vector<Turn> turns; // applied in order: the box's own first
    bool onTop = false;      // drawn over every other box (FaceRecipe::onTop)
};
// The models' order and signs: x, then y, then z, with x and z turned the
// other way (checked against the game's models: a hoglin's head pitched
// +50° looks down, a goat's face slopes to its mouth).
inline Vec3d turned(Vec3d p, Turn const& t) {
    double const ax = -t.angles.x, ay = t.angles.y, az = -t.angles.z;
    double x = p.x - t.pivot.x, y = p.y - t.pivot.y, z = p.z - t.pivot.z;
    double const y1 = y * std::cos(ax) - z * std::sin(ax), z1 = y * std::sin(ax) + z * std::cos(ax);
    double const x2 = x * std::cos(ay) + z1 * std::sin(ay), z2 = -x * std::sin(ay) + z1 * std::cos(ay);
    double const x3 = x2 * std::cos(az) - y1 * std::sin(az), y3 = x2 * std::sin(az) + y1 * std::cos(az);
    return {x3 + t.pivot.x, y3 + t.pivot.y, z2 + t.pivot.z};
}
// A face's corners at its texture's top left, top right and bottom left, seen from outside the box: from the
// front (-z) +x is on the left; the top's lower edge meets the front.
inline void faceCorners(FaceSide side, Vec3d a, Vec3d b, Vec3d out[3]) {
    switch (side) {
    case sideNorth: out[0] = {b.x, b.y, a.z}; out[1] = {a.x, b.y, a.z}; out[2] = {b.x, a.y, a.z}; break;
    case sideSouth: out[0] = {a.x, b.y, b.z}; out[1] = {b.x, b.y, b.z}; out[2] = {a.x, a.y, b.z}; break;
    case sideEast: out[0] = {b.x, b.y, b.z}; out[1] = {b.x, b.y, a.z}; out[2] = {b.x, a.y, b.z}; break;
    case sideWest: out[0] = {a.x, b.y, a.z}; out[1] = {a.x, b.y, b.z}; out[2] = {a.x, a.y, a.z}; break;
    case sideUp: out[0] = {b.x, b.y, b.z}; out[1] = {a.x, b.y, b.z}; out[2] = {b.x, b.y, a.z}; break;
    case sideDown: out[0] = {b.x, a.y, a.z}; out[1] = {a.x, a.y, a.z}; out[2] = {b.x, a.y, b.z}; break;
    }
}
// How the picture is taken: turned by `yaw` degrees (90 shows the +x side, the front on the right) and
// tipped by `pitch`, then seen from the front; cut to at most `rows` model units from the top and `columns`
// from the front (0: all). The picture's grid starts on whole units, which spreads a model built on half units
// over one row or column more (a wolf's head stands from 7.5 to 13.5: six rows of texels over seven, its
// snout's top on its eyes): `aligned` starts it where the largest face seen does. A pixel whose centre is on
// a face's edge takes it on both sides of the picture, so a part three wide on a head six wide is drawn four
// wide; `oneSided` gives it to one side only. Reduced to fit `side`, by picking when `picked` (reduceFace);
// `centered`, a picture of odd width gets a clear column before it, so halving pairs its columns about its middle.
struct FaceShot {
    double yaw = 0, pitch = 0;
    int rows = 0, columns = 0;
    bool aligned = false, oneSided = false;
    int side = maxFaceSide;
    bool picked = false, centered = false;
};
// The picture of `boxes` at `texelsPerUnit` image pixels per model unit, cropped to what is drawn and to the
// shot's cut. Reduced as composeFace's to fit the shot's side.
inline std::optional<Face> projectFace(std::uint8_t const* rgba, int width, int height, double texelsPerUnit,
                                       std::vector<ModelBox> const& boxes, FaceShot const& shot = {}) {
    if (!rgba || width <= 0 || height <= 0 || !(texelsPerUnit > 0) || boxes.empty()) return std::nullopt;
    struct Quad { double ax, ay, az, e1x, e1y, e1z, e2x, e2y, e2z, det; FaceRect uv; bool onTop; };
    double const yaw = shot.yaw * std::numbers::pi / 180, pitch = shot.pitch * std::numbers::pi / 180;
    double const cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
    std::vector<Quad> quads;
    double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    for (auto const& box : boxes) {
        FaceRect faces[6];
        std::copy(std::begin(box.faces), std::end(box.faces), faces);
        if (box.mirrored) {
            // A mirrored box swaps its sides and reads every face right to left.
            std::swap(faces[sideWest], faces[sideEast]);
            for (auto& f : faces) f.u += f.w, f.w = -f.w;
        }
        for (int side = 0; side < 6; ++side) {
            if (!faces[side].valid) continue;
            Vec3d c[3];
            faceCorners(static_cast<FaceSide>(side), box.from, box.to, c);
            double p[3][3];
            for (int k = 0; k < 3; ++k) {
                for (auto const& t : box.turns) c[k] = turned(c[k], t);
                // The shot's turn, then seen from the front: +x on the left, y down, smaller z nearer.
                double const x = c[k].x * cy + c[k].z * sy, z = -c[k].x * sy + c[k].z * cy;
                double const y = c[k].y * cp - z * sp, z2 = c[k].y * sp + z * cp;
                p[k][0] = -x, p[k][1] = -y, p[k][2] = z2;
            }
            Quad q{p[0][0], p[0][1], p[0][2], p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2],
                   p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2], 0, faces[side], box.onTop};
            q.det = q.e1x * q.e2y - q.e1y * q.e2x;
            if (!(q.det > 1e-9)) continue; // facing away, or seen edge on
            for (double s : {0., 1.})
                for (double t : {0., 1.}) {
                    double const x = q.ax + s * q.e1x + t * q.e2x, y = q.ay + s * q.e1y + t * q.e2y;
                    minX = std::min(minX, x); maxX = std::max(maxX, x); minY = std::min(minY, y); maxY = std::max(maxY, y);
                }
            quads.push_back(q);
        }
    }
    if (quads.empty()) return std::nullopt;
    double x0 = std::floor(minX), y0 = std::floor(minY), x1 = std::ceil(maxX), y1 = std::ceil(maxY);
    if (shot.aligned) {
        auto const big = std::max_element(quads.begin(), quads.end(), [](Quad const& a, Quad const& b) { return a.det < b.det; });
        // Whole units from that face's corner, far enough to hold everything.
        auto start = [](double corner, double least) {
            double phase = corner - std::floor(corner + 1e-6);
            if (phase < 1e-6) phase = 0;
            return std::floor(least - phase + 1e-6) + phase;
        };
        x0 = start(std::min({big->ax, big->ax + big->e1x, big->ax + big->e2x}), minX);
        y0 = start(std::min({big->ay, big->ay + big->e1y, big->ay + big->e2y}), minY);
        x1 = x0 + std::ceil(maxX - x0 - 1e-6), y1 = y0 + std::ceil(maxY - y0 - 1e-6);
    }
    int const gridW = static_cast<int>(std::lround((x1 - x0) * texelsPerUnit));
    int const gridH = static_cast<int>(std::lround((y1 - y0) * texelsPerUnit));
    if (gridW <= 0 || gridH <= 0 || gridW > 512 || gridH > 512) return std::nullopt;
    std::vector<std::uint32_t> grid(static_cast<size_t>(gridW) * gridH, 0);
    std::vector<double> depth(grid.size(), 1e18);
    auto texel = [&](double at) { return static_cast<int>(std::floor(at * texelsPerUnit - 1e-6)); };
    // Each pixel is sampled at its centre moved a hair toward the picture's middle, so an edge on a half unit
    // falls the same way on both sides (a frog's body was a pixel wider on its right).
    double const midX = (x0 + x1) / 2, midY = (y0 + y1) / 2;
    auto toward = [&](double at, double mid) { return shot.oneSided ? at + 1e-7 : at < mid ? at + 1e-4 : at - 1e-4; };
    for (auto const& q : quads)
        for (int gy = 0; gy < gridH; ++gy)
            for (int gx = 0; gx < gridW; ++gx) {
                double const dx = toward(x0 + (gx + .5) / texelsPerUnit, midX) - q.ax, dy = toward(y0 + (gy + .5) / texelsPerUnit, midY) - q.ay;
                double const s = (dx * q.e2y - dy * q.e2x) / q.det, t = (q.e1x * dy - q.e1y * dx) / q.det;
                if (s < 0 || s >= 1 || t < 0 || t >= 1) continue;
                double const z = q.az + s * q.e1z + t * q.e2z - (q.onTop ? 1e6 : 0);
                size_t const at = static_cast<size_t>(gy) * gridW + gx;
                if (z >= depth[at]) continue;
                // The texel holding this point; a face read right to left (negative width) steps back.
                int const sx = q.uv.w < 0 ? texel(q.uv.u + s * q.uv.w) : static_cast<int>(std::floor((q.uv.u + s * q.uv.w) * texelsPerUnit));
                int const sy = q.uv.h < 0 ? texel(q.uv.v + t * q.uv.h) : static_cast<int>(std::floor((q.uv.v + t * q.uv.h) * texelsPerUnit));
                if (sx < 0 || sy < 0 || sx >= width || sy >= height) continue;
                auto const* px = rgba + (static_cast<size_t>(sy) * width + sx) * 4;
                if (!px[3]) continue; // only 0 is clear (composeFace)
                grid[at] = packColor(px[0], px[1], px[2]);
                depth[at] = z;
            }
    int left = gridW, right = -1, top = gridH, bottom = -1;
    for (int y = 0; y < gridH; ++y)
        for (int x = 0; x < gridW; ++x)
            if (grid[static_cast<size_t>(y) * gridW + x] >> 24) left = std::min(left, x), right = std::max(right, x), top = std::min(top, y), bottom = std::max(bottom, y);
    if (right < 0) return std::nullopt;
    int h = bottom - top + 1;
    if (shot.rows > 0) h = std::min(h, static_cast<int>(std::lround(shot.rows * texelsPerUnit)));
    // From the front: on the right when its side is seen (yaw > 0), else the whole width.
    if (shot.columns > 0 && shot.yaw > 0) left = std::max(left, right + 1 - static_cast<int>(std::lround(shot.columns * texelsPerUnit)));
    int const drawn = right - left + 1, pad = shot.centered && drawn % 2 ? 1 : 0, w = drawn + pad;
    std::vector<std::uint32_t> cut(static_cast<size_t>(w) * h, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < drawn; ++x) cut[static_cast<size_t>(y) * w + pad + x] = grid[static_cast<size_t>(top + y) * gridW + left + x];
    return reduceFace(cut, w, h, texelsPerUnit, shot.side, shot.picked);
}

// A player's head from their skin image (L-87): the front of the head in
// the classic skin layout (8x8 at (8, 8) per 64 pixels of width, 64x64 or
// the legacy 64x32 and their multiples) with the outer layer at (40, 8)
// laid over it. The base layer is drawn whole, as the game draws it; outer
// texels with any alpha cover it. None for other layouts or a clear face.
inline std::optional<Face> playerHead(std::uint8_t const* rgba, int width, int height) {
    if (!rgba || width < 64 || width % 64 || (height != width && height * 2 != width)) return std::nullopt;
    int unit = width / 64, side = 8 * unit;
    int step = side > maxFaceSide ? side / 8 : 1; // a finer skin drops to the classic eight texels
    Face face{side / step, side / step, {}};
    face.pixels.assign(static_cast<size_t>(face.width) * face.height, 0);
    bool any = false;
    for (int y = 0; y < face.height; ++y)
        for (int x = 0; x < face.width; ++x) {
            auto at = [&](int u) { return rgba + (static_cast<size_t>(8 * unit + y * step) * width + u + x * step) * 4; };
            auto const* base = at(8 * unit);
            auto const* outer = at(40 * unit);
            auto const* p = outer[3] ? outer : base;
            any = any || base[3] || outer[3];
            face.pixels[static_cast<size_t>(y * face.width + x)] = packColor(p[0], p[1], p[2]);
        }
    if (!any) return std::nullopt;
    return face;
}

// Faces share one runtime texture: a grid of cells, each a face with a
// one-texel black outline around its visible pixels (look B). The outline
// is part of the image, so it moves with the face exactly.
inline constexpr int faceCellSide = widestFaceSide + 2;
inline constexpr int faceAtlasSide = 512, faceAtlasCells = faceAtlasSide / faceCellSide;
inline constexpr int faceAtlasCapacity = faceAtlasCells * faceAtlasCells;
inline constexpr std::uint32_t faceOutline = packColor(0, 0, 0, 230);
struct AtlasCell { int x, y; };
inline AtlasCell faceAtlasCell(int index) {
    return {(index % faceAtlasCells) * faceCellSide, (index / faceAtlasCells) * faceCellSide};
}
// The face's outlined image size inside its cell.
inline int outlinedWidth(Face const& face) { return face.width + 2; }
inline int outlinedHeight(Face const& face) { return face.height + 2; }
inline void writeFace(std::vector<std::uint32_t>& atlas, int index, Face const& face) {
    if (index < 0 || index >= faceAtlasCapacity) return;
    atlas.resize(static_cast<size_t>(faceAtlasSide) * faceAtlasSide, 0);
    auto cell = faceAtlasCell(index);
    auto opaque = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < face.width && y < face.height
            && (face.pixels[static_cast<size_t>(y * face.width + x)] >> 24);
    };
    for (int y = -1; y <= face.height; ++y)
        for (int x = -1; x <= face.width; ++x) {
            std::uint32_t c = 0;
            if (opaque(x, y)) c = face.pixels[static_cast<size_t>(y * face.width + x)];
            else
                for (int dy = -1; dy <= 1 && !c; ++dy)
                    for (int dx = -1; dx <= 1 && !c; ++dx)
                        if (opaque(x + dx, y + dy)) c = faceOutline;
            atlas[static_cast<size_t>(cell.y + y + 1) * faceAtlasSide + cell.x + x + 1] = c;
        }
}
// Screen pixels per face texel, always whole so the texture's pixels stay
// even, and the same for every mob (maintainer 2026-11-26, L-330): an
// 8-texel face fills about `target` screen pixels, the others keep their
// models' sizes beside it, and the one-texel outline is as wide on all.
inline int faceTexelPixels(int longerSide, double target) {
    if (longerSide <= 0 || !(target > 0)) return 1;
    return std::max(1, static_cast<int>(std::lround(target / 8)));
}
}

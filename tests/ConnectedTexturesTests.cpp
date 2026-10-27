#include "features/visuals/ConnectedTextures.h"
#include <array>
#include <cstdint>
#include <utility>
#include <vector>
void check(bool, char const*);
void connectedTexturesTests() {
    using namespace lamium::visuals::connected;
    check(connects("minecraft:glass") && connects("minecraft:red_stained_glass") && connects("minecraft:tinted_glass"),
          "glass blocks connect");
    check(!connects("minecraft:glass_pane") && !connects("minecraft:red_stained_glass_pane") && !connects("minecraft:stone"),
          "panes and other blocks do not");
    for (auto face : {Face::Down, Face::Up, Face::North, Face::South, Face::West, Face::East}) {
        auto s = sides(face);
        bool opposite = s.left.x == -s.right.x && s.left.y == -s.right.y && s.left.z == -s.right.z
            && s.top.x == -s.bottom.x && s.top.y == -s.bottom.y && s.top.z == -s.bottom.z;
        check(opposite, "left/right and top/bottom are opposite neighbors");
    }
    check(sides(Face::North).top.y == 1 && sides(Face::East).top.y == 1 && sides(Face::Up).top.y == 0,
          "side faces have the block above at their top; horizontal faces stay in their plane");
    check(ruleFor("minecraft:glass") && !ruleFor("minecraft:glass")->sidesOnly && ruleFor("minecraft:bookshelf")
              && ruleFor("minecraft:bookshelf")->sidesOnly && ruleFor("minecraft:bookshelf")->top == 0
              && ruleFor("minecraft:sandstone")->top == 4 && ruleFor("minecraft:sandstone")->left == 0
              && !ruleFor("minecraft:stone") && !ruleFor("minecraft:chiseled_sandstone"),
          "rules per block");
    // Textures that get sprites, by file name.
    check(ruleForTexture("textures/blocks/glass") && ruleForTexture("textures/blocks/glass_light_blue")
              && ruleForTexture("textures/blocks/tinted_glass") && ruleForTexture("textures/blocks/bookshelf")
              && ruleForTexture("textures/blocks/sandstone_normal") && ruleForTexture("textures/blocks/red_sandstone_normal"),
          "the rule blocks' textures get sprites");
    check(!ruleForTexture("textures/blocks/glass_pane_top") && !ruleForTexture("textures/blocks/glass_pane_top_red")
              && !ruleForTexture("textures/blocks/hard_glass") && !ruleForTexture("textures/blocks/sandstone_top")
              && !ruleForTexture("textures/blocks/chiseled_bookshelf_empty") && !ruleForTexture("textures/blocks/stone"),
          "pane edges, tops and other textures do not");
    check(ruleForTexture("textures/blocks/bookshelf")->left == ruleFor("minecraft:bookshelf")->left
              && ruleForTexture("textures/blocks/sandstone_normal")->top == ruleFor("minecraft:sandstone")->top,
          "a texture is baked with its block's rule");
    check(variantMasks(glassRule()).size() == 46 && variantMasks(bookshelfRule()).size() == 3
              && variantMasks(sandstoneRule()).size() == 1 && variantMasks(sandstoneRule())[0] == Top,
          "one sprite per joined combination a rule can show, glass also per kept corner");
    check(validMask(Left | Bottom | BottomLeft) && !validMask(Left | BottomLeft) && !validMask(Left | Right | TopLeft)
              && validMask(15 | TopLeft | TopRight | BottomLeft | BottomRight),
          "a corner is kept only between two joined sides");
    check(spriteName("textures/blocks/glass_red", Left | Top, 1) == "lamium_ct1_glass_red_5"
              && spritePath("textures/blocks/glass", 15, 1) == "textures/blocks/lamium_ct1_glass_15"
              && spriteName("textures/blocks/bookshelf", 0, 12) == "lamium_ct12_bookshelf_0",
          "sprite names carry the atlas build, the file and the joined sides");
    check(spritePath("textures/blocks/glass", 15, 1) != spritePath("textures/blocks/glass", 15, 2),
          "each atlas build names its sprites anew");
    check(bakeable(16, 16) && bakeable(64, 64) && !bakeable(128, 128) && !bakeable(16, 32) && !bakeable(8, 8),
          "only square textures up to 64 px get sprites");
    // Baking: a 16x16 image whose texel is its own column/row number.
    std::vector<std::uint32_t> image(16 * 16);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) image[y * 16 + x] = static_cast<std::uint32_t>(y * 100 + x);
    auto at = [](std::vector<std::uint32_t> const& pixels, int x, int y) { return pixels[y * 16 + x]; };
    auto glass = bake(image.data(), 16, 16, glassRule(), Left | Right | Top | Bottom);
    check(at(glass, 0, 5) == 507 && at(glass, 1, 5) == 508 && at(glass, 14, 5) == 507 && at(glass, 15, 5) == 508
              && at(glass, 5, 5) == 505,
          "joined glass refills two texels on each side from the middle (7-8)");
    check(at(glass, 0, 0) == 707 && at(glass, 15, 15) == 808, "corners take the middle on both axes");
    auto leftOnly = bake(image.data(), 16, 16, glassRule(), Left);
    check(at(leftOnly, 0, 0) == 7 && at(leftOnly, 15, 0) == 15 && at(leftOnly, 15, 15) == 1515,
          "only the joined side changes");
    auto shelf = bake(image.data(), 16, 16, bookshelfRule(), Left | Right);
    check(at(shelf, 0, 3) == 301 && at(shelf, 15, 3) == 314 && at(shelf, 1, 3) == 301 && at(shelf, 14, 3) == 314,
          "bookshelves copy the column next to the dropped one");
    auto sandstone = bake(image.data(), 16, 16, sandstoneRule(), Top);
    check(at(sandstone, 4, 0) == 804 && at(sandstone, 4, 3) == 1104 && at(sandstone, 4, 4) == 404,
          "sandstone under another shows rock rows 8-11 for its band");
    auto turn = bake(image.data(), 16, 16, glassRule(), Left | Bottom | BottomLeft);
    check(at(turn, 0, 15) == 1500, "a kept corner shows the frame's corner texel");
    check(at(turn, 1, 15) == 808 && at(turn, 0, 14) == 707 && at(turn, 1, 14) == 708,
          "only that one texel: the neighbors' borders end beside it, so more would stick out");
    check(at(turn, 0, 5) == 507 && at(turn, 5, 15) == 805 && at(turn, 0, 0) == 7,
          "the joined strips still refill away from the corner");
    auto field = bake(image.data(), 16, 16, glassRule(), 15 | TopRight | BottomLeft);
    check(at(field, 15, 0) == 15 && at(field, 0, 15) == 1500 && at(field, 0, 0) == 707 && at(field, 15, 15) == 808,
          "a surrounded face keeps exactly the corners asked for");
    std::vector<std::uint32_t> large(32 * 32);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) large[y * 32 + x] = static_cast<std::uint32_t>(y * 100 + x);
    auto largeTurn = bake(large.data(), 32, 32, glassRule(), Right | Top | TopRight);
    check(largeTurn[0 * 32 + 31] == 31 && largeTurn[1 * 32 + 30] == 130 && largeTurn[2 * 32 + 31] != 231,
          "larger textures keep a corner of one 16th");
    // Which sides and corners a face joins, in a small world of the same block.
    auto world = [](std::vector<Offset> blocks) {
        return [blocks](Offset o) {
            if (o.x == 0 && o.y == 0 && o.z == 0) return true;
            for (auto b : blocks)
                if (b.x == o.x && b.y == o.y && b.z == o.z) return true;
            return false;
        };
    };
    // Top faces of a flat L: this block joins west (image left) and south
    // (image bottom); the south-west diagonal is the notch.
    auto flatL = world({{-1, 0, 0}, {0, 0, 1}});
    check(faceMask(Face::Up, glassRule(), false, false, false, flatL) == (Left | Bottom | BottomLeft),
          "a flat L keeps the frame texel in its notch");
    check(faceMask(Face::Up, glassRule(), false, false, false, world({{-1, 0, 0}, {0, 0, 1}, {-1, 0, 1}})) == (Left | Bottom),
          "a filled square has no notch");
    check(faceMask(Face::Up, glassRule(), false, false, false, world({{-1, 0, 0}, {1, 0, 0}, {0, 0, 1}, {1, 0, 1}}))
              == (Left | Right | Bottom | BottomLeft),
          "three joined sides keep the corner whose diagonal is missing");
    check(faceMask(Face::Up, bookshelfRule(), false, false, false, flatL) == Left, "rules without corners never keep one");
    // Walls in an L seen from above: this block is east of the corner block
    // (-1,0,0), the other wall runs south from it (-1,0,1). Its south face looks
    // into the turn; the corner block's south face is hidden by the other wall.
    auto wallL = world({{-1, 0, 0}, {-1, 0, 1}});
    check(faceMask(Face::South, glassRule(), false, false, false, wallL) == Left,
          "by default the inside of a turn joins like a straight wall");
    check(faceMask(Face::South, glassRule(), false, false, true, wallL) == 0,
          "with inside corners on, the turn keeps its edge");
    check(faceMask(Face::North, glassRule(), false, false, true, wallL) == Right
              && faceMask(Face::South, glassRule(), false, false, true, world({{-1, 0, 0}})) == Left,
          "the outside of the turn and straight walls still join");
    check(faceMask(Face::South, glassRule(), true, false, true, wallL) == 0
              && faceMask(Face::South, glassRule(), true, false, false, wallL) == Right,
          "a backwards texture joins the image's other side");
    check(sourceTexel(1, 32, true, 2, 7, false, 0, -1) == 15 && sourceTexel(4, 32, true, 2, 7, false, 0, -1) == 4,
          "larger textures scale the strips");
    // Orientation.
    check(imageMask(true, false, true, false, false, false) == (Left | Top)
              && imageMask(true, false, true, false, true, true) == (Right | Bottom),
          "a face whose texture runs backwards joins the image's other side");
    check(edgeSide(0.25f, 0.25f, 0.5f, 0.25f / 16, Left, Right) == Left && edgeSide(0.5f, 0.25f, 0.5f, 0.25f / 16, Left, Right) == Right
              && edgeSide(0.375f, 0.25f, 0.5f, 0.25f / 16, Left, Right) == 0,
          "a pane vertex on a joined edge names the tile edge it shows");
    // Panes (geometry dumped in game 2026-10-11).
    check(connectsPane("minecraft:glass_pane") && connectsPane("minecraft:lime_stained_glass_pane") && !connectsPane("minecraft:glass"),
          "panes take the pane path");
    check(partOf(0.437f, 0.563f, 0.437f, 0.563f) == Part::Center && partOf(0.563f, 1.f, 0.437f, 0.563f) == Part::East
              && partOf(0.f, 0.437f, 0.437f, 0.563f) == Part::West && partOf(0.437f, 0.563f, 0.563f, 1.f) == Part::South
              && partOf(0.563f, 0.563f, 0.f, 0.437f) == Part::North && partOf(0.437f, 0.563f, 0.563f, 0.563f) == Part::Center,
          "faces belong to the center post or the arm they reach into");
    auto corner = partsOf(std::vector<Box>{{0.5f, 1.f, 0.438f, 0.562f}, {0.438f, 0.562f, 0.f, 0.5f}});
    check(corner.has(Part::Center) && corner.has(Part::East) && corner.has(Part::North) && !corner.has(Part::West)
              && !corner.has(Part::South),
          "an L corner has its two arms and the center its boxes leave out");
    auto post = partsOf(std::vector<Box>{{0.438f, 0.562f, 0.438f, 0.562f}});
    check(post.has(Part::Center) && !post.has(Part::East) && !post.has(Part::North), "a lone pane is only its post");
    auto straight = partsOf(std::vector<Box>{{0.f, 1.f, 0.438f, 0.562f}});
    check(straight.has(Part::East) && straight.has(Part::West), "a straight pane has both arms");
    check(!Parts{}.has(Part::Center), "no pane, no parts");
    // L-208: the L corner point, per orientation, and the post UV rewrite.
    auto arms = [](bool east, bool west, bool south, bool north) {
        Parts parts;
        parts.present = true;
        parts.part[partIndex(Part::Center)] = true;
        parts.part[partIndex(Part::East)] = east;
        parts.part[partIndex(Part::West)] = west;
        parts.part[partIndex(Part::South)] = south;
        parts.part[partIndex(Part::North)] = north;
        return parts;
    };
    auto es = paneCornerPoint(arms(true, false, true, false));
    auto en = paneCornerPoint(arms(true, false, false, true));
    auto wn = paneCornerPoint(arms(false, true, false, true));
    auto ws = paneCornerPoint(arms(false, true, true, false));
    check(es && es->first == 0.437f && es->second == 0.437f && en && en->first == 0.437f && en->second == 0.562f
              && wn && wn->first == 0.562f && wn->second == 0.562f && ws && ws->first == 0.562f && ws->second == 0.437f,
          "each L orientation turns around the corner away from its two arms");
    check(!paneCornerPoint(arms(true, true, false, false)) && !paneCornerPoint(arms(false, false, true, true))
              && !paneCornerPoint(arms(true, false, false, false)) && !paneCornerPoint(arms(false, false, false, false)),
          "straight, lone and absent panes have no corner to turn around");
    // L-255: the same turn has a second corner on its inside, between the arms.
    auto esIn = paneCornerPoint(arms(true, false, true, false), true);
    auto enIn = paneCornerPoint(arms(true, false, false, true), true);
    auto wnIn = paneCornerPoint(arms(false, true, false, true), true);
    auto wsIn = paneCornerPoint(arms(false, true, true, false), true);
    check(esIn && esIn->first == 0.562f && esIn->second == 0.562f && enIn && enIn->first == 0.562f
              && enIn->second == 0.437f && wnIn && wnIn->first == 0.437f && wnIn->second == 0.437f && wsIn
              && wsIn->first == 0.437f && wsIn->second == 0.562f,
          "the inside corner of each L lies between its two arms");
    check(!paneCornerPoint(arms(true, true, false, false), true) && !paneCornerPoint(arms(true, false, false, false), true),
          "straight and lone panes have no inside corner either");
    // L-208/L-255: which face takes which corner, for every part and all four
    // L orientations. The post's own faces take the outside corner; the inside
    // corner goes to the arm faces that meet there, because the post's two
    // inside faces are the joints the arms' boxes cover (review, 2026-10-27).
    constexpr float p0 = 0.4375f, p1 = 0.5625f;
    struct Quad {
        char const* what;
        Part part;
        float x0, x1, z0, z1;
        PaneCorner corner;
    };
    auto ask = [](Parts const& pane, Quad const& q) { return paneCornerFor(q.part, q.x0, q.x1, q.z0, q.z1, pane); };
    // East and north arms: the turn bends around the post's south-west corner
    // and the arms meet at its north-east one.
    Quad const eastNorth[] = {
        {"east+north: the post's west face keeps the outside edge", Part::Center, p0, p0, p0, p1, PaneCorner::Outside},
        {"east+north: the post's south face keeps the outside edge", Part::Center, p0, p1, p1, p1, PaneCorner::Outside},
        {"east+north: the post's east face is buried in the east arm", Part::Center, p1, p1, p0, p1, PaneCorner::None},
        {"east+north: the post's north face is buried in the north arm", Part::Center, p0, p1, p0, p0, PaneCorner::None},
        {"east+north: the east arm's north face takes the inside edge", Part::East, p1, 1.f, p0, p0, PaneCorner::Inside},
        {"east+north: the north arm's east face takes the inside edge", Part::North, p1, p1, 0.f, p0, PaneCorner::Inside},
        {"east+north: the east arm's south face takes nothing", Part::East, p1, 1.f, p1, p1, PaneCorner::None},
        {"east+north: the north arm's west face takes nothing", Part::North, p0, p0, 0.f, p0, PaneCorner::None},
    };
    // (An arm's inner end face lies exactly on the post face it buries, so the
    // two are one quad's worth of extents and both take nothing.)
    // East and south arms: outside at the north-west corner, inside at the
    // south-east one.
    Quad const eastSouth[] = {
        {"east+south: the post's west face keeps the outside edge", Part::Center, p0, p0, p0, p1, PaneCorner::Outside},
        {"east+south: the post's north face keeps the outside edge", Part::Center, p0, p1, p0, p0, PaneCorner::Outside},
        {"east+south: the post's east face is buried in the east arm", Part::Center, p1, p1, p0, p1, PaneCorner::None},
        {"east+south: the post's south face is buried in the south arm", Part::Center, p0, p1, p1, p1, PaneCorner::None},
        {"east+south: the east arm's south face takes the inside edge", Part::East, p1, 1.f, p1, p1, PaneCorner::Inside},
        {"east+south: the south arm's east face takes the inside edge", Part::South, p1, p1, p1, 1.f, PaneCorner::Inside},
        {"east+south: the east arm's north face takes nothing", Part::East, p1, 1.f, p0, p0, PaneCorner::None},
        {"east+south: the south arm's west face takes nothing", Part::South, p0, p0, p1, 1.f, PaneCorner::None},
    };
    // West and north arms: outside at the south-east corner, inside at the
    // north-west one.
    Quad const westNorth[] = {
        {"west+north: the post's east face keeps the outside edge", Part::Center, p1, p1, p0, p1, PaneCorner::Outside},
        {"west+north: the post's south face keeps the outside edge", Part::Center, p0, p1, p1, p1, PaneCorner::Outside},
        {"west+north: the post's west face is buried in the west arm", Part::Center, p0, p0, p0, p1, PaneCorner::None},
        {"west+north: the post's north face is buried in the north arm", Part::Center, p0, p1, p0, p0, PaneCorner::None},
        {"west+north: the west arm's north face takes the inside edge", Part::West, 0.f, p0, p0, p0, PaneCorner::Inside},
        {"west+north: the north arm's west face takes the inside edge", Part::North, p0, p0, 0.f, p0, PaneCorner::Inside},
        {"west+north: the west arm's south face takes nothing", Part::West, 0.f, p0, p1, p1, PaneCorner::None},
        {"west+north: the north arm's east face takes nothing", Part::North, p1, p1, 0.f, p0, PaneCorner::None},
    };
    // West and south arms: outside at the north-east corner, inside at the
    // south-west one.
    Quad const westSouth[] = {
        {"west+south: the post's east face keeps the outside edge", Part::Center, p1, p1, p0, p1, PaneCorner::Outside},
        {"west+south: the post's north face keeps the outside edge", Part::Center, p0, p1, p0, p0, PaneCorner::Outside},
        {"west+south: the post's west face is buried in the west arm", Part::Center, p0, p0, p0, p1, PaneCorner::None},
        {"west+south: the post's south face is buried in the south arm", Part::Center, p0, p1, p1, p1, PaneCorner::None},
        {"west+south: the west arm's south face takes the inside edge", Part::West, 0.f, p0, p1, p1, PaneCorner::Inside},
        {"west+south: the south arm's west face takes the inside edge", Part::South, p0, p0, p1, 1.f, PaneCorner::Inside},
        {"west+south: the west arm's north face takes nothing", Part::West, 0.f, p0, p0, p0, PaneCorner::None},
        {"west+south: the south arm's east face takes nothing", Part::South, p1, p1, p1, 1.f, PaneCorner::None},
    };
    for (auto const& q : eastNorth) check(ask(arms(true, false, false, true), q) == q.corner, q.what);
    for (auto const& q : eastSouth) check(ask(arms(true, false, true, false), q) == q.corner, q.what);
    for (auto const& q : westNorth) check(ask(arms(false, true, false, true), q) == q.corner, q.what);
    for (auto const& q : westSouth) check(ask(arms(false, true, true, false), q) == q.corner, q.what);
    check(paneCornerFor(Part::Center, p0, p0, p0, p1, arms(true, true, false, false)) == PaneCorner::None
              && paneCornerFor(Part::East, p1, 1.f, p0, p0, arms(true, true, false, false)) == PaneCorner::None
              && paneCornerFor(Part::East, p1, 1.f, p0, p0, arms(true, true, false, true)) == PaneCorner::None,
          "straight panes and T junctions turn no corner, so nothing slides");
    std::array<float, 4> px{}, pz{}, pu{}, turned{};
    auto postFace = [&](float cornerX, float nearU, float farU) {
        float other = cornerX < 0.5f ? 0.562f : 0.437f;
        px = {cornerX, other, cornerX, other};
        pz = {0.437f, 0.437f, 0.437f, 0.437f};
        pu = {nearU, farU, nearU, farU};
    };
    // The post face's u window is two texels (0.4375..0.5625 of the tile); the
    // slide keeps that width, only moving it onto the border the corner leans
    // to - no stretching.
    postFace(0.437f, 0.5625f, 0.4375f);
    check(paneCornerU(px, pz, pu, 0.437f, 0.437f, 0.0f, 1.0f, turned) && turned[0] == 1.0f && turned[2] == 1.0f
              && turned[1] == 0.875f && turned[3] == 0.875f && turned[1] - turned[0] == pu[1] - pu[0],
          "the face slides onto the high border and keeps its two-texel width");
    postFace(0.562f, 0.4375f, 0.5625f);
    check(paneCornerU(px, pz, pu, 0.562f, 0.437f, 0.0f, 1.0f, turned) && turned[0] == 0.0f && turned[2] == 0.0f
              && turned[1] == 0.125f && turned[3] == 0.125f && turned[1] - turned[0] == pu[1] - pu[0],
          "a corner leaning to the low border slides there with its width kept");
    postFace(0.562f, 0.9f, 0.1f);
    check(!paneCornerU(px, pz, pu, 0.562f, 0.562f, 0.25f, 0.75f, turned),
          "a quad that does not sit at the corner is left to the mask path");
    // L-255: the inside faces are the arms' own, so their u window is a longer
    // slice of the tile (seven texels here); the slide moves all of it, which
    // brings the tile's border to the corner end and keeps the slice's width.
    std::array<float, 4> ax{p1, 1.f, p1, 1.f}, az{p0, p0, p0, p0}, au{0.4375f, 0.f, 0.4375f, 0.f};
    check(paneCornerU(ax, az, au, p1, p0, 0.f, 1.f, turned) && turned[0] == 1.f && turned[2] == 1.f
              && turned[1] == 0.5625f && turned[3] == 0.5625f && turned[0] - turned[1] == au[0] - au[1],
          "the east arm's north face slides its whole window onto the inside border");
    std::array<float, 4> bx{p1, p1, p1, p1}, bz{0.f, p0, 0.f, p0}, bu{1.f, 0.5625f, 1.f, 0.5625f};
    check(paneCornerU(bx, bz, bu, p1, p0, 0.f, 1.f, turned) && turned[1] == 0.f && turned[3] == 0.f
              && turned[0] == 0.4375f && turned[2] == 0.4375f && turned[1] - turned[0] == bu[1] - bu[0],
          "the north arm's east face slides onto the low border the same way");
    std::array<float, 4> ox{0.437f, 0.562f, 0.562f, 0.437f}, oz{0.562f, 0.562f, 0.562f, 0.562f};
    std::array<float, 4> ou{0.4375f, 0.5625f, 0.4375f, 0.5625f};
    check(!paneCornerU(ox, oz, ou, 0.437f, 0.437f, 0.0f, 1.0f, turned)
              && paneCornerU(ox, oz, ou, 0.562f, 0.562f, 0.0f, 1.0f, turned),
          "an outside face takes the outside corner, not the inside one");
}

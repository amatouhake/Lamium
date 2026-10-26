#include "features/schematic/SchematicActions.h"
#include "features/camera/CameraSessions.h"
#include "features/camera/ViewFeet.h"
#include "features/schematic/GhostRenderer.h"
#include "features/schematic/MoveDirection.h"
#include "features/schematic/MoveTarget.h"
#include "features/schematic/TargetRay.h"
#include "features/schematic/UnreadableNotice.h"
#include "features/schematic/SchematicSession.h"
#include "app/Runtime.h"
#include "features/schematic/Selection.h"
#include "input/Actions.h"
#include "ui/Localization.h"
#include "ui/Toast.h"
#include "ui/Widgets.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/deps/input/MouseAction.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/world/phys/HitResult.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/ShapeType.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include <atomic>
#include <cmath>
#include <limits>
#include <memory>

namespace lamium::schematic::actions {
namespace {
using input::Action;
MoveTarget moveTarget = MoveTarget::Placement;
std::optional<menu::Stepper> remembered;
UnreadableNotice unreadableNotice;
std::uint64_t lastGeneration = std::numeric_limits<std::uint64_t>::max();
int lastDimension = std::numeric_limits<int>::min();
void changeTarget(MoveTarget next);
std::string targetName(MoveTarget target);
// The world join and the dimension own the schematic view state: drop the
// move target and repeat memory when either changes, even if no key is pressed.
void syncLifecycle(IClientInstance& client) {
    auto generation = session::generation();
    auto* player = client.getLocalPlayer();
    int dimension = player ? static_cast<int>(player->getDimensionId()) : lastDimension;
    if (lastGeneration == std::numeric_limits<std::uint64_t>::max()) {
        lastGeneration = generation;
        lastDimension = dimension;
        return;
    }
    if (generation != lastGeneration || dimension != lastDimension) {
        lastGeneration = generation;
        lastDimension = dimension;
        resetState();
    }
}
std::optional<Point> cellAt(camera::Position p) {
    return Point{static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y)), static_cast<int>(std::floor(p.z))};
}
// The block the view stands in: the player's feet, or the camera's virtual
// feet (its position minus the player's eye height) in Freelook / FreeCamera.
std::optional<Point> viewFeet(IClientInstance& client, LocalPlayer& player) {
    auto ray = CameraSessions::instance().detachedViewRay(client);
    std::optional<camera::Position> detached;
    if (ray) detached = camera::Position{ray->x, ray->y, ray->z};
    auto eye = player.getEyePos();
    auto body = player.getFeetPos();
    auto point = camera::viewFeet(detached, {body.x, body.y, body.z}, {eye.x, eye.y, eye.z});
    if (!point) return std::nullopt;
    return cellAt(*point);
}
// The drawn view's axes: a detached Freelook or FreeCamera view owns the
// crosshair, so a step follows it rather than the body (L-230).
Axes viewAxes(IClientInstance& client) {
    if (auto ray = CameraSessions::instance().detachedViewRay(client)) {
        double length = std::sqrt(ray->dx * ray->dx + ray->dy * ray->dy + ray->dz * ray->dz);
        if (length > 1e-9) return moveAxes(ray->dx / length, ray->dy / length, ray->dz / length);
    }
    auto* player = client.getLocalPlayer();
    if (!player) return moveAxes(0, 0, 1);
    auto v = player->getViewVector(1.f);
    double length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (!(length > 1e-9)) return moveAxes(0, 0, 1);
    return moveAxes(v.x / length, v.y / length, v.z / length);
}
Point stepFor(IClientInstance& client, Action action) {
    auto axes = viewAxes(client);
    switch (action) {
    case Action::MovePlacementForward: return axes.forward;
    case Action::MovePlacementBack: return {-axes.forward.x, -axes.forward.y, -axes.forward.z};
    // Left and right stay horizontal even when forward points up or down.
    case Action::MovePlacementLeft: return {-axes.right.x, -axes.right.y, -axes.right.z};
    case Action::MovePlacementRight: return axes.right;
    case Action::MovePlacementUp: return axes.up;
    default: return {-axes.up.x, -axes.up.y, -axes.up.z};
    }
}
// Applies `apply` to the selected placement and reports `message` built from it.
template <class Apply, class Message>
void changeSelected(Apply&& apply, Message&& message) {
    std::string text;
    bool found = false;
    bool saved = session::change([&](PlacementSet& set) {
        if (set.selected < 0 || set.selected >= static_cast<int>(set.placements.size())) return false;
        found = true;
        auto& p = set.placements[static_cast<size_t>(set.selected)];
        apply(p);
        text = message(p);
        return true;
    });
    if (!found) { ui::showMessageToast(ui::translated("schematic.toast.noPlacement")); return; }
    ui::showMessageToast(saved ? text : ui::translated("schematic.saveError"));
}
// The selected placement's structure, looked up before session::change: the
// session's lock is held while a change runs, so nothing inside may call back.
std::shared_ptr<Structure const> selectedStructure() {
    auto set = session::current();
    if (set.selected < 0 || set.selected >= static_cast<int>(set.placements.size())) return nullptr;
    return session::structure(set.placements[static_cast<size_t>(set.selected)].file);
}
// A new-form cut plane depends on the placed size, so turning the placement
// while its structure is missing would leave the old fields stale. All
// layers do not depend on the turn.
bool turnRefused(Structure const* structure) {
    if (structure) return false;
    auto set = session::current();
    bool refused = set.selected >= 0 && set.selected < static_cast<int>(set.placements.size())
        && turnNeedsStructure(set.placements[static_cast<size_t>(set.selected)].layers);
    if (refused) ui::showMessageToast(ui::translated("schematic.rotationNeedsStructure"));
    return refused;
}
// Turns or mirrors the selected placement, keeping its cut plane attached.
template <class Turn, class Message>
void changeTurn(Turn&& turn, Message&& message) {
    auto structure = selectedStructure();
    if (turnRefused(structure.get())) return;
    Size const* size = structure ? &structure->size : nullptr;
    changeSelected([&](SavedPlacement& p) { turnPlacement(p.placement, p.layers, size, turn); }, message);
}
// Where the view ray enters a box, or nullopt when it misses within `reach`.
std::optional<double> enter(Vec3 const& eye, Vec3 const& dir, Point low, Size size, double reach) {
    double near = 0, far = reach;
    double o[3]{eye.x, eye.y, eye.z}, d[3]{dir.x, dir.y, dir.z};
    double lo[3]{static_cast<double>(low.x), static_cast<double>(low.y), static_cast<double>(low.z)};
    double hi[3]{lo[0] + size.x, lo[1] + size.y, lo[2] + size.z};
    for (int i = 0; i < 3; ++i) {
        if (std::abs(d[i]) < 1e-9) {
            if (o[i] < lo[i] || o[i] > hi[i]) return std::nullopt;
            continue;
        }
        double t1 = (lo[i] - o[i]) / d[i], t2 = (hi[i] - o[i]) / d[i];
        if (t1 > t2) std::swap(t1, t2);
        near = std::max(near, t1);
        far = std::min(far, t2);
        if (near > far) return std::nullopt;
    }
    return near;
}
// The first block along the drawn view, using the same solid-block filter as
// the target card so water and lava are looked through. The game's latest hit
// result follows the body, not a detached camera.
struct BlockHit {
    bool tile = false;
    Point block{};
    Vec3 pos{};
    double distance = std::numeric_limits<double>::max();
};
BlockHit cameraBlock(LocalPlayer& player, Vec3 const& eye, Vec3 const& dir) {
    BlockHit result;
    double length = std::sqrt(static_cast<double>(dir.x) * dir.x + static_cast<double>(dir.y) * dir.y
        + static_cast<double>(dir.z) * dir.z);
    if (!(length > 1e-9)) return result;
    constexpr int reach = 256;
    Vec3 to{eye.x + static_cast<float>(dir.x / length * reach), eye.y + static_cast<float>(dir.y / length * reach),
        eye.z + static_cast<float>(dir.z / length * reach)};
    auto& source = player.getDimensionBlockSource();
    auto hit = source.clip(eye, to, false, ShapeType::Outline, reach, false, false, nullptr,
        [](BlockSource const&, Block const& block, bool) {
            auto name = block.getTypeName();
            return name != "minecraft:water" && name != "minecraft:flowing_water" && name != "minecraft:lava"
                && name != "minecraft:flowing_lava";
        }, false);
    if (hit.mType != HitResultType::Tile) return result;
    result.tile = true;
    result.block = {hit.mBlock.x, hit.mBlock.y, hit.mBlock.z};
    result.pos = hit.mPos;
    double dx = hit.mPos.x - eye.x, dy = hit.mPos.y - eye.y, dz = hit.mPos.z - eye.z;
    result.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    return result;
}
struct View {
    Vec3 eye{};
    Vec3 dir{0, 0, 1};
    bool detached = false;
};
View view(IClientInstance& client, LocalPlayer& player) {
    View v;
    v.eye = player.getEyePos();
    v.dir = player.getViewVector(1.f);
    if (auto ray = CameraSessions::instance().detachedViewRay(client)) {
        v.eye = Vec3{static_cast<float>(ray->x), static_cast<float>(ray->y), static_cast<float>(ray->z)};
        v.dir = Vec3{static_cast<float>(ray->dx), static_cast<float>(ray->dy), static_cast<float>(ray->dz)};
        v.detached = true;
    }
    return v;
}
// The block the view picks: the game's crosshair in normal play (so block
// seams match what is outlined), our own ray for a detached camera.
BlockHit viewBlock(IClientInstance& client, LocalPlayer& player) {
    View v = view(client, player);
    if (v.detached) return cameraBlock(player, v.eye, v.dir);
    BlockHit result;
    auto const& latest = client.getLatestHitResult();
    if (latest.mType != HitResultType::Tile) return result;
    result.tile = true;
    result.block = {latest.mBlock.x, latest.mBlock.y, latest.mBlock.z};
    result.pos = latest.mPos;
    double dx = latest.mPos.x - v.eye.x, dy = latest.mPos.y - v.eye.y, dz = latest.mPos.z - v.eye.z;
    result.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    return result;
}
// The first block that fully hides what is behind it. Glass, plants and other
// see-through blocks do not count, so a corner behind them stays pickable.
BlockHit opaqueBlock(LocalPlayer& player, View const& v) {
    BlockHit result;
    double length = std::sqrt(static_cast<double>(v.dir.x) * v.dir.x + static_cast<double>(v.dir.y) * v.dir.y
        + static_cast<double>(v.dir.z) * v.dir.z);
    if (!(length > 1e-9)) return result;
    constexpr int reach = 256;
    Vec3 to{v.eye.x + static_cast<float>(v.dir.x / length * reach), v.eye.y + static_cast<float>(v.dir.y / length * reach),
        v.eye.z + static_cast<float>(v.dir.z / length * reach)};
    auto& source = player.getDimensionBlockSource();
    auto hit = source.clip(v.eye, to, false, ShapeType::Outline, reach, false, false, nullptr,
        [](BlockSource const&, Block const& block, bool) { return block.getBlockType().mIsOpaqueFullBlock; }, false);
    if (hit.mType != HitResultType::Tile) return result;
    result.tile = true;
    result.block = {hit.mBlock.x, hit.mBlock.y, hit.mBlock.z};
    result.pos = hit.mPos;
    double dx = hit.mPos.x - v.eye.x, dy = hit.mPos.y - v.eye.y, dz = hit.mPos.z - v.eye.z;
    result.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    return result;
}
// A target that overlaps the opaque block is not hidden by it: only a block
// strictly between the view and the target hides it.
bool cellOverlapsBox(Point cell, double lx, double ly, double lz, double hx, double hy, double hz) {
    return cell.x < hx && cell.x + 1.0 > lx && cell.y < hy && cell.y + 1.0 > ly && cell.z < hz && cell.z + 1.0 > lz;
}
// The move target the drawn view points at. A corner cell the ray passes
// through wins even in the air and behind see-through blocks; then the nearest
// placement box; then the save area when the ray enters it. A fully opaque
// block in front hides a target unless the target overlaps that block.
void selectLooked(IClientInstance& client, LocalPlayer& player) {
    int dimension = static_cast<int>(player.getDimensionId());
    View v = view(client, player);
    TargetRay ray{v.eye.x, v.eye.y, v.eye.z, v.dir.x, v.dir.y, v.dir.z, 256};
    auto set = session::current();
    auto state = selection::current();
    BlockHit opaque = opaqueBlock(player, v);
    double opaqueAt = opaque.tile ? opaque.distance : std::numeric_limits<double>::max();
    // A target on the face of an opaque block is still visible: only something
    // strictly in front of it hides it.
    constexpr double face = 1e-6;
    MoveTarget cornerTarget = MoveTarget::None;
    if (state.dimension == dimension) {
        auto first = state.first ? cellEntry(ray, state.first->x, state.first->y, state.first->z) : std::nullopt;
        auto second = state.second ? cellEntry(ray, state.second->x, state.second->y, state.second->z) : std::nullopt;
        auto visible = [&](std::optional<Point> const& cell, std::optional<double> const& entry) {
            if (!entry) return false;
            return *entry <= opaqueAt + face || (opaque.tile && cell && *cell == opaque.block);
        };
        bool firstVisible = visible(state.first, first);
        bool secondVisible = visible(state.second, second);
        if (firstVisible && secondVisible && std::abs(*first - *second) < 1e-9) {
            // Both corners in one cell: keep the current corner if it is one.
            cornerTarget = moveTarget == MoveTarget::Corner1 || moveTarget == MoveTarget::Corner2 ? moveTarget : MoveTarget::Corner1;
        } else if (firstVisible && (!secondVisible || *first < *second)) {
            cornerTarget = MoveTarget::Corner1;
        } else if (secondVisible) {
            cornerTarget = MoveTarget::Corner2;
        }
    }
    if (cornerTarget != MoveTarget::None) {
        changeTarget(cornerTarget);
        remembered.reset();
        ui::showMessageToast(ui::translated("schematic.menu.moveTarget") + ": " + targetName(cornerTarget));
        return;
    }
    int best = -1;
    double bestDistance = std::numeric_limits<double>::max();
    for (int i = 0; i < static_cast<int>(set.placements.size()); ++i) {
        auto const& p = set.placements[static_cast<size_t>(i)];
        auto structure = p.dimension == dimension && p.visible ? session::structure(p.file) : nullptr;
        if (!structure) continue;
        auto size = placedSize(structure->size, p.placement.rotation);
        auto distance = enter(v.eye, v.dir, p.placement.origin, size, 256);
        bool overlaps = opaque.tile && cellOverlapsBox(opaque.block, p.placement.origin.x, p.placement.origin.y,
            p.placement.origin.z, p.placement.origin.x + size.x, p.placement.origin.y + size.y, p.placement.origin.z + size.z);
        if (distance && *distance < bestDistance && (overlaps || *distance <= opaqueAt + face)) { best = i; bestDistance = *distance; }
    }
    if (best >= 0) {
        session::change([&](PlacementSet& s) {
            if (s.selected == best) return false;
            s.selected = best;
            return true;
        });
        placementSelected();
        ui::showMessageToast(ui::translated("schematic.toast.selected", set.placements[static_cast<size_t>(best)].name));
        return;
    }
    if (state.dimension == dimension && state.first && state.second) {
        Area area{*state.first, *state.second};
        auto low = area.low();
        auto size = area.size();
        auto entry = boxEntry(ray, low.x, low.y, low.z, low.x + size.x, low.y + size.y, low.z + size.z);
        bool overlaps = opaque.tile && cellOverlapsBox(opaque.block, low.x, low.y, low.z, low.x + size.x, low.y + size.y, low.z + size.z);
        if (entry && (overlaps || *entry <= opaqueAt + face)) {
            changeTarget(MoveTarget::Area);
            remembered.reset();
            ui::showMessageToast(ui::translated("schematic.menu.moveTarget") + ": " + targetName(MoveTarget::Area));
            return;
        }
    }
    ui::showMessageToast(ui::translated("schematic.toast.notLooking"));
}
// Always the mistake nearest to where the player is now.
void nearestMistake(LocalPlayer& player) {
    auto set = session::current();
    auto result = ghosts::verification();
    if (set.selected < 0) { ui::showMessageToast(ui::translated("schematic.toast.noPlacement")); return; }
    if (result->placement != set.selected || !result->complete || result->stale) { ui::showMessageToast(ui::translated("schematic.toast.counting")); return; }
    auto p = player.getFeetPos();
    Mismatch const* nearest = nullptr;
    double best = std::numeric_limits<double>::max();
    for (auto const& m : result->mismatches) {
        if (m.state == CellState::Missing) continue;
        double dx = m.position.x + .5 - p.x, dy = m.position.y + .5 - p.y, dz = m.position.z + .5 - p.z;
        double d = dx * dx + dy * dy + dz * dz;
        if (d < best) { best = d; nearest = &m; }
    }
    if (!nearest) { ui::showMessageToast(ui::translated("schematic.toast.noMistakes")); return; }
    ghosts::point(nearest->position);
    char const* kind = nearest->state == CellState::Wrong ? "schematic.kind.wrong"
        : nearest->state == CellState::Extra ? "schematic.kind.extra" : "schematic.kind.state";
    ui::showMessageToast(ui::translated("schematic.toast.nearest", ui::translated(kind), static_cast<int>(std::lround(std::sqrt(best)))));
}
// A corner of the area to save: the block in the crosshair.
void setCorner(IClientInstance& client, LocalPlayer& player, int which) {
    BlockHit hit = viewBlock(client, player);
    if (!hit.tile) { ui::showMessageToast(ui::translated("schematic.toast.lookAtBlock")); return; }
    Point at = hit.block;
    selection::setCorner(which, at, static_cast<int>(player.getDimensionId()));
    if (auto area = selection::current().area()) {
        auto size = area->size();
        ui::showMessageToast(ui::translated("schematic.toast.cornerArea", which + 1, at.x, at.y, at.z, size.x, size.y, size.z));
    } else ui::showMessageToast(ui::translated("schematic.toast.corner", which + 1, at.x, at.y, at.z));
}
std::atomic<bool> adjustHeld{false};
std::atomic<int> pendingWheel{0};
std::atomic<IClientInstance*> adjustClient{nullptr};
ll::event::ListenerPtr wheelListener;

std::string targetName(MoveTarget target) {
    static constexpr std::array<char const*, 5> keys{"schematic.target.placement", "schematic.target.corner1",
        "schematic.target.corner2", "schematic.target.area", "schematic.target.none"};
    return ui::translated(keys[static_cast<size_t>(target)]);
}
// Which targets exist now: the selected placement and the corners/area must be
// in the player's dimension.
TargetValidity validity(LocalPlayer& player) {
    auto set = session::current();
    int dimension = static_cast<int>(player.getDimensionId());
    TargetValidity v;
    v.placement = set.selected >= 0 && set.selected < static_cast<int>(set.placements.size())
        && set.placements[static_cast<size_t>(set.selected)].dimension == dimension;
    auto state = selection::current();
    bool same = state.dimension == dimension;
    v.corner1 = same && state.first.has_value();
    v.corner2 = same && state.second.has_value();
    v.area = same && state.first && state.second;
    return v;
}
// The target a move acts on; an invalid remembered target falls back to the
// first valid one and drops the repeat memory.
MoveTarget resolveTarget(LocalPlayer& player) {
    TargetValidity v = validity(player);
    if (valid(moveTarget, v)) return moveTarget;
    moveTarget = fallback(v);
    remembered.reset();
    return moveTarget;
}
void changeTarget(MoveTarget next) {
    if (next == moveTarget) return;
    moveTarget = next;
    remembered.reset();
}
// Moves the move target by `d`: the selected placement, a corner or both.
void moveBy(LocalPlayer& player, Point d) {
    auto shifted = [&](Point p) { return Point{p.x + d.x, p.y + d.y, p.z + d.z}; };
    MoveTarget target = resolveTarget(player);
    if (target == MoveTarget::None) {
        ui::showMessageToast(ui::translated("schematic.toast.noTarget"));
        return;
    }
    if (target == MoveTarget::Placement) {
        changeSelected([&](SavedPlacement& p) { p.placement.origin = shifted(p.placement.origin); }, [](SavedPlacement const& p) {
            return ui::translated("schematic.toast.moved", p.name, p.placement.origin.x, p.placement.origin.y, p.placement.origin.z);
        });
        return;
    }
    auto state = selection::current();
    int dimension = static_cast<int>(player.getDimensionId());
    // An area in another dimension is not moved from here.
    if (state.dimension != dimension) { ui::showMessageToast(ui::translated("schematic.toast.noArea")); return; }
    auto corner = [&](std::optional<Point> const& c, int which) {
        if (!c) return false;
        selection::setCorner(which, shifted(*c), state.dimension);
        return true;
    };
    bool moved = false;
    if (target == MoveTarget::Corner1 || target == MoveTarget::Area) moved = corner(state.first, 0) || moved;
    if (target == MoveTarget::Corner2 || target == MoveTarget::Area) moved = corner(state.second, 1) || moved;
    if (!moved) { ui::showMessageToast(ui::translated("schematic.toast.noArea")); return; }
    auto now = selection::current();
    auto const& shown = target == MoveTarget::Corner2 ? now.second : now.first;
    if (shown) ui::showMessageToast(targetName(target) + ": " + ui::translated("schematic.toast.corner",
        target == MoveTarget::Corner2 ? 2 : 1, shown->x, shown->y, shown->z));
}
// Moves the target to the feet: the placement's origin, one corner, or the
// whole area with corner 1 at the feet (corner 2 follows).
void moveTargetToFeet(IClientInstance& client, LocalPlayer& player) {
    auto at = viewFeet(client, player);
    if (!at) return;
    MoveTarget target = resolveTarget(player);
    if (target == MoveTarget::None) {
        ui::showMessageToast(ui::translated("schematic.toast.noTarget"));
        return;
    }
    int dimension = static_cast<int>(player.getDimensionId());
    if (target == MoveTarget::Placement) {
        changeSelected([&](SavedPlacement& p) { p.placement.origin = *at; p.dimension = dimension; }, [](SavedPlacement const& p) {
            return ui::translated("schematic.toast.moved", p.name, p.placement.origin.x, p.placement.origin.y, p.placement.origin.z);
        });
        return;
    }
    if (target == MoveTarget::Corner1 || target == MoveTarget::Corner2) {
        int which = target == MoveTarget::Corner2 ? 1 : 0;
        selection::setCorner(which, *at, dimension);
        ui::showMessageToast(targetName(target) + ": " + ui::translated("schematic.toast.corner", which + 1, at->x, at->y, at->z));
        return;
    }
    auto state = selection::current();
    if (!state.first) { ui::showMessageToast(ui::translated("schematic.toast.noArea")); return; }
    moveBy(player, {at->x - state.first->x, at->y - state.first->y, at->z - state.first->z});
}
std::string mirrorName(Mirror mirror) {
    return ui::translated(mirror == Mirror::X ? "schematic.mirror.x" : mirror == Mirror::Z ? "schematic.mirror.z" : "schematic.mirror.none");
}
}
std::string axisText(LayerAxis axis) {
    return ui::translated(axis == LayerAxis::Y ? "schematic.axis.y" : axis == LayerAxis::X ? "schematic.axis.x" : "schematic.axis.z");
}
std::string modeText(LayerMode mode) {
    return ui::translated(mode == LayerMode::Only ? "schematic.mode.only"
        : mode == LayerMode::UpTo ? "schematic.mode.upTo" : "schematic.mode.all");
}
std::string keepText(LayerKeep keep) {
    return ui::translated(keep == LayerKeep::High ? "schematic.keep.high" : "schematic.keep.low");
}
namespace {
std::string layerChangeText(Structure const* structure, SavedPlacement const& p, menu::Stepper stepper, bool leftAll) {
    if (!structure) return p.name;
    Layers view = resolvedLayers(p.layers, structure->size, p.placement);
    int count = std::max(1, layerCount(structure->size, view.axis));
    switch (stepper) {
    case menu::Stepper::LayerAxis:
        return ui::translated("schematic.toast.axis", p.name, axisText(view.axis), view.cut + 1, count);
    case menu::Stepper::LayerMode:
        return ui::translated("schematic.toast.mode", p.name, modeText(view.mode), view.cut + 1, count);
    case menu::Stepper::LayerKeep:
        return ui::translated("schematic.toast.keep", p.name, keepText(view.keep), view.cut + 1, count);
    default:
        return leftAll ? ui::translated("schematic.toast.mode", p.name, modeText(view.mode), view.cut + 1, count)
                       : ui::translated("schematic.toast.cut", p.name, view.cut + 1, count, axisText(view.axis));
    }
}
std::string cutHereText(Structure const* structure, SavedPlacement const& p, bool leftAll) {
    if (leftAll) return layerChangeText(structure, p, menu::Stepper::LayerMode, false);
    if (!structure) return p.name;
    Layers view = resolvedLayers(p.layers, structure->size, p.placement);
    int count = std::max(1, layerCount(structure->size, view.axis));
    return ui::translated("schematic.toast.cutHere", p.name, view.cut + 1, count, axisText(view.axis));
}
Layers effectiveLayers(SavedPlacement const& p) {
    auto structure = session::structure(p.file);
    return structure ? resolvedLayers(p.layers, structure->size, p.placement) : p.layers;
}
// One layer stepper step. The menu remembers it for Repeat; the dedicated keys
// do not (Repeat is menu-only).
void applyLayerStep(menu::Stepper stepper, int amount) {
    auto structure = selectedStructure();
    if (!structure) { ui::showMessageToast(ui::translated("schematic.layerNeedsStructure")); return; }
    bool leftAll = false;
    changeSelected([&](SavedPlacement& p) {
        if (p.layers.legacy) p.layers = fromLegacy(p.layers, structure->size, p.placement);
        if (stepper == menu::Stepper::LayerAxis) {
            // Changing the axis keeps the cut; only Cut here moves it.
            p.layers.axis = static_cast<LayerAxis>(((static_cast<int>(p.layers.axis) + amount) % 3 + 3) % 3);
        } else if (stepper == menu::Stepper::LayerMode) {
            p.layers.mode = static_cast<LayerMode>(((static_cast<int>(p.layers.mode) + amount) % 3 + 3) % 3);
        } else if (stepper == menu::Stepper::LayerKeep) {
            // A wheel can deliver several notches in one frame; keep the side
            // for an even count, toggle for an odd one.
            if (amount % 2 != 0)
                p.layers.keep = p.layers.keep == LayerKeep::Low ? LayerKeep::High : LayerKeep::Low;
        } else {
            leftAll = leaveAll(p.layers);
            if (!leftAll) p.layers.cut += amount;
        }
        p.layers.cut = std::clamp(p.layers.cut, 0, std::max(0, layerCount(structure->size, p.layers.axis) - 1));
        if (stepper != menu::Stepper::LayerMode) noteLayerEdit(p.layers);
        dualWrite(p.layers, structure->size, p.placement);
    }, [&](SavedPlacement const& p) { return layerChangeText(structure.get(), p, stepper, leftAll); });
}
}

std::optional<Point> viewFeet(IClientInstance& client) {
    auto* player = client.getLocalPlayer();
    if (!player) return std::nullopt;
    return viewFeet(client, *player);
}

bool handles(Action action) {
    switch (action) {
    case Action::NearestMistake: case Action::SelectLookedPlacement: case Action::NextPlacement: case Action::CycleMoveTarget:
    case Action::MovePlacementForward: case Action::MovePlacementBack: case Action::MovePlacementHere:
    case Action::MovePlacementLeft: case Action::MovePlacementRight: case Action::MovePlacementUp: case Action::MovePlacementDown:
    case Action::RotatePlacement: case Action::MirrorPlacement:
    case Action::LayerUp: case Action::LayerDown: case Action::LayerHere: case Action::CycleLayerAxis:
    case Action::CycleLayerMode: case Action::ToggleLayerKeep:
    case Action::SchematicCorner1: case Action::SchematicCorner2:
    case Action::ResetPlacementTurn: case Action::RotatePlacementBack: case Action::PreviousPlacement:
    case Action::TogglePlacementShown: case Action::TogglePlacementExtras: case Action::TogglePlacementEntities:
    case Action::ClearSchematicArea:
        return true;
    default: return false;
    }
}

void press(IClientInstance& client, Action action) {
    auto* player = client.getLocalPlayer();
    if (!player) return;
    syncLifecycle(client);
    switch (action) {
    case Action::NearestMistake: nearestMistake(*player); return;
    case Action::SelectLookedPlacement: selectLooked(client, *player); return;
    case Action::SchematicCorner1: setCorner(client, *player, 0); return;
    case Action::SchematicCorner2: setCorner(client, *player, 1); return;
    case Action::CycleMoveTarget: {
        MoveTarget next = nextValid(moveTarget, validity(*player));
        changeTarget(next);
        ui::showMessageToast(ui::translated("schematic.menu.moveTarget") + ": " + targetName(next));
        return;
    }
    case Action::NextPlacement: case Action::PreviousPlacement: {
        int direction = action == Action::NextPlacement ? 1 : -1;
        std::string name;
        bool any = session::change([&](PlacementSet& set) {
            if (set.placements.empty()) return false;
            int count = static_cast<int>(set.placements.size());
            set.selected = steppedPlacement(set.selected, count, direction);
            name = set.placements[static_cast<size_t>(set.selected)].name;
            return true;
        });
        ui::showMessageToast(any ? ui::translated("schematic.toast.selected", name) : ui::translated("schematic.toast.noPlacement"));
        if (any) placementSelected();
        return;
    }
    case Action::MovePlacementForward: case Action::MovePlacementBack: case Action::MovePlacementLeft:
    case Action::MovePlacementRight: case Action::MovePlacementUp: case Action::MovePlacementDown: {
        moveBy(*player, stepFor(client, action));
        return;
    }
    case Action::MovePlacementHere: {
        moveTargetToFeet(client, *player);
        return;
    }
    case Action::RotatePlacement: case Action::RotatePlacementBack: {
        int turns = action == Action::RotatePlacement ? 1 : -1;
        changeTurn([turns](Placement& t) { t.rotation = quarterTurns(t.rotation + turns); },
            [](SavedPlacement const& p) { return ui::translated("schematic.toast.rotated", p.name, p.placement.rotation * 90); });
        return;
    }
    case Action::MirrorPlacement:
        changeTurn([](Placement& t) { t.mirror = static_cast<Mirror>((static_cast<int>(t.mirror) + 1) % 3); },
            [](SavedPlacement const& p) { return ui::translated("schematic.toast.mirror", p.name, mirrorName(p.placement.mirror)); });
        return;
    case Action::LayerUp: case Action::LayerDown: {
        int direction = action == Action::LayerUp ? 1 : -1;
        auto structure = selectedStructure();
        if (!structure) { ui::showMessageToast(ui::translated("schematic.layerNeedsStructure")); return; }
        bool leftAll = false;
        changeSelected([&](SavedPlacement& p) {
            if (p.layers.legacy) p.layers = fromLegacy(p.layers, structure->size, p.placement);
            leftAll = leaveAll(p.layers);
            if (!leftAll) {
                p.layers.cut = std::clamp(p.layers.cut + direction, 0, std::max(0, layerCount(structure->size, p.layers.axis) - 1));
            }
            noteLayerEdit(p.layers);
            dualWrite(p.layers, structure->size, p.placement);
        }, [&](SavedPlacement const& p) { return layerChangeText(structure.get(), p, menu::Stepper::Layer, leftAll); });
        return;
    }
    case Action::LayerHere: {
        auto at = viewFeet(client, *player);
        auto structure = selectedStructure();
        if (!structure) { ui::showMessageToast(ui::translated("schematic.layerNeedsStructure")); return; }
        if (!at) return;
        bool leftAll = false;
        changeSelected([&](SavedPlacement& p) {
            if (p.layers.legacy) p.layers = fromLegacy(p.layers, structure->size, p.placement);
            p.layers.cut = cutAlong(structure->size, p.placement, p.layers.axis, *at);
            leftAll = leaveAll(p.layers);
            noteLayerEdit(p.layers);
            dualWrite(p.layers, structure->size, p.placement);
        }, [&](SavedPlacement const& p) { return cutHereText(structure.get(), p, leftAll); });
        return;
    }
    case Action::CycleLayerAxis: applyLayerStep(menu::Stepper::LayerAxis, 1); return;
    case Action::CycleLayerMode: applyLayerStep(menu::Stepper::LayerMode, 1); return;
    case Action::ToggleLayerKeep: applyLayerStep(menu::Stepper::LayerKeep, 1); return;
    // The menu's commands, as keys (L-238).
    case Action::ResetPlacementTurn: run(client, menu::Command::ResetTurn); return;
    case Action::TogglePlacementShown: run(client, menu::Command::ToggleShown); return;
    case Action::TogglePlacementExtras: run(client, menu::Command::ToggleExtras); return;
    case Action::TogglePlacementEntities: run(client, menu::Command::ToggleEntities); return;
    case Action::ClearSchematicArea: run(client, menu::Command::ClearArea); return;
    default: return;
    }
}

MoveTarget target() { return moveTarget; }
void setTarget(IClientInstance& client, menu::Target value) {
    MoveTarget next = static_cast<MoveTarget>(value);
    auto* player = client.getLocalPlayer();
    syncLifecycle(client);
    if (!player || !valid(next, validity(*player))) { ui::showMessageToast(ui::translated("schematic.toast.targetUnset")); return; }
    changeTarget(next);
    ui::showMessageToast(ui::translated("schematic.menu.moveTarget") + ": " + targetName(next));
}
void placementSelected() {
    moveTarget = MoveTarget::Placement;
    remembered.reset();
}
void resetState() { placementSelected(); }

void step(IClientInstance& client, menu::Stepper stepper, int amount) {
    using S = menu::Stepper;
    auto* player = client.getLocalPlayer();
    if (!player || !amount) return;
    syncLifecycle(client);
    bool all = Runtime::instance().preferences().schematic.repeatAllSteps;
    // Validate a step before remembering it: a rejected layer or turn must not
    // replace the last successful adjustment for Repeat.
    bool layerStep = stepper == S::LayerAxis || stepper == S::LayerMode || stepper == S::Layer || stepper == S::LayerKeep;
    bool turnStep = stepper == S::Rotate || stepper == S::Mirror;
    if (layerStep && !selectedStructure()) {
        ui::showMessageToast(ui::translated("schematic.layerNeedsStructure"));
        return;
    }
    if (turnStep && turnRefused(selectedStructure().get())) return;
    if (all ? menu::repeatableAll(stepper) : menu::repeatable(stepper)) {
        // Resolve a fallback target before remembering the step, or the
        // fallback would clear the adjustment this very move should remember.
        bool moving = stepper == S::ForwardBack || stepper == S::LeftRight || stepper == S::UpDown;
        if (moving && resolveTarget(*player) == MoveTarget::None) remembered.reset();
        else remembered = stepper;
    }
    switch (stepper) {
    case S::Target: {
        TargetValidity v = validity(*player);
        if (v.placement || v.corner1 || v.corner2 || v.area) {
            int turns = std::abs(amount);
            for (int i = 0; i < turns; ++i)
                moveTarget = amount > 0 ? nextValid(moveTarget, v) : previousValid(moveTarget, v);
        } else moveTarget = MoveTarget::None;
        remembered.reset();
        ui::showMessageToast(ui::translated("schematic.menu.moveTarget") + ": " + targetName(moveTarget));
        return;
    }
    case S::ForwardBack: case S::LeftRight: {
        auto axes = viewAxes(client);
        moveBy(*player, scaled(stepper == S::ForwardBack ? axes.forward : axes.right, amount));
        return;
    }
    case S::UpDown: moveBy(*player, scaled(viewAxes(client).up, amount)); return;
    case S::Rotate:
        changeTurn([&](Placement& t) { t.rotation = quarterTurns(t.rotation + amount); },
            [](SavedPlacement const& p) { return ui::translated("schematic.toast.rotated", p.name, p.placement.rotation * 90); });
        return;
    case S::Mirror:
        changeTurn([&](Placement& t) { t.mirror = static_cast<Mirror>(((static_cast<int>(t.mirror) + amount) % 3 + 3) % 3); },
            [](SavedPlacement const& p) { return ui::translated("schematic.toast.mirror", p.name, mirrorName(p.placement.mirror)); });
        return;
    case S::LayerAxis: case S::LayerMode: case S::Layer: case S::LayerKeep:
        applyLayerStep(stepper, amount);
        return;
    case S::Placement: {
        std::string name;
        bool any = session::change([&](PlacementSet& set) {
            if (set.placements.empty()) return false;
            int count = static_cast<int>(set.placements.size());
            set.selected = steppedPlacement(set.selected, count, amount);
            name = set.placements[static_cast<size_t>(set.selected)].name;
            return true;
        });
        ui::showMessageToast(any ? ui::translated("schematic.toast.selected", name) : ui::translated("schematic.toast.noPlacement"));
        if (any) placementSelected();
        return;
    }
    }
}

void run(IClientInstance& client, menu::Command command) {
    using C = menu::Command;
    auto* player = client.getLocalPlayer();
    if (!player) return;
    switch (command) {
    case C::ToFeet:
        moveTargetToFeet(client, *player);
        return;
    case C::ResetTurn:
        changeTurn([](Placement& t) { t.rotation = 0; t.mirror = Mirror::None; },
            [](SavedPlacement const& p) { return ui::translated("schematic.toast.rotated", p.name, 0); });
        return;
    case C::LayerHere: press(client, Action::LayerHere); return;
    case C::ToggleShown:
        changeSelected([](SavedPlacement& p) { p.visible = !p.visible; },
            [](SavedPlacement const& p) { return p.name + ": " + ui::translated("schematic.menu.toggleShown") + " " + ui::translated(p.visible ? "on" : "off"); });
        return;
    case C::ToggleExtras:
        changeSelected([](SavedPlacement& p) { p.countExtras = !p.countExtras; },
            [](SavedPlacement const& p) { return p.name + ": " + ui::translated("schematic.menu.toggleExtras") + " " + ui::translated(p.countExtras ? "schematic.extras.show" : "schematic.extras.ignore"); });
        return;
    case C::ToggleEntities:
        changeSelected([](SavedPlacement& p) { p.entities = !p.entities; },
            [](SavedPlacement const& p) { return p.name + ": " + ui::translated("schematic.menu.toggleEntities") + " " + ui::translated(p.entities ? "on" : "off"); });
        return;
    case C::Corner1Here: press(client, Action::SchematicCorner1); return;
    case C::Corner2Here: press(client, Action::SchematicCorner2); return;
    case C::ClearArea:
        selection::clear();
        changeTarget(fallback(validity(*player)));
        remembered.reset();
        ui::showMessageToast(ui::translated("schematic.toast.areaCleared"));
        return;
    case C::SelectLooked: press(client, Action::SelectLookedPlacement); return;
    case C::NearestMistake: press(client, Action::NearestMistake); return;
    default: return; // screen commands
    }
}

std::string value(menu::Stepper stepper) {
    using S = menu::Stepper;
    auto set = session::current();
    SavedPlacement const* p = set.selected >= 0 && set.selected < static_cast<int>(set.placements.size())
        ? &set.placements[static_cast<size_t>(set.selected)] : nullptr;
    auto structure = p ? session::structure(p->file) : std::shared_ptr<Structure const>{};
    switch (stepper) {
    case S::Target: return targetName(moveTarget);
    case S::Placement: return p ? p->name : std::string{};
    case S::Rotate: return p ? std::format("{}°", p->placement.rotation * 90) : std::string{};
    case S::Mirror: return p ? mirrorName(p->placement.mirror) : std::string{};
    // No size: a layer view cannot be resolved; do not show guessed values.
    case S::LayerAxis:
        return p ? (structure ? axisText(effectiveLayers(*p).axis) : std::string("—")) : std::string{};
    case S::LayerMode: return p ? (structure ? modeText(effectiveLayers(*p).mode) : std::string("—")) : std::string{};
    case S::LayerKeep: return p ? (structure ? keepText(effectiveLayers(*p).keep) : std::string("—")) : std::string{};
    case S::Layer: {
        if (!p) return {};
        if (!structure) return "—";
        auto view = effectiveLayers(*p);
        return ui::translated("schematic.layerValue", view.cut + 1,
            std::max(1, layerCount(structure->size, view.axis)));
    }
    default: return {};
    }
}

// What the adjust key repeats, for its toast: the item's name, and the
// move target or the current value.
std::string adjustText() {
    if (!remembered) return ui::translated("schematic.adjust.none");
    std::string name;
    for (auto const& category : menu::categories)
        for (auto const& item : category.items)
            if (item.stepper() && std::get<menu::Stepper>(item.what) == *remembered && name.empty()) name = ui::translated(item.label);
    bool moving = *remembered == menu::Stepper::ForwardBack || *remembered == menu::Stepper::LeftRight || *remembered == menu::Stepper::UpDown;
    auto shown = moving ? value(menu::Stepper::Target) : value(*remembered);
    return ui::translated("schematic.adjust.hint", shown.empty() ? name : ui::translated("schematic.withDetail", name, shown));
}
void setAdjustHeld(bool held) {
    // Pressing it says what the wheel will repeat, like other toggles' toasts.
    if (held && !adjustHeld.exchange(true)) ui::showMessageToast(adjustText());
    adjustHeld = held;
    if (!held) pendingWheel = 0;
}
std::optional<menu::Stepper> lastStepper() { return remembered; }
void startAdjust() {
    if (wheelListener) return;
    // The wheel arrives on the window's input thread: only count it here;
    // adjustFrame applies it on the client thread.
    wheelListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::input::MouseInputEvent>([](auto& event) {
        if (!adjustHeld.load() || event.actionButtonId() != MouseAction::ActionWheel || event.buttonData() == 0) return;
        auto* current = adjustClient.load();
        if (!current || !lamium::gameplayScreen(current->getScreenName())) return;
        pendingWheel += event.buttonData() > 0 ? 1 : -1;
        event.cancel();
    });
}
void stopAdjust() {
    if (wheelListener) {
        ll::event::EventBus::getInstance().removeListener(wheelListener);
        wheelListener.reset();
    }
    adjustHeld = false;
    pendingWheel = 0;
    remembered.reset();
    moveTarget = MoveTarget::Placement;
}
void adjustFrame(MinecraftUIRenderContext& context, float, float) {
    IClientInstance& client = context.mClient;
    adjustClient = &client;
    syncLifecycle(client);
    std::vector<UnreadablePlacement> unreadable;
    for (auto const& p : session::unreadable()) unreadable.push_back({p.id, p.name});
    auto names = unreadableNotice.update(session::generation(), Runtime::instance().preferences().schematic.enabled, unreadable);
    if (!names.empty()) {
        std::string shown = names.front();
        if (names.size() > 1) shown += ", " + names[1];
        if (names.size() > 2) shown += "...";
        ui::showWarningToast(ui::translated("schematic.toast.unreadable", names.size(), shown));
    }
    if (!adjustHeld.load()) return;
    // One toast: what the wheel repeats, then what the step reported, so the
    // step's own toast does not replace the first at once.
    if (int turns = pendingWheel.exchange(0); turns && remembered) {
        bool all = Runtime::instance().preferences().schematic.repeatAllSteps;
        if (!(all ? menu::repeatableAll(*remembered) : menu::repeatable(*remembered))) {
            remembered.reset();
            ui::showMessageToast(ui::translated("schematic.adjust.none"));
            return;
        }
        step(client, *remembered, turns);
        auto shown = ui::currentToggleToast(ui::toastNow());
        auto prefix = adjustText();
        ui::showMessageToast(shown && shown->text != prefix ? prefix + "\n" + shown->text : prefix);
    }
}
}

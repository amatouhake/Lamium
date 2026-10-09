#pragma once
// Missing schematic entities drawn as their game models with light-blue part
// outlines, without a live entity (BACKLOG L-115). The model comes from the
// data-driven renderer named like the entity; the pose from the bone rest
// values and the constant parts of the entity's setup animations.
#include "features/schematic/Placement.h"
#include "mc/deps/core/math/Vec3.h"
#include <functional>
#include <string>
#include <vector>

class IClientInstance;
class ScreenContext;
namespace lamium::schematic::models {
struct Spot {
    Position at;            // world position of the feet
    std::string identifier; // minecraft:wolf
    float yaw = 0;          // world facing, degrees (0 = south)
};
// Draws the spots whose entity has a model, camera-relative, each inside
// `inWorld` (which pushes the world matrix). Returns which spots were drawn;
// the rest keep the caller's stand-in. `outlines`: the light-blue part
// outlines of world ghosts (off in the screen previews).
std::vector<bool> draw(ScreenContext& screen, IClientInstance& client, Vec3 const& camera, std::vector<Spot> const& spots,
                       std::function<void(std::function<void()> const&)> const& inWorld, bool outlines = true);
// Forgets models, poses and measurements (world exit, resource reload).
void reset();
} // namespace lamium::schematic::models

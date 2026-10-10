#pragma once
// What the schematic draws in the world besides the ghost meshes: the area
// chosen for saving, a waiting save's columns, the placement frames, missing
// entities and their name tags, and the "Show in world" point.
#include "features/schematic/ResolvedPlacement.h"
#include "features/schematic/SchematicSession.h"
#include <glm/vec3.hpp>
#include <functional>
#include <vector>
class BaseActorRenderer;
class BlockSource;
class IClientInstance;
class ScreenContext;
class Vec3;
namespace mce { class MaterialPtr; }
namespace lamium::schematic::ghosts {
// Draws with the world matrix moved by `offset` (camera-relative) and pulled
// toward the eye (Depth.h), popped again also when the draw throws.
void translated(ScreenContext& screen, glm::vec3 offset, std::function<void()> const& draw);
// `twoSided` (overlay::FaceMaterial): the material culls, so each face also
// gets its reversed quad.
void drawSelection(ScreenContext& screen, Vec3 const& camera, int dimension, mce::MaterialPtr const& faceMaterial, bool twoSided);
void drawWaitingColumns(ScreenContext& screen, Vec3 const& camera, mce::MaterialPtr const& faceMaterial, bool twoSided);
void drawPlacementFrames(ScreenContext& screen, session::Snapshot const& snapshot, int dimension, Vec3 const& camera);
void drawEntities(ScreenContext& screen, IClientInstance& client, session::Snapshot const& snapshot, int dimension, Vec3 const& camera,
                  std::vector<Resolved> const& resolved);
void drawNameTags(ScreenContext& screen, IClientInstance& client, BlockSource& region, BaseActorRenderer& renderer, Vec3 const& camera);
void drawPoint(ScreenContext& screen, Vec3 const& camera, mce::MaterialPtr const& faceMaterial, bool twoSided);
// Drops the frame mesh and the name tags (world exit, dimension change).
void resetMarks();
}

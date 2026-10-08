#pragma once
// The schematic screen's 3D preview (BACKLOG L-114): a structure's blocks,
// tessellated by the game's block tessellator and drawn into a UI box.
#include "features/schematic/PreviewView.h"
#include "features/schematic/Structure.h"
#include <memory>

class MinecraftUIRenderContext;
namespace lamium::schematic::preview {
// Draws `structure` into the box seen from `view`. False when nothing could
// be drawn (no world to tessellate with, too many blocks, render path
// unavailable); the caller keeps its text.
bool draw(MinecraftUIRenderContext& context, std::shared_ptr<Structure const> const& structure, float x, float y, float width, float height,
          View view);
// Forgets the built mesh (world exit, screen closed).
void reset();
} // namespace lamium::schematic::preview

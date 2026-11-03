#include "features/schematic/SchematicRegion.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/chunk/ChunkSource.h"
#include "mc/world/level/dimension/Dimension.h"

namespace lamium::schematic {

// Not public and without ticking changes: nothing is ever written through it.
SchematicRegion::SchematicRegion(::BlockSource& world)
: ::BlockSource(world.getLevel(), world.getDimension(), world.getChunkSource(), false, false, false, false),
  world(world) {}

SchematicRegion::~SchematicRegion() = default;

::Block const& SchematicRegion::getBlock(::BlockPos const& pos) const {
    if (answer)
        if (auto const* block = answer(pos)) return *block;
    return world.getBlock(pos);
}

::Block const& SchematicRegion::getBlock(::BlockPos const& pos, uint layer) const {
    return layer == 0 ? getBlock(pos) : world.getBlock(pos, layer);
}

::Material const& SchematicRegion::getMaterial(::BlockPos const& pos) const { return getBlock(pos).getMaterial(); }

::Material const& SchematicRegion::getMaterial(int x, int y, int z) const { return getBlock(::BlockPos{x, y, z}).getMaterial(); }

float SchematicRegion::getBrightness(::BlockPos const& pos) const { return fullLight ? 1.f : world.getBrightness(pos); }

} // namespace lamium::schematic

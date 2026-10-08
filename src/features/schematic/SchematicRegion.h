#pragma once
// A block source for tessellating schematic blocks against the schematic's
// own neighbors: doors find their other half, fences and panes connect to
// the blocks the file puts beside them. Each position is asked of `answer`
// first; where it has nothing (nullptr, or no answer set) the real world
// answers. Lives for one frame or one build job, on the thread that made it.
#include "mc/world/level/BlockSource.h"
#include <functional>

namespace lamium::schematic {

class SchematicRegion final : public ::BlockSource {
public:
    using Answer = std::function<::Block const*(::BlockPos const&)>;
    explicit SchematicRegion(::BlockSource& world);
    ~SchematicRegion() override;

    Answer answer;

    ::Block const& getBlock(::BlockPos const& pos) const override;
    ::Block const& getBlock(::BlockPos const& pos, uint layer) const override;
    ::Material const& getMaterial(::BlockPos const& pos) const override;
    ::Material const& getMaterial(int x, int y, int z) const override;

private:
    ::BlockSource& world;
};

} // namespace lamium::schematic

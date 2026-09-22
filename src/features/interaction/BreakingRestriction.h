#pragma once
#include "features/interaction/RestrictionRegion.h"
class IClientInstance;
class Player;
class BlockPos;
namespace lamium::interaction::breaking {
void start();
void stop();
void reset();
void capture(IClientInstance&);
bool allows(Player&, BlockPos const&);
std::optional<RestrictionRegion> region();
}

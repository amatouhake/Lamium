#pragma once
#include "features/inventory/game/ResponseBarrier.h"
namespace lamium::inventory::game {
void installRequestTracker();
void removeRequestTracker();
void beginTransfer();
void endTransfer();
ResponseBarrier::Result transferResult();
}

#pragma once
#include "features/inventory/game/ResponseBarrier.h"
class ContainerManagerController;
namespace lamium::inventory::game {
void installRequestTracker();
void removeRequestTracker();
bool beginTransfer(ContainerManagerController& controller);
void endTransfer();
ResponseBarrier::Result transferResult();
}

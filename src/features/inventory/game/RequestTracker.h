#pragma once
#include "features/inventory/game/ResponseBarrier.h"
class ContainerManagerController;
namespace lamium::inventory::game {
void installRequestTracker();
void removeRequestTracker();
// Forget only Lamium's observation state. Vanilla owns outstanding requests.
void cancelTransfer();
bool beginTransfer(ContainerManagerController& controller);
void endTransfer();
ResponseBarrier::Result transferResult();
}

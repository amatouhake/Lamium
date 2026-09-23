#pragma once
#include "features/inventory/game/OwnedResponseBarrier.h"
class ContainerManagerController;
namespace lamium::inventory::game {
void installRequestTracker();
void removeRequestTracker();
// Forget only Lamium's observation state. Vanilla owns outstanding requests.
void cancelTransfer(TransferToken token);
std::optional<TransferToken> beginTransfer(ContainerManagerController& controller);
void endTransfer(TransferToken token);
ResponseBarrier::Result transferResult(TransferToken token);
}

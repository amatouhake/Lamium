#pragma once
#include "features/inventory/transfer/TransferGesture.h"
#include <string>
#include <vector>
class ContainerScreenController;
namespace lamium::inventory::game::transferRoutes {
void start();
void stop();
bool allowed();
bool supported(ContainerScreenController& controller);
std::vector<std::string> collections(ContainerScreenController& controller);
// Dispatch only: completion still belongs to RequestTracker and slot checks.
bool autoPlace(ContainerScreenController& controller, std::string const& collection,
               int index, int amount, transfer::Side destination);
// Vanilla's drop of one item or the whole stack (the drop key's request).
bool drop(ContainerScreenController& controller, std::string const& collection, int index, bool one);
// Takes `amount` onto the empty cursor and drops it there (all but one).
bool dropAmount(ContainerScreenController& controller, std::string const& collection, int index, int amount);
// Presses the plain left button where the pointer is, so vanilla decides what is there.
bool pressAsPlainClick(ContainerScreenController& controller);
}

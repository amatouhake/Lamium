#pragma once
class ContainerManagerController;
namespace lamium::inventory::game::restockTrace {
void start();
void stop();
// L-17: log the transfer context of a controller (HUD at use time, container
// screen when opened). Read-only; fixed labels and numeric values only.
void inspectUseController(ContainerManagerController& controller) noexcept;
void inspectScreenController(ContainerManagerController& controller) noexcept;
}

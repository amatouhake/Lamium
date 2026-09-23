#pragma once
#include "features/camera/DetachedCameraMotion.h"
struct RawMoveInputComponent;

namespace lamium::camera {
// Call only for the detached session's local owner, after vanilla HID extraction.
// Returns right/up/forward axes and removes movement from that extracted output.
// Stored physical input and non-movement look/selection flags remain untouched.
DetachedCameraMotion::Vector consumeMovement(RawMoveInputComponent&);
// Stash source for the FreeCamera adapter. Keyboard state may not have reached
// the extracted analog vector yet, so WASD-equivalent direction flags are read
// first with the analog vector as a controller fallback; axes are unit-clamped.
// Axis signs follow vanilla (Up/W is forward).
DetachedCameraMotion::Vector freecameraInputAxes(RawMoveInputComponent const& raw);
}

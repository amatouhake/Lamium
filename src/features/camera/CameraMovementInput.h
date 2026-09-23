#pragma once
#include "features/camera/DetachedCameraMotion.h"
struct RawMoveInputComponent;

namespace lamium::camera {
// Call only for the detached session's local owner, after vanilla HID extraction.
// Returns right/up/forward axes and removes movement from that extracted output.
// Stored physical input and non-movement look/selection flags remain untouched.
DetachedCameraMotion::Vector consumeMovement(RawMoveInputComponent&);
}

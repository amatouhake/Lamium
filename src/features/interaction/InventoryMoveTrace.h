#pragma once
// L-132 research: whether movement keys fed into the raw HID input while the
// inventory screen is open move the player. Only active with
// `xmake f --inventorymove_trace=y`.
namespace lamium::interaction::inventoryMoveTrace {
void start();
void stop();
}

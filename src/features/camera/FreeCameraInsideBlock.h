#pragma once
// L-244: the game decides the inside-a-block screen effect from where the
// body's head is, so a FreeCamera started with the head buried kept the
// effect while the camera itself flew out in the open. While FreeCamera owns
// the view the effect is not drawn; it is back as soon as the session ends
// and vanilla is untouched in every other frame.
namespace lamium::camera::insideBlock {
// The view keeps the game's own answer unless this frame draws FreeCamera's
// view; a false never becomes true here.
inline constexpr bool drawsInsideCubes(bool vanilla, bool suppress) noexcept { return vanilla && !suppress; }
// Only the primary client's own FreeCamera frame is suppressed. Split screen
// and every camera Lamium does not own keep what the game decided.
inline constexpr bool suppress(bool primaryView, bool freeCameraOwnsView) noexcept {
    return primaryView && freeCameraOwnsView;
}
void start() noexcept;
void stop() noexcept;
}

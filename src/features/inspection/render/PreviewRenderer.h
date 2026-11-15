#pragma once
#include "features/inspection/render/PreviewLayout.h"
#include <optional>

class MinecraftUIRenderContext;
class ScreenView;

namespace lamium::inspection::preview {
struct ContainerPreview;
}

namespace lamium::inspection::render {

/// Draws a ContainerPreview as an overlay on top of a container screen.
///
/// Must be called from the AfterUIRenderEvent of the ScreenView that owns the
/// hovered slot: at that point the engine has already flushed its own UI
/// batches, so everything drawn here is flushed explicitly.
/// The game's hover text this frame: its box and the pointer position it was placed from.
struct HoverTextBox {
    Rect  box;
    float cursorX, cursorY;
    bool  attach = false; // Sit right above it even when apart (the bundle tooltip, drawn below the pointer)
};

class PreviewRenderer {
public:
    // With `hoverText`, the frame follows the same pointer position as the hover text and keeps clear of its box.
    void render(ScreenView& view, MinecraftUIRenderContext& context, preview::ContainerPreview const& preview,
        std::optional<HoverTextBox> hoverText = std::nullopt);
};

} // namespace lamium::inspection::render


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
class PreviewRenderer {
public:
    // `hoverText`: the game's hover text box this frame, which the frame keeps clear of.
    void render(ScreenView& view, MinecraftUIRenderContext& context, preview::ContainerPreview const& preview,
        std::optional<Rect> hoverText = std::nullopt);
};

} // namespace lamium::inspection::render


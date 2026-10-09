#pragma once
// Schematic lines that keep their color under Vibrant Visuals: the block
// selection outline's material (selection_box), colored by the current
// shader color. On the debug material the lines took their color from the
// vertices, which Vibrant Visuals drew black. One color a draw; vertex
// colors are still written for the debug fallback.
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/renderer/ShaderColor.h"

namespace lamium::schematic::lines {
inline mce::MaterialPtr material() {
    mce::MaterialPtr material(mce::RenderMaterialGroup::common(), HashedString{"selection_box"});
    if (!material.mRenderMaterialInfoPtr) material = mce::MaterialPtr(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    return material;
}
// Runs `draw` with the current shader color set to (r, g, b), then restores it.
template <class Draw>
void colored(ScreenContext& screen, float r, float g, float b, Draw&& draw) {
    auto& shaderColor = static_cast<ShaderColor&>(screen.currentShaderColor);
    struct Restore {
        ShaderColor& color;
        mce::Color was;
        ~Restore() {
            color.color = was;
            color.dirty = true;
        }
    } restore{shaderColor, shaderColor.color};
    shaderColor.color = mce::Color{r, g, b, 1.f};
    shaderColor.dirty = true;
    draw();
}
} // namespace lamium::schematic::lines

#pragma once
#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

namespace lamium::inspection::lockedTrades {
// Pure parts of the locked-trade tooltip (L-129).
// The trade list shows the packet's recipes grouped by tier in packet order:
// the index-th entry under a tier header is the index-th recipe of that tier.
inline std::optional<std::size_t> recipeAt(std::vector<int> const& recipeTiers, int tier, int index) {
    if (index < 0) return std::nullopt;
    int seen = 0;
    for (std::size_t i = 0; i < recipeTiers.size(); ++i)
        if (recipeTiers[i] == tier && seen++ == index) return i;
    return std::nullopt;
}
// A tooltip beside the pointer, as the game places hover text: to the right
// and above, flipped left when it would leave the screen, kept on screen.
struct TipBox { float x, y; };
inline TipBox tipBox(float pointerX, float pointerY, float w, float h, float screenW, float screenH) {
    constexpr float gap = 8;
    float x = pointerX + gap, y = pointerY - h - gap;
    if (x + w > screenW) x = pointerX - gap - w;
    x = std::clamp(x, 0.f, std::max(0.f, screenW - w));
    y = std::clamp(y, 0.f, std::max(0.f, screenH - h));
    return {x, y};
}
}

#pragma once
#include <algorithm>
#include <cmath>
namespace lamium::ui {
struct HudLayout {
    float x{}, y{}, width{};
    int lines{};
    static HudLayout fit(float width, float height, float horizontal, float vertical, int count, float maxWidth = 230.f) {
        if (!std::isfinite(width) || !std::isfinite(height) || width < 12 || height < 18 || count <= 0
            || !std::isfinite(horizontal) || !std::isfinite(vertical) || !std::isfinite(maxWidth) || maxWidth <= 0) return {};
        float available = std::min(maxWidth,width-8);
        int lines = std::min(count,static_cast<int>((height-8)/14));
        return {4+(width-8-available)*std::clamp(horizontal,0.f,100.f)/100,
                4+(height-8-lines*14)*std::clamp(vertical,0.f,100.f)/100, available,lines};
    }
};
}

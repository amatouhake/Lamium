#pragma once
#include <algorithm>
#include <cmath>

namespace lamium::ui {
// GUI units, shared by drawing and hit testing. Keep the selected row visible
// instead of drawing settings or the Close control below a short window's edge.
struct SettingsLayout {
    float left{}, top{}, width{}, rowsTop{}, footer{}, bottom{};
    int first{}, visible{};
    bool subtitle{}, secondHint{};
    static constexpr float rowPitch = 16;
    static constexpr float rowHeight = 14;

    static SettingsLayout fit(float width, float height, int count, int selected, int first) {
        SettingsLayout layout;
        if (!std::isfinite(width) || !std::isfinite(height) || width < 120 || height < 100 || count <= 0)
            return layout;
        layout.width = std::min(460.0f, width - 24);
        layout.left = (width - layout.width) * .5f;
        float panelHeight = std::min(300.0f, height - 16);
        layout.top = (height - panelHeight) * .5f;
        layout.subtitle = panelHeight >= 200;
        layout.secondHint = panelHeight >= 200;
        layout.rowsTop = layout.top + (layout.subtitle ? 34 : 20);
        layout.footer = layout.top + panelHeight - (layout.secondHint ? 60 : 16);
        layout.visible = std::clamp(static_cast<int>((layout.footer - layout.rowsTop - 4) / rowPitch), 1, count);
        // Keep the search/header anchored while filtering, but fit the bottom
        // to the results instead of leaving a large empty panel behind them.
        layout.footer = layout.rowsTop + layout.visible * rowPitch + 4;
        layout.bottom = layout.footer + (layout.secondHint ? 58 : 15);
        layout.first = std::clamp(first, 0, count - layout.visible);
        selected = std::clamp(selected, 0, count - 1);
        if (selected < layout.first) layout.first = selected;
        if (selected >= layout.first + layout.visible) layout.first = selected - layout.visible + 1;
        return layout;
    }
    float rowY(int index) const { return rowsTop + (index - first) * rowPitch; }
    int hitPixels(float x, float y, float inverseScale) const {
        if (!std::isfinite(inverseScale) || inverseScale <= 0) return -1;
        return hit(x * inverseScale, y * inverseScale);
    }
    int hit(float x, float y) const {
        if (!std::isfinite(x) || !std::isfinite(y)) return -1;
        if (visible <= 0 || x < left || x >= left + width || y < rowsTop) return -1;
        int offset = static_cast<int>((y - rowsTop) / rowPitch);
        if (offset >= visible || y >= rowY(first + offset) + rowHeight) return -1;
        return first + offset;
    }
};
}

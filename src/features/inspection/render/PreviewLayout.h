#pragma once

#include <algorithm>

// Pure geometry for the preview overlay. Deliberately free of game types so it
// can be unit-tested without LeviLamina or the game.
namespace lamium::inspection::render {

struct Rect {
    float x0{0};
    float y0{0};
    float x1{0};
    float y1{0};

    [[nodiscard]] constexpr float width() const { return x1 - x0; }
    [[nodiscard]] constexpr float height() const { return y1 - y0; }

    constexpr bool operator==(Rect const&) const = default;
};

/// Positions a `columns` x `rows` grid of vanilla-sized slots inside a framed
/// box. All units are GUI units (the same space the UI render context uses).
struct PreviewLayout {
    static constexpr float kCellSize = 18.0f; // vanilla slot pitch
    static constexpr float kIconSize = 16.0f; // vanilla item icon size
    static constexpr float kPadding  = 4.0f;  // frame padding around the grid
    static constexpr float kGap      = 8.0f;  // distance kept from the anchor point

    int  columns{0};
    int  rows{0};
    Rect frame; // outer box, including padding

    [[nodiscard]] static constexpr float frameWidth(int columns) { return columns * kCellSize + 2 * kPadding; }
    [[nodiscard]] static constexpr float frameHeight(int rows) { return rows * kCellSize + 2 * kPadding; }

    /// Places the frame above-right of `anchor` (the pointer). When it would
    /// run off the right edge it goes to the left of the pointer instead, and
    /// when it would run off the top it goes below; clamping into the screen
    /// is only the last resort when neither side has room. The vanilla hover
    /// text is drawn below-right of the pointer, so preferring "above" keeps
    /// the two apart.
    [[nodiscard]] static constexpr PreviewLayout
    anchored(int columns, int rows, float anchorX, float anchorY, float screenWidth, float screenHeight) {
        float const width  = frameWidth(columns);
        float const height = frameHeight(rows);

        float x = anchorX + kGap;
        if (x + width > screenWidth) {
            x = anchorX - kGap - width;
        }
        if (x < 0.0f) {
            x = std::max(0.0f, std::min(anchorX + kGap, screenWidth - width));
        }

        float y = anchorY - kGap - height;
        if (y < 0.0f) {
            y = anchorY + kGap;
        }
        if (y + height > screenHeight) {
            y = std::max(0.0f, screenHeight - height);
        }

        return PreviewLayout{
            columns,
            rows,
            Rect{x, y, x + width, y + height}
        };
    }

    /// Full 18x18 cell of `slot` (row-major, matching container slot order).
    [[nodiscard]] constexpr Rect cell(int slot) const {
        int const   column = slot % columns;
        int const   row    = slot / columns;
        float const x      = frame.x0 + kPadding + column * kCellSize;
        float const y      = frame.y0 + kPadding + row * kCellSize;
        return Rect{x, y, x + kCellSize, y + kCellSize};
    }

    /// 16x16 icon area centred in the cell of `slot`.
    [[nodiscard]] constexpr Rect icon(int slot) const {
        Rect const  c     = cell(slot);
        float const inset = (kCellSize - kIconSize) / 2.0f;
        return Rect{c.x0 + inset, c.y0 + inset, c.x0 + inset + kIconSize, c.y0 + inset + kIconSize};
    }
};

} // namespace lamium::inspection::render


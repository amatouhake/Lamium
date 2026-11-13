#pragma once
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

namespace lamium::inspection::tooltip {
// Food values inside the vanilla item tooltip (BACKLOG L-92). The tooltip
// text ends with a line of blank U+E0FF (":nbsp:") glyphs that reserves the
// room; the game draws the whole tooltip text in one font call, and Lamium
// paints the hunger-bar icons there. The food glyph U+E100 faces the other
// way from the hunger icons, so it cannot be painted over. Pure: the
// placeholder line and where it sits in that text.
inline constexpr std::string_view glyph = "\xEE\x83\xBF"; // U+E0FF
// Line height in GUI units, measured on 2026-10-02.
inline constexpr float lineHeight = 10;

// The blank glyph is 4 units wide (measured 2026-10-02); icons sit 8 apart
// and are 9 wide, so n icons need 8n + 1 units of room.
inline constexpr int placeholderWidth = 4;
inline constexpr int placeholders(int icons) {
    return icons <= 0 ? 0 : (8 * icons + 1 + placeholderWidth - 1) / placeholderWidth;
}
inline std::string glyphLine(int count) {
    std::string line;
    if (count <= 0) return line;
    line = "\n";
    for (int i = 0; i < count; ++i) line += glyph;
    return line;
}
// The glyphs start their own line, so they sit at the left edge of it.
struct GlyphRun {
    int line = 0; // Line index from the top of the text
    int count = 0;
};
// The last line made of glyphs alone, below the first: only Lamium's own
// line counts, never a glyph a name or lore happens to contain. Lamium's
// other lines may follow it.
inline std::optional<GlyphRun> findGlyphRun(std::string_view text) {
    std::optional<GlyphRun> found;
    int line = 0;
    for (size_t start = 0; start <= text.size(); ++line) {
        auto const stop = std::min(text.find('\n', start), text.size());
        auto const row = text.substr(start, stop - start);
        if (line > 0 && !row.empty() && row.size() % glyph.size() == 0) {
            bool only = true;
            for (size_t i = 0; i < row.size() && only; i += glyph.size()) only = row.substr(i, glyph.size()) == glyph;
            if (only) found = GlyphRun{line, static_cast<int>(row.size() / glyph.size())};
        }
        start = stop + 1;
    }
    return found;
}
}

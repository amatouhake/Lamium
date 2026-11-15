#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace lamium::ui {
namespace detail {
// The end of the next line: the last fitting UTF-8 codepoint, moved back to a
// word boundary when the line ran out of width (CJK and long unbroken words
// keep the codepoint boundary). With `whole`, a glyph wider than the area
// still takes one codepoint; otherwise such a line ends at 0.
template<class Measure>
size_t lineEnd(std::string_view source, float width, Measure& measure, bool whole) {
    size_t end = 0, space = std::string_view::npos;
    while (end < source.size() && source[end] != '\n') {
        size_t next = end + 1;
        while (next < source.size() && (static_cast<unsigned char>(source[next]) & 0xc0) == 0x80) ++next;
        if (measure(source.substr(0, next)) > width) {
            if (!end && whole) end = next;
            break;
        }
        if (source[end] == ' ') space = end;
        end = next;
    }
    if (end < source.size() && source[end] != '\n' && source[end] != ' '
        && space != std::string_view::npos && space > 0) end = space;
    return end;
}
inline void skipBreak(std::string_view& source) {
    if (!source.empty() && source.front() == '\n') source.remove_prefix(1);
    while (!source.empty() && source.front() == ' ') source.remove_prefix(1);
}
}
// Complete measured wrapping for the scrolling description. A glyph wider than the
// area still consumes one whole codepoint, so no text is silently discarded.
template<class Measure>
std::vector<std::string> wrapFullText(std::string_view source, float width, Measure measure) {
    std::vector<std::string> lines;
    if (!std::isfinite(width) || width <= 0) return lines;
    while (!source.empty()) {
        size_t end = detail::lineEnd(source, width, measure, true);
        lines.emplace_back(source.substr(0, end));
        source.remove_prefix(end);
        detail::skipBreak(source);
    }
    return lines;
}
// Native font measurement is supplied by the caller. Remove whole UTF-8
// codepoints, so truncation never feeds a partial Japanese character to Font.
template<class Measure>
std::string fitLabel(std::string_view source, float width, Measure measure) {
    if (!std::isfinite(width) || width <= 0) return {};
    if (measure(source) <= width) return std::string(source);
    constexpr std::string_view ellipsis = "...";
    if (measure(ellipsis) > width) return {};
    std::string prefix(source);
    while (!prefix.empty()) {
        size_t end = prefix.size()-1;
        while (end && (static_cast<unsigned char>(prefix[end]) & 0xc0) == 0x80) --end;
        prefix.resize(end);
        auto candidate = prefix + std::string(ellipsis);
        if (measure(candidate) <= width) return candidate;
    }
    return std::string(ellipsis);
}

template<class Measure>
std::vector<std::string> wrapLabel(std::string_view source, float width, size_t maxLines, Measure measure) {
    std::vector<std::string> lines;
    if (!std::isfinite(width) || width <= 0 || !maxLines) return lines;
    while (!source.empty() && lines.size() < maxLines) {
        if (lines.size() + 1 == maxLines) {
            std::string remainder(source);
            for (auto& c : remainder) if (c == '\n') c = ' ';
            lines.push_back(fitLabel(remainder, width, measure));
            break;
        }
        size_t end = detail::lineEnd(source, width, measure, false);
        if (!end && source.front() != '\n') break; // A glyph cannot fit.
        lines.emplace_back(source.substr(0, end));
        source.remove_prefix(end);
        detail::skipBreak(source);
    }
    return lines;
}
// Label runs. In the game's font a few characters measure 0 wide and are
// drawn one character to the left of the pen, over what came before ("件"
// and "渡" on 1.26.60, found by measuring every character Lamium uses).
// Such a character is a run of its own so the label can give it a full
// width and draw it where it belongs; the rest splits into ASCII and other
// text, as Latin runs are raised in Japanese.
enum class RunKind { Ascii, Wide, Zero };
struct LabelRun { std::string_view text; RunKind kind; };
template<class IsZero>
std::vector<LabelRun> labelRuns(std::string_view text, IsZero isZero) {
    std::vector<LabelRun> runs;
    for (size_t at = 0; at < text.size();) {
        size_t next = at + 1;
        while (next < text.size() && (static_cast<unsigned char>(text[next]) & 0xc0) == 0x80) ++next;
        auto glyph = text.substr(at, next - at);
        RunKind kind = static_cast<unsigned char>(text[at]) < 0x80 ? RunKind::Ascii : isZero(glyph) ? RunKind::Zero : RunKind::Wide;
        if (!runs.empty() && kind != RunKind::Zero && runs.back().kind == kind)
            runs.back().text = std::string_view(runs.back().text.data(), runs.back().text.size() + glyph.size());
        else runs.push_back({glyph, kind});
        at = next;
    }
    return runs;
}
// How many characters of `text` are zero-wide ones.
template<class IsZero>
size_t zeroGlyphs(std::string_view text, IsZero isZero) {
    size_t count = 0;
    for (size_t at = 0; at < text.size();) {
        size_t next = at + 1;
        while (next < text.size() && (static_cast<unsigned char>(text[next]) & 0xc0) == 0x80) ++next;
        if (static_cast<unsigned char>(text[at]) >= 0x80 && isZero(text.substr(at, next - at))) ++count;
        at = next;
    }
    return count;
}
}

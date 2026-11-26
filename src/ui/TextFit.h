#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace lamium::ui {
namespace detail {
// A Japanese or Chinese character (kana, ideographs, their punctuation, full-width forms). Other scripts beyond
// ASCII (Cyrillic, accented Latin) are words between spaces, as English.
inline bool wide(std::string_view source, size_t at) {
    auto byte = [&](size_t i) { return i < source.size() ? static_cast<unsigned char>(source[i]) : 0u; };
    unsigned const lead = byte(at);
    unsigned c = 0;
    if (lead >= 0xf0) c = (lead & 0x07) << 18 | (byte(at + 1) & 0x3f) << 12 | (byte(at + 2) & 0x3f) << 6 | (byte(at + 3) & 0x3f);
    else if (lead >= 0xe0) c = (lead & 0x0f) << 12 | (byte(at + 1) & 0x3f) << 6 | (byte(at + 2) & 0x3f);
    return (c >= 0x2e80 && c <= 0x9fff) || (c >= 0xf900 && c <= 0xfaff) || (c >= 0xff00 && c <= 0xffef) || (c >= 0x20000 && c <= 0x3ffff);
}
inline size_t previous(std::string_view source, size_t at) {
    while (at && (static_cast<unsigned char>(source[--at]) & 0xc0) == 0x80) {}
    return at;
}
// Punctuation that does not begin a line (Japanese and Chinese rules, and the
// ASCII ones that follow a word).
inline bool noLineStart(std::string_view source, size_t at) {
    constexpr std::string_view closing[] = {"。", "、", "，", "．", "・", "）", "」", "』", "】", "〉", "》", "！", "？", "：", "；",
                                            ".", ",", ":", ";", "!", "?", ")"};
    auto rest = source.substr(at);
    return std::ranges::any_of(closing, [&](std::string_view mark) { return rest.starts_with(mark); });
}
// Whether a line may break before `at`: at a space, or between characters
// where either side is Japanese or Chinese, which break without spaces.
inline bool breakBefore(std::string_view source, size_t at) {
    if (!at || at >= source.size() || noLineStart(source, at)) return false;
    return source[at] == ' ' || (source[at] != ' ' && source[previous(source, at)] != ' '
        && (wide(source, at) || wide(source, previous(source, at))));
}
// The end of the next line: the last fitting UTF-8 codepoint, moved back to
// the last place a line may break when it ran out of width (a long unbroken
// word keeps the codepoint boundary). With `whole`, a glyph wider than the
// area still takes one codepoint; otherwise such a line ends at 0.
template<class Measure>
size_t lineEnd(std::string_view source, float width, Measure& measure, bool whole) {
    size_t end = 0, chance = 0;
    while (end < source.size() && source[end] != '\n') {
        size_t next = end + 1;
        while (next < source.size() && (static_cast<unsigned char>(source[next]) & 0xc0) == 0x80) ++next;
        if (measure(source.substr(0, next)) > width) {
            if (!end && whole) end = next;
            break;
        }
        if (breakBefore(source, end)) chance = end;
        end = next;
    }
    if (end < source.size() && source[end] != '\n' && !breakBefore(source, end) && chance > 0) end = chance;
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

// What a press-twice button reads (DESIGN "Controls"): its label, once armed
// `confirm` (what a second press does), or `brief` where `confirm` would be
// cut in `width`.
template<class Measure>
std::string const& armedText(std::string const& label, std::string const& confirm, std::string const& brief, bool armed,
                             float width, Measure measure) {
    if (!armed) return label;
    return measure(confirm) <= width ? confirm : brief;
}
// A label with a shorter form ("Ignore on every Lever", "Ignore on all"):
// `full` where it fits in `width`, else `brief`.
template<class Measure>
std::string const& fittingText(std::string const& full, std::string const& brief, float width, Measure measure) {
    return measure(full) <= width ? full : brief;
}
// A press-twice button sized by its text holds the longer of its two labels,
// so arming neither moves nor cuts it.
template<class Measure>
float armedTextWidth(std::string_view label, std::string_view confirm, Measure measure) {
    return std::max(measure(label), measure(confirm));
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

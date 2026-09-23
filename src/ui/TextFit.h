#pragma once
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace lamium::ui {
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
        size_t end = 0, space = std::string_view::npos;
        while (end < source.size() && source[end] != '\n') {
            size_t next = end + 1;
            while (next < source.size() && (static_cast<unsigned char>(source[next]) & 0xc0) == 0x80) ++next;
            if (measure(source.substr(0, next)) > width) break;
            if (source[end] == ' ') space = end;
            end = next;
        }
        // Prefer a word boundary when the line ran out of width. CJK and long
        // unbroken words fall back to a complete UTF-8 codepoint boundary.
        if (end < source.size() && source[end] != '\n' && source[end] != ' '
            && space != std::string_view::npos && space > 0) end = space;
        if (!end && source.front() != '\n') break; // A glyph cannot fit.
        lines.emplace_back(source.substr(0, end));
        source.remove_prefix(end);
        if (!source.empty() && source.front() == '\n') source.remove_prefix(1);
        while (!source.empty() && source.front() == ' ') source.remove_prefix(1);
    }
    return lines;
}
}

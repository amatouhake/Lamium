#pragma once
#include <cmath>
#include <string>
#include <string_view>

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
}

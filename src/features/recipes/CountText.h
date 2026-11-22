#pragma once
#include <cstdint>
#include <format>
#include <string>

// A count in a slot or a list's cell (L-370): as it is up to 999, then thousands ("2k", "749k"), millions ("1.2M",
// "12M"), always rounded down; "++" from a billion. Three characters fit at full size; a fourth is drawn smaller
// (slotCount, 2026-11-22). Text columns that have room show the whole number. Pure.
namespace lamium::recipes {
inline std::string slotCountText(std::uint64_t n) {
    if (n < 1000) return std::to_string(n);
    if (n < 1000000) return std::format("{}k", n / 1000);
    if (n < 10000000) return std::format("{}.{}M", n / 1000000, n / 100000 % 10);
    if (n < 1000000000) return std::format("{}M", n / 1000000);
    return "++";
}
// The whole count with its thousands apart ("2,400,008"), for a tip where a slot or cell shows it cut.
inline std::string wholeCountText(std::uint64_t n) {
    std::string digits = std::to_string(n), out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) out += ',';
        out += digits[i];
    }
    return out;
}
}

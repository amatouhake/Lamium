#pragma once
#include <cstdint>
#include <format>
#include <string>

// A count in a slot or a list's cell, where three characters fit (L-370): as it is up to 999, then thousands
// ("2k", "23k"), and "++" past them. Text columns that have room show the whole number. Pure.
namespace lamium::recipes {
inline std::string slotCountText(std::uint64_t n) {
    return n < 1000 ? std::to_string(n) : n < 100000 ? std::format("{}k", n / 1000) : std::string("++");
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

#pragma once
#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace lamium::inspection::details {
// What an item's tooltip can say beyond the game's own text (BACKLOG L-319).
// Pure: the values come from the game, the lines and their limits from here.
struct Entry { std::string key, value; };
struct Item {
    std::string id;
    int aux = 0;
    std::vector<std::string> tags;
    std::vector<Entry> data; // Top-level user data, values already short
    int armor = 0, toughness = 0;
    float knockbackResistance = 0; // 0..1
    int enchantability = 0, maxStack = 64, cooldownTicks = 0;
    float burnTicks = 0;
};
// A translated line with one value, or a line taken as it is (no key).
struct Line {
    std::string key, value;
    bool dim = false; // Identity and data, darker than the values
    bool operator==(Line const&) const = default;
};
inline constexpr size_t valueLimit = 40, lineWidth = 44, tagLines = 4, dataLines = 8;
inline constexpr float smeltTicks = 200, ticksPerSecond = 20;

// Up to two decimals, without trailing zeros.
inline std::string number(float value) {
    auto text = std::format("{:.2f}", value);
    text.erase(text.find_last_not_of('0') + 1);
    if (text.ends_with('.')) text.pop_back();
    return text;
}
// One line of at most `limit` characters; the section sign would recolour the tooltip.
inline std::string clip(std::string_view text, size_t limit = valueLimit) {
    std::string out;
    size_t characters = 0;
    for (size_t i = 0; i < text.size();) {
        auto const lead = static_cast<unsigned char>(text[i]);
        size_t const length = lead < 0x80 ? 1 : lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
        auto const piece = text.substr(i, length);
        i += length;
        if (characters == limit) return out + "\xE2\x80\xA6";
        ++characters;
        if (piece == "\xC2\xA7") out += '&';
        else if (lead < 0x20) out += ' ';
        else out += piece;
    }
    return out;
}
// Names joined by ", " into lines of about `width` bytes; past `lines`, an ellipsis.
inline std::vector<std::string> wrapped(std::vector<std::string> const& names, size_t width = lineWidth,
                                        size_t lines = tagLines) {
    std::vector<std::string> out;
    for (size_t i = 0; i < names.size(); ++i) {
        auto const piece = names[i] + (i + 1 < names.size() ? ", " : "");
        if (out.empty() || (!out.back().empty() && out.back().size() + piece.size() > width)) {
            if (out.size() == lines) {
                out.back() += "\xE2\x80\xA6";
                return out;
            }
            out.emplace_back();
        }
        out.back() += piece;
    }
    return out;
}
inline std::vector<Line> lines(Item const& item) {
    std::vector<Line> out;
    auto value = [&](char const* key, std::string text) { out.push_back({key, std::move(text)}); };
    if (item.armor > 0) value("itemDetails.armor", std::to_string(item.armor));
    if (item.toughness > 0) value("itemDetails.toughness", std::to_string(item.toughness));
    if (item.knockbackResistance > 0)
        value("itemDetails.knockbackResistance", number(std::round(item.knockbackResistance * 1000) / 10));
    if (item.enchantability > 0) value("itemDetails.enchantability", std::to_string(item.enchantability));
    // 64 and 1 say nothing; the stacks of 16 are the ones worth telling.
    if (item.maxStack > 1 && item.maxStack < 64) value("itemDetails.maxStack", std::to_string(item.maxStack));
    if (item.cooldownTicks > 0) value("itemDetails.cooldown", number(item.cooldownTicks / ticksPerSecond));
    if (item.burnTicks > 0) value("itemDetails.fuel", number(item.burnTicks / smeltTicks));
    if (!item.id.empty()) out.push_back({"", item.aux ? std::format("{} #{}", item.id, item.aux) : item.id, true});
    bool first = true;
    for (auto& row : wrapped(item.tags)) {
        out.push_back({first ? "itemDetails.tags" : "", first ? std::move(row) : "  " + row, true});
        first = false;
    }
    for (size_t i = 0; i < item.data.size(); ++i) {
        if (i == dataLines) {
            out.push_back({"itemDetails.more", std::to_string(item.data.size() - i), true});
            break;
        }
        out.push_back({"", clip(item.data[i].key) + ": " + clip(item.data[i].value), true});
    }
    return out;
}
}

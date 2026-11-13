#pragma once
#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
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
    std::vector<Entry> data; // The user data's top level; a compound or list is its whole text on one line
    bool full = false;       // The long form: the data itself rather than its count
    int enchantability = 0, maxStack = 64, cooldownTicks = 0;
    float burnTicks = 0, useTicks = 0;
};
// A translated line with one value, or a line taken as it is (no key).
struct Line {
    std::string key, value;
    bool dim = false; // Identity and data, darker than the values
    bool operator==(Line const&) const = default;
};
inline constexpr size_t valueLimit = 40, lineWidth = 44, tagLines = 4, dataLines = 24, dataWidth = 60;
inline constexpr float smeltTicks = 200, ticksPerSecond = 20;
// A bow or a shield is held for as long as the button is; that is not a time to tell.
inline constexpr float longestUse = 1200;

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
// A tooltip's box for another text, from the box the game made for the text it has: the room around the text
// stays, the lines and the widest line change. Widths are of the widest line of each text.
struct Box { float width = 0, height = 0; };
inline int lineCount(std::string_view text) { return 1 + static_cast<int>(std::count(text.begin(), text.end(), '\n')); }
inline Box resized(Box box, float widestNow, int linesNow, float widestThen, int linesThen, float lineHeight) {
    return {std::max(0.f, box.width + widestThen - widestNow), std::max(0.f, box.height + (linesThen - linesNow) * lineHeight)};
}
// Text without its colour and style codes: what the player reads.
inline std::string plain(std::string_view text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text.substr(i, 2) == "\xC2\xA7") {
            i += 2; // The sign's two bytes, then the loop steps over the code
            continue;
        }
        out += text[i];
    }
    return out;
}
// An effect's lines take the game's colours: red for a harmful one, blue otherwise, white when it cannot be told.
inline constexpr char effectTint(std::optional<bool> harmful) { return !harmful ? 'f' : *harmful ? 'c' : '9'; }
// What an effect does to an attribute, as the game words it: "-0.5 Attack Damage", "+20% Speed". Only the first
// operation adds a plain amount; the others are shares.
inline std::string modifierLine(float value, int operation, std::string_view name) {
    auto const amount = number(std::abs(operation == 0 ? value : value * 100));
    return std::format("{}{}{} {}", value < 0 ? '-' : '+', amount, operation == 0 ? "" : "%", name);
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
    if (item.enchantability > 0) value("itemDetails.enchantability", std::to_string(item.enchantability));
    // 64 and 1 say nothing; the stacks of 16 are the ones worth telling.
    if (item.maxStack > 1 && item.maxStack < 64) value("itemDetails.maxStack", std::to_string(item.maxStack));
    if (item.cooldownTicks > 0) value("itemDetails.cooldown", number(item.cooldownTicks / ticksPerSecond));
    if (item.useTicks > 0 && item.useTicks <= longestUse) value("itemDetails.useTime", number(item.useTicks / ticksPerSecond));
    if (item.burnTicks > 0) value("itemDetails.fuel", number(item.burnTicks / smeltTicks));
    if (!item.id.empty()) out.push_back({"", item.aux ? std::format("{} #{}", item.id, item.aux) : item.id, true});
    // One row sits beside the label; more go under it, all indented alike, since no count of spaces lines a
    // row up with text after a label.
    if (auto rows = wrapped(item.tags); rows.size() == 1) out.push_back({"itemDetails.tags", std::move(rows[0]), true});
    else if (!rows.empty()) {
        out.push_back({"itemDetails.tags", "", true});
        for (auto& row : rows) out.push_back({"", "  " + row, true});
    }
    // Short by default: the data is counted. The long form tells each top-level entry on a line of its own, cut at
    // the width, so a shulker box's items are one line and not hundreds.
    if (!item.full) {
        if (!item.data.empty()) out.push_back({"itemDetails.data", std::to_string(item.data.size()), true});
        return out;
    }
    size_t const shown = std::min(item.data.size(), dataLines);
    for (size_t i = 0; i < shown; ++i)
        out.push_back({"", clip(item.data[i].key) + ": " + clip(item.data[i].value, dataWidth), true});
    if (item.data.size() > shown) out.push_back({"itemDetails.more", std::to_string(item.data.size() - shown), true});
    return out;
}
}

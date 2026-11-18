#pragma once
#include "features/recipes/RecipeIndex.h"
#include <map>
#include <string>
#include <string_view>
#include <vector>

// L-359: narrowing the item list: by what is typed, and to what can be made
// from what is held. Pure.
namespace lamium::recipes {
// ASCII letters only: the rest of UTF-8 has no case here and compares as it is.
inline std::string lowered(std::string_view text) {
    std::string out(text);
    for (char& c : out) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return out;
}
// Every word of the query is somewhere in the item's name or its key. `query`
// and both texts are lowered already. An empty query matches everything.
inline bool searchMatches(std::string_view query, std::string_view name, std::string_view key) {
    size_t at = 0;
    while (at < query.size()) {
        while (at < query.size() && query[at] == ' ') ++at;
        size_t end = query.find(' ', at);
        if (end == std::string_view::npos) end = query.size();
        if (end > at) {
            auto const word = query.substr(at, end - at);
            if (name.find(word) == std::string_view::npos && key.find(word) == std::string_view::npos) return false;
        }
        at = end;
    }
    return true;
}
// Per item: whether a crafting recipe for it can be made once from `held`
// as it is, without making anything first. An ingredient any of several
// items satisfies takes them together.
inline std::vector<bool> canMakeNow(Index const& index, std::map<Item, Count> const& held) {
    std::vector<bool> out(index.items.size(), false);
    auto have = [&](Item item) {
        auto found = held.find(item);
        return found == held.end() ? Count{0} : found->second;
    };
    for (auto const& recipe : index.recipes) {
        if (recipe.station != Station::Crafting || recipe.own || out[static_cast<size_t>(recipe.makes)]) continue;
        bool enough = !recipe.uses.empty();
        for (auto const& part : recipe.uses) {
            Count got = 0;
            if (part.group) for (Item member : index.groups[static_cast<size_t>(part.id)].members) got += have(member);
            else got = have(part.id);
            if (got < part.count) { enough = false; break; }
        }
        if (enough) out[static_cast<size_t>(recipe.makes)] = true;
    }
    return out;
}
// The lines of an item's hover text as the game gives it, under its name: what the game says of the
// item and what Lamium's item details add to it (L-363). The first line, the name, is left out; so are
// the game's color codes, empty lines and lines of icon glyphs (private-use characters), which a
// tooltip of plain text cannot show. At most `most` lines, each cut to 120 bytes at a character's end.
inline std::vector<std::string> hoverLines(std::string_view text, size_t most = 12) {
    std::vector<std::string> out;
    bool first = true;
    while (!text.empty() && out.size() < most) {
        auto const end = text.find('\n');
        std::string_view line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
        if (first) { first = false; continue; }
        std::string plain;
        bool glyphs = false;
        for (size_t i = 0; i < line.size(); ++i) {
            auto const c = static_cast<unsigned char>(line[i]);
            // The section sign and the character after it are a color or style code.
            if (c == 0xC2 && i + 1 < line.size() && static_cast<unsigned char>(line[i + 1]) == 0xA7) {
                i += 2;
                while (i + 1 < line.size() && (static_cast<unsigned char>(line[i + 1]) & 0xC0) == 0x80) ++i;
                continue;
            }
            if (c == 0xEE || (c == 0xEF && i + 1 < line.size() && static_cast<unsigned char>(line[i + 1]) < 0xA4)) glyphs = true;
            if (c != '\r') plain += line[i];
        }
        if (glyphs || plain.find_first_not_of(' ') == std::string::npos) continue;
        if (plain.size() > 120) {
            size_t cut = 120;
            while (cut > 0 && (static_cast<unsigned char>(plain[cut]) & 0xC0) == 0x80) --cut;
            plain.resize(cut);
        }
        out.push_back(std::move(plain));
    }
    return out;
}
}

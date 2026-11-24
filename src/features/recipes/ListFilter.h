#pragma once
#include "features/recipes/RecipeIndex.h"
#include <algorithm>
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
// The open screen as a place to lay a recipe in (L-380): the crafting grid (`side` 2 or 3), or a station whose
// slots take a recipe's cells one each: the furnaces' and the stonecutter's input, the smithing table's template,
// base and addition. `Other`: none.
struct Bench {
    Station station = Station::Other;
    int side = 0;
    bool operator==(Bench const&) const = default;
};
inline int benchSlots(Station station) {
    switch (station) {
    case Station::Furnace: case Station::BlastFurnace: case Station::Smoker: case Station::Stonecutter: return 1;
    case Station::Smithing: return 3;
    case Station::BrewingStand: return 2; // the ingredient and the bottle before (L-361); the stand's own slots differ
    default: return 0;
    }
}
inline bool benchAny(Bench const& bench) { return bench.station == Station::Crafting ? bench.side > 0 : benchSlots(bench.station) > 0; }
// Whether the bench can take the recipe: a crafting recipe that fits the grid, or one its station does with as many
// cells as it has slots.
inline bool benchMakes(Bench const& bench, Recipe const& recipe) {
    if (bench.station == Station::Crafting) return bench.side > 0 && recipe.station == Station::Crafting && fitsGrid(recipe, bench.side);
    int const slots = benchSlots(bench.station);
    return slots > 0 && static_cast<int>(recipe.cells.size()) == slots
        && std::find(recipe.stations.begin(), recipe.stations.end(), bench.station) != recipe.stations.end();
}
// The view's tab whose recipes the bench takes: crafting, or the station's process.
inline Kind benchKind(Bench const& bench) { return bench.station == Station::Crafting ? Kind::Crafting : kindOf(bench.station); }
// Per item: whether a recipe for it can be made once from `held` as it is, without making anything first: at a
// bench (`bench`), one it takes; elsewhere a crafting recipe of any size. An ingredient any of several items
// satisfies takes them together.
inline std::vector<bool> canMakeNow(Index const& index, std::map<Item, Count> const& held, Bench const& bench = {}) {
    std::vector<bool> out(index.items.size(), false);
    for (auto const& recipe : index.recipes) {
        if (recipe.own || out[static_cast<size_t>(recipe.makes)]) continue;
        if (benchAny(bench) ? !benchMakes(bench, recipe) : recipe.station != Station::Crafting) continue;
        bool enough = !recipe.uses.empty();
        for (auto const& part : recipe.uses)
            if (heldOf(index, held, part.group ? index.groupItem(part.id) : part.id) < part.count) { enough = false; break; }
        if (enough) out[static_cast<size_t>(recipe.makes)] = true;
    }
    return out;
}
// The lines of an item's hover text as the game gives it, under its name: what the game says of the
// item and what Lamium's item details add to it (L-363). The first line, the name, is left out; so are
// empty lines and lines of icon glyphs (private-use characters), which a tooltip of plain text cannot
// show. A line keeps the color code its text starts in (0 for none), not the codes themselves; a color runs
// on into the next lines until another or a reset, as the game draws it (a firework star's color line). At
// most `most` lines, each cut to 120 bytes at a character's end. `raw` is the line with its codes for the game's
// font to draw (ui::gameText), led by the color running on from above.
struct HoverLine {
    std::string text;
    char color = 0; // the game's color code letter: '0'-'9', 'a'-'u'
    std::string raw;
    bool operator==(HoverLine const& other) const { return text == other.text && color == other.color; }
};
// A line with codes cut after `most` bytes of what it shows, at a character's end: the codes are kept and not
// counted, so it ends where the same line without them is cut.
inline std::string cutVisible(std::string_view line, size_t most) {
    std::string out;
    size_t shown = 0;
    for (size_t at = 0; at < line.size();) {
        if (line.substr(at, 2) == "\xC2\xA7") {
            size_t const end = std::min(line.size(), at + 3);
            if (end == at + 3) out += line.substr(at, 3);
            at = end;
            continue;
        }
        size_t next = at + 1;
        while (next < line.size() && (static_cast<unsigned char>(line[next]) & 0xC0) == 0x80) ++next;
        if (shown + (next - at) > most) break;
        out += line.substr(at, next - at);
        shown += next - at;
        at = next;
    }
    return out;
}
// The hover text's first line, the item's name, with its codes and without.
struct HoverName {
    std::string raw, plain;
};
inline HoverName hoverName(std::string_view text) {
    std::string_view line = text.substr(0, text.find('\n'));
    HoverName out;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\r') continue;
        out.raw += line[i];
        if (static_cast<unsigned char>(line[i]) == 0xC2 && i + 2 < line.size() && static_cast<unsigned char>(line[i + 1]) == 0xA7) {
            out.raw += line.substr(i + 1, 2);
            i += 2;
            continue;
        }
        out.plain += line[i];
    }
    out.raw = cutVisible(out.raw, 240);
    return out;
}
inline std::vector<HoverLine> hoverLines(std::string_view text, size_t most = 12) {
    std::vector<HoverLine> out;
    bool first = true;
    char running = 0; // the color in effect where the last line ended
    while (!text.empty() && out.size() < most) {
        auto const end = text.find('\n');
        std::string_view line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
        std::string plain;
        char const above = running;
        char color = running;
        bool glyphs = false;
        for (size_t i = 0; i < line.size(); ++i) {
            auto const c = static_cast<unsigned char>(line[i]);
            // The section sign and the character after it are a color or style code.
            if (c == 0xC2 && i + 1 < line.size() && static_cast<unsigned char>(line[i + 1]) == 0xA7) {
                i += 2;
                if (i < line.size()) {
                    char const code = line[i];
                    bool const leading = plain.find_first_not_of(' ') == std::string::npos;
                    bool const tint = (code >= '0' && code <= '9') || (code >= 'a' && code <= 'u' && code != 'k' && code != 'l' && code != 'o' && code != 'r');
                    if (tint || code == 'r') {
                        running = tint ? code : 0;
                        if (leading) color = running;
                    }
                }
                while (i + 1 < line.size() && (static_cast<unsigned char>(line[i + 1]) & 0xC0) == 0x80) ++i;
                continue;
            }
            if (c == 0xEE || (c == 0xEF && i + 1 < line.size() && static_cast<unsigned char>(line[i + 1]) < 0xA4)) glyphs = true;
            if (c != '\r') plain += line[i];
        }
        // The name's color (a rarity's) is not seen running on, so it does not.
        if (first) { first = false, running = 0; continue; }
        if (glyphs || plain.find_first_not_of(' ') == std::string::npos) continue;
        if (plain.size() > 120) {
            size_t cut = 120;
            while (cut > 0 && (static_cast<unsigned char>(plain[cut]) & 0xC0) == 0x80) --cut;
            plain.resize(cut);
        }
        std::string raw = above ? std::string("\xC2\xA7") + above : std::string();
        for (char c : line) if (c != '\r') raw += c;
        out.push_back({std::move(plain), color, cutVisible(raw, 120)});
    }
    return out;
}
}

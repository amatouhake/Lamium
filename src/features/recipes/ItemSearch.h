#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// L-377: the item list's search. Words must all match; `-word` leaves out; `a|b` either (AND binds tighter,
// read left to right, no parentheses); `"…"` a phrase, also for a sign character itself; `#` a tag, `$` the
// tooltip text, `@` the id's namespace, combinable with `-`. Plain words match the name, the id and the
// English name. Full-width signs and spaces read as ASCII; hiragana and katakana match each other; ASCII case
// is ignored. Pure.
namespace lamium::recipes::search {
// Text folded for matching: ASCII lower case, full-width ASCII (U+FF01-FF5E) and the ideographic space as
// ASCII, katakana (U+30A1-30F6) as hiragana. Other UTF-8 is kept as it is.
inline std::string folded(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    auto put = [&](std::uint32_t c) {
        if (c < 0x80) out += static_cast<char>(c);
        else if (c < 0x800) out += static_cast<char>(0xC0 | (c >> 6)), out += static_cast<char>(0x80 | (c & 0x3F));
        else out += static_cast<char>(0xE0 | (c >> 12)), out += static_cast<char>(0x80 | ((c >> 6) & 0x3F)), out += static_cast<char>(0x80 | (c & 0x3F));
    };
    for (size_t i = 0; i < text.size();) {
        auto const b = static_cast<unsigned char>(text[i]);
        if (b < 0x80) {
            out += static_cast<char>(b >= 'A' && b <= 'Z' ? b - 'A' + 'a' : b);
            ++i;
            continue;
        }
        // Three-byte sequences are the only ones folded; anything else is copied.
        if ((b & 0xF0) == 0xE0 && i + 2 < text.size()) {
            std::uint32_t c = ((b & 0x0F) << 12) | ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6) | (static_cast<unsigned char>(text[i + 2]) & 0x3F);
            if (c >= 0xFF01 && c <= 0xFF5E) {
                char a = static_cast<char>(c - 0xFF01 + 0x21);
                out += a >= 'A' && a <= 'Z' ? static_cast<char>(a - 'A' + 'a') : a;
            } else if (c == 0x3000) out += ' ';
            else if (c >= 0x30A1 && c <= 0x30F6) put(c - 0x60);
            else out.append(text.substr(i, 3));
            i += 3;
            continue;
        }
        size_t n = (b & 0xE0) == 0xC0 ? 2 : (b & 0xF8) == 0xF0 ? 4 : 1;
        out.append(text.substr(i, std::min(n, text.size() - i)));
        i += n;
    }
    return out;
}

enum class Field { Plain, Tag, Tooltip, Namespace };
struct Term {
    Field field = Field::Plain;
    std::string text; // folded
    bool exclude = false;
    bool operator==(Term const&) const = default;
};
// Alternatives (`|`), each a list of terms that must all hold.
using Query = std::vector<std::vector<Term>>;

// Reads a query typed as it is (folded here). A sign at a word's start is the word's kind; inside quotes
// everything is text. An empty alternative is dropped; a query with none matches everything.
inline Query parse(std::string_view typed) {
    std::string const text = folded(typed);
    Query out;
    std::vector<Term> current;
    size_t i = 0;
    auto finish = [&] {
        if (!current.empty()) out.push_back(std::move(current));
        current.clear();
    };
    while (i < text.size()) {
        char c = text[i];
        if (c == ' ') { ++i; continue; }
        if (c == '|') { finish(); ++i; continue; }
        Term term;
        if (c == '-') { term.exclude = true; ++i; }
        if (i < text.size()) {
            char const k = text[i];
            if (k == '#') term.field = Field::Tag, ++i;
            else if (k == '$') term.field = Field::Tooltip, ++i;
            else if (k == '@') term.field = Field::Namespace, ++i;
        }
        if (i < text.size() && text[i] == '"') {
            size_t const end = text.find('"', i + 1);
            term.text = text.substr(i + 1, (end == std::string::npos ? text.size() : end) - i - 1);
            i = end == std::string::npos ? text.size() : end + 1;
        } else {
            size_t end = i;
            while (end < text.size() && text[end] != ' ' && text[end] != '|') ++end;
            term.text = text.substr(i, end - i);
            i = end;
        }
        if (!term.text.empty()) current.push_back(std::move(term));
    }
    finish();
    return out;
}
inline bool uses(Query const& query, Field field) {
    for (auto const& all : query)
        for (auto const& term : all) if (term.field == field) return true;
    return false;
}

// What one item is searched by, each folded: the name as shown, the id ("minecraft:oak_planks"), the English
// name, its tags joined by new lines, its tooltip text.
struct Fields {
    std::string_view name, id, english, tags, tooltip;
};
inline bool holds(Term const& term, Fields const& item) {
    auto in = [&](std::string_view where) { return !where.empty() && where.find(term.text) != std::string_view::npos; };
    switch (term.field) {
    case Field::Tag: return in(item.tags);
    case Field::Tooltip: return in(item.tooltip);
    case Field::Namespace: {
        auto const colon = item.id.find(':');
        std::string_view const space = colon == std::string_view::npos ? std::string_view("minecraft") : item.id.substr(0, colon);
        return space.find(term.text) != std::string_view::npos;
    }
    case Field::Plain: {
        if (in(item.name) || in(item.english) || in(item.id)) return true;
        // An id's words are also read with spaces ("oak planks" finds minecraft:oak_planks).
        if (term.text.find(' ') == std::string::npos || item.id.find('_') == std::string_view::npos) return false;
        std::string spaced(item.id);
        for (char& ch : spaced) if (ch == '_') ch = ' ';
        return spaced.find(term.text) != std::string::npos;
    }
    }
    return false;
}
inline bool matches(Query const& query, Fields const& item) {
    if (query.empty()) return true;
    for (auto const& all : query) {
        bool ok = true;
        for (auto const& term : all)
            if (holds(term, item) == term.exclude) { ok = false; break; }
        if (ok) return true;
    }
    return false;
}

// The searches typed before, newest last, for Up and Down in the field. The list follows what is typed
// as it goes (`typed`), so a search is kept once it has stood a moment, not only when the field is left.
struct History {
    std::vector<std::string> entries;
    int at = -1; // stepping: an index into entries; -1 when not stepping
    static constexpr size_t most = 20;
    static constexpr double settleMs = 1000;
    std::string seen;       // the field's text as last seen
    double seenSince = 0;   // when it became that
    bool seenKept = true;   // it was kept already, or came from stepping
    // The field's text at `nowMs`: kept once it has stood `settleMs`. Text Up or Down put there is not kept again.
    void typed(std::string const& text, double nowMs) {
        if (text != seen) seen = text, seenSince = nowMs, seenKept = false;
        else if (!seenKept && nowMs - seenSince >= settleMs) keep(text);
    }
    void keep(std::string const& text) {
        at = -1;
        seen = text, seenKept = true;
        if (text.find_first_not_of(' ') == std::string::npos) return;
        std::erase(entries, text);
        entries.push_back(text);
        if (entries.size() > most) entries.erase(entries.begin());
    }
    // `older`: Up. The entry to show, or nothing when there is none that way (Down past the newest: empty).
    std::string step(bool older) {
        if (entries.empty()) return {};
        int const n = static_cast<int>(entries.size());
        if (older) at = at < 0 ? n - 1 : std::max(0, at - 1);
        else if (at >= 0) at = at + 1 >= n ? -1 : at + 1;
        std::string out = at < 0 ? std::string() : entries[static_cast<size_t>(at)];
        seen = out, seenKept = true;
        return out;
    }
};
}

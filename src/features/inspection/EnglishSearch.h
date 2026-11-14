#pragma once
#include <string>
#include <string_view>

namespace lamium::inspection::englishSearch {
// Recipe and creative search also match English names (L-130). Pure part: a
// query matches when each of its words appears in the English name or the
// identifier (namespace dropped, '_' read as a space), ignoring ASCII case.
// `extra` is more searchable text, one name per line: an enchanted book's
// stored enchantments (L-344).
inline std::string folded(std::string_view text) {
    std::string out(text);
    for (auto& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        else if (c == '_') c = ' ';
    }
    return out;
}
inline bool matches(std::string_view query, std::string_view englishName, std::string_view identifier,
                    std::string_view extra = {}) {
    if (auto colon = identifier.find(':'); colon != std::string_view::npos) identifier.remove_prefix(colon + 1);
    auto haystack = folded(englishName) + "\n" + folded(identifier) + "\n" + folded(extra);
    auto words = folded(query);
    bool any = false;
    size_t at = 0;
    while (at < words.size()) {
        size_t end = words.find(' ', at);
        if (end == std::string::npos) end = words.size();
        if (end > at) {
            if (haystack.find(words.substr(at, end - at)) == std::string::npos) return false;
            any = true;
        }
        at = end + 1;
    }
    return any;
}
// The en_US key of an enchantment's name from `Enchant::mDescription`, which
// may or may not carry the "enchantment." prefix the strings use.
inline std::string enchantNameKey(std::string_view description) {
    if (description.empty() || description.starts_with("enchantment.")) return std::string(description);
    return "enchantment." + std::string(description);
}
bool start();
void stop();
void forget();
}

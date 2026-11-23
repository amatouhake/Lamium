#pragma once
#include <charconv>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// L-359: an item as the recipe code names it. Most items are their name. The
// few the game tells apart by an aux value (bed and banner colors, tipped
// arrows, stews, firework stars, map kinds) carry it: "minecraft:bed@14".
// Aux 0 is the bare name, so an item without variants never changes its key.
// "name@*" is how the game's table says "any aux"; it is never an item.
// L-363: the few told apart by data alone carry the data that makes them what
// they are, as SNBT after the rest: "minecraft:enchanted_book{ench:[{id:9s,lvl:5s}]}".
// A key is worked out from the stack alone, so a carried stack and a creative
// entry meet; the recipe table's keys never carry data.
namespace lamium::recipes {
inline constexpr int anyAux = 32767;
// Longest data a key carries (keys are saved, up to 400 bytes, RecipeNotesStore.cpp); longer data is left out.
inline constexpr size_t maxKeyData = 190;
inline std::string itemKey(std::string_view name, int aux, std::string_view data = {}) {
    std::string out(name);
    if (aux == anyAux) out += "@*";
    else if (aux != 0) out += "@" + std::to_string(aux);
    out += data;
    return out;
}
struct ItemKeyParts {
    std::string_view name;
    int aux = 0;
    std::string_view data; // "{...}", empty for none
};
// The key without its data: what the recipe table and a name's lookups know.
inline std::string_view baseKey(std::string_view key) {
    auto const brace = key.find('{'); // an item's name never holds one; its data may hold '@'
    return brace == std::string_view::npos || key.back() != '}' ? key : key.substr(0, brace);
}
inline ItemKeyParts splitItemKey(std::string_view key) {
    std::string_view const base = baseKey(key);
    std::string_view const data = key.substr(base.size());
    auto const at = base.rfind('@');
    if (at == std::string_view::npos) return {base, 0, data};
    auto const tail = base.substr(at + 1);
    if (tail == "*") return {base.substr(0, at), anyAux, data};
    int aux = 0;
    auto const end = tail.data() + tail.size();
    auto const read = std::from_chars(tail.data(), end, aux);
    if (read.ec != std::errc{} || read.ptr != end) return {base, 0, data}; // not a key's tail: the name has an '@' of its own
    return {base.substr(0, at), aux, data};
}
// The top-level user data tags that make an item what it is (seen in the creative list, VALIDATION-LOG
// 2026-11-24), each with the value (SNBT) that adds nothing: every banner color has Type 0, the ominous one 1.
// A banner's patterns count too, though no creative banner has any (a carried one keeps them); none is plain.
struct IdentityTag {
    std::string_view tag;
    std::string_view plain; // empty: every value counts
};
inline std::span<IdentityTag const> identityTags(std::string_view name) {
    static constexpr IdentityTag book[]{{"ench", ""}}, rocket[]{{"Fireworks", ""}}, banner[]{{"Type", "0"}, {"Patterns", "[]"}};
    if (name == "minecraft:enchanted_book") return book;
    if (name == "minecraft:firework_rocket") return rocket;
    if (name == "minecraft:banner") return banner;
    return {};
}
// A key's data from the identity tags a stack holds: `valueOf` gives a tag's value as SNBT, nothing where the
// stack lacks it. Empty where nothing counts or the data is too long to keep.
inline std::string identityData(std::string_view name, std::function<std::optional<std::string>(std::string_view tag)> const& valueOf) {
    std::string out;
    for (auto const& [tag, plain] : identityTags(name)) {
        auto const value = valueOf(tag);
        if (!value || value->empty() || (!plain.empty() && *value == plain)) continue;
        out += (out.empty() ? "{" : ",") + std::string(tag) + ":" + *value;
    }
    if (out.empty()) return out;
    out += "}";
    return out.size() <= maxKeyData ? out : std::string();
}
}

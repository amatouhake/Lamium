#pragma once
#include <charconv>
#include <string>
#include <string_view>

// L-359: an item as the recipe code names it. Most items are their name. The
// few the game tells apart by an aux value (bed and banner colors, tipped
// arrows, stews, firework stars, map kinds) carry it: "minecraft:bed@14".
// Aux 0 is the bare name, so an item without variants never changes its key.
// "name@*" is how the game's table says "any aux"; it is never an item.
namespace lamium::recipes {
inline constexpr int anyAux = 32767;
inline std::string itemKey(std::string_view name, int aux) {
    std::string out(name);
    if (aux == anyAux) out += "@*";
    else if (aux != 0) out += "@" + std::to_string(aux);
    return out;
}
struct ItemKeyParts {
    std::string_view name;
    int aux = 0;
};
inline ItemKeyParts splitItemKey(std::string_view key) {
    auto const at = key.rfind('@');
    if (at == std::string_view::npos) return {key, 0};
    auto const tail = key.substr(at + 1);
    if (tail == "*") return {key.substr(0, at), anyAux};
    int aux = 0;
    auto const end = tail.data() + tail.size();
    auto const read = std::from_chars(tail.data(), end, aux);
    if (read.ec != std::errc{} || read.ptr != end) return {key, 0}; // not a key's tail: the name has an '@' of its own
    return {key.substr(0, at), aux};
}
}

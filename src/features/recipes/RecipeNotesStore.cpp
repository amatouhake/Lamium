#include "features/recipes/RecipeNotes.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace lamium::recipes {
namespace {
using Json = nlohmann::json;
constexpr size_t maxDocument = 512 * 1024, maxText = 400;
// A string of sane length, else empty.
std::string text(Json const& value) {
    if (!value.is_string()) return {};
    auto out = value.get<std::string>();
    return out.size() <= maxText ? out : std::string();
}
Json parsed(std::string_view document) {
    if (document.size() > maxDocument) return Json();
    Json root = Json::parse(document, nullptr, false);
    return root.is_object() ? root : Json();
}
}
std::string encodeNotes(Notes const& notes) {
    Json choices = Json::object(), picks = Json::object();
    for (auto const& [item, recipe] : notes.choices) choices[item] = recipe;
    for (auto const& [group, member] : notes.picks) picks[group] = member;
    return Json{{"version", 1}, {"favourites", notes.favourites}, {"recipes", std::move(choices)}, {"picks", std::move(picks)},
                {"gathered", notes.stops}}.dump(2);
}
Notes decodeNotes(std::string_view document) {
    Notes out;
    Json const root = parsed(document);
    if (!root.is_object()) return out;
    if (auto list = root.find("favourites"); list != root.end() && list->is_array())
        for (auto const& entry : *list)
            if (auto key = text(entry); !key.empty() && !out.favourite(key) && out.favourites.size() < 512) out.favourites.push_back(std::move(key));
    auto pairs = [&](char const* name, std::map<std::string, std::string>& to) {
        auto found = root.find(name);
        if (found == root.end() || !found->is_object()) return;
        for (auto const& [key, value] : found->items())
            if (auto held = text(value); !key.empty() && key.size() <= maxText * 8 && !held.empty() && to.size() < 4096) to[key] = std::move(held);
    };
    pairs("recipes", out.choices);
    pairs("picks", out.picks);
    if (auto list = root.find("gathered"); list != root.end() && list->is_array())
        for (auto const& entry : *list) if (auto key = text(entry); !key.empty() && out.stops.size() < 4096) out.stops.insert(std::move(key));
    return out;
}
std::string encodeWanted(Wanted const& wanted) {
    Json targets = Json::array();
    for (auto const& [key, amount] : wanted.targets) targets.push_back({{"item", key}, {"count", amount}});
    return Json{{"version", 1}, {"wanted", std::move(targets)}}.dump(2);
}
Wanted decodeWanted(std::string_view document) {
    Wanted out;
    Json const root = parsed(document);
    if (!root.is_object()) return out;
    auto list = root.find("wanted");
    if (list == root.end() || !list->is_array()) return out;
    for (auto const& entry : *list) {
        if (!entry.is_object()) continue;
        auto key = text(entry.value("item", Json()));
        auto const amount = entry.value("count", Json());
        if (key.empty() || out.has(key) || !amount.is_number_unsigned()) continue;
        Count const count = amount.get<Count>();
        if (count >= 1 && out.targets.size() < 512) out.targets.emplace_back(std::move(key), std::min(count, Wanted::most));
    }
    return out;
}
bool documentReadable(std::string_view document) { return parsed(document).is_object(); }
std::optional<std::string> readDocument(std::filesystem::path const& path, bool& unreadable) {
    unreadable = false;
    std::error_code failed;
    bool const there = std::filesystem::exists(path, failed);
    if (failed) { unreadable = true; return std::nullopt; }
    if (!there) return std::nullopt;
    unreadable = true;
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    std::string out(maxDocument + 1, '\0');
    file.read(out.data(), static_cast<std::streamsize>(out.size()));
    if (file.bad()) return std::nullopt;
    out.resize(static_cast<size_t>(file.gcount()));
    unreadable = false;
    return out;
}
}

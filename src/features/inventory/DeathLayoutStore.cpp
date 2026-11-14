#include "features/inventory/DeathLayoutStore.h"
#include "app/AtomicFile.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace lamium::inventory::death {
namespace {
using Json = nlohmann::json;
constexpr size_t maxDocument = 256 * 1024;
int integer(Json const& value, int fallback) {
    if (!value.is_number_integer()) return fallback;
    auto number = value.get<double>();
    if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) return fallback;
    return static_cast<int>(number);
}
}
std::string encodeLayout(Layout const& layout) {
    Json slots = Json::array();
    for (int s = 0; s < slotCount; ++s) {
        auto const& stack = layout.slots[s];
        if (stack.empty()) continue;
        slots.push_back({{"slot", s}, {"kind", stack.kind}, {"exact", stack.exact}, {"count", stack.count},
                         {"maxStack", stack.maxStack}, {"done", layout.done[s]}});
    }
    Json root{{"version", 1}, {"x", layout.x}, {"y", layout.y}, {"z", layout.z}, {"dimension", layout.dimension},
              {"started", layout.started}, {"loadedTicks", layout.loadedTicks}, {"slots", std::move(slots)}};
    return root.dump(2);
}
Layout decodeLayout(std::string_view text) {
    if (text.size() > maxDocument) throw std::length_error("Death layout document exceeds size limit");
    auto root = Json::parse(text);
    if (!root.is_object() || integer(root.value("version", Json()), 0) != 1)
        throw std::invalid_argument("Unsupported death layout document");
    Layout layout;
    layout.x = integer(root.value("x", Json()), 0);
    layout.y = integer(root.value("y", Json()), 0);
    layout.z = integer(root.value("z", Json()), 0);
    layout.dimension = integer(root.value("dimension", Json()), 0);
    if (auto found = root.find("started"); found != root.end() && found->is_boolean()) layout.started = found->get<bool>();
    layout.loadedTicks = std::clamp(integer(root.value("loadedTicks", Json()), 0), 0, despawnTicks);
    if (auto found = root.find("slots"); found != root.end() && found->is_array())
        for (auto const& entry : *found) {
            if (!entry.is_object()) continue;
            int s = integer(entry.value("slot", Json()), -1);
            if (s < 0 || s >= slotCount) continue;
            auto kind = entry.value("kind", std::string());
            int count = integer(entry.value("count", Json()), 0);
            if (kind.empty() || count <= 0) continue;
            layout.slots[s] = {kind, entry.value("exact", kind), count,
                               std::max(1, integer(entry.value("maxStack", Json()), 64))};
            layout.done[s] = entry.value("done", false);
        }
    return layout;
}
std::optional<Layout> readLayout(std::filesystem::path const& path) {
    if (!std::filesystem::exists(path)) return std::nullopt;
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Could not open death layout document");
    std::string text(maxDocument + 1, '\0');
    file.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (file.bad()) throw std::runtime_error("Could not read death layout document");
    text.resize(static_cast<size_t>(file.gcount()));
    return decodeLayout(text);
}
void writeLayout(std::filesystem::path const& path, std::optional<Layout> const& layout) {
    if (!layout) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        return;
    }
    writeFileReplacing(path, encodeLayout(*layout), "death layout");
}
}

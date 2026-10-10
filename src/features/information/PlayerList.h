#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class IClientInstance;
namespace lamium::information::playerList {
// The player list while a key is held (BACKLOG L-128, docs/demos/player-list.html).
// Pure parts here; collect() reads the game's list into these owned rows.
struct Row {
    std::string name;
    bool self = false, host = false;
    int platform = -1;             // BuildPlatform value
    std::optional<int> dimension;  // 0 Overworld, 1 Nether, 2 End; empty when never seen
    bool dimensionCurrent = false; // In your dimension now; otherwise where they were last seen (L-131)
    std::optional<double> distance; // Blocks, only for players known to share your dimension
    std::optional<int> permission; // PlayerPermissionLevel: 0 visitor, 1 member, 2 operator, 3 custom
    int face = -1;                 // Radar face atlas index, -1 without one
};

// Short platform names: text, not logos (decided 2026-10-10).
inline std::string_view platformText(int buildPlatform) {
    switch (buildPlatform) {
    case 1: return "Android";
    case 2: return "iOS";
    case 3: return "Mac";
    case 4: return "Fire";
    case 7: case 8: return "Win";
    case 9: return "Server";
    case 11: return "PS";
    case 12: return "Switch";
    case 13: return "Xbox";
    case 15: return "Linux";
    default: return "?";
    }
}
// The vanilla permission mark for a level; members only when asked (L-131).
inline std::string_view permissionTexture(std::optional<int> level, bool members) {
    if (!level) return {};
    switch (*level) {
    case 0: return "textures/ui/permissions_visitor_hand";
    case 1: return members ? "textures/ui/permissions_member_star" : std::string_view{};
    case 2: return "textures/ui/permissions_op_crown";
    case 3: return "textures/ui/permissions_custom_dots";
    default: return {};
    }
}
inline std::string distanceText(double blocks) {
    if (!std::isfinite(blocks) || blocks < 0) return "-";
    return std::format("{} m", static_cast<long long>(std::llround(blocks)));
}
inline std::string folded(std::string_view text) {
    std::string out(text);
    for (auto& c : out)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return out;
}
// You first, then names ignoring ASCII case.
inline void order(std::vector<Row>& rows) {
    std::stable_sort(rows.begin(), rows.end(), [](Row const& a, Row const& b) {
        if (a.self != b.self) return a.self;
        return folded(a.name) < folded(b.name);
    });
}
// Up to perColumn rows per list column, as many columns as fit, the rest
// summed up as "and N more".
struct Grid { int columns = 0, perColumn = 20, shown = 0; };
inline Grid plan(int count, float columnWidth, float gap, float available, int perColumn = 20) {
    Grid grid;
    grid.perColumn = std::max(1, perColumn);
    if (count <= 0) return grid;
    int wanted = (count + grid.perColumn - 1) / grid.perColumn;
    int fitting = columnWidth > 0 && std::isfinite(available)
        ? std::max(1, static_cast<int>(std::floor((available + gap) / (columnWidth + gap)))) : 1;
    grid.columns = std::min(wanted, fitting);
    grid.shown = std::min(count, grid.columns * grid.perColumn);
    return grid;
}
// The name, or its longest prefix (whole UTF-8 characters) plus an ellipsis
// that fits `maxWidth` as `measure` sees it.
template <class Measure>
std::string fitName(std::string const& name, float maxWidth, Measure&& measure) {
    if (measure(name) <= maxWidth) return name;
    std::string cut = name;
    while (!cut.empty()) {
        // One whole character: its continuation bytes, then its first byte.
        while (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80) cut.pop_back();
        if (!cut.empty()) cut.pop_back();
        if (measure(cut + "...") <= maxWidth) return cut + "...";
    }
    return "...";
}

void setHeld(bool held);
bool held();
// The server's player list with what the client knows about each player.
std::vector<Row> collect(IClientInstance&);
}

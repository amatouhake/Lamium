#pragma once
#include "features/schematic/Placement.h"
#include "features/schematic/Structure.h"
#include "features/schematic/Verify.h"
#include <algorithm>
#include <charconv>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Differences the player chose to disregard in the check (L-183): one state
// of one cell, kept per placement in structure coordinates, so moving,
// turning or mirroring the placement keeps them on the same build cells. An
// ignored difference counts as matching; another block in that cell, or a
// difference not named, is still a mistake. An entry whose cell no longer
// holds the named block (the file changed) is stale: kept, never applied.
namespace lamium::schematic {
struct IgnoredDifference {
    Point cell;        // structure-local
    std::string block; // the file's block there (palette name)
    std::string state; // the state key, as StateDifference::key
    bool operator==(IgnoredDifference const&) const = default;
};
// Per placement. Kept small so a full placement document stays far under its
// size limit (PlacementStore's 1 MB with 64 placements); names are bounded for
// the same reason.
inline constexpr size_t maxIgnored = 64, maxIgnoredBlockName = 96, maxIgnoredStateName = 48;

// Sorted by cell, then block, then state; shared and immutable, because the
// placement it belongs to is copied into every frame's snapshot.
class IgnoreList {
public:
    enum class Added { Added, Already, Full, Invalid };
    IgnoreList() = default;
    explicit IgnoreList(std::vector<IgnoredDifference> entries) {
        std::erase_if(entries, [](auto const& e) { return !valid(e); });
        std::sort(entries.begin(), entries.end(), less);
        entries.erase(std::unique(entries.begin(), entries.end()), entries.end());
        if (entries.size() > maxIgnored) entries.resize(maxIgnored);
        if (!entries.empty()) list_ = std::make_shared<std::vector<IgnoredDifference> const>(std::move(entries));
    }
    std::vector<IgnoredDifference> const& entries() const {
        static std::vector<IgnoredDifference> const none;
        return list_ ? *list_ : none;
    }
    size_t size() const { return list_ ? list_->size() : 0; }
    bool empty() const { return size() == 0; }
    static bool valid(IgnoredDifference const& e) {
        auto plain = [](std::string_view text, size_t limit) {
            return !text.empty() && text.size() <= limit
                && std::none_of(text.begin(), text.end(), [](char c) { return c == ' ' || c == ',' || c < 0x20; });
        };
        return e.cell.x >= 0 && e.cell.y >= 0 && e.cell.z >= 0 && plain(e.block, maxIgnoredBlockName)
            && plain(e.state, maxIgnoredStateName);
    }
    Added add(IgnoredDifference entry) {
        if (!valid(entry)) return Added::Invalid;
        if (contains(entry)) return Added::Already;
        if (size() >= maxIgnored) return Added::Full;
        auto copy = entries();
        copy.push_back(std::move(entry));
        *this = IgnoreList(std::move(copy));
        return Added::Added;
    }
    bool remove(IgnoredDifference const& entry) {
        auto copy = entries();
        auto before = copy.size();
        std::erase(copy, entry);
        if (copy.size() == before) return false;
        *this = IgnoreList(std::move(copy));
        return true;
    }
    void clear() { list_.reset(); }
    bool contains(IgnoredDifference const& entry) const {
        auto const& e = entries();
        return std::binary_search(e.begin(), e.end(), entry, less);
    }
    // The cheap question a check pass asks of every cell first.
    bool anyAt(Point cell) const {
        auto const& e = entries();
        auto found = std::lower_bound(e.begin(), e.end(), cell, [](IgnoredDifference const& a, Point const& p) { return before(a.cell, p); });
        return found != e.end() && found->cell == cell;
    }
    bool ignored(Point cell, std::string_view block, std::string_view state) const {
        auto const& e = entries();
        for (auto it = std::lower_bound(e.begin(), e.end(), cell, [](IgnoredDifference const& a, Point const& p) { return before(a.cell, p); });
             it != e.end() && it->cell == cell; ++it)
            if (it->block == block && it->state == state) return true;
        return false;
    }
    // Changes whenever the entries do: the check restarts on it.
    std::string fingerprint() const {
        std::uint64_t hash = 1469598103934665603ull;
        auto mix = [&](std::string_view text) {
            for (unsigned char c : text) hash = (hash ^ c) * 1099511628211ull;
            hash = (hash ^ 0xff) * 1099511628211ull;
        };
        for (auto const& e : entries()) {
            mix(std::to_string(e.cell.x) + "," + std::to_string(e.cell.y) + "," + std::to_string(e.cell.z));
            mix(e.block);
            mix(e.state);
        }
        return empty() ? std::string{} : std::to_string(size()) + ":" + std::to_string(hash);
    }
    bool operator==(IgnoreList const& other) const { return entries() == other.entries(); }

private:
    static bool before(Point const& a, Point const& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }
    static bool less(IgnoredDifference const& a, IgnoredDifference const& b) {
        if (a.cell != b.cell) return before(a.cell, b.cell);
        if (a.block != b.block) return a.block < b.block;
        return a.state < b.state;
    }
    std::shared_ptr<std::vector<IgnoredDifference> const> list_;
};

// Saved form, one string per entry: "x,y,z block state".
inline std::string encodeIgnored(IgnoredDifference const& e) {
    return std::to_string(e.cell.x) + "," + std::to_string(e.cell.y) + "," + std::to_string(e.cell.z) + " " + e.block + " " + e.state;
}
inline std::optional<IgnoredDifference> decodeIgnored(std::string_view text) {
    auto space = text.find(' ');
    if (space == std::string_view::npos) return std::nullopt;
    auto second = text.find(' ', space + 1);
    if (second == std::string_view::npos || text.find(' ', second + 1) != std::string_view::npos) return std::nullopt;
    auto coords = text.substr(0, space);
    int values[3]{};
    char const* at = coords.data();
    char const* end = coords.data() + coords.size();
    for (int i = 0; i < 3; ++i) {
        auto [next, error] = std::from_chars(at, end, values[i]);
        if (error != std::errc{}) return std::nullopt;
        at = next;
        if (i < 2) {
            if (at == end || *at != ',') return std::nullopt;
            ++at;
        }
    }
    if (at != end) return std::nullopt;
    IgnoredDifference e{{values[0], values[1], values[2]}, std::string(text.substr(space + 1, second - space - 1)),
                        std::string(text.substr(second + 1))};
    if (!IgnoreList::valid(e)) return std::nullopt;
    return e;
}

// The structure still holds the named block at the entry's cell.
inline bool ignoreApplies(IgnoredDifference const& e, Structure const& structure) {
    auto const& s = structure.size;
    if (e.cell.x >= s.x || e.cell.y >= s.y || e.cell.z >= s.z) return false;
    auto index = static_cast<size_t>(structure.cell(e.cell.x, e.cell.y, e.cell.z));
    if (index >= structure.blocks.size()) return false;
    auto palette = structure.blocks[index];
    if (palette == voidCell || static_cast<size_t>(palette) >= structure.palette.size()) return false;
    return structure.palette[static_cast<size_t>(palette)].name == e.block;
}
// The entries that apply, for the count the screen shows.
inline size_t appliedIgnores(IgnoreList const& list, Structure const& structure) {
    return static_cast<size_t>(std::count_if(list.entries().begin(), list.entries().end(),
                                             [&](auto const& e) { return ignoreApplies(e, structure); }));
}

// A wrong-state cell's differences once the ignored ones are left out.
// `fixed` rows (block entity data, a shulker box's opening) can never be
// ignored. The cell passes only when every difference was named and ignored;
// `unlisted` says the reading found a cause it could not name as a row, which
// keeps the cell a mistake.
struct IgnoreOutcome {
    CellState state;
    std::vector<StateDifference> shown; // the differences still to list
};
inline IgnoreOutcome applyIgnores(CellState state, std::vector<StateDifference> differences, IgnoreList const& list,
                                  Point cell, std::string_view block, bool unlisted = false) {
    if (state != CellState::State || list.empty()) return {state, std::move(differences)};
    bool any = !differences.empty();
    std::erase_if(differences, [&](StateDifference const& d) { return !d.fixed && list.ignored(cell, block, d.key); });
    if (any && differences.empty() && !unlisted) return {CellState::Correct, {}};
    return {state, std::move(differences)};
}

// The world cell an ignore entry stands for under the placement now.
inline Point ignoredWorldCell(IgnoredDifference const& e, Size size, Placement const& placement) {
    return toWorld(size, placement, e.cell);
}
}

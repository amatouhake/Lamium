#pragma once
#include "features/schematic/Placement.h"
#include "features/schematic/Structure.h"
#include "features/schematic/Verification.h"
#include "features/schematic/Verify.h"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Differences the player chose to disregard in the check (L-183), kept per
// placement in structure coordinates, so moving, turning or mirroring the
// placement keeps them on the same build cells. Three kinds:
// - Block: the whole block of one cell. The cell is left out of the counts,
//   like extras' "do not count".
// - State: one state of one cell; that difference counts as matching.
// - Every: one state of every cell holding the block in this placement.
// A cell entry whose cell no longer holds the named block, or an Every entry
// whose block the file no longer has, is stale: kept, never applied.
namespace lamium::schematic {
enum class IgnoreKind : std::uint8_t { Block, State, Every };
struct IgnoreEntry {
    IgnoreKind kind = IgnoreKind::State;
    Point cell{};       // structure-local; {0, 0, 0} for Every
    std::string block;  // the file's block (palette name)
    std::string state;  // the state key, as StateDifference::key; empty for Block
    bool operator==(IgnoreEntry const&) const = default;
};
inline IgnoreEntry blockEntry(Point cell, std::string block) { return {IgnoreKind::Block, cell, std::move(block), {}}; }
inline IgnoreEntry stateEntry(Point cell, std::string block, std::string state) {
    return {IgnoreKind::State, cell, std::move(block), std::move(state)};
}
inline IgnoreEntry everyEntry(std::string block, std::string state) {
    return {IgnoreKind::Every, {}, std::move(block), std::move(state)};
}
// Per placement, all kinds together. Kept small so a full placement document
// stays far under its size limit (PlacementStore's 1 MB with 64 placements);
// names are bounded for the same reason.
inline constexpr size_t maxIgnored = 64, maxIgnoredBlockName = 96, maxIgnoredStateName = 48;

// Cell entries sorted by cell, then the Every entries; shared and immutable,
// because the placement it belongs to is copied into every frame's snapshot.
class IgnoreList {
public:
    enum class Added { Added, Already, Full, Invalid };
    IgnoreList() = default;
    explicit IgnoreList(std::vector<IgnoreEntry> entries) {
        std::erase_if(entries, [](auto const& e) { return !valid(e); });
        std::sort(entries.begin(), entries.end(), less);
        entries.erase(std::unique(entries.begin(), entries.end()), entries.end());
        if (entries.size() > maxIgnored) entries.resize(maxIgnored);
        if (entries.empty()) return;
        auto list = std::make_shared<Data>();
        list->entries = std::move(entries);
        list->everyFrom = static_cast<size_t>(std::find_if(list->entries.begin(), list->entries.end(),
            [](auto const& e) { return e.kind == IgnoreKind::Every; }) - list->entries.begin());
        list_ = std::move(list);
    }
    std::vector<IgnoreEntry> const& entries() const {
        static std::vector<IgnoreEntry> const none;
        return list_ ? list_->entries : none;
    }
    size_t size() const { return list_ ? list_->entries.size() : 0; }
    bool empty() const { return size() == 0; }
    static bool valid(IgnoreEntry const& e) {
        auto plain = [](std::string_view text, size_t limit) {
            return !text.empty() && text.size() <= limit
                && std::none_of(text.begin(), text.end(), [](char c) { return c == ' ' || c == ',' || c == '*' || c < 0x20; });
        };
        if (!plain(e.block, maxIgnoredBlockName)) return false;
        switch (e.kind) {
        case IgnoreKind::Block: return e.state.empty() && e.cell.x >= 0 && e.cell.y >= 0 && e.cell.z >= 0;
        case IgnoreKind::State: return plain(e.state, maxIgnoredStateName) && e.cell.x >= 0 && e.cell.y >= 0 && e.cell.z >= 0;
        case IgnoreKind::Every: return plain(e.state, maxIgnoredStateName) && e.cell == Point{};
        }
        return false;
    }
    Added add(IgnoreEntry entry) { return addAll({std::move(entry)}); }
    // All or nothing: the new entries are added only when every one fits
    // under the cap; entries already there are left alone.
    Added addAll(std::vector<IgnoreEntry> wanted) {
        std::erase_if(wanted, [&](auto const& e) { return !valid(e); });
        if (wanted.empty()) return Added::Invalid;
        std::sort(wanted.begin(), wanted.end(), less);
        wanted.erase(std::unique(wanted.begin(), wanted.end()), wanted.end());
        std::erase_if(wanted, [&](auto const& e) { return contains(e); });
        if (wanted.empty()) return Added::Already;
        if (size() + wanted.size() > maxIgnored) return Added::Full;
        auto copy = entries();
        copy.insert(copy.end(), wanted.begin(), wanted.end());
        *this = IgnoreList(std::move(copy));
        return Added::Added;
    }
    bool remove(IgnoreEntry const& entry) {
        return removeIf([&](IgnoreEntry const& e) { return e == entry; }) > 0;
    }
    template <class Pred>
    size_t removeIf(Pred pred) {
        auto copy = entries();
        auto removed = static_cast<size_t>(std::erase_if(copy, pred));
        if (removed) *this = IgnoreList(std::move(copy));
        return removed;
    }
    void clear() { list_.reset(); }
    bool contains(IgnoreEntry const& entry) const {
        auto const& e = entries();
        return std::binary_search(e.begin(), e.end(), entry, less);
    }
    // The whole block of the cell is ignored.
    bool wholeBlock(Point cell, std::string_view block) const {
        for (auto it = firstAt(cell); it != cellEnd() && it->cell == cell; ++it)
            if (it->kind == IgnoreKind::Block && it->block == block) return true;
        return false;
    }
    // Which entry covers a state difference of the cell: its own first.
    Covered covers(Point cell, std::string_view block, std::string_view state) const {
        for (auto it = firstAt(cell); it != cellEnd() && it->cell == cell; ++it)
            if (it->kind == IgnoreKind::State && it->block == block && it->state == state) return Covered::Here;
        for (auto it = cellEnd(); it != entries().end(); ++it)
            if (it->block == block && it->state == state) return Covered::Every;
        return Covered::None;
    }
    // The cheap question a check pass asks of every cell first: whether any
    // entry could apply to it.
    bool touches(Point cell, std::string_view block) const {
        if (empty()) return false;
        if (auto it = firstAt(cell); it != cellEnd() && it->cell == cell) return true;
        for (auto it = cellEnd(); it != entries().end(); ++it)
            if (it->block == block) return true;
        return false;
    }
    // Changes whenever the entries do: the check restarts on it.
    std::string fingerprint() const {
        std::uint64_t hash = 1469598103934665603ull;
        auto mix = [&](std::string_view text) {
            for (unsigned char c : text) hash = (hash ^ c) * 1099511628211ull;
            hash = (hash ^ 0xff) * 1099511628211ull;
        };
        for (auto const& e : entries()) mix(encode(e));
        return empty() ? std::string{} : std::to_string(size()) + ":" + std::to_string(hash);
    }
    bool operator==(IgnoreList const& other) const { return entries() == other.entries(); }

    // Saved form, one string per entry: "x,y,z block" (whole block),
    // "x,y,z block state" (one state of a cell; the per-state build's only
    // form) and "* block state" (every same block).
    static std::string encode(IgnoreEntry const& e) {
        if (e.kind == IgnoreKind::Every) return "* " + e.block + " " + e.state;
        auto cell = std::to_string(e.cell.x) + "," + std::to_string(e.cell.y) + "," + std::to_string(e.cell.z) + " " + e.block;
        return e.kind == IgnoreKind::Block ? cell : cell + " " + e.state;
    }
    static std::optional<IgnoreEntry> decode(std::string_view text) {
        std::vector<std::string_view> parts;
        for (size_t at = 0; parts.size() <= 3;) {
            auto space = text.find(' ', at);
            auto end = space == std::string_view::npos ? text.size() : space;
            parts.push_back(text.substr(at, end - at));
            if (space == std::string_view::npos) break;
            at = space + 1;
        }
        if (parts.size() < 2 || parts.size() > 3) return std::nullopt;
        IgnoreEntry e;
        e.block = std::string(parts[1]);
        if (parts.size() == 3) e.state = std::string(parts[2]);
        if (parts[0] == "*") {
            if (parts.size() != 3) return std::nullopt;
            e.kind = IgnoreKind::Every;
        } else {
            int values[3]{};
            char const* at = parts[0].data();
            char const* end = parts[0].data() + parts[0].size();
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
            e.cell = {values[0], values[1], values[2]};
            e.kind = parts.size() == 2 ? IgnoreKind::Block : IgnoreKind::State;
        }
        if (!valid(e)) return std::nullopt;
        return e;
    }

private:
    struct Data {
        std::vector<IgnoreEntry> entries;
        size_t everyFrom = 0;
    };
    using Iterator = std::vector<IgnoreEntry>::const_iterator;
    Iterator cellEnd() const { return list_ ? list_->entries.begin() + static_cast<std::ptrdiff_t>(list_->everyFrom) : entries().end(); }
    Iterator firstAt(Point cell) const {
        return std::lower_bound(entries().begin(), cellEnd(), cell, [](IgnoreEntry const& a, Point const& p) { return before(a.cell, p); });
    }
    static bool before(Point const& a, Point const& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }
    static bool less(IgnoreEntry const& a, IgnoreEntry const& b) {
        bool ae = a.kind == IgnoreKind::Every, be = b.kind == IgnoreKind::Every;
        if (ae != be) return be;
        if (a.cell != b.cell) return before(a.cell, b.cell);
        if (a.kind != b.kind) return a.kind < b.kind;
        if (a.block != b.block) return a.block < b.block;
        return a.state < b.state;
    }
    std::shared_ptr<Data const> list_;
};
inline std::string encodeIgnored(IgnoreEntry const& e) { return IgnoreList::encode(e); }
inline std::optional<IgnoreEntry> decodeIgnored(std::string_view text) { return IgnoreList::decode(text); }

// How many cells of each block the file holds (air and structure void left
// out): the "every same block" count, and whether such an entry applies.
using BlockCounts = std::map<std::string, std::uint64_t, std::less<>>;
inline BlockCounts blockCounts(Structure const& structure) {
    std::vector<std::uint64_t> perPalette(structure.palette.size());
    for (auto index : structure.blocks)
        if (index != voidCell && static_cast<size_t>(index) < perPalette.size()) ++perPalette[static_cast<size_t>(index)];
    BlockCounts counts;
    for (size_t i = 0; i < perPalette.size(); ++i)
        if (perPalette[i] && !structure.palette[i].isAir()) counts[structure.palette[i].name] += perPalette[i];
    return counts;
}
inline std::uint64_t sameBlocks(BlockCounts const& counts, std::string_view block) {
    auto found = counts.find(block);
    return found == counts.end() ? 0 : found->second;
}

// The structure still holds the named block (never air) where the entry says.
inline bool ignoreApplies(IgnoreEntry const& e, Structure const& structure, BlockCounts const& counts) {
    if (e.kind == IgnoreKind::Every) return sameBlocks(counts, e.block) > 0;
    auto const& s = structure.size;
    if (e.cell.x >= s.x || e.cell.y >= s.y || e.cell.z >= s.z) return false;
    auto index = static_cast<size_t>(structure.cell(e.cell.x, e.cell.y, e.cell.z));
    if (index >= structure.blocks.size()) return false;
    auto palette = structure.blocks[index];
    if (palette == voidCell || static_cast<size_t>(palette) >= structure.palette.size()) return false;
    auto const& block = structure.palette[static_cast<size_t>(palette)];
    return !block.isAir() && block.name == e.block;
}
struct IgnoreCount {
    size_t applied = 0, stale = 0;
};
inline IgnoreCount countIgnores(IgnoreList const& list, Structure const& structure, BlockCounts const& counts) {
    IgnoreCount out;
    for (auto const& e : list.entries()) ++(ignoreApplies(e, structure, counts) ? out.applied : out.stale);
    return out;
}
// The Ignored button (maintainer 2026-11-15): hidden with no entries;
// "Ignored: N" counts the entries that apply; with only stale ones left it
// also says how many, so a list that is not empty never reads "Ignored: 0".
enum class IgnoredButton { Hidden, Count, CountStale };
inline IgnoredButton ignoredButton(IgnoreCount count) {
    if (count.applied + count.stale == 0) return IgnoredButton::Hidden;
    return count.applied == 0 ? IgnoredButton::CountStale : IgnoredButton::Count;
}

// Marks which differences the entries cover (fixed rows never: block entity
// data, a shulker box's opening).
inline void markCovered(std::vector<StateDifference>& differences, IgnoreList const& list, Point cell, std::string_view block) {
    for (auto& d : differences) d.covered = d.fixed ? Covered::None : list.covers(cell, block, d.key);
}
// A cell's verdict once the entries are applied. `differences` are all of a
// wrong-state cell's differences (filled with what covers each); `unlisted`
// says the reading found a cause it could not name as a row, which keeps a
// wrong state a mistake. A whole-block entry wins over states, whatever the
// cell's state; a cell expecting air (an extra) has no entries.
inline IgnoreMark judgeIgnores(CellState state, bool expectsBlock, std::vector<StateDifference>& differences,
                               IgnoreList const& list, Point cell, std::string_view block, bool unlisted = false) {
    if (!expectsBlock || list.empty()) return IgnoreMark::None;
    markCovered(differences, list, cell, block);
    if (list.wholeBlock(cell, block)) return IgnoreMark::Block;
    if (state != CellState::State || unlisted || differences.empty()) return IgnoreMark::None;
    bool all = std::all_of(differences.begin(), differences.end(), [](auto const& d) { return d.covered != Covered::None; });
    return all ? IgnoreMark::States : IgnoreMark::None;
}

// The world cell an entry stands for under the placement now.
inline Point ignoredWorldCell(IgnoreEntry const& e, Size size, Placement const& placement) {
    return toWorld(size, placement, e.cell);
}
// The Ignored list's order: every-same-block entries first, then the cell
// entries nearest to `from` (a world cell), stale ones last.
inline std::vector<IgnoreEntry> listOrder(IgnoreList const& list, Structure const* structure, BlockCounts const& counts,
                                          Placement const& placement, Point from) {
    auto out = list.entries();
    auto rank = [&](IgnoreEntry const& e) {
        if (!structure || !ignoreApplies(e, *structure, counts)) return 2;
        return e.kind == IgnoreKind::Every ? 0 : 1;
    };
    auto distance = [&](IgnoreEntry const& e) -> std::int64_t {
        if (e.kind == IgnoreKind::Every || !structure) return 0;
        auto w = toWorld(structure->size, placement, e.cell);
        std::int64_t dx = w.x - from.x, dy = w.y - from.y, dz = w.z - from.z;
        return dx * dx + dy * dy + dz * dz;
    };
    std::stable_sort(out.begin(), out.end(), [&](auto const& a, auto const& b) {
        int ra = rank(a), rb = rank(b);
        if (ra != rb) return ra < rb;
        return distance(a) < distance(b);
    });
    return out;
}

// Several Check rows at once (L-183 with L-190's selection). A row can take
// entries when it names a block cell of the file: not an entity, an extra or
// another cell expecting air.
inline bool takesIgnores(Mismatch const& m) { return !m.entity && m.local.x >= 0 && !m.fileBlock.empty(); }
// A row's mark under the entries as they are now, before the next pass has
// judged it again: the Check tab dims and tags rows by it, so a press shows
// at once. A wrong state passes when every difference listed is covered.
inline IgnoreMark liveMark(Mismatch const& m, IgnoreList const& list) {
    if (!takesIgnores(m)) return m.ignore;
    if (list.wholeBlock(m.local, m.fileBlock)) return IgnoreMark::Block;
    if (m.state != CellState::State || m.states.empty()) return IgnoreMark::None;
    bool all = std::all_of(m.states.begin(), m.states.end(), [&](auto const& d) {
        return !d.fixed && list.covers(m.local, m.fileBlock, d.key) != Covered::None;
    });
    return all ? IgnoreMark::States : IgnoreMark::None;
}
// "Ignore whole blocks": the rows not yet ignored as a whole block.
inline std::vector<IgnoreEntry> wholeBlocksToAdd(std::vector<Mismatch const*> const& rows, IgnoreList const& list) {
    std::vector<IgnoreEntry> out;
    for (auto const* m : rows)
        if (takesIgnores(*m) && !list.wholeBlock(m->local, m->fileBlock)) out.push_back(blockEntry(m->local, m->fileBlock));
    return out;
}
// "Un-ignore": the rows' own entries (whole block and states here); an
// every-same-block entry stays, it covers other blocks too.
inline std::vector<IgnoreEntry> ownEntries(std::vector<Mismatch const*> const& rows, IgnoreList const& list) {
    std::vector<IgnoreEntry> out;
    for (auto const& e : list.entries()) {
        if (e.kind == IgnoreKind::Every) continue;
        for (auto const* m : rows)
            if (takesIgnores(*m) && m->local == e.cell && m->fileBlock == e.block) { out.push_back(e); break; }
    }
    return out;
}
// Every state key differing on some of the rows, in first-seen order, with
// how many rows have it: `open` the entries "Ignore on N rows" adds (rows
// where nothing covers it yet), `own` the rows' entries for it that Undo
// takes back, `every` whether an every-same-block entry covers it somewhere.
// Rows ignored as a whole block and block entity rows take no part.
struct SelectedState {
    std::string key, identifier;
    size_t rows = 0;
    std::vector<IgnoreEntry> open, own;
    bool every = false;
};
inline std::vector<SelectedState> selectedStates(std::vector<Mismatch const*> const& rows, IgnoreList const& list) {
    std::vector<SelectedState> out;
    for (auto const* m : rows) {
        if (!takesIgnores(*m) || list.wholeBlock(m->local, m->fileBlock)) continue;
        for (auto const& d : m->states) {
            if (d.fixed) continue;
            auto found = std::find_if(out.begin(), out.end(), [&](auto const& s) { return s.key == d.key; });
            if (found == out.end()) { out.push_back({d.key, m->identifier}); found = out.end() - 1; }
            ++found->rows;
            switch (list.covers(m->local, m->fileBlock, d.key)) {
            case Covered::None: found->open.push_back(stateEntry(m->local, m->fileBlock, d.key)); break;
            case Covered::Here: found->own.push_back(stateEntry(m->local, m->fileBlock, d.key)); break;
            case Covered::Every: found->every = true; break;
            }
        }
    }
    return out;
}
}

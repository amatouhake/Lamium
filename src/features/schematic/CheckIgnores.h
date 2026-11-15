#pragma once
#include "features/schematic/BlockEntityCheck.h"
#include "features/schematic/Placement.h"
#include "features/schematic/Structure.h"
#include "features/schematic/Verification.h"
#include "features/schematic/Verify.h"
#include <algorithm>
#include <charconv>
#include <climits>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// Differences the player chose to disregard in the check (L-183), kept per
// placement in structure coordinates, so moving, turning or mirroring the
// placement keeps them on the same build cells. Two kinds:
// - Place: one cell, whatever is there (an extra's air too). The cell is left
//   out of the counts, like extras' "do not count".
// - Rule: one item (a block state or a compared block entity field) on every
//   cell of the placement holding the block; that difference counts as
//   matching.
// The per-state build's one item on one cell (CellItem) still loads and
// saves unchanged, but is never applied or widened: it shows as stale.
// A place whose cell no longer holds the named block, or a rule whose block
// the file no longer has, is stale: kept, never applied.
namespace lamium::schematic {
enum class IgnoreKind : std::uint8_t { Place, CellItem, Rule };
struct IgnoreEntry {
    IgnoreKind kind = IgnoreKind::Place;
    Point cell{};       // structure-local; {0, 0, 0} for a rule
    std::string block;  // the file's block (palette name; minecraft:air for an extra's place)
    std::string item;   // itemId of the difference; empty for a place
    bool operator==(IgnoreEntry const&) const = default;
};
inline IgnoreEntry placeEntry(Point cell, std::string block) { return {IgnoreKind::Place, cell, std::move(block), {}}; }
inline IgnoreEntry ruleEntry(std::string block, std::string item) { return {IgnoreKind::Rule, {}, std::move(block), std::move(item)}; }
inline IgnoreEntry cellItemEntry(Point cell, std::string block, std::string item) {
    return {IgnoreKind::CellItem, cell, std::move(block), std::move(item)};
}
// Per placement, all kinds together. Kept small so a full placement document
// stays far under its size limit (PlacementStore's 1 MB with 64 placements);
// names are bounded for the same reason.
inline constexpr size_t maxIgnored = 64, maxIgnoredBlockName = 96, maxIgnoredStateName = 48;

// Cell entries sorted by cell, then the rules; shared and immutable, because
// the placement it belongs to is copied into every frame's snapshot.
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
        list->rulesFrom = static_cast<size_t>(std::find_if(list->entries.begin(), list->entries.end(),
            [](auto const& e) { return e.kind == IgnoreKind::Rule; }) - list->entries.begin());
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
        bool cell = e.cell.x >= 0 && e.cell.y >= 0 && e.cell.z >= 0;
        switch (e.kind) {
        case IgnoreKind::Place: return e.item.empty() && cell;
        case IgnoreKind::CellItem: return plain(e.item, maxIgnoredStateName) && cell;
        case IgnoreKind::Rule: return plain(e.item, maxIgnoredStateName) && e.cell == Point{};
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
    // The cell is ignored as a place.
    bool place(Point cell, std::string_view block) const {
        for (auto it = firstAt(cell); it != cellEnd() && it->cell == cell; ++it)
            if (it->kind == IgnoreKind::Place && it->block == block) return true;
        return false;
    }
    // A rule disregards the item on every such block.
    bool ruled(std::string_view block, std::string_view item) const {
        for (auto it = cellEnd(); it != entries().end(); ++it)
            if (it->block == block && it->item == item) return true;
        return false;
    }
    // The items ruled on the block, in saved order.
    std::vector<std::string> rules(std::string_view block) const {
        std::vector<std::string> out;
        for (auto it = cellEnd(); it != entries().end(); ++it)
            if (it->block == block) out.push_back(it->item);
        return out;
    }
    // The cheap question a check pass asks of every cell first: whether any
    // entry could apply to it.
    bool touches(Point cell, std::string_view block) const {
        if (empty()) return false;
        if (place(cell, block)) return true;
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

    // Saved form, one string per entry: "x,y,z block" (a place), "* block
    // item" (a rule) and "x,y,z block item" (the per-state build's one item
    // on one cell, kept as it was).
    static std::string encode(IgnoreEntry const& e) {
        if (e.kind == IgnoreKind::Rule) return "* " + e.block + " " + e.item;
        auto cell = std::to_string(e.cell.x) + "," + std::to_string(e.cell.y) + "," + std::to_string(e.cell.z) + " " + e.block;
        return e.kind == IgnoreKind::Place ? cell : cell + " " + e.item;
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
        if (parts.size() == 3) e.item = std::string(parts[2]);
        if (parts[0] == "*") {
            if (parts.size() != 3) return std::nullopt;
            e.kind = IgnoreKind::Rule;
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
            e.kind = parts.size() == 2 ? IgnoreKind::Place : IgnoreKind::CellItem;
        }
        if (!valid(e)) return std::nullopt;
        return e;
    }

private:
    struct Data {
        std::vector<IgnoreEntry> entries;
        size_t rulesFrom = 0;
    };
    using Iterator = std::vector<IgnoreEntry>::const_iterator;
    Iterator cellEnd() const { return list_ ? list_->entries.begin() + static_cast<std::ptrdiff_t>(list_->rulesFrom) : entries().end(); }
    Iterator firstAt(Point cell) const {
        return std::lower_bound(entries().begin(), cellEnd(), cell, [](IgnoreEntry const& a, Point const& p) { return before(a.cell, p); });
    }
    static bool before(Point const& a, Point const& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }
    static bool less(IgnoreEntry const& a, IgnoreEntry const& b) {
        bool ar = a.kind == IgnoreKind::Rule, br = b.kind == IgnoreKind::Rule;
        if (ar != br) return br;
        if (a.cell != b.cell) return before(a.cell, b.cell);
        if (a.kind != b.kind) return a.kind < b.kind;
        if (a.block != b.block) return a.block < b.block;
        return a.item < b.item;
    }
    std::shared_ptr<Data const> list_;
};
inline std::string encodeIgnored(IgnoreEntry const& e) { return IgnoreList::encode(e); }
inline std::optional<IgnoreEntry> decodeIgnored(std::string_view text) { return IgnoreList::decode(text); }

// How many cells of each block the file holds (air and structure void left
// out): whether a rule applies.
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

// A place applies while its cell still holds the named block (air included);
// a rule while the file holds its block; a one-item cell entry never.
inline bool ignoreApplies(IgnoreEntry const& e, Structure const& structure, BlockCounts const& counts) {
    if (e.kind == IgnoreKind::Rule) return sameBlocks(counts, e.block) > 0;
    if (e.kind == IgnoreKind::CellItem) return false;
    auto const& s = structure.size;
    if (e.cell.x >= s.x || e.cell.y >= s.y || e.cell.z >= s.z) return false;
    auto index = static_cast<size_t>(structure.cell(e.cell.x, e.cell.y, e.cell.z));
    if (index >= structure.blocks.size()) return false;
    auto palette = structure.blocks[index];
    if (palette == voidCell || static_cast<size_t>(palette) >= structure.palette.size()) return false;
    return structure.palette[static_cast<size_t>(palette)].name == e.block;
}

// Marks which differences a rule covers.
inline void markRuled(std::vector<StateDifference>& differences, IgnoreList const& list, std::string_view block) {
    for (auto& d : differences) d.ruled = list.ruled(block, itemId(d));
}
// A cell's verdict once the entries are applied. `differences` are all of a
// wrong-state cell's differences (marked with what a rule covers);
// `unlisted` says the reading found a cause it could not name as a row,
// which keeps a wrong state a mistake. A place wins over items, whatever
// the cell's state.
inline IgnoreMark judgeIgnores(CellState state, std::vector<StateDifference>& differences, IgnoreList const& list,
                               Point cell, std::string_view block, bool unlisted = false) {
    if (list.empty()) return IgnoreMark::None;
    markRuled(differences, list, block);
    if (list.place(cell, block)) return IgnoreMark::Place;
    if (state != CellState::State || unlisted || differences.empty()) return IgnoreMark::None;
    bool all = std::all_of(differences.begin(), differences.end(), [](auto const& d) { return d.ruled; });
    return all ? IgnoreMark::Items : IgnoreMark::None;
}

// The world cell an entry stands for under the placement now.
inline Point ignoredWorldCell(IgnoreEntry const& e, Size size, Placement const& placement) {
    return toWorld(size, placement, e.cell);
}

// A Check row can take a place when it names a cell of the file (an extra's
// air too), not an entity.
inline bool takesIgnores(Mismatch const& m) { return !m.entity && m.local.x >= 0 && !m.fileBlock.empty(); }
// A row's mark under the entries as they are now, before the next pass has
// judged it again: the Check tab tags rows by it, so a press shows at once.
// A wrong state passes when every difference listed is ruled.
inline IgnoreMark liveMark(Mismatch const& m, IgnoreList const& list) {
    if (!takesIgnores(m)) return m.ignore;
    if (list.place(m.local, m.fileBlock)) return IgnoreMark::Place;
    if (m.state != CellState::State || m.states.empty()) return IgnoreMark::None;
    bool all = std::all_of(m.states.begin(), m.states.end(), [&](auto const& d) { return list.ruled(m.fileBlock, itemId(d)); });
    return all ? IgnoreMark::Items : IgnoreMark::None;
}
// Several rows' "Ignore places": the rows not yet ignored as a place.
inline std::vector<IgnoreEntry> placesToAdd(std::vector<Mismatch const*> const& rows, IgnoreList const& list) {
    std::vector<IgnoreEntry> out;
    for (auto const* m : rows)
        if (takesIgnores(*m) && !list.place(m->local, m->fileBlock)) out.push_back(placeEntry(m->local, m->fileBlock));
    return out;
}
// Several rows' "Stop ignoring": the rows' places; rules stay, they cover
// other blocks too.
inline std::vector<IgnoreEntry> placesToRemove(std::vector<Mismatch const*> const& rows, IgnoreList const& list) {
    std::vector<IgnoreEntry> out;
    for (auto const* m : rows)
        if (takesIgnores(*m) && list.place(m->local, m->fileBlock)) out.push_back(placeEntry(m->local, m->fileBlock));
    return out;
}

// The items a rule can name on each block type of the file: its block states
// (every palette entry of that name), then the compared block entity fields
// of its cells (BlockEntityCheck.h; none while block entities are not
// compared, no sign fields while sign text is not), a note block's
// instrument and a shulker box's opening. Air has none; a block without
// items is left out.
using ComparedItems = std::map<std::string, std::vector<std::string>, std::less<>>;
inline ComparedItems comparedItems(Structure const& structure, bool ignoreSign, bool ignoreEntities) {
    ComparedItems out;
    std::vector<bool> used(structure.palette.size());
    for (auto index : structure.blocks)
        if (index != voidCell && static_cast<size_t>(index) < used.size()) used[static_cast<size_t>(index)] = true;
    auto add = [&](std::string const& block, std::string item) {
        auto& items = out[block];
        if (std::find(items.begin(), items.end(), item) == items.end()) items.push_back(std::move(item));
    };
    for (size_t i = 0; i < structure.palette.size(); ++i) {
        auto const& entry = structure.palette[i];
        if (!used[i] || entry.isAir()) continue;
        out[entry.name];
        for (auto const& state : entry.states.entries) add(entry.name, state.name);
    }
    if (!ignoreEntities) {
        std::set<std::pair<std::string, std::string>> seen;
        for (auto const& [cell, data] : structure.blockEntities) {
            if (cell < 0 || static_cast<size_t>(cell) >= structure.blocks.size()) continue;
            auto index = structure.blocks[static_cast<size_t>(cell)];
            if (index == voidCell || static_cast<size_t>(index) >= structure.palette.size()) continue;
            auto const& block = structure.palette[static_cast<size_t>(index)];
            auto id = std::string(entitycheck::entityId(data));
            if (block.isAir() || id.empty() || !seen.insert({block.name, id}).second) continue;
            for (auto const& f : entitycheck::fields)
                if (f.id == id && !(f.sign && ignoreSign)) add(block.name, entitycheck::itemId(f));
            if (id == "Music") add(block.name, std::string(instrumentItem));
            if (id == "ShulkerBox") add(block.name, std::string(openingItem));
        }
    }
    std::erase_if(out, [](auto const& line) { return line.second.empty(); });
    return out;
}
// The ignore view's block types in the Materials tab's order: by where a
// line of the same name stands there, the others after them by name.
inline std::vector<std::string> inMaterialsOrder(std::vector<std::pair<std::string, std::string>> blocks,
                                                 std::vector<std::string> const& materialNames) {
    auto rank = [&](std::string const& name) {
        return static_cast<size_t>(std::find(materialNames.begin(), materialNames.end(), name) - materialNames.begin());
    };
    std::stable_sort(blocks.begin(), blocks.end(), [&](auto const& a, auto const& b) {
        auto ra = rank(a.second), rb = rank(b.second);
        if (ra != rb) return ra < rb;
        return a.second < b.second;
    });
    std::vector<std::string> out;
    for (auto& b : blocks) out.push_back(std::move(b.first));
    return out;
}

// The ignore view's list (L-183): "Ignored places" (applied ones nearest to
// `from`, a world cell, then stale ones and old one-item entries; "None"
// without any), then "Blocks whose differences can be ignored" (`blocks`, in
// the order given, each with its rules), then the rules of blocks the file
// no longer has, one row per block (a ruled block not given but still in the
// file comes before them as a block).
struct IgnoreRow {
    enum class Kind { PlacesHeading, NoPlaces, Place, BlocksHeading, Block, StaleBlock };
    Kind kind = Kind::PlacesHeading;
    IgnoreEntry entry{};            // Place
    std::string block;              // Block, StaleBlock, and a place's block
    std::vector<std::string> rules; // Block, StaleBlock: the items ruled
    bool stale = false;
    bool selectable() const { return kind == Kind::Place || kind == Kind::Block || kind == Kind::StaleBlock; }
};
inline std::vector<IgnoreRow> ignoreRows(IgnoreList const& list, Structure const* structure, BlockCounts const& counts,
                                         std::vector<std::string> const& blocks, Placement const& placement, Point from) {
    std::vector<IgnoreRow> out;
    out.push_back({IgnoreRow::Kind::PlacesHeading});
    std::vector<std::pair<std::int64_t, IgnoreRow>> places;
    for (auto const& e : list.entries()) {
        if (e.kind == IgnoreKind::Rule) continue;
        bool applies = structure && ignoreApplies(e, *structure, counts);
        std::int64_t order = INT64_MAX;
        if (applies) {
            auto w = toWorld(structure->size, placement, e.cell);
            std::int64_t dx = w.x - from.x, dy = w.y - from.y, dz = w.z - from.z;
            order = dx * dx + dy * dy + dz * dz;
        }
        places.push_back({order, {IgnoreRow::Kind::Place, e, e.block, {}, !applies}});
    }
    std::stable_sort(places.begin(), places.end(), [](auto const& a, auto const& b) { return a.first < b.first; });
    if (places.empty()) out.push_back({IgnoreRow::Kind::NoPlaces});
    for (auto& p : places) out.push_back(std::move(p.second));
    out.push_back({IgnoreRow::Kind::BlocksHeading});
    for (auto const& block : blocks) out.push_back({IgnoreRow::Kind::Block, {}, block, list.rules(block)});
    // Ruled blocks not given (none of their items compared now, or gone).
    std::vector<IgnoreRow> later;
    for (auto const& e : list.entries()) {
        if (e.kind != IgnoreKind::Rule || std::find(blocks.begin(), blocks.end(), e.block) != blocks.end()
            || std::any_of(later.begin(), later.end(), [&](auto const& r) { return r.block == e.block; }))
            continue;
        bool applies = structure && ignoreApplies(e, *structure, counts);
        later.push_back({applies ? IgnoreRow::Kind::Block : IgnoreRow::Kind::StaleBlock, {}, e.block, list.rules(e.block), !applies});
    }
    std::stable_partition(later.begin(), later.end(), [](auto const& r) { return !r.stale; });
    out.insert(out.end(), later.begin(), later.end());
    return out;
}
// How many listed rows of the block differ in the item now.
inline size_t differingRows(std::vector<Mismatch> const& rows, std::string_view block, std::string_view item) {
    return static_cast<size_t>(std::count_if(rows.begin(), rows.end(), [&](Mismatch const& m) {
        return m.state == CellState::State && m.fileBlock == block
            && std::any_of(m.states.begin(), m.states.end(), [&](auto const& d) { return itemId(d) == item; });
    }));
}
}

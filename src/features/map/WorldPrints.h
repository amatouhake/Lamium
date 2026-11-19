#pragma once
#include <algorithm>
#include <bit>
#include <cstdint>
#include <string>
#include <vector>

// Which data set of one identity (a server's key, a Realm's, a friend's world's) the joined world uses (BACKLOG
// L-365). The world behind an identity can change: a Realm's slots, a server's reset, a copy of a world put in a
// slot. Nothing tells a copy from the same world changed, so each data set remembers the prints it was used with,
// a print seen before is used without a word, and a new one is decided for now and asked once. Pure.
namespace lamium::map::prints {
struct Print {
    std::uint64_t seed = 0; // 0: the server hides it.
    std::string name;
    int mode = -1;          // The default game mode.
    bool hardcore = false;
    bool operator==(Print const&) const = default;
};
// What differs from the print compared with.
enum Diff : unsigned { Seed = 1, Name = 2, Mode = 4 };
inline unsigned diff(Print const& a, Print const& b) {
    return (a.seed != b.seed ? Seed : 0u) | (a.name != b.name ? Name : 0u)
         | (a.mode != b.mode || a.hardcore != b.hardcore ? Mode : 0u);
}
// One print a data set was used with. `confirmed` false: decided for now and not answered yet; `fresh` says the
// decision was new data, and `other` the data it was compared with.
struct Row {
    std::string base, key;
    Print print;
    bool confirmed = true, fresh = false;
    std::string other;
    unsigned diff = 0;
    bool operator==(Row const&) const = default;
};
inline constexpr size_t maxRows = 2000, maxPerBase = 24;
// The n-th data set of a base after the base itself.
inline std::string variantKey(std::string const& base, int n) { return base + "-w" + std::to_string(n); }
// The answer to wait for, when a join's print is decided for now.
struct Pick {
    std::string key;
    bool asked = false, fresh = false;
    std::string other;
    unsigned diff = 0;
};
namespace detail {
inline auto find(std::vector<Row>& rows, std::string const& base, Print const& print) {
    return std::find_if(rows.rbegin(), rows.rend(), [&](Row const& r) { return r.base == base && r.print == print; });
}
// A key no row and no folder holds yet.
template<class Exists>
std::string unused(std::vector<Row> const& rows, std::string const& base, Exists exists) {
    for (int n = 2;; ++n) {
        auto key = variantKey(base, n);
        if (std::none_of(rows.begin(), rows.end(), [&](Row const& r) { return r.key == key; }) && !exists(key)) return key;
    }
}
inline void add(std::vector<Row>& rows, Row row) {
    // The oldest row of the same base goes first, an unanswered one before an answered one; then the oldest of all.
    auto const base = row.base;
    if (std::count_if(rows.begin(), rows.end(), [&](Row const& r) { return r.base == base; }) >= static_cast<long>(maxPerBase)) {
        auto oldest = std::find_if(rows.begin(), rows.end(), [&](Row const& r) { return r.base == base && !r.confirmed; });
        if (oldest == rows.end()) oldest = std::find_if(rows.begin(), rows.end(), [&](Row const& r) { return r.base == base; });
        rows.erase(oldest);
    }
    if (rows.size() >= maxRows) rows.erase(rows.begin());
    rows.push_back(std::move(row));
}
}
// The key for a join to `base` with `print`; rows are kept most recently used last. `exists(key)`: a folder or file
// already holds data under that name, so a new data set does not take it.
template<class Exists>
Pick pick(std::vector<Row>& rows, std::vector<std::string> const& unsplit, std::string const& base, Print const& print,
          Exists exists) {
    if (base.empty() || std::find(unsplit.begin(), unsplit.end(), base) != unsplit.end()) return {base, false, false, {}, 0};
    if (auto seen = detail::find(rows, base, print); seen != rows.rend()) {
        Row row = *seen;
        rows.erase(std::next(seen).base());
        rows.push_back(row);
        return {row.key, !row.confirmed, row.fresh, row.other, row.diff};
    }
    auto latest = [&](auto test) {
        return std::find_if(rows.rbegin(), rows.rend(), [&](Row const& r) { return r.base == base && test(r); });
    };
    auto last = latest([](Row const& r) { return r.confirmed; });
    if (last == rows.rend()) last = latest([](Row const&) { return true; });
    // Nothing recorded: data from before L-365, or the first join. It is this world's.
    if (last == rows.rend()) {
        detail::add(rows, {base, base, print, true, false, {}, 0});
        return {base, false, false, {}, 0};
    }
    Row const compared = *last;
    // Of the data sets used with this seed, the one whose print is closest, the latest of those.
    auto sameSeed = rows.rend();
    if (print.seed)
        for (auto it = rows.rbegin(); it != rows.rend(); ++it)
            if (it->base == base && it->print.seed == print.seed
                && (sameSeed == rows.rend() || std::popcount(diff(it->print, print)) < std::popcount(diff(sameSeed->print, print))))
                sameSeed = it;
    Row row{base, {}, print, false, false, {}, 0};
    if (sameSeed == rows.rend() && print.seed && compared.print.seed) {
        row.key = detail::unused(rows, base, exists);
        row.fresh = true;
        row.other = compared.key;
        row.diff = diff(compared.print, print);
    } else {
        Row const& kept = sameSeed != rows.rend() ? *sameSeed : compared;
        row.key = row.other = kept.key;
        row.diff = diff(kept.print, print);
    }
    Pick result{row.key, true, row.fresh, row.other, row.diff};
    detail::add(rows, std::move(row));
    return result;
}
// The player's answers for the joined world's print. Each returns the key to use, empty when the print has no row.
// Keep what was decided: new data stays new, kept data stays kept.
inline std::string keep(std::vector<Row>& rows, std::string const& base, Print const& print) {
    auto row = detail::find(rows, base, print);
    if (row == rows.rend()) return {};
    *row = {row->base, row->key, row->print, true, false, {}, 0};
    return row->key;
}
// This world uses `key` from now on: the saved data compared with, or any data the player chose.
inline std::string use(std::vector<Row>& rows, std::string const& base, Print const& print, std::string const& key) {
    if (key.empty()) return {};
    auto row = detail::find(rows, base, print);
    if (row == rows.rend()) {
        detail::add(rows, {base, key, print, true, false, {}, 0});
        return key;
    }
    *row = {base, key, print, true, false, {}, 0};
    return key;
}
// New data for this world, unless it already has its own.
template<class Exists>
std::string startNew(std::vector<Row>& rows, std::string const& base, Print const& print, Exists exists) {
    auto row = detail::find(rows, base, print);
    if (row == rows.rend()) return {};
    if (!row->fresh) {
        auto shared = std::count_if(rows.begin(), rows.end(), [&](Row const& r) { return r.key == row->key; });
        if (shared > 1 || row->key == base) row->key = detail::unused(rows, base, exists);
    }
    *row = {row->base, row->key, row->print, true, false, {}, 0};
    return row->key;
}
// "Do not split here": the base is used for every world of it from now on.
inline std::string noSplit(std::vector<std::string>& unsplit, std::string const& base) {
    if (base.empty()) return {};
    if (std::find(unsplit.begin(), unsplit.end(), base) == unsplit.end()) unsplit.push_back(base);
    return base;
}
// The data under `key` was deleted: rows naming it go.
inline void forget(std::vector<Row>& rows, std::string const& key) {
    std::erase_if(rows, [&](Row const& r) { return r.key == key; });
}
}

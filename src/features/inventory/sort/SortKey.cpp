#include "features/inventory/sort/SortKey.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>

namespace lamium::inventory::sort {

namespace {

template <class T>
int cmp(T const& a, T const& b) {
    if (a < b) return -1;
    if (b < a) return 1;
    return 0;
}

int compareEnchantmentLists(std::vector<Enchantment> const& a, std::vector<Enchantment> const& b) {
    size_t const n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        if (a[i].id != b[i].id) return cmp(a[i].id, b[i].id);
        if (a[i].level != b[i].level) return cmp(b[i].level, a[i].level); // higher level first
    }
    // Same prefix: the item with more enchantments leads.
    return cmp(b.size(), a.size());
}

// Fixed-width decimal so that string comparison equals numeric comparison.
std::string padded(int value, int width) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%0*d", width, value < 0 ? 0 : value);
    return buf;
}

} // namespace

int compareKeys(SortKey const& a, SortKey const& b) {
    if (int c = cmp(static_cast<int>(a.section), static_cast<int>(b.section))) return c;
    if (int c = cmp(a.creativeIndex, b.creativeIndex)) return c;
    if (int c = cmp(a.typeName, b.typeName)) return c;
    if (int c = cmp(a.aux, b.aux)) return c;
    if (int c = cmp(a.nameRank, b.nameRank)) return c;
    if (int c = cmp(a.name, b.name)) return c;
    if (int c = cmp(a.variantRank, b.variantRank)) return c;
    if (int c = compareEnchantmentLists(a.enchantments, b.enchantments)) return c;
    if (int c = cmp(a.contents, b.contents)) return c;
    if (int c = cmp(a.damage, b.damage)) return c;
    if (int c = cmp(a.tail, b.tail)) return c;
    return cmp(a.detail, b.detail);
}

std::string normalizeName(std::string const& name) {
    std::string out;
    out.reserve(name.size());
    for (size_t i = 0; i < name.size(); ++i) {
        // "§" is the two-byte UTF-8 sequence C2 A7; it and the following
        // code character are formatting only.
        if (static_cast<unsigned char>(name[i]) == 0xC2 && i + 1 < name.size()
            && static_cast<unsigned char>(name[i + 1]) == 0xA7) {
            i += 2; // skip the code character too
            continue;
        }
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(name[i]))));
    }
    auto const first = out.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    auto const last = out.find_last_not_of(" \t\r\n");
    return out.substr(first, last - first + 1);
}

std::vector<Enchantment> canonicalEnchantments(std::vector<Enchantment> list) {
    std::sort(list.begin(), list.end(), [](Enchantment const& x, Enchantment const& y) {
        if (x.id != y.id) return x.id < y.id;
        return x.level > y.level;
    });
    list.erase(std::unique(list.begin(), list.end()), list.end());
    return list;
}

std::string keySignature(SortKey const& key) {
    std::string out;
    out += padded(static_cast<int>(key.section), 1);
    out += padded(key.creativeIndex == INT_MAX ? 999999999 : key.creativeIndex, 9);
    out += key.typeName;
    out += ':';
    out += padded(key.aux, 5);
    out += ':';
    out += padded(key.nameRank, 1);
    out += key.name;
    out += ':';
    out += padded(key.variantRank, 1);
    for (auto const& e : key.enchantments) {
        out += padded(e.id, 3);
        out += '-';
        out += padded(99 - std::min(e.level, 99), 2); // higher level first
        out += ',';
    }
    out += ':';
    out += key.contents;
    out += ':';
    out += padded(key.damage, 6);
    out += ':';
    out += key.tail;
    out += ':';
    out += key.detail;
    return out;
}

std::string contentSignature(std::vector<ContentEntry> entries) {
    // Merge equal kinds regardless of slot layout, then order them like an
    // inventory would be ordered.
    struct KeyLess {
        bool operator()(SortKey const& a, SortKey const& b) const { return compareKeys(a, b) < 0; }
    };
    std::map<SortKey, int, KeyLess> totals;
    for (auto const& e : entries) {
        if (e.count <= 0 || e.key.typeName.empty()) continue;
        totals[e.key] += e.count;
    }
    std::string out;
    for (auto const& [key, total] : totals) {
        out += keySignature(key);
        out += 'x';
        out += padded(99999 - std::min(total, 99999), 5); // bigger totals first
        out += ';';
    }
    return out;
}

char sectionLabel(Section section) {
    switch (section) {
    case Section::ShulkerBox:
        return 'S';
    case Section::Equipment:
        return 'E';
    case Section::Items:
        return 'I';
    case Section::Construction:
        return 'C';
    case Section::Nature:
        return 'N';
    default:
        return '?';
    }
}

} // namespace lamium::inventory::sort

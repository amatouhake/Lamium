#pragma once
#include "features/schematic/Nbt.h"
#include "features/schematic/Placement.h"
#include <cmath>
#include <optional>
#include <string_view>
#include <vector>

// The block entity fields the check compares (L-303, maintainer 2026-11-03):
// only those the client keeps current, which are the ones it draws, plus a
// lectern's page, which a comparator reads and the client was seen to follow.
// A container's contents reach the client once and never update, so they are
// not here. A field is a row of the table, not code. Names are the game's as
// it saves them (Bedrock block entity ids and keys).
namespace lamium::schematic::entitycheck {
enum class Kind {
    Value, // equal as stored (numbers by value)
    Angle, // degrees, turned with the placement like an entity's yaw
    Item,  // an item stack by its item and data value, not its count or tags
    Block, // a block entry by name and states, not its version
};
struct Field {
    std::string_view id, key;
    // The compound the field sits in (a sign's side); empty: the top level.
    std::string_view within = {};
    Kind kind = Kind::Value;
    // The game leaves the field out when it holds nothing (an empty frame or
    // pot), so a missing field is "none". Otherwise a side without it is not
    // compared: a file from another source may not store it.
    bool absentIsEmpty = false;
    // Compared only while both sides hold this field (a lectern's page needs
    // its book).
    std::string_view onlyWith = {};
    // Covered by "Ignore sign text".
    bool sign = false;
};
inline constexpr Field fields[] = {
    {"Music", "note"},
    {"Lectern", "hasBook"},
    {"Lectern", "page", {}, Kind::Value, false, "hasBook"},
    {"FlowerPot", "PlantBlock", {}, Kind::Block, true},
    {"Banner", "Base"},
    {"Banner", "Patterns", {}, Kind::Value, true},
    {"Banner", "Type"},
    {"Skull", "SkullType"},
    {"Skull", "Rotation", {}, Kind::Angle},
    {"Bed", "color"},
    {"DecoratedPot", "sherds"},
    {"ItemFrame", "Item", {}, Kind::Item, true},
    {"ItemFrame", "ItemRotation", {}, Kind::Value, false, "Item"},
    {"GlowItemFrame", "Item", {}, Kind::Item, true},
    {"GlowItemFrame", "ItemRotation", {}, Kind::Value, false, "Item"},
    {"Sign", "Text", "FrontText", Kind::Value, false, {}, true},
    {"Sign", "SignTextColor", "FrontText", Kind::Value, false, {}, true},
    {"Sign", "IgnoreLighting", "FrontText", Kind::Value, false, {}, true},
    {"Sign", "Text", "BackText", Kind::Value, false, {}, true},
    {"Sign", "SignTextColor", "BackText", Kind::Value, false, {}, true},
    {"Sign", "IgnoreLighting", "BackText", Kind::Value, false, {}, true},
    {"HangingSign", "Text", "FrontText", Kind::Value, false, {}, true},
    {"HangingSign", "SignTextColor", "FrontText", Kind::Value, false, {}, true},
    {"HangingSign", "IgnoreLighting", "FrontText", Kind::Value, false, {}, true},
    {"HangingSign", "Text", "BackText", Kind::Value, false, {}, true},
    {"HangingSign", "SignTextColor", "BackText", Kind::Value, false, {}, true},
    {"HangingSign", "IgnoreLighting", "BackText", Kind::Value, false, {}, true},
};

inline std::string_view entityId(nbt::Compound const& data) {
    auto const* id = data.find("id");
    auto const* text = id ? id->as<std::string>() : nullptr;
    return text ? std::string_view(*text) : std::string_view{};
}
// Whether the check reads the world's block entity for a cell whose file
// entry is `data`: only for an id with a compared field, so a pass does not
// save every chest it meets.
inline bool compared(nbt::Compound const& data, bool ignoreSign) {
    auto id = entityId(data);
    if (id.empty()) return false;
    for (auto const& f : fields)
        if (f.id == id && !(f.sign && ignoreSign)) return true;
    return false;
}

namespace detail {
inline nbt::Tag const* lookup(nbt::Compound const& data, Field const& f) {
    if (f.within.empty()) return data.find(f.key);
    auto const* outer = data.find(f.within);
    auto const* compound = outer ? outer->as<nbt::Compound>() : nullptr;
    return compound ? compound->find(f.key) : nullptr;
}
inline std::optional<double> number(nbt::Tag const& tag) {
    std::int64_t whole = 0;
    if (tag.integer(whole)) return static_cast<double>(whole);
    if (auto const* f = tag.as<float>()) return *f;
    if (auto const* d = tag.as<double>()) return *d;
    return std::nullopt;
}
inline bool sameValue(nbt::Tag const& a, nbt::Tag const& b) {
    auto x = number(a), y = number(b);
    if (x && y) return std::abs(*x - *y) < 1e-3;
    return nbt::text(a) == nbt::text(b);
}
inline bool sameEntry(nbt::Tag const& a, nbt::Tag const& b, std::initializer_list<std::string_view> keys) {
    auto const* x = a.as<nbt::Compound>();
    auto const* y = b.as<nbt::Compound>();
    if (!x || !y) return sameValue(a, b);
    for (auto key : keys) {
        auto const* p = x->find(key);
        auto const* q = y->find(key);
        if (!p && !q) continue;
        // An item's data value is left out when it is 0.
        std::int64_t zero = 0;
        if (!p || !q) {
            auto const* present = p ? p : q;
            if (present->integer(zero) && zero == 0) continue;
            return false;
        }
        if (!sameValue(*p, *q)) return false;
    }
    return true;
}
// Degrees apart, folded into [0, 180].
inline double angleApart(double a, double b) {
    double d = std::fmod(std::abs(a - b), 360.0);
    return d > 180 ? 360 - d : d;
}
}

// What the file stores, as the world should hold it after the placement's
// turns and mirror (only an angle changes).
inline nbt::Tag placed(Field const& f, nbt::Tag tag, Placement const& placement) {
    if (f.kind != Kind::Angle) return tag;
    auto value = detail::number(tag);
    if (!value) return tag;
    return nbt::Tag{toWorldYaw(static_cast<float>(*value), placement)};
}

// One compared field that differs. A side without the field (where it means
// "none") holds nothing.
struct Difference {
    Field const* field = nullptr;
    std::optional<nbt::Tag> expected, actual;
};
// The compared fields that differ between the file's block entity and the
// world's, both as saved. A field that one side cannot say anything about
// is not a difference: the block alone decides, as before.
inline std::vector<Difference> differences(nbt::Compound const& file, nbt::Compound const& world,
                                           Placement const& placement, bool ignoreSign) {
    std::vector<Difference> out;
    auto id = entityId(file);
    if (id.empty() || entityId(world) != id) return out;
    for (auto const& f : fields) {
        if (f.id != id || (f.sign && ignoreSign)) continue;
        if (!f.onlyWith.empty()) {
            Field gate{f.id, f.onlyWith, f.within};
            auto const* a = detail::lookup(file, gate);
            auto const* b = detail::lookup(world, gate);
            std::int64_t flag = 1;
            auto held = [&](nbt::Tag const* t) { return t && (!t->integer(flag) || flag != 0); };
            if (!held(a) || !held(b)) continue;
        }
        auto const* want = detail::lookup(file, f);
        auto const* now = detail::lookup(world, f);
        if ((!want || !now) && !f.absentIsEmpty) continue;
        if (!want && !now) continue;
        std::optional<nbt::Tag> expected;
        if (want) expected = placed(f, *want, placement);
        bool same = false;
        if (expected && now) {
            switch (f.kind) {
            case Kind::Value: same = detail::sameValue(*expected, *now); break;
            case Kind::Angle: {
                auto x = detail::number(*expected), y = detail::number(*now);
                same = x && y ? detail::angleApart(*x, *y) < .5 : detail::sameValue(*expected, *now);
                break;
            }
            case Kind::Item: same = detail::sameEntry(*expected, *now, {"Name", "Damage"}); break;
            case Kind::Block: same = detail::sameEntry(*expected, *now, {"name", "states"}); break;
            }
        }
        if (same) continue;
        out.push_back({&f, std::move(expected), now ? std::optional<nbt::Tag>(*now) : std::nullopt});
    }
    return out;
}
}

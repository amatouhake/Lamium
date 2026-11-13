#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

// Restoring the death-time inventory layout on pickup (BACKLOG L-109). Pure:
// the glue snapshots the inventory, asks for one move at a time, performs it
// and plans again from the inventory it then reads.
namespace lamium::inventory::death {
// Slots: 0-35 the inventory (0-8 hotbar), 36-39 armor (head, chest, legs,
// feet), 40 the offhand.
inline constexpr int slotCount = 41, armorFirst = 36, offhandSlot = 40;
inline bool equipment(int slot) { return slot >= armorFirst; }
// Which slots are put back (maintainer, 2026-10-08): the hotbar and
// equipment by default, since restoring everything takes a while. Items for
// them are still taken from anywhere.
enum class Scope { HotbarEquipment, All };
inline bool inScope(int slot, Scope scope) {
    return slot < 9 || equipment(slot) || scope == Scope::All;
}
struct Stack {
    std::string kind;  // the item, e.g. minecraft:diamond_sword
    std::string exact; // kind plus what tells two of it apart (damage, enchantments, ...)
    int count = 0;
    int maxStack = 64;
    bool empty() const { return count <= 0 || kind.empty(); }
    bool operator==(Stack const&) const = default;
};
using Slots = std::array<Stack, slotCount>;
struct Layout {
    int x = 0, y = 0, z = 0, dimension = 0; // the death point
    Slots slots;
    // Set once a slot holds its death-time item; it is never touched again,
    // so later rearranging by the player is left alone.
    std::array<bool, slotCount> done{};
    // An item of it was picked up at the place of the death (L-332).
    bool started = false;
    bool operator==(Layout const&) const = default;
};
// L-332: the layout belongs to the place of the death, not to a waypoint.
// Pickups count only within this distance of it, in its dimension; once one
// happened there, going farther ends the layout, so pickups elsewhere never
// rearrange the inventory.
inline constexpr double placeDistance = 32;
inline bool atPlace(Layout const& layout, int dimension, double x, double y, double z) {
    return dimension == layout.dimension
        && std::hypot(layout.x + .5 - x, double(layout.y) - y, layout.z + .5 - z) <= placeDistance;
}
// Equipment, offhand, hotbar, then the main inventory.
inline std::array<int, slotCount> targetOrder() {
    std::array<int, slotCount> order{};
    int i = 0;
    for (int s = armorFirst; s < slotCount; ++s) order[i++] = s;
    for (int s = 0; s < armorFirst; ++s) order[i++] = s;
    return order;
}
inline bool satisfied(Stack const& want, Stack const& now) {
    return !want.empty() && now.exact == want.exact && now.count >= want.count;
}
// Marks the slots that hold their death-time item; true when every slot is done.
inline bool markDone(Layout& layout, Slots const& now, Scope scope = Scope::All) {
    bool all = true;
    for (int s = 0; s < slotCount; ++s) {
        auto const& want = layout.slots[s];
        if (want.empty() || !inScope(s, scope)) continue;
        if (!layout.done[s] && satisfied(want, now[s])) layout.done[s] = true;
        all = all && layout.done[s];
    }
    return all;
}
// Whether the layout names any item; an empty one is not worth keeping.
inline bool anyItem(Slots const& slots) {
    return std::any_of(slots.begin(), slots.end(), [](Stack const& s) { return !s.empty(); });
}
// Deaths and respawns of the local player. Only a player first seen alive in
// this world can die: while joining a world the player briefly reads as not
// alive, and taking that for a death threw away a saved layout.
class LifeWatch {
public:
    enum class Event { None, Died, Respawned };
    Event update(bool alive) {
        Event event = Event::None;
        if (seenAlive && wasAlive && !alive) event = Event::Died;
        else if (dead && alive) event = Event::Respawned;
        if (event == Event::Died) dead = true;
        if (event == Event::Respawned) dead = false;
        seenAlive = seenAlive || alive;
        wasAlive = alive;
        return event;
    }
private:
    bool seenAlive = false, wasAlive = false, dead = false;
};
enum class MoveKind { Swap, Transfer };
struct Move {
    MoveKind kind;
    int from, to;
    int count = 0; // Transfer: how many from `from` onto `to` (empty or the same item)
    bool operator==(Move const&) const = default;
};
// The next move toward the layout, or none. Never drops anything: a swap or a
// transfer only moves items between the player's own slots. Equipment slots
// already wearing something else are left alone, and items are never taken
// out of equipment.
inline std::optional<Move> nextMove(Layout const& layout, Slots const& now, Scope scope = Scope::All) {
    // Slots whose contents stay: done, or already holding their own item.
    std::array<bool, slotCount> reserved{};
    for (int s = 0; s < slotCount; ++s)
        reserved[s] = layout.done[s] || satisfied(layout.slots[s], now[s]) || equipment(s);
    auto sources = [&](int target, auto&& matches) {
        std::vector<int> found;
        for (int s = 0; s < armorFirst; ++s)
            if (s != target && !reserved[s] && !now[s].empty() && matches(now[s])) found.push_back(s);
        // Misplaced stacks first (their slot wants something else), then larger ones.
        std::stable_sort(found.begin(), found.end(), [&](int a, int b) {
            bool am = now[a].kind != layout.slots[a].kind, bm = now[b].kind != layout.slots[b].kind;
            if (am != bm) return am;
            return now[a].count > now[b].count;
        });
        return found;
    };
    auto emptySlot = [&](int except) -> std::optional<int> {
        std::optional<int> any;
        for (int s = 0; s < armorFirst; ++s) {
            if (s == except || reserved[s] || !now[s].empty()) continue;
            if (layout.slots[s].empty()) return s; // Free in the layout too.
            if (!any) any = s;
        }
        return any;
    };
    for (int t : targetOrder()) {
        auto const& want = layout.slots[t];
        if (!inScope(t, scope) || want.empty() || layout.done[t] || satisfied(want, now[t])) continue;
        auto const& cur = now[t];
        auto exact = sources(t, [&](Stack const& s) { return s.exact == want.exact; });
        auto kind = sources(t, [&](Stack const& s) { return s.kind == want.kind; });
        auto const& pool = exact.empty() ? kind : exact;
        if (!cur.empty() && cur.exact == want.exact) {
            // Top up the same item.
            int room = std::min(want.count, cur.maxStack) - cur.count;
            if (room <= 0 || exact.empty()) { reserved[t] = true; continue; }
            return Move{MoveKind::Transfer, exact.front(), t, std::min(room, now[exact.front()].count)};
        }
        if (!cur.empty() && cur.kind == want.kind && exact.empty()) {
            // The same item, told apart (another durability): good enough
            // while the exact one is not back.
            reserved[t] = true;
            continue;
        }
        if (pool.empty()) { if (!cur.empty()) reserved[t] = true; continue; }
        int source = pool.front();
        if (equipment(t)) {
            if (!cur.empty()) { reserved[t] = true; continue; } // Wearing something else.
            return Move{MoveKind::Swap, source, t};
        }
        if (cur.empty()) {
            if (now[source].count <= want.count) return Move{MoveKind::Swap, source, t};
            return Move{MoveKind::Transfer, source, t, want.count};
        }
        // Something else is in the way.
        if (now[source].count <= want.count) return Move{MoveKind::Swap, source, t};
        if (auto free = emptySlot(t)) return Move{MoveKind::Transfer, t, *free, cur.count};
        return Move{MoveKind::Swap, source, t};
    }
    return std::nullopt;
}
// Applies a move to a copy of the slots: what the inventory should read
// afterwards (tests, and the glue's check that a move took effect).
inline Slots applied(Slots slots, Move const& move) {
    if (move.kind == MoveKind::Swap) {
        std::swap(slots[move.from], slots[move.to]);
        return slots;
    }
    auto& from = slots[move.from];
    auto& to = slots[move.to];
    int n = std::min(move.count, from.count);
    if (to.empty()) { to = from; to.count = 0; }
    to.count += n;
    from.count -= n;
    if (from.count <= 0) from = {};
    return slots;
}
// How many of each kind the layout names, from `now`: an increase is a pickup.
inline int countOf(Slots const& slots, std::string const& kind) {
    int n = 0;
    for (auto const& s : slots) if (!s.empty() && s.kind == kind) n += s.count;
    return n;
}
inline bool pickedUp(Layout const& layout, Slots const& before, Slots const& after, Scope scope = Scope::All) {
    for (int s = 0; s < slotCount; ++s) {
        auto const& kind = layout.slots[s].kind;
        if (layout.slots[s].empty() || layout.done[s] || !inScope(s, scope)) continue;
        if (countOf(after, kind) > countOf(before, kind)) return true;
    }
    return false;
}
}

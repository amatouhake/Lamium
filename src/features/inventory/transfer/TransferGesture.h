#pragma once
#include <optional>
#include <span>
#include <string_view>
#include <algorithm>
#include <vector>
#include <array>

namespace lamium::inventory::transfer {
enum class Gesture { None, StackDrag, OneDrag, OneWheel, StackWheel,
                     MatchingWheel, AllWheel, MatchingClick, AllClick, KeepOneDrag,
                     // L-169, 2026-10-15: a held stack into an empty slot with its
                     // matching items; drops with the drop key held; matching drops.
                     CursorMatchingClick, DropOneDrag, DropKeepOneDrag, DropAllDrag, DropMatching };
// Storage is the upper grid and Player the lower one. The inventory screen
// has no storage: there the main inventory is the upper side and the hotbar
// the lower, so the wheel keeps its direction (2026-10-07).
enum class Side { Player, Storage };
struct GestureOptions {
    bool wheelOne, wheelStack, dragStack, dragOne;
    bool wheelMatching = true, wheelAll = true, clickMatching = true, clickAll = true, dragKeepOne = true;
    bool cursorMatching = true, dropOne = true, dropKeepOne = true, dropAll = true, dropMatching = true;
};
// Left and right gestures can overlap; each cancelled press owns its release.
class ConsumedButtons {
    std::array<bool, 3> buttons{};
    static bool valid(int button) { return button > 0 && button < 3; }
public:
    bool consumed(int button) const { return valid(button) && buttons[static_cast<size_t>(button)]; }
    void press(int button) { if (valid(button)) buttons[static_cast<size_t>(button)] = false; }
    void consume(int button) { if (valid(button)) buttons[static_cast<size_t>(button)] = true; }
    bool release(int button) { bool result = consumed(button); press(button); return result; }
};

inline bool enabled(Gesture gesture, GestureOptions options) {
    switch (gesture) {
    case Gesture::OneWheel: return options.wheelOne;
    case Gesture::StackWheel: return options.wheelStack;
    case Gesture::StackDrag: return options.dragStack;
    case Gesture::OneDrag: return options.dragOne;
    case Gesture::MatchingWheel: return options.wheelMatching;
    case Gesture::AllWheel: return options.wheelAll;
    case Gesture::MatchingClick: return options.clickMatching;
    case Gesture::AllClick: return options.clickAll;
    case Gesture::KeepOneDrag: return options.dragKeepOne;
    case Gesture::CursorMatchingClick: return options.cursorMatching;
    case Gesture::DropOneDrag: return options.dropOne;
    case Gesture::DropKeepOneDrag: return options.dropKeepOne;
    case Gesture::DropAllDrag: return options.dropAll;
    case Gesture::DropMatching: return options.dropMatching;
    default: return false;
    }
}

// With the game's drop key held, the drags drop instead of moving.
inline Gesture dragGesture(int button, bool shift, bool control, bool drop = false) {
    if (drop) {
        if (control && button == 1) return Gesture::DropOneDrag;
        if (shift && button == 1) return Gesture::DropAllDrag;
        if (shift && button == 2) return Gesture::DropKeepOneDrag;
        return Gesture::None;
    }
    if (control && button == 1) return Gesture::OneDrag;
    if (shift && button == 1) return Gesture::StackDrag;
    if (shift && !control && button == 2) return Gesture::KeepOneDrag;
    return Gesture::None;
}

inline Gesture wheelGesture(bool shift, bool control = false) {
    if (control) return shift ? Gesture::AllWheel : Gesture::MatchingWheel;
    return shift ? Gesture::StackWheel : Gesture::OneWheel;
}
// With items on the cursor only Shift + left click (on an empty slot) is Lamium's.
inline Gesture mouseGesture(int button, bool shift, bool control, bool alt, bool drop = false, bool cursor = false) {
    if (cursor) return shift && !control && !alt && !drop && button == 1 ? Gesture::CursorMatchingClick : Gesture::None;
    if (alt) return button == 1 && !drop ? (shift ? Gesture::AllClick : Gesture::MatchingClick) : Gesture::None;
    return dragGesture(button, shift, control, drop);
}
inline bool bulk(Gesture gesture) {
    return gesture == Gesture::MatchingWheel || gesture == Gesture::AllWheel
        || gesture == Gesture::MatchingClick || gesture == Gesture::AllClick;
}
inline bool everything(Gesture gesture) { return gesture == Gesture::AllWheel || gesture == Gesture::AllClick; }
inline bool wheel(Gesture gesture) {
    return gesture == Gesture::OneWheel || gesture == Gesture::StackWheel
        || gesture == Gesture::MatchingWheel || gesture == Gesture::AllWheel;
}
inline bool dropping(Gesture gesture) {
    return gesture == Gesture::DropOneDrag || gesture == Gesture::DropKeepOneDrag || gesture == Gesture::DropAllDrag
        || gesture == Gesture::DropMatching;
}
inline bool dragging(Gesture gesture) {
    return gesture == Gesture::StackDrag || gesture == Gesture::OneDrag || gesture == Gesture::KeepOneDrag
        || gesture == Gesture::DropOneDrag || gesture == Gesture::DropKeepOneDrag || gesture == Gesture::DropAllDrag;
}
inline bool extended(Gesture gesture) {
    return bulk(gesture) || gesture == Gesture::KeepOneDrag || gesture == Gesture::CursorMatchingClick || dropping(gesture);
}

inline Side wheelDestination(int direction) { return direction > 0 ? Side::Storage : Side::Player; }
inline Side otherSide(Side side) { return side == Side::Player ? Side::Storage : Side::Player; }
inline bool canReceive(int count, int maxStackSize) { return count < maxStackSize; }

// The pointer's source stack is a fallback, even if its slot index is high.
inline int chooseSource(std::span<unsigned char const> matches, int hoveredIndex = -1) {
    for (int i = static_cast<int>(matches.size()) - 1; i >= 0; --i)
        if (i != hoveredIndex && matches[static_cast<size_t>(i)]) return i;
    return hoveredIndex >= 0 && hoveredIndex < static_cast<int>(matches.size())
        && matches[static_cast<size_t>(hoveredIndex)] ? hoveredIndex : -1;
}

// A 36-slot inventory collection includes the hotbar at 0-8.
inline bool playerCollection(std::string_view name) {
    return name == "inventory_items" || name == "hotbar_items" || name == "combined_hotbar_and_inventory_items";
}
inline bool excludedCollection(std::string_view name) {
    return name.empty() || name == "cursor_items" || name == "armor_items" || name == "offhand_items"
        || name.starts_with("recipe_") || name.starts_with("creative_") || name.starts_with("preview_")
        || name == "crafting_output_items";
}
inline bool readOnlyCollection(std::string_view name) { return name.ends_with("_output_items"); }
inline bool routeTarget(std::string_view name, Side destination, bool registeredStorage) {
    return destination == Side::Player ? playerCollection(name)
        : registeredStorage && !playerCollection(name) && !excludedCollection(name) && !readOnlyCollection(name);
}
inline bool hotbarSlot(std::string_view name, int index, int inventorySize) {
    return name == "hotbar_items" || (((name == "inventory_items" && inventorySize >= 36)
        || name == "combined_hotbar_and_inventory_items") && index < 9);
}
inline std::optional<Side> collectionSide(std::string_view name, int index, int inventorySize, bool inventoryScreen,
                                         bool registeredStorage = false) {
    if (index < 0 || excludedCollection(name)) return {};
    if (inventoryScreen) {
        if (name == "hotbar_items") return Side::Player;
        if (name == "inventory_items") return inventorySize >= 36 && index < 9 ? Side::Player : Side::Storage;
        if (name == "combined_hotbar_and_inventory_items") return index < 9 ? Side::Player : Side::Storage;
        return {};
    }
    if (playerCollection(name)) return Side::Player;
    if (name == "container_items" || name == "barrel_items" || name == "shulker_box_items") return Side::Storage;
    if (registeredStorage) return Side::Storage;
    return {};
}

struct SweepSource { bool occupied, matches, hotbar; };
// "Everything" moves one section of the player side: the hotbar when a
// hotbar slot is hovered, otherwise the main inventory (maintainer
// 2026-10-15: the hotbar needs the same move, but not together with the rest).
inline std::vector<int> sweepOrder(std::span<SweepSource const> sources, bool all, int hovered = -1) {
    std::vector<int> order;
    bool const hotbar = hovered >= 0 && hovered < static_cast<int>(sources.size())
        && sources[static_cast<size_t>(hovered)].hotbar;
    auto eligible = [&](int i) {
        auto const& source = sources[static_cast<size_t>(i)];
        return source.occupied && (all ? source.hotbar == hotbar : source.matches);
    };
    for (int i = static_cast<int>(sources.size()) - 1; i >= 0; --i)
        if (i != hovered && eligible(i)) order.push_back(i);
    if (hovered >= 0 && hovered < static_cast<int>(sources.size()) && eligible(hovered)) order.push_back(hovered);
    return order;
}
// Prediction is only a candidate state. The glue also requires all owned
// responses and then compares the reacquired slots to that candidate.
// Moves sent together in one UI tick, as one vanilla batch (L-169; a traced
// resource pack sent 44 auto-places in one frame). More wait for the next tick.
inline constexpr size_t batchPerTick = 64;
struct CountChange { bool source; int before, after; bool sameKind = true; };
inline bool conservedMove(std::span<CountChange const> changes, int requested) {
    int taken = 0, placed = 0, sources = 0;
    for (auto const& change : changes) {
        if (change.before < 0 || change.after < 0) return false;
        int delta = change.after - change.before;
        if (!change.sameKind) return false;
        if (change.source) {
            ++sources;
            if (delta >= 0) return false;
            taken -= delta;
        } else {
            if (delta < 0) return false;
            placed += delta;
        }
    }
    return sources == 1 && taken > 0 && taken <= requested && taken == placed;
}
// A drop's prediction: only its source shrinks, by at most the request.
inline bool droppedMove(std::span<CountChange const> changes, int requested) {
    int taken = 0, sources = 0;
    for (auto const& change : changes) {
        if (change.before < 0 || change.after < 0 || !change.sameKind) return false;
        int delta = change.after - change.before;
        if (!change.source) {
            if (delta != 0) return false;
            continue;
        }
        ++sources;
        if (delta >= 0) return false;
        taken -= delta;
    }
    return sources == 1 && taken > 0 && taken <= requested;
}
// The game's ItemTransferAmount as its drop calls pass it (fields unnamed in
// the SDK; traced 2026-10-15 on 1.26.51.01): take type 1, then 2 for one
// item or 0 for the whole stack, then a flag left zero.
inline std::array<unsigned char, 12> dropAmountBytes(bool one) {
    return {1, 0, 0, 0, static_cast<unsigned char>(one ? 2 : 0), 0, 0, 0, 0, 0, 0, 0};
}

// Where an inventory-screen move lands: the first matching stack with room,
// else the first empty slot. Vanilla auto-place there would equip armor.
struct Destination { bool empty = true, matches = false; int count = 0, maxStack = 64; };
inline int chooseDestination(std::span<Destination const> slots) {
    for (size_t i = 0; i < slots.size(); ++i)
        if (!slots[i].empty && slots[i].matches && slots[i].count < slots[i].maxStack) return static_cast<int>(i);
    for (size_t i = 0; i < slots.size(); ++i)
        if (slots[i].empty) return static_cast<int>(i);
    return -1;
}
inline int room(Destination const& slot) { return slot.empty ? slot.maxStack : slot.maxStack - slot.count; }

// In the inventory screen a Shift + left press on something worn (armor,
// elytra, heads, carved pumpkins) goes to vanilla, whose quick move equips
// it, and the held drag then transfers from the next slot on. A drag begun
// elsewhere transfers worn items like any other (maintainer, 2026-10-07).
inline bool vanillaShift(Gesture gesture, bool inventoryScreen, bool worn) {
    return gesture == Gesture::StackDrag && inventoryScreen && worn;
}
inline bool wornItem(std::string_view name, bool armor) {
    if (armor) return true;
    constexpr std::string_view prefix = "minecraft:";
    if (!name.starts_with(prefix)) return false;
    name.remove_prefix(prefix.size());
    return name == "elytra" || name == "carved_pumpkin" || name == "turtle_helmet" || name == "skull"
        || name.ends_with("_head") || name.ends_with("_skull");
}

// Which directions the screen's own routes allow. A side can be registered
// only as a source (a crafting grid gives back to the inventory, nothing is
// sent into it); a gesture needing the missing direction stays vanilla rather
// than taking the press and moving nothing.
struct Routes { bool toStorage = true, toPlayer = true; };
inline bool routed(Gesture gesture, Side hovered, Routes routes, int wheelDirection = 0) {
    auto to = [&](Side destination) { return destination == Side::Storage ? routes.toStorage : routes.toPlayer; };
    if (dropping(gesture)) return true;
    if (gesture == Gesture::CursorMatchingClick) return to(hovered);
    if (wheel(gesture)) {
        Side const destination = wheelDestination(wheelDirection);
        // One item into the hovered stack is placed explicitly, without a route.
        return (gesture == Gesture::OneWheel && hovered == destination) || to(destination);
    }
    return to(otherSide(hovered));
}

inline int amount(Gesture gesture, int count) {
    if (gesture == Gesture::KeepOneDrag || gesture == Gesture::DropKeepOneDrag) return std::max(0, count - 1);
    return gesture == Gesture::OneDrag || gesture == Gesture::OneWheel || gesture == Gesture::DropOneDrag ? 1 : count;
}
}

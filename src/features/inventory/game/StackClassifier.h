#pragma once

#include "features/inventory/sort/SortPlanner.h"

#include "mc/world/item/ItemStack.h"

#include <climits>
#include <map>
#include <utility>
#include <vector>

class CreativeItemRegistry;
class ItemStackBase;

namespace lamium::inventory::game {

/// Turns real item stacks into planner input.
///
/// Mergeability is never decided here: two stacks share a planner group only
/// if vanilla's own `ItemStackBase::isStackable(other)` says the player could
/// stack them through the inventory UI.
///
/// Ordering: a small semantic layer (Shulker Boxes first, then Equipment,
/// Items, Construction, Nature, unknown) on top of the client's Creative
/// registry order, with the item's own state (custom name, enchantments,
/// damage, container contents) deciding the order of variants of one item.
class StackClassifier {
public:
    /// `creativeRegistry` may be null; keys then fall back to identifier order.
    explicit StackClassifier(CreativeItemRegistry const* creativeRegistry) : mCreativeRegistry(creativeRegistry) {}

    /// Classifies `slots` (null stacks become empty slots). Every returned
    /// group id indexes `representatives()`. Item-locked stacks are marked
    /// `locked`; `inventoryLockMovable` says whether a "lock in inventory"
    /// item may still change slots (true for the player's own inventory).
    [[nodiscard]] std::vector<sort::SlotStack> classify(std::vector<ItemStack> const& slots, bool inventoryLockMovable);

    /// One stack per group, in group id order. Used to verify that a slot
    /// still holds the item kind the plan expects.
    [[nodiscard]] std::vector<ItemStack> const& representatives() const { return mRepresentatives; }

    /// Whether the last classify() could consult the Creative registry.
    [[nodiscard]] bool usedCreativeOrder() const { return mUsedCreativeOrder; }

private:
    /// Creative placement of an item: section and ordinal inside it.
    struct Placement {
        sort::Section section{sort::Section::Unknown};
        int           creativeIndex{INT_MAX};
    };

    int           groupOf(ItemStack const& stack);
    sort::SortKey keyOf(int group);
    sort::SortKey buildKey(ItemStackBase const& stack, bool describeContents);
    Placement     placementOf(ItemStackBase const& stack);
    void          describeShulkerBox(ItemStackBase const& stack, sort::SortKey& key, bool describeContents);

    CreativeItemRegistry const*              mCreativeRegistry;
    std::vector<ItemStack>                   mRepresentatives;
    std::map<int, sort::SortKey>             mKeyCache;       ///< group -> key
    std::map<std::pair<int, int>, Placement> mPlacementCache; ///< (id, aux) -> placement
    bool                                     mUsedCreativeOrder{false};
};

} // namespace lamium::inventory::game

#include "features/inventory/game/StackClassifier.h"

#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/deps/nbt/Tag.h"
#include "mc/deps/shared_types/item/CreativeItemCategory.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemLockHelper.h"
#include "mc/world/item/ItemLockMode.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/ShulkerBoxBlockItem.h"
#include "mc/world/item/registry/CreativeGroupInfo.h"
#include "mc/world/item/registry/CreativeItemEntry.h"
#include "mc/world/item/registry/CreativeItemRegistry.h"

#include <climits>
#include <string>
#include <string_view>

namespace lamium::inventory::game {

namespace {

// NBT keys as the game writes them.
constexpr std::string_view kEnchantsKey     = "ench"; // ItemStackBase::TAG_ENCHANTS
constexpr std::string_view kEnchantIdKey    = "id";
constexpr std::string_view kEnchantLevelKey = "lvl";
constexpr std::string_view kItemsKey        = "Items"; // container item contents
// Generic identifier used as the Shulker Box key's `typeName`; the real
// identifier (colour) goes to `tail`.
constexpr std::string_view kShulkerBoxKind = "shulker_box";

// Maps the game's Creative categories onto Lamium's sections. Only these
// four are visible tabs; anything else is treated as unknown.
sort::Section sectionOf(SharedTypes::CreativeItemCategory category) {
    switch (category) {
    case SharedTypes::CreativeItemCategory::Equipment:
        return sort::Section::Equipment;
    case SharedTypes::CreativeItemCategory::Items:
        return sort::Section::Items;
    case SharedTypes::CreativeItemCategory::Construction:
        return sort::Section::Construction;
    case SharedTypes::CreativeItemCategory::Nature:
        return sort::Section::Nature;
    default:
        return sort::Section::Unknown;
    }
}

// A vanilla Shulker Box is an instance of the game's ShulkerBoxBlockItem
// class, whatever its colour. Comparing the item's vtable with the class's
// exported vtable is exact and cheap; an add-on item that merely names itself
// "*_shulker_box" is a different (data-driven) class and does not match.
bool isShulkerBox(ItemStackBase const& stack) {
    auto const item = stack.mItem;
    if (!item) return false;
    return *reinterpret_cast<void** const*>(item.get()) == ShulkerBoxBlockItem::$vftable();
}

// Vanilla item lock: LockInSlot pins the item to its slot. LockInInventory
// only forbids taking the item out of the player's inventory, so it may still
// be moved between inventory slots; inside a container region such an item
// is left alone as well, since vanilla never lets a player put it there.
bool isFixedInPlace(ItemStackBase const& stack, bool inventoryLockMovable) {
    switch (ItemLockHelper::getItemLockMode(stack)) {
    case ItemLockMode::LockInSlot:
        return true;
    case ItemLockMode::LockInInventory:
        return !inventoryLockMovable;
    default:
        return false;
    }
}

// The game's own food flag (Item::isFood). Bedrock's Creative screen files
// most food under Equipment (and some under Nature); a chest reads better
// with food among the items, so this is the one category override.
bool isFood(ItemStackBase const& stack) {
    auto const item = stack.mItem;
    return item && item->isFood();
}

int readInt(CompoundTag const& tag, std::string_view key, int fallback) {
    auto it = tag.mTags.find(key);
    if (it == tag.mTags.end() || !it->second.is_number_integer()) return fallback;
    return static_cast<int>(it->second);
}

ListTag const* findList(CompoundTag const* tag, std::string_view key) {
    if (!tag) return nullptr;
    auto it = tag->mTags.find(key);
    if (it == tag->mTags.end() || !it->second.is_array()) return nullptr;
    return &it->second.get<ListTag>();
}

// Enchantments as stored on the item ("ench" list of {id, lvl}); enchanted
// books use the same list for their stored enchantment.
std::vector<sort::Enchantment> readEnchantments(ItemStackBase const& stack) {
    std::vector<sort::Enchantment> list;
    auto const*                    ench = findList(stack.mUserData.get(), kEnchantsKey);
    if (!ench) return list;
    for (auto const& entryPtr : *ench) {
        if (!entryPtr || entryPtr->getId() != Tag::Type::Compound) continue;
        auto const& entry = entryPtr->as<CompoundTag>();
        int const   id    = readInt(entry, kEnchantIdKey, -1);
        if (id < 0) continue;
        list.push_back(sort::Enchantment{id, readInt(entry, kEnchantLevelKey, 1)});
    }
    return sort::canonicalEnchantments(std::move(list));
}

} // namespace

std::vector<sort::SlotStack> StackClassifier::classify(std::vector<ItemStack> const& slots, bool inventoryLockMovable) {
    mRepresentatives.clear();
    mKeyCache.clear();
    mUsedCreativeOrder = mCreativeRegistry != nullptr;

    std::vector<sort::SlotStack> result;
    result.reserve(slots.size());
    for (auto const& stack : slots) {
        if (stack.isNull() || stack.mCount <= 0) {
            result.push_back(sort::SlotStack::emptySlot());
            continue;
        }
        sort::SlotStack s;
        s.count        = stack.mCount;
        s.maxStackSize = stack.getMaxStackSize();
        s.group        = groupOf(stack);
        // Every member of a group is, by vanilla's verdict, the same kind of
        // item; keying off the group's representative guarantees they share
        // one key even if their user data is encoded slightly differently
        // (e.g. an empty compound versus none).
        s.key    = keyOf(s.group);
        s.locked = isFixedInPlace(stack, inventoryLockMovable);
        result.push_back(std::move(s));
    }
    return result;
}

int StackClassifier::groupOf(ItemStack const& stack) {
    // isStackable(other) is the game's own verdict on whether two stacks may
    // be combined: same item, aux value, user data (enchantments, names,
    // damage, container contents, ...) and block, and both stackable at all.
    // Unstackable items therefore never join a group, not even with an
    // identical copy of themselves.
    for (size_t i = 0; i < mRepresentatives.size(); ++i) {
        if (stack.isStackable(mRepresentatives[i])) {
            return static_cast<int>(i);
        }
    }
    mRepresentatives.push_back(stack);
    return static_cast<int>(mRepresentatives.size() - 1);
}

sort::SortKey StackClassifier::keyOf(int group) {
    if (auto it = mKeyCache.find(group); it != mKeyCache.end()) return it->second;
    auto const key = buildKey(mRepresentatives[static_cast<size_t>(group)], true);
    mKeyCache.emplace(group, key);
    return key;
}

sort::SortKey StackClassifier::buildKey(ItemStackBase const& stack, bool describeContents) {
    sort::SortKey key;

    auto const placement = placementOf(stack);
    key.section          = placement.section;
    key.creativeIndex    = placement.creativeIndex;
    key.typeName         = stack.getTypeName();
    key.aux              = stack.getAuxValue();

    // A custom name is explicit player intent: named variants lead and are
    // ordered by their visible name.
    key.name     = sort::normalizeName(stack.getCustomName());
    key.nameRank = key.name.empty() ? 1 : 0;

    // Enchanted variants lead the plain ones and group by enchantment
    // identity (id order), then level; damage (worse condition) comes last.
    key.enchantments = readEnchantments(stack);
    key.variantRank  = key.enchantments.empty() ? 1 : 0;
    key.damage       = stack.getDamageValue();

    if (isShulkerBox(stack)) {
        describeShulkerBox(stack, key, describeContents);
    }

    // Everything else that keeps stacks apart (lore, other components, the
    // adventure-mode restrictions that live outside the NBT compound) still
    // needs a deterministic order; these hashes are the last resort only.
    if (stack.mUserData && !stack.mUserData->mTags.empty()) {
        key.detail = std::to_string(stack.mUserData->hash());
    }
    if (stack.mCanPlaceOnHash != 0 || stack.mCanDestroyHash != 0) {
        key.detail += "|p" + std::to_string(stack.mCanPlaceOnHash) + "|d" + std::to_string(stack.mCanDestroyHash);
    }
    return key;
}

// Shulker Boxes form the leading section: filled boxes before empty ones,
// then custom name, then a signature of the contents (independent of the
// internal slot layout), then colour. The contents are read-only input.
//
// Inner items get the same key an inventory item would get, so a box with
// an enchanted, named or worn tool differs from one with a plain tool. The
// description is bounded to one level: vanilla never nests container items,
// so an inner item's own contents are not expanded.
void StackClassifier::describeShulkerBox(ItemStackBase const& stack, sort::SortKey& key, bool describeContents) {
    key.section     = sort::Section::ShulkerBox;
    key.tail        = key.typeName; // colour / variant decides late
    key.typeName    = std::string(kShulkerBoxKind);
    key.aux         = 0;
    key.variantRank = 0;
    key.enchantments.clear();
    key.damage = 0;

    std::vector<sort::ContentEntry> entries;
    auto const*                     items = describeContents ? findList(stack.mUserData.get(), kItemsKey) : nullptr;
    if (items) {
        for (auto const& entryPtr : *items) {
            if (!entryPtr || entryPtr->getId() != Tag::Type::Compound) continue;
            // fromTag resolves the item through the client's registry; a
            // malformed entry must not take the whole signature down.
            try {
                ItemStack inner = ItemStack::fromTag(entryPtr->as<CompoundTag>());
                if (inner.isNull() || inner.mCount <= 0) continue;
                entries.push_back(sort::ContentEntry{buildKey(inner, false), static_cast<int>(inner.mCount)});
            } catch (...) {
                sort::SortKey unknown;
                unknown.typeName = "?";
                entries.push_back(sort::ContentEntry{unknown, 1});
            }
        }
    }
    key.contents      = sort::contentSignature(std::move(entries));
    key.creativeIndex = key.contents.empty() ? 1 : 0; // filled boxes first
}

StackClassifier::Placement StackClassifier::placementOf(ItemStackBase const& stack) {
    if (!mCreativeRegistry) return {};

    auto const id       = static_cast<int>(stack.getId());
    auto const aux      = static_cast<int>(stack.getAuxValue());
    auto const cacheKey = std::make_pair(id, aux);
    if (auto it = mPlacementCache.find(cacheKey); it != mPlacementCache.end()) {
        return it->second;
    }

    // The ordinal preserves the Creative screen's visual order inside a
    // section: group by group, then entry by entry. Prefer the entry with the
    // same aux value (potion variants and the like), otherwise the item's
    // first entry; stacks that differ only in user data then share a
    // placement and are ordered by their own state.
    constexpr int kGroupSpan = 10000; // entries per group, far above reality
    auto const&   groups     = mCreativeRegistry->mCreativeGroups.get();
    Placement     found;
    Placement     idOnly;
    bool          haveIdOnly = false;
    for (auto const& entry : mCreativeRegistry->mCreativeItems.get()) {
        auto const& item = entry.mItemInstance.get();
        if (item.getId() != id) continue;
        Placement p;
        if (entry.mGroupIndex < groups.size()) {
            p.section = sectionOf(groups[entry.mGroupIndex].mCategory);
        }
        p.creativeIndex = static_cast<int>(entry.mGroupIndex) * kGroupSpan + static_cast<int>(entry.mIndex);
        if (item.getAuxValue() == aux) {
            found = p;
            break;
        }
        if (!haveIdOnly) {
            idOnly     = p;
            haveIdOnly = true;
        }
    }
    if (found.creativeIndex == INT_MAX && haveIdOnly) found = idOnly;
    if (found.section != sort::Section::Unknown && isFood(stack)) {
        found.section = sort::Section::Items;
    }
    mPlacementCache.emplace(cacheKey, found);
    return found;
}

} // namespace lamium::inventory::game

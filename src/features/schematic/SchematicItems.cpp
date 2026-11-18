#include "features/recipes/ItemKey.h"
#include "features/schematic/SchematicItems.h"
#include "features/schematic/Nbt.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/deps/nbt/Tag.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/ItemStack.h"
#include <algorithm>
#include <atomic>

namespace lamium::schematic::items {
namespace {
std::map<std::string, ItemStack> stacks;
// Bumped from the exit event; the drawing thread clears on its next use.
std::atomic<unsigned> generation{0};
unsigned seen = 0;
std::map<std::string, Shown> shownItems;
unsigned seenShown = 0;
}
void forget() { ++generation; }
ItemStack const* iconStack(std::string const& icon) {
    if (unsigned now = generation.load(); now != seen) {
        stacks.clear();
        seen = now;
    }
    if (icon.empty()) return nullptr;
    auto found = stacks.find(icon);
    if (found == stacks.end()) {
        // Never emptied within a world: a caller draws from pointers it took earlier in the frame, and the
        // item list alone shows a couple of thousand items over its pages (L-359).
        if (stacks.size() > 16384) return nullptr;
        ItemStack stack;
        if (auto tag = CompoundTag::fromBinaryNbt(icon)) {
            try {
                stack = ItemStack::fromTag(*tag);
                // A decoded stack counts as just picked up: no pickup squash.
                stack.mShowPickUp = false;
                stack.mWasPickedUp = false;
            } catch (...) {}
        }
        found = stacks.emplace(icon, std::move(stack)).first;
    }
    return found->second.isNull() ? nullptr : &found->second;
}
Shown const& shown(std::string const& item) {
    if (unsigned now = generation.load(); now != seenShown) {
        shownItems.clear();
        seenShown = now;
    }
    if (auto found = shownItems.find(item); found != shownItems.end()) return found->second;
    if (shownItems.size() > 16384) shownItems.clear();
    Shown out{item, {}, 64};
    // A variant's key carries its aux ("minecraft:bed@14", recipes/ItemKey.h).
    auto const parts = recipes::splitItemKey(item);
    nbt::Root tag;
    tag.compound.set("Name", {std::string(parts.name)});
    tag.compound.set("Count", {std::int8_t{1}});
    tag.compound.set("Damage", {static_cast<std::int16_t>(parts.aux == recipes::anyAux ? 0 : parts.aux)});
    std::string icon = nbt::write(tag);
    if (auto const* stack = iconStack(icon)) {
        try {
            out.name = stack->getName();
            out.stack = std::max(1, static_cast<int>(stack->getMaxStackSize()));
            out.icon = std::move(icon);
        } catch (...) { out.name = item; }
    }
    return shownItems.emplace(item, std::move(out)).first->second;
}
std::map<std::string, std::uint64_t> carried(LocalPlayer& player) {
    std::map<std::string, std::uint64_t> out;
    for (auto const& stack : carriedStacks(player)) out[stack.name] += stack.count;
    return out;
}
void addStack(ItemStack const& stack, std::vector<Carried>& out) {
    if (stack.isNull()) return;
    out.push_back({stack.getTypeName(), stack.getAuxValue(), static_cast<std::uint64_t>(stack.mCount)});
    if (!stack.getTypeName().ends_with("shulker_box")) return;
    auto const* data = stack.mUserData.get();
    if (!data) return;
    auto items = data->mTags.find("Items");
    if (items == data->mTags.end() || !items->second.is_array()) return;
    for (auto const& entry : items->second.get<ListTag>()) {
        if (!entry || entry->getId() != Tag::Type::Compound) continue;
        try {
            auto inner = ItemStack::fromTag(entry->as<CompoundTag>());
            if (!inner.isNull()) out.push_back({inner.getTypeName(), inner.getAuxValue(), static_cast<std::uint64_t>(inner.mCount)});
        } catch (...) {}
    }
}
std::vector<Carried> carriedStacks(LocalPlayer& player) {
    std::vector<Carried> out;
    auto& inventory = player.getInventory();
    for (int slot = 0; slot < inventory.getContainerSize(); ++slot) addStack(inventory.getItem(slot), out);
    return out;
}
}

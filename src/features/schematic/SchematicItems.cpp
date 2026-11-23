#include "features/recipes/ItemKey.h"
#include "features/schematic/SchematicItems.h"
#include "app/Versions.h"
#include "features/inspection/preview/BundleContents.h"
#include "features/schematic/Nbt.h"
#include "mc/client/gui/screens/controllers/BundleHelper.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/deps/nbt/IntTag.h"
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
    // Full: worked out for this call only, never clearing what callers still hold (as `stacks` does).
    static Shown spare;
    bool const full = shownItems.size() > 16384;
    Shown out{item, {}, 64};
    // A variant's key carries its aux ("minecraft:bed@14", recipes/ItemKey.h).
    auto const parts = recipes::splitItemKey(item);
    nbt::Root tag;
    tag.compound.set("Name", {std::string(parts.name)});
    tag.compound.set("Count", {std::int8_t{1}});
    tag.compound.set("Damage", {static_cast<std::int16_t>(parts.aux == recipes::anyAux ? 0 : parts.aux)});
    std::string icon = nbt::write(tag);
    // An item told apart by its data (L-363) gets it as the stack's user data, where reading item data is confirmed
    // for this version; data the game cannot read leaves the stack bare.
    if (!parts.data.empty() && versionSensitiveAllowed(capabilities::Id::ItemDetails))
        if (auto data = CompoundTag::fromSnbt(parts.data))
            if (auto stackTag = CompoundTag::fromBinaryNbt(icon)) {
                stackTag->putCompound("tag", std::move(*data));
                icon = stackTag->toBinaryNbt();
            }
    if (auto const* stack = iconStack(icon)) {
        try {
            out.name = stack->getName();
            out.stack = std::max(1, static_cast<int>(stack->getMaxStackSize()));
            out.icon = std::move(icon);
        } catch (...) { out.name = item; }
    }
    if (full) return spare = std::move(out);
    return shownItems.emplace(item, std::move(out)).first->second;
}
std::map<std::string, std::uint64_t> carried(LocalPlayer& player) {
    std::map<std::string, std::uint64_t> out;
    for (auto const& stack : carriedStacks(player)) out[stack.name] += stack.count;
    return out;
}
namespace {
// What each bundle held when an item screen last read it, by the bundle's id; dropped with the world.
std::map<int, std::vector<Carried>> bundles;
unsigned seenBundles = 0;
void addBundle(ItemStack const& stack, std::vector<Carried>& out, ContainerScreenController const* screen) {
    if (unsigned now = generation.load(); now != seenBundles) bundles.clear(), seenBundles = now;
    std::optional<int> id;
    if (auto const* data = stack.mUserData.get())
        if (auto found = data->mTags.find("bundle_id"); found != data->mTags.end())
            if (auto const* value = std::get_if<IntTag>(&found->second.mTagStorage)) id = value->data;
    if (screen) {
        std::vector<Carried> held;
        for (int index = 0; index < inspection::preview::BundleGrid::kMaxSlots; ++index) {
            ItemStack const& inner = BundleHelper::getItemStackFromBundle(*screen, stack, index);
            if (!inner.isNull()) held.push_back({inner.getTypeName(), inner.getAuxValue(), static_cast<std::uint64_t>(inner.mCount)});
        }
        out.insert(out.end(), held.begin(), held.end());
        if (id && (bundles.size() < 1024 || bundles.contains(*id))) bundles[*id] = std::move(held);
    } else if (id) {
        if (auto found = bundles.find(*id); found != bundles.end()) out.insert(out.end(), found->second.begin(), found->second.end());
    }
}
}
void addStack(ItemStack const& stack, std::vector<Carried>& out, ContainerScreenController const* screen) {
    if (stack.isNull()) return;
    std::string const name = stack.getTypeName();
    out.push_back({name, stack.getAuxValue(), static_cast<std::uint64_t>(stack.mCount)});
    if (inspection::preview::isBundleTypeName(name)) {
        try { addBundle(stack, out, screen); } catch (...) {}
        return;
    }
    if (!name.ends_with("shulker_box")) return;
    auto const* data = stack.mUserData.get();
    if (!data) return;
    auto items = data->mTags.find("Items");
    if (items == data->mTags.end() || !items->second.is_array()) return;
    for (auto const& entry : items->second.get<ListTag>()) {
        if (!entry || entry->getId() != Tag::Type::Compound) continue;
        try {
            auto inner = ItemStack::fromTag(entry->as<CompoundTag>());
            // A bundle in the box counts with what it holds, as one carried does.
            addStack(inner, out, screen);
        } catch (...) {}
    }
}
std::vector<Carried> carriedStacks(LocalPlayer& player, ContainerScreenController const* screen) {
    std::vector<Carried> out;
    auto& inventory = player.getInventory();
    for (int slot = 0; slot < inventory.getContainerSize(); ++slot) addStack(inventory.getItem(slot), out, screen);
    return out;
}
}

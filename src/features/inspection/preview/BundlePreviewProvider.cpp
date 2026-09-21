#include "features/inspection/preview/BundlePreviewProvider.h"

#include "mc/client/gui/screens/controllers/BundleHelper.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/deps/nbt/Tag.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/ItemStackBase.h"

#include "app/Runtime.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace lamium::inspection::preview {

namespace {

constexpr std::string_view kItemsKey = "Items";
constexpr std::string_view kSlotKey  = "Slot";

// Reads the container slot index of one stored item entry, or -1 if absent or
// not an integer. The game writes it as a byte, but be lenient about the type.
int readSlotIndex(CompoundTag const& entry) {
    auto it = entry.mTags.find(kSlotKey);
    if (it == entry.mTags.end() || !it->second.is_number_integer()) {
        return -1;
    }
    return static_cast<int>(it->second);
}

#ifdef LAMIUM_TRACE
// Logs the Bundle's raw NBT key names once per distinct key-set so a trace
// build can confirm (or correct) the storage-key assumption in-game: hover a
// real Bundle and read the `Bundle NBT keys:` line in trace.log. Key names
// only — never item data.
void logBundleNbtKeys(CompoundTag const& userData) {
    static std::string sLastKeys;
    std::string        keys;
    for (auto const& [name, _] : userData.mTags) {
        if (!keys.empty()) {
            keys += ',';
        }
        keys += name;
    }
    if (keys != sLastKeys) {
        sLastKeys = keys;
        Runtime::instance().self().getLogger().debug("Bundle NBT keys: [{}]", keys);
    }
}
#endif

} // namespace

bool BundlePreviewProvider::supports(ItemStackBase const& item) const {
    if (item.isNull()) {
        return false;
    }
    return isBundleTypeName(item.getTypeName());
}

std::optional<ContainerPreview>
BundlePreviewProvider::extract(ItemStackBase const& item, ContainerScreenController const* controller) const {
    if (!supports(item)) {
        return std::nullopt;
    }

    struct DecodedEntry {
        int       slot;
        ItemStack stack;
    };
    std::vector<DecodedEntry> decoded;
    int                       skipped = 0;
#ifdef LAMIUM_TRACE
    char const* source = "none";
#endif

    // Authoritative path (confirmed in-game on 1.26.51 / LeviLamina 26.51.3):
    // the hovered Bundle's own NBT holds only a `bundle_id` reference; the
    // contents live in the client's dynamic container for that id, which the
    // game's own Bundle UI reads through BundleHelper::getItemStackFromBundle
    // with the hovered slot's ContainerScreenController. Walk every container index
    // (Bedrock storage items hold up to 64) and keep the non-null stacks; the
    // helper yields the empty stack for unused or out-of-range indices.
    if (controller) {
#ifdef LAMIUM_TRACE
        source = "dynamic-container";
#endif
        for (int index = 0; index < BundleGrid::kMaxSlots; ++index) {
            ItemStack const& stack = BundleHelper::getItemStackFromBundle(*controller, item, index);
            if (stack.isNull()) {
                continue;
            }
            decoded.push_back(DecodedEntry{index, ItemStack(stack)});
        }
    }

    // Fallback: a Bundle whose contents were flattened into its own NBT as an
    // `Items` list (entries keyed by `Slot`, the Shulker/box-entity save
    // format). Only used when the live container path yielded nothing.
    auto const* userData = item.mUserData.get();
    if (decoded.empty() && userData) {
#ifdef LAMIUM_TRACE
        logBundleNbtKeys(*userData);
#endif
        auto itemsIt = userData->mTags.find(kItemsKey);
        if (itemsIt != userData->mTags.end() && itemsIt->second.is_array()) {
#ifdef LAMIUM_TRACE
            source = "nbt-items";
#endif
            for (auto const& entryPtr : itemsIt->second.get<ListTag>()) {
                if (!entryPtr || entryPtr->getId() != Tag::Type::Compound) {
                    ++skipped;
                    continue;
                }
                auto const& entry = entryPtr->as<CompoundTag>();

                int const slot = readSlotIndex(entry);
                if (slot < 0 || slot >= BundleGrid::kMaxSlots) {
                    ++skipped;
                    continue;
                }

                // fromTag resolves the item by name through the client's item
                // registry and yields a null stack for unknown or malformed
                // entries. Allowed here: extract runs only on cache-key
                // change, never per frame. A single entry that throws must
                // not take the rest of the Bundle with it.
                try {
                    ItemStack stack = ItemStack::fromTag(entry);
                    if (stack.isNull()) {
                        ++skipped;
                        continue;
                    }
                    decoded.push_back(DecodedEntry{slot, std::move(stack)});
                } catch (...) {
                    ++skipped;
                }
            }
        }
    }

    if (decoded.empty()) {
        // Empty Bundle (or nothing decodable): report the minimal 3x1 frame so
        // `bundle.showEmpty` has something to draw, and let the render layer
        // decide (it skips empty grids unless asked).
        auto preview             = ContainerPreview::empty(BundleGrid::kEmptyColumns, BundleGrid::kEmptyRows);
        preview.family           = ContainerPreview::Family::Bundle;
        preview.skippedSlotCount = skipped;
#ifdef LAMIUM_TRACE
        Runtime::instance().self().getLogger().debug(
            "Bundle extract: source={} entries=0 skipped={} grid=3x1",
            source,
            skipped
        );
#endif
        return preview;
    }

    // Pack in slot order into the dynamic grid. Sorting by stored slot keeps
    // insertion-adjacent items adjacent on screen, and compacting drops the
    // sparse gaps a fixed grid would draw as holes. Duplicate Slot values in
    // the NBT fallback can push decoded past kMaxSlots even though each index
    // is in range, so truncate to the cap (folding the tail into skipped)
    // BEFORE sizing the grid: shapeFor clamps, but the pack loop must never
    // write more entries than the grid holds.
    std::sort(decoded.begin(), decoded.end(), [](DecodedEntry const& a, DecodedEntry const& b) {
        return a.slot < b.slot;
    });
    if (static_cast<int>(decoded.size()) > BundleGrid::kMaxSlots) {
        skipped += static_cast<int>(decoded.size()) - BundleGrid::kMaxSlots;
        decoded.resize(static_cast<size_t>(BundleGrid::kMaxSlots));
    }
    BundleGrid const grid   = BundleGrid::shapeFor(static_cast<int>(decoded.size()));
    auto             preview = ContainerPreview::empty(grid.columns, grid.rows);
    preview.family           = ContainerPreview::Family::Bundle;
    assert(preview.slots.size() >= decoded.size());
    for (size_t i = 0; i < decoded.size(); ++i) {
        preview.slots[i] = std::move(decoded[i].stack);
    }
    preview.skippedSlotCount = skipped;
#ifdef LAMIUM_TRACE
    Runtime::instance().self().getLogger().debug(
        "Bundle extract: source={} entries={} skipped={} grid={}x{}",
        source,
        decoded.size(),
        skipped,
        grid.columns,
        grid.rows
    );
#endif
    return preview;
}

} // namespace lamium::inspection::preview


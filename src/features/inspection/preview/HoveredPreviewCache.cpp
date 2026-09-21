#include "features/inspection/preview/HoveredPreviewCache.h"

#include "app/Runtime.h"
#include "features/inspection/hover/HoverTracker.h"

#include "mc/client/gui/screens/controllers/BundleHelper.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/deps/nbt/Tag.h"
#include "mc/world/item/ItemStackBase.h"

namespace lamium::inspection::preview {

namespace {
// Fingerprints the Bundle-relevant content bytes of `item` WITHOUT touching
// the item registry: each `Items` entry contributes its stored `Slot` index
// (read straight off the entry map), its tag `Type` id and its `Tag::hash()`
// — no `ItemStack::fromTag`, no name resolution, no stack reconstruction.
// O(entries) map/hash work only; the ban is on registry work, not iterating.
// List order, entry count and the Bundle stack's own id/aux/count are folded
// in, so insert/remove/reorder/content-mutation at a stable user-data
// pointer all change the key. Null and non-Compound elements mix sentinels
// (extract counts them as skipped). Returns the tail mix over a zero seed
// when the item carries no Bundle content list (empty Bundle): stable across
// identical empties. Shulker Boxes skip this (their static contents change
// only with a new user-data object, caught by the pointer key).
uint64_t fingerprintBundleContent(ItemStackBase const& item, ContainerScreenController const* controller) {
    auto const* userData = item.mUserData.get();
    uint64_t    fingerprint = 0;
    uint64_t    entryCount  = 0;
    if (controller) {
        // Live contents (the confirmed 26.51.3 data path, see
        // BundlePreviewProvider): index/id/aux/count of every non-null stack
        // the game's own Bundle UI would read. Reference access only — no
        // stack copies, no registry work.
        for (int index = 0; index < BundleGrid::kMaxSlots; ++index) {
            ItemStack const& stack = BundleHelper::getItemStackFromBundle(*controller, item, index);
            if (stack.isNull()) {
                continue;
            }
            ++entryCount;
            fingerprint = fingerprintBundleLiveEntry(fingerprint, index, stack.getId(), stack.mAuxValue, stack.mCount);
        }
    }
    if (entryCount == 0 && userData) {
        auto itemsIt = userData->mTags.find("Items");
        if (itemsIt != userData->mTags.end() && itemsIt->second.is_array()) {
            for (auto const& entryPtr : itemsIt->second.get<ListTag>()) {
                ++entryCount;
                if (!entryPtr) {
                    fingerprint = fingerprintBundleEntry(
                        fingerprint,
                        kBundleFingerprintNoSlot,
                        kBundleFingerprintNullKind,
                        kBundleFingerprintNullHash
                    );
                    continue;
                }
                uint32_t const kind = static_cast<uint32_t>(entryPtr->getId());
                uint64_t     entryHash = 0;
                try {
                    entryHash = entryPtr->hash();
                } catch (...) {
                    // A throwing hash must still invalidate: fall back to a
                    // sentinel no real hash mix is likely to collide with.
                    entryHash = kBundleFingerprintNullHash;
                }
                int slot = kBundleFingerprintNoSlot;
                if (entryPtr->getId() == Tag::Type::Compound) {
                    auto const& entry = entryPtr->as<CompoundTag>();
                    slot              = -1;
                    if (auto slotIt = entry.mTags.find("Slot");
                        slotIt != entry.mTags.end() && slotIt->second.is_number_integer()) {
                        slot = static_cast<int>(slotIt->second);
                    }
                }
                fingerprint = fingerprintBundleEntry(fingerprint, slot, kind, entryHash);
            }
        }
    }
    // Fold the Bundle stack's own identity plus the entry count so a
    // swapped-in Bundle with identical contents still re-extracts, and a
    // grown/shrunk list changes the key even if surviving entries hash equal.
    fingerprint =
        fingerprintBundleFinal(fingerprint, item.getId(), item.mAuxValue, item.mCount, entryCount);
    return fingerprint;
}

} // namespace

HoveredPreviewCache::Key
HoveredPreviewCache::makeKey(ItemStackBase const& item, ContainerScreenController const* controller) {
    Key key;
    key.stack    = &item;
    key.userData = item.mUserData.get();
    key.id       = item.getId();
    key.aux      = item.mAuxValue;
    key.count    = item.mCount;
    // Fingerprinting every hovered item every frame would hash NBT on the
    // hot path; only Bundles mutate in place, so only they pay for it. The
    // fingerprint itself never resolves items (slot/tag-kind/tag-hash only).
    // The predicate is the production one (no duplication).
    if (isBundleTypeName(item.getTypeName())) {
        try {
            key.contentFingerprint = fingerprintBundleContent(item, controller);
        } catch (...) {
            key.contentFingerprint = 0;
        }
    }
    return key;
}

ContainerPreview const* HoveredPreviewCache::resolve(ScreenController const& controller) {
    ItemStackBase const* item = hover::HoverTracker::getInstance().resolveItem(controller);
    if (!item || item->isNull()) {
        clear();
        return nullptr;
    }

    // The hovered slot's own controller (validated by resolveItem to be the
    // one being rendered) gives Bundle providers access to the live contents.
    auto const& hovered = hover::HoverTracker::getInstance().current();
    ContainerScreenController const* container = hovered ? hovered->controller : nullptr;

    Key const key = makeKey(*item, container);
    if (!mKey || *mKey != key) {
        mKey     = key;
        mPreview = extract(*item, container);
        if (!mPreview) {
            Runtime::instance().self().getLogger().debug(
                "Hovered '{}' x{} (userData: {}) - not previewable",
                item->getTypeName(),
                item->mCount,
                item->mUserData ? "yes" : "no"
            );
        }
        if (mPreview) {
            Runtime::instance().self().getLogger().debug(
                "Preview for '{}': {}/{} slots filled",
                item->getTypeName(),
                mPreview->filledSlotCount(),
                mPreview->slotCount()
            );
            if (mPreview->skippedSlotCount > 0) {
                Runtime::instance().self().getLogger().warn(
                    "Preview for '{}': {} slot(s) could not be decoded and were left empty",
                    item->getTypeName(),
                    mPreview->skippedSlotCount
                );
            }
        }
    }
    return mPreview ? &*mPreview : nullptr;
}

void HoveredPreviewCache::clear() {
    mKey.reset();
    mPreview.reset();
}

std::optional<ContainerPreview>
HoveredPreviewCache::extract(ItemStackBase const& item, ContainerScreenController const* controller) {
    for (auto const* provider : mProviders) {
        if (!provider->supports(item)) {
            continue;
        }
        try {
            return provider->extract(item, controller);
        } catch (...) {
            // Malformed item data must never take the game down. Report once
            // and treat the item as not previewable.
            if (!mWarnedExtractionFailure) {
                mWarnedExtractionFailure = true;
                Runtime::instance().self().getLogger().warn(
                    "Failed to extract preview data from hovered item '{}'; further failures are silent",
                    item.getTypeName()
                );
            }
            return std::nullopt;
        }
    }
    return std::nullopt;
}

} // namespace lamium::inspection::preview


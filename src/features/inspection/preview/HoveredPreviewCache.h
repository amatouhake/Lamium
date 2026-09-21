#pragma once

#include "features/inspection/preview/BundleContents.h"
#include "features/inspection/preview/BundlePreviewProvider.h"
#include "features/inspection/preview/ContainerPreview.h"
#include "features/inspection/preview/ShulkerPreviewProvider.h"
#include <array>
#include <cstdint>
#include <optional>

class CompoundTag;
class ContainerScreenController;
class ItemStackBase;
class ScreenController;

namespace lamium::inspection::preview {

/// Resolves the preview for whatever item is hovered in a container screen,
/// re-extracting only when the hovered item changes.
///
/// This is the glue between the hover tracker and the providers; it knows
/// nothing about rendering.
class HoveredPreviewCache {
public:
    /// Returns the preview for the item hovered in `controller`, or nullptr
    /// when nothing previewable is hovered. The pointer stays valid until the
    /// next call. `controller` must be alive (owned by the view being rendered).
    [[nodiscard]] ContainerPreview const* resolve(ScreenController const& controller);

    void clear();
private:
    // Identity of the item the cached preview was extracted from. Pointer
    // fields catch a different stack in the slot; the content fingerprint
    // catches in-place NBT mutation at stable addresses (Bundle insert/remove
    // rewrites the same user-data object, so pointer comparison alone would go
    // stale). `id`/`aux`/`count` are folded into the fingerprint for Bundles;
    // the Shulker path keeps its cheap pointer fast-path via the same key.
    struct Key {
        ItemStackBase const* stack{nullptr};
        CompoundTag const*   userData{nullptr};
        short                id{0};
        short                aux{0};
        unsigned char        count{0};
        uint64_t             contentFingerprint{0};

        bool operator==(Key const&) const = default;
    };

    [[nodiscard]] Key makeKey(ItemStackBase const& item, ContainerScreenController const* controller);

    [[nodiscard]] std::optional<ContainerPreview>
    extract(ItemStackBase const& item, ContainerScreenController const* controller);

    ShulkerPreviewProvider                mShulkerProvider;
    BundlePreviewProvider                 mBundleProvider;
    std::array<PreviewProvider const*, 2> mProviders{&mShulkerProvider, &mBundleProvider};
    std::optional<Key>                    mKey;
    std::optional<ContainerPreview>       mPreview;
    bool                                  mWarnedExtractionFailure{false};
};

} // namespace lamium::inspection::preview


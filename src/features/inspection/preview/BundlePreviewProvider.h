#pragma once

#include "features/inspection/preview/BundleContents.h"
#include "features/inspection/preview/PreviewProvider.h"

namespace lamium::inspection::preview {

/// Preview provider for Bundles of every colour (including undyed).
///
/// Data path (26.51.3, confirmed in-game with the trace build): the hovered
/// Bundle's own NBT carries only a `bundle_id` reference (`Bundle NBT keys:
/// [bundle_id]`); the contents live in the client's dynamic container for
/// that id, which the game's Bundle UI reads through
/// `BundleHelper::getItemStackFromBundle(controller, bundleItem, index)`
/// with the hovered slot's ContainerScreenController. `extract` walks every
/// container index (Bedrock storage items hold up to 64, `max_slots: 64` in
/// Mojang's `behavior_pack/items/bundle.json`) and keeps the non-null
/// stacks, so the vanilla tooltip's `num_viewable_slots` (12) never limits
/// the preview. The hover-cache fingerprint walks the same indices without
/// decoding anything, so in-place insert/remove re-extracts immediately.
///
/// Fallback: when no container screen is available or the live path yields
/// nothing, a Bundle whose contents were flattened into its NBT as an `Items`
/// list (entries keyed by `Slot`, the Shulker/box-entity save format) is
/// decoded with `ItemStack::fromTag` — only in `extract`, never per frame.
///
/// Why not the dynamic-container registry directly: see
/// `docs/bundle-link-probe.md` — `StorageItemUtility::getStorageItemID` /
/// `ContainerManagerController::getDynamicContainerModel` are client-only
/// declarations without a proven client link; `BundleHelper` is a client
/// class that links and is exactly what vanilla uses.
///
/// Trace build (`--trace=y`): logs `Bundle extract: source=<dynamic-container
/// | nbt-items | none> entries=N skipped=M grid=CxR` per extraction and the
/// raw NBT key set when the fallback runs.
class BundlePreviewProvider final : public PreviewProvider {
public:
    [[nodiscard]] bool supports(ItemStackBase const& item) const override;

    [[nodiscard]] std::optional<ContainerPreview>
    extract(ItemStackBase const& item, ContainerScreenController const* controller) const override;
};

} // namespace lamium::inspection::preview


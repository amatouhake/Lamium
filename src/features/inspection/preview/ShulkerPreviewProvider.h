#pragma once

#include "features/inspection/preview/PreviewProvider.h"

namespace lamium::inspection::preview {

/// Preview provider for Shulker Boxes of every colour (including undyed).
///
/// Bedrock stores a Shulker Box's contents in the item's user data (NBT), as
/// the block entity would save them:
///
///   { "Items": [ { "Slot": 0b, "Count": 1b, "Name": "minecraft:stone",
///                  "Damage": 0s, "tag": {...}, "Block": {...} }, ... ] }
///
/// Only slots present in the list are stored, so gaps are empty slots.
class ShulkerPreviewProvider final : public PreviewProvider {
public:
    static constexpr int kColumns = 9;
    static constexpr int kRows    = 3;

    [[nodiscard]] bool supports(ItemStackBase const& item) const override;

    [[nodiscard]] std::optional<ContainerPreview>
    extract(ItemStackBase const& item, ContainerScreenController const* controller) const override;
};

} // namespace lamium::inspection::preview


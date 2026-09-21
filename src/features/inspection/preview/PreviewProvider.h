#pragma once

#include "features/inspection/preview/ContainerPreview.h"

#include <optional>

class ContainerScreenController;
class ItemStackBase;

namespace lamium::inspection::preview {

/// Extraction boundary between "what item is hovered" and "how it is drawn".
///
/// A provider recognises one family of items (Shulker Boxes today, Bundles in
/// the future) and knows how to turn that item's data into a ContainerPreview.
/// Providers are read-only and must tolerate malformed or missing item data:
/// `extract` returns std::nullopt rather than throwing.
class PreviewProvider {
public:
    virtual ~PreviewProvider() = default;

    /// Cheap check used every frame; must not parse item data.
    [[nodiscard]] virtual bool supports(ItemStackBase const& item) const = 0;

    /// Builds the preview for a supported item. Returns std::nullopt if the
    /// item is not supported or its data cannot be interpreted safely.
    /// `controller` is the container screen the item is hovered in (may be
    /// null); providers whose contents live outside the item's own data
    /// (Bundles) resolve them through it.
    [[nodiscard]] virtual std::optional<ContainerPreview>
    extract(ItemStackBase const& item, ContainerScreenController const* controller) const = 0;
};

} // namespace lamium::inspection::preview


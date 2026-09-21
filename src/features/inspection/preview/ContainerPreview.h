#pragma once

#include "mc/world/item/ItemStack.h"

#include <vector>

namespace lamium::inspection::preview {

/// The data needed to draw a preview of a container-like item: a grid of
/// slots, each holding the item stored there (or a null stack when empty).
///
/// Shulker Boxes mirror the real container, so slot `row * columns + col` is
/// rendered at that grid position including gaps; Bundles pack their entries
/// in slot order into a dynamic grid instead.
struct ContainerPreview {
    /// Which item family the preview was extracted from, stamped by the
    /// extracting provider. Grid shape alone cannot identify the family, so
    /// the render layer gates per-family config on this field.
    enum class Family { Shulker, Bundle };

    Family                 family{Family::Shulker};
    int                    columns{0};
    int                    rows{0};
    std::vector<ItemStack> slots;               // size == rows * columns; null stacks for empty slots
    int                    skippedSlotCount{0}; // entries that could not be decoded and were omitted from the grid

    [[nodiscard]] int slotCount() const { return rows * columns; }

    /// Number of slots holding an item; 0 for a completely empty container.
    [[nodiscard]] int filledSlotCount() const {
        int filled = 0;
        for (auto const& slot : slots) {
            if (!slot.isNull()) ++filled;
        }
        return filled;
    }

    [[nodiscard]] static ContainerPreview empty(int columns, int rows) {
        ContainerPreview preview;
        preview.columns = columns;
        preview.rows    = rows;
        preview.slots.resize(static_cast<size_t>(columns) * static_cast<size_t>(rows));
        return preview;
    }
};

} // namespace lamium::inspection::preview


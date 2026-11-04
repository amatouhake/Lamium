#pragma once

#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/world/item/ItemStack.h"

namespace lamium::inventory::game {
// Whether the screen's cursor holds items. The game's own test is no longer
// exported (SDK 26.60); the cursor's slot is.
inline bool cursorHasItems(ContainerScreenController const& controller) {
    auto const& cursor = ContainerScreenController::mCursorSlotData();
    return !controller._getItemStack(cursor.mCollectionName, cursor.mCollectionIndex).isNull();
}
} // namespace lamium::inventory::game

#pragma once

#include <optional>
#include <string>

class ContainerScreenController;
class ItemStackBase;
class ScreenController;

namespace lamium::inspection::hover {

/// One slot of a container screen, addressed the way the game's UI addresses
/// it: the owning controller plus a collection name/index pair.
struct HoveredSlot {
    ContainerScreenController* controller{nullptr};
    std::string                collectionName;
    int                        collectionIndex{-1};
};

/// Observes the game's own slot hover/unhover callbacks and remembers which
/// container slot the pointer is currently over.
///
/// The tracker is strictly read-only: it never touches container contents and
/// only resolves items through the controller's own accessors.
///
/// All members are expected to be called from the client's main (render)
/// thread, which is where both the hover callbacks and UI rendering happen.
class HoverTracker {
public:
    static HoverTracker& getInstance();

    /// Installs the hover hooks. Safe to call once per enable().
    void install();
    /// Removes the hover hooks and forgets the current slot.
    void uninstall();

    void onSlotHovered(ContainerScreenController& controller, std::string const& collectionName, int index);
    void onSlotUnhovered(ContainerScreenController& controller, std::string const& collectionName, int index);
    /// Forgets the tracked slot if it belongs to the given controller.
    void onControllerLeft(ContainerScreenController& controller);

    [[nodiscard]] std::optional<HoveredSlot> const& current() const { return mCurrent; }

    /// Returns the item the game displays in the tracked slot, but only if
    /// `controller` is the controller that received the hover. The caller must
    /// guarantee that `controller` is alive (e.g. it is owned by the ScreenView
    /// being rendered).
    [[nodiscard]] ItemStackBase const* resolveItem(ScreenController const& controller) const;

private:
    std::optional<HoveredSlot> mCurrent;
    bool                       mInstalled{false};
};

} // namespace lamium::inspection::hover


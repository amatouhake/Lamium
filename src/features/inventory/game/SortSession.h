#pragma once

#include <optional>
#include <string>

class ContainerScreenController;
class CreativeItemRegistry;

namespace ll::io {
class Logger;
}

namespace lamium::inventory::game {

/// A contiguous run of slots in one of the screen's item collections that
/// Lamium is allowed to reorder. Nothing outside it is ever addressed.
struct SortRegion {
    std::string collectionName; ///< The game's collection name, e.g. "inventory_items".
    int         size{0};
    std::string label; ///< Human readable, for logs.
};

/// One explicit Sort request: picks the region, snapshots it, plans, executes
/// the plan through the game's own container transfer machinery and verifies
/// the outcome after every step. Any disagreement aborts the remaining steps;
/// the operations already issued are ordinary, server-validated transfers.
class SortSession {
public:
    /// Chooses what to sort for the given screen: the storage container under
    /// the pointer when it is an ordinary one and container sorting is
    /// enabled, otherwise the player's main inventory. Empty when the screen
    /// offers no sortable region.
    static std::optional<SortRegion> selectRegion(ContainerScreenController& controller, bool sortContainers);

    /// Diagnostic summary of the screen's container collections, for logs.
    static std::string describeScreen(ContainerScreenController& controller);

    /// Runs the sort. Returns true when the region ended in the planned
    /// state (including "nothing to do").
    static bool
    run(ContainerScreenController&  controller,
        SortRegion const&           region,
        CreativeItemRegistry const* creativeRegistry,
        ll::io::Logger&             logger);
};

} // namespace lamium::inventory::game

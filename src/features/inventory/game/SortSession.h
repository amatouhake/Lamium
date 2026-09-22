#pragma once

#include <optional>
#include <string>
#include <string_view>

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
    static void tick(ContainerScreenController& controller);
    // A reason is logged only when there is an active job. Already-reported
    // failures and successful completion can reset silently.
    static void cancel(std::string_view reason = {});
    /// Chooses what to sort for the given screen: the storage container under
    /// the pointer when it is an ordinary one and container sorting is
    /// enabled, otherwise the player's main inventory. Empty when the screen
    /// offers no sortable region.
    static std::optional<SortRegion> selectRegion(ContainerScreenController& controller, bool sortContainers);

    /// Diagnostic summary of the screen's container collections, for logs.
    static std::string describeScreen(ContainerScreenController& controller);

    /// Plans the sort. Returns true when scheduled (or already sorted).
    /// tick() sends one operation and waits for its matching server responses.
    static bool
    run(ContainerScreenController&  controller,
        SortRegion const&           region,
        CreativeItemRegistry const* creativeRegistry,
        ll::io::Logger&             logger);
};

} // namespace lamium::inventory::game

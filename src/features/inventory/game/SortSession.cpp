#include "features/inventory/game/SortSession.h"
#include "features/inventory/game/ScreenTracker.h"
#include "features/inventory/game/TextInputTracker.h"
#include "features/inventory/game/RequestTracker.h"
#include "app/Runtime.h"

#include "features/inventory/game/StackClassifier.h"
#include "features/inventory/sort/SortPlanner.h"

#include "ll/api/io/Logger.h"

#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/deps/shared_types/legacy/ContainerType.h"
#include "mc/world/containers/SlotData.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/item/ItemStack.h"

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lamium::inventory::game {

namespace {

using SharedTypes::Legacy::ContainerType;
struct PendingSort {
    std::weak_ptr<ContainerScreenController> controller;
    SortRegion region;
    std::vector<ItemStack> representatives;
    sort::Plan plan;
    std::vector<sort::SlotStack> state;
    size_t next = 0;
    bool waiting = false;
};
std::shared_ptr<PendingSort> pending;

// Item collection names as the game's own UI definitions address them. They
// are only ever used after the screen confirms it has such a collection.
constexpr std::string_view                kInventoryCollection = "inventory_items";
constexpr std::array<std::string_view, 3> kStorageCollections{"container_items", "barrel_items", "shulker_box_items"};

// Ordinary storage: a plain grid of general-purpose slots with no
// role-specific slot. Furnaces, brewing stands, crafting tables and every
// other interface with semantic slots are deliberately not sortable.
bool isOrdinaryStorage(ContainerType type) {
    switch (type) {
    case ContainerType::Container: // chest, trapped chest, ender chest, barrel, shulker box
    case ContainerType::MinecartChest:
    case ContainerType::ChestBoat:
        return true;
    default:
        return false;
    }
}

std::string describeStack(ItemStack const& stack) {
    if (stack.isNull() || stack.mCount <= 0) return "-";
    return stack.getTypeName() + " x" + std::to_string(stack.mCount);
}

// "[S] minecraft:red_shulker_box x1" style label for a planned stack.
std::string describeStack(sort::SlotStack const& s) {
    if (s.empty()) return "-";
    std::string out(1, '[');
    out += sort::sectionLabel(s.key.section);
    out += "] ";
    out += s.key.tail.empty() ? s.key.typeName : s.key.tail;
    if (!s.key.name.empty()) out += " \"" + s.key.name + "\"";
    if (!s.key.enchantments.empty()) out += " ench" + std::to_string(s.key.enchantments.size());
    if (s.key.damage > 0) out += " dmg" + std::to_string(s.key.damage);
    out += " x" + std::to_string(s.count);
    if (s.fixed()) out += " (locked)";
    return out;
}

// Runtime view of a slot, compared against the planner's simulated state.
struct SlotVerifier {
    ContainerManagerController const& manager;
    std::string const&                collection;
    std::vector<ItemStack> const&     representatives;

    [[nodiscard]] ItemStack const& actual(int index) const { return manager.getItemStack(collection, index); }

    // A slot agrees with the simulation when it is empty exactly when the
    // simulation says so, and otherwise holds the same count of an item that
    // vanilla considers the same item as the group's representative.
    [[nodiscard]] bool matches(int index, sort::SlotStack const& expected) const {
        auto const& item        = actual(index);
        bool const  actualEmpty = item.isNull() || item.mCount <= 0;
        if (expected.empty() || actualEmpty) return expected.empty() && actualEmpty;
        if (item.mCount != expected.count) return false;
        if (expected.group < 0 || expected.group >= static_cast<int>(representatives.size())) return false;
        return item.matchesItem(representatives[static_cast<size_t>(expected.group)]);
    }

    [[nodiscard]] std::string mismatch(int index, sort::SlotStack const& expected) const {
        return "#" + std::to_string(index) + " expected " + describeStack(expected) + ", found "
             + describeStack(actual(index));
    }
};

} // namespace

std::optional<SortRegion> SortSession::selectRegion(ContainerScreenController& controller, bool sortContainers) {
    auto const& manager = controller.mContainerManagerController;
    if (!manager) return std::nullopt;

    if (sortContainers && isOrdinaryStorage(manager->getContainerType())) {
        // The pointer decides: hovering a slot of the opened container sorts
        // the container, anything else sorts the player's inventory.
        auto const& hovered = controller.mHoveredSlotData->mCollectionName.get();
        for (auto name : kStorageCollections) {
            std::string const collection(name);
            if (collection != hovered || !manager->hasContainerController(collection)) continue;
            int const size = manager->getContainerSize(collection);
            if (size <= 0) continue;
            return SortRegion{collection, size, "container"};
        }
    }

    std::string const inventory(kInventoryCollection);
    if (!manager->hasContainerController(inventory)) return std::nullopt;
    int const size = manager->getContainerSize(inventory);
    if (size <= 0) return std::nullopt;
    return SortRegion{inventory, size, "player inventory"};
}

std::string SortSession::describeScreen(ContainerScreenController& controller) {
    auto const& manager = controller.mContainerManagerController;
    if (!manager) return "no container manager";
    std::string out  = "type=" + std::to_string(static_cast<int>(manager->getContainerType()));
    out             += " hovered=\"" + controller.mHoveredSlotData->mCollectionName.get() + "\"";
    out             += " containers=[";
    bool first       = true;
    for (auto const& [name, ctrl] : manager->mContainers.get()) {
        if (!first) out += ", ";
        first  = false;
        out   += name + ":" + std::to_string(manager->getContainerSize(name));
    }
    out += "]";
    return out;
}

bool SortSession::run(
    ContainerScreenController&  controller,
    SortRegion const&           region,
    CreativeItemRegistry const* creativeRegistry,
    ll::io::Logger&             logger
) {
    if (pending) return false;
    auto const manager = controller.mContainerManagerController;
    if (!manager) {
        logger.warn("Sort aborted: screen has no container manager");
        return false;
    }
    if (controller._isCursorSelectedActive()) {
        logger.info("Sort skipped: an item is being held on the cursor");
        return false;
    }

    // 1. Snapshot. Copies keep the item kinds around for verification even
    //    after the live slots have changed.
    std::vector<ItemStack> snapshot;
    snapshot.reserve(static_cast<size_t>(region.size));
    for (int i = 0; i < region.size; ++i) {
        snapshot.emplace_back(manager->getItemStack(region.collectionName, i));
    }

    // 2. Classify with vanilla's stackability, 3. plan.
    StackClassifier classifier(creativeRegistry);
    bool const      isPlayerInventory = region.collectionName == kInventoryCollection;
    auto const      slots             = classifier.classify(snapshot, isPlayerInventory);
    auto const      plan              = sort::planSort(slots);

    int fixedSlots = 0;
    for (auto const& s : slots) fixedSlots += s.fixed() ? 1 : 0;
    if (fixedSlots > 0) {
        logger.info("{} item-locked slot(s) stay in place; the rest is sorted around them", fixedSlots);
    }

    int occupied = 0;
    for (auto const& s : slots) occupied += s.empty() ? 0 : 1;
    logger.info(
        "Sort start: {} \"{}\" ({} slots, {} occupied, {} kinds, order={}) -> {} operation(s)",
        region.label,
        region.collectionName,
        region.size,
        occupied,
        classifier.representatives().size(),
        classifier.usedCreativeOrder() ? "creative" : "identifier",
        plan.ops.size()
    );
    // Planned layout, for checking the order and consolidation by eye.
    {
        std::string layout;
        for (auto const& s : plan.expected) {
            if (s.empty()) continue;
            if (!layout.empty()) layout += ", ";
            layout += describeStack(s);
        }
        logger.debug("Planned layout: [{}]", layout);
    }
    if (plan.ops.empty()) {
        logger.info("Sort complete: already sorted");
        return true;
    }

    auto active = ScreenTracker::getInstance().current();
    if (!active || active.get() != &controller) return false;
    pending = std::make_shared<PendingSort>(active, region, classifier.representatives(), plan, slots, 0, false);
    return true;
}

void SortSession::cancel() { pending.reset(); }

void SortSession::tick(ContainerScreenController& controller) {
    if (!pending) return;
    // Vanilla transfers may synchronously close the screen and cancel the job.
    // Keep this invocation's state alive until control returns from vanilla.
    auto currentJob = pending;
    auto& job = *currentJob;
    auto active = job.controller.lock();
    auto& logger = Runtime::instance().self().getLogger();
    auto manager = controller.mContainerManagerController.get();
    if (!active || active.get() != &controller || !manager || manager->mContainersClosed
        || controller._isCursorSelectedActive()
        || !Runtime::instance().preferences().inventory.sorting
        || TextInputTracker::getInstance().isEditing(ScreenTracker::getInstance().currentView())) {
        logger.info("Sort cancelled: screen, input, or settings changed");
        cancel();
        return;
    }
    if (job.waiting) {
        auto result = transferResult();
        if (result == ResponseBarrier::Result::Waiting) return;
        if (result != ResponseBarrier::Result::Accepted) {
            logger.warn("Sort stopped: request rejected, untracked, or timed out ({})", static_cast<int>(result));
            cancel();
            return;
        }
        job.waiting = false;
        ++job.next;
    }
    if (!manager->hasContainerController(job.region.collectionName)
        || manager->getContainerSize(job.region.collectionName) != job.region.size) {
        logger.warn("Sort cancelled: container changed");
        cancel();
        return;
    }
    SlotVerifier verify{*manager, job.region.collectionName, job.representatives};
    // Compare the whole region, including slots not touched by our last step.
    // A manual action or another player editing storage invalidates the plan.
    for (int i = 0; i < job.region.size; ++i) {
        if (!verify.matches(i, job.state[static_cast<size_t>(i)])) {
            logger.warn("Sort stopped: {}", verify.mismatch(i, job.state[static_cast<size_t>(i)]));
            cancel();
            return;
        }
    }
    if (job.next == job.plan.ops.size()) {
        logger.info("Sort complete: {} operation(s) acknowledged", job.next);
        cancel();
        return;
    }
    auto const op = job.plan.ops[job.next];
    auto expected = job.state;
    if (!sort::applyOperation(expected, op)) {
        logger.error("Sort stopped: invalid planned operation");
        cancel();
        return;
    }
    SlotData const source(job.region.collectionName, op.from);
    SlotData const destination(job.region.collectionName, op.to);
    if (!beginTransfer(*manager)) {
        logger.warn("Sort stopped: no available client request scope");
        cancel();
        return;
    }
    bool success = false;
    try {
        success = op.kind == sort::OpKind::Move
            ? manager->handlePlaceAmount(source, op.count, destination)
            : manager->handleSwap(source, destination);
    } catch (...) {
        endTransfer();
        cancel();
        throw;
    }
    endTransfer();
    if (pending != currentJob) return;
    if (!success) {
        logger.warn("Sort stopped: vanilla refused a transfer");
        cancel();
        return;
    }
    job.state = std::move(expected);
    job.waiting = true;
}
} // namespace lamium::inventory::game

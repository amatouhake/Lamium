#include "features/inventory/game/TransferSession.h"
#include "features/inventory/game/CursorState.h"
#include "features/inventory/game/TransferRoutes.h"
#include "features/inventory/game/RequestTracker.h"
#include "features/inventory/game/ScreenTracker.h"
#include "features/inventory/game/TextInputTracker.h"
#include "features/inspection/hover/HoverTracker.h"
#include "app/Runtime.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/input/KeyboardRemappingLayout.h"
#include "mc/client/input/Keymapping.h"
#include "mc/client/options/IOptionRegistry.h"
#ifdef LAMIUM_TRANSFER_TRACE
#include "app/TraceLog.h"
#endif

#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/deps/shared_types/legacy/ContainerType.h"
#include "mc/world/containers/SlotData.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/VanillaItemTags.h"

#include <chrono>
#include <deque>
#include <compare>
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <tuple>

namespace lamium::inventory::game {
namespace {
using transfer::Gesture;
using transfer::Side;
using SharedTypes::Legacy::ContainerType;

#ifdef LAMIUM_TRANSFER_TRACE
TraceBudget inputBudget, queueBudget, sendBudget;
#define TRANSFER_TRACE(budget, ...) traceLog(budget, 400, __VA_ARGS__)
#else
#define TRANSFER_TRACE(...) ((void)0)
#endif

struct Slot {
    std::string collection;
    int index = -1;
    Side side = Side::Player;
    unsigned generation = 0;
    bool vanillaShift = false; // Shift + left stays vanilla here (worn items, inventory screen).
    bool occupied = true, extensions = false;
    bool cursor = false; // Items were on the cursor.
    bool operator==(Slot const& other) const {
        return std::tie(collection, index, side, generation) == std::tie(other.collection, other.index, other.side, other.generation);
    }
};
struct Request { Slot slot; Gesture gesture; int wheelDirection = 0; };
struct QueuedRequest { Request request; ItemStack expected; bool resolved = false; };
struct SlotState { Slot slot; ItemStack item; };
struct Confirmation { std::vector<Gesture> gestures; std::vector<SlotState> expected; };
std::optional<Confirmation> confirmation;
std::optional<int> observedDimension;

std::mutex inputLock;
std::optional<Slot> hover;
std::deque<Request> pulses;
Gesture drag = Gesture::None;
// Buttons whose presses Lamium cancelled; their releases are cancelled too, even
// if the drag already ended (a released modifier), so vanilla never sees a
// lone release.
transfer::ConsumedButtons consumedButtons;
// The button holding the drag. Its press is not consumed when vanilla
// received the press (a worn item equipped from the inventory screen).
int dragButton = -1;
// That press's slot: vanilla handled it, so the drag skips it.
std::optional<Slot> vanillaFirst;
std::atomic<unsigned> generation{1};
unsigned observedGeneration = 0;
unsigned stroke = 0;
unsigned observedStroke = 0;
// The slot the drag last took from. A drag moves each slot once per entry:
// staying on it does nothing more, entering it again moves what is there now
// (an item can go out and come back in one drag).
std::optional<Slot> lastDrag;
std::deque<QueuedRequest> queued;
// Guarded by inputLock: cancel() must release it even when no container
// screen is left to tick, or the shared request barrier stays busy.
std::optional<TransferToken> waiting;
// Guarded by inputLock. The game's drop key, read on the client thread for
// the input callbacks; whether items are on the cursor, likewise.
std::vector<int> dropKeys{'Q'};
std::chrono::steady_clock::time_point dropKeysRead{};
bool cursorHeld = false;
// A Shift + left press outside every slot with items on the cursor: if
// vanilla drops them soon after, the matching items follow (L-169). Vanilla
// ignores that press, so the next tick hands it over as a plain one.
std::optional<std::chrono::steady_clock::time_point> outsideShift;
bool outsideShiftUnsent = false;
std::optional<ItemStack> pendingDrop;

void releaseWaiting() {
    std::optional<TransferToken> token;
    {
        std::scoped_lock lock(inputLock);
        token = std::exchange(waiting, std::nullopt);
    }
    if (token) cancelTransfer(*token);
}

bool ordinary(ContainerType type) {
    return type == ContainerType::Container || type == ContainerType::MinecartChest
        || type == ContainerType::ChestBoat;
}

transfer::GestureOptions gestureOptions() {
    auto const& value = Runtime::instance().preferences().inventory;
    return {value.transferWheelOne, value.transferWheelStack,
            value.transferDragStack, value.transferDragOne, value.transferWheelMatching,
            value.transferWheelAll, value.transferClickMatching, value.transferClickAll, value.transferDragKeepOne,
            value.transferCursorMatching, value.transferDropOne, value.transferDropKeepOne, value.transferDropAll,
            value.transferDropMatching};
}

// The inventory screen moves between the main inventory and the hotbar in
// every game mode; creative's item catalog is not a transfer side.
bool inventoryScreen(ContainerManagerController& manager) {
    if (manager.getContainerType() != ContainerType::Inventory) return false;
    // Collection sizes are not documented (survival: 27 + 9); record each
    // layout once so a creative difference shows in the log.
    static std::pair<int, int> logged{-2, -2};
    std::pair<int, int> sizes{
        manager.hasContainerController("inventory_items") ? manager.getContainerSize("inventory_items") : -1,
        manager.hasContainerController("hotbar_items") ? manager.getContainerSize("hotbar_items") : -1};
    if (sizes != logged) {
        logged = sizes;
        Runtime::instance().self().getLogger().info("Inventory transfer: inventory screen with inventory_items {} hotbar_items {}",
            sizes.first, sizes.second);
    }
    return true;
}

int inventorySize(ContainerManagerController& manager) {
    return manager.hasContainerController("inventory_items") ? manager.getContainerSize("inventory_items") : 0;
}

std::optional<Side> sideOf(ContainerScreenController& controller, std::string const& name, int index) {
    auto& manager = *controller.mContainerManagerController;
    auto storage = transferRoutes::collections(controller);
    return transfer::collectionSide(name, index, inventorySize(manager), inventoryScreen(manager),
        std::ranges::find(storage, name) != storage.end());
}

// Every slot of one side, in grid order.
std::vector<Slot> sideSlots(ContainerScreenController& controller, Side side, unsigned slotGeneration) {
    auto& manager = *controller.mContainerManagerController;
    std::vector<Slot> slots;
    bool const screen = inventoryScreen(manager);
    int const size = inventorySize(manager);
    bool overflow = false;
    auto append = [&](std::string const& collection, bool storage = false) {
        if (!manager.hasContainerController(collection)) return;
        int const count = manager.getContainerSize(collection);
        if (count < 0 || count > 128 || slots.size() + static_cast<size_t>(count) > 128) {
            overflow = true; return;
        }
        for (int i = 0; i < count; ++i)
            if (transfer::collectionSide(collection, i, size, screen, storage) == side)
                slots.push_back({collection, i, side, slotGeneration});
    };
    if (side == Side::Player || screen) {
        // A 36-slot inventory already includes the hotbar; smaller main
        // inventories need the separate hotbar collection below them.
        if (size == 0 && manager.hasContainerController("combined_hotbar_and_inventory_items")) {
            append("combined_hotbar_and_inventory_items");
        } else {
            if (size < 36) append("hotbar_items");
            append("inventory_items");
        }
    } else {
        auto storage = transferRoutes::collections(controller);
        if (!storage.empty()) {
            for (auto const& name : storage) append(name, true);
        } else for (auto name : {"container_items", "barrel_items", "shulker_box_items"}) {
            if (manager.hasContainerController(name) && manager.getContainerSize(name) > 0) {
                append(name);
                break;
            }
        }
    }
    return overflow ? std::vector<Slot>{} : slots;
}

std::optional<Slot> matchingSource(ContainerScreenController& controller, Slot const& hovered,
                                   ItemStack const& reference, Side sourceSide) {
    auto& manager = *controller.mContainerManagerController;
    auto candidates = sideSlots(controller, sourceSide, hovered.generation);
    int const inventorySize = game::inventorySize(manager);
    std::vector<unsigned char> matches(candidates.size());
    int hoveredIndex = -1;
    for (size_t i = 0; i < candidates.size(); ++i) {
        auto const& candidate = candidates[i];
        auto const& item = manager.getItemStack(candidate.collection, candidate.index);
        matches[i] = !item.isNull() && item.mCount > 0 && item.matchesItem(reference);
        bool const sameSlot = candidate.collection == hovered.collection && candidate.index == hovered.index;
        bool const hotbarAlias = inventorySize >= 36
            && candidate.collection == "inventory_items"
            && hovered.collection == "hotbar_items" && candidate.index == hovered.index;
        if (hovered.side == sourceSide && (sameSlot || hotbarAlias)) hoveredIndex = static_cast<int>(i);
    }
    int const index = transfer::chooseSource(std::span<unsigned char const>{matches}, hoveredIndex);
    if (index < 0) return {};
    return candidates[static_cast<size_t>(index)];
}

// Items on the cursor pause transfers; hovering still tracks the slot for the
// held stack's own gesture.
bool available(ContainerScreenController& controller, bool cursor = false) {
    auto& runtime = Runtime::instance();
    auto manager = controller.mContainerManagerController;
    return runtime.enabled() && runtime.preferences().inventory.transfer
        && ScreenTracker::getInstance().current().get() == &controller
        && manager && !manager->mContainersClosed
        && (ordinary(manager->getContainerType()) || inventoryScreen(*manager)
            || !transferRoutes::collections(controller).empty())
        && (cursor || !game::cursorHasItems(controller))
        && !TextInputTracker::getInstance().isEditing(ScreenTracker::getInstance().currentView());
}

std::optional<Slot> slotAt(ContainerScreenController& controller, std::string const& name, int index) {
    if (!available(controller, true)) return {};
    auto manager = controller.mContainerManagerController;
    auto side = sideOf(controller, name, index);
    if (!side || !manager->hasContainerController(name)
        || index < 0 || index >= manager->getContainerSize(name) || manager->getContainerSize(name) > 128) return {};
    if (sideSlots(controller, Side::Player, generation.load()).empty()
        || sideSlots(controller, Side::Storage, generation.load()).empty()) return {};
    auto const& stack = manager->getItemStack(name, index);
    bool const occupied = !stack.isNull() && stack.mCount > 0;
    bool worn = inventoryScreen(*manager) && stack.mItem
        && transfer::wornItem(stack.getTypeName(),
                              stack.mItem->isHumanoidArmor() || stack.mItem->hasTag(VanillaItemTags::Armor()));
    Slot result{name, index, *side, generation.load(), worn, occupied, transferRoutes::supported(controller),
                game::cursorHasItems(controller)};
    int const size = inventorySize(*manager);
    if (size == 0 && manager->hasContainerController("combined_hotbar_and_inventory_items")) {
        if (name == "hotbar_items") result.collection = "combined_hotbar_and_inventory_items";
    } else if (name == "combined_hotbar_and_inventory_items") {
        if (size >= 36) result.collection = "inventory_items";
        else if (index < 9 && manager->hasContainerController("hotbar_items")) result.collection = "hotbar_items";
        else if (index >= 9 && size > 0) { result.collection = "inventory_items"; result.index -= 9; }
    } else if (name == "hotbar_items" && size >= 36) result.collection = "inventory_items";
    return result;
}

std::optional<Slot> currentSlot(ContainerScreenController& controller) {
    auto manager = controller.mContainerManagerController;
    bool const oldScreen = manager && (ordinary(manager->getContainerType()) || inventoryScreen(*manager));
    if (!oldScreen && transferRoutes::supported(controller)) {
        auto const& native = controller.mHoveredSlotData.get();
        if (!native.mCollectionName->empty())
            return slotAt(controller, native.mCollectionName.get(), native.mCollectionIndex);
    }
    auto const& current = inspection::hover::HoverTracker::getInstance().current();
    return current && current->controller == &controller
        ? slotAt(controller, current->collectionName, current->collectionIndex) : std::nullopt;
}

void stopPending() {
    releaseWaiting();
    queued.clear();
    lastDrag.reset();
    confirmation.reset();
}

void stopOperation(char const* reason) {
    TransferSession::cancel();
    stopPending();
    Runtime::instance().self().getLogger().info("Inventory transfer stopped: {}", reason);
}

bool sameStack(ItemStack const& first, ItemStack const& second) {
    bool const empty = first.isNull() || first.mCount == 0;
    bool const otherEmpty = second.isNull() || second.mCount == 0;
    return empty || otherEmpty ? empty == otherEmpty : first.mCount == second.mCount && first.matchesItem(second);
}
std::vector<SlotState> snapshot(ContainerScreenController& controller, unsigned revision) {
    std::vector<SlotState> state;
    auto manager = controller.mContainerManagerController;
    if (!manager || manager->mContainersClosed) return state;
    for (auto side : {Side::Player, Side::Storage})
        for (auto const& slot : sideSlots(controller, side, revision))
            state.push_back({slot, manager->getItemStack(slot.collection, slot.index)});
    return state;
}
bool unchanged(std::span<SlotState const> before, std::span<SlotState const> after) {
    if (before.size() != after.size()) return false;
    for (size_t i = 0; i < before.size(); ++i)
        if (before[i].slot != after[i].slot || !sameStack(before[i].item, after[i].item)) return false;
    return true;
}
bool confirmed(ContainerScreenController& controller, std::span<SlotState const> expected) {
    auto manager = controller.mContainerManagerController;
    if (!manager || manager->mContainersClosed || expected.empty()) return false;
    for (auto const& state : expected) {
        auto const& slot = state.slot;
        if (!manager->hasContainerController(slot.collection) || slot.index < 0
            || slot.index >= manager->getContainerSize(slot.collection)
            || !sameStack(state.item, manager->getItemStack(slot.collection, slot.index))) return false;
    }
    return true;
}
bool predictedMove(std::span<SlotState const> before, std::span<SlotState const> after,
                   Slot const& source, ItemStack const& reference, int requested) {
    if (before.size() != after.size()) return false;
    std::vector<transfer::CountChange> changes;
    for (size_t i = 0; i < before.size(); ++i) {
        auto const& old = before[i]; auto const& next = after[i];
        if (old.slot != next.slot) return false;
        bool const empty = old.item.isNull() || old.item.mCount == 0;
        bool const nextEmpty = next.item.isNull() || next.item.mCount == 0;
        bool const isSource = old.slot == source;
        bool const unchangedItem = sameStack(old.item, next.item);
        bool const sameKind = unchangedItem || ((isSource || old.slot.side != source.side)
            && (empty || old.item.matchesItem(reference)) && (nextEmpty || next.item.matchesItem(reference)));
        changes.push_back({isSource, empty ? 0 : int(old.item.mCount), nextEmpty ? 0 : int(next.item.mCount), sameKind});
    }
    return transfer::conservedMove(changes, requested);
}
bool predictedDrop(std::span<SlotState const> before, std::span<SlotState const> after,
                   Slot const& source, ItemStack const& reference, int requested) {
    if (before.size() != after.size()) return false;
    std::vector<transfer::CountChange> changes;
    for (size_t i = 0; i < before.size(); ++i) {
        auto const& old = before[i]; auto const& next = after[i];
        if (old.slot != next.slot) return false;
        bool const nextEmpty = next.item.isNull() || next.item.mCount == 0;
        bool const isSource = old.slot == source;
        bool const sameKind = sameStack(old.item, next.item) || (isSource && (nextEmpty || next.item.matchesItem(reference)));
        bool const empty = old.item.isNull() || old.item.mCount == 0;
        changes.push_back({isSource, empty ? 0 : int(old.item.mCount), nextEmpty ? 0 : int(next.item.mCount), sameKind});
    }
    return transfer::droppedMove(changes, requested);
}

void enqueue(ContainerScreenController& controller, Request request) {
    if (queued.size() >= 128) return;
    auto manager = controller.mContainerManagerController;
    if (!manager || !manager->hasContainerController(request.slot.collection)
        || request.slot.index < 0 || request.slot.index >= manager->getContainerSize(request.slot.collection)) return;
    auto const& stack = manager->getItemStack(request.slot.collection, request.slot.index);
    if (stack.isNull() || stack.mCount <= 0) return;
    TRANSFER_TRACE(queueBudget, "Transfer trace: enqueue {}:{} gesture={} wheel={} count={} queued={} stroke={}",
        request.slot.collection, request.slot.index, static_cast<int>(request.gesture), request.wheelDirection,
        static_cast<int>(stack.mCount), queued.size(), stroke);
    queued.push_back({std::move(request), stack});
}

void enqueueDrag(ContainerScreenController& controller, Slot const& slot, Gesture mode) {
    if (!transfer::dragging(mode) || lastDrag == slot) return;
    lastDrag = slot;
    enqueue(controller, {slot, mode});
}
void enqueueBulk(ContainerScreenController& controller, Request const& request) {
    auto& manager = *controller.mContainerManagerController;
    auto const& reference = manager.getItemStack(request.slot.collection, request.slot.index);
    bool const all = transfer::everything(request.gesture);
    auto const sourceSide = transfer::wheel(request.gesture)
        ? transfer::otherSide(transfer::wheelDestination(request.wheelDirection)) : request.slot.side;
    auto candidates = sideSlots(controller, sourceSide, request.slot.generation);
    // In the inventory screen the hotbar is a whole side of its own.
    bool const sections = sourceSide == Side::Player && !inventoryScreen(manager);
    std::vector<transfer::SweepSource> sources;
    int hovered = -1;
    for (size_t i = 0; i < candidates.size(); ++i) {
        auto const& candidate = candidates[i];
        auto const& stack = manager.getItemStack(candidate.collection, candidate.index);
        bool const occupied = !stack.isNull() && stack.mCount > 0;
        sources.push_back({occupied, occupied && !reference.isNull() && stack.matchesItem(reference),
            sections && transfer::hotbarSlot(candidate.collection, candidate.index, inventorySize(manager))});
        if (candidate == request.slot) hovered = static_cast<int>(i);
    }
    auto order = transfer::sweepOrder(sources, all, hovered);
    if (queued.size() + order.size() > 128) return;
    for (int index : order) enqueue(controller, {candidates[static_cast<size_t>(index)], request.gesture});
}
// Every stack of the reference's kind on one side, high slots first.
void enqueueMatching(ContainerScreenController& controller, Side side, ItemStack const& reference, Gesture gesture,
                     unsigned revision) {
    auto& manager = *controller.mContainerManagerController;
    auto candidates = sideSlots(controller, side, revision);
    std::vector<transfer::SweepSource> sources;
    for (auto const& candidate : candidates) {
        auto const& stack = manager.getItemStack(candidate.collection, candidate.index);
        bool const occupied = !stack.isNull() && stack.mCount > 0;
        sources.push_back({occupied, occupied && stack.matchesItem(reference), false});
    }
    auto order = transfer::sweepOrder(sources, false);
    if (queued.size() + order.size() > 128) return;
    for (int index : order) enqueue(controller, {candidates[static_cast<size_t>(index)], gesture});
}
enum class Move { Sent, Skipped, Stop };
// A drop of one item, all but one or the stack, through vanilla's drop
// request; confirmed with the batch like a move.
Move sendDrop(ContainerScreenController& controller, QueuedRequest const& entry, unsigned revision) {
    auto const& slot = entry.request.slot;
    auto manager = controller.mContainerManagerController;
    auto const& stack = manager->getItemStack(slot.collection, slot.index);
    ItemStack const reference = stack;
    int const count = stack.mCount;
    int const requested = transfer::amount(entry.request.gesture, count);
    if (requested <= 0) return Move::Skipped;
    auto before = snapshot(controller, revision);
    if (generation.load() != revision) return Move::Skipped;
    TRANSFER_TRACE(sendBudget, "Transfer trace: drop {}:{} amount={} count={} gesture={}", slot.collection, slot.index,
        requested, count, static_cast<int>(entry.request.gesture));
    bool submitted = false;
    try {
        submitted = requested == count || requested == 1
            ? transferRoutes::drop(controller, slot.collection, slot.index, requested < count)
            : transferRoutes::dropAmount(controller, slot.collection, slot.index, requested);
    } catch (...) {
        stopOperation("vanilla drop failed");
        return Move::Stop;
    }
    if (game::cursorHasItems(controller)) {
        stopOperation("a drop left items on the cursor");
        return Move::Stop;
    }
    auto after = snapshot(controller, revision);
    if (unchanged(before, after)) return Move::Skipped;
    if (!submitted || !predictedDrop(before, after, slot, reference, requested)) {
        stopOperation("vanilla did not predict the drop");
        return Move::Stop;
    }
    return Move::Sent;
}
// One move of a batch: vanilla predicts it and adds its request; the batch
// tracks the requests and confirms them all at once.
Move sendMove(ContainerScreenController& controller, QueuedRequest entry, unsigned revision,
              std::deque<QueuedRequest>& later) {
    auto const& request = entry.request;
    if (!transfer::enabled(request.gesture, gestureOptions())) return Move::Skipped;
    auto manager = controller.mContainerManagerController;
    if (!manager || !manager->hasContainerController(request.slot.collection)
        || request.slot.index < 0 || request.slot.index >= manager->getContainerSize(request.slot.collection))
        return Move::Skipped;
    auto const& hovered = manager->getItemStack(request.slot.collection, request.slot.index);
    bool const wheel = !entry.resolved && (request.gesture == Gesture::OneWheel || request.gesture == Gesture::StackWheel);
    if (hovered.isNull() || hovered.mCount <= 0 || !hovered.matchesItem(entry.expected)
        || (!wheel && hovered.mCount != entry.expected.mCount)) {
        Runtime::instance().self().getLogger().info("Inventory transfer stopped: source slot changed ({}:{} now {}, expected {})",
            request.slot.collection, request.slot.index, static_cast<int>(hovered.mCount),
            static_cast<int>(entry.expected.mCount));
        stopOperation("source slot changed");
        return Move::Stop;
    }
    if (transfer::dropping(request.gesture)) return sendDrop(controller, entry, revision);
    std::optional<Slot> source;
    std::optional<Slot> destination;
    if (wheel) {
        auto targetSide = transfer::wheelDestination(request.wheelDirection);
        auto sourceSide = transfer::otherSide(targetSide);
        bool const stackWheel = request.gesture == Gesture::StackWheel;
        if (!stackWheel && request.slot.side == targetSide) {
            if (!transfer::canReceive(hovered.mCount, hovered.getMaxStackSize())
                || transfer::readOnlyCollection(request.slot.collection)) return Move::Skipped;
            destination = request.slot;
        }
        source = !stackWheel && request.slot.side == sourceSide ? std::optional<Slot>(request.slot)
            : matchingSource(controller, request.slot, hovered, sourceSide);
    } else source = request.slot;
    if (!source) {
        TRANSFER_TRACE(sendBudget, "Transfer trace: no source for {}:{}", request.slot.collection, request.slot.index);
        return Move::Skipped;
    }
    auto const& stack = manager->getItemStack(source->collection, source->index);
    if (stack.isNull() || stack.mCount <= 0 || !stack.matchesItem(hovered)) return Move::Skipped;
    ItemStack const reference = stack;
    int const requested = transfer::amount(request.gesture, stack.mCount);
    if (requested <= 0) return Move::Skipped;
    int moved = 1;
    std::optional<QueuedRequest> rest;
    if (!destination && inventoryScreen(*manager)) {
        auto targets = sideSlots(controller, transfer::otherSide(source->side), source->generation);
        std::vector<transfer::Destination> room;
        for (auto const& target : targets) {
            auto const& item = manager->getItemStack(target.collection, target.index);
            bool const empty = item.isNull() || item.mCount <= 0;
            room.push_back({empty, !empty && item.matchesItem(stack), empty ? 0 : item.mCount, stack.getMaxStackSize()});
        }
        int const at = transfer::chooseDestination(std::span<transfer::Destination const>{room});
        if (at < 0) {
            TRANSFER_TRACE(sendBudget, "Transfer trace: no destination for {}:{}", source->collection, source->index);
            return Move::Skipped;
        }
        destination = targets[static_cast<size_t>(at)];
        moved = std::min(requested, transfer::room(room[static_cast<size_t>(at)]));
        // The rest of a stack follows in the same batch, on vanilla's prediction.
        if (moved < requested) {
            rest = entry;
            rest->request.slot = *source;
            rest->expected = reference;
            rest->expected.mCount = static_cast<uchar>(stack.mCount - moved);
            rest->resolved = true;
        }
    }
    if (!destination && !controller.tryGetAutoPlaceOrder(source->collection)) return Move::Skipped;
    auto before = snapshot(controller, revision);
    auto beforeSource = std::ranges::find_if(before, [&](auto const& state) { return state.slot == *source; });
    if (beforeSource == before.end() || !sameStack(beforeSource->item, reference)) {
        stopOperation("source changed while planning");
        return Move::Stop;
    }
    if (generation.load() != revision) return Move::Skipped;
    TRANSFER_TRACE(sendBudget, "Transfer trace: send {}:{} -> {} amount={} count={} gesture={} rest={}",
        source->collection, source->index,
        destination ? destination->collection + ":" + std::to_string(destination->index) : std::string("auto"),
        destination ? moved : transfer::amount(request.gesture, stack.mCount), static_cast<int>(stack.mCount),
        static_cast<int>(request.gesture), rest.has_value());
    bool submitted = true;
    try {
        if (destination) {
            SlotData const src(source->collection, source->index);
            SlotData const dst(destination->collection, destination->index);
            submitted = manager->handlePlaceAmount(src, moved, dst);
        } else if (transferRoutes::supported(controller)) {
            submitted = transferRoutes::autoPlace(controller, source->collection, source->index,
                requested, transfer::otherSide(source->side));
        } else controller._handleAutoPlace(requested, source->collection, source->index);
    } catch (...) {
        stopOperation("vanilla transfer failed");
        return Move::Stop;
    }
    auto after = snapshot(controller, revision);
    // No predicted mutation: this source has no valid room.
    if (unchanged(before, after)) return Move::Skipped;
    if (!submitted || !predictedMove(before, after, *source, reference, requested)) {
        stopOperation("vanilla did not predict a conserved transfer");
        return Move::Stop;
    }
    if (!destination) {
        auto nextSource = std::ranges::find_if(after, [&](auto const& state) { return state.slot == *source; });
        if (nextSource != after.end() && !nextSource->item.isNull()
            && transfer::amount(request.gesture, nextSource->item.mCount) > 0
            && request.gesture != Gesture::OneWheel && request.gesture != Gesture::OneDrag) {
            rest = QueuedRequest{{*source, request.gesture}, nextSource->item, true};
        }
    }
    if (rest) {
        if (destination) queued.push_front(std::move(*rest));
        else later.push_back(std::move(*rest));
    }
    return Move::Sent;
}
// Sends what is queued, up to the tick's budget, under `token`; `already`
// names gestures whose requests the token tracks before these moves.
void sendBatch(ContainerScreenController& controller, TransferToken token, std::vector<SlotState> before, unsigned revision,
               std::vector<Gesture> already) {
    std::vector<Gesture> gestures = std::move(already);
    std::deque<QueuedRequest> later;
    size_t sent = 0;
    for (size_t budget = transfer::batchPerTick; budget > 0 && !queued.empty(); --budget) {
        auto entry = std::move(queued.front());
        queued.pop_front();
        auto const gesture = entry.request.gesture;
        auto outcome = sendMove(controller, std::move(entry), revision, later);
        captureTransfer(token);
        if (outcome == Move::Stop || generation.load() != revision) {
            cancelTransfer(token);
            if (outcome != Move::Stop) stopPending();
            return;
        }
        if (outcome == Move::Sent) {
            ++sent;
            if (std::ranges::find(gestures, gesture) == gestures.end()) gestures.push_back(gesture);
        }
    }
    for (auto& rest : later) queued.push_back(std::move(rest));
    endTransfer(token);
    auto after = snapshot(controller, revision);
    std::vector<SlotState> touched;
    if (before.size() == after.size())
        for (size_t i = 0; i < after.size(); ++i)
            if (!sameStack(before[i].item, after[i].item)) touched.push_back(std::move(after[i]));
    TRANSFER_TRACE(sendBudget, "Transfer trace: batch sent={} touched={} queued={}", sent, touched.size(), queued.size());
    if (!sent && gestures.empty()) { cancelTransfer(token); return; }
    if (touched.empty() && transferResult(token) == ResponseBarrier::Result::Untracked) {
        cancelTransfer(token);
        return;
    }
    confirmation = Confirmation{std::move(gestures), std::move(touched)};
    bool stale;
    {
        std::scoped_lock lock(inputLock);
        stale = generation.load() != revision;
        if (!stale) waiting = token;
    }
    // Cancelled while vanilla ran (it may close the screen synchronously).
    if (stale) cancelTransfer(token);
}
// The held stack goes into the clicked empty slot, then the matching items
// from the other side follow, all in one batch (L-169, maintainer 2026-10-15).
void placeHeld(ContainerScreenController& controller, Slot const& slot, unsigned revision) {
    bool busy;
    {
        std::scoped_lock lock(inputLock);
        busy = waiting.has_value();
    }
    auto manager = controller.mContainerManagerController;
    if (busy || confirmation || !queued.empty() || !manager || manager->mContainersClosed
        || !transferRoutes::supported(controller) || !manager->hasContainerController("cursor_items")) return;
    ItemStack const held = manager->getItemStack("cursor_items", 0);
    auto const& target = manager->getItemStack(slot.collection, slot.index);
    if (held.isNull() || held.mCount <= 0 || (!target.isNull() && target.mCount > 0)) return;
    auto token = beginTransfer(*manager);
    if (!token) return;
    auto before = snapshot(controller, revision);
    try {
        controller._handlePlaceAll(slot.collection, slot.index);
    } catch (...) {
        cancelTransfer(*token);
        stopOperation("vanilla place failed");
        return;
    }
    captureTransfer(*token);
    auto const& placed = manager->getItemStack(slot.collection, slot.index);
    // Part or none placed (a slot with a role): vanilla keeps its own click.
    if (game::cursorHasItems(controller) || placed.isNull() || placed.mCount != held.mCount || !placed.matchesItem(held)) {
        cancelTransfer(*token);
        stopOperation("held stack not placed");
        return;
    }
    enqueueMatching(controller, transfer::otherSide(slot.side), held, Gesture::CursorMatchingClick, revision);
    sendBatch(controller, *token, std::move(before), revision, {Gesture::CursorMatchingClick});
}
}

bool TransferSession::mouseButton(int button, bool down, bool shift, bool control, bool alt, bool drop, bool cancelled) {
    std::scoped_lock lock(inputLock);
    if (!down) {
        TRANSFER_TRACE(inputBudget, "Transfer trace: release button={} consumed={} drag={}", button, consumedButtons.consumed(button),
            static_cast<int>(drag));
        bool consumed = consumedButtons.release(button);
        if (button == dragButton) {
            dragButton = -1;
            drag = Gesture::None;
        }
        return consumed;
    }
    // A new press decides afresh; a release lost to focus changes never
    // swallows a later vanilla click.
    consumedButtons.press(button);
    if (button == dragButton) dragButton = -1;
    TRANSFER_TRACE(inputBudget, "Transfer trace: press button={} shift={} control={} cancelled={} hover={}:{} drag={} stroke={}",
        button, shift, control, cancelled, hover ? hover->collection : std::string("-"), hover ? hover->index : -1,
        static_cast<int>(drag), stroke);
    if (!cancelled && !hover && button == 1 && shift && !control && !alt && cursorHeld) {
        outsideShift = std::chrono::steady_clock::now();
        outsideShiftUnsent = true;
    } else {
        outsideShift.reset();
        outsideShiftUnsent = false;
    }
    if (cancelled || !hover) return false;
    auto mode = transfer::mouseGesture(button, shift, control, alt, drop, hover->cursor);
    if (!transfer::enabled(mode, gestureOptions())) return false;
    if (hover->cursor) {
        if (mode != Gesture::CursorMatchingClick || hover->occupied || !hover->extensions) return false;
        consumedButtons.consume(button);
        if (pulses.size() < 128) pulses.push_back({*hover, mode});
        return true;
    }
    if ((!hover->occupied && !transfer::everything(mode)) || (transfer::extended(mode) && !hover->extensions)) return false;
    drag = transfer::dragging(mode) ? mode : Gesture::None;
    dragButton = transfer::dragging(mode) ? button : -1;
    ++stroke;
    if (transfer::vanillaShift(mode, true, hover->vanillaShift)) {
        vanillaFirst = *hover;
        return false;
    }
    consumedButtons.consume(button);
    auto source = *hover;
    if (pulses.size() < 128) pulses.push_back({source, mode});
    return true;
}

bool TransferSession::wheel(int direction, bool shift, bool control, bool cancelled) {
    std::scoped_lock lock(inputLock);
    TRANSFER_TRACE(inputBudget, "Transfer trace: wheel direction={} shift={} cancelled={} hover={}:{}",
        direction, shift, cancelled, hover ? hover->collection : std::string("-"), hover ? hover->index : -1);
    if (cancelled || !hover || hover->cursor) return false;
    auto mode = transfer::wheelGesture(shift, control);
    if (!transfer::enabled(mode, gestureOptions())) return false;
    if ((!hover->occupied && !transfer::everything(mode)) || (transfer::extended(mode) && !hover->extensions)) return false;
    if (pulses.size() < 128) pulses.push_back({*hover, mode, direction});
    return true;
}

void TransferSession::cursorDropped(ContainerScreenController&, ItemStack const& held) noexcept {
    try {
        std::scoped_lock lock(inputLock);
        auto at = std::exchange(outsideShift, std::nullopt);
        if (!at || std::chrono::steady_clock::now() - *at > std::chrono::seconds(1)
            || !transfer::enabled(Gesture::DropMatching, gestureOptions())) return;
        pendingDrop = held;
    } catch (...) {}
}

bool TransferSession::dropKey(int key) {
    std::scoped_lock lock(inputLock);
    return std::ranges::find(dropKeys, key) != dropKeys.end();
}

void TransferSession::modifierReleased() {
    std::scoped_lock lock(inputLock);
    TRANSFER_TRACE(inputBudget, "Transfer trace: modifier released drag={}", static_cast<int>(drag));
    drag = Gesture::None;
}

void TransferSession::cancel() {
    {
        std::scoped_lock lock(inputLock);
        TRANSFER_TRACE(inputBudget, "Transfer trace: cancel drag={} pulses={} generation={}", static_cast<int>(drag),
            pulses.size(), generation.load());
        drag = Gesture::None;
        dragButton = -1;
        vanillaFirst.reset();
        hover.reset();
        pulses.clear();
        outsideShift.reset();
        outsideShiftUnsent = false;
        pendingDrop.reset();
        ++generation;
        ++stroke;
    }
    // No tick may follow (the screen closed), so release the barrier now.
    // cancelTransfer locks the request tracker, so the window procedure may
    // call this too.
    releaseWaiting();
}

void TransferSession::slotHovered(ContainerScreenController& controller, std::string const& collection, int index) {
    auto slot = slotAt(controller, collection, index);
    std::scoped_lock lock(inputLock);
    hover = slot;
    if (drag != Gesture::None && pulses.size() < 128) pulses.push_back({hover.value_or(Slot{}), drag});
}

void TransferSession::slotUnhovered(ContainerScreenController&, std::string const& collection, int index) {
    std::scoped_lock lock(inputLock);
    if (hover && hover->collection == collection && hover->index == index) hover.reset();
    if (drag != Gesture::None && pulses.size() < 128) pulses.push_back({Slot{}, drag});
}

void TransferSession::tick(ContainerScreenController& controller) {
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) { stopOperation("world unavailable"); return; }
    int const dimension = static_cast<int>(player->getDimensionId());
    if (observedDimension && *observedDimension != dimension) {
        observedDimension = dimension;
        stopOperation("dimension changed");
        return;
    }
    observedDimension = dimension;
    auto const now = std::chrono::steady_clock::now();
    std::optional<std::vector<int>> keys;
    if (now - dropKeysRead > std::chrono::seconds(1)) {
        dropKeysRead = now;
        if (auto layout = client->getOptions().getCurrentKeyboardRemapping()) {
            auto const& mapped = layout->getKeymappingByAction("key.drop").mKeys.get();
            if (!mapped.empty() && mapped.size() <= 4) keys = mapped;
        }
    }
    auto slot = currentSlot(controller);
    std::deque<Request> incoming;
    std::optional<Slot> first;
    Gesture mode;
    unsigned revision;
    unsigned currentStroke;
    {
        std::scoped_lock lock(inputLock);
        if (slot && slot->generation != generation.load()) slot.reset();
        hover = slot;
        if (keys) dropKeys = std::move(*keys);
        cursorHeld = game::cursorHasItems(controller);
        incoming.swap(pulses);
        first = std::exchange(vanillaFirst, std::nullopt);
        mode = drag;
        revision = generation.load();
        currentStroke = stroke;
    }
    if (observedGeneration != revision) {
        TRANSFER_TRACE(queueBudget, "Transfer trace: generation {} -> {} drops queued={}", observedGeneration, revision,
            queued.size());
        stopPending();
        observedGeneration = revision;
    }
    if (observedStroke != currentStroke) { lastDrag.reset(); observedStroke = currentStroke; }
    if (first) lastDrag = first;
    if (game::cursorHasItems(controller) && slot && available(controller, true))
        for (auto const& request : incoming)
            if (request.gesture == Gesture::CursorMatchingClick && request.slot == *slot
                && transfer::enabled(request.gesture, gestureOptions())) {
                placeHeld(controller, *slot, revision);
                return;
            }
    bool sendOutside = false;
    {
        std::scoped_lock lock(inputLock);
        sendOutside = std::exchange(outsideShiftUnsent, false);
    }
    // Before the check below, which needs an empty cursor. Unlocked: vanilla's
    // drop comes back through cursorDropped.
    if (sendOutside && available(controller, true) && transfer::enabled(Gesture::DropMatching, gestureOptions()))
        transferRoutes::pressAsPlainClick(controller);
    if (!available(controller)) {
        if (!queued.empty() || !incoming.empty())
            TRANSFER_TRACE(queueBudget, "Transfer trace: unavailable drops queued={}", queued.size());
        if (!queued.empty() || !incoming.empty() || mode != Gesture::None || confirmation)
            stopOperation("screen unavailable");
        else stopPending();
        return;
    }
    auto const options = gestureOptions();
    if ((confirmation && std::ranges::any_of(confirmation->gestures,
            [&](Gesture gesture) { return !transfer::enabled(gesture, options); }))
        || std::ranges::any_of(queued, [&](auto const& entry) { return !transfer::enabled(entry.request.gesture, options); })) {
        stopOperation("gesture disabled");
        return;
    }
    if (mode != Gesture::None && !transfer::enabled(mode, options)) { modifierReleased(); mode = Gesture::None; }
    std::optional<ItemStack> dropped;
    {
        std::scoped_lock lock(inputLock);
        dropped = std::exchange(pendingDrop, std::nullopt);
    }
    // The held stack is gone: every matching item in the player's inventory
    // follows it (in the inventory screen both sides are the player's).
    if (dropped && transfer::enabled(Gesture::DropMatching, options) && transferRoutes::supported(controller)) {
        enqueueMatching(controller, Side::Player, *dropped, Gesture::DropMatching, revision);
        if (inventoryScreen(*controller.mContainerManagerController))
            enqueueMatching(controller, Side::Storage, *dropped, Gesture::DropMatching, revision);
    }
    for (auto const& request : incoming) {
        if (request.slot.index < 0) { lastDrag.reset(); continue; } // The pointer left a slot.
        if (request.slot.generation != revision) continue;
        if (!transfer::enabled(request.gesture, gestureOptions())) continue;
        if (transfer::bulk(request.gesture)) {
            if (!slot || request.slot != *slot) continue;
            enqueueBulk(controller, request);
        } else if (transfer::wheel(request.gesture)) {
            if (!slot || request.slot.collection != slot->collection || request.slot.index != slot->index) continue;
            enqueue(controller, request);
        } else enqueueDrag(controller, request.slot, request.gesture);
    }
    if (slot) enqueueDrag(controller, *slot, mode);
    else lastDrag.reset();
    std::optional<TransferToken> pending;
    {
        std::scoped_lock lock(inputLock);
        pending = waiting;
    }
    if (pending) {
        auto result = transferResult(*pending);
        if (result == ResponseBarrier::Result::Waiting) return;
        TRANSFER_TRACE(sendBudget, "Transfer trace: response {} queued={}", static_cast<int>(result), queued.size());
        if (result != ResponseBarrier::Result::Accepted) {
            Runtime::instance().self().getLogger().warn("Inventory transfer stopped: response {}", static_cast<int>(result));
            stopOperation("request not accepted");
            return;
        }
        if (!confirmation || !confirmed(controller, confirmation->expected)) {
            stopOperation("authoritative slots differ from the transfer");
            return;
        }
        confirmation.reset();
        releaseWaiting();
    }
    if (queued.empty()) return;
    auto manager = controller.mContainerManagerController;
    if (!manager) return;
    // Every queued move goes out in this tick, as one vanilla batch; the
    // answers are awaited and checked once (DESIGN "Inventory transfer").
    auto token = beginTransfer(*manager);
    if (!token) {
        TRANSFER_TRACE(sendBudget, "Transfer trace: barrier busy, queued={}", queued.size());
        return;
    }
    sendBatch(controller, *token, snapshot(controller, revision), revision, {});
}
}

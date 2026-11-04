#include "features/inventory/RepeatTrades.h"
#include "features/inventory/RepeatTradeState.h"
#include "features/inventory/game/CursorState.h"
#include "features/inventory/game/RequestTracker.h"
#include "features/inventory/game/ScreenTracker.h"
#include "features/inventory/game/TextInputTracker.h"
#include "app/Runtime.h"
#include "app/TraceLog.h"
#include "app/Versions.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
// ScreenButtonMapping must be complete before InputComponent's vector member.
#include "mc/client/gui/controls/ScreenButtonMapping.h"
#include "mc/client/gui/controls/InputComponent.h"
#include "mc/client/gui/controls/UIPropertyBag.h"
#include "mc/client/gui/screens/ScreenEvent.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/Trade2ScreenController.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/json/Value.h"
#include "mc/network/packet/UpdateTradePacket.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/containers/managers/models/Trade2ContainerManagerModel.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/trading/MerchantRecipeList.h"
#include "mc/world/level/Level.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <string>

namespace lamium::inventory::repeatTrades {
namespace {
// Verified from the 1.26.51.01 trade row events; protected at every invocation.
constexpr uint selectRow = 2196830738u, tradeRow = 4096534505u, rightSource = 710676172u;
// Shift + right click arrives as its own global button that rows do not map;
// observed on 1.26.51.01 next to rightSource.
constexpr uint shiftRightSource = 1485818800u;
std::atomic<uint64_t> epoch{0};
struct Job {
    State state;
    uint64_t generation;
    int64_t entity;
    int tier, row, dimension;
    uint32_t recipe;
    std::optional<game::TransferToken> transfer;
    std::string prepareLayout;
    bool refilled = false;
    unsigned completed = 0;
};
std::shared_ptr<Job> pending;
thread_local bool invoking = false;
// Rows queue their screen event, so a remapped press marks only the next row
// press, briefly; any unmodified row press clears it.
std::optional<std::chrono::steady_clock::time_point> shiftedPress;
bool takeShiftedPress() {
    auto at = std::exchange(shiftedPress, std::nullopt);
    return at && std::chrono::steady_clock::now() - *at < std::chrono::milliseconds(250);
}
template <class... Args>
void trace(std::format_string<Args...> format, Args&&... args) noexcept {
#ifdef LAMIUM_REPEAT_TRADE_TRACE
    static TraceBudget budget;
    try {
        if (budget.take(160)) Runtime::instance().self().getLogger().info("{}", std::format(format, std::forward<Args>(args)...));
    } catch (...) {}
#endif
}
struct Snapshot {
    Offer offer;
    Counts counts{};
    int room = 0;
    uint32_t recipe = 0;
    int dimension = 0;
    std::string layout;
    bool inputsReturnable = false;
};
void clear() {
    auto job = std::move(pending);
    if (job && job->transfer) game::cancelTransfer(*job->transfer);
}
void end(char const* why) {
    unsigned completed = pending ? pending->completed : 0;
    trace("L-166 repeat stopped: {} completed={}", why, completed);
    clear();
    Runtime::instance().self().getLogger().debug("Repeated trading stopped: {} ({} trades)", why, completed);
}
bool enabled() {
    auto& runtime = Runtime::instance();
    return runtime.enabled() && runtime.snapshot()->inventory.repeatTrades;
}
bool focused(ContainerScreenController& controller) {
    auto& tracker = game::ScreenTracker::getInstance();
    auto const* view = tracker.currentView();
    return tracker.current().get() == &controller && view && view->mHasFocus
        && !game::cursorHasItems(controller) && !game::TextInputTracker::getInstance().isEditing(view);
}
bool shiftOnly() {
    constexpr int down = 0x8000;
    return (GetAsyncKeyState(VK_SHIFT) & down) && !(GetAsyncKeyState(VK_CONTROL) & down)
        && !(GetAsyncKeyState(VK_MENU) & down);
}
bool handler(ScreenController& controller, uint id) {
    auto const& interacted = controller.mButtonInteractedEventCallbackMap.get();
    auto other = interacted.find(id);
    return other != interacted.end() && other->second.size() == 1 && bool(other->second.front());
}
std::optional<Snapshot> read(ContainerScreenController& controller, int tier, int row) {
    auto manager = controller.mContainerManagerController;
    if (!manager || manager->mContainersClosed || manager->getContainerType() != SharedTypes::Legacy::ContainerType::Trade
        || !manager->hasContainerController("combined_hotbar_and_inventory_items")
        || manager->getContainerSize("combined_hotbar_and_inventory_items") != 36) return {};
    auto model = manager->mContainerManagerModel.lock();
    if (!model || *reinterpret_cast<void***>(model.get()) != Trade2ContainerManagerModel::$vftable()) {
        trace("L-166 repeat model does not match"); return {};
    }
    auto& trade = *static_cast<Trade2ContainerManagerModel*>(model.get());
    int index = trade.getTradeIndex(tier, row);
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    auto* actor = player ? player->getLevel().fetchEntity(controller.mEntityUniqueID.get(), false) : nullptr;
    auto* offers = actor ? actor->getTradeOffers() : nullptr;
    if (!offers || index < 0 || size_t(index) >= offers->mRecipeList->size()) return {};
    auto const& recipe = offers->mRecipeList.get()[size_t(index)];
    if (recipe.mBuyA->isNull() || recipe.mSell->isNull()) return {};
    bool second = !recipe.mBuyB->isNull() && recipe.mBuyB->mCount > 0;
    for (auto name : {"trade2_ingredient1_item", "trade2_ingredient2_item"})
        if (!manager->hasContainerController(name) || manager->getContainerSize(name) != 1) return {};
    auto const& inputA = manager->getItemStack("trade2_ingredient1_item", 0);
    auto const& inputB = manager->getItemStack("trade2_ingredient2_item", 0);
    InputReturnSpace returning{{inputA.isNull() ? 0 : int(inputA.mCount), inputB.isNull() ? 0 : int(inputB.mCount)},
        {inputA.isNull() ? 0 : int(inputA.getMaxStackSize()), inputB.isNull() ? 0 : int(inputB.getMaxStackSize())}, {}, 0,
        !inputA.isNull() && !inputB.isNull() && inputA.matchesItem(inputB)};
    Snapshot result;
    result.dimension = static_cast<int>(player->getDimensionId());
    result.recipe = recipe.mRecipeNetId->mRawId;
    result.offer = {int(recipe.mBuyA->mCount), second ? int(recipe.mBuyB->mCount) : 0,
        int(recipe.mSell->mCount), int(recipe.mUses), int(recipe.mMaxUses),
        int(recipe.mTier) <= trade.getEntityTradeTier(),
        second && recipe.mBuyA->matchesItem(recipe.mBuyB.get()),
        recipe.mSell->matchesItem(recipe.mBuyA.get()), second && recipe.mSell->matchesItem(recipe.mBuyB.get())};
    auto collect = [&](ItemStack const& stack, bool inventory) {
        bool empty = stack.isNull() || stack.mCount == 0;
        unsigned bits = 0;
        if (!empty) {
            if (stack.matchesItem(recipe.mBuyA.get())) bits |= 1;
            if (second && stack.matchesItem(recipe.mBuyB.get())) bits |= 2;
            if (stack.matchesItem(recipe.mSell.get())) bits |= 4;
            for (unsigned i = 0; i < 3; ++i) if (bits & (1u << i)) result.counts[i] += int(stack.mCount);
        }
        if (inventory) {
            if (empty) { result.room += int(recipe.mSell->getMaxStackSize()); ++returning.emptySlots; }
            else if (bits & 4) result.room += std::max(0, int(stack.getMaxStackSize()) - int(stack.mCount));
            if (!empty) {
                int room = std::max(0, int(stack.getMaxStackSize()) - int(stack.mCount));
                if (!inputA.isNull() && stack.matchesItem(inputA)) returning.partialRoom[0] += room;
                if (!inputB.isNull() && stack.matchesItem(inputB)) returning.partialRoom[1] += room;
            }
        }
        result.layout += std::format("{}:{}:{};", empty ? "empty" : stack.getTypeName(), int(stack.mCount), bits);
    };
    for (int i = 0; i < 36; ++i) collect(manager->getItemStack("combined_hotbar_and_inventory_items", i), true);
    for (auto name : {"trade2_ingredient1_item", "trade2_ingredient2_item"}) {
        collect(manager->getItemStack(name, 0), false);
    }
    result.inputsReturnable = inputsCanReturn(returning);
    return result;
}
void invoke(ScreenController& controller, uint id, int tier, int row) {
    UIPropertyBag bag;
    auto& json = bag.mJsonValue.get();
    json["#collections"]["trade_tiers"] = Json::Value(tier);
    json["#collections"]["trades"] = Json::Value(row);
    json["#enabled"] = Json::Value(true);
    json["#toggle_state"] = Json::Value(true);
    struct Guard { Guard() { invoking = true; } ~Guard() { invoking = false; } } guard;
    controller.mButtonInteractedEventCallbackMap->at(id).front()(&bag);
}
void advance(Trade2ScreenController& controller) {
    auto job = pending;
    if (!job) return;
    if (job->generation != epoch.load() || !enabled() || !focused(controller)
        || job->entity != controller.mEntityUniqueID->rawID
        || !versionSensitiveAllowed(capabilities::Id::RepeatedTrades)) { end("context changed"); return; }
    auto snapshot = read(controller, job->tier, job->row);
    if (!snapshot || snapshot->recipe != job->recipe || snapshot->dimension != job->dimension
        || !handler(controller, selectRow) || !handler(controller, tradeRow)) {
        end("trade route changed"); return;
    }
    auto phase = job->state.phase();
    if (phase == Phase::WaitingPrepare || phase == Phase::WaitingTrade) {
        auto response = job->transfer ? game::transferResult(*job->transfer) : game::ResponseBarrier::Result::Untracked;
        if (response == game::ResponseBarrier::Result::Waiting) return;
        trace("L-166 repeat reply phase={} result={} counts={},{},{} uses={}", int(phase), int(response),
            snapshot->counts[0], snapshot->counts[1], snapshot->counts[2], snapshot->offer.uses);
        bool noop = phase == Phase::WaitingPrepare && response == game::ResponseBarrier::Result::Untracked
            && snapshot->layout == job->prepareLayout;
        job->state.acknowledge(response == game::ResponseBarrier::Result::Accepted || noop ? Reply::Accepted : Reply::Failed,
            snapshot->counts, snapshot->offer.uses);
        if (job->transfer) game::cancelTransfer(*job->transfer);
        job->transfer.reset();
        if (job->state.phase() == Phase::Idle) { end("request or resulting state did not match"); return; }
        if (phase == Phase::WaitingTrade) { ++job->completed; job->refilled = false; }
        return; // At most one request group per UI tick.
    }
    if (!materials(snapshot->offer, snapshot->counts)) { end("unavailable offer or inputs"); return; }
    if (phase == Phase::Trade && snapshot->room < snapshot->offer.output) {
        if (job->refilled) { end("output does not fit"); return; }
        job->state.prepareAgain();
        phase = Phase::Prepare;
    }
    auto manager = controller.mContainerManagerController;
    // Selection returns both old inputs before refilling; vanilla drops any overflow.
    if (phase == Phase::Prepare && !snapshot->inputsReturnable) { end("inputs cannot return safely"); return; }
    auto token = game::beginTransfer(*manager);
    if (!token) { end("another inventory request owns the tracker"); return; }
    job->transfer = token;
    trace("L-166 repeat invoke phase={} price={},{} output={} counts={},{},{} uses={}/{} room={}",
        int(phase), snapshot->offer.costA, snapshot->offer.costB, snapshot->offer.output,
        snapshot->counts[0], snapshot->counts[1], snapshot->counts[2], snapshot->offer.uses, snapshot->offer.maxUses, snapshot->room);
    if (phase == Phase::Prepare) {
        job->prepareLayout = snapshot->layout;
        job->state.prepared(snapshot->counts, snapshot->offer.uses);
        job->refilled = true;
    } else if (!job->state.submitted(snapshot->offer, snapshot->counts, snapshot->room)) {
        end("trade no longer fits"); return;
    }
    try {
        if (!versionSensitiveAllowed(capabilities::Id::RepeatedTrades)) { end("unverified game"); return; }
        invoke(controller, phase == Phase::Prepare ? selectRow : tradeRow, job->tier, job->row);
        if (pending != job || job->generation != epoch.load()) { clear(); return; }
        game::endTransfer(*token);
    } catch (...) { clear(); throw; }
}
LL_TYPE_INSTANCE_HOOK(RepeatTradeInput, ll::memory::HookPriority::Normal, ScreenController,
    &ScreenController::$handleEvent, ::ui::ViewRequest, ScreenEvent& event) {
    try {
        if (!invoking && event.type == ScreenEventType::ButtonEvent) {
            auto const& button = event.data->button.get();
            if (button.state == ButtonState::Down && button.inputMode != InputMode::Undefined) {
                clear();
                bool shifted = takeShiftedPress();
                if (shifted && button.id != tradeRow) trace("L-166 repeat shifted press reached another button");
                if (shifted && button.inputMode == InputMode::Mouse && button.id == tradeRow
                    && button.fromButtonId == rightSource && enabled() && button.properties
                    && versionSensitiveAllowed(capabilities::Id::RepeatedTrades)) {
                    auto active = game::ScreenTracker::getInstance().current();
                    auto const& json = button.properties->mJsonValue.get();
                    if (active && static_cast<ScreenController*>(active.get()) == this && focused(*active)
                        && json.isMember("#collections") && json.isMember("#enabled")
                        && json["#enabled"].isBool() && json["#enabled"].asBool(false)) {
                        auto const& collections = json["#collections"];
                        if (collections.isMember("trade_tiers") && collections.isMember("trades")
                            && collections["trade_tiers"].isInt() && collections["trades"].isInt()) {
                            int tier = collections["trade_tiers"].asInt(-1), row = collections["trades"].asInt(-1);
                            auto snapshot = tier >= 0 && row >= 0 ? read(*active, tier, row) : std::nullopt;
                            trace("L-166 repeat row snapshot={} tier={} row={}", bool(snapshot), tier, row);
                            if (snapshot && handler(*this, selectRow) && handler(*this, tradeRow)) {
                                auto job = std::make_shared<Job>();
                                job->generation = epoch.load(); job->entity = active->mEntityUniqueID->rawID;
                                job->tier = tier; job->row = row; job->recipe = snapshot->recipe; job->dimension = snapshot->dimension;
                                job->state.start(); pending = std::move(job);
                                trace("L-166 repeat started tier={} row={} recipe={}", tier, row, snapshot->recipe);
                                return ::ui::ViewRequest::Refresh;
                            }
                        }
                    }
                }
                // Shift + right click on a row does nothing in vanilla; keep it so.
                if (shifted && button.id == tradeRow) return ::ui::ViewRequest::Refresh;
            }
        }
    } catch (...) { clear(); }
    return origin(event);
}
bool tradeRowMapping(InputComponent const& component) {
    for (auto const& mapping : component.mButtonMappings.get())
        if (mapping.fromButtonId == rightSource && mapping.toButtonId == tradeRow
            && mapping.type == ButtonMappingType::Pressed) return true;
    return false;
}
LL_TYPE_INSTANCE_HOOK(RepeatTradeRow, ll::memory::HookPriority::Normal, InputComponent,
    &InputComponent::handleButtonEvent, ::ButtonHandleResult, ::VisualTree& tree, ::UIAnimationController& animation,
    ::ScreenInputContext& context, uint id, ::ButtonState state, ::glm::vec2 const& pointer, ::InputMode mode,
    bool tts) {
    try {
        if (id == rightSource && state == ButtonState::Down && tradeRowMapping(*this)) shiftedPress.reset();
        if (id == shiftRightSource && mode == InputMode::Mouse && enabled() && shiftOnly() && tradeRowMapping(*this)
            && versionSensitiveAllowed(capabilities::Id::RepeatedTrades)) {
            auto result = origin(tree, animation, context, rightSource, state, pointer, mode, tts);
            if (state == ButtonState::Down && result != ButtonHandleResult::NotHandled) {
                shiftedPress = std::chrono::steady_clock::now();
                trace("L-166 repeat shifted row press");
            }
            return result;
        }
    } catch (...) { shiftedPress.reset(); }
    return origin(tree, animation, context, id, state, pointer, mode, tts);
}
LL_TYPE_INSTANCE_HOOK(RepeatTradeTick, ll::memory::HookPriority::Normal, Trade2ScreenController,
    &Trade2ScreenController::$tick, ::ui::DirtyFlag) {
    auto result = origin();
    try { advance(*this); } catch (...) { clear(); }
    return result;
}
LL_TYPE_INSTANCE_HOOK(RepeatTradeLeave, ll::memory::HookPriority::Normal, Trade2ScreenController,
    &Trade2ScreenController::$onLeave, void) {
    try { clear(); shiftedPress.reset(); } catch (...) {}
    origin();
}
LL_TYPE_INSTANCE_HOOK(RepeatTradeRefresh, ll::memory::HookPriority::Normal, ClientNetworkHandler,
    &ClientNetworkHandler::$handle, void, NetworkIdentifier const& source, UpdateTradePacket const& packet) {
    try { clear(); } catch (...) {}
    origin(source, packet);
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {{RepeatTradeRow::hook, RepeatTradeRow::unhook}, {RepeatTradeInput::hook, RepeatTradeInput::unhook}, {RepeatTradeTick::hook, RepeatTradeTick::unhook},
    {RepeatTradeLeave::hook, RepeatTradeLeave::unhook}, {RepeatTradeRefresh::hook, RepeatTradeRefresh::unhook}};
ll::event::ListenerPtr exitListener;
}
void cancel() { epoch.fetch_add(1); }
bool start() {
    capabilities::begin(capabilities::Id::RepeatedTrades);
    try {
        for (auto& hook : hooks) if (!hook.installed) {
            if (hook.install(true) != 0) { capabilities::hooks(capabilities::Id::RepeatedTrades,false); stop(); return false; }
            hook.installed = true;
        }
        if (!exitListener) exitListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientExitLevelEvent>(
            [](auto&) { try { cancel(); clear(); } catch (...) {} });
        capabilities::hooks(capabilities::Id::RepeatedTrades,bool(exitListener));
        return bool(exitListener);
    } catch (...) { capabilities::hooks(capabilities::Id::RepeatedTrades,false); stop(); return false; }
}
void stop() {
    capabilities::stop(capabilities::Id::RepeatedTrades);
    cancel(); clear();
    if (exitListener) ll::event::EventBus::getInstance().removeListener(exitListener);
    exitListener.reset();
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
    for (auto const& hook : hooks) if (hook.installed) {
        capabilities::fail(capabilities::Id::RepeatedTrades,capabilities::Reason::RemovalFailed);
        Runtime::instance().self().getLogger().error("Could not remove repeated trading hooks"); break;
    }
}
}

#include "features/inventory/FakeOffhand.h"
#include "features/inventory/FakeOffhandPlan.h"
#include "features/interaction/PeriodicInput.h"
#include "features/camera/CameraSessions.h"
#include "app/Runtime.h"
#ifdef LAMIUM_OFFHAND_TRACE
#include "app/TraceLog.h"
#endif
#include "input/Actions.h"
#include "input/Binding.h"
#include "ui/SettingsScreen.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/client/network/LegacyClientNetworkHandler.h"
#include "mc/network/packet/PlayerHotbarPacket.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/VanillaItemTags.h"
#include "mc/world/item/components/IFoodItemComponent.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
#include "mc/world/item/HandSlot.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/InteractionResult.h"
#include "mc/network/packet/MobEquipmentPacket.h"
#include "mc/network/packet/MobEquipmentPacketPayload.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/block/actor/BlockActorType.h"
#include "mc/world/phys/HitResult.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

namespace lamium::inventory::fakeOffhand {
namespace {
// Settings are cached here: the build hook runs every client tick and must not
// copy the whole Settings object.
std::atomic_bool enabled = false;
std::atomic_int targetSlot = 8;
// Set from the window procedure as the dispatcher decides it, so a right-click
// chord is already active when vanilla receives the same click.
std::atomic_bool rightChordHeld = false;
// The native right-click handler already decided this hold; vanilla may have
// acted on it, so a later queued press must not decide again (L-122).
std::atomic_bool nativeDecided = false;
// A non-mouse trigger replays vanilla use edges; only the thread that sent
// the down edge may send the matching up edge.
std::atomic_bool synthetic = false;
std::atomic<std::thread::id> syntheticThread{};
// Only slot identities survive an instant-use hold; selection is restored
// inside each native call and vanilla owns the repeat timer.
std::atomic_int instantPrimary = -1, instantTarget = -1;
bool installed = false;
// The last reported instant restore. The server can answer the use with an
// update that reselects the borrowed slot (seen with fireworks, ~50 ms later).
using Clock = std::chrono::steady_clock;
std::atomic_int echoBorrowed = -1, echoPrevious = -1;
std::atomic<Clock::rep> echoUntil{0};

void reportSelection(LocalPlayer& player, int slot) {
    auto const& held = player.getInventory().getItem(slot);
    MobEquipmentPacket packet{MobEquipmentPacketPayload{player.getRuntimeID(), held, slot, slot, ContainerID::Inventory}};
    player.sendNetworkPacket(packet);
    player.mSentSelectedSlot = slot;
    player.mSentInventoryItem = held;
}
// The name must outlive the traits; callers keep it alongside.
ItemTraits traitsOf(ItemStack const& item, std::string const& name) {
    ItemTraits traits{.name = name};
    if (item.isNull()) {
        traits.empty = true;
        return traits;
    }
    if (!item.mItem) {
        traits.name = {};
        return traits;
    }
    auto const& native = *item.mItem;
    traits.block = static_cast<bool>(item.mBlock);
    traits.food = native.isFood();
    traits.timed = native.getMaxUseDuration(&item) > 0;
    traits.throwable = native.isThrowable();
    traits.bucket = native.isBucket();
    traits.liquidClip = native.isLiquidClipItem();
    traits.wearable = native.isHumanoidArmor() || native.hasTag(VanillaItemTags::Armor());
    traits.planter = native.isBlockPlanterItem();
    traits.fertilizer = native.isFertilizer();
    traits.dye = native.isDye();
    traits.damageable = native.isDamageable();
    // Checked in game: neither has a use, though swords report a use duration.
    traits.idleTool = native.hasTag(VanillaItemTags::Sword()) || native.hasTag(VanillaItemTags::Pickaxe());
    return traits;
}
std::string nameOf(ItemStack const& item) { return item.isNull() ? std::string{} : item.getTypeName(); }
// A solid block without a block entity.
bool plainBlock(LocalPlayer& player, HitResult const& hit) {
    if (hit.mType != HitResultType::Tile) return false;
    auto const& type = player.getDimensionBlockSource().getBlock(hit.mBlock).getBlockType();
    return type.mSolid && type.getBlockEntityType() == BlockActorType::Undefined;
}
bool primaryPasses(ItemStack const& item, LocalPlayer& player, HitResult const& hit) {
    auto name = nameOf(item);
    auto traits = traitsOf(item, name);
    bool cannotEat = false;
    if (traits.food) {
        auto* food = item.mItem->getFood();
        auto* hunger = player.getAttribute(Player::HUNGER()).mPtr;
        if (!food || !hunger) return false;
        cannotEat = foodBlocked(food->canAlwaysEat(), player.isCreative(),
            static_cast<float>(hunger->mCurrentValue), hunger->mCurrentMaxValue);
    }
    return primaryPass(traits, hit.mType == HitResultType::Tile, cannotEat, plainBlock(player, hit));
}
bool secondaryUsable(ItemStack const& item, LocalPlayer& player, HitResult const& hit) {
    auto name = nameOf(item);
    return secondaryInstant(traitsOf(item, name), player.isGliding(), hit.mType == HitResultType::Tile);
}
bool endsOnRightClick(Settings const& value) {
    auto chord = input::effectiveChord(value.bindings, input::Action::FakeOffhandUse);
    return !chord.empty() && chord.back() == input::Token{input::Device::Mouse, 2};
}
std::optional<int> chooseSlot(IClientInstance& client, HitResult const& solid, int& selected, bool& instant) {
    auto* player = client.getLocalPlayer();
    if (!Runtime::instance().enabled() || !player || !player->isAlive() || player->isSpectator()
        || !client.isInGameInputEnabled() || ui::ownsInput() || !gameplayScreen(client.getScreenName())
        || CameraSessions::instance().blocksLookInteraction(*player)) return {};
    auto* inventory = player->mInventory.get();
    if (!inventory || inventory->mSelectedContainerId != ContainerID::Inventory) return {};
    selected = inventory->mSelected;
    int target = targetSlot.load();
    if (selected < 0 || selected >= 9 || target < 0 || target >= 9) return {};
    auto const& secondary = player->getInventory().getItem(target);
    bool targetInstant = secondaryUsable(secondary, *player, solid);
    bool blockItem = !secondary.isNull() && secondary.mBlock && !targetInstant;
    bool hitBlock = solid.mType == HitResultType::Tile;
    bool interactive = hitBlock && player->getDimensionBlockSource().getBlock(solid.mBlock)
        .getBlockType().isInteractiveBlock();
    if (blockItem) return placementSlot(true, true, selected, target, true, hitBlock, interactive, player->isSneaking());
    // Timed use stops on a selection change (L-95 baseline). Never start it
    // from a per-call borrow or preempt an existing primary use.
    if (!player->mItemInUse->mItem->isNull()) return {};
    auto slot = instantUseSlot(true, true, selected, target,
        primaryPasses(player->getInventory().getItem(selected), *player, solid), targetInstant,
        solid.mType == HitResultType::Entity, interactive, player->isSneaking());
    instant = slot.has_value();
    return slot;
}
#ifdef LAMIUM_OFFHAND_TRACE
// One letter per property, so a trace shows what the classifier saw.
std::string traitText(ItemStack const& item) {
    auto name = nameOf(item);
    auto t = traitsOf(item, name);
    std::string text;
    for (auto [on, letter] : {std::pair{t.block, 'B'}, {t.food, 'F'}, {t.timed, 'T'}, {t.throwable, 'P'},
        {t.bucket, 'K'}, {t.liquidClip, 'L'}, {t.wearable, 'W'}, {t.planter, 'S'}, {t.fertilizer, 'M'},
        {t.dye, 'D'}, {t.damageable, 'G'}, {t.idleTool, 'I'}}) if (on) text += letter;
    return text.empty() ? "-" : text;
}
#endif
void traceChoice(char const* stage, IClientInstance& client,
    int selected, std::optional<int> slot, bool instant, HitResult const* buildHit = nullptr) noexcept {
#ifdef LAMIUM_OFFHAND_TRACE
    try {
        static TraceBudget budget;
        auto const& hit = buildHit ? *buildHit : client.getLatestHitResult();
        auto* player = client.getLocalPlayer();
        if (!player || !player->mInventory) return;
        int actual = player->mInventory->mSelected;
        if (selected < 0) selected = actual;
        int target = targetSlot.load();
        if (selected < 0 || selected >= 9 || target < 0 || target >= 9) return;
        auto const& primary = player->getInventory().getItem(selected);
        auto const& secondary = player->getInventory().getItem(target);
        bool sword = !primary.isNull() && primary.mItem && primary.mItem->hasTag(VanillaItemTags::Sword());
        bool pickaxe = !primary.isNull() && primary.mItem && primary.mItem->hasTag(VanillaItemTags::Pickaxe());
        traceLog(budget, 128,
            "L-95 adapter stage={} enabled={} synthetic={} owned={}/{} selected={} actual={} target={} chosen={} instant={} hit={} primary={}[{}] block={} sword={} pickaxe={} idle={} secondary={}[{}] known={} gliding={} using={} input={} ui={} screen={}",
            stage, enabled.load(), synthetic.load(), instantPrimary.load(), instantTarget.load(),
            selected, actual, target, slot.value_or(-1), instant, static_cast<int>(hit.mType),
            primary.isNull() ? "empty" : primary.getTypeName(), traitText(primary), static_cast<bool>(primary.mBlock),
            sword, pickaxe, primaryPasses(primary, *player, hit), secondary.isNull() ? "empty" : secondary.getTypeName(),
            traitText(secondary), secondaryUsable(secondary, *player, hit), player->isGliding(), !player->mItemInUse->mItem->isNull(),
            client.isInGameInputEnabled(), ui::ownsInput(), client.getScreenName());
    } catch (...) {}
#else
    (void)stage; (void)client; (void)selected; (void)slot; (void)instant; (void)buildHit;
#endif
}
void traceRestore(LocalPlayer& player, int slot, int previous, bool instant, bool reported,
    bool owned, bool restored) noexcept {
#ifdef LAMIUM_OFFHAND_TRACE
    try {
        static TraceBudget budget;
        auto* inventory = player.mInventory.get();
        traceLog(budget, 128,
            "L-95 restore borrowed={} previous={} instant={} reported={} owned={} restored={} now={} sent={} using={} gliding={}",
            slot, previous, instant, reported, owned, restored, inventory ? inventory->mSelected : -1,
            player.mSentSelectedSlot, !player.mItemInUse->mItem->isNull(), player.isGliding());
    } catch (...) {}
#else
    (void)player; (void)slot; (void)previous; (void)instant; (void)reported; (void)owned; (void)restored;
#endif
}
// Only a synchronous action owns this pointer. Vanilla acquires its own
// item references after selection; no item argument is substituted.
struct SelectionRestore;
thread_local SelectionRestore* instantBorrow = nullptr;
struct SelectionRestore {
    LocalPlayer& player;
    int slot, previous;
    bool instant, reported = false;
    SelectionRestore* outer = instantBorrow;
    SelectionRestore(LocalPlayer& value, int target, int before, bool isInstant)
        : player(value), slot(target), previous(before), instant(isInstant) {
        if (instant) instantBorrow = this;
    }
    ~SelectionRestore() {
        if (instant) instantBorrow = outer;
        try {
            auto client = ll::service::getClientInstance();
            if (!client || client->getLocalPlayer() != &player) return;
            auto* inventory = player.mInventory.get();
            bool owned = inventory && inventory->mSelectedContainerId == ContainerID::Inventory
                && inventory->mSelected == slot;
            bool restored = owned && inventory->selectSlot(previous, ContainerID::Inventory);
            if (restored && reported) {
                reportSelection(player, previous);
                if (instant) {
                    echoBorrowed.store(slot);
                    echoPrevious.store(previous);
                    echoUntil.store((Clock::now() + std::chrono::seconds(1)).time_since_epoch().count());
                }
            }
            traceRestore(player, slot, previous, instant, reported, owned, restored);
        } catch (...) {}
    }
};
bool beginInstant(IClientInstance& client, int selected, int slot, bool clearPrimary) {
    auto* player = client.getLocalPlayer();
    if (clearPrimary && !interaction::periodic::sendUseEdge(client, false)) {
        traceChoice("up-unavailable", client, selected, slot, true);
        return false;
    }
    if (client.getLocalPlayer() != player || !player || !player->mInventory
        || player->mInventory->mSelectedContainerId != ContainerID::Inventory
        || player->mInventory->mSelected != selected) return false;
    if (!player->mInventory->selectSlot(slot, ContainerID::Inventory)) {
        traceChoice("press-select-failed", client, selected, slot, true);
        return false;
    }
    SelectionRestore restore{*player, slot, selected, true};
    struct ReleaseUse {
        IClientInstance& client;
        bool owed = true;
        ~ReleaseUse() {
            if (owed) try { interaction::periodic::sendUseEdge(client, false); } catch (...) {}
        }
    } releaseUse{client};
    syntheticThread.store(std::this_thread::get_id());
    bool delivered = interaction::periodic::sendUseEdge(client, true);
    if (delivered) {
        instantPrimary.store(selected);
        instantTarget.store(slot);
        synthetic.store(true);
        traceChoice(clearPrimary ? "down-sent" : "native-down-sent", client, selected, slot, true);
    } else {
        traceChoice("down-unavailable", client, selected, slot, true);
    }
    releaseUse.owed = false;
    return delivered;
}
void reportBorrow(Player& source, HandSlot hand) noexcept {
    try {
        auto* borrow = instantBorrow;
        if (!borrow || borrow->reported || hand != HandSlot::Mainhand || &source != &borrow->player) return;
        auto* inventory = borrow->player.mInventory.get();
        if (!inventory || inventory->mSelectedContainerId != ContainerID::Inventory
            || inventory->mSelected != borrow->slot) return;
        reportSelection(borrow->player, borrow->slot);
        borrow->reported = true;
    } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(BuildAction, ll::memory::HookPriority::Normal, ClientInstance,
    &ClientInstance::_tickBuildAction, void, HitResult const& solid, HitResult const& liquid, bool advanceTime) {
    if (!enabled.load() || !(rightChordHeld.load() || synthetic.load())) {
        origin(solid, liquid, advanceTime);
        return;
    }
    std::optional<int> slot;
    int selected = -1;
    bool instant = false;
    LocalPlayer* player = nullptr;
    try {
        slot = chooseSlot(*this, solid, selected, instant);
        int primary = instantPrimary.load();
        if (primary >= 0) {
            if (!ownsInstantHold(primary, instantTarget.load(), selected,
                targetSlot.load(), slot.has_value() && instant)) {
                traceChoice("hold-cancel", *this, selected, slot, instant, &solid);
                release();
                slot.reset();
            }
        } else if (instant) slot.reset();
        if (slot) {
            player = getLocalPlayer();
            if (!player->mInventory->selectSlot(*slot, ContainerID::Inventory)) {
                traceChoice("hold-select-failed", *this, selected, slot, instant, &solid);
                if (instant) release();
                slot.reset();
            }
        }
    } catch (...) {
        if (instantPrimary.load() >= 0) try { release(); } catch (...) {}
        slot.reset();
    }
    if (!slot) {
        origin(solid, liquid, advanceTime);
        return;
    }
    SelectionRestore restore{*player, *slot, selected, instant};
    origin(solid, liquid, advanceTime);
}

LL_TYPE_INSTANCE_HOOK(ReportUse, ll::memory::HookPriority::High, GameMode,
    &GameMode::$useItem, bool, ItemStack& item, HandSlot hand) {
    reportBorrow(mPlayer, hand);
    return origin(item, hand);
}
LL_TYPE_INSTANCE_HOOK(ReportOn, ll::memory::HookPriority::High, GameMode,
    &GameMode::$useItemOn, InteractionResult, ItemStack& item, BlockPos const& pos, uchar face,
    Vec3 const& hit, HandSlot hand, Block const* block, bool first) {
    reportBorrow(mPlayer, hand);
    return origin(item, pos, face, hit, hand, block, first);
}
int selectedSlot() {
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    auto* inventory = player ? player->mInventory.get() : nullptr;
    return inventory && inventory->mSelectedContainerId == ContainerID::Inventory ? inventory->mSelected : -1;
}
void undoEcho(char const* source, int before) noexcept {
    try {
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        if (!player || !player->mInventory) return;
        int after = selectedSlot(), borrowed = echoBorrowed.load(), previous = echoPrevious.load();
        bool recent = enabled.load() && Clock::now().time_since_epoch().count() < echoUntil.load();
        if (!undoesRestore(before, after, borrowed, previous, recent)) return;
        bool restored = player->mInventory->selectSlot(previous, ContainerID::Inventory);
        if (restored) reportSelection(*player, previous);
#ifdef LAMIUM_OFFHAND_TRACE
        static TraceBudget budget;
        traceLog(budget, 64, "L-95 echo source={} before={} after={} restored={} now={}",
            source, before, after, restored, selectedSlot());
#else
        (void)source;
#endif
    } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(EquipmentEcho, ll::memory::HookPriority::Normal, LegacyClientNetworkHandler,
    &LegacyClientNetworkHandler::$handle, void, NetworkIdentifier const& source,
    std::shared_ptr<MobEquipmentPacket> packet) {
    int before = selectedSlot();
    origin(source, std::move(packet));
    undoEcho("equipment", before);
}
LL_TYPE_INSTANCE_HOOK(HotbarEcho, ll::memory::HookPriority::Normal, LegacyClientNetworkHandler,
    &LegacyClientNetworkHandler::$handle, void, NetworkIdentifier const& source, PlayerHotbarPacket const& packet) {
    int before = selectedSlot();
    origin(source, packet);
    undoEcho("hotbar", before);
}
LL_TYPE_INSTANCE_HOOK(ChangeDimension, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$onWillChangeDimension, void, Player& player) {
    try {
        auto client = ll::service::getClientInstance();
        if (client && client->getLocalPlayer() == &player) release();
    } catch (...) {}
    origin(player);
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {{BuildAction::hook, BuildAction::unhook}, {ReportUse::hook, ReportUse::unhook},
    {ReportOn::hook, ReportOn::unhook}, {ChangeDimension::hook, ChangeDimension::unhook},
    {EquipmentEcho::hook, EquipmentEcho::unhook}, {HotbarEcho::hook, HotbarEcho::unhook}};
}
void configure(Settings const& value) {
    enabled.store(value.inventory.fakeOffhand);
    targetSlot.store(value.inventory.fakeOffhandSlot - 1);
}
void rightChord(bool held) {
    rightChordHeld.store(held);
    if (!held) nativeDecided.store(false);
}
bool rightChordActive() { return rightChordHeld.load(); }
bool instantPress(IClientInstance& client) noexcept {
    if (!enabled.load() || !rightChordHeld.load()) return false;
    // A queued press may have arrived first. Its captured handlers already
    // ran once; do not let a later physical handler try the primary item.
    if (synthetic.load()) return instantPrimary.load() >= 0;
    bool borrowed = false;
    try {
        int selected = -1;
        bool instant = false;
        auto slot = chooseSlot(client, client.getLatestHitResult(), selected, instant);
        traceChoice("native-choice", client, selected, slot, instant);
        if (!slot || !instant) return false;
        borrowed = true;
        // Replay the complete captured handler list once under selection.
        // sendUseEdge uses raw handlers, so it cannot reenter this wrapper.
        return beginInstant(client, selected, *slot, false);
    } catch (...) {
        // A callback may already have acted. Never retry it with the primary.
        return borrowed;
    }
}
void nativeDown(IClientInstance& client, std::function<void()> const& vanilla) {
    if (enabled.load() && rightChordHeld.load()) nativeDecided.store(true);
    if (instantPress(client)) return;
    // Vanilla acts on the first press inside this handler, before any build
    // tick; e.g. an empty hand opens a chest even while sneaking. Borrow the
    // placement slot for that first action too.
    std::optional<int> slot;
    int selected = -1;
    LocalPlayer* player = nullptr;
    try {
        if (enabled.load() && rightChordHeld.load() && !synthetic.load()) {
            bool instant = false;
            slot = chooseSlot(client, client.getLatestHitResult(), selected, instant);
            player = client.getLocalPlayer();
            if (!slot || instant || !player || !player->mInventory->selectSlot(*slot, ContainerID::Inventory))
                slot.reset();
            traceChoice("native-placement", client, selected, slot, false);
        }
    } catch (...) { slot.reset(); }
    if (!slot) {
        vanilla();
        return;
    }
    SelectionRestore restore{*player, *slot, selected, false};
    vanilla();
}
void press(IClientInstance& client) {
    auto value = Runtime::instance().preferences();
    if (!value.inventory.fakeOffhand) return;
    if (synthetic.load()) {
        traceChoice("press-already-held", client, -1, {}, false);
        return;
    }
    if (!queuedPressDecides(endsOnRightClick(value), nativeDecided.load())) {
        traceChoice("press-native-decided", client, -1, {}, false);
        return;
    }
    try {
        if (!Runtime::instance().enabled() || !client.getLocalPlayer() || !client.isInGameInputEnabled()
            || ui::ownsInput() || !gameplayScreen(client.getScreenName())) return;
        int selected = -1;
        bool instant = false;
        auto slot = chooseSlot(client, client.getLatestHitResult(), selected, instant);
        traceChoice("press-choice", client, selected, slot, instant);
        if (slot && instant) {
            beginInstant(client, selected, *slot, true);
            return;
        }
    } catch (...) { return; }
    // A right-click chord lets vanilla receive the click; rightChord() marks it.
    if (endsOnRightClick(value)) return;
    syntheticThread.store(std::this_thread::get_id());
    synthetic.store(interaction::periodic::sendUseEdge(client, true));
}
void release() {
    nativeDecided.store(false);
    instantPrimary.store(-1);
    instantTarget.store(-1);
    if (!synthetic.exchange(false)) return;
    // Never touch input handlers off the client thread (e.g. a shutdown
    // disable). Vanilla drops the stale hold on its next focus/input reset.
    if (syntheticThread.load() != std::this_thread::get_id()) return;
    if (auto client = ll::service::getClientInstance()) interaction::periodic::sendUseEdge(*client, false);
}
void start() {
    if (installed) return;
    try {
        for (auto& hook : hooks) if (!hook.installed) {
            if (hook.install(true) != 0) throw std::runtime_error("Could not install Fake Offhand hook");
            hook.installed = true;
        }
        installed = true;
    } catch (...) { stop(); throw; }
}
void stop() {
    release();
    rightChordHeld.store(false);
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
    installed = false;
}
}

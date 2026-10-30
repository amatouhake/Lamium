#include "features/interaction/ElytraSwap.h"
#include "features/inventory/EquipmentPlan.h"
#include "features/inventory/game/InventoryMove.h"
#include "app/Runtime.h"
#include "app/TraceLog.h"
#include "ui/SettingsScreen.h"
#include "ll/api/service/TargetedBedrock.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include "mc/entity/components/MoveInputComponent.h"
#include "mc/entity/components/RiptideTridentSpinAttackComponent.h"
#include "mc/world/actor/ActorFlags.h"
#include "mc/input/MoveInputState.h"
#include "mc/world/item/enchanting/ItemEnchants.h"
#include "mc/world/item/enchanting/EnchantmentInstance.h"
#include <atomic>
#include <stdexcept>
#include <utility>

namespace lamium::interaction::elytraSwap {
namespace {
using namespace inventory;
bool installed = false;
ll::event::ListenerPtr tickListener, exitListener;
ElytraSwapState state;
ItemStack worn;      // What the chest held before the elytra went on.
int dimension = -1;
bool jumpWasDown = false, keyPressed = false, wasSpinning = false;
void trace(char const* stage, int value = 0) noexcept {
#ifdef LAMIUM_RESTOCK_TRACE
    static TraceBudget budget;
    traceLog(budget, 1024, "ElytraSwap: {} value={}", stage, value);
#else
    (void)stage; (void)value;
#endif
}
bool enabled() {
    auto& runtime = Runtime::instance();
    return runtime.enabled() && runtime.snapshot()->interaction.elytraSwap;
}
LocalPlayer* localPlayer() {
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player || !player->isAlive() || player->isSpectator() || !player->mInventory || ui::ownsInput()
        || !client->isInGameInputEnabled()) return nullptr;
    return player;
}
bool empty(ItemStack const& stack) { return stack.isNull() || stack.mCount <= 0; }
bool isElytra(ItemStack const& stack) { return !empty(stack) && stack.getTypeName() == "minecraft:elytra"; }
ItemStack const& chest(LocalPlayer& player) { return player.getArmor(SharedTypes::Legacy::ArmorSlot::Torso); }
// The elytra with the most durability left, main inventory first, never the
// held slot or one about to stop working.
std::optional<int> elytraSlot(LocalPlayer& player) {
    std::vector<ReplacementCandidate> main, hotbar;
    for (int slot = 0; slot < 36; ++slot) {
        auto const& stack = player.getInventory().getItem(slot);
        if (slot == player.mInventory->mSelected || !isElytra(stack) || !stack.mItem) continue;
        (slot < 9 ? hotbar : main).push_back({slot,0,stack.mItem->getMaxDamage() - stack.getDamageValue()});
    }
    if (auto slot = chooseReplacement(main)) return slot;
    return chooseReplacement(hotbar);
}
bool returnSlotHoldsChest(LocalPlayer& player) {
    auto const& there = player.getInventory().getItem(*state.returnSlot);
    return !empty(there) && there.matchesItem(worn);
}
// The best chestplate in the inventory, for an elytra whose previous chest
// item is unknown (worn by hand) or gone.
std::optional<int> chestplateSlot(LocalPlayer& player) {
    std::vector<ChestplateCandidate> candidates;
    for (int slot = 0; slot < 36; ++slot) {
        auto const& stack = player.getInventory().getItem(slot);
        if (empty(stack) || !stack.mItem || !stack.getTypeName().ends_with("_chestplate")) continue;
        int levels = 0;
        if (stack.isEnchanted())
            for (auto const& e : stack.constructItemEnchantsFromUserData().getAllEnchants()) levels += e.mLevel;
        candidates.push_back({slot,stack.mItem->getArmorValue(),stack.mItem->getToughnessValue(),levels,
            stack.mItem->getMaxDamage() - stack.getDamageValue()});
    }
    return chooseChestplate(candidates);
}
// A firework jump means flying: return the chestplate after landing even if
// no glide followed. A key press on the ground waits for the key again.
void putOn(LocalPlayer& player, bool flying) {
    if (isElytra(chest(player))) return;
    auto slot = elytraSlot(player);
    trace("put-on-slot",slot ? *slot : -1);
    if (!slot) return;
    ItemStack elytra = player.getInventory().getItem(*slot), before = chest(player);
    if (!game::movePair(player,{game::Place::Armor,1},elytra,{game::Place::Inventory,*slot},before)) { trace("put-on-busy"); return; }
    state = {true,empty(before) ? std::nullopt : std::optional<int>{*slot},flying,false,0};
    worn = before;
    dimension = static_cast<int>(player.getDimensionId());
    // Gliding is not started here: tryStartGliding never succeeded after a
    // swap (5728561, 05648bf). A second jump press glides as in vanilla.
}
// Back to the remembered chest item, else the best chestplate; the elytra
// takes that item's slot. Without either the elytra stays on.
void takeOff(LocalPlayer& player) {
    auto slot = state.returnSlot && returnSlotHoldsChest(player) ? state.returnSlot : chestplateSlot(player);
    if (!slot) { trace("keep-elytra"); state = {}; return; }
    ItemStack elytra = chest(player), back = player.getInventory().getItem(*slot);
    if (game::movePair(player,{game::Place::Armor,1},back,{game::Place::Inventory,*slot},elytra)) {
        trace("take-off",*slot);
        state = {};
    }
}
bool jumpDown(LocalPlayer& player) {
    auto input = player.getEntityContext().tryGetComponent<MoveInputComponent>();
    if (!input) return false;
    auto& flags = *input->mInputState->mFlagValues;
    return flags.test(static_cast<size_t>(MoveInputState::Flag::JumpDown));
}
// A Riptide launch: the spin the trident starts (L-180).
bool spinning(LocalPlayer& player) {
    return player.getEntityContext().tryGetComponent<RiptideTridentSpinAttackComponent>() != nullptr
        || player.getStatusFlag(ActorFlags::Damagenearbymobs);
}
bool holdingFireworks(LocalPlayer& player, int slot) {
    auto const& held = player.getInventory().getItem(slot);
    return !empty(held) && held.getTypeName() == "minecraft:firework_rocket";
}
// L-246: Fake Offhand's slot counts too, whatever the main hand holds.
bool jumpFireworks(LocalPlayer& player, Settings const& settings) {
    bool main = holdingFireworks(player, player.mInventory->mSelected);
    bool fake = false;
    int slot = settings.inventory.fakeOffhandSlot - 1;
    if (settings.inventory.fakeOffhand && slot >= 0 && slot < 9) fake = holdingFireworks(player, slot);
    return jumpHasFireworks(main, settings.inventory.fakeOffhand, fake);
}
void tick() noexcept {
    try {
        bool key = std::exchange(keyPressed,false);
        auto* player = localPlayer();
        if (!player || !enabled() || player->isCreative()) { jumpWasDown = wasSpinning = false; return; }
        bool down = jumpDown(*player);
        bool jumped = down && !jumpWasDown;
        jumpWasDown = down;
        bool spin = spinning(*player);
        bool launched = spin && !wasSpinning;
        wasSpinning = spin;
        if (state.active && static_cast<int>(player->getDimensionId()) != dimension) { state = {}; trace("forget-dimension"); }
        auto const preferences = Runtime::instance().snapshot();
        auto const& settings = preferences->interaction;
        if (key) {
            // The key toggles: any worn elytra comes off, otherwise one goes on.
            if (isElytra(chest(*player))) takeOff(*player);
            else putOn(*player,false);
        } else if (settings.elytraFireworkJump && jumped && jumpFireworks(*player, *preferences)
            && !player->getVehicle() && !player->isInWater()) {
            // Any jump, the one from the ground included (decided 2026-09-30).
            trace("firework-jump");
            putOn(*player,true);
        } else if (settings.elytraRiptide && launched && !player->getVehicle()) {
            // At the launch, not while charging: Riptide flies without an
            // elytra, so only a real launch is worth a swap (decided 2026-10-14).
            trace("riptide-launch");
            putOn(*player,true);
        }
        ElytraInput in;
        in.enabled = true;
        in.gliding = player->isGliding();
        in.onGround = player->isOnGround();
        in.wearingElytra = isElytra(chest(*player));
        if (!state.active && !(in.wearingElytra && in.gliding)) return;
        if (!state.active) { worn = {}; dimension = static_cast<int>(player->getDimensionId()); trace("follow-worn"); }
        in.returnSlotHoldsChest = state.returnSlot && returnSlotHoldsChest(*player);
        in.chestplateAvailable = true; // Looked up only when it is time (takeOff).
        in.landingTicks = static_cast<int>(settings.elytraReturnSeconds * 20);
        switch (elytraStep(state,in)) {
        case ElytraStep::TakeOff:
        case ElytraStep::WearChestplate: takeOff(*player); break;
        case ElytraStep::Forget: trace("forget"); state = {}; break;
        default: break;
        }
    } catch (std::exception const& error) {
        state = {};
        static bool reported = false;
        if (!reported) { reported = true; Runtime::instance().self().getLogger().error("Auto Elytra stopped: {}",error.what()); }
    } catch (...) { state = {}; }
}
}
void press() { keyPressed = true; }
void start() {
    if (installed) return;
    try {
        auto& bus = ll::event::EventBus::getInstance();
        tickListener = bus.emplaceListener<ll::event::ClientLevelTickEvent>([](auto&) { tick(); });
        exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { state = {}; });
        if (!tickListener || !exitListener) throw std::runtime_error("Could not subscribe Auto Elytra lifecycle");
    } catch (...) { stop(); throw; }
    installed = true;
}
void stop() {
    for (auto* listener : {&tickListener,&exitListener}) if (*listener) {
        ll::event::EventBus::getInstance().removeListener(*listener); listener->reset();
    }
    state = {};
    installed = false;
}
}

#include "features/interaction/InventoryMoveTrace.h"
#ifdef LAMIUM_INVENTORYMOVE_TRACE
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/entity/systems/ClientInputUpdateSystem.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/input/ClientMoveInputHandler.h"
#include "mc/client/input/KeyboardRemappingLayout.h"
#include "mc/client/input/Keymapping.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/entity/components/MoveInputComponent.h"
#include "mc/entity/components/RawMoveInputComponent.h"
#include "mc/input/MoveInputState.h"
#include "mc/entity/components/ClientInputLockComponent.h"
#include "mc/world/actor/provider/PlayerMovement.h"
#include "mc/entity/components/ActorOwnerComponent.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/input/ClientInputMappingFactory.h"
#include "mc/deps/input/InputMapping.h"
#include "mc/deps/input/KeyboardInputMapping.h"
#include "mc/deps/input/KeyboardKeyBinding.h"
#include <unordered_set>
#include "mc/network/LoopbackPacketSender.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/packet/CorrectPlayerMovePredictionPacket.h"
#include "mc/network/packet/PlayerAuthInputPacket.h"
#include <cstring>
#include <intrin.h>
#include <Windows.h>
#include <array>
#include <chrono>
#include <format>
#include <string>
#include <vector>

#pragma comment(lib, "user32.lib") // GetAsyncKeyState, trace builds only

namespace lamium::interaction::inventoryMoveTrace {
namespace {
using Flag = MoveInputState::Flag;
using Clock = std::chrono::steady_clock;
template <class... Args>
void log(std::format_string<Args...> format, Args&&... args) noexcept {
    try { Runtime::instance().self().getLogger().info(std::format(format, std::forward<Args>(args)...)); } catch (...) {}
}
// Vanilla's keyboard bindings for the moves we feed; read again each time the
// inventory opens so remapping in the controls screen is picked up.
enum Move { Forward, Back, Left, Right, Jump, Sprint, Count };
constexpr std::array<char const*, Count> actionNames{"key.forward", "key.back", "key.left", "key.right", "key.jump", "key.sprint"};
std::array<std::vector<int>, Count> keys;
std::string lastScreen;
bool wasJump = false;
// Round 2: the keys fed this tick, for the later stages to re-apply.
bool feeding = false;
// Round 3: the flags fed this tick, OR-ed into the move input component too.
unsigned fedBits = 0;
unsigned updates = 0;
// Round 4: which states the two calculateMoveVector calls per tick read.
void const* componentState = nullptr;
void const* componentRaw = nullptr;
void const* rawComponentState = nullptr;
uintptr_t const moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
unsigned calcLogs = 0;
// Round 5: the auth input packet and the server's corrections.
unsigned corrections = 0, packetLogs = 0;
// Round 6: only feed the raw keys and lift MoveInputStateLocked; the later
// injections of rounds 3-5 stay off (they left flags stuck after closing).
constexpr bool injectLater = false;
// Round 7: let the inventory screen pass input on instead of feeding keys.
constexpr bool feedRaw = false;
unsigned absorbLogs = 0;
constexpr bool passInput = false; // Round 7 had no effect.
// Round 8: copy the gameplay mapping's movement key bindings into the
// other (screen) mappings once, and log what each mapping binds.
bool mappingsPatched = false;
unsigned lastComponentFlags = 0xFFFFFFFF, flagLogs = 0;
std::string lastPacket;
std::string lastUpdate;
float feedX = 0, feedZ = 0;
unsigned clears = 0, calcs = 0, locksSeen = 0;
unsigned lastLocks = 0xFFFFFFFF;
std::string lastCalc;
bool inventoryOpen = false;
unsigned fedTicks = 0, extractCalls = 0;
Clock::time_point lastReport{};

bool inventoryScreen(std::string const& name) { return name.starts_with("inventory_screen"); }
void readBindings(IClientInstance& client) {
    auto layout = client.getOptions().getCurrentKeyboardRemapping();
    if (!layout) { log("L-132 no keyboard layout"); return; }
    for (int i = 0; i < Count; ++i) {
        keys[i] = layout->getKeymappingByAction(actionNames[i]).mKeys;
        std::string text;
        for (int key : keys[i]) text += std::to_string(key) + " ";
        log("L-132 binding {} = {}", actionNames[i], text);
    }
}
bool down(Move move) {
    for (int key : keys[move])
        if (key > 0 && key < 256 && (GetAsyncKeyState(key) & 0x8000)) return true;
    return false;
}
bool focused() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

LL_STATIC_HOOK(InventoryMoveExtract, ll::memory::HookPriority::Normal,
    &ClientInputUpdateSystem::extractRawHIDInput, void,
    MovementAbilitiesComponent const& abilities, MoveInputComponent const& input,
    ActorDataFlagComponent const& flags, RawMoveInputComponent& raw,
    Optional<SneakingComponent const> sneaking, Optional<WasInWaterFlagComponent const> water) {
    if (inventoryOpen) try {
        auto& component = const_cast<MoveInputComponent&>(input);
        unsigned bits = 0;
        for (size_t i = 0; i < 11; ++i)
            if (component.mFlagValues->test(i)) bits |= 1u << i;
        if (bits != lastComponentFlags && flagLogs < 60) {
            ++flagLogs;
            lastComponentFlags = bits;
            log("L-132 MoveInputComponent flags {:#05x} (locked {})", bits, (bits >> 6) & 1);
        }
        component.mFlagValues->set(static_cast<size_t>(MoveInputComponent::Flag::MoveInputStateLocked), false);
    } catch (...) {}
    origin(abilities, input, flags, raw, sneaking, water);
    try {
        auto client = ll::service::getClientInstance();
        if (!client || ClientMoveInputHandler::getMoveInput(*client) != &input) return;
        auto screen = client->getScreenName();
        if (screen != lastScreen) {
            log("L-132 screen '{}' -> '{}' (in-game input {})", lastScreen, screen, client->isInGameInputEnabled());
            lastScreen = screen;
            if (inventoryScreen(screen)) readBindings(*client);
            wasJump = false;
        }
        inventoryOpen = inventoryScreen(screen);
        feeding = false;
        fedBits = 0;
        if (!inventoryOpen) return;
        ++extractCalls;
        if (!feedRaw || !focused()) return;
        rawComponentState = &*raw.mRawInput;
        auto& bits = *raw.mRawInput->mFlagValues;
        auto set = [&](Flag flag, bool on) {
            if (!on) return;
            bits.set(static_cast<size_t>(flag), true);
            fedBits |= 1u << static_cast<int>(flag);
        };
        bool f = down(Forward), b = down(Back), l = down(Left), r = down(Right), j = down(Jump), s = down(Sprint);
        float x = static_cast<float>(l) - static_cast<float>(r), z = static_cast<float>(f) - static_cast<float>(b);
        if (x != 0 || z != 0) {
            set(Flag::Up, f); set(Flag::Down, b); set(Flag::Left, l); set(Flag::Right, r);
            if (x != 0 && z != 0) { float n = 0.70710678f; x *= n; z *= n; }
            *raw.mRawMove = Vec2{x, z};
            feeding = true;
            feedX = x;
            feedZ = z;
        }
        set(Flag::JumpDown, j);
        set(Flag::JumpInputCurrentlyDown, j);
        set(Flag::JumpInputWasPressed, j && !wasJump);
        set(Flag::JumpInputWasReleased, !j && wasJump);
        wasJump = j;
        set(Flag::SprintDown, s);
        if (f || b || l || r || j) ++fedTicks;
        if (auto now = Clock::now(); now - lastReport > std::chrono::seconds(1)) {
            lastReport = now;
            log("L-132 inventory open: extract calls {}, fed {}, keys F{} B{} L{} R{} J{} S{}, rawMove {:.2f},{:.2f}; clears {}, calcs {}",
                extractCalls, fedTicks, f, b, l, r, j, s, raw.mRawMove->x, raw.mRawMove->z, clears, calcs);
            log("L-132 corrections from the server so far: {}", corrections);
        }
    } catch (...) {}
}
// Round 2: who drops the movement later. clearInputState wiping the
// component, input locks, and the move vector computed from the flags.
LL_STATIC_HOOK(InventoryMoveClear, ll::memory::HookPriority::Normal, &PlayerMovement::clearInputState, void,
    MoveInputComponent& input) {
    if (inventoryOpen) ++clears;
    origin(input);
}
LL_STATIC_HOOK(InventoryMoveLocks, ll::memory::HookPriority::Normal, &PlayerMovement::applyInputLocks, void,
    ClientInputLockComponent const& lock, MoveInputState& state) {
    origin(lock, state);
    try {
        unsigned short categories = 0, locks = 0;
        std::memcpy(&categories, &lock.mActiveCategories, 2);
        std::memcpy(&locks, &lock.mClientInputLocks, 2);
        unsigned both = (static_cast<unsigned>(categories) << 16) | locks;
        if (both != lastLocks && locksSeen < 200) {
            ++locksSeen;
            lastLocks = both;
            log("L-132 input locks: categories {:#06x} locks {:#06x} (inventory {})", categories, locks, inventoryOpen);
        }
    } catch (...) {}
}
unsigned bitsOf(MoveInputState const& state);
std::string_view stateName(void const* state) {
    if (state == componentState) return "MoveInputComponent.mInputState";
    if (state == componentRaw) return "MoveInputComponent.mRawInputState";
    if (state == rawComponentState) return "RawMoveInputComponent.mRawInput";
    return "other";
}
LL_STATIC_HOOK(InventoryMoveCalc, ll::memory::HookPriority::Normal, &PlayerMovement::calculateMoveVector, Vec2,
    MoveInputState const& state, bool flying, ActorDataFlagComponent const& data, bool water, SneakingComponent const* sneak) {
    if (!inventoryOpen) return origin(state, flying, data, water, sneak);
    ++calcs;
    auto before = bitsOf(state);
    // Round 4: put the fed keys into whichever state is read.
    if (injectLater && (feeding || fedBits)) {
        auto& writable = const_cast<MoveInputState&>(state);
        for (int i = 0; i < 27; ++i)
            if (fedBits & (1u << i)) writable.mFlagValues->set(static_cast<size_t>(i), true);
        if (feeding) *writable.mAnalogMoveVector = Vec2{feedX, feedZ};
    }
    auto result = origin(state, flying, data, water, sneak);
    try {
        if (calcLogs < 6 && fedBits) {
            ++calcLogs;
            log("L-132 calculateMoveVector #{} state {} ({}) caller +{:#x} flags before {:#09x} -> move {:.2f},{:.2f}", calcs,
                stateName(&state), static_cast<void const*>(&state),
                reinterpret_cast<uintptr_t>(_ReturnAddress()) - moduleBase, before, result.x, result.z);
        }
    } catch (...) {}
    return result;
}
unsigned bitsOf(MoveInputState const& state) {
    unsigned bits = 0;
    for (size_t i = 0; i < 27; ++i)
        if (state.mFlagValues->test(i)) bits |= 1u << i;
    return bits;
}
// Round 3: the step that fills the move input component from the input
// handler; put the fed flags into its states after it.
LL_STATIC_HOOK(InventoryMoveUpdate, ll::memory::HookPriority::Normal,
    &ClientInputUpdateSystem::inputHandlerUpdatePlayerState, void,
    MovementAbilitiesComponent const& abilities, MobEffectsComponent const& effects, ActorDataFlagComponent const& data,
    ActorOwnerComponent& owner, MoveInputComponent& input, Optional<PassengerComponent const> riding,
    Optional<WasInWaterFlagComponent const> water) {
    origin(abilities, effects, data, owner, input, riding, water);
    if (!inventoryOpen) return;
    componentState = &*input.mInputState;
    componentRaw = &*input.mRawInputState;
    try {
        ++updates;
        auto text = std::format("state {:#09x} raw {:#09x} move {:.2f},{:.2f} fed {:#09x}", bitsOf(*input.mInputState),
                                bitsOf(*input.mRawInputState), input.mMove->x, input.mMove->z, fedBits);
        if (text != lastUpdate) {
            lastUpdate = text;
            log("L-132 inputHandlerUpdatePlayerState after: {} (updates {})", text, updates);
        }
        if (injectLater) for (int i = 0; i < 27; ++i)
            if (fedBits & (1u << i)) {
                input.mInputState->mFlagValues->set(static_cast<size_t>(i), true);
                input.mRawInputState->mFlagValues->set(static_cast<size_t>(i), true);
            }
        if (injectLater && feeding) {
            *input.mInputState->mAnalogMoveVector = Vec2{feedX, feedZ};
            *input.mRawInputState->mAnalogMoveVector = Vec2{feedX, feedZ};
        }
    } catch (...) {}
}
}
using Input = PlayerAuthInputPacketPayload::InputData;
LL_TYPE_INSTANCE_HOOK(InventoryMoveSend, ll::memory::HookPriority::Normal, LoopbackPacketSender,
    &LoopbackPacketSender::$sendToServer, void, Packet& packet) {
    if (inventoryOpen) try {
        if (packet.getId() == MinecraftPacketIds::PlayerAuthInputPacket) {
            auto& auth = static_cast<PlayerAuthInputPacket&>(packet);
            auto& data = *auth.mInputData;
            auto flagText = [&] {
                std::string out;
                for (auto [flag, name] : {std::pair{Input::Up, "Up"}, {Input::Down, "Down"}, {Input::Left, "Left"}, {Input::Right, "Right"},
                                          {Input::JumpDown, "JumpDown"}, {Input::Jumping, "Jumping"}, {Input::StartJumping, "StartJumping"},
                                          {Input::JumpCurrentRaw, "JumpCurrentRaw"}, {Input::SprintDown, "SprintDown"},
                                          {Input::Sprinting, "Sprinting"}, {Input::StartSprinting, "StartSprinting"}})
                    if (data.contains(flag)) { out += name; out += ' '; }
                return out;
            };
            auto text = std::format("move {:.2f},{:.2f} raw {:.2f},{:.2f} analog {:.2f},{:.2f} delta {:.3f},{:.3f},{:.3f} flags [{}]",
                auth.mMove->x, auth.mMove->z, auth.mRawMoveVector->x, auth.mRawMoveVector->z, auth.mAnalogMoveVector->x,
                auth.mAnalogMoveVector->z, auth.mPosDelta->x, auth.mPosDelta->y, auth.mPosDelta->z, flagText());
            if (text != lastPacket && packetLogs < 120) {
                ++packetLogs;
                lastPacket = text;
                log("L-132 PlayerAuthInput as built: {} (fed {:#09x})", text, fedBits);
            }
            // Put the fed keys into the packet when the game left them out.
            if (!injectLater) { origin(packet); return; }
            if (feeding) {
                if (auth.mMove->x == 0 && auth.mMove->z == 0) *auth.mMove = Vec2{feedX, feedZ};
                if (auth.mRawMoveVector->x == 0 && auth.mRawMoveVector->z == 0) *auth.mRawMoveVector = Vec2{feedX, feedZ};
                if (auth.mAnalogMoveVector->x == 0 && auth.mAnalogMoveVector->z == 0) *auth.mAnalogMoveVector = Vec2{feedX, feedZ};
            }
            auto fed = [&](Flag flag) { return (fedBits & (1u << static_cast<int>(flag))) != 0; };
            if (fed(Flag::Up)) data.insert(Input::Up);
            if (fed(Flag::Down)) data.insert(Input::Down);
            if (fed(Flag::Left)) data.insert(Input::Left);
            if (fed(Flag::Right)) data.insert(Input::Right);
            if (fed(Flag::SprintDown)) data.insert(Input::SprintDown);
            if (fed(Flag::JumpDown)) {
                data.insert(Input::JumpDown);
                data.insert(Input::JumpCurrentRaw);
            }
            if (fed(Flag::JumpInputWasPressed)) data.insert(Input::JumpPressedRaw);
        }
    } catch (...) {}
    origin(packet);
}
LL_TYPE_INSTANCE_HOOK(InventoryMoveCorrection, ll::memory::HookPriority::Normal, ClientNetworkHandler,
    static_cast<void (ClientNetworkHandler::*)(NetworkIdentifier const&, CorrectPlayerMovePredictionPacket const&)>(
        &ClientNetworkHandler::$handle),
    void, NetworkIdentifier const& source, CorrectPlayerMovePredictionPacket const& packet) {
    if (inventoryOpen) ++corrections;
    origin(source, packet);
}
LL_TYPE_INSTANCE_HOOK(InventoryMoveAbsorb, ll::memory::HookPriority::Normal, UIScene, &UIScene::$absorbsInput, bool) {
    bool absorbs = origin();
    try {
        if (passInput && absorbs && getScreenName().starts_with("inventory_screen")) {
            if (absorbLogs < 3) {
                ++absorbLogs;
                log("L-132 inventory_screen absorbsInput {} -> false", absorbs);
            }
            return false;
        }
    } catch (...) {}
    return absorbs;
}
LL_TYPE_INSTANCE_HOOK(InventoryMoveMapping, ll::memory::HookPriority::Normal, ClientInputMappingFactory,
    &ClientInputMappingFactory::$getMapping, InputMapping const*, std::string const& name) {
    if (!mappingsPatched && !mActiveInputMappings->empty()) try {
        mappingsPatched = true;
        constexpr std::array movementKeys{87, 83, 65, 68, 32, 17};
        auto isMovementKey = [&](int key) { return std::find(movementKeys.begin(), movementKeys.end(), key) != movementKeys.end(); };
        auto& mappings = *mActiveInputMappings;
        log("L-132 first mapping request '{}' with {} active mappings", name, mappings.size());
        InputMapping const* source = nullptr;
        for (auto& [mappingName, mapping] : mappings) {
            std::string text;
            for (auto& binding : *mapping.keyboardMapping->keyBindings)
                if (isMovementKey(binding.keyNum)) text += std::format("{}={} ", *binding.buttonName, binding.keyNum);
            log("L-132 mapping '{}': {} key bindings; movement keys: {}", mappingName, mapping.keyboardMapping->keyBindings->size(), text);
            for (auto& binding : *mapping.keyboardMapping->keyBindings)
                if (binding.keyNum == 87 && binding.buttonName->find("forward") != std::string::npos) source = &mapping;
        }
        if (!source) {
            log("L-132 no mapping binds W to a forward button");
        } else {
            std::vector<KeyboardKeyBinding> movement;
            for (auto& binding : *source->keyboardMapping->keyBindings)
                if (isMovementKey(binding.keyNum)) movement.push_back(binding);
            for (auto& [mappingName, mapping] : mappings) {
                if (&mapping == source) continue;
                auto& bindings = *mapping.keyboardMapping->keyBindings;
                bool hasForward = std::any_of(bindings.begin(), bindings.end(),
                    [](KeyboardKeyBinding const& b) { return b.buttonName->find("forward") != std::string::npos; });
                if (hasForward) continue;
                for (auto& binding : movement) bindings.push_back(binding);
                log("L-132 added {} movement bindings to mapping '{}'", movement.size(), mappingName);
            }
        }
    } catch (...) { log("L-132 mapping patch failed"); }
    return origin(name);
}
void start() {
    InventoryMoveExtract::hook();
    InventoryMoveClear::hook();
    InventoryMoveLocks::hook();
    InventoryMoveCalc::hook();
    InventoryMoveUpdate::hook();
    InventoryMoveSend::hook();
    InventoryMoveAbsorb::hook();
    InventoryMoveMapping::hook();
    InventoryMoveCorrection::hook();
    Runtime::instance().self().getLogger().warn("Inventory move diagnostics enabled (L-132)");
}
void stop() {
    InventoryMoveExtract::unhook(true);
    InventoryMoveClear::unhook(true);
    InventoryMoveLocks::unhook(true);
    InventoryMoveCalc::unhook(true);
    InventoryMoveUpdate::unhook(true);
    InventoryMoveSend::unhook(true);
    InventoryMoveAbsorb::unhook(true);
    InventoryMoveMapping::unhook(true);
    InventoryMoveCorrection::unhook(true);
}
}
#else
namespace lamium::interaction::inventoryMoveTrace { void start() {} void stop() {} }
#endif

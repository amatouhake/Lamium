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
#include <cstring>
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
        if (!inventoryOpen) return;
        ++extractCalls;
        if (!focused()) return;
        auto& bits = *raw.mRawInput->mFlagValues;
        auto set = [&](Flag flag, bool on) { if (on) bits.set(static_cast<size_t>(flag), true); };
        bool f = down(Forward), b = down(Back), l = down(Left), r = down(Right), j = down(Jump), s = down(Sprint);
        float x = static_cast<float>(r) - static_cast<float>(l), z = static_cast<float>(f) - static_cast<float>(b);
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
LL_STATIC_HOOK(InventoryMoveCalc, ll::memory::HookPriority::Normal, &PlayerMovement::calculateMoveVector, Vec2,
    MoveInputState const& state, bool flying, ActorDataFlagComponent const& data, bool water, SneakingComponent const* sneak) {
    auto result = origin(state, flying, data, water, sneak);
    if (!inventoryOpen) return result;
    ++calcs;
    try {
        unsigned bits = 0;
        for (size_t i = 0; i < 27; ++i)
            if (state.mFlagValues->test(i)) bits |= 1u << i;
        auto text = std::format("flags {:#09x} analog {:.2f},{:.2f} -> {:.2f},{:.2f}", bits,
                                state.mAnalogMoveVector->x, state.mAnalogMoveVector->z, result.x, result.z);
        if (text != lastCalc) {
            lastCalc = text;
            log("L-132 calculateMoveVector {} (feeding {})", text, feeding);
        }
    } catch (...) {}
    // Force: when the flags arrived empty, use what we fed.
    if (feeding && result.x == 0 && result.z == 0) return Vec2{feedX, feedZ};
    return result;
}
}
void start() {
    InventoryMoveExtract::hook();
    InventoryMoveClear::hook();
    InventoryMoveLocks::hook();
    InventoryMoveCalc::hook();
    Runtime::instance().self().getLogger().warn("Inventory move diagnostics enabled (L-132)");
}
void stop() {
    InventoryMoveExtract::unhook(true);
    InventoryMoveClear::unhook(true);
    InventoryMoveLocks::unhook(true);
    InventoryMoveCalc::unhook(true);
}
}
#else
namespace lamium::interaction::inventoryMoveTrace { void start() {} void stop() {} }
#endif

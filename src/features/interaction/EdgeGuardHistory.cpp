#include "features/interaction/EdgeGuardHistory.h"
#include "features/interaction/EdgeGuardPlan.h"
#include "features/interaction/EdgeGuardReplayPlan.h"
#include "features/interaction/EdgeGuardTracePlan.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/entity/components/MoveInputComponent.h"
#include "mc/entity/components/ReplayStateComponent.h"
#include "mc/entity/utilities/IReplayableActorInput.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>

namespace lamium::interaction::edgeGuard::history {
namespace {
std::uintptr_t captureTarget = 0;
bool installed = false, reported = false;
constexpr auto capability = capabilities::Id::EdgeGuardReplay;

bool enabled() {
    auto& runtime = Runtime::instance();
    return onClientThread() && runtime.enabled() && runtime.snapshot()->interaction.edgeGuard
        && versionSensitiveAllowed(capabilities::Id::EdgeGuardMovement)
        && capabilities::registry().status(capability).canProbe();
}
bool readable(void const* address, size_t length, bool executable, bool game) {
    MEMORY_BASIC_INFORMATION page{};
    if (!VirtualQuery(address, &page, sizeof(page)) || page.State != MEM_COMMIT
        || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS))
        || (game && page.AllocationBase != GetModuleHandleW(nullptr))) return false;
    auto begin = reinterpret_cast<std::uintptr_t>(address), base = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
    if (begin < base || length > page.RegionSize || begin - base > page.RegionSize - length) return false;
    auto protection = page.Protect & 0xff;
    return !executable || protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ
        || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}
template <class Member>
std::optional<size_t> inputSlot(Member member) {
    if constexpr (sizeof(Member) != sizeof(void*)) return std::nullopt;
    else {
        auto* code = static_cast<uint8_t const*>(ll::memory::toFuncPtr(member));
        if (!readable(code, 16, true, false)) return std::nullopt;
        return replayInputSlot(std::span{code, size_t{16}});
    }
}
LL_TYPE_INSTANCE_HOOK(EdgeGuardCaptureHook, ll::memory::HookPriority::Low, IReplayableActorInput,
    captureTarget, void, MoveInputComponent const& input) {
    std::optional<MoveInputComponent> replacement;
    try {
        if (capabilities::registry().status(capability).available() && enabled()) {
            auto client = ll::service::getClientInstance();
            auto* player = client ? client->getLocalPlayer() : nullptr;
            auto own = player ? player->getEntityContext().tryGetComponent<MoveInputComponent>() : nullptr;
            using Flag = MoveInputState::Flag;
            if (own && &input == &*own && walking(*player)
                && !input.mInputState->mFlagValues->test(static_cast<size_t>(Flag::JumpDown))
                && !input.mRawInputState->mFlagValues->test(static_cast<size_t>(Flag::JumpDown))) {
                constexpr std::array directions{
                    static_cast<size_t>(Flag::Up), static_cast<size_t>(Flag::Down),
                    static_cast<size_t>(Flag::Left), static_cast<size_t>(Flag::Right),
                    static_cast<size_t>(Flag::UpLeft), static_cast<size_t>(Flag::UpRight),
                    static_cast<size_t>(Flag::DownLeft), static_cast<size_t>(Flag::DownRight)};
                auto const& processed = *input.mInputState;
                auto const& raw = *input.mRawInputState;
                auto const& analog = *processed.mAnalogMoveVector;
                bool changed = raw.mAnalogMoveVector->x != analog.x || raw.mAnalogMoveVector->z != analog.z;
                for (auto index : directions) changed |= raw.mFlagValues->test(index) != processed.mFlagValues->test(index);
                if (changed && std::isfinite(analog.x) && std::isfinite(analog.z)) {
                    replacement = replayInputCopy(input, directions);
                    if (!reported) {
                        reported = true;
                        Runtime::instance().self().getLogger().info("Edge Guard: capturing guarded replay input from an owned copy");
                    }
                }
            }
        }
    } catch (...) { replacement.reset(); }
    if (replacement) origin(*replacement);
    else origin(input);
}
}
std::optional<std::array<size_t, 3>> inputSlots() {
    auto capture = inputSlot(static_cast<void (IReplayableActorInput::*)(MoveInputComponent const&)>(&IReplayableActorInput::capture));
    auto pre = inputSlot(&IReplayableActorInput::preApplyInput), post = inputSlot(&IReplayableActorInput::postApplyInput);
    if (!capture || !pre || !post || capture == pre || capture == post || pre == post) return std::nullopt;
    return std::array{*capture, *pre, *post};
}
std::optional<std::array<std::uintptr_t, 3>> inputTargets(IReplayableActorInput& input) {
    auto slots = inputSlots();
    if (!slots) return std::nullopt;
    auto** table = *reinterpret_cast<void***>(&input);
    size_t last = *std::max_element(slots->begin(), slots->end());
    if (!readable(table, (last + 1) * sizeof(void*), false, true)) return std::nullopt;
    std::array<std::uintptr_t, 3> targets{};
    for (size_t i = 0; i < slots->size(); ++i) {
        auto* target = table[(*slots)[i]];
        if (!readable(target, 1, true, true)) return std::nullopt;
        targets[i] = reinterpret_cast<std::uintptr_t>(target);
    }
    return targets;
}
std::uint32_t checks = 0;
void observe() noexcept {
    try {
        if (!enabled()) return;
        // The hook is attached once and the target it was verified against
        // cannot change while it is held. Re-deriving the replay targets on
        // every own move-vector call cost milliseconds each (L-250), so after
        // the first attach the check only runs now and then to notice a
        // changed native contract.
        if (!checkReplayTarget(installed, ++checks)) return;
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        auto replay = player ? player->getEntityContext().tryGetComponent<ReplayStateComponent>() : nullptr;
        auto* input = replay ? replay->getCurrentCapturedInput() : nullptr;
        if (!input) return;
        auto targets = inputTargets(*input);
        if (!targets || (installed && captureTarget != (*targets)[0])) {
            capabilities::fail(capability,targets ? capabilities::Reason::TargetChanged : capabilities::Reason::NativeContract,
                "unverified or changed input target");
            Runtime::instance().self().getLogger().warn("Edge Guard: replay capture left vanilla (unverified or changed input target)");
            return;
        }
        if (installed) return;
        captureTarget = (*targets)[0];
        if (EdgeGuardCaptureHook::hook(true) != 0) {
            capabilities::hooks(capability,false);
            Runtime::instance().self().getLogger().warn("Edge Guard: replay capture left vanilla (could not attach)");
            return;
        }
        installed = true;
        capabilities::ready(capability);
        Runtime::instance().self().getLogger().info("Edge Guard: replay capture hook attached");
    } catch (...) { capabilities::fail(capability,capabilities::Reason::NativeContract,"replay binding failed"); }
}
bool stop() noexcept {
    capabilities::stop(capability);
    if (installed && !EdgeGuardCaptureHook::unhook(true)) {
        capabilities::fail(capability,capabilities::Reason::RemovalFailed); return false;
    }
    installed = reported = false;
    captureTarget = 0;
    checks = 0;
    return true;
}
}

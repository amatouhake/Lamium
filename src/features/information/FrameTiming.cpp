#include "features/information/FrameTiming.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/game/MinecraftGame.h"
#include <chrono>
#include <mutex>
#include <stdexcept>
namespace lamium::information {
namespace {
std::mutex mutex;
FrameRateMeter meter;
bool installed = false;
double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
LL_TYPE_INSTANCE_HOOK(FrameCompleted, ll::memory::HookPriority::Normal, MinecraftGame,
    &MinecraftGame::endFrame, void) {
    origin();
    std::lock_guard lock(mutex);
    meter.frame(now());
}
}
void startFrameTiming() {
    if (installed) return;
    { std::lock_guard lock(mutex); meter.reset(); }
    installed = FrameCompleted::hook(true) == 0;
    if (!installed) throw std::runtime_error("Could not install frame timing hook");
}
void stopFrameTiming() {
    if (installed && FrameCompleted::unhook(true)) installed = false;
    std::lock_guard lock(mutex);
    meter.reset();
}
std::optional<FrameStatistics> frameStatistics() {
    std::lock_guard lock(mutex);
    return meter.read(now());
}
}

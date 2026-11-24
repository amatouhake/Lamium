#include "features/research/CreativeTrace.h"
#ifdef LAMIUM_CREATIVE_TRACE
#include "app/Runtime.h"
#include "app/TraceLog.h"
#include "features/recipes/ItemKey.h"
#include "features/recipes/ItemList.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/network/packet/CommandRequestPacket.h"
#include "mc/network/packet/CommandRequestPacketPayload.h"
#include "mc/network/packet/ItemStackResponseContainerInfo.h"
#include "mc/network/packet/ItemStackResponseInfo.h"
#include "mc/network/packet/ItemStackResponsePacket.h"
#include "mc/server/commands/AutomationPlayerCommandOrigin.h"
#include "mc/server/commands/CommandContext.h"
#include "mc/server/commands/CommandOriginData.h"
#include "mc/server/commands/CommandOriginType.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/server/commands/CurrentCmdVersion.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <format>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace lamium::researchTrace::creative {
namespace {
using Clock = std::chrono::steady_clock;
TraceBudget pressBudget, requestBudget, responseBudget, cellBudget, experimentBudget;
std::atomic<unsigned> sequence{0};
ll::event::ListenerPtr renderListener;
int64_t nowMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count(); }
template <class... Args>
void log(TraceBudget& budget, unsigned limit, std::format_string<Args...> format, Args&&... args) noexcept {
    if (!budget.take(limit)) return;
    try {
        Runtime::instance().self().getLogger().info("L-360 creative seq={} ms={} {}", ++sequence, nowMs(),
            std::format(format, std::forward<Args>(args)...));
    } catch (...) {}
}
std::string shortName(std::string const& name) { return name.starts_with("minecraft:") ? name.substr(10) : name; }
std::string stackText(ItemStackBase const& stack) {
    if (stack.isNull()) return "-";
    return std::format("{}@{}x{}", shortName(stack.getTypeName()), stack.getAuxValue(), static_cast<int>(stack.mCount));
}
std::string cellText(ContainerScreenController& screen, std::string const& collection, int index) {
    try { return stackText(screen._getVisualItemStack(collection, index)); } catch (...) { return "?"; }
}
std::string cursorText(ContainerScreenController& screen) {
    try {
        auto manager = screen.mContainerManagerController.get();
        if (!manager || manager->mContainersClosed || !manager->hasContainerController("cursor_items")) return "?";
        return stackText(manager->getItemStack("cursor_items", 0));
    } catch (...) { return "?"; }
}
bool creativeCollection(ContainerScreenController& screen, std::string const& name) {
    if (name.starts_with("recipe_") || name.starts_with("creative")) return true;
    try { return screen._isInCreativeContainer(name); } catch (...) { return false; }
}
// Answers are logged only for a while after a press or an experiment, so other traffic stays out.
std::atomic<int64_t> pressedAt{0};
bool recent() { auto const at = pressedAt.load(); return at != 0 && nowMs() - at < 3000; }

// The game's own presses, with the cell's item and the pointer before and after.
LL_TYPE_INSTANCE_HOOK(CreativeAutoPlace, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::_handleAutoPlace, void, int amount, std::string const& collection, int index) {
    pressedAt = nowMs();
    std::string const before = cursorText(*this), item = cellText(*this, collection, index);
    origin(amount, collection, index);
    log(pressBudget, 400, "auto-place amount={} {}:{} item={} creative={} cursor {} -> {}", amount, collection, index, item,
        creativeCollection(*this, collection), before, cursorText(*this));
}
LL_TYPE_INSTANCE_HOOK(CreativeTakeAmount, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::_handleTakeAmount, void, int amount, std::string const& collection, int index) {
    pressedAt = nowMs();
    std::string const before = cursorText(*this), item = cellText(*this, collection, index);
    origin(amount, collection, index);
    log(pressBudget, 400, "take-amount amount={} {}:{} item={} creative={} cursor {} -> {}", amount, collection, index, item,
        creativeCollection(*this, collection), before, cursorText(*this));
}
LL_TYPE_INSTANCE_HOOK(CreativeTakeHalf, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::$_handleTakeHalf, void, std::string const& collection, int index) {
    pressedAt = nowMs();
    std::string const before = cursorText(*this), item = cellText(*this, collection, index);
    origin(collection, index);
    log(pressBudget, 400, "take-half {}:{} item={} creative={} cursor {} -> {}", collection, index, item, creativeCollection(*this, collection),
        before, cursorText(*this));
}
LL_TYPE_INSTANCE_HOOK(CreativeTakePlace, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::$_handleTakePlace, int, std::string const& collection, int index, bool all) {
    pressedAt = nowMs();
    std::string const before = cursorText(*this), item = cellText(*this, collection, index);
    int const result = origin(collection, index, all);
    log(pressBudget, 400, "take-place all={} {}:{} item={} creative={} result={} cursor {} -> {}", all, collection, index, item,
        creativeCollection(*this, collection), result, before, cursorText(*this));
    return result;
}
// What the server answers (a request it refuses shows here, on a server too).
LL_TYPE_INSTANCE_HOOK(CreativeResponse, ll::memory::HookPriority::Normal, ClientNetworkHandler,
    &ClientNetworkHandler::$handle, void, NetworkIdentifier const& source, ItemStackResponsePacket const& packet) {
    origin(source, packet);
    try {
        if (!recent()) return;
        unsigned count = 0;
        for (auto const& response : packet.mResponses.get()) {
            if (++count > 16) break;
            log(responseBudget, 300, "response id={} result={} containers={}", response.mClientRequestId->mRawId, static_cast<int>(response.mResult),
                response.mContainers->size());
        }
    } catch (...) {}
}

// Every item screen, every 10 frames: the creative collections, their size, how many cells show an item, how
// many name@aux are told apart and the first items; logged when that changes, also the screen and the game mode.
struct Cell { std::string collection; int index; };
std::string lastCells;
int frames = 0;
std::string cellsSummary(ContainerScreenController& screen, ContainerManagerController& manager) {
    std::string out;
    std::set<std::string> distinct;
    int total = 0;
    for (auto const& [name, container] : manager.mContainers.get()) {
        if (!creativeCollection(screen, name)) continue;
        int const size = manager.getContainerSize(name);
        int shown = 0;
        std::string first;
        for (int i = 0; i < size && i < 4000; ++i) {
            auto const& stack = screen._getVisualItemStack(name, i);
            if (stack.isNull()) continue;
            ++shown;
            distinct.insert(std::format("{}@{}", stack.getTypeName(), stack.getAuxValue()));
            if (shown <= 6) first += " " + stackText(stack);
        }
        total += shown;
        out += std::format(" [{} size={} shown={}{}]", name, size, shown, first);
    }
    return std::format("shown={} distinct={}{}", total, distinct.size(), out.empty() ? std::string(" (no creative collections)") : out);
}
std::vector<Cell> cellsFor(ContainerScreenController& screen, ContainerManagerController& manager, std::string const& name, int aux, bool exact) {
    std::vector<Cell> cells;
    for (auto const& [collection, container] : manager.mContainers.get()) {
        if (!creativeCollection(screen, collection)) continue;
        int const size = manager.getContainerSize(collection);
        for (int i = 0; i < size && i < 4000; ++i) {
            auto const& stack = screen._getVisualItemStack(collection, i);
            if (stack.isNull() || stack.getTypeName() != name) continue;
            if (exact && stack.getAuxValue() != aux) continue;
            cells.push_back({collection, i});
        }
    }
    return cells;
}
bool f6 = false, f7 = false, f8 = false, f9 = false;
bool edge(int key, bool& was) {
    bool const down = (GetAsyncKeyState(key) & 0x8000) != 0;
    bool const pressed = down && !was;
    was = down;
    return pressed;
}
// `/give`, as a typed command would send it; the server checks the rights.
void give(LocalPlayer& player, std::string const& name, int aux, int count) {
    auto& level = player.getLevel();
    std::string command = std::format("/give @s {} {} {}", name, count, aux);
    bool const cheats = level.getLevelData().mCheatsEnabled;
    log(experimentBudget, 200, "give '{}' cheats={} commands={} permission={}", command, cheats, level.hasCommandsEnabled(),
        static_cast<int>(player.getCommandPermissionLevel()));
    CommandContext context(command, std::make_unique<AutomationPlayerCommandOrigin>("", player), static_cast<int>(CurrentCmdVersion::Latest));
    CommandRequestPacketPayload payload(context, false);
    payload.mOrigin->mType = CommandOriginType::Player;
    CommandRequestPacket packet(payload);
    player.sendNetworkPacket(packet);
}
// Not read-only. With the pointer over an item in Lamium's lists: F6 logs the cells showing it (read-only); F7
// takes one onto the pointer by `_handleTakeAmount` (Shift: a stack); F8 a stack into the inventory by
// `_handleAutoPlace` (Shift: `_handleTakePlace` all, as a left press might); F9 `/give`s one (Shift: a stack).
// F7 and F8 only in creative, with an empty pointer and a cell that shows the item by name and aux.
void experiment(ContainerScreenController& screen, ContainerManagerController& manager, LocalPlayer& player) {
    bool const shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool const look = edge(VK_F6, f6), take = edge(VK_F7, f7), place = edge(VK_F8, f8), command = edge(VK_F9, f9);
    if (!look && !take && !place && !command) return;
    pressedAt = nowMs();
    std::string const key = recipes::itemList::hoveredItem();
    auto const parts = recipes::splitItemKey(key);
    std::string const name(parts.name);
    int const aux = parts.aux == recipes::anyAux ? 0 : parts.aux;
    if (name.empty()) { log(experimentBudget, 200, "experiment: nothing hovered in Lamium's lists"); return; }
    auto const cells = cellsFor(screen, manager, name, aux, true);
    if (look) {
        auto const loose = cellsFor(screen, manager, name, aux, false);
        std::string text;
        for (size_t i = 0; i < cells.size() && i < 12; ++i) text += std::format(" {}:{}", cells[i].collection, cells[i].index);
        log(experimentBudget, 200, "cells for '{}' (data '{}'): exact={} by name={}{}", key, std::string(parts.data), cells.size(), loose.size(), text);
        return;
    }
    if (command) {
        int const stack = cells.empty() ? 64 : screen._getVisualItemStack(cells[0].collection, cells[0].index).getMaxStackSize();
        try { give(player, name, aux, shift ? stack : 1); } catch (std::exception const& error) { log(experimentBudget, 200, "give failed: {}", error.what()); }
        return;
    }
    std::string const cursor = cursorText(screen);
    if (!player.isCreative() || cells.empty() || cursor != "-") {
        log(experimentBudget, 200, "experiment for '{}' refused: creative={} cells={} cursor={}", key, player.isCreative(), cells.size(), cursor);
        return;
    }
    auto const& cell = cells[0];
    int const stack = screen._getVisualItemStack(cell.collection, cell.index).getMaxStackSize();
    std::string const call = take ? (shift ? std::format("take-amount {}", stack) : std::string("take-amount 1"))
                                  : (shift ? std::string("take-place all") : std::format("auto-place {}", stack));
    log(experimentBudget, 200, "experiment for '{}': {} on {}:{} item={}", key, call, cell.collection, cell.index, cellText(screen, cell.collection, cell.index));
    try {
        if (take) screen._handleTakeAmount(shift ? stack : 1, cell.collection, cell.index);
        else if (shift) screen._handleTakePlace(cell.collection, cell.index, true);
        else screen._handleAutoPlace(stack, cell.collection, cell.index);
    } catch (...) { log(experimentBudget, 200, "experiment threw"); }
    log(experimentBudget, 200, "experiment after: cursor={}", cursorText(screen));
}
std::string lastScreen;
void seeScreen(ll::event::UIRenderEvent& event) {
    auto& view = event.screenView();
    if (!view.mHasFocus || !view.mController || !view.mController->_isContainerScreen()) return;
    auto& screen = static_cast<ContainerScreenController&>(*view.mController);
    auto manager = screen.mContainerManagerController.get();
    if (!manager || manager->mContainersClosed) return;
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return;
    try { experiment(screen, *manager, *player); } catch (...) {}
    std::string const name = view.getScreenName();
    if (name != lastScreen) {
        lastScreen = name, frames = 0, lastCells.clear();
        std::string collections;
        unsigned seen = 0;
        for (auto const& [collection, container] : manager->mContainers.get())
            if (++seen <= 40) collections += std::format(" {}:{}{}", collection, manager->getContainerSize(collection), creativeCollection(screen, collection) ? "*" : "");
        log(cellBudget, 300, "screen '{}' creative-player={} collections (* creative):{}", name, player->isCreative(), collections);
    }
    if (++frames % 10) return;
    std::string cells = cellsSummary(screen, *manager);
    if (cells == lastCells) return;
    lastCells = cells;
    for (size_t at = 0; at < cells.size(); at += 1500) log(cellBudget, 300, "cells '{}': {}", name, cells.substr(at, 1500));
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {
    {CreativeAutoPlace::hook, CreativeAutoPlace::unhook}, {CreativeTakeAmount::hook, CreativeTakeAmount::unhook},
    {CreativeTakeHalf::hook, CreativeTakeHalf::unhook}, {CreativeTakePlace::hook, CreativeTakePlace::unhook},
    {CreativeResponse::hook, CreativeResponse::unhook},
};
}
void start() {
    try {
        for (auto& hook : hooks) if (!hook.installed) {
            if (hook.install(true) == 0) hook.installed = true;
            else throw std::runtime_error("Could not install creative diagnostics");
        }
        if (!renderListener)
            renderListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) {
                try { seeScreen(event); } catch (...) {}
            });
        Runtime::instance().self().getLogger().warn("L-360 creative diagnostics enabled (F6-F9 act on Lamium's hovered item)");
    } catch (...) { stop(); throw; }
}
void stop() {
    if (renderListener) { ll::event::EventBus::getInstance().removeListener(renderListener); renderListener.reset(); }
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
}
}
#else
namespace lamium::researchTrace::creative { void start() {} void stop() {} }
#endif

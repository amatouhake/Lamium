#include "features/inventory/ToolSwitch.h"
#include "features/interaction/BreakingRestriction.h"
#include "features/inventory/ToolChoice.h"
#include "features/inventory/EquipmentPlan.h"
#include "features/inventory/FetchSlot.h"
#include "features/inventory/RestockUse.h"
#include "features/inventory/game/InventoryMove.h"
#include "app/Runtime.h"
#include "ui/SettingsScreen.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/enchanting/EnchantmentInstance.h"
#include "mc/world/item/enchanting/ItemEnchants.h"
#include "mc/deps/core/math/IRandom.h"
#include "mc/world/level/block/ResourceDrops.h"
#include "mc/world/level/block/ResourceDropsContext.h"
#include "mc/world/level/block/IResourceDropsStrategy.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace lamium::inventory::tools {
namespace {
using Clock = std::chrono::steady_clock;
ToolTarget target;
// L-69: a fetch waits until the last break is this far behind, so the move
// reaches the server after that break's durability change (L-66 ordering).
bool fetchPending = false;
Clock::time_point lastBreak{};
bool quietSinceBreak() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - lastBreak).count() >= restockQuietMs;
}
// L-318: what a block drops plainly, with Silk Touch and with Fortune III, from
// the game's own drop strategy. The query builds item stacks, so the answer is
// cached per block type. A fixed random keeps the answers stable; the Fortune
// pair uses a high one so a bonus roll shows in the count.
struct DropRandom : IRandom {
    bool high = false;
    explicit DropRandom(bool high) : high(high) {}
    int nextInt() override { return high ? 0x7fffffff : 0; }
    int nextInt(int bound) override { return high && bound > 0 ? bound - 1 : 0; }
    int64 nextLong() override { return 0; }
    bool nextBoolean() override { return high; }
    float nextFloat() override { return high ? 0.999f : 0.f; }
    double nextDouble() override { return high ? 0.999 : 0.5; }
    double nextGaussianDouble() override { return 0.0; }
    void consumeCount(uint) override {}
};
std::vector<std::string> dropNames(ResourceDrops const& drops) {
    std::vector<std::string> names;
    for (auto const& stack : drops.mItems.get())
        names.push_back(stack.getName() + " x" + std::to_string(static_cast<int>(stack.mCount)));
    return names;
}
struct Enchants { bool silk = false; int fortune = 0; };
Enchants enchantsOf(ItemStack const& stack) {
    Enchants found;
    if (stack.isNull()) return found;
    for (auto const& enchant : stack.constructItemEnchantsFromUserData().getAllEnchants()) {
        if (enchant.mEnchantType == Enchant::Type::SilkTouch) found.silk = true;
        if (enchant.mEnchantType == Enchant::Type::Fortune) found.fortune = std::max(found.fortune, static_cast<int>(enchant.mLevel));
    }
    return found;
}
ItemStack enchanted(ItemStack const& plain, Enchant::Type type, int level) {
    ItemStack stack = plain;
    ItemEnchants enchants;
    enchants.mSlot = 0;
    EnchantmentInstance instance;
    instance.mEnchantType = type;
    instance.mLevel = level;
    (*enchants.mItemEnchants)[0].push_back(instance);
    stack.saveEnchantsToUserData(enchants);
    return stack;
}
BlockDrops blockDrops(Block const& block, Player& player) {
    static std::unordered_map<BlockType const*, BlockDrops> cache;
    auto const& type = block.getBlockType();
    if (auto found = cache.find(&type); found != cache.end()) return found->second;
    if (!type.mResourceDropsStrategy) return {};
    auto& inventory = player.getInventory();
    for (int slot = 0; slot < 36; ++slot) {
        auto const& base = inventory.getItem(slot);
        if (base.isNull() || !base.mItem) continue;
        try {
            ItemStack plain;
            plain.reinit(*base.mItem, 1, 0);
            auto touched = enchanted(plain, Enchant::Type::SilkTouch, 1);
            auto lucky = enchanted(plain, Enchant::Type::Fortune, 3);
            BlockPos at{0, 0, 0};
            auto& region = player.getDimensionBlockSource();
            auto dimension = region.getDimensionId();
            auto drops = [&](ItemStack const& used, bool high) {
                DropRandom random{high};
                ResourceDropsContext context{ResourceDropsCause::PlayerMining, 0.f, used, at, dimension, region};
                return dropNames(type.mResourceDropsStrategy->getResourceDrops(block, random, context));
            };
            BlockDrops answer{scopeOfDrops(drops(plain,false), drops(touched,false)),
                              dropsDiffer(drops(plain,true), drops(lucky,true))};
            cache.emplace(&type, answer);
            return answer;
        } catch (...) { return {}; }
    }
    return {}; // Nothing to test with; asked again next time.
}
// L-318: the slot that broke a block last, for ties between equal tools.
ToolUse toolUse;
enum class Choice { None, Selected, Fetched, Wait };
Choice selectTool(Player& player, BlockPos const& pos) {
    if (!interaction::breaking::allows(player,pos)) return Choice::None;
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.snapshot()->inventory.toolSwitch || ui::ownsInput()) return Choice::None;
    auto client = ll::service::getClientInstance();
    if (!client || client->getLocalPlayer() != &player || player.isCreative() || player.isSpectator()) return Choice::None;
    auto* supplies = player.mInventory.get();
    if (!supplies || supplies->mSelectedContainerId != ContainerID::Inventory) return Choice::None;
    int selected = supplies->mSelected;
    if (selected < 0 || selected >= 9) return Choice::None;
    auto const& block = player.getDimensionBlockSource().getBlock(pos);
    bool requiresTool = block.getBlockType().mRequiresCorrectToolForDrops;
    bool fetch = runtime.snapshot()->inventory.toolSwitchInventory;
    // L-318: what the block asks of the tool, from its drop list and the priority.
    auto want = wantFor(blockDrops(block,player), static_cast<DropPriority>(runtime.snapshot()->inventory.toolSwitchDrop));
    std::array<ToolCandidate,36> candidates;
    for (int slot=0; slot<(fetch ? 36 : 9); ++slot) {
        auto const& stack = player.getInventory().getItem(slot);
        if (stack.isNull() || !stack.mItem) continue;
        auto enchants = want != Want::Speed ? enchantsOf(stack) : Enchants{};
        candidates[slot] = {stack.mItem->getDestroySpeed(stack,block),
            !requiresTool || stack.mItem->canDestroySpecial(block),
            enchants.silk, enchants.fortune, toolUse.last(slot, stack.mItem.get())};
        // Never fetch a tool that is about to break (L-62 would swap it back).
        if (slot >= 9 && aboutToBreak(stack.mItem->getMaxDamage(),stack.getDamageValue())) candidates[slot] = {};
    }
    std::array<ToolCandidate,9> hotbar;
    std::copy_n(candidates.begin(),9,hotbar.begin());
    if (auto slot = chooseHotbarTool(hotbar,selected,want)) {
        supplies->selectSlot(*slot,ContainerID::Inventory);
        return Choice::Selected;
    }
    if (!fetch) return Choice::None;
    auto source = chooseInventoryTool(candidates,selected,want);
    if (!source) return Choice::None;
    if (!quietSinceBreak()) return Choice::Wait;
    auto* local = client->getLocalPlayer();
    auto prefs = runtime.preferences();
    int slot = fetchSlot(selected, prefs.inventory.toolSwitchSlot,
        prefs.inventory.fakeOffhand ? std::optional<int>(prefs.inventory.fakeOffhandSlot - 1) : std::nullopt);
    ItemStack held = player.getInventory().getItem(slot), tool = player.getInventory().getItem(*source);
    if (!game::movePair(*local,{game::Place::Inventory,slot},tool,{game::Place::Inventory,*source},held)) return Choice::None;
    if (slot != selected) supplies->selectSlot(slot,ContainerID::Inventory);
    return Choice::Fetched;
}
bool clientPlayer(Player const& player) {
    auto client = ll::service::getClientInstance();
    return client && client->getLocalPlayer() == &player;
}
Choice choose(Player& player, BlockPos const& pos, bool starting) {
    // In a local world the integrated server's player breaks the same blocks,
    // possibly on another thread; only the client's own player is tracked.
    if (!clientPlayer(player)) return Choice::None;
    try {
        bool moved = target.enter({pos.x, pos.y, pos.z});
        if (starting || moved || fetchPending) {
            auto choice = selectTool(player,pos);
            fetchPending = choice == Choice::Wait;
            return choice;
        }
    } catch (std::exception const& error) {
        fetchPending = false;
        static bool reported = false;
        if (!reported) { Runtime::instance().self().getLogger().error("Tool selection failed: {}",error.what()); reported = true; }
    }
    return Choice::None;
}
}
interaction::mining::Gate startGate(Player& player, BlockPos const& pos) {
    choose(player,pos,true); // A waiting fetch is taken up by the continued breaking.
    return interaction::mining::Gate::Proceed;
}
// Holding the attack button across blocks continues breaking on the new block
// without a new start, so choose again when the position changes. A fetch
// right after a break pauses progress until it can be sent, then restarts.
interaction::mining::Gate continueGate(Player& player, BlockPos const& pos) {
    switch (choose(player,pos,false)) {
    case Choice::Wait: return interaction::mining::Gate::Pause;
    case Choice::Fetched: return interaction::mining::Gate::Restart;
    default: return interaction::mining::Gate::Proceed;
    }
}
void mined(bool destroyed) {
    if (!destroyed) return;
    lastBreak = Clock::now();
    try {
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        auto* supplies = player ? player->mInventory.get() : nullptr;
        if (!supplies || supplies->mSelectedContainerId != ContainerID::Inventory) return;
        int selected = supplies->mSelected;
        if (selected < 0 || selected >= 9) return;
        auto const& stack = player->getInventory().getItem(selected);
        if (!stack.isNull() && stack.mItem) toolUse.mark(selected, stack.mItem.get());
    } catch (...) {}
}
void stopped() { if (!fetchPending) target.clear(); }
void start() {}
void stop() {
    target.clear();
    toolUse.clear();
    fetchPending = false;
}
}

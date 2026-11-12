#include "features/inventory/EnchantOrderGame.h"
#include "app/Capabilities.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "features/inspection/render/ItemIcon.h"
#include "ui/Localization.h"
#include "ui/Widgets.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/world/containers/managers/controllers/AnvilContainerManagerController.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/enchanting/Enchant.h"
#include "mc/world/item/enchanting/EnchantResult.h"
#include "mc/world/item/enchanting/EnchantUtils.h"
#include "mc/world/item/enchanting/EnchantmentInstance.h"
#include "mc/world/item/enchanting/ItemEnchants.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <string_view>

namespace lamium::inventory::enchantOrder::game {
namespace {
using enchantPlan::Goal;
using enchantPlan::Plan;
// One item per enchant slot: what goes on an item follows the slot, not the
// material. An id the game does not have, or a slot seen before, is skipped.
constexpr std::array<std::string_view, 24> kindItems{
    "minecraft:diamond_sword", "minecraft:diamond_pickaxe", "minecraft:diamond_axe", "minecraft:diamond_shovel",
    "minecraft:diamond_hoe", "minecraft:bow", "minecraft:crossbow", "minecraft:trident", "minecraft:mace",
    "minecraft:diamond_spear", "minecraft:iron_spear", "minecraft:diamond_helmet", "minecraft:diamond_chestplate",
    "minecraft:diamond_leggings", "minecraft:diamond_boots", "minecraft:fishing_rod", "minecraft:elytra", "minecraft:shield",
    "minecraft:shears", "minecraft:flint_and_steel", "minecraft:brush", "minecraft:carrot_on_a_stick",
    "minecraft:warped_fungus_on_a_stick", "minecraft:compass"};
constexpr std::string_view bookItem = "minecraft:enchanted_book";

struct EnchantInfo { int type = 0, maxLevel = 1, frequency = 0; };
struct Kind {
    int slot = 0;
    std::string name;
    std::vector<Offer> offers; // in the game's order, curses last
    ItemStack icon;
};
struct Held {
    bool present = false, stateKnown = false;
    int kind = -1, work = 0;
    std::string name;
    std::vector<Enchantment> base;
    ItemStack icon;
};
// Read at open, dropped at close: the stacks point at the world's items.
std::vector<EnchantInfo> enchants;
std::vector<Kind> kinds;
Conflicts conflicts;
Held held;
ItemStack book;
bool opened = false;
// The session's choices; values only, kept after the screen closes.
int chosenSlot = 0;
bool useHeldState = true, creativeNow = false;
Goal goal = Goal::FewestLevels;
std::vector<Enchantment> wanted;
std::map<int, std::vector<Enchantment>> lastBySlot;
View shown;
// The plan the anvil list follows.
struct Active {
    bool valid = false, creative = false;
    int slot = 0;
    Goal goal = Goal::FewestLevels;
    std::string itemId, itemName;
    std::vector<Enchantment> base, wanted;
    Plan plan;
    std::vector<StepLine> lines;
} active;
ItemStack anvilItemIcon, anvilBookIcon;
bool anvilIconsMade = false;
ll::event::ListenerPtr renderListener, exitListener;

bool curse(int type) {
    return type == static_cast<int>(Enchant::Type::CurseOfBinding) || type == static_cast<int>(Enchant::Type::CurseOfVanishing);
}
// Formatting codes are not for Lamium's labels.
std::string plain(std::string text) {
    for (size_t at = 0; (at = text.find("\xC2\xA7", at)) != std::string::npos;) text.erase(at, std::min<size_t>(3, text.size() - at));
    return text;
}
std::string nameOf(int type, int level) {
    try { return plain(EnchantUtils::getEnchantNameAndLevel(static_cast<Enchant::Type>(type), level)); } catch (...) { return {}; }
}
ItemStack stackOf(std::string_view id) {
    ItemStack stack;
    try { stack.reinit(id, 1, 0); } catch (...) { stack = ItemStack(); }
    stack.mShowPickUp = false;
    stack.mWasPickedUp = false;
    return stack;
}
std::vector<Enchantment> enchantsOn(ItemStack const& stack) {
    std::vector<Enchantment> out;
    try {
        for (auto const& e : stack.constructItemEnchantsFromUserData().getAllEnchants())
            out.push_back({static_cast<int>(static_cast<Enchant::Type>(e.mEnchantType)), static_cast<int>(e.mLevel)});
    } catch (...) { out.clear(); }
    return out;
}
bool goesOn(ItemStack const& stack, EnchantInfo const& info) {
    EnchantmentInstance wantedOne;
    wantedOne.mEnchantType = static_cast<Enchant::Type>(info.type);
    wantedOne.mLevel = info.maxLevel;
    return static_cast<EnchantResultType>(EnchantUtils::canEnchant(stack, wantedOne, false).result) != EnchantResultType::Fail;
}
bool readCatalog() {
    enchants.clear(); kinds.clear(); conflicts.pairs.clear();
    auto const& registry = Enchant::mEnchants();
    for (auto const& enchant : registry) {
        if (!enchant || static_cast<bool>(enchant->mIsDisabled)) continue;
        enchants.push_back({static_cast<int>(static_cast<Enchant::Type>(enchant->mEnchantType)), std::max(1, enchant->getMaxLevel()),
            static_cast<int>(static_cast<Enchant::Frequency>(enchant->mFrequency))});
    }
    for (auto const& a : registry) {
        if (!a) continue;
        for (auto const& b : registry) {
            if (!b || a.get() == b.get()) continue;
            int const x = static_cast<int>(static_cast<Enchant::Type>(a->mEnchantType)),
                      y = static_cast<int>(static_cast<Enchant::Type>(b->mEnchantType));
            if (!a->isCompatibleWith(static_cast<Enchant::Type>(y)) && !conflicts.between(x, y)) conflicts.pairs.emplace_back(x, y);
        }
    }
    for (auto id : kindItems) {
        ItemStack stack = stackOf(id);
        if (stack.isNull() || !stack.mItem) continue;
        int const slot = stack.mItem->getEnchantSlot();
        if (slot == 0 || std::ranges::any_of(kinds, [&](Kind const& k) { return k.slot == slot; })) continue;
        Kind kind{slot, {}, {}, std::move(stack)};
        for (bool curses : {false, true})
            for (auto const& info : enchants)
                if (curse(info.type) == curses && goesOn(kind.icon, info)) kind.offers.push_back({info.type, info.maxLevel, info.frequency});
        if (kind.offers.empty()) continue;
        try { kind.name = plain(kind.icon.getName()); } catch (...) {}
        kinds.push_back(std::move(kind));
    }
    return !enchants.empty() && !kinds.empty();
}
void readHeld(LocalPlayer& player) {
    held = {};
    ItemStack const& stack = player.getSelectedItem();
    if (stack.isNull() || !stack.mItem) return;
    int const slot = stack.mItem->getEnchantSlot();
    auto found = std::ranges::find_if(kinds, [&](Kind const& k) { return k.slot == slot; });
    if (slot == 0 || found == kinds.end()) return;
    held.present = true;
    held.kind = static_cast<int>(found - kinds.begin());
    held.base = enchantsOn(stack);
    try { held.name = plain(stack.getName()); } catch (...) {}
    try {
        if (auto uses = enchantPlan::priorUses(stack.getBaseRepairCost())) { held.work = *uses; held.stateKnown = true; }
    } catch (...) {}
    held.icon = ItemStack(stack);
    held.icon.mShowPickUp = false;
    held.icon.mWasPickedUp = false;
}
int chosenKind() {
    auto found = std::ranges::find_if(kinds, [&](Kind const& k) { return k.slot == chosenSlot; });
    return found == kinds.end() ? -1 : static_cast<int>(found - kinds.begin());
}
bool usingHeld(int kind) { return held.present && held.kind == kind && held.stateKnown && useHeldState; }
void remember() { if (chosenSlot && !wanted.empty()) lastBySlot[chosenSlot] = wanted; }
void rebuild() {
    shown = {};
    shown.goal = goal;
    shown.creative = creativeNow;
    for (size_t i = 0; i < kinds.size(); ++i) shown.kinds.push_back({kinds[i].name, held.present && held.kind == static_cast<int>(i)});
    int const kind = chosenKind();
    shown.kind = kind;
    if (kind < 0) { active = {}; return; }
    auto const& chosen = kinds[static_cast<size_t>(kind)];
    bool const fromHeld = usingHeld(kind);
    shown.inHand = held.present && held.kind == kind;
    shown.stateKnown = held.stateKnown;
    shown.useHeld = fromHeld;
    shown.itemName = fromHeld && !held.name.empty() ? held.name : chosen.name;
    std::vector<Enchantment> const base = fromHeld ? held.base : std::vector<Enchantment>{};
    prune(wanted, chosen.offers, base, conflicts);
    shown.wanted = static_cast<int>(wanted.size());
    shown.hasLast = lastBySlot.contains(chosenSlot);
    // What the item has first, then what can be added.
    for (auto const& e : base) shown.rows.push_back({e.type, e.level, e.level, nameOf(e.type, e.level), {}, RowState::Fixed});
    for (auto const& offer : chosen.offers) {
        if (has(base, offer.type)) continue;
        Row row{offer.type, offer.maxLevel, offer.maxLevel, {}, {}, RowState::Off};
        if (auto on = std::ranges::find_if(wanted, [&](Enchantment const& e) { return e.type == offer.type; }); on != wanted.end()) {
            row.level = on->level;
            row.state = RowState::On;
        } else if (int by = blocker(offer.type, base, wanted, conflicts); by >= 0) {
            row.state = RowState::Blocked;
            row.reason = nameOf(by, 1);
        } else if (static_cast<int>(wanted.size()) >= maxBooks) row.state = RowState::Full;
        row.label = nameOf(offer.type, row.level);
        shown.rows.push_back(std::move(row));
    }
    active = {};
    if (wanted.empty()) return;
    auto made = inputs(wanted, chosen.offers, fromHeld ? held.work : 0);
    if (!made) { shown.costUnknown = true; return; }
    int const limit = creativeNow ? 0 : 40;
    auto mine = enchantPlan::plan(*made, {limit, goal});
    auto other = enchantPlan::plan(*made, {limit, goal == Goal::FewestLevels ? Goal::LeastPriorWork : Goal::FewestLevels});
    if (!mine) { shown.refused = true; return; }
    auto side = [&](unsigned mask) {
        if (mask & 1u) return shown.itemName;
        std::string text;
        for (size_t i = 0; i < wanted.size(); ++i) {
            if (!(mask >> (i + 1) & 1u)) continue;
            if (!text.empty()) text += ", ";
            text += nameOf(wanted[i].type, wanted[i].level);
        }
        return text;
    };
    for (auto const& step : mine->steps) {
        // The step's cost is the sacrificed side's levels plus both surcharges.
        int value = 0;
        for (size_t i = 0; i < made->size(); ++i) if (step.sacrifice >> i & 1u) value += (*made)[i].value;
        shown.steps.push_back({side(step.target), side(step.sacrifice), (step.target & 1u) != 0, step.cost, step.cost - value});
    }
    shown.total = mine->total; shown.highest = mine->highest; shown.work = mine->work;
    if (other) {
        shown.otherKnown = true;
        shown.otherTotal = other->total; shown.otherWork = other->work;
        shown.sameForBoth = other->total == mine->total && other->work == mine->work;
    }
    active.valid = true;
    active.creative = creativeNow;
    active.slot = chosen.slot;
    active.goal = goal;
    try { active.itemId = chosen.icon.getTypeName(); } catch (...) {}
    active.itemName = shown.itemName;
    active.base = base;
    active.wanted = wanted;
    active.plan = *mine;
    active.lines = shown.steps;
    anvilIconsMade = false;
}

// The step list beside the anvil screen. It takes no input.
std::optional<unsigned> sideOf(ItemStack const& stack) {
    if (stack.isNull() || !stack.mItem) return std::nullopt;
    bool const isBook = stack.getTypeName() == bookItem;
    bool const isItem = !isBook && stack.mItem->getEnchantSlot() == active.slot;
    if (!isBook && !isItem) return std::nullopt;
    return enchantPlan::carried(isItem, enchantsOn(stack), active.base, active.wanted);
}
void drawBeside(ll::event::AfterUIRenderEvent& event) {
    if (!active.valid || active.lines.empty()) return;
    auto& view = event.screenView();
    if (!view.mHasFocus || !view.mController || !view.mController->_isContainerScreen()) return;
    if (view.getScreenName() != "anvil_screen") return;
    if (!versionSensitiveAllowed(capabilities::Id::EnchantOrder)) return;
    auto* tree = view.mVisualTree.get();
    if (!tree) return;
    static std::string const windowName = "root_panel";
    auto window = tree->getControlByName(windowName, true);
    if (!window || window->mCachedPositionDirty) return;
    glm::vec2 const at = *window->mCachedPosition, size = *window->mSize, screen = *view.mSize;
    if (size.x <= 0 || size.y <= 0 || size.x >= screen.x - 1) return; // not laid out yet
    using namespace lamium::ui;
    constexpr float gap = 4, pad = 4, line = 10, wide = 190, narrow = 110;
    float width = std::min(wide, screen.x - (at.x + size.x) - 2 * gap), left = at.x + size.x + gap;
    if (width < narrow) {
        width = std::min(wide, at.x - 2 * gap);
        left = at.x - gap - width;
    }
    if (width < narrow) return;

    auto& context = event.uiRenderContext();
    int now = -1, gameCost = -1;
    bool both = false;
    try {
        auto& screenController = static_cast<ContainerScreenController&>(*view.mController);
        if (auto manager = screenController.mContainerManagerController; manager && !manager->mContainersClosed) {
            auto const& input = manager->getItemStack("anvil_input_items", 0);
            auto const& material = manager->getItemStack("anvil_material_items", 0);
            both = !input.isNull() && !material.isNull();
            if (both) {
                auto target = sideOf(input), sacrifice = sideOf(material);
                if (target && sacrifice) now = enchantPlan::stepFor(active.plan, *target, *sacrifice);
                gameCost = static_cast<AnvilContainerManagerController&>(*manager).mCost;
            }
        }
    } catch (...) { now = -1; both = false; }

    int const steps = static_cast<int>(active.lines.size());
    bool const note = both; // a line under the total says what the slots are
    auto heightFor = [&](bool twoLines) { return pad + line + 2 + steps * (twoLines ? 2 * line + 1 : line + 1) + 2 + line + (note ? line : 0) + pad; };
    bool twoLines = heightFor(true) <= screen.y - 2 * gap;
    float const height = heightFor(twoLines);
    if (height > screen.y - 2 * gap) return;
    float const top = std::clamp(at.y, gap, screen.y - gap - height);
    if (!anvilIconsMade) {
        anvilItemIcon = stackOf(active.itemId);
        anvilBookIcon = stackOf(bookItem);
        anvilIconsMade = true;
    }
    panel(context, left, top, width, height, .8f);
    float y = top + pad;
    std::string const goalName = translated(active.goal == Goal::FewestLevels ? "enchantOrder.goal.levels" : "enchantOrder.goal.work");
    float const goalWidth = textWidth(context, goalName) + 2;
    label(context, left + pad, y, width - 2 * pad - goalWidth, translated("feature.enchantOrder"));
    label(context, left + width - pad - goalWidth, y, goalWidth, goalName, palette::faint, Align::Right);
    y += line + 2;
    std::vector<inspection::render::IconAt> icons;
    constexpr float icon = 8, numberWidth = 10, costWidth = 16;
    auto sideAt = [&](float x, float rowY, float w, std::string const& text, bool item) {
        ItemStack const* stack = item ? &anvilItemIcon : &anvilBookIcon;
        if (!stack->isNull()) icons.push_back({stack, std::round(x), std::round(rowY), icon / 16});
        label(context, x + icon + 2, rowY, w - icon - 2, text);
    };
    for (int i = 0; i < steps; ++i) {
        auto const& step = active.lines[static_cast<size_t>(i)];
        float const rowHeight = twoLines ? 2 * line : line;
        if (i == now) {
            fill(context, left + 1, y - 1, width - 2, rowHeight + 1, palette::accent, .16f);
            fill(context, left + 1, y - 1, 2, rowHeight + 1, palette::accent);
        }
        label(context, left + pad, y, numberWidth, std::to_string(i + 1), palette::faint, Align::Right);
        float const textLeft = left + pad + numberWidth + 3, textWidthAll = width - 2 * pad - numberWidth - 3 - costWidth;
        if (twoLines) {
            sideAt(textLeft, y, textWidthAll, step.kept, step.keptIsItem);
            label(context, textLeft - 1, y + line, 6, "+", palette::faint);
            sideAt(textLeft + 6, y + line, textWidthAll - 6, step.gone, false);
        } else {
            float const half = (textWidthAll - 8) / 2;
            sideAt(textLeft, y, half, step.kept, step.keptIsItem);
            label(context, textLeft + half + 1, y, 6, "+", palette::faint);
            sideAt(textLeft + half + 8, y, half, step.gone, false);
        }
        label(context, left + width - pad - costWidth, y, costWidth, std::to_string(step.cost), i == now ? palette::accent : palette::text, Align::Right);
        y += rowHeight + 1;
    }
    y += 2;
    label(context, left + pad, y, width - 2 * pad, translated("enchantOrder.total", active.plan.total), palette::dim);
    if (note) {
        y += line;
        if (now < 0) label(context, left + pad, y, width - 2 * pad, translated("enchantOrder.anvil.notInPlan"), palette::faint);
        else if (gameCost != active.lines[static_cast<size_t>(now)].cost)
            label(context, left + pad, y, width - 2 * pad, translated("enchantOrder.anvil.costDiffers", gameCost), palette::warning);
        else label(context, left + pad, y, width - 2 * pad, translated("enchantOrder.anvil.match", now + 1), palette::accent);
    }
    context.flushText(0, std::nullopt);
    if (!icons.empty()) inspection::render::drawItemIcons(context, icons, 17);
}
void dropStacks() {
    kinds.clear();
    held = {};
    book = ItemStack();
    anvilItemIcon = ItemStack();
    anvilBookIcon = ItemStack();
    anvilIconsMade = false;
    opened = false;
}
}

bool open(IClientInstance& client, std::string& problem) {
    close();
    auto* player = client.getLocalPlayer();
    if (!player) { problem = "worldOnly"; return false; }
    if (!versionSensitiveAllowed(capabilities::Id::EnchantOrder)) { problem = "enchantOrder.unverified"; return false; }
    try {
        if (!readCatalog()) { dropStacks(); problem = "enchantOrder.noCatalog"; return false; }
        readHeld(*player);
        creativeNow = player->isCreative();
        book = stackOf(bookItem);
    } catch (...) { dropStacks(); problem = "enchantOrder.noCatalog"; return false; }
    opened = true;
    // The main hand's kind when it can be enchanted, else the kind left last.
    if (held.present) {
        int const slot = kinds[static_cast<size_t>(held.kind)].slot;
        if (slot != chosenSlot) { remember(); wanted.clear(); chosenSlot = slot; }
    } else if (chosenKind() < 0) { wanted.clear(); chosenSlot = kinds.front().slot; }
    rebuild();
    return true;
}
void close() {
    if (!opened) return;
    remember();
    dropStacks();
    shown = {};
}
View const& view() { return shown; }
void pickKind(int index) {
    if (!opened || index < 0 || index >= static_cast<int>(kinds.size())) return;
    int const slot = kinds[static_cast<size_t>(index)].slot;
    if (slot == chosenSlot) return;
    remember();
    wanted.clear();
    chosenSlot = slot;
    rebuild();
}
void setUseHeld(bool use) {
    if (!opened || useHeldState == use) return;
    useHeldState = use;
    rebuild();
}
Change toggleRow(int row) {
    if (!opened || row < 0 || row >= static_cast<int>(shown.rows.size())) return Change::Blocked;
    int const kind = chosenKind();
    if (kind < 0) return Change::Blocked;
    auto const& offers = kinds[static_cast<size_t>(kind)].offers;
    int const type = shown.rows[static_cast<size_t>(row)].type;
    auto offer = std::ranges::find_if(offers, [&](Offer const& o) { return o.type == type; });
    if (offer == offers.end()) return Change::Fixed;
    auto change = toggle(wanted, *offer, usingHeld(kind) ? held.base : std::vector<Enchantment>{}, conflicts);
    rebuild();
    return change;
}
void stepRow(int row, int delta) {
    if (!opened || row < 0 || row >= static_cast<int>(shown.rows.size())) return;
    int const kind = chosenKind();
    if (kind < 0) return;
    auto const& offers = kinds[static_cast<size_t>(kind)].offers;
    int const type = shown.rows[static_cast<size_t>(row)].type;
    auto offer = std::ranges::find_if(offers, [&](Offer const& o) { return o.type == type; });
    if (offer != offers.end() && step(wanted, *offer, delta)) rebuild();
}
void setAllHighest() {
    int const kind = chosenKind();
    if (!opened || kind < 0) return;
    remember();
    wanted = allHighest(kinds[static_cast<size_t>(kind)].offers, usingHeld(kind) ? held.base : std::vector<Enchantment>{}, conflicts);
    rebuild();
}
void restoreLast() {
    auto found = lastBySlot.find(chosenSlot);
    if (!opened || found == lastBySlot.end()) return;
    auto previous = found->second;
    remember();
    wanted = std::move(previous);
    rebuild();
}
void clearWanted() {
    if (!opened || wanted.empty()) return;
    remember();
    wanted.clear();
    rebuild();
}
void flipGoal() {
    goal = goal == Goal::FewestLevels ? Goal::LeastPriorWork : Goal::FewestLevels;
    if (opened) rebuild();
}
ItemStack const* kindIcon(int index) {
    if (index < 0 || index >= static_cast<int>(kinds.size()) || kinds[static_cast<size_t>(index)].icon.isNull()) return nullptr;
    return &kinds[static_cast<size_t>(index)].icon;
}
ItemStack const* itemIcon() {
    int const kind = chosenKind();
    if (kind < 0) return nullptr;
    if (usingHeld(kind) && !held.icon.isNull()) return &held.icon;
    return kindIcon(kind);
}
ItemStack const* bookIcon() { return book.isNull() ? nullptr : &book; }

bool start() {
    auto& bus = ll::event::EventBus::getInstance();
    renderListener = bus.emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) { try { drawBeside(event); } catch (...) {} });
    exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { dropStacks(); shown = {}; });
    return true;
}
void stop() {
    auto& bus = ll::event::EventBus::getInstance();
    for (auto* listener : {&renderListener, &exitListener})
        if (*listener) { bus.removeListener(*listener); listener->reset(); }
    dropStacks();
    shown = {};
    active = {};
}
}

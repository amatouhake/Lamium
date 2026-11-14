#include "features/inventory/EnchantOrderGame.h"
#include "app/Capabilities.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "features/inspection/Inspection.h"
#include "features/inspection/render/DurabilityBar.h"
#include "features/inspection/render/ItemIcon.h"
#include "ui/EnchantChooser.h"
#include "ui/EnchantLayout.h"
#include "ui/EnchantSteps.h"
#include "ui/Localization.h"
#include "ui/Widgets.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontRepository.h"
#include "mc/client/gui/TextAlignment.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/input/RectangleArea.h"
#include "ui/GameFont.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/locale/I18n.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
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
#include <bit>
#include <chrono>
#include <mutex>
#include <cmath>
#include <format>
#include <map>
#include <optional>
#include <span>
#include <string_view>

namespace lamium::inventory::enchantOrder::game {
namespace {
using enchantPlan::Goal;
using enchantPlan::Plan;
// One item per enchant slot: what goes on an item follows the slot, not the
// material. An id the game does not have, or a slot seen before, is skipped.
// A kind is named by the game's own creative group where it has one
// ("Swords"), else by the item ("Bow"), so the words are the game's in
// every language.
struct KindItem { std::string_view id, group; };
constexpr std::array<KindItem, 27> kindItems{{
    {"minecraft:diamond_sword", "itemGroup.name.sword"}, {"minecraft:diamond_pickaxe", "itemGroup.name.pickaxe"},
    {"minecraft:diamond_axe", "itemGroup.name.axe"}, {"minecraft:diamond_shovel", "itemGroup.name.shovel"},
    {"minecraft:diamond_hoe", "itemGroup.name.hoe"}, {"minecraft:bow", ""}, {"minecraft:crossbow", ""}, {"minecraft:trident", ""},
    {"minecraft:mace", ""}, {"minecraft:diamond_spear", "itemGroup.name.spear"}, {"minecraft:iron_spear", "itemGroup.name.spear"},
    {"minecraft:diamond_helmet", "itemGroup.name.helmet"}, {"minecraft:diamond_chestplate", "itemGroup.name.chestplate"},
    {"minecraft:diamond_leggings", "itemGroup.name.leggings"}, {"minecraft:diamond_boots", "itemGroup.name.boots"},
    {"minecraft:fishing_rod", ""}, {"minecraft:elytra", ""}, {"minecraft:shield", ""}, {"minecraft:shears", ""},
    {"minecraft:flint_and_steel", ""}, {"minecraft:brush", ""}, {"minecraft:carrot_on_a_stick", ""},
    {"minecraft:warped_fungus_on_a_stick", ""}, {"minecraft:compass", ""}, {"minecraft:carved_pumpkin", ""},
    {"minecraft:skeleton_skull", "itemGroup.name.skull"}, {"minecraft:enchanted_book", "itemGroup.name.enchantedBook"}}};
constexpr std::string_view bookItem = "minecraft:enchanted_book";

struct EnchantInfo { int type = 0, maxLevel = 1, frequency = 0, reach = 0; };
// Books alone (several enchanted books merged into one) have no enchant
// slot of their own; the kind uses this one.
constexpr int bookSlot = -1;
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
// The items offered as cells: the main hand's, and the anvil's left slot's while the anvil's panel is open.
Held held, slotItem;
ItemStack book;
bool opened = false, anvilOpened = false;
// The session's choices; values only, kept after the screen closes.
int chosenSlot = 0;
bool creativeNow = false;
// The item the plan is made from when it is not a fresh one of the kind: a record taken when its cell was
// chosen, so the plan stays the same after the item has left the hand or the slot.
struct PlanItem {
    bool use = false;
    int slot = 0, work = 0;
    std::string id, name;
    std::vector<Enchantment> base;
    bool fromSlot = false; // taken from the anvil slot's cell, not the main hand's
} planItem;
ItemStack planIcon; // rebuilt at open
Goal goal = Goal::FewestLevels;
std::vector<Enchantment> wanted;
std::map<int, std::vector<Enchantment>> lastBySlot;
View shown;
// The plan the anvil list follows.
struct Active {
    bool valid = false;
    int slot = 0;
    Goal goal = Goal::FewestLevels;
    std::string itemId, itemName;
    bool heldBook = false; // input 0 is an enchanted book the player held
    bool otherKnown = false, sameForBoth = false; // the other goal's outcome, for the goal's help
    int otherTotal = 0, otherWork = 0;
    std::vector<Enchantment> base, wanted;
    Plan plan;
    std::vector<StepLine> lines;
} active;
ItemStack anvilItemIcon, anvilBookIcon, anvilItemGlint;
bool anvilIconsMade = false;
// One stack per drawn icon: a stack drawn several times in a frame lost its glint, and the others' with it
// (seen 2026-11-12 with two or more steps; the container previews draw a stack per slot).
constexpr size_t iconPool = 48;
std::vector<ItemStack> anvilItems, anvilBooks, screenItems, anvilGlints;
ItemStack const* pooled(std::vector<ItemStack>& pool, ItemStack const& source, int number) {
    if (source.isNull() || number < 0 || static_cast<size_t>(number) >= iconPool) return source.isNull() ? nullptr : &source;
    // Reserved once: pointers into the pool are held for the frame and must not move as it grows.
    pool.reserve(iconPool);
    while (pool.size() <= static_cast<size_t>(number)) {
        pool.emplace_back(source);
        pool.back().mShowPickUp = false;
        pool.back().mWasPickedUp = false;
    }
    return &pool[static_cast<size_t>(number)];
}
std::chrono::steady_clock::time_point anvilSeen{};
bool finishedAtAnvil = false;
ll::event::ListenerPtr renderListener, exitListener, enterListener;

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
// The name without a level: the game only offers both together, so it is
// what the texts for level 1 and level 5 ("I" and "V") share.
std::string nameOnly(int type) {
    std::string const one = nameOf(type, 1), five = nameOf(type, 5);
    size_t same = 0;
    while (same < one.size() && same < five.size() && one[same] == five[same]) ++same;
    // Never end inside a multi-byte character.
    while (same > 0 && same < one.size() && (static_cast<unsigned char>(one[same]) & 0xC0) == 0x80) --same;
    std::string name = one.substr(0, same);
    while (!name.empty() && name.back() == ' ') name.pop_back();
    return name.empty() ? one : name;
}
std::string gameText(std::string_view key) {
    if (key.empty()) return {};
    try {
        std::string const id(key);
        std::string text = getI18n().get(id, getI18n().getCurrentLanguage());
        return text == id ? std::string{} : plain(std::move(text));
    } catch (...) { return {}; }
}
ItemStack stackOf(std::string_view id) {
    ItemStack stack;
    try { stack.reinit(id, 1, 0); } catch (...) { stack = ItemStack(); }
    stack.mShowPickUp = false;
    stack.mWasPickedUp = false;
    return stack;
}
// The item with one of the plan's enchantments on it, for its glint at the steps where it has one.
ItemStack enchantedCopy(ItemStack const& source, std::vector<Enchantment> const& from) {
    ItemStack out{source};
    if (out.isNull() || from.empty()) return out;
    EnchantmentInstance one;
    one.mEnchantType = static_cast<Enchant::Type>(from.front().type);
    one.mLevel = from.front().level;
    try { EnchantUtils::applyEnchant(out, one, true); } catch (...) { out = ItemStack{source}; }
    return out;
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
        auto const slots = static_cast<unsigned>(static_cast<int>(enchant->mPrimarySlots)) | static_cast<unsigned>(static_cast<int>(enchant->mSecondarySlots));
        enchants.push_back({static_cast<int>(static_cast<Enchant::Type>(enchant->mEnchantType)), std::max(1, enchant->getMaxLevel()),
            static_cast<int>(static_cast<Enchant::Frequency>(enchant->mFrequency)), std::popcount(slots)});
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
    for (auto const& item : kindItems) {
        ItemStack stack = stackOf(item.id);
        if (stack.isNull() || !stack.mItem) continue;
        bool const isBook = item.id == bookItem;
        int const slot = isBook ? bookSlot : stack.mItem->getEnchantSlot();
        if (slot == 0 || std::ranges::any_of(kinds, [&](Kind const& k) { return k.slot == slot; })) continue;
        Kind kind{slot, {}, {}, std::move(stack)};
        std::vector<Offer> found;
        for (auto const& info : enchants)
            if (goesOn(kind.icon, info)) found.push_back({info.type, info.maxLevel, info.frequency, curse(info.type), info.reach});
        // A book takes every enchantment; if the game's check says otherwise for the book item, offer them all.
        if (isBook && found.empty())
            for (auto const& info : enchants) found.push_back({info.type, info.maxLevel, info.frequency, curse(info.type), info.reach});
        if (found.empty()) continue;
        kind.offers = grouped(found, conflicts);
        kind.name = gameText(item.group);
        if (kind.name.empty()) try { kind.name = plain(kind.icon.getName()); } catch (...) {}
        kinds.push_back(std::move(kind));
    }
    return !enchants.empty() && !kinds.empty();
}
void readItem(Held& held, ItemStack const& stack) {
    held = {};
    if (stack.isNull() || !stack.mItem) return;
    // An enchanted book has no enchant slot of its own; it belongs to the book kind.
    // Only the enchanted book. A plain book has the same "everything" slot value and an anvil does enchant it
    // from an enchanted book (read in game 2026-11-12), but it is not offered: once through the anvil the list
    // could not tell it from the fresh books. Open in BACKLOG L-170.
    bool const isBook = stack.getTypeName() == bookItem;
    int const slot = isBook ? bookSlot : stack.mItem->getEnchantSlot();
    auto found = std::ranges::find_if(kinds, [&](Kind const& k) { return k.slot == slot; });
    if (slot == 0 || (slot == bookSlot && !isBook) || found == kinds.end()) return;
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
bool usingItem(int kind) { return planItem.use && kind >= 0 && kinds[static_cast<size_t>(kind)].slot == planItem.slot; }
// An offered item is the one the plan was made from: same kind, same enchantments, same anvil uses.
bool isPlanItem(Held const& item) {
    return planItem.use && item.present && item.stateKnown && kinds[static_cast<size_t>(item.kind)].slot == planItem.slot
        && item.work == planItem.work && sameSet(item.base, planItem.base);
}
void take(Held const& item) {
    planItem = {true, kinds[static_cast<size_t>(item.kind)].slot, item.work, {}, item.name, item.base, &item == &slotItem};
    try { planItem.id = item.icon.getTypeName(); } catch (...) {}
    planIcon = ItemStack(item.icon);
}
void remember() { if (chosenSlot && !wanted.empty()) lastBySlot[chosenSlot] = wanted; }
bool onlyCurses(std::vector<Enchantment> const& list, unsigned mask) {
    bool any = false;
    for (size_t k = 0; k < list.size(); ++k)
        if (mask >> (k + 1) & 1u) { if (!curse(list[k].type)) return false; any = true; }
    return any;
}
void rebuild() {
    screenItems.clear();
    shown = {};
    shown.goal = goal;
    shown.creative = creativeNow;
    for (size_t i = 0; i < kinds.size(); ++i) shown.kinds.push_back({kinds[i].name, held.present && held.kind == static_cast<int>(i)});
    int const kind = chosenKind();
    shown.kind = kind;
    if (kind < 0) { active = {}; return; }
    auto const& chosen = kinds[static_cast<size_t>(kind)];
    // A held book is input 0 like an item, but it may be the sacrificed side.
    bool const fromHeld = usingItem(kind), heldBook = fromHeld && chosen.slot == bookSlot, withItem = chosen.slot != bookSlot || heldBook;
    shown.inHand = held.present && held.kind == kind;
    shown.stateKnown = held.stateKnown;
    shown.useItem = fromHeld;
    // Two like items, one in the hand and one in the slot: only the cell that was pressed is the chosen one.
    // The other is when the pressed one no longer holds the item (it went from the hand into the slot, say).
    bool const handIs = fromHeld && isPlanItem(held), slotIs = fromHeld && isPlanItem(slotItem);
    shown.useHeld = handIs && (!planItem.fromSlot || !slotIs);
    shown.heldCell = held.present && held.stateKnown;
    shown.heldName = held.name;
    shown.useSlot = slotIs && (planItem.fromSlot || !handIs);
    shown.slotCell = slotItem.present && slotItem.stateKnown;
    shown.slotName = slotItem.name;
    shown.bookKind = !withItem;
    shown.itemName = fromHeld && !planItem.name.empty() ? planItem.name : chosen.name;
    std::vector<Enchantment> const base = fromHeld ? planItem.base : std::vector<Enchantment>{};
    // A held book's own enchantments are not raised: it may end up the sacrificed side, which was not read from the anvil.
    if (heldBook) std::erase_if(wanted, [&](Enchantment const& e) { return has(base, e.type); });
    prune(wanted, chosen.offers, base, conflicts);
    shown.wanted = static_cast<int>(wanted.size());
    if (auto last = lastBySlot.find(chosenSlot); last != lastBySlot.end()) shown.hasLast = last->second != wanted;
    // One order whether the held state is used or not: what the item has
    // stays in its place, fixed. Anything on it that the kind does not
    // offer comes last.
    for (auto const& offer : chosen.offers) {
        if (auto on = std::ranges::find_if(base, [&](Enchantment const& e) { return e.type == offer.type; }); on != base.end()) {
            // It stays on; a level above what the item has is a wanted book raising it.
            int const level = std::max(on->level, levelOf(wanted, on->type));
            Row fixed{on->type, level, heldBook ? on->level : std::max(on->level, offer.maxLevel), nameOf(on->type, level), {}, RowState::Fixed, offer.curse};
            fixed.name = nameOnly(on->type);
            fixed.carried = on->level;
            if (offer.maxLevel > 1 && fixed.label.size() > fixed.name.size() && fixed.label.starts_with(fixed.name)) {
                fixed.levelText = fixed.label.substr(fixed.name.size());
                while (!fixed.levelText.empty() && fixed.levelText.front() == ' ') fixed.levelText.erase(0, 1);
            }
            shown.rows.push_back(std::move(fixed));
            continue;
        }
        Row row{offer.type, offer.maxLevel, offer.maxLevel, {}, {}, RowState::Off, offer.curse};
        if (auto on = std::ranges::find_if(wanted, [&](Enchantment const& e) { return e.type == offer.type; }); on != wanted.end()) {
            row.level = on->level;
            row.state = RowState::On;
        } else if (int by = blocker(offer.type, base, wanted, conflicts); by >= 0) {
            row.state = RowState::Blocked;
            row.reason = nameOnly(by);
        } else if (static_cast<int>(wanted.size()) >= maxBooks) row.state = RowState::Full;
        row.label = nameOf(offer.type, row.level);
        row.name = nameOnly(offer.type);
        // The level alone is what the full text has after the name.
        if (offer.maxLevel > 1 && row.label.size() > row.name.size() && row.label.starts_with(row.name)) {
            row.levelText = row.label.substr(row.name.size());
            while (!row.levelText.empty() && row.levelText.front() == ' ') row.levelText.erase(0, 1);
        }
        shown.rows.push_back(std::move(row));
    }
    for (auto const& e : base)
        if (std::ranges::none_of(chosen.offers, [&](Offer const& o) { return o.type == e.type; }))
            shown.rows.push_back({e.type, e.level, e.level, nameOf(e.type, e.level), {}, RowState::Fixed, curse(e.type)});
    active = {};
    if (wanted.empty()) return;
    std::optional<int> heldValue;
    if (heldBook) {
        heldValue = bookValue(base, chosen.offers);
        if (!heldValue) { shown.costUnknown = true; return; }
    }
    auto made = inputs(wanted, chosen.offers, fromHeld ? planItem.work : 0, withItem, heldValue, base);
    if (!made) { shown.costUnknown = true; return; }
    int const limit = creativeNow ? 0 : 40;
    auto mine = enchantPlan::plan(*made, {limit, goal});
    auto other = enchantPlan::plan(*made, {limit, goal == Goal::FewestLevels ? Goal::LeastPriorWork : Goal::FewestLevels});
    if (!mine) { shown.refused = true; return; }
    // Bit 0 is the item and bit i + 1 is wanted[i], with or without an item.
    int const shift = withItem ? 0 : 1;
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
    auto parts = [&](unsigned mask) {
        std::vector<NamePart> out;
        if (mask & 1u) { out.push_back({shown.itemName, 1u}); return out; }
        for (size_t i = 0; i < wanted.size(); ++i)
            if (mask >> (i + 1) & 1u) out.push_back({nameOf(wanted[i].type, wanted[i].level), 1u << (i + 1)});
        return out;
    };
    bool itemEnchanted = !base.empty();
    for (auto& step : mine->steps) {
        // The step's cost is the sacrificed side's levels plus both surcharges.
        // On the item a raising book adds only the difference.
        bool const ontoItem = withItem && !heldBook && (step.target & 1u);
        int value = 0;
        for (size_t i = 0; i < made->size(); ++i)
            if (step.sacrifice >> i & 1u) value += ontoItem && (*made)[i].onItem >= 0 ? (*made)[i].onItem : (*made)[i].value;
        step.target <<= shift; step.sacrifice <<= shift;
        shown.steps.push_back({side(step.target), side(step.sacrifice), (step.target & 1u) != 0, step.cost, step.cost - value,
            onlyCurses(wanted, step.target), onlyCurses(wanted, step.sacrifice)});
        if (shown.steps.back().keptIsItem) shown.steps.back().keptEnchanted = std::exchange(itemEnchanted, true);
        shown.steps.back().keptParts = parts(step.target);
        shown.steps.back().goneParts = parts(step.sacrifice);
    }
    shown.total = mine->total; shown.highest = mine->highest; shown.work = mine->work;
    shown.planned = true;
    if (other) {
        shown.otherKnown = true;
        shown.otherTotal = other->total; shown.otherWork = other->work;
        shown.sameForBoth = other->total == mine->total && other->work == mine->work;
    }
    if (mine->steps.empty()) return;
    active.valid = true;
    active.slot = chosen.slot;
    active.goal = goal;
    try { active.itemId = fromHeld && !planItem.id.empty() ? planItem.id : chosen.icon.getTypeName(); } catch (...) {}
    active.itemName = shown.itemName;
    active.base = base;
    active.heldBook = heldBook;
    active.otherKnown = shown.otherKnown; active.sameForBoth = shown.sameForBoth;
    active.otherTotal = shown.otherTotal; active.otherWork = shown.otherWork;
    active.wanted = wanted;
    active.plan = *mine;
    active.lines = shown.steps;
    anvilIconsMade = false;
    finishedAtAnvil = false;
}

// The step list beside the anvil screen. It takes no input.
std::optional<unsigned> sideOf(ItemStack const& stack) {
    if (stack.isNull() || !stack.mItem) return std::nullopt;
    bool const isBook = stack.getTypeName() == bookItem;
    bool const isItem = !isBook && active.slot != bookSlot && stack.mItem->getEnchantSlot() == active.slot;
    if (!isBook && !isItem) return std::nullopt;
    auto const on = enchantsOn(stack);
    // The held book is told from the fresh ones by carrying everything it had.
    if (isBook && active.heldBook && std::ranges::all_of(active.base, [&](Enchantment const& e) { return std::ranges::find(on, e) != on.end(); }))
        return enchantPlan::carried(true, on, active.base, active.wanted);
    return enchantPlan::carried(isItem, on, active.base, active.wanted);
}
std::string sideText(unsigned mask) {
    if (mask & 1u) return active.itemName;
    std::string text;
    for (size_t i = 0; i < active.wanted.size(); ++i) {
        if (!(mask >> (i + 1) & 1u)) continue;
        if (!text.empty()) text += ", ";
        text += nameOf(active.wanted[i].type, active.wanted[i].level);
    }
    return text;
}
StepsInfo info;
void makeInfo(std::span<unsigned const> raw, int level, bool creative) {
    info = {};
    info.goal = goal;
    info.creative = creative;
    info.level = creative ? -1 : level;
    if (!active.valid || active.lines.empty()) {
        info.message = ui::translated(!opened ? "enchantOrder.anvil.noPlan" : shown.costUnknown ? "enchantOrder.unknownCost"
            : shown.refused ? "enchantOrder.refused" : shown.planned ? "enchantOrder.noSteps" : "enchantOrder.empty");
        info.warning = opened && (shown.costUnknown || shown.refused);
        return;
    }
    info.planned = true;
    info.goal = active.goal;
    info.total = active.plan.total; info.highest = active.plan.highest; info.work = active.plan.work;
    info.otherKnown = active.otherKnown; info.sameForBoth = active.sameForBoth;
    info.otherTotal = active.otherTotal; info.otherWork = active.otherWork;
    info.steps = active.lines;
    info.following = follow(raw, active.plan);
    auto const& f = info.following;
    if (f.next < 0) { info.note = ui::translated("enchantOrder.anvil.finished"); info.noteTone = 1; return; }
    // The next step when it can be taken; else everything the plan still lacks.
    std::string lacking;
    for (unsigned bit = 1; bit != 0 && bit <= f.full; bit <<= 1)
        if (f.lacks(bit)) { if (!lacking.empty()) lacking += ", "; lacking += sideText(bit); }
    if (f.ready || lacking.empty()) info.note = ui::translated("enchantOrder.anvil.next", f.next + 1);
    else { info.note = ui::translated("enchantOrder.anvil.missing", lacking); info.noteTone = 2; }
}
void ensureStepIcons() {
    if (anvilIconsMade) return;
    anvilItemIcon = stackOf(active.itemId);
    anvilBookIcon = stackOf(bookItem);
    anvilItemGlint = enchantedCopy(anvilItemIcon, active.wanted);
    anvilItems.clear(); anvilGlints.clear();
    anvilBooks.clear();
    anvilIconsMade = true;
}
// What the list shows, worked out once a frame after the anvil screen has
// drawn. It is painted from here, or one frame later from inside the
// screen's own drawing, right before the game's tooltip, which then stays on
// top while the list is above the screen's dark layer.
struct ListState {
    bool ready = false;
    std::chrono::steady_clock::time_point at{};
    float left = 0, top = 0, width = 0, height = 0;
    float opacity = 1;
    float pointerX = -1, pointerY = -1;
    bool free = false;    // nothing on the pointer: a press beside the window would drop it
    bool planned = false; // there are steps to list; else one line says there is no plan
    bool chooser = false; // the choosing panel is shown left of the window
    ui::EnchantLayout choose;
    ui::EnchantStepsLayout steps;
    // The next step's two stacks, outlined where they lie, and the anvil slot each goes in (L-170 trial).
    enum class Tone { Left, Right, Wrong, Good, Warn };
    struct Mark {
        Box box;
        Tone tone = Tone::Left;
        bool placed = false; // an anvil slot holding the right stack: said more strongly
    };
    std::vector<Mark> marks;
} list;
// What can be pressed beside the anvil, as last drawn, for presses that arrive from the window procedure; and
// the presses taken there, which the client thread acts on with the next frame.
std::mutex pressLock;
struct PressState {
    std::chrono::steady_clock::time_point at{};
    float pointerX = -1, pointerY = -1;
    bool live = false; // fully shown and nothing on the pointer
    Box change, panel; // an empty box takes no press
    ui::EnchantLayout choose;
    ui::EnchantStepsLayout steps;
    bool goalLive = false;
} pressState;
struct Pending {
    ui::EnchantLayout::Hit hit;
    bool goal = false, scrollbar = false, stepsScrollbar = false;
    float y = 0;
};
std::vector<Pending> pendingPresses;
int pendingWheel = 0, pendingStepsWheel = 0;
bool pressSwallow = false;
int chooserFirst = 0, stepsFirst = 0;
std::string chooserMessage;
std::chrono::steady_clock::time_point chooserMessageAt{};
bool insideAnvil = false, paintedInside = false;
// The item held on the pointer, to be drawn once more over the list: the
// game's stack and the box the game draws it in. Valid for one frame.
struct HeldIcon {
    ItemStack const* stack = nullptr;
    float x = 0, y = 0, size = 16;
    // Where the game writes the stack's count, when it shows one.
    bool counted = false;
    float countX = 0, countY = 0, countWidth = 0, countHeight = 0;
};
// The game's count label of the held item, `selected_stack_size_text` (12x10 units; read from the log 2026-11-12).
UIControl const* countControl(UIControl const& control, int depth = 0) {
    if (depth > 8) return nullptr;
    for (auto const& child : *control.mChildren) {
        if (!child || (static_cast<int>(child->mVisible) & 1) == 0) continue;
        glm::vec2 const extent = *child->mSize;
        if (*child->mName == "selected_stack_size_text" && extent.x > 0 && extent.y > 0) return child.get();
        if (auto const* found = countControl(*child, depth + 1)) return found;
    }
    return nullptr;
}
using Icons = std::vector<inspection::render::IconAt>;
// The marks on the slots. Drawn over the items (Lamium cannot draw under them): a half-strength tint over the
// whole slot, chosen in game 2026-11-13 over frames and fainter tints - the colour has to be seen, the item
// under it only recognised. The stack that goes left and its slot in the shapes' cyan, the right one in
// their pink, so the accent green is left to "this pair is right", the curses' red to "wrong" and the
// warning yellow to "the cost differs". What an anvil slot says once something lies in it has a frame too.
void paintMarks(MinecraftUIRenderContext& context) {
    using namespace lamium::ui;
    using Tone = ListState::Tone;
    constexpr Rgb cyan{.25f, .82f, .88f}, pink{.94f, .5f, .75f}; // as in ShapesView
    constexpr float tint = .5f;
    for (auto const& mark : list.marks) {
        auto const& b = mark.box;
        Rgb const color = mark.tone == Tone::Left ? cyan : mark.tone == Tone::Right ? pink
            : mark.tone == Tone::Good ? palette::accent : mark.tone == Tone::Wrong ? palette::vanilla::curse : palette::warning;
        fill(context, b.x, b.y, b.w, b.h, color, tint);
        if (mark.placed || mark.tone == Tone::Wrong || mark.tone == Tone::Good || mark.tone == Tone::Warn) frame(context, b.x, b.y, b.w, b.h, color);
    }
}
// The choosing panel left of the window: the planner screen's own column (ui/EnchantChooser), with two lines
// of status under it.
void paintChooser(MinecraftUIRenderContext& context, Icons& icons) {
    using namespace lamium::ui;
    auto const& l = list.choose;
    if (!l.usable() || !opened) return;
    float const pad = SettingsTable::pad;
    panel(context, l.left, l.headTop, l.width, l.footerTop + EnchantLayout::statusHeight - l.headTop, .8f);
    frame(context, l.left, l.headTop, l.width, l.footerTop + EnchantLayout::statusHeight - l.headTop, palette::white, .14f);
    auto const hover = list.free ? l.hit(list.pointerX, list.pointerY) : EnchantLayout::Hit{};
    size_t const before = icons.size();
    int itemsDrawn = 0;
    enchant_chooser::draw(context, l, shown, hover, icons, itemsDrawn, list.free && l.rows.onScrollbar(list.pointerX, list.pointerY));
    // In their turn and not in batches, so an item held on the pointer, drawn last, is above them all.
    for (size_t i = before; i < icons.size(); ++i) { icons[i].alpha = list.opacity; icons[i].direct = true; }
    fill(context, l.left, l.footerTop, l.width, 1, palette::white, .14f);
    bool const said = !chooserMessage.empty() && std::chrono::steady_clock::now() - chooserMessageAt < std::chrono::seconds(4);
    std::string text = said ? chooserMessage : enchant_chooser::status(shown, hover);
    // The steps' side has no status line of its own: its help is said here too.
    if (text.empty() && list.free) text = enchant_steps::status(info, list.steps, list.pointerX, list.pointerY);
    if (!text.empty()) paragraph(context, l.left + pad, l.footerTop + 3, l.width - 2 * pad, text, 3, said ? palette::warning : palette::text);
}
// The Change button, in a row of its own at the bottom right: it opens the planner screen where the choosing
// panel is not shown.
void paintChange(MinecraftUIRenderContext& context, Box& change) {
    using namespace lamium::ui;
    std::string const text = translated("enchantOrder.anvil.change");
    change = changeButton(list.left, list.top, list.width, list.height, textWidth(context, text), 4);
    bool const over = list.free && list.opacity > .98f && change.has(list.pointerX, list.pointerY);
    fill(context, change.x, change.y, change.w, change.h, palette::white, over ? .22f : .08f);
    label(context, change.x, change.y + 1, change.w, text, over ? palette::text : palette::dim, Align::Center);
}
// The steps right of the window: the planner screen's own right column (ui/EnchantSteps).
ui::enchant_steps::Tip paintSteps(MinecraftUIRenderContext& context, Icons& icons, Box& change) {
    using namespace lamium::ui;
    auto const& l = list.steps;
    if (!l.usable()) return {};
    panel(context, list.left, list.top, list.width, list.height, .8f);
    frame(context, list.left, list.top, list.width, list.height, palette::white, .14f); // the outline Lamium's screen has
    size_t const before = icons.size();
    // No pointer while an item is on it.
    auto const tip = enchant_steps::draw(context, l, info, list.free ? list.pointerX : -1e6f, list.free ? list.pointerY : -1e6f, list.chooser, icons);
    // In their turn and not in batches, so an item held on the pointer, drawn last, is above them all.
    for (size_t i = before; i < icons.size(); ++i) { icons[i].alpha = list.opacity; icons[i].direct = true; }
    if (!list.chooser) paintChange(context, change);
    return tip;
}
void paintList(MinecraftUIRenderContext& context, HeldIcon const* held = nullptr) {
    using namespace lamium::ui;
    if (list.opacity <= .02f) return;
    DrawOpacity const fade{list.opacity};
    Icons icons;
    Box change, panelBox;
    paintMarks(context);
    if (list.chooser) {
        paintChooser(context, icons);
        auto const& l = list.choose;
        panelBox = {l.left, l.headTop, l.width, l.footerTop + EnchantLayout::statusHeight - l.headTop};
    }
    auto const tip = paintSteps(context, icons, change);
    {
        std::scoped_lock lock(pressLock);
        pressState = {std::chrono::steady_clock::now(), list.pointerX, list.pointerY, list.free && list.opacity > .98f, change, panelBox,
            list.chooser ? list.choose : EnchantLayout{}, list.steps, list.chooser};
    }
    context.flushText(0, std::nullopt);
    // The held item goes with the panels' own icons, last so it is on top: a
    // glint drawn after another batch of icons does not show (ItemIcon.cpp).
    if (held) icons.push_back({held->stack, held->x, held->y, held->size / 16, 0, list.opacity, true, true});
    if (!icons.empty()) inspection::render::drawItemIcons(context, icons, 17);
    enchant_steps::drawTip(context, tip);
    if (held && held->stack->mItem) {
        // Its durability bar as the game draws it on a slot, at the held item's size.
        int const maxDamage = static_cast<int>(held->stack->mItem->getMaxDamage());
        if (inspection::render::shouldShowDurabilityBar(held->stack->isDamageableItem(), held->stack->getDamageValue(), maxDamage)) {
            float const unit = held->size / 16, ratio = inspection::render::durabilityRatio(held->stack->getDamageValue(), maxDamage);
            auto const back = inspection::render::durabilityBackground({0, 0, 16, 16});
            auto const front = inspection::render::durabilityForeground(back, ratio);
            auto const color = inspection::render::durabilityColor(ratio);
            fill(context, held->x + back.x0 * unit, held->y + back.y0 * unit, back.width() * unit, back.height() * unit, palette::shade);
            if (front.width() > 0)
                fill(context, held->x + front.x0 * unit, held->y + front.y0 * unit, front.width() * unit, front.height() * unit,
                    Rgb{color.r, color.g, color.b});
        }
        // Its count, on top of it and where the game writes it (the game's own number is under the item
        // drawn again), drawn as the container previews draw theirs.
        if (held->stack->mCount > 1 && held->counted) {
            auto const& fontHandle = context.mClient.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default");
            TextMeasureData const textData{1.0f, 0.0f, true, false, false, ::ui::TextAlignment::Right};
            CaretMeasureData const caret{-1, false};
            context.drawText(ui::fontOf(fontHandle),
                RectangleArea{held->countX, held->countX + held->countWidth, held->countY, held->countY + held->countHeight},
                std::to_string(static_cast<int>(held->stack->mCount)), mce::Color{1.f, 1.f, 1.f, list.opacity}, 1.0f, ::ui::TextAlignment::Right,
                textData, caret);
            context.flushText(0, std::nullopt);
        }
    }
}
// The game's tooltip is about to be drawn in the anvil screen. Painted here
// the panels are above the screen's dark layer and under the tooltip. (Painting
// from the slot renderer instead broke the game's item drawing.)
void beforeTooltip(MinecraftUIRenderContext& context) {
    if (!insideAnvil || paintedInside || !list.ready || std::chrono::steady_clock::now() - list.at > std::chrono::milliseconds(250)) return;
    paintedInside = true;
    paintList(context);
}
float windowRest = 0;
bool isAnvil(ScreenView& view) {
    return view.mHasFocus && view.mController && view.mController->_isContainerScreen() && view.getScreenName() == "anvil_screen";
}
void enterScreen(ll::event::UIRenderEvent& event) {
    insideAnvil = isAnvil(event.screenView());
    if (insideAnvil) paintedInside = false;
}
// Everything Lamium attaches to the anvil screen goes with it.
void endAnvil() {
    if (anvilOpened) close();
    chooserMessage.clear();
    std::scoped_lock lock(pressLock);
    pressState = {};
    pendingPresses.clear();
    pendingWheel = pendingStepsWheel = 0;
}
// An item cell follows what is in its place while the anvil is open. True when it changed.
bool see(Held& cell, ItemStack const& stack) {
    Held now;
    readItem(now, stack);
    bool const same = now.present == cell.present
        && (!now.present || (now.kind == cell.kind && now.work == cell.work && now.stateKnown == cell.stateKnown
               && now.name == cell.name && sameSet(now.base, cell.base)));
    if (same) return false;
    cell = std::move(now);
    return true;
}
// The main hand's cell and the anvil's left slot's. With nothing chosen yet, an item put in the slot is
// taken as the plan's.
void seeItems(ItemStack const* hand, ItemStack const& input) {
    bool const handChanged = hand && see(held, *hand), slotChanged = see(slotItem, input);
    if (slotChanged && slotItem.present && slotItem.stateKnown && wanted.empty() && !active.valid) pickSlot();
    else if (handChanged || slotChanged) rebuild();
}
// The presses and wheel turns taken since the last frame.
void applyPresses() {
    std::vector<Pending> presses;
    int wheel = 0, stepsWheel = 0;
    {
        std::scoped_lock lock(pressLock);
        presses.swap(pendingPresses);
        wheel = std::exchange(pendingWheel, 0);
        stepsWheel = std::exchange(pendingStepsWheel, 0);
    }
    auto const& rows = list.choose.rows;
    auto const& stepRows = list.steps.rows;
    if (wheel) chooserFirst = ui::SettingsTable::clampFirst(chooserFirst + wheel, rows.listCount, rows.listVisible);
    if (stepsWheel) stepsFirst = ui::SettingsTable::clampFirst(stepsFirst + stepsWheel, stepRows.listCount, stepRows.listVisible);
    for (auto const& press : presses) {
        if (press.goal) { if (opened) { flipGoal(); stepsFirst = 0; } continue; }
        if (press.stepsScrollbar) { stepsFirst = stepRows.firstAt(press.y); continue; }
        if (press.scrollbar) { chooserFirst = rows.firstAt(press.y); continue; }
        if (!opened) continue;
        std::string message;
        auto const pressed = ui::enchant_chooser::press(press.hit, message);
        if (pressed == ui::enchant_chooser::Pressed::NewItem) chooserFirst = 0;
        if (!message.empty()) { chooserMessage = std::move(message); chooserMessageAt = std::chrono::steady_clock::now(); }
        else if (pressed != ui::enchant_chooser::Pressed::No) chooserMessage.clear();
    }
}
// Beside the anvil screen: the steps right of the window and, with room and the setting, the choosing panel
// left of it. A finished plan leaves once the anvil has been closed.
void drawBeside(ll::event::UIRenderEvent& event) {
    auto& view = event.screenView();
    auto const clock = std::chrono::steady_clock::now();
    if (!isAnvil(view)) {
        if (clock - anvilSeen > std::chrono::milliseconds(500)) {
            if (finishedAtAnvil) {
                // Its enchantments go to "Last choice", so the next anvil starts empty.
                remember();
                wanted.clear();
                planItem.use = false;
                active = {};
                finishedAtAnvil = false;
            }
            if (anvilOpened) endAnvil();
        }
        return;
    }
    insideAnvil = false;
    bool const painted = std::exchange(paintedInside, false);
    list.ready = false;
    bool const reopened = clock - anvilSeen > std::chrono::milliseconds(500);
    anvilSeen = clock;
    auto const preferences = Runtime::instance().preferences().inventory;
    if (!preferences.enchantOrderAnvil || !versionSensitiveAllowed(capabilities::Id::EnchantOrder)) { endAnvil(); return; }
    auto* tree = view.mVisualTree.get();
    if (!tree) return;
    static std::string const windowName = "root_panel";
    auto window = tree->getControlByName(windowName, true);
    if (!window || window->mCachedPositionDirty) return;
    glm::vec2 const at = *window->mCachedPosition, size = *window->mSize, screen = *view.mSize;
    if (size.x <= 0 || size.y <= 0 || size.x >= screen.x - 1) return; // not laid out yet
    constexpr float gap = 4, wide = 200, narrow = 110, chooseWide = 190, chooseNarrow = 130, chooseTall = 300;
    float const roomRight = screen.x - (at.x + size.x) - 2 * gap, roomLeft = at.x - 2 * gap;
    auto& context = event.uiRenderContext();
    // The choosing panel needs both sides: itself on the left, the steps on the right.
    bool chooser = preferences.enchantOrderAnvilChoose && roomRight >= narrow && roomLeft >= chooseNarrow;
    if (chooser && !anvilOpened) {
        std::string problem;
        chooser = open(context.mClient, problem, true);
        chooserFirst = 0;
    } else if (!chooser && anvilOpened) endAnvil();
    float width = std::min(wide, roomRight), left = at.x + size.x + gap;
    if (width < narrow) {
        width = std::min(wide, roomLeft);
        left = at.x - gap - width;
    }
    if (width < narrow) return;
    // Where the window rests is its highest place since the screen opened. While it slides away on closing the
    // panels go with it by the same distance, off the screen too.
    windowRest = reopened ? at.y : std::min(windowRest, at.y);

    // What the slots and the inventory hold, as planner inputs.
    int now = -1, gameCost = -1, level = -1;
    bool both = false, creative = false;
    ItemStack const* heldItem = nullptr; // the stack on the pointer; the manager's, valid for this frame
    ItemStack const* inHand = nullptr;   // the main hand's; the player's, valid for this frame
    std::optional<unsigned> target, sacrifice;
    std::vector<unsigned> raw;
    std::array<unsigned, 36> slotMasks{}; // the hotbar's nine, then the inventory's twenty-seven; 0 for no part of the plan
    bool inputEmpty = true, materialEmpty = true;
    unsigned pointerMask = 0;
    try {
        if (auto* player = context.mClient.getLocalPlayer()) {
            creative = player->isCreative();
            inHand = &player->getSelectedItem();
            if (auto const* attribute = player->getAttribute(Player::LEVEL()).mPtr) level = static_cast<int>(attribute->mCurrentValue);
        }
        auto& screenController = static_cast<ContainerScreenController&>(*view.mController);
        if (auto manager = screenController.mContainerManagerController; manager && !manager->mContainersClosed) {
            auto const& input = manager->getItemStack("anvil_input_items", 0);
            auto const& material = manager->getItemStack("anvil_material_items", 0);
            // First what changes the plan, then the plan against the slots.
            if (chooser) seeItems(inHand, input);
            applyPresses();
            auto const& onPointer = manager->getItemStack("cursor_items", 0);
            if (!onPointer.isNull()) heldItem = &onPointer;
            if (active.valid) {
                if (!input.isNull()) target = sideOf(input);
                if (!material.isNull()) sacrifice = sideOf(material);
                both = !input.isNull() && !material.isNull();
                if (target && sacrifice) now = enchantPlan::stepFor(active.plan, *target, *sacrifice);
                if (both) gameCost = static_cast<AnvilContainerManagerController&>(*manager).mCost;
                if (target) raw.push_back(*target);
                if (sacrifice) raw.push_back(*sacrifice);
                if (auto mask = sideOf(onPointer)) { raw.push_back(*mask); pointerMask = *mask; }
                inputEmpty = input.isNull();
                materialEmpty = material.isNull();
                // The two collections apart, so a slot's place on the screen is known: its grid and its index there.
                auto read = [&](char const* collection, int count, size_t first) {
                    int const there = std::min(count, manager->getContainerSize(collection));
                    for (int i = 0; i < there; ++i)
                        if (auto mask = sideOf(manager->getItemStack(collection, i))) {
                            raw.push_back(*mask);
                            slotMasks[first + static_cast<size_t>(i)] = *mask;
                        }
                };
                read("hotbar_items", 9, 0);
                read("inventory_items", 27, 9);
            }
        }
    } catch (...) { now = -1; both = false; raw.clear(); }
    makeInfo(raw, level, creative);
    bool const planned = info.planned;
    if (planned) {
        // The pair lying in the slots is what the last line speaks of.
        info.inSlots = now;
        if (both && now >= 0 && gameCost != active.lines[static_cast<size_t>(now)].cost) {
            info.note = ui::translated("enchantOrder.anvil.costDiffers", gameCost);
            info.noteTone = 2;
        } else if (both && now >= 0) {
            info.note = ui::translated("enchantOrder.anvil.match", now + 1);
            info.noteTone = 1;
        } else if (both) {
            info.note = ui::translated(!target ? "enchantOrder.anvil.leftNot" : !sacrifice ? "enchantOrder.anvil.rightNot" : "enchantOrder.anvil.notInPlan");
            info.noteTone = 2;
        }
    }
    finishedAtAnvil = planned && info.following.next < 0;
    // What the anvil's three slots say, and the stacks to place next where they lie.
    std::vector<ListState::Mark> marks;
    if (planned && preferences.enchantOrderAnvilMarks) {
        try {
            using Tone = ListState::Tone;
            // A stack lying in the other anvil slot is at hand too: the empty slot it belongs in still asks for it.
            auto atHand = [&](unsigned side) {
                return pointerMask == side || target.value_or(0u) == side || sacrifice.value_or(0u) == side
                    || std::ranges::find(slotMasks, side) != slotMasks.end();
            };
            auto const say = anvilSay(active.plan, info.following.next, !inputEmpty, target.value_or(0u), !materialEmpty, sacrifice.value_or(0u), gameCost,
                atHand);
            auto box = [&](char const* name) -> std::optional<Box> {
                static std::string key;
                key = name;
                auto control = tree->getControlByName(key, true);
                if (!control || control->mCachedPositionDirty) return std::nullopt;
                glm::vec2 const where = *control->mCachedPosition, extent = *control->mSize;
                return Box{where.x, where.y, extent.x, extent.y};
            };
            auto slot = [&](int index) -> std::optional<Box> {
                auto grid = box(index < 9 ? "hotbar_grid" : "inventory_grid");
                if (!grid) return std::nullopt;
                return gridSlot(grid->x, grid->y, index < 9 ? index : index - 9);
            };
            // The anvil's slots (`input_item_slot`, `material_item_slot`, `result_item_slot`, 18 units; read from
            // the log 2026-11-13).
            auto place = [&](char const* name, SlotSay state, Tone tone) {
                if (state == SlotSay::None) return;
                if (auto b = box(name)) marks.push_back({*b, state == SlotSay::Wrong ? Tone::Wrong : tone, state == SlotSay::Right});
            };
            place("input_item_slot", say.left, Tone::Left);
            place("material_item_slot", say.right, Tone::Right);
            if (say.result != ResultSay::None)
                if (auto b = box("result_item_slot"))
                    marks.push_back({*b, say.result == ResultSay::Match ? Tone::Good : say.result == ResultSay::CostDiffers ? Tone::Warn : Tone::Wrong});
            // In the inventory: the stacks that step still wants. A side that lies in its anvil slot or is being
            // carried on the pointer is not looked for; the other one keeps its mark meanwhile. A slot holding
            // something else has not placed its side, so that side keeps its mark too.
            if (say.step >= 0) {
                auto const& step = active.plan.steps[static_cast<size_t>(say.step)];
                auto const stacks = nextStacks(step, slotMasks, pointerMask == step.target ? step.target : target.value_or(0u),
                    pointerMask == step.sacrifice ? step.sacrifice : sacrifice.value_or(0u));
                if (stacks.left >= 0) if (auto b = slot(stacks.left)) marks.push_back({*b, Tone::Left});
                if (stacks.right >= 0) if (auto b = slot(stacks.right)) marks.push_back({*b, Tone::Right});
            }
        } catch (...) { marks.clear(); }
    }

    ui::EnchantLayout choose;
    if (chooser) {
        float const chooseWidth = std::min(chooseWide, roomLeft), chooseHeight = std::min(chooseTall, screen.y - 2 * gap);
        float const chooseTop = std::clamp(windowRest + (size.y - chooseHeight) / 2, gap, std::max(gap, screen.y - gap - chooseHeight)) + (at.y - windowRest);
        // One height whatever the kind: a panel as tall as its rows changed size with every press (tried 2026-11-12).
        choose = ui::EnchantLayout::fitPanel(at.x - gap - chooseWidth, chooseTop, chooseWidth, chooseHeight, static_cast<int>(kinds.size()),
            shown.heldCell, shown.slotCell, static_cast<int>(shown.rows.size()), chooserFirst);
        chooserFirst = choose.rows.listFirst;
        chooser = choose.usable();
    }
    // The steps: as tall as the plan needs, scrolling where the screen is not.
    float const changeSpace = chooser ? 0 : changeRow, room = screen.y - 2 * gap;
    int const stepCount = static_cast<int>(info.steps.size());
    int const lines = planned ? ui::enchant_steps::detailLines(context, info, width) : 1;
    int const foot = planned ? ui::enchant_steps::footLines(context, info, width) : 1;
    // Without a plan the same side with its goal, and a few lines saying what to do, in the room of two steps.
    float const height = std::min(room, ui::EnchantStepsLayout::neededHeight(planned ? stepCount : 2, lines, foot) + changeSpace + 2);
    // Level with the choosing panel's top where there is one, so the two start on one line; else with the window's.
    float const rest = chooser ? choose.headTop - (at.y - windowRest) : windowRest;
    float const top = std::clamp(rest, gap, std::max(gap, screen.y - gap - height)) + (at.y - windowRest);
    auto const stepsLayout =
        ui::EnchantStepsLayout::fit(left, top + 1, width, top + height - changeSpace - 1, planned ? stepCount : 0, stepsFirst, lines, foot);
    if (!stepsLayout.usable()) return;
    stepsFirst = stepsLayout.rows.listFirst;
    // The game's window fades as it slides away on closing; the panels fade with the distance, or with the
    // window's own opacity when that is lower.
    float const slide = std::max(0.f, at.y - windowRest), way = std::max(1.f, (screen.y - windowRest) * .5f);
    float opacity = std::clamp(1 - slide / way, 0.f, 1.f);
    // Only while it is moving and only a value that looks like a fade: at rest the member may not be the drawn opacity.
    if (float const own = window->mAlpha; slide > .5f && std::isfinite(own) && own > 0 && own < opacity) opacity = own;
    list.ready = true;
    list.at = clock;
    list.left = left; list.top = top; list.width = width; list.height = height;
    list.opacity = opacity;
    list.pointerX = view.mPointerLocationPrevious->x; list.pointerY = view.mPointerLocationPrevious->y;
    list.free = heldItem == nullptr;
    list.planned = planned;
    list.chooser = chooser;
    list.choose = choose;
    list.steps = stepsLayout;
    list.marks = std::move(marks);
    // Painted after the screen, the panels cover the item held on the pointer. The game draws that item in the
    // control `selected_item_icon` (21 units, following the pointer; read from the log 2026-11-12), so while
    // it overlaps one of them it is drawn once more in the same box, with them.
    HeldIcon onTop;
    if (!painted && heldItem && opacity > .02f) {
        static std::string const heldName = "selected_item_icon";
        try {
            if (auto icon = tree->getControlByName(heldName, true); icon && !icon->mCachedPositionDirty) {
                glm::vec2 const where = *icon->mCachedPosition, extent = *icon->mSize;
                auto over = [&](float x, float y, float w, float h) {
                    return extent.x > 0 && where.x < x + w && where.x + extent.x > x && where.y < y + h && where.y + extent.y > y;
                };
                if (over(left, top, width, height)
                    || (chooser && over(choose.left, choose.headTop, choose.width, choose.footerTop + ui::EnchantLayout::statusHeight - choose.headTop))) {
                    onTop = {heldItem, where.x, where.y, extent.x};
                    static std::string const buttonName = "inventory_selected_icon_button";
                    if (auto button = tree->getControlByName(buttonName, true))
                        if (auto const* count = countControl(*button)) {
                            glm::vec2 const at2 = *count->mCachedPosition, size2 = *count->mSize;
                            onTop.counted = true;
                            onTop.countX = at2.x; onTop.countY = at2.y; onTop.countWidth = size2.x; onTop.countHeight = size2.y;
                        }
                }
            }
        } catch (...) { onTop = {}; }
    }
    // Not painted from inside the screen this frame (no tooltip, or the first frame): paint it now.
    if (!painted) paintList(context, onTop.stack ? &onTop : nullptr);
}
void dropStacks() {
    kinds.clear();
    held = {};
    slotItem = {};
    planIcon = ItemStack();
    book = ItemStack();
    anvilItemIcon = ItemStack();
    anvilBookIcon = ItemStack();
    anvilItemGlint = ItemStack();
    anvilItems.clear(); anvilBooks.clear(); screenItems.clear(); anvilGlints.clear();
    anvilIconsMade = false;
    opened = false;
}
}

bool open(IClientInstance& client, std::string& problem, bool atAnvil) {
    close();
    auto* player = client.getLocalPlayer();
    if (!player) { problem = "worldOnly"; return false; }
    if (!versionSensitiveAllowed(capabilities::Id::EnchantOrder)) { problem = "enchantOrder.unverified"; return false; }
    try {
        if (!readCatalog()) { dropStacks(); problem = "enchantOrder.noCatalog"; return false; }
        readItem(held, player->getSelectedItem());
        creativeNow = player->isCreative();
        book = stackOf(bookItem);
    } catch (...) { dropStacks(); problem = "enchantOrder.noCatalog"; return false; }
    opened = true;
    anvilOpened = atAnvil;
    if (active.valid && chosenKind() >= 0) {
        // An unfinished plan opens as it was left (decided 2026-11-12): the items at hand do not replace it,
        // their cells are only offered. The item it was made from is shown from its record.
        if (planItem.use) planIcon = enchantedCopy(stackOf(planItem.id), planItem.base);
    } else if (held.present) {
        // Without one: the main hand's item when it can be enchanted, else the kind left last.
        int const slot = kinds[static_cast<size_t>(held.kind)].slot;
        if (slot != chosenSlot) { remember(); wanted.clear(); chosenSlot = slot; }
        if (held.stateKnown) take(held); else planItem.use = false;
    } else {
        planItem.use = false;
        if (chosenKind() < 0) { wanted.clear(); chosenSlot = kinds.front().slot; }
    }
    rebuild();
    return true;
}
void close() {
    anvilOpened = false;
    if (!opened) return;
    remember();
    dropStacks();
    shown = {};
}
View const& view() { return shown; }
void pickKind(int index) {
    if (!opened || index < 0 || index >= static_cast<int>(kinds.size())) return;
    int const slot = kinds[static_cast<size_t>(index)].slot;
    if (slot == chosenSlot) {
        // The plan's item's own kind: the same item from fresh, keeping what it carries as wanted.
        if (!usingItem(index)) return;
        release(wanted, planItem.base, kinds[static_cast<size_t>(index)].offers);
    } else {
        remember();
        wanted.clear();
        chosenSlot = slot;
    }
    planItem.use = false;
    rebuild();
}
namespace {
void pickItem(Held const& item) {
    if (!opened || !item.present || !item.stateKnown) return;
    if (!isPlanItem(item)) {
        int const slot = kinds[static_cast<size_t>(item.kind)].slot;
        if (slot != chosenSlot) { remember(); wanted.clear(); chosenSlot = slot; }
        take(item);
    } else planItem.fromSlot = &item == &slotItem; // the same item's other cell
    rebuild();
}
}
void pickHeld() { pickItem(held); }
void pickSlot() { pickItem(slotItem); }
Change toggleRow(int row) {
    if (!opened || row < 0 || row >= static_cast<int>(shown.rows.size())) return Change::Blocked;
    int const kind = chosenKind();
    if (kind < 0) return Change::Blocked;
    auto const& offers = kinds[static_cast<size_t>(kind)].offers;
    int const type = shown.rows[static_cast<size_t>(row)].type;
    auto offer = std::ranges::find_if(offers, [&](Offer const& o) { return o.type == type; });
    if (offer == offers.end()) return Change::Fixed;
    auto change = toggle(wanted, *offer, usingItem(kind) ? planItem.base : std::vector<Enchantment>{}, conflicts);
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
    bool const raising = usingItem(kind) && kinds[static_cast<size_t>(kind)].slot != bookSlot;
    if (offer != offers.end() && step(wanted, *offer, delta, raising ? levelOf(planItem.base, type) : 0)) rebuild();
}
void setAllHighest() {
    int const kind = chosenKind();
    if (!opened || kind < 0) return;
    auto all = allHighest(kinds[static_cast<size_t>(kind)].offers, usingItem(kind) ? planItem.base : std::vector<Enchantment>{}, conflicts);
    // A held book's own enchantments are not raised.
    if (usingItem(kind) && kinds[static_cast<size_t>(kind)].slot == bookSlot)
        std::erase_if(all, [&](Enchantment const& e) { return has(planItem.base, e.type); });
    if (all == wanted) return;
    remember();
    wanted = std::move(all);
    rebuild();
}
void restoreLast() {
    auto found = lastBySlot.find(chosenSlot);
    if (!opened || found == lastBySlot.end()) return;
    auto previous = found->second;
    if (previous == wanted) return;
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
int anvilPress(bool left, bool down) {
    std::scoped_lock lock(pressLock);
    if (!left) return 0;
    if (!down) return std::exchange(pressSwallow, false) ? 1 : 0;
    pressSwallow = false;
    auto const& state = pressState;
    // What a frame drew a moment ago: the panels are gone or moved otherwise.
    if (!state.live || std::chrono::steady_clock::now() - state.at > std::chrono::milliseconds(300)) return 0;
    float const x = state.pointerX, y = state.pointerY;
    if (state.change.has(x, y)) { pressSwallow = true; return 2; }
    if (state.steps.usable()) {
        if (state.goalLive && state.steps.hit(x, y) == ui::EnchantStepsLayout::Zone::Goal) {
            Pending press;
            press.goal = true;
            pendingPresses.push_back(press);
            pressSwallow = true;
            return 1;
        }
        if (state.steps.rows.onScrollbar(x, y)) {
            Pending press;
            press.stepsScrollbar = true;
            press.y = y;
            pendingPresses.push_back(press);
            pressSwallow = true;
            return 1;
        }
    }
    if (state.panel.has(x, y)) {
        Pending press;
        press.y = y;
        if (state.choose.rows.onScrollbar(x, y)) press.scrollbar = true;
        else press.hit = state.choose.hit(x, y);
        pendingPresses.push_back(press);
        pressSwallow = true;
        return 1;
    }
    return 0;
}
bool anvilWheel(int rows) {
    std::scoped_lock lock(pressLock);
    auto const& state = pressState;
    if (!state.live || std::chrono::steady_clock::now() - state.at > std::chrono::milliseconds(300)) return false;
    float const x = state.pointerX, y = state.pointerY;
    if (state.panel.has(x, y)) { pendingWheel += rows; return true; }
    auto const& steps = state.steps;
    if (steps.usable() && x >= steps.left && x < steps.left + steps.width && y >= steps.top && y < steps.bottom) {
        pendingStepsWheel += rows;
        return true;
    }
    return false;
}
void followInventory(IClientInstance& client) {
    std::vector<unsigned> raw;
    int level = -1;
    bool creative = creativeNow;
    try {
        if (auto* player = client.getLocalPlayer()) {
            creative = player->isCreative();
            if (auto const* attribute = player->getAttribute(Player::LEVEL()).mPtr) level = static_cast<int>(attribute->mCurrentValue);
            if (active.valid) {
                auto& inventory = player->getInventory();
                for (int slot = 0; slot < 36; ++slot)
                    if (auto mask = sideOf(inventory.getItem(slot))) raw.push_back(*mask);
            }
        }
    } catch (...) { raw.clear(); }
    makeInfo(raw, level, creative);
}
StepsInfo const& steps() { return info; }
ItemStack const* stepItemIcon(int number, bool enchanted) {
    if (!active.valid) return nullptr;
    ensureStepIcons();
    return enchanted ? pooled(anvilGlints, anvilItemGlint, number) : pooled(anvilItems, anvilItemIcon, number);
}
ItemStack const* stepBookIcon(int number) {
    if (!active.valid) return nullptr;
    ensureStepIcons();
    return pooled(anvilBooks, anvilBookIcon, number);
}
ItemStack const* kindIcon(int index) {
    if (index < 0 || index >= static_cast<int>(kinds.size()) || kinds[static_cast<size_t>(index)].icon.isNull()) return nullptr;
    return &kinds[static_cast<size_t>(index)].icon;
}
ItemStack const* itemIcon() {
    int const kind = chosenKind();
    if (kind < 0) return nullptr;
    if (usingItem(kind) && !planIcon.isNull()) return &planIcon;
    return kindIcon(kind);
}
ItemStack const* heldIcon() { return held.present && !held.icon.isNull() ? &held.icon : nullptr; }
ItemStack const* slotIcon() { return slotItem.present && !slotItem.icon.isNull() ? &slotItem.icon : nullptr; }
ItemStack const* bookIcon() { return book.isNull() ? nullptr : &book; }
ItemStack const* itemIcon(int number) {
    auto const* source = itemIcon();
    return source ? pooled(screenItems, *source, number) : nullptr;
}

bool start() {
    auto& bus = ll::event::EventBus::getInstance();
    renderListener = bus.emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) { try { drawBeside(event); } catch (...) {} });
    enterListener = bus.emplaceListener<ll::event::BeforeUIRenderEvent>([](auto& event) { try { enterScreen(event); } catch (...) {} });
    inspection::setBeforeTooltip(beforeTooltip);

    exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { dropStacks(); shown = {}; });
    return true;
}
void stop() {
    auto& bus = ll::event::EventBus::getInstance();
    inspection::setBeforeTooltip(nullptr);

    for (auto* listener : {&renderListener, &exitListener, &enterListener})
        if (*listener) { bus.removeListener(*listener); listener->reset(); }
    endAnvil();
    dropStacks();
    shown = {};
    active = {};
}
}

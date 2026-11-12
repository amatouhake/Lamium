#include "features/inventory/EnchantOrderGame.h"
#include "app/Capabilities.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "features/inspection/Inspection.h"
#include "features/inspection/render/DurabilityBar.h"
#include "features/inspection/render/ItemIcon.h"
#include "ui/EnchantChooser.h"
#include "ui/EnchantLayout.h"
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
#include "mc/world/actor/player/Player.h"
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
    std::vector<Enchantment> base, wanted;
    Plan plan;
    std::vector<StepLine> lines;
} active;
ItemStack anvilItemIcon, anvilBookIcon, anvilItemGlint, screenItemGlint;
bool anvilIconsMade = false;
// One stack per drawn icon: a stack drawn several times in a frame lost its glint, and the others' with it
// (seen 2026-11-12 with two or more steps; the container previews draw a stack per slot).
constexpr size_t iconPool = 48;
std::vector<ItemStack> anvilItems, anvilBooks, screenItems, screenBooks, anvilGlints, screenGlints;
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
    int const slot = stack.getTypeName() == bookItem ? bookSlot : stack.mItem->getEnchantSlot();
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
bool usingItem(int kind) { return planItem.use && kind >= 0 && kinds[static_cast<size_t>(kind)].slot == planItem.slot; }
// An offered item is the one the plan was made from: same kind, same enchantments, same anvil uses.
bool isPlanItem(Held const& item) {
    return planItem.use && item.present && item.stateKnown && kinds[static_cast<size_t>(item.kind)].slot == planItem.slot
        && item.work == planItem.work && sameSet(item.base, planItem.base);
}
void take(Held const& item) {
    planItem = {true, kinds[static_cast<size_t>(item.kind)].slot, item.work, {}, item.name, item.base};
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
    screenItems.clear(); screenGlints.clear();
    screenItemGlint = ItemStack();
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
    shown.useHeld = fromHeld && isPlanItem(held);
    shown.heldCell = held.present && held.stateKnown;
    shown.heldName = held.name;
    shown.useSlot = fromHeld && isPlanItem(slotItem);
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
// What the list shows, worked out once a frame after the anvil screen has
// drawn. It is painted from here, or one frame later from inside the
// screen's own drawing, right before the game's tooltip, which then stays on
// top while the list is above the screen's dark layer.
struct ListState {
    bool ready = false;
    std::chrono::steady_clock::time_point at{};
    float left = 0, top = 0, width = 0, height = 0;
    bool twoLines = true, both = false, creative = false, target = false, sacrifice = false;
    float opacity = 1;
    int now = -1, next = -1, gameCost = -1, level = -1, leftPenalty = -1, rightPenalty = -1;
    std::vector<unsigned> atHand;
    float pointerX = -1, pointerY = -1;
    bool free = false;    // nothing on the pointer: a press beside the window would drop it
    bool planned = false; // there are steps to list; else one line says there is no plan
    bool chooser = false; // the choosing panel is shown left of the window
    ui::EnchantLayout choose;
} list;
// What can be pressed beside the anvil, as last drawn, for presses that arrive from the window procedure; and
// the presses taken there, which the client thread acts on with the next frame.
std::mutex pressLock;
struct PressState {
    std::chrono::steady_clock::time_point at{};
    float pointerX = -1, pointerY = -1;
    bool live = false; // fully shown and nothing on the pointer
    Box change, goal, panel; // an empty box takes no press
    ui::EnchantLayout choose;
} pressState;
struct Pending {
    ui::EnchantLayout::Hit hit;
    bool goal = false, scrollbar = false;
    float y = 0;
};
std::vector<Pending> pendingPresses;
int pendingWheel = 0;
bool pressSwallow = false;
int chooserFirst = 0;
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
// The choosing panel left of the window: the planner screen's own column (ui/EnchantChooser), with two lines
// of status under it.
void paintChooser(MinecraftUIRenderContext& context, Icons& icons) {
    using namespace lamium::ui;
    auto const& l = list.choose;
    if (!l.usable() || !opened) return;
    float const pad = SettingsTable::pad;
    panel(context, l.left, l.headTop, l.width, l.footerTop + EnchantLayout::statusHeight - l.headTop, .8f);
    auto const hover = list.free ? l.hit(list.pointerX, list.pointerY) : EnchantLayout::Hit{};
    size_t const before = icons.size();
    int itemsDrawn = 0;
    enchant_chooser::draw(context, l, shown, hover, icons, itemsDrawn);
    // In their turn and not in batches, so an item held on the pointer, drawn last, is above them all.
    for (size_t i = before; i < icons.size(); ++i) { icons[i].alpha = list.opacity; icons[i].direct = true; }
    fill(context, l.left, l.footerTop, l.width, 1, palette::white, .14f);
    bool const said = !chooserMessage.empty() && std::chrono::steady_clock::now() - chooserMessageAt < std::chrono::seconds(4);
    std::string const text = said ? chooserMessage : enchant_chooser::status(shown, hover);
    if (!text.empty()) paragraph(context, l.left + pad, l.footerTop + 2, l.width - 2 * pad, text, 2, said ? palette::warning : palette::text);
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
// No plan: one small panel saying so.
void paintNoPlan(MinecraftUIRenderContext& context, Box& change) {
    using namespace lamium::ui;
    constexpr float pad = 4, line = 10;
    panel(context, list.left, list.top, list.width, list.height, .8f);
    label(context, list.left + pad, list.top + pad, list.width - 2 * pad, translated("feature.enchantOrder"), palette::dim);
    if (list.chooser)
        paragraph(context, list.left + pad, list.top + pad + line + 3, list.width - 2 * pad, translated("enchantOrder.empty"), 3, palette::faint);
    else {
        label(context, list.left + pad, list.top + pad + line + 3, list.width - 2 * pad, translated("enchantOrder.anvil.noPlan"), palette::faint);
        paintChange(context, change);
    }
}
void paintSteps(MinecraftUIRenderContext& context, Icons& icons, Box& change, Box& goalBox) {
    using namespace lamium::ui;
    constexpr float pad = 4, line = 10;
    float const left = list.left, top = list.top, width = list.width, height = list.height;
    bool const twoLines = list.twoLines, both = list.both, creative = list.creative;
    int const now = list.now, next = list.next, gameCost = list.gameCost, level = list.level, leftPenalty = list.leftPenalty,
              rightPenalty = list.rightPenalty, steps = static_cast<int>(active.lines.size());
    bool const target = list.target, sacrifice = list.sacrifice;
    auto const& atHandNow = list.atHand;
    if (!active.valid || steps == 0 || now >= steps || next >= steps || static_cast<int>(active.plan.steps.size()) != steps) return;
    if (!anvilIconsMade) {
        anvilItemIcon = stackOf(active.itemId);
        anvilBookIcon = stackOf(bookItem);
        anvilItemGlint = enchantedCopy(anvilItemIcon, active.wanted);
        anvilItems.clear(); anvilGlints.clear();
        anvilBooks.clear();
        anvilIconsMade = true;
    }
    int itemsDrawn = 0, booksDrawn = 0;
    panel(context, left, top, width, height, .8f);
    float y = top + pad;
    float const inner = width - 2 * pad;
    // The total first, then what the player has to spend. With the choosing panel the goal's name is pressed to
    // change the goal, as its stepper is in the planner screen.
    std::string const goalName = translated(active.goal == Goal::FewestLevels ? "enchantOrder.goal.levels" : "enchantOrder.goal.work");
    float const goalWidth = std::min(textWidth(context, goalName) + 2, inner * .5f);
    label(context, left + pad, y, inner - goalWidth - 2, translated("enchantOrder.total", active.plan.total));
    if (list.chooser) {
        goalBox = {left + width - pad - goalWidth - 2, y - 1, goalWidth + 4, line + 1};
        bool const over = list.free && list.opacity > .98f && goalBox.has(list.pointerX, list.pointerY);
        fill(context, goalBox.x, goalBox.y, goalBox.w, goalBox.h, palette::white, over ? .22f : .08f);
        label(context, left + width - pad - goalWidth, y, goalWidth, goalName, over ? palette::text : palette::dim, Align::Right);
    } else label(context, left + width - pad - goalWidth, y, goalWidth, goalName, palette::faint, Align::Right);
    y += line;
    if (!creative && level >= 0) {
        int const need = next >= 0 ? active.lines[static_cast<size_t>(next)].cost : 0;
        label(context, left + pad, y, inner, translated("enchantOrder.anvil.have", level), level < need ? palette::warning : palette::dim);
    } else label(context, left + pad, y, inner, translated("feature.enchantOrder"), palette::faint);
    y += line + 3;
    constexpr float icon = 8, numberWidth = 10, costWidth = 16;
    auto sideAt = [&](float x, float rowY, float w, std::string const& text, bool item, Rgb color, bool enchanted = false) {
        // A stack of its own, drawn in its turn and not in a batch, so the held item drawn last is above them all.
        ItemStack const* stack = !item ? pooled(anvilBooks, anvilBookIcon, booksDrawn++)
            : enchanted ? pooled(anvilGlints, anvilItemGlint, itemsDrawn++) : pooled(anvilItems, anvilItemIcon, itemsDrawn++);
        if (stack) icons.push_back({stack, std::round(x), std::round(rowY), icon / 16, 0, list.opacity, true, true});
        label(context, x + icon + 2, rowY, w - icon - 2, text, color);
    };
    for (int i = 0; i < steps; ++i) {
        auto const& step = active.lines[static_cast<size_t>(i)];
        float const rowHeight = twoLines ? 2 * line : line;
        bool const finished = done(active.plan.steps[static_cast<size_t>(i)], atHandNow), marked = i == now, upNext = i == next;
        if (i > 0) fill(context, left + pad, y - 1, inner, 1, palette::white, .1f);
        if (marked) fill(context, left + 1, y, width - 2, rowHeight + 1, palette::accent, .16f);
        if (marked || upNext) fill(context, left + 1, y, 2, rowHeight + 1, marked ? palette::accent : palette::white);
        Rgb const color = finished ? palette::faint : upNext || marked ? palette::text : palette::dim;
        label(context, left + pad, y + 1, numberWidth, std::to_string(i + 1), finished ? palette::faint : palette::dim, Align::Right);
        float const textLeft = left + pad + numberWidth + 3, textWidthAll = inner - numberWidth - 3 - costWidth;
        if (twoLines) {
            sideAt(textLeft, y + 1, textWidthAll, step.kept, step.keptIsItem, step.keptCurse && !finished ? palette::curse : color,
                step.keptEnchanted);
            label(context, textLeft - 1, y + 1 + line, 6, "+", palette::faint);
            sideAt(textLeft + 6, y + 1 + line, textWidthAll - 6, step.gone, false, step.goneCurse && !finished ? palette::curse : color);
        } else {
            float const half = (textWidthAll - 8) / 2;
            sideAt(textLeft, y + 1, half, step.kept, step.keptIsItem, color, step.keptEnchanted);
            label(context, textLeft + half + 1, y + 1, 6, "+", palette::faint);
            sideAt(textLeft + half + 8, y + 1, half, step.gone, false, color);
        }
        bool const short_ = !creative && level >= 0 && level < step.cost && !finished;
        label(context, left + width - pad - costWidth, y + 1, costWidth, std::to_string(step.cost),
            finished ? palette::faint : short_ ? palette::warning : marked ? palette::accent : palette::text, Align::Right);
        y += rowHeight + 2;
    }
    y += 2;
    // What to do next, or what is wrong with the pair in the slots.
    if (both && now >= 0 && gameCost != active.lines[static_cast<size_t>(now)].cost)
        label(context, left + pad, y, inner, translated("enchantOrder.anvil.costDiffers", gameCost), palette::warning);
    else if (both && now >= 0) label(context, left + pad, y, inner, translated("enchantOrder.anvil.match", now + 1), palette::accent);
    else if (both) label(context, left + pad, y, inner,
        translated(!target ? "enchantOrder.anvil.leftNot" : !sacrifice ? "enchantOrder.anvil.rightNot" : "enchantOrder.anvil.notInPlan"), palette::warning);
    else if (next < 0) label(context, left + pad, y, inner, translated("enchantOrder.anvil.finished"), palette::accent);
    else {
        auto const& step = active.plan.steps[static_cast<size_t>(next)];
        if (!atHand(step.target, atHandNow)) label(context, left + pad, y, inner, translated("enchantOrder.anvil.missing", sideText(step.target)), palette::warning);
        else if (!atHand(step.sacrifice, atHandNow))
            label(context, left + pad, y, inner, translated("enchantOrder.anvil.missing", sideText(step.sacrifice)), palette::warning);
        else label(context, left + pad, y, inner, translated("enchantOrder.anvil.next", next + 1), palette::text);
    }
    y += line;
    if (leftPenalty >= 0 || rightPenalty >= 0)
        label(context, left + pad, y, inner, translated("enchantOrder.anvil.penalties", std::max(0, leftPenalty), std::max(0, rightPenalty)), palette::faint);
    if (!list.chooser) paintChange(context, change);
}
void paintList(MinecraftUIRenderContext& context, HeldIcon const* held = nullptr) {
    using namespace lamium::ui;
    if (list.opacity <= .02f) return;
    DrawOpacity const fade{list.opacity};
    Icons icons;
    Box change, goalBox, panelBox;
    if (list.chooser) {
        paintChooser(context, icons);
        auto const& l = list.choose;
        panelBox = {l.left, l.headTop, l.width, l.footerTop + EnchantLayout::statusHeight - l.headTop};
    }
    if (list.planned) paintSteps(context, icons, change, goalBox);
    else paintNoPlan(context, change);
    {
        std::scoped_lock lock(pressLock);
        pressState = {std::chrono::steady_clock::now(), list.pointerX, list.pointerY, list.free && list.opacity > .98f, change, goalBox, panelBox,
            list.chooser ? list.choose : EnchantLayout{}};
    }
    context.flushText(0, std::nullopt);
    // The held item goes with the panels' own icons, last so it is on top: a
    // glint drawn after another batch of icons does not show (ItemIcon.cpp).
    if (held) icons.push_back({held->stack, held->x, held->y, held->size / 16, 0, list.opacity, true, true});
    if (!icons.empty()) inspection::render::drawItemIcons(context, icons, 17);
    if (held && held->stack->mItem) {
        // Its durability bar as the game draws it on a slot, at the held item's size.
        int const maxDamage = static_cast<int>(held->stack->mItem->getMaxDamage());
        if (inspection::render::shouldShowDurabilityBar(held->stack->isDamageableItem(), held->stack->getDamageValue(), maxDamage)) {
            float const unit = held->size / 16, ratio = inspection::render::durabilityRatio(held->stack->getDamageValue(), maxDamage);
            auto const back = inspection::render::durabilityBackground({0, 0, 16, 16});
            auto const front = inspection::render::durabilityForeground(back, ratio);
            auto const color = inspection::render::durabilityColor(ratio);
            fill(context, held->x + back.x0 * unit, held->y + back.y0 * unit, back.width() * unit, back.height() * unit, Rgb{0, 0, 0});
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
    pendingWheel = 0;
}
// The anvil's left slot as an item cell. With nothing chosen yet, an item put there is taken as the plan's.
void seeSlot(ItemStack const& input) {
    Held now;
    readItem(now, input);
    bool const same = now.present == slotItem.present
        && (!now.present || (now.kind == slotItem.kind && now.work == slotItem.work && now.stateKnown == slotItem.stateKnown
               && now.name == slotItem.name && sameSet(now.base, slotItem.base)));
    if (same) return;
    slotItem = std::move(now);
    if (slotItem.present && slotItem.stateKnown && wanted.empty() && !active.valid) pickSlot();
    else rebuild();
}
// The presses and wheel turns taken since the last frame.
void applyPresses() {
    std::vector<Pending> presses;
    int wheel = 0;
    {
        std::scoped_lock lock(pressLock);
        presses.swap(pendingPresses);
        wheel = std::exchange(pendingWheel, 0);
    }
    auto const& rows = list.choose.rows;
    if (wheel) chooserFirst = ui::SettingsTable::clampFirst(chooserFirst + wheel, rows.listCount, rows.listVisible);
    for (auto const& press : presses) {
        if (press.goal) { flipGoal(); continue; }
        if (press.scrollbar) { chooserFirst = rows.firstAt(press.y); continue; }
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
    constexpr float gap = 4, pad = 4, line = 10, wide = 200, narrow = 110, chooseWide = 190, chooseNarrow = 130, chooseTall = 300;
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
    int now = -1, gameCost = -1, level = -1, leftPenalty = -1, rightPenalty = -1;
    bool both = false, creative = false;
    ItemStack const* heldItem = nullptr; // the stack on the pointer; the manager's, valid for this frame
    std::optional<unsigned> target, sacrifice;
    std::vector<unsigned> atHandNow;
    try {
        if (auto* player = context.mClient.getLocalPlayer()) {
            creative = player->isCreative();
            if (auto const* attribute = player->getAttribute(Player::LEVEL()).mPtr) level = static_cast<int>(attribute->mCurrentValue);
        }
        auto& screenController = static_cast<ContainerScreenController&>(*view.mController);
        if (auto manager = screenController.mContainerManagerController; manager && !manager->mContainersClosed) {
            auto const& input = manager->getItemStack("anvil_input_items", 0);
            auto const& material = manager->getItemStack("anvil_material_items", 0);
            // First what changes the plan, then the plan against the slots.
            if (chooser) {
                seeSlot(input);
                applyPresses();
            }
            auto const& onPointer = manager->getItemStack("cursor_items", 0);
            if (!onPointer.isNull()) heldItem = &onPointer;
            if (active.valid) {
                if (!input.isNull()) { target = sideOf(input); leftPenalty = input.getBaseRepairCost(); }
                if (!material.isNull()) { sacrifice = sideOf(material); rightPenalty = material.getBaseRepairCost(); }
                both = !input.isNull() && !material.isNull();
                if (target && sacrifice) now = enchantPlan::stepFor(active.plan, *target, *sacrifice);
                if (both) gameCost = static_cast<AnvilContainerManagerController&>(*manager).mCost;
                if (target) atHandNow.push_back(*target);
                if (sacrifice) atHandNow.push_back(*sacrifice);
                if (auto mask = sideOf(onPointer)) atHandNow.push_back(*mask);
                int const carriedSlots = std::min(36, manager->getContainerSize("combined_hotbar_and_inventory_items"));
                for (int i = 0; i < carriedSlots; ++i)
                    if (auto mask = sideOf(manager->getItemStack("combined_hotbar_and_inventory_items", i))) atHandNow.push_back(*mask);
            }
        }
    } catch (...) { now = -1; both = false; atHandNow.clear(); }
    bool const planned = active.valid && !active.lines.empty();
    int next = -1;
    if (planned) {
        atHandNow = progress(atHandNow, active.plan);
        next = nextStep(active.plan, atHandNow);
    }
    finishedAtAnvil = planned && next < 0;

    ui::EnchantLayout choose;
    if (chooser) {
        float const chooseWidth = std::min(chooseWide, roomLeft), chooseHeight = std::min(chooseTall, screen.y - 2 * gap);
        float const chooseTop = std::clamp(windowRest + (size.y - chooseHeight) / 2, gap, std::max(gap, screen.y - gap - chooseHeight)) + (at.y - windowRest);
        choose = ui::EnchantLayout::fitPanel(at.x - gap - chooseWidth, chooseTop, chooseWidth, chooseHeight, static_cast<int>(kinds.size()),
            shown.heldCell, shown.slotCell, static_cast<int>(shown.rows.size()), chooserFirst);
        chooserFirst = choose.rows.listFirst;
        chooser = choose.usable();
    }
    int const steps = static_cast<int>(active.lines.size());
    constexpr int headLines = 2, footLines = 2;
    float const changeSpace = chooser ? 0 : changeRow;
    auto heightFor = [&](bool twoLines) {
        return pad + headLines * line + 3 + steps * (twoLines ? 2 * line + 2 : line + 2) + 2 + footLines * line + changeSpace + pad;
    };
    bool const twoLines = heightFor(true) <= screen.y - 2 * gap;
    float const height = planned ? heightFor(twoLines) : pad + line + 3 + (chooser ? 3 * line : line) + changeSpace + pad;
    if (height > screen.y - 2 * gap) return;
    float const top = std::clamp(windowRest, gap, screen.y - gap - height) + (at.y - windowRest);
    // The game's window fades as it slides away on closing; the panels fade with the distance, or with the
    // window's own opacity when that is lower.
    float const slide = std::max(0.f, at.y - windowRest), way = std::max(1.f, (screen.y - windowRest) * .5f);
    float opacity = std::clamp(1 - slide / way, 0.f, 1.f);
    // Only while it is moving and only a value that looks like a fade: at rest the member may not be the drawn opacity.
    if (float const own = window->mAlpha; slide > .5f && std::isfinite(own) && own > 0 && own < opacity) opacity = own;
    list.ready = true;
    list.at = clock;
    list.left = left; list.top = top; list.width = width; list.height = height;
    list.twoLines = twoLines; list.both = both; list.creative = creative;
    list.target = target.has_value(); list.sacrifice = sacrifice.has_value();
    list.opacity = opacity;
    list.now = now; list.next = next; list.gameCost = gameCost; list.level = level;
    list.leftPenalty = leftPenalty; list.rightPenalty = rightPenalty;
    list.atHand = std::move(atHandNow);
    list.pointerX = view.mPointerLocationPrevious->x; list.pointerY = view.mPointerLocationPrevious->y;
    list.free = heldItem == nullptr;
    list.planned = planned;
    list.chooser = chooser;
    list.choose = choose;
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
    screenItemGlint = ItemStack();
    anvilItems.clear(); anvilBooks.clear(); screenItems.clear(); screenBooks.clear(); anvilGlints.clear(); screenGlints.clear();
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
    }
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
    if (state.goal.has(x, y)) {
        Pending press;
        press.goal = true;
        pendingPresses.push_back(press);
        pressSwallow = true;
        return 1;
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
    if (!state.live || std::chrono::steady_clock::now() - state.at > std::chrono::milliseconds(300)
        || !state.panel.has(state.pointerX, state.pointerY)) return false;
    pendingWheel += rows;
    return true;
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
ItemStack const* itemIcon(int number, bool enchanted) {
    auto const* source = itemIcon();
    if (!source) return nullptr;
    if (!enchanted) return pooled(screenItems, *source, number);
    if (screenItemGlint.isNull()) screenItemGlint = enchantedCopy(*source, wanted);
    return pooled(screenGlints, screenItemGlint, number);
}
ItemStack const* bookIcon(int number) { return pooled(screenBooks, book, number); }

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

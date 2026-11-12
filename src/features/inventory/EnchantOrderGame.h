#pragma once
#include "features/inventory/EnchantOrder.h"
#include <string>
#include <vector>
class IClientInstance;
class ItemStack;

// L-170: the enchantment order screen's data, read from the game when the
// screen opens, and the step list beside the anvil screen. The plan stays for
// the session after the screen closes. Client thread only.
namespace lamium::inventory::enchantOrder::game {
enum class RowState { Off, On, Fixed, Blocked, Full };
struct Row {
    int type = 0, level = 1, maxLevel = 1;
    std::string label;  // name and level, in the game's words
    std::string reason; // what blocks it, when blocked
    RowState state = RowState::Off;
    bool curse = false;
};
struct KindCell {
    std::string name;
    bool held = false; // the main-hand item is of this kind
};
struct StepLine {
    std::string kept, gone;
    bool keptIsItem = false;
    int cost = 0, penalty = 0;
    bool keptCurse = false, goneCurse = false; // that side is nothing but curses
};
// What the screen draws; rebuilt after every change.
struct View {
    std::vector<KindCell> kinds;
    int kind = -1;
    std::string itemName;
    bool inHand = false;     // the chosen kind is the main-hand item's
    bool stateKnown = false; // its anvil uses could be read
    bool useHeld = false;
    bool bookKind = false;   // books merged into one; there is no item
    bool planned = false;    // the totals below are valid
    std::vector<Row> rows;
    int wanted = 0;
    bool hasLast = false;
    enchantPlan::Goal goal = enchantPlan::Goal::FewestLevels;
    bool creative = false;
    bool costUnknown = false; // an enchantment's cost class is not one Lamium knows
    bool refused = false;     // every order has a step the anvil refuses
    std::vector<StepLine> steps;
    int total = 0, highest = 0, work = 0;
    bool otherKnown = false, sameForBoth = false;
    int otherTotal = 0, otherWork = 0;
};

// Reads the game's enchantments and the main-hand item. False with a
// translation key in `problem` when there is nothing to plan with.
bool open(IClientInstance& client, std::string& problem);
// Drops what was read; the plan stays for the anvil.
void close();
View const& view();
void pickKind(int index);
void setUseHeld(bool use);
Change toggleRow(int row);
void stepRow(int row, int delta);
void setAllHighest();
void restoreLast();
void clearWanted();
void flipGoal();
// Icons, valid while the screen is open.
ItemStack const* kindIcon(int index);
ItemStack const* itemIcon();
ItemStack const* bookIcon();

bool start();
void stop();
}

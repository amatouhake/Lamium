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
    std::string name;   // the name alone
    std::string levelText; // the level alone ("V"); empty when it has only one
    int carried = 0;       // the level the held item already has; above it the row is raising it
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
    bool useHeld = false;    // the main-hand item's cell is chosen
    bool heldCell = false;   // there is such a cell
    std::string heldName;
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
// A kind plans a fresh item of it.
void pickKind(int index);
// The main-hand item: what it carries stays and its anvil uses are charged.
void pickHeld();
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
ItemStack const* heldIcon();
ItemStack const* bookIcon();
// The same pictures as stacks of their own, by a running number within a frame: drawing one stack many
// times in a frame loses the glints.
ItemStack const* itemIcon(int number);
ItemStack const* bookIcon(int number);

bool start();
void stop();
}

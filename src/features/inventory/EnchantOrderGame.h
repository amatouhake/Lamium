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
// One name of a step's side and the plan input it stands for (enchantPlan's bit), so a name can be marked
// when that input is not at hand.
struct NamePart {
    std::string text;
    unsigned bit = 0;
};
struct StepLine {
    std::string kept, gone;
    bool keptIsItem = false;
    int cost = 0, penalty = 0;
    bool keptCurse = false, goneCurse = false; // that side is nothing but curses
    bool keptEnchanted = false; // the kept side is the item and it carries an enchantment by this step
    std::vector<NamePart> keptParts, goneParts;
};
// The steps' side as both screens draw it (ui/EnchantSteps): the plan and where the player stands with it.
struct StepsInfo {
    bool planned = false;
    std::string message; // said in place of the steps when there are none
    bool warning = false;
    enchantPlan::Goal goal = enchantPlan::Goal::FewestLevels;
    bool creative = false;
    int level = -1; // the player's; -1 when it does not matter (Creative) or is not known
    int total = 0, highest = 0, work = 0;
    bool otherKnown = false, sameForBoth = false;
    int otherTotal = 0, otherWork = 0;
    std::vector<StepLine> steps;
    Following following;
    int inSlots = -1; // the step whose two stacks lie in the anvil's slots
    std::string note; // the last line: what to do next
    int noteTone = 0; // 0 plain, 1 good, 2 warning
};
// What the screen draws; rebuilt after every change.
struct View {
    std::vector<KindCell> kinds;
    int kind = -1;
    std::string itemName;
    bool inHand = false;     // the chosen kind is the main-hand item's
    bool stateKnown = false; // its anvil uses could be read
    bool useItem = false;    // planned from an item's state, not a fresh one of the kind
    bool useHeld = false;    // and that item is the one in the main hand: its cell is chosen
    bool heldCell = false;   // there is such a cell
    std::string heldName;
    bool useSlot = false;    // the same for the item in the anvil's left slot (anvil screen only)
    bool slotCell = false;
    std::string slotName;
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
// `atAnvil`: opened by the panel beside the anvil screen, which closes it again.
bool open(IClientInstance& client, std::string& problem, bool atAnvil = false);
// Drops what was read; the plan stays for the anvil.
void close();
View const& view();
// A kind plans a fresh item of it.
void pickKind(int index);
// The main-hand item: what it carries stays and its anvil uses are charged.
void pickHeld();
// The same for the item in the anvil's left slot.
void pickSlot();
Change toggleRow(int row);
void stepRow(int row, int delta);
void setAllHighest();
void restoreLast();
void clearWanted();
void flipGoal();
// The steps' side. In the planner screen it is brought up to date from the player's inventory every frame;
// at the anvil the panel does that itself.
void followInventory(IClientInstance& client);
StepsInfo const& steps();
// The steps' icons, each a stack of its own by a running number within a frame.
ItemStack const* stepItemIcon(int number, bool enchanted);
ItemStack const* stepBookIcon(int number);
// Icons, valid while the screen is open.
// A mouse button changed, told from the window procedure (values only, no game state). 0: not Lamium's. 1: a
// press on the panels beside the anvil, kept from the game; what it does happens on the client thread with the
// next frame. 2: the same on the Change button, which the caller answers by opening the planner on the client
// thread.
int anvilPress(bool left, bool down);
// The wheel over the anvil's choosing panel, in rows: true when the panel takes it.
bool anvilWheel(int rows);
ItemStack const* kindIcon(int index);
ItemStack const* itemIcon();
ItemStack const* heldIcon();
ItemStack const* slotIcon();
ItemStack const* bookIcon();
// The chosen item's picture as a stack of its own, by a running number within a frame: drawing one stack many
// times in a frame loses the glints.
ItemStack const* itemIcon(int number);

bool start();
void stop();
}

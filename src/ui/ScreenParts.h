#pragma once
// What the settings screen's core (SettingsScreen.cpp) offers the views it
// shows (BACKLOG L-134): the footer message, the one number being typed,
// navigation and closing, and where the player stands. Each view owns its
// own state in its own file (ShapesView, WaypointsView, ...) and reaches the
// rest of the screen only through these. Everything runs on the client
// thread under the screen's lock.
#include "ui/NumberInput.h"
#include "ui/SettingsTable.h"
#include "ui/ShapesLayout.h"
#include <optional>
#include <string>
#include <string_view>
class IClientInstance;
namespace lamium::ui::screen {
// The client while the screen is open, else null.
IClientInstance* client();

// The footer message every view shows instead of its description: a failed
// save, a refused value. Empty when there is none.
std::string const& message();
void setMessage(std::string text);
void clearMessage();
// A range warning goes again once the user moves to another tab, row or
// field (L-81).
void warnRange(std::string text);

// The one number being typed, into whichever stepper is editing.
NumberInput& number();
// The typed number changed: the core asks the editing view to apply it.
void numberTyped();
// Applies and ends any typing (a number or a name) in every view.
void finishEditing();

bool heldCtrl();
bool heldShift();
void close();
// Back to the world map the view was opened from.
void returnToMap();
// Tab / Shift+Tab: the next or previous navigation item.
void nextNav(bool back);
// A click on the sidebar or the version in an undocked view; true if it
// was one of them.
bool navClick(float x, float y);
// A press on a list's scrollbar moves the list there and starts a drag.
bool pressScrollbar(ShapesLayout const& layout, int& first, float x, float y);
// Flips a boolean option by id ("overlays.shapes").
void toggleOption(std::string_view id);
// Shows a feature's row, expanded, in its settings category.
void showFeatureKeys(std::string_view featureId);
// The settings table as last laid out (undocked views fit inside it).
SettingsTable const& table();

// Where the local player stands (block, dimension), if anywhere.
struct Place { int x, y, z, dimension; };
std::optional<Place> standingPlace();
int playerDimension();
std::string dimensionName(int dimension);
// Blocks from the player to (x, z), horizontally.
int distanceTo(int x, int z);
}

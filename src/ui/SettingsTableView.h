#pragma once
// The settings table of the settings screen (BACKLOG L-134): the sidebar or
// tabs, search, the rows of features, options and key bindings (Hotkeys
// included), key capture and the footer, and the frame the undocked tool
// views draw in. Its state lives in SettingsTableView.cpp; the screen's core
// forwards input and asks it to draw.
#include "input/Binding.h"
#include "ui/SettingsTable.h"
#include <glm/vec2.hpp>
#include <string>
#include <string_view>
class IClientInstance;
class MinecraftUIRenderContext;
namespace lamium::ui::table_view {
// The screen opens again: search, typing and capture end; rows are rebuilt
// keeping the selection.
void open();
// The screen is gone.
void reset();
// Another category is chosen: a search, which spans every category, ends.
void endSearch();
// The category changed: from the top, rows rebuilt.
void enterCategory();
void rebuild(bool keepSelection);

bool searchFocused();
// Ctrl+F: the search field takes the typing, its text selected.
void focusSearch();
// Text from the native keyboard for the search field.
void typeSearch(std::string const& text);
void unfocusSearch();

// A number typed into an option row (screen::number()).
bool editingNumber();
void applyNumber();
void endEditing();
// Flips or steps an option and saves.
void adjustOption(std::string_view id, int direction);

// Key capture for a binding.
bool capturing();
void captureInput(input::Token token, bool down);
void cancelCapture();
// A captured binding is saved with the next frame, outside input events.
void applyBinding();
// Whether (x, y) is on one of the capture footer's buttons.
bool onCaptureButton(float x, float y);

void click(float x, float y, bool right);
void key(int key);
void wheel(int step);
// The left button let go: a slider drag ends.
void release();
// The armed reset ends when the screen closes.
void disarm();
glm::vec2 caret();
void render(MinecraftUIRenderContext& context, IClientInstance& client, glm::vec2 size, glm::vec2 pointer);

// The table as last laid out, for the views drawn inside it.
SettingsTable const& layout();
float tabWidth();
void copyVersion();
// Where a range warning was raised (L-81).
int selectedRow();
// A feature's row, expanded: expand before its category is chosen, select
// after, so the row is there and in view.
void expand(std::string_view featureId);
void selectFeature(std::string_view featureId);
}

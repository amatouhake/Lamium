#pragma once
#include <string>

namespace lamium {
// Hands a web link to the system's browser; false when it could not.
bool openUrl(std::string const& url);
// Puts text on the system clipboard; false when it could not.
bool copyText(std::string const& text);
// The clipboard's text as UTF-8; empty when there is none.
std::string clipboardText();
// Whether the keyboard key (a virtual-key code) is held down right now.
bool keyDown(int key);
// Windows' name for the key (a virtual-key code) in the current keyboard layout; empty when it has none.
std::string keyName(int key);
}

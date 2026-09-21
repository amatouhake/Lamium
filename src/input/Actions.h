#pragma once
#include <string_view>
namespace lamium {
inline bool gameplayScreen(std::string_view name) { return name.starts_with("hud_screen"); }
void registerActions();
}

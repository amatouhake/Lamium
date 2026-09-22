#pragma once
#include <string>
#include <string_view>
#include "input/Binding.h"
class IClientInstance;
namespace lamium {
inline bool gameplayScreen(std::string_view name) { return name.starts_with("hud_screen"); }
void registerActions();
std::string gameplayKeyHint(IClientInstance& client);
std::string actionBindingName(IClientInstance& client, input::Action action);
}

#pragma once
#include <string>
#include <string_view>
class IClientInstance;
namespace lamium {
inline bool gameplayScreen(std::string_view name) { return name.starts_with("hud_screen"); }
void registerActions();
std::string gameplayKeyHint(IClientInstance& client);
}

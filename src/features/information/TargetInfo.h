#pragma once
#include <optional>
#include <string>
#include <vector>
class IClientInstance;
namespace lamium::information {
// Owned snapshot shared by target HUD and future detailed debug providers.
struct TargetInfo { std::string name, identifier; std::vector<std::string> states; };
std::optional<TargetInfo> collectTargetInfo(IClientInstance&, bool includeStates = false);
}

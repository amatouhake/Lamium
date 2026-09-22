#pragma once
#include <optional>
#include <string>
class IClientInstance;
namespace lamium::information {
// Owned snapshot shared by target HUD and future detailed debug providers.
struct TargetInfo { std::string name, identifier; };
std::optional<TargetInfo> collectTargetInfo(IClientInstance&);
}

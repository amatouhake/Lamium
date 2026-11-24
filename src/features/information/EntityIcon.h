#pragma once
#include <string>
class IClientInstance;
namespace lamium::information {
// The item that stands for an entity in an icon: its spawn egg (by name, then
// by the item registry's egg for that actor), else an item of the same kind
// (painting, minecart, arrow), else empty. Shared by the target card and the
// schematic's entity lines, so one entity looks the same in both. Client
// thread.
std::string entityIconItem(IClientInstance&, std::string const& identifier);
// The eggs indexed by actor id belong to one world (add-ons bring their own).
void clearSpawnEggIndex();
}

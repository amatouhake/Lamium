#pragma once
#include "features/information/DebugLines.h"
#include <optional>
#include <vector>
class IClientInstance;
// Client-side counts for the Debug View (BACKLOG L-57). Read-only, at most once
// a second; a count that cannot be read is left out.
namespace lamium::information {
struct ClientCounters {
    std::optional<int> entities;  // client actors in the player's dimension
    std::optional<int> chunks;    // chunks the client's chunk source holds for that dimension
    std::optional<int> particles; // legacy particles plus particle-system particles
    std::optional<EntityKinds> entityKinds; // the entities by kind (L-120)
    std::vector<TypeCount> entityTypes;     // every type present, with its localized name
};
ClientCounters clientCounters(IClientInstance&);
}

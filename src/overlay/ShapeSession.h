#pragma once
#include "overlay/ShapeCollection.h"
#include "overlay/ShapeStore.h"
#include <optional>

namespace lamium::overlay::shapes {
// UI-facing value snapshots never expose render geometry or game pointers.
struct Summary { ShapeId id; ShapeDefinition definition; };
std::vector<Summary> list();
std::optional<ShapeDefinition> find(ShapeId);
ShapeId add(ShapeDefinition);
void edit(ShapeId, ShapeDefinition);
void setVisible(ShapeId, bool);
void rename(ShapeId, std::string);
bool remove(ShapeId);
void clear();
// The shapes of a dimension seen from above (the maps, L-139), each box
// computed when its shape changes and kept until then.
struct Footprint {
    ShapeId id;
    std::string name;
    ShapeColor color;
    bool visible;
    ShapeFootprint area;
};
std::vector<Footprint> footprints(int dimension);
// Previews a definition that is not yet part of the collection; nullopt ends it.
void setDraft(std::optional<ShapeDefinition>);
enum class Storage { Session, LocalWorld, LoadFailed };
Storage storage();
}

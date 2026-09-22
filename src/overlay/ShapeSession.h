#pragma once
#include "overlay/ShapeCollection.h"
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
}

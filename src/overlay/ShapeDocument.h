#pragma once
#include "overlay/ShapeCollection.h"
#include <string_view>

namespace lamium::overlay {
// Versioned definition-only document. Runtime IDs and derived geometry are not
// persisted. Decoding validates the whole collection before returning it.
std::vector<ShapeDefinition> decodeShapes(std::string_view);
std::string encodeShapes(std::vector<ShapeDefinition> const&);
}

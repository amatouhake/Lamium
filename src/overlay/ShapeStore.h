#pragma once
#include "overlay/ShapeDocument.h"
#include <filesystem>

namespace lamium::overlay {
class ShapeSaveError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
std::vector<ShapeDefinition> readShapes(std::filesystem::path const&);
// Validate and write a complete sibling temporary file before replacement.
// A failed save preserves the destination and never removes another writer's file.
void writeShapes(std::filesystem::path const&, std::vector<ShapeDefinition> const&);
}

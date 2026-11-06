#pragma once
#include "features/schematic/Structure.h"

// The runtime's inflater for the compressed body 1.26.60 writes (L-307), kept
// out of Structure.cpp so the parser stays game-free and its tests supply
// their own.
namespace lamium::schematic {
std::optional<std::string> inflateStructureBody(std::span<std::uint8_t const> bytes, std::size_t maxSize);
}

#pragma once
#include "features/schematic/Nbt.h"
#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

// A .mcstructure file as data (BACKLOG L-93): size, block palette, the two
// block layers, block entity data and entities. No game types.
namespace lamium::schematic {
struct Size {
    int x = 0, y = 0, z = 0;
    bool operator==(Size const&) const = default;
};
struct PaletteBlock {
    std::string name;
    nbt::Compound states;
    std::int32_t version = 0;
    bool isAir() const { return name == "minecraft:air"; }
    // Name plus states sorted by name, e.g. `minecraft:oak_stairs[upside_down_bit=0b,weirdo_direction=1]`;
    // equal keys mean the same block state.
    std::string key() const;
};
struct EntityRecord {
    std::string identifier;
    // Relative to the structure's lower north-west corner.
    double x = 0, y = 0, z = 0;
    nbt::Compound data;
};
// A cell that places nothing (structure void).
inline constexpr std::int32_t voidCell = -1;
// Guards against corrupt sizes; far above the vanilla 64x384x64 limit.
inline constexpr std::uint64_t maxCells = 16ull * 1024 * 1024;
// What writeStructure writes: int-array layers, as the game's own exports.
inline constexpr std::int32_t writtenFormatVersion = 2;

struct Structure {
    Size size;
    std::array<int, 3> worldOrigin{};
    std::int32_t formatVersion = writtenFormatVersion; // as read; writing always uses writtenFormatVersion
    std::vector<PaletteBlock> palette;
    // Palette index per cell, or voidCell. `liquids` is the second layer
    // (water in a waterlogged block) and is empty when it holds nothing; it
    // is written only when some cell has one, like the game's exports.
    std::vector<std::int32_t> blocks, liquids;
    std::map<std::int32_t, nbt::Compound> blockEntities; // cell index -> block_entity_data
    std::vector<EntityRecord> entities;

    std::uint64_t cells() const { return static_cast<std::uint64_t>(size.x) * size.y * size.z; }
    // Cells run in ZYX order: z fastest, then y, then x.
    std::int32_t cell(int x, int y, int z) const { return (x * size.y + y) * size.z + z; }
    std::array<int, 3> position(std::int32_t cell) const {
        return {cell / (size.y * size.z), cell / size.z % size.y, cell % size.z};
    }
};

// Throws std::runtime_error with a reason when the file is not a usable structure.
Structure parseStructure(std::span<std::uint8_t const> bytes);
std::string writeStructure(Structure const& structure);
}

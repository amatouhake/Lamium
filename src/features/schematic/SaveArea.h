#pragma once
#include "features/schematic/Placement.h"
#include "features/schematic/Structure.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <string_view>

// Saving an area of the world as a .mcstructure (BACKLOG L-93): the area
// between two corner blocks, the file name typed for it, and a builder that
// collects blocks cell by cell. Reading the world is glue.
namespace lamium::schematic {
struct Area {
    Point a, b; // the two corner blocks, both inside the area
    Point low() const { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
    Size size() const { return {std::abs(a.x - b.x) + 1, std::abs(a.y - b.y) + 1, std::abs(a.z - b.z) + 1}; }
    std::uint64_t cells() const {
        auto s = size();
        return static_cast<std::uint64_t>(s.x) * s.y * s.z;
    }
};
inline constexpr size_t maxSaveName = 64; // bytes of the name, without the extension

// The area cut along chunk borders. A save reads one column when its chunk
// is loaded and waits for the others, so an area larger than the render
// distance can be saved by walking along it.
struct Column {
    int chunkX = 0, chunkZ = 0;
    int lowX = 0, highX = 0, lowZ = 0, highZ = 0; // world blocks, inclusive
    std::uint64_t cells(int height) const {
        return static_cast<std::uint64_t>(highX - lowX + 1) * (highZ - lowZ + 1) * static_cast<std::uint64_t>(height);
    }
};
// The direction from one point to another on the map, as one of eight:
// 0 north (-z), 1 north-east, 2 east (+x), ... 7 north-west.
inline int compassOctant(double dx, double dz) {
    double angle = std::atan2(dx, -dz); // 0 toward north, clockwise
    int octant = static_cast<int>(std::lround(angle / (3.14159265358979323846 / 4)));
    return ((octant % 8) + 8) % 8;
}
inline int chunkOf(int block) { return block >= 0 ? block / 16 : -((-block + 15) / 16); }
inline std::vector<Column> chunkColumns(Area const& area) {
    Point low = area.low();
    Size size = area.size();
    std::vector<Column> out;
    for (int cx = chunkOf(low.x); cx <= chunkOf(low.x + size.x - 1); ++cx)
        for (int cz = chunkOf(low.z); cz <= chunkOf(low.z + size.z - 1); ++cz)
            out.push_back({cx, cz, std::max(low.x, cx * 16), std::min(low.x + size.x - 1, cx * 16 + 15),
                           std::max(low.z, cz * 16), std::min(low.z + size.z - 1, cz * 16 + 15)});
    return out;
}

// A typed name as a file name in the schematics folder: characters a Windows
// file name cannot hold are dropped, surrounding spaces and trailing dots
// trimmed, a typed ".mcstructure" not doubled. Empty when nothing usable is left.
inline std::string schematicFileName(std::string_view typed) {
    constexpr std::string_view extension = ".mcstructure";
    if (typed.size() >= extension.size() && typed.substr(typed.size() - extension.size()) == extension)
        typed.remove_suffix(extension.size());
    std::string name;
    for (char c : typed) {
        auto u = static_cast<unsigned char>(c);
        if (u < 32 || std::string_view("<>:\"/\\|?*").find(c) != std::string_view::npos) continue;
        name += c;
    }
    auto first = name.find_first_not_of(' ');
    if (first == std::string::npos) return {};
    name.erase(0, first);
    if (name.size() > maxSaveName) {
        size_t cut = maxSaveName;
        // Do not split a UTF-8 sequence.
        while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xc0) == 0x80) --cut;
        name.resize(cut);
    }
    while (!name.empty() && (name.back() == ' ' || name.back() == '.')) name.pop_back();
    return name.empty() ? std::string{} : name + std::string(extension);
}

// Fills a structure cell by cell. Equal block states share one palette entry;
// cells not set stay structure void.
class StructureBuilder {
public:
    StructureBuilder(Size size, Point worldOrigin) {
        out.size = size;
        out.worldOrigin = {worldOrigin.x, worldOrigin.y, worldOrigin.z};
        out.blocks.assign(out.cells(), voidCell);
    }
    void setBlock(std::int32_t cell, PaletteBlock const& block) { out.blocks[static_cast<size_t>(cell)] = paletteIndex(block); }
    // The second layer: water in a waterlogged block.
    void setLiquid(std::int32_t cell, PaletteBlock const& block) {
        if (out.liquids.empty()) out.liquids.assign(out.cells(), voidCell);
        out.liquids[static_cast<size_t>(cell)] = paletteIndex(block);
    }
    void addEntity(EntityRecord entity) { out.entities.push_back(std::move(entity)); }
    // A cell's block entity data (bed color, sign text, banner, skull).
    void setBlockEntity(std::int32_t cell, nbt::Compound data) { out.blockEntities[cell] = std::move(data); }
    Structure const& structure() const { return out; }

private:
    std::int32_t paletteIndex(PaletteBlock const& block) {
        auto [found, added] = index.try_emplace(block.key(), static_cast<std::int32_t>(out.palette.size()));
        if (added) out.palette.push_back(block);
        return found->second;
    }
    Structure out;
    std::map<std::string, std::int32_t> index;
};
}

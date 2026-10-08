#include "features/schematic/Structure.h"
#include <algorithm>
#include <charconv>
#include <climits>
#include <format>
#include <stdexcept>

namespace lamium::schematic {
namespace {
[[noreturn]] void fail(std::string const& what) { throw std::runtime_error("Structure: " + what); }

nbt::Compound const& compound(nbt::Compound const& parent, char const* name) {
    auto const* tag = parent.find(name);
    auto const* value = tag ? tag->as<nbt::Compound>() : nullptr;
    if (!value) fail(std::format("missing compound '{}'", name));
    return *value;
}
nbt::List const& list(nbt::Compound const& parent, char const* name) {
    auto const* tag = parent.find(name);
    auto const* value = tag ? tag->as<nbt::List>() : nullptr;
    if (!value) fail(std::format("missing list '{}'", name));
    return *value;
}
std::array<std::int64_t, 3> triple(nbt::Compound const& parent, char const* name) {
    auto const& values = list(parent, name);
    if (values.items.size() != 3) fail(std::format("'{}' must hold three values", name));
    std::array<std::int64_t, 3> out{};
    for (int i = 0; i < 3; ++i) if (!values.items[i].integer(out[i])) fail(std::format("'{}' must hold integers", name));
    return out;
}
// A layer is an int array (current exports) or a list of ints (older ones).
std::vector<std::int32_t> layer(nbt::Tag const& values, std::uint64_t cells, size_t palette, char const* which) {
    std::vector<std::int32_t> out;
    if (auto const* array = values.as<std::vector<std::int32_t>>()) out = *array;
    else if (auto const* list = values.as<nbt::List>()) {
        out.reserve(list->items.size());
        for (auto const& item : list->items) {
            std::int64_t index;
            if (!item.integer(index)) fail("block indices must be integers");
            out.push_back(static_cast<std::int32_t>(std::clamp<std::int64_t>(index, INT32_MIN, INT32_MAX)));
        }
    } else fail(std::format("the {} block layer is neither an int array nor a list", which));
    if (out.size() != cells) fail(std::format("the {} block layer has {} cells, expected {}", which, out.size(), cells));
    for (auto index : out)
        if (index < voidCell || index >= static_cast<std::int64_t>(palette))
            fail(std::format("the {} block layer names palette entry {}, the palette has {}", which, index, palette));
    return out;
}
nbt::Tag intList(std::initializer_list<std::int32_t> values) {
    nbt::List out{nbt::Type::Int, {}};
    for (auto value : values) out.items.push_back({value});
    return {std::move(out)};
}
}

std::string PaletteBlock::key() const {
    std::vector<nbt::Entry const*> sorted;
    for (auto const& entry : states.entries) sorted.push_back(&entry);
    std::sort(sorted.begin(), sorted.end(), [](auto a, auto b) { return a->name < b->name; });
    std::string out = name;
    if (sorted.empty()) return out;
    out += '[';
    for (size_t i = 0; i < sorted.size(); ++i) out += (i ? "," : "") + sorted[i]->name + "=" + nbt::text(sorted[i]->tag);
    return out + ']';
}

Structure parseStructure(std::span<std::uint8_t const> bytes) {
    auto root = nbt::read(bytes).compound;
    Structure out;
    if (auto const* version = root.find("format_version")) {
        std::int64_t value;
        if (version->integer(value)) out.formatVersion = static_cast<std::int32_t>(value);
    }
    auto size = triple(root, "size");
    for (auto value : size) if (value < 1 || value > 1 << 20) fail(std::format("size {} out of range", value));
    if (static_cast<std::uint64_t>(size[0]) * size[1] * size[2] > maxCells) fail("too many cells");
    out.size = {static_cast<int>(size[0]), static_cast<int>(size[1]), static_cast<int>(size[2])};
    if (root.find("structure_world_origin")) {
        auto origin = triple(root, "structure_world_origin");
        for (int i = 0; i < 3; ++i) out.worldOrigin[i] = static_cast<int>(origin[i]);
    }

    auto const& structure = compound(root, "structure");
    auto const& palette = compound(compound(structure, "palette"), "default");
    for (auto const& item : list(palette, "block_palette").items) {
        auto const* entry = item.as<nbt::Compound>();
        auto const* name = entry && entry->find("name") ? entry->find("name")->as<std::string>() : nullptr;
        if (!name) fail("a palette entry has no name");
        PaletteBlock block{*name, {}, 0};
        if (auto const* states = entry->find("states"); states && states->as<nbt::Compound>()) block.states = *states->as<nbt::Compound>();
        if (auto const* version = entry->find("version")) {
            std::int64_t value;
            if (version->integer(value)) block.version = static_cast<std::int32_t>(value);
        }
        out.palette.push_back(std::move(block));
    }

    auto const& layers = list(structure, "block_indices");
    if (layers.items.empty() || layers.items.size() > 2) fail("block_indices must hold one or two layers");
    for (size_t i = 0; i < layers.items.size(); ++i) {
        // A second-layer cell over air (water in an air cell) and palette
        // entries no cell uses are valid: vanilla exports have both.
        auto cells = layer(layers.items[i], out.cells(), out.palette.size(), i == 0 ? "first" : "second");
        if (i == 0) out.blocks = std::move(cells);
        else if (std::any_of(cells.begin(), cells.end(), [](auto c) { return c != voidCell; })) out.liquids = std::move(cells);
    }

    if (auto const* data = palette.find("block_position_data"); data && data->as<nbt::Compound>()) {
        for (auto const& [key, value] : data->as<nbt::Compound>()->entries) {
            std::int32_t cell = -1;
            auto [end, error] = std::from_chars(key.data(), key.data() + key.size(), cell);
            if (error != std::errc{} || end != key.data() + key.size() || cell < 0
                || static_cast<std::uint64_t>(cell) >= out.cells()) continue;
            auto const* entry = value.as<nbt::Compound>();
            auto const* blockEntity = entry ? entry->find("block_entity_data") : nullptr;
            if (blockEntity && blockEntity->as<nbt::Compound>()) out.blockEntities[cell] = *blockEntity->as<nbt::Compound>();
        }
    }

    if (auto const* entities = structure.find("entities"); entities && entities->as<nbt::List>()) {
        for (auto const& item : entities->as<nbt::List>()->items) {
            auto const* entity = item.as<nbt::Compound>();
            if (!entity) continue;
            EntityRecord record;
            record.data = *entity;
            if (auto const* id = entity->find("identifier"); id && id->as<std::string>()) record.identifier = *id->as<std::string>();
            if (auto const* pos = entity->find("Pos"); pos && pos->as<nbt::List>() && pos->as<nbt::List>()->items.size() == 3) {
                auto const& p = pos->as<nbt::List>()->items;
                auto coordinate = [&](int i) -> double {
                    if (auto const* f = p[i].as<float>()) return *f;
                    if (auto const* d = p[i].as<double>()) return *d;
                    return 0;
                };
                record.x = coordinate(0) - out.worldOrigin[0];
                record.y = coordinate(1) - out.worldOrigin[1];
                record.z = coordinate(2) - out.worldOrigin[2];
            }
            out.entities.push_back(std::move(record));
        }
    }
    return out;
}

std::string writeStructure(Structure const& structure) {
    auto cells = structure.cells();
    if (!cells || cells > maxCells) fail("size out of range");
    if (structure.blocks.size() != cells || (!structure.liquids.empty() && structure.liquids.size() != cells))
        fail("layer sizes do not match the size");

    nbt::List palette{nbt::Type::Compound, {}};
    for (auto const& block : structure.palette) {
        nbt::Compound entry;
        entry.set("name", {block.name});
        entry.set("states", {block.states});
        entry.set("version", {block.version});
        palette.items.push_back({std::move(entry)});
    }
    nbt::Compound positions;
    for (auto const& [cell, data] : structure.blockEntities) {
        nbt::Compound entry;
        entry.set("block_entity_data", {data});
        positions.set(std::to_string(cell), {std::move(entry)});
    }
    nbt::Compound paletteDefault;
    paletteDefault.set("block_palette", {std::move(palette)});
    paletteDefault.set("block_position_data", {std::move(positions)});
    nbt::Compound palettes;
    palettes.set("default", {std::move(paletteDefault)});

    nbt::List layers{nbt::Type::IntArray, {}};
    layers.items.push_back({structure.blocks});
    if (std::any_of(structure.liquids.begin(), structure.liquids.end(), [](auto c) { return c != voidCell; }))
        layers.items.push_back({structure.liquids});
    nbt::List entities{nbt::Type::Compound, {}};
    for (auto const& entity : structure.entities) entities.items.push_back({entity.data});

    nbt::Compound body;
    body.set("block_indices", {std::move(layers)});
    body.set("entities", {std::move(entities)});
    body.set("palette", {std::move(palettes)});

    nbt::Root root;
    root.compound.set("format_version", {writtenFormatVersion});
    root.compound.set("size", intList({structure.size.x, structure.size.y, structure.size.z}));
    root.compound.set("structure", {std::move(body)});
    root.compound.set("structure_world_origin",
        intList({structure.worldOrigin[0], structure.worldOrigin[1], structure.worldOrigin[2]}));
    return nbt::write(root);
}
}

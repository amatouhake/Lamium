#pragma once
// A structure as a placement turns and mirrors it, at no origin: what the
// Check tab's preview draws, so it looks like the build in the world (L-114).
// Cells, block entity cells, entity positions and facings move; palette
// entries stay as the file has them (their states are turned where the game
// blocks are looked up). Pure.
#include "features/schematic/Placement.h"
#include "features/schematic/Structure.h"

namespace lamium::schematic {
inline Structure placedStructure(Structure const& s, Placement turn) {
    turn.origin = {};
    Structure out;
    out.size = placedSize(s.size, turn.rotation);
    out.worldOrigin = s.worldOrigin;
    out.formatVersion = s.formatVersion;
    out.palette = s.palette;
    auto const total = static_cast<size_t>(out.cells());
    if (s.blocks.size() != total) return s; // a broken file draws as it is
    out.blocks.assign(total, voidCell);
    if (!s.liquids.empty()) out.liquids.assign(total, voidCell);
    for (int x = 0; x < s.size.x; ++x)
        for (int y = 0; y < s.size.y; ++y)
            for (int z = 0; z < s.size.z; ++z) {
                auto from = static_cast<size_t>(s.cell(x, y, z));
                auto p = localToPlaced(s.size, turn, {x, y, z});
                auto to = static_cast<size_t>(out.cell(p.x, p.y, p.z));
                out.blocks[to] = s.blocks[from];
                if (from < s.liquids.size()) out.liquids[to] = s.liquids[from];
            }
    for (auto const& [cell, data] : s.blockEntities) {
        auto [x, y, z] = s.position(cell);
        auto p = localToPlaced(s.size, turn, {x, y, z});
        out.blockEntities.emplace(out.cell(p.x, p.y, p.z), data);
    }
    out.entities.reserve(s.entities.size());
    for (auto entity : s.entities) {
        auto p = toWorldPosition(s.size, turn, {entity.x, entity.y, entity.z});
        entity.x = p.x;
        entity.y = p.y;
        entity.z = p.z;
        if (auto* rotation = entity.data.find("Rotation"); rotation && rotation->as<nbt::List>() && !rotation->as<nbt::List>()->items.empty()) {
            auto list = *rotation->as<nbt::List>();
            if (auto* yaw = list.items.front().as<float>()) *yaw = toWorldYaw(*yaw, turn);
            entity.data.set("Rotation", {std::move(list)});
        }
        out.entities.push_back(std::move(entity));
    }
    return out;
}
}

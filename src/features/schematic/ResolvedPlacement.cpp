#include "features/schematic/ResolvedPlacement.h"
#include "features/schematic/Verification.h"
#include "mc/locale/I18n.h"
#include "mc/util/Mirror.h"
#include "mc/util/Rotation.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockRenderLayer.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/block/states/VanillaBlockStateTransformUtils.h"
#include <algorithm>
#include <cmath>
#include <format>

namespace lamium::schematic::ghosts {
namespace {
::Rotation gameRotation(int quarterTurns) {
    switch (quarterTurns) {
    case 1: return ::Rotation::Clockwise90;
    case 2: return ::Rotation::Clockwise180;
    case 3: return ::Rotation::CounterClockwise90;
    default: return ::Rotation::None;
    }
}
// The game names a mirror by the axis it reflects across: its Z mirror
// flips east-west states, Lamium's X (seen in game with `mixture` mirrored
// beside a structure block, 2026-10-09: chests, stairs, pane and fence
// connections all faced the other way).
::Mirror gameMirror(Mirror mirror) {
    return mirror == Mirror::X ? ::Mirror::Z : mirror == Mirror::Z ? ::Mirror::X : ::Mirror::None;
}
bool flagged(nbt::Compound const& states, char const* name) {
    std::int64_t value = 0;
    auto const* tag = states.find(name);
    return tag && tag->integer(value) && value != 0;
}
ItemInfo itemFor(PaletteBlock const& entry, Block const* block) {
    if (entry.isAir()) return {};
    ItemInfo out = block ? describe(*block, entry.name) : ItemInfo{"", entry.name, "", 1};
    out.perBlock = itemsPerBlock(entry.name, flagged(entry.states, "upper_block_bit") || flagged(entry.states, "head_piece_bit"));
    return out;
}
}

Resolved resolve(Structure const& structure, SavedPlacement const& placement) {
    Resolved out{nullptr, &structure, placement.placement.rotation, placement.placement.mirror, {}};
    out.blocks.reserve(structure.palette.size());
    unsigned missing = 0;
    auto turn = [&](Block const* block) {
        if (block && (out.rotation || out.mirror != Mirror::None))
            if (auto const* turned = VanillaBlockStateTransformUtils::transformBlock(*block, gameRotation(out.rotation), gameMirror(out.mirror)))
                return turned;
        return block;
    };
    for (auto const& entry : structure.palette) {
        Block const* block = entry.isAir() ? nullptr : lookup(entry);
        if (!block && !entry.isAir()) ++missing;
        out.items.push_back(itemFor(entry, block));
        out.blocks.push_back(turn(block));
        auto half = block ? otherHalf(entry) : std::nullopt;
        Block const* other = half ? turn(lookup(half->block)) : nullptr;
        out.halves.push_back(other);
        out.halfSteps.push_back(other ? half->step : 0);
        if (block)
            if (int extra = block->getBlockType().getExtraRenderLayers())
                log(std::format("{}: render layer {}, extra layers {:#x}", entry.name,
                    static_cast<int>(static_cast<BlockRenderLayer>(block->getBlockType().mRenderLayer)), extra));
    }
    if (missing) log(std::format("{}: {} palette entries are not known blocks", placement.file, missing));
    Size placed = placedSize(structure.size, placement.placement.rotation);
    for (auto const& entity : structure.entities) {
        if (entity.identifier.empty() || out.entities.size() >= maxEntities) continue;
        EntityGhost ghost{entity.identifier, {}, {}, toWorldPosition(structure.size, placement.placement, {entity.x, entity.y, entity.z}), {}};
        auto const& o = placement.placement.origin;
        ghost.offset = {std::clamp(static_cast<int>(std::floor(ghost.at.x)) - o.x, 0, placed.x - 1),
                        std::clamp(static_cast<int>(std::floor(ghost.at.y)) - o.y, 0, placed.y - 1),
                        std::clamp(static_cast<int>(std::floor(ghost.at.z)) - o.z, 0, placed.z - 1)};
        if (auto const* turn = entity.data.find("Rotation"); turn && turn->as<nbt::List>() && !turn->as<nbt::List>()->items.empty())
            if (auto const* yaw = turn->as<nbt::List>()->items.front().as<float>()) ghost.yaw = toWorldYaw(*yaw, placement.placement);
        auto key = entityNameKey(entity.identifier);
        ghost.name = getI18n().get(key, getI18n().getCurrentLanguage());
        if (ghost.name.empty() || ghost.name == key) ghost.name = entity.identifier;
        nbt::Root tag;
        tag.compound.set("Name", {entity.identifier});
        tag.compound.set("Count", {std::int8_t{1}});
        tag.compound.set("Damage", {std::int16_t{0}});
        ghost.icon = nbt::write(tag);
        out.entities.push_back(std::move(ghost));
    }
    out.entityPlaced.assign(out.entities.size(), std::nullopt);
    return out;
}
}

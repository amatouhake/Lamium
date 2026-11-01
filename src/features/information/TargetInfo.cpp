#include "features/information/TargetInfo.h"
#include "features/information/SchematicTarget.h"
#include "features/information/TargetPick.h"
#include "features/schematic/GhostRenderer.h"
#include "features/schematic/LiquidName.h"
#include "features/schematic/RayCells.h"
#include "features/schematic/SchematicSession.h"
#include "app/Runtime.h"
#include "ui/Localization.h"
#include "features/information/TargetCard.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ActorPlacerItem.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/block/BlockGraphics.h"
#include "mc/client/renderer/texture/TextureUVCoordinateSet.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/item/ItemActor.h"
#include "mc/world/phys/HitResult.h"
#include "mc/world/phys/AABB.h"
#include "mc/world/phys/AABBHitResult.h"
#include "mc/world/level/ShapeType.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include <cmath>
#include <mutex>
#include <unordered_map>
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/BlockPalette.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/locale/I18n.h"
#include "mc/deps/nbt/CompoundTagVariant.h"
#include <algorithm>
#include <set>
#ifdef LAMIUM_RESEARCH_TRACE
#include <format>
#endif

namespace lamium::information {
namespace {
// What a detached camera looks at: the nearest block or entity box along the
// ray. Only owned values and this frame's pointers leave this function.
struct Pick { HitResultType type = HitResultType::NoHit; BlockPos block; Actor* entity = nullptr; };
Pick pickAlong(LocalPlayer& player, ViewRay const& ray) {
    Vec3 from{static_cast<float>(ray.x), static_cast<float>(ray.y), static_cast<float>(ray.z)};
    Vec3 to{static_cast<float>(ray.x + ray.dx * ray.reach), static_cast<float>(ray.y + ray.dy * ray.reach),
            static_cast<float>(ray.z + ray.dz * ray.reach)};
    auto distance = [&](Vec3 const& p) {
        double dx = p.x - from.x, dy = p.y - from.y, dz = p.z - from.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    };
    auto& source = player.getDimensionBlockSource();
    // Like the game's own pick, look through water and lava.
    auto solid = [](Block const& block) { return !schematic::isLiquidName(block.getTypeName()); };
    Pick pick;
    double best = ray.reach;
    auto hit = source.clip(from, to, false, ShapeType::Outline, static_cast<int>(ray.reach) + 1, false, false, nullptr,
        [&](BlockSource const&, Block const& block, bool) { return solid(block); }, false);
    std::optional<CellHit> game;
    if (hit.mType == HitResultType::Tile) game = CellHit{{hit.mBlock.x, hit.mBlock.y, hit.mBlock.z}, distance(hit.mPos)};
    // The game's clip can name the block above the aimed one (L-217): check
    // the cells up to its hit and keep the nearest face.
    auto cellHit = nearestCellHit(ray.x, ray.y, ray.z, ray.dx, ray.dy, ray.dz, ray.reach, game,
        [&](schematic::Point const& cell) -> std::optional<double> {
            BlockPos pos{cell.x, cell.y, cell.z};
            auto const& block = source.getBlock(pos);
            if (block.isAir() || !solid(block)) return std::nullopt;
            auto result = block.clip(source, pos, from, to, ShapeType::Outline, {});
            if (result.mType != HitResultType::Tile) return std::nullopt;
            return distance(result.mPos);
        },
        [&](schematic::Point const& cell) { return !source.getChunkAt(BlockPos{cell.x, cell.y, cell.z}); });
    if (cellHit) {
        pick = {HitResultType::Tile, BlockPos{cellHit->cell.x, cellHit->cell.y, cellHit->cell.z}, nullptr};
        best = cellHit->distance;
    }
    AABB area{Vec3{std::min(from.x, to.x) - 1, std::min(from.y, to.y) - 1, std::min(from.z, to.z) - 1},
              Vec3{std::max(from.x, to.x) + 1, std::max(from.y, to.y) + 1, std::max(from.z, to.z) + 1}};
    for (Actor* actor : source.fetchEntities(&player, area, true, false)) {
        if (!actor || actor->mRemoved) continue;
        auto result = actor->getAABB().clip(from, to);
        if (!result.mIsHit) continue;
        double d = distance(result.mPos);
        if (d < best) { best = d; pick = {HitResultType::Entity, {}, actor}; }
    }
    return pick;
}
// Some eggs' actor id differs from the entity's own identifier, so the name
// convention cannot find them. Index every spawn-egg item by its actor id once
// per world (add-ons bring their own eggs); only strings are kept.
std::mutex eggMutex;
std::unordered_map<std::string, std::string> eggIndex;
bool eggIndexBuilt = false;
ll::event::ListenerPtr exitListener;
std::optional<std::string> spawnEggFor(ItemRegistryRef const& registry, std::string const& actorId) {
    std::scoped_lock lock(eggMutex);
    if (!eggIndexBuilt && registry.isRegistryInitialized()) {
        auto const& items = registry.getNameToItemMap();
        for (auto const& [name, weak] : items) {
            auto* item = weak.get();
            if (!item || !item->isActorPlacerItem()) continue;
            auto const* placer = static_cast<ActorPlacerItem const*>(item);
            std::string const& actor = placer->mActorID->mFullName.get();
            if (!actor.empty()) eggIndex.try_emplace(actor, name.getString());
        }
        eggIndexBuilt = !items.empty();
    }
    if (auto found = eggIndex.find(actorId); found != eggIndex.end()) return found->second;
    return std::nullopt;
}
void clearSpawnEggIndex() {
    std::scoped_lock lock(eggMutex);
    eggIndex.clear();
    eggIndexBuilt = false;
}
// Spawn-egg icons come from the item registry: the exact name and the few
// renamed entities first, then the registry audit for anything else.
TargetIcon entityIcon(IClientInstance& client, std::string const& identifier) {
    if (identifier.empty()) return {};
    try {
        auto registry = client.getItemRegistry();
        for (auto const& candidate : spawnEggCandidates(identifier))
            if (registry.getItem(HashedString(candidate))) return {IconKind::Item, candidate, 0};
        if (auto egg = spawnEggFor(registry, identifier)) return {IconKind::Item, std::move(*egg), 0};
        // Projectiles and carried blocks have no egg, but an item of the same
        // subject often shares a usable id (snowball, arrow, painting).
        for (auto const& candidate : entityItemCandidates(identifier))
            if (registry.getItem(HashedString(candidate))) return {IconKind::Item, candidate, 0};
    } catch (...) {} // Icon resolution must not replace the target snapshot.
    return {};
}
// A block's own texture as an icon; the uv keeps one animation frame instead
// of the whole vertical strip.
TargetIcon textureIcon(Block const& block) {
    std::string texture;
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
    float textureW = 0, textureH = 0, sourceW = 0, sourceH = 0;
    {
        try {
            auto const* graphics = BlockGraphics::getForBlock(block);
            if (graphics) {
                auto const& uv = graphics->getTexture(graphics->mIconTextureIndex, 0);
                // The ui loader wants the pack-relative path; getFullPath()
                // would prepend the file system the uv set was stored in.
                Core::PathBuffer<std::string> const& rawPath = uv.sourceFileLocation.get().mPath;
                texture = rawPath.value;
                if (texture.empty()) return {};
                textureW = uv._texSizeW;
                textureH = uv._texSizeH;
                sourceW = uv._sourceImageWidth;
                sourceH = uv._sourceImageHeight;
                // The uv set carries its own end coordinates; the size ratio is
                // only a stand-in for sets that leave them unset.
                u0 = uv._u0;
                v0 = uv._v0;
                if (uv._u1 > u0 && uv._v1 > v0) {
                    u1 = uv._u1;
                    v1 = uv._v1;
                } else if (uv._sourceImageWidth > 0 && uv._sourceImageHeight > 0 && uv._texSizeW > 0
                           && uv._texSizeH > 0) {
                    u1 = std::min(1.f, u0 + static_cast<float>(uv._texSizeW) / uv._sourceImageWidth);
                    v1 = std::min(1.f, v0 + static_cast<float>(uv._texSizeH) / uv._sourceImageHeight);
                }
                // L-294: water draws its tile of the terrain atlas, which the
                // game animates and blends as it does for the water item's icon;
                // the source file would show one still frame.
                if (schematic::isWaterName(block.getTypeName())) {
                    TargetIcon water{IconKind::Texture, "atlas.terrain", 0, u0, v0, u1, v1};
                    water.tinted = true;
                    return water;
                }
                // The set addresses the stitched atlas while the icon draws
                // the source file, so restate one frame in file coordinates.
                auto frame = fileFrameUv(TargetIcon{IconKind::Texture, texture, 0, u0, v0, u1, v1}, textureW,
                                         textureH, sourceW, sourceH);
                u0 = frame.u0;
                v0 = frame.v0;
                u1 = frame.u1;
                v1 = frame.v1;
#ifdef LAMIUM_RESEARCH_TRACE
                // L-58: one line per block kind proves what the fallback picked.
                static std::string logged;
                if (logged != texture) {
                    logged = texture;
                    auto& location = uv.sourceFileLocation.get();
                    ResourceFileSystem fileSystem = location.mFileSystem;
                    Runtime::instance().self().getLogger().info(
                        "L-58 texture {} -> {} fs={} tex={}x{} source={}x{} raw_uv={:.4f},{:.4f}-{:.4f},{:.4f}"
                        " uv={:.4f},{:.4f}-{:.4f},{:.4f}",
                        block.getTypeName(), texture, static_cast<int>(fileSystem), uv._texSizeW, uv._texSizeH,
                        uv._sourceImageWidth, uv._sourceImageHeight, uv._u0, uv._v0, uv._u1, uv._v1, u0, v0, u1, v1);
                }
#endif
            }
        } catch (...) { return {}; }
    }
    auto icon = chooseBlockIcon({}, 0, std::move(texture));
    icon.u0 = u0;
    icon.v0 = v0;
    icon.u1 = u1;
    icon.v1 = v1;
    return icon;
}
// Blocks without an item (portal, fire, ...) draw their own texture, and so
// does water (L-294).
TargetIcon blockIcon(BlockSource& source, BlockPos const& pos, Block const& block) {
    if (schematic::isWaterName(block.getTypeName())) return textureIcon(block);
    std::string pick;
    short aux = 0;
    try {
        auto item = block.asItemInstance(source, pos, true);
        if (!item.isNull() && item.mItem) {
            pick = item.mItem->mFullName->getString();
            aux = item.getAuxValue();
        }
    } catch (...) {}
    if (!pick.empty()) return chooseBlockIcon(std::move(pick), aux, {});
    return textureIcon(block);
}
// A dropped stack shows the item it holds; its own identifier is
// "minecraft:item", which names no item of its own.
TargetIcon droppedIcon(Actor& entity) {
    try {
        if (!entity.hasType(ActorType::ItemEntity)) return {};
        auto& stack = static_cast<ItemActor&>(entity).item();
        if (!stack.mItem) return {};
        return {IconKind::Item, stack.mItem->mFullName->getString(), stack.getAuxValue()};
    } catch (...) { return {}; }
}
// A falling block carries no item. The client learns the carried block only
// from the actor's variant (a network block id): mFallingBlockId/Data stay 0:0
// on the client and name info_update, which was the wrong icon before.
TargetIcon fallingIcon(Actor& entity, LocalPlayer& player) {
    try {
        if (!entity.hasType(ActorType::FallingBlock)) return {};
        auto network = static_cast<uint>(entity.getVariant());
        auto const& block = entity.getLevel().getBlockPalette().getBlock(network);
        if (block.getTypeName() == "minecraft:air") return {};
        auto const& at = entity.getPosition();
        BlockPos where{static_cast<int>(std::floor(at.x)), static_cast<int>(std::floor(at.y)),
                       static_cast<int>(std::floor(at.z))};
        return blockIcon(player.getDimensionBlockSource(), where, block);
    } catch (...) { return {}; }
}
}
TargetIcon waterIcon() {
    try {
        // Looked up each time: a block pointer kept across a registry reload crashed elsewhere.
        auto found = Block::tryGetFromRegistry(HashedString{"minecraft:water"});
        Block const* water = found ? &*found : nullptr;
        return water ? textureIcon(*water) : TargetIcon{};
    } catch (...) { return {}; }
}
// The block's states as card rows (named where L-112 knows them) and the
// rest as raw "key: value" lines, leaving out `fixed` (shown by the
// schematic rows instead).
void appendStates(TargetInfo& result, Block const& block, std::set<std::string> const& fixedStates, bool nether) {
    auto const& tags = block.mSerializationId->mTags;
    auto found = tags.find("states");
    if (found != tags.end()) {
        if (auto* states = std::get_if<CompoundTag>(&found->second.mTagStorage)) {
            for (auto const& [key,value] : states->mTags) {
                if (fixedStates.contains(key)) continue;
                std::optional<TargetInfo::DetailRow> detail;
                if (auto* v = std::get_if<ByteTag>(&value.mTagStorage))
                    detail = interpretBlockState(key, StateKind::Integer, v->data, {}, result.identifier, nether);
                else if (auto* v = std::get_if<IntTag>(&value.mTagStorage))
                    detail = interpretBlockState(key, StateKind::Integer, v->data, {}, result.identifier, nether);
                else if (auto* v = std::get_if<StringTag>(&value.mTagStorage))
                    detail = interpretBlockState(key, StateKind::Text, 0, *v, result.identifier, nether);
                if (detail) {
                    result.details.push_back(std::move(*detail));
                    continue;
                }
                std::optional<std::string> text;
                if (auto* v = std::get_if<ByteTag>(&value.mTagStorage)) text = std::to_string(v->data);
                else if (auto* v = std::get_if<IntTag>(&value.mTagStorage)) text = std::to_string(v->data);
                else if (auto* v = std::get_if<StringTag>(&value.mTagStorage)) text = *v;
                if (text) result.states.push_back(key + ": " + *text);
            }
        }
    }
}
// The nearest schematic ghost in front of the aimed block (L-184): its block
// as the card's target, marked as a block still to place.
std::optional<TargetInfo> ghostTarget(LocalPlayer& player, BlockSource& source, bool includeStates, std::optional<ViewRay> const& ray,
                                      bool throughBlocks) {
    double x, y, z, dx, dy, dz, reach;
    if (ray) { x = ray->x; y = ray->y; z = ray->z; dx = ray->dx; dy = ray->dy; dz = ray->dz; reach = ray->reach; }
    else {
        Vec3 eye = player.getEyePos(), view = player.getViewVector(1.f);
        x = eye.x; y = eye.y; z = eye.z; dx = view.x; dy = view.y; dz = view.z;
        reach = 6;
    }
    if (!(reach > 0)) return std::nullopt;
    auto ghost = schematic::ghosts::ghostAlong(source, static_cast<int>(player.getDimensionId()), x, y, z, dx, dy, dz, reach,
                                                   throughBlocks);
    if (!ghost || !ghost->block) return std::nullopt;
    Block const& block = *ghost->block;
    BlockPos pos{ghost->cell.x, ghost->cell.y, ghost->cell.z};
    TargetInfo result{block.buildDescriptionName(), block.getTypeName()};
    if (result.name.empty()) result.name = result.identifier;
    result.blockPosition = TargetInfo::BlockPosition{pos.x, pos.y, pos.z};
    result.icon = blockIcon(source, pos, block);
    if (includeStates) appendStates(result, block, {}, static_cast<int>(player.getDimensionId()) == 1);
    result.details.push_back(ghostRow());
    result.ghost = true;
    return result;
}
// The first real water or lava along the ray, before any other block (L-264).
std::optional<TargetInfo> liquidTarget(BlockSource& source, bool includeStates, ViewRay const& ray, bool nether) {
    for (auto const& cell : schematic::cellsAlong(ray.x, ray.y, ray.z, ray.dx, ray.dy, ray.dz, ray.reach)) {
        BlockPos pos{cell.x, cell.y, cell.z};
        if (!source.getChunkAt(pos)) return std::nullopt;
        Block const& block = source.getBlock(pos);
        if (!schematic::ghosts::liquidKind(block)) {
            if (block.isAir()) continue;
            return std::nullopt;
        }
        TargetInfo result{block.buildDescriptionName(), block.getTypeName()};
        if (result.name.empty()) result.name = result.identifier;
        result.blockPosition = TargetInfo::BlockPosition{pos.x, pos.y, pos.z};
        result.icon = blockIcon(source, pos, block);
        if (includeStates) appendStates(result, block, {}, nether);
        return result;
    }
    return std::nullopt;
}
std::optional<TargetInfo> collectTargetInfo(IClientInstance& client, bool includeStates, std::optional<ViewRay> ray,
                                            bool includeSchematic, bool includeLiquids) {
    auto* player = client.getLocalPlayer();
    if (!player) return {};
    Pick pick;
    auto const& latest = client.getLatestHitResult();
    bool gameHit = latest.mType == HitResultType::Tile || latest.mType == HitResultType::Entity;
    std::optional<double> gameDistance;
    if (gameHit && ray && !ray->detached) {
        double dx = latest.mPos.x - ray->x, dy = latest.mPos.y - ray->y, dz = latest.mPos.z - ray->z;
        gameDistance = std::sqrt(dx * dx + dy * dy + dz * dz);
        gameHit = *gameDistance <= ray->reach;
        // L-285: the block under the feet, which the game reports as an
        // indirect hit when its ray meets nothing, is not the aimed block; the
        // card then looks along the view as the ghost pick does.
        if (gameHit && latest.mIndirectHit) {
            gameHit = false;
            gameDistance.reset();
        }
    }
    auto ghostRay = ray;
    bool onGameLine = false;
    if (ray && (ray->detached || !gameHit)) pick = pickAlong(*player, *ray);
    else {
        // Ghosts are searched on the game's own ray (its start and direction
        // from the hit), up to just short of its hit. The eye ray and the
        // frame's view run a little off that ray, which at a block's or a
        // ghost's edge met other cells and flickered (L-222, L-224). An entity
        // hit counts too: a ghost in front of it is the target.
        if (ray && gameDistance) {
            auto const& from = latest.mStartPos;
            auto const& way = latest.mRayDir;
            double length = std::sqrt(double(way.x) * way.x + double(way.y) * way.y + double(way.z) * way.z);
            double ex = latest.mPos.x - from.x, ey = latest.mPos.y - from.y, ez = latest.mPos.z - from.z;
            double d = std::sqrt(ex * ex + ey * ey + ez * ez);
            double ox = from.x - ray->x, oy = from.y - ray->y, oz = from.z - ray->z;
            // Trust the game's ray only when it starts near the eye.
            if (length > 0 && std::isfinite(d) && d > 0 && ox * ox + oy * oy + oz * oz < 1) {
                ghostRay = ViewRay{from.x, from.y, from.z, way.x / length, way.y / length, way.z / length, d - .05};
                onGameLine = true;
            }
            // Otherwise: back from the game's hit along the frame's view.
            double view = std::sqrt(ray->dx * ray->dx + ray->dy * ray->dy + ray->dz * ray->dz);
            if (!onGameLine && view > 0) {
                double ux = ray->dx / view, uy = ray->dy / view, uz = ray->dz / view, back = *gameDistance;
                ghostRay = ViewRay{latest.mPos.x - ux * back, latest.mPos.y - uy * back, latest.mPos.z - uz * back,
                                   ux, uy, uz, back - .05};
                onGameLine = true;
            }
        }
        pick.type = latest.mType;
        pick.block = latest.mBlock;
        if (latest.mType == HitResultType::Entity) pick.entity = latest.getEntity();
    }
    struct { HitResultType mType; BlockPos mBlock; } hit{pick.type, pick.block};
    // Real water and lava, when the option is on, come before the ghost: with
    // it on the player wants the liquid they are aiming at, even when a ghost
    // stands behind it (L-278). Off, the ghost keeps the target as before.
    if (includeLiquids && ghostRay && (hit.mType != HitResultType::Entity || onGameLine))
        if (auto liquid = liquidTarget(player->getDimensionBlockSource(), includeStates, *ghostRay,
                                         static_cast<int>(player->getDimensionId()) == 1)) return liquid;
    // A ghost in front of the aimed block (or in the air) is the target.
    if (includeSchematic && (hit.mType != HitResultType::Entity || onGameLine))
        if (auto ghost = ghostTarget(*player, player->getDimensionBlockSource(), includeStates, ghostRay, onGameLine))
            return ghost;
    if (hit.mType == HitResultType::Entity) {
        auto* entity = pick.entity;
        if (!entity || entity == player || entity->mRemoved
            || entity->getDimensionId() != player->getDimensionId()) return {};
        // Respect the game's filtered name, then use its localized entity type.
        // The weak hit reference is resolved only for this snapshot.
        TargetInfo result{entity->getFilteredNameTag(),entity->getTypeName()};
        result.icon = droppedIcon(*entity);
        if (result.icon.kind == IconKind::None) result.icon = fallingIcon(*entity, *player);
        if (result.icon.kind == IconKind::None) result.icon = entityIcon(client, result.identifier);
        if (result.name.empty()) {
            auto key = entity->getEntityLocNameString();
            result.name = getI18n().get(key,getI18n().getCurrentLanguage());
            if (result.name.empty() || result.name == key) result.name = result.identifier;
        }
        // Mob details need no hook beyond the hit reference already held.
        if (includeStates && entity->hasType(ActorType::Mob)) {
            int health = entity->getHealth(), maxHealth = entity->getMaxHealth();
            if (maxHealth > 0 && health >= 0) {
                float progress = std::clamp(static_cast<float>(health) / maxHealth, 0.f, 1.f);
                result.details.push_back({"target.health",
                    std::to_string(health) + " / " + std::to_string(maxHealth), false, progress, DetailKind::Health,
                    health, maxHealth});
            }
            int armor = static_cast<Mob*>(entity)->getArmorValue();
            if (armor > 0)
                result.details.push_back({"target.armor", std::to_string(armor), false,
                                          std::clamp(static_cast<float>(armor) / 20.f, 0.f, 1.f), DetailKind::Armor});
            result.details.push_back({"target.age", entity->isBaby() ? "target.baby" : "target.adult", true, {}});
            if (entity->isTame()) {
                std::string owner = "target.yes";
                bool ownerIsKey = true;
                if (auto* ownerMob = entity->getOwner()) {
                    auto name = ownerMob->getFilteredNameTag();
                    if (!name.empty()) { owner = std::move(name); ownerIsKey = false; }
                }
                result.details.push_back({"target.tamed", std::move(owner), ownerIsKey, {}});
            }
        }
        return result;
    }
    if (hit.mType != HitResultType::Tile) return {};
    auto const& range = player->getDimension().mHeightRange;
    if (hit.mBlock.y < range->mMin || hit.mBlock.y >= range->mMax) return {};
    auto& source = player->getDimensionBlockSource();
    if (!source.getChunkAt(hit.mBlock)) return {};
    auto const& block = source.getBlock(hit.mBlock);
    if (block.isAir()) return {};
    TargetInfo result{block.buildDescriptionName(),block.getTypeName()};
    result.blockPosition = TargetInfo::BlockPosition{hit.mBlock.x,hit.mBlock.y,hit.mBlock.z};
    // The pick-block item (seeds for crops, the block item otherwise).
    result.icon = blockIcon(source, hit.mBlock, block);
    if (result.name.empty()) result.name = result.identifier;
    // The selected schematic placement's block here, when it differs (L-93):
    // its rows replace the card's own rows for the same states.
    std::vector<TargetInfo::DetailRow> schematicDetails;
    std::set<std::string> fixedStates;
    if (auto& runtime = Runtime::instance(); includeSchematic && runtime.enabled() && runtime.snapshot()->schematic.enabled) {
        auto verification = schematic::ghosts::verification();
        auto snapshot = schematic::session::snapshot();
        int index = verification->placement;
        if (!verification->stale && verification->revision == snapshot.revision && index == snapshot.selected
            && index >= 0 && static_cast<size_t>(index) < snapshot.placements.size()) {
            auto const& shown = snapshot.placements[static_cast<size_t>(index)];
            auto const& placement = shown.placement;
            schematic::Point offset{hit.mBlock.x - placement.placement.origin.x,
                hit.mBlock.y - placement.placement.origin.y, hit.mBlock.z - placement.placement.origin.z};
            if (shown.structure && placement.visible && placement.dimension == static_cast<int>(player->getDimensionId())
                && schematic::layerShown(placement.layers, shown.structure->size, placement.placement, offset)) {
                // Read just this cell now; the capped background list may be old or omit it.
                if (auto m = schematic::ghosts::mismatchAt(source, {hit.mBlock.x, hit.mBlock.y, hit.mBlock.z});
                    m && !m->entity) {
                    schematicDetails = schematicRows(m->state, m->expectedName, m->expected, m->states,
                        result.identifier, [](std::string_view key) { return ui::translated(key); });
                    for (auto const& d : m->states) fixedStates.insert(d.key);
                }
            }
        }
    }
    if (includeStates) appendStates(result, block, fixedStates, static_cast<int>(player->getDimensionId()) == 1);
    for (auto& row : schematicDetails) result.details.push_back(std::move(row));
    return result;
}
void startTargetIcons() {
    exitListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientExitLevelEvent>(
        [](auto&) { clearSpawnEggIndex(); });
}
void stopTargetIcons() {
    if (exitListener) {
        ll::event::EventBus::getInstance().removeListener(exitListener);
        exitListener.reset();
    }
    clearSpawnEggIndex();
}
}

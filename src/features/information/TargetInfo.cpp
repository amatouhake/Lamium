#include "features/information/TargetInfo.h"
#include "features/information/TargetCard.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/phys/HitResult.h"
#include "mc/world/phys/AABB.h"
#include "mc/world/phys/AABBHitResult.h"
#include "mc/world/level/ShapeType.h"
#include <cmath>
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/locale/I18n.h"
#include "mc/deps/nbt/CompoundTagVariant.h"
#include <algorithm>

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
    Pick pick;
    double best = ray.reach;
    auto hit = source.clip(from, to, false, ShapeType::Outline, static_cast<int>(ray.reach) + 1, false, false, nullptr,
        [](BlockSource const&, Block const&, bool) { return true; }, false);
    if (hit.mType == HitResultType::Tile) {
        pick = {HitResultType::Tile, hit.mBlock, nullptr};
        best = distance(hit.mPos);
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
}
std::optional<TargetInfo> collectTargetInfo(IClientInstance& client, bool includeStates, std::optional<ViewRay> ray) {
    auto* player = client.getLocalPlayer();
    if (!player) return {};
    Pick pick;
    if (ray) pick = pickAlong(*player, *ray);
    else {
        auto const& latest = client.getLatestHitResult();
        pick.type = latest.mType;
        pick.block = latest.mBlock;
        if (latest.mType == HitResultType::Entity) pick.entity = latest.getEntity();
    }
    struct { HitResultType mType; BlockPos mBlock; } hit{pick.type, pick.block};
    if (hit.mType == HitResultType::Entity) {
        auto* entity = pick.entity;
        if (!entity || entity == player || entity->mRemoved
            || entity->getDimensionId() != player->getDimensionId()) return {};
        // Respect the game's filtered name, then use its localized entity type.
        // The weak hit reference is resolved only for this snapshot.
        TargetInfo result{entity->getFilteredNameTag(),entity->getTypeName()};
        result.iconItem = spawnEggItem(result.identifier);
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
                    std::to_string(health) + " / " + std::to_string(maxHealth), false, progress, DetailKind::Health});
            }
            int armor = static_cast<Mob*>(entity)->getArmorValue();
            if (armor > 0) result.details.push_back({"target.armor", std::to_string(armor), false, {}});
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
    auto item = block.asItemInstance(source, hit.mBlock, true);
    if (!item.isNull() && item.mItem) {
        result.iconItem = item.mItem->mFullName->getString();
        result.iconAux = item.getAuxValue();
    }
    if (result.name.empty()) result.name = result.identifier;
    if (includeStates) {
        auto const& tags = block.mSerializationId->mTags;
        auto found = tags.find("states");
        if (found != tags.end()) {
            if (auto* states = std::get_if<CompoundTag>(&found->second.mTagStorage)) {
                for (auto const& [key,value] : states->mTags) {
                    std::optional<TargetInfo::DetailRow> detail;
                    if (auto* v = std::get_if<ByteTag>(&value.mTagStorage))
                        detail = interpretBlockState(key, StateKind::Integer, v->data, {}, result.identifier);
                    else if (auto* v = std::get_if<IntTag>(&value.mTagStorage))
                        detail = interpretBlockState(key, StateKind::Integer, v->data, {}, result.identifier);
                    else if (auto* v = std::get_if<StringTag>(&value.mTagStorage))
                        detail = interpretBlockState(key, StateKind::Text, 0, *v, result.identifier);
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
    return result;
}
}

#pragma once
#include <optional>
#include <cmath>
#include <initializer_list>
#include <string_view>

namespace lamium::inventory::fakeOffhand {
// Item properties read from the game. The primary hand passes to the
// secondary item only when these show that its ordinary use cannot act on
// the current aim; anything uncertain keeps vanilla priority.
struct ItemTraits {
    std::string_view name;
    bool empty = false, block = false, food = false, timed = false, throwable = false;
    bool bucket = false, liquidClip = false, wearable = false, planter = false;
    bool fertilizer = false, dye = false, damageable = false, idleTool = false;
};
inline bool foodBlocked(bool alwaysEat, bool creative, std::optional<float> hunger, float maximum) {
    return !alwaysEat && !creative && hunger && std::isfinite(*hunger)
        && std::isfinite(maximum) && maximum > 0 && *hunger == maximum;
}
namespace detail {
inline bool any(std::string_view id, std::initializer_list<std::string_view> names) {
    for (auto name : names) if (id == name) return true;
    return false;
}
inline bool suffix(std::string_view id, std::initializer_list<std::string_view> ends) {
    for (auto end : ends) if (id.ends_with(end)) return true;
    return false;
}
// Air uses that the generic properties do not reveal.
inline bool airUse(std::string_view id) {
    return any(id, {"fishing_rod", "carrot_on_a_stick", "warped_fungus_on_a_stick", "firework_rocket",
        "empty_map", "writable_book", "written_book", "ender_eye", "shield", "elytra", "bundle",
        "carved_pumpkin", "camera", "ice_bomb"})
        || suffix(id, {"_bundle", "_head", "_skull"}) || id == "skull";
}
// Block-target uses of non-block items that the generic properties do not reveal.
inline bool blockUse(std::string_view id) {
    return any(id, {"fire_charge", "armor_stand", "end_crystal", "frame", "glow_frame", "painting",
        "lead", "honeycomb", "ender_eye", "compass", "trial_key",
        "ominous_trial_key", "writable_book", "written_book", "lodestone_compass",
        "resin_clump",
        "glass_bottle", "bed", "cake", "banner", "repeater", "comparator", "cauldron",
        "brewing_stand", "flower_pot", "hopper", "campfire", "soul_campfire", "string", "redstone",
        "sugar_cane", "reeds", "nether_wart", "kelp", "bamboo", "minecart", "boat", "chest_boat",
        "cocoa_beans", "pitcher_pod", "decorated_pot", "wind_charge", "brush", "name_tag"})
        || suffix(id, {"_door", "_sign", "_seeds", "_spawn_egg", "_minecart", "_boat", "_raft", "_bed",
            "_banner", "_candle", "_bundle"})
        || id.starts_with("music_disc");
}
// Projectiles and buckets also match by identity, so a property that a
// data-driven item does not report cannot turn them into a passing primary.
inline bool throwable(ItemTraits const& item, std::string_view id) {
    return item.throwable || any(id, {"snowball", "egg", "blue_egg", "brown_egg", "ender_pearl",
        "splash_potion", "lingering_potion", "experience_bottle", "wind_charge", "trident"});
}
inline bool bucket(ItemTraits const& item, std::string_view id) {
    return item.bucket || id == "bucket" || id.ends_with("_bucket");
}
inline std::string_view vanillaId(std::string_view name) {
    constexpr std::string_view prefix = "minecraft:";
    return name.starts_with(prefix) ? name.substr(prefix.size()) : std::string_view{};
}
}
// L-189: the few tools whose block use the game offers no check for, by the
// vanilla blocks they change (ids without "minecraft:"). Blocks with a block
// entity (a lit campfire, a beehive) never reach this: plain targets only.
inline bool shovelChanges(std::string_view block) {
    return detail::any(block, {"grass_block", "dirt", "coarse_dirt", "podzol", "mycelium", "dirt_with_roots", "rooted_dirt"});
}
inline bool hoeChanges(std::string_view block) {
    return detail::any(block, {"grass_block", "dirt", "coarse_dirt", "dirt_with_roots", "rooted_dirt", "grass_path", "dirt_path"});
}
inline bool shearsChange(std::string_view block) { return detail::any(block, {"pumpkin", "trip_wire", "tripwire"}); }
// What a planter needs below when the game does not say what it plants
// (ids without "minecraft:"): crops on farmland, nether wart on soul sand;
// empty for others, which stay unread.
inline std::string_view plantsOn(std::string_view item) {
    if (item.ends_with("_seeds") || detail::any(item, {"pitcher_pod", "carrot", "potato"})) return "farmland";
    if (item == "nether_wart") return "soul_sand";
    return {};
}
// Whether the item plants a block. Wheat seeds and carrots report neither the
// planter property nor its component (seen in game, L-189), so the vanilla
// planters also count by name.
inline bool plants(ItemTraits const& item) {
    auto id = detail::vanillaId(item.name);
    return item.planter || !plantsOn(id).empty() || detail::any(id, {"sweet_berries", "glow_berries", "cocoa_beans"});
}
// A log, wood, stem or hyphae that is not stripped yet, or a bamboo block.
inline bool strippable(std::string_view block) {
    return block == "bamboo_block"
        || (!block.starts_with("stripped_") && detail::suffix(block, {"_log", "_wood", "_stem", "_hyphae"}));
}
// Which target-specific use an item has, so the glue knows what to read.
enum class TargetUse { None, Planter, Fertilizer, Rail, Honeycomb, Axe, Shovel, Hoe, Shears, Fill };
inline TargetUse targetUseOf(ItemTraits const& item, bool shovelTag, bool hoeTag, bool shearsTag) {
    auto id = detail::vanillaId(item.name);
    if (item.empty || id.empty()) return TargetUse::None;
    if (plants(item)) return TargetUse::Planter;
    if (item.fertilizer) return TargetUse::Fertilizer;
    if (id == "minecart" || id.ends_with("_minecart")) return TargetUse::Rail;
    if (id == "honeycomb") return TargetUse::Honeycomb;
    if (id.ends_with("_axe")) return TargetUse::Axe;
    if (shovelTag) return TargetUse::Shovel;
    if (hoeTag) return TargetUse::Hoe;
    if (shearsTag || id == "shears") return TargetUse::Shears;
    if (id == "bucket" || id == "glass_bottle") return TargetUse::Fill;
    return TargetUse::None;
}
// Whether the primary item's ordinary use cannot act on this aim. Interactive
// blocks and entities are decided before this and keep vanilla priority.
// `plainTarget`: a solid block without a block entity. Items act on special
// blocks (decorated pots, lecterns, signs, bookshelves, vaults) mostly through
// their block entity, so other materials pass only on plain blocks; an item
// the identity list misses cannot lose such a use (2026-10-07).
// L-189: `targetUse` is what the game says the item's target-specific use can
// do on this block (a planter's canUseOn, a fertilizable block, rails, copper
// state, the tool lists above); none when unread or the item has none, which
// keeps vanilla priority. `idle`: the item's use cannot act now (a bow
// without arrows or Riptide out of water and rain anywhere; a firework while
// not gliding or an empty bucket or bottle at no liquid, aimed at nothing).
inline bool primaryPass(ItemTraits const& item, bool blockTarget, bool cannotEat, bool plainTarget = true,
                        std::optional<bool> targetUse = std::nullopt, bool idle = false) {
    if (item.empty) return true;
    auto id = detail::vanillaId(item.name);
    if (id.empty()) return false;
    if (idle && (!blockTarget || plainTarget)) return true;
    // Full hunger only stops eating; a food that plants (carrots) still can
    // where its planter says so.
    if (item.food) {
        if (!cannotEat) return false;
        if (!blockTarget) return true;
        return plainTarget && (!plants(item) || targetUse == false);
    }
    if (item.wearable || detail::airUse(id)) return false;
    // A block has no air use, even when its native item reports a duration.
    if (item.block) return !blockTarget && !item.liquidClip;
    if (item.idleTool) return true;
    if (blockTarget && plainTarget && targetUse == false) return true;
    if (item.timed || detail::throwable(item, id) || detail::bucket(item, id) || item.liquidClip) return false;
    if (!blockTarget) return true;
    // Dyes and books act on signs, lecterns and shelves, which have block
    // entities: on plain blocks they pass.
    return plainTarget && !item.block && !plants(item) && !item.fertilizer && !item.damageable && !detail::blockUse(id);
}
// Whether the secondary item has an instant use the per-call borrow can run.
// Timed and tethered items (a fishing line follows the selected item) cannot.
inline bool secondaryInstant(ItemTraits const& item, bool gliding, bool blockTarget) {
    auto id = detail::vanillaId(item.name);
    if (item.empty || id.empty()) return false;
    if (id == "firework_rocket") return gliding || blockTarget;
    if (detail::any(id, {"fishing_rod", "carrot_on_a_stick", "warped_fungus_on_a_stick", "milk_bucket", "trident"}))
        return false;
    return (detail::throwable(item, id) && !item.timed) || detail::bucket(item, id);
}
// A server update that reselects the borrowed slot right after an instant use
// is an echo of the borrow (fireworks), not a choice; any other change is kept.
inline bool undoesRestore(int before, int after, int borrowed, int previous, bool recent) {
    return recent && borrowed >= 0 && borrowed < 9 && previous >= 0 && previous < 9
        && borrowed != previous && before == previous && after == borrowed;
}
// A right-click activation is decided once, by whichever of the native handler
// and the queued press comes first. A queued press after the native handler
// would see the world that click already changed (an opened door, a placed
// last block) and could fire the secondary item as well (L-122).
inline bool queuedPressDecides(bool rightClickBinding, bool nativeDecided) {
    return !rightClickBinding || !nativeDecided;
}
// L-288: what a queued press runs once the slot choice is known. Only an
// instant borrow or a placement borrow has anything to run; a press that
// borrows nothing must do nothing, because the replayed vanilla use edge acts
// on the primary item - the right click the player did not press. A binding
// that ends on right click is left to vanilla, so the placement replay is only
// for other bindings.
enum class QueuedPress { Nothing, Instant, Placement };
inline QueuedPress queuedPressAction(bool rightClickBinding, std::optional<int> slot, bool instant) {
    if (!slot) return QueuedPress::Nothing;
    if (instant) return QueuedPress::Instant;
    return rightClickBinding ? QueuedPress::Nothing : QueuedPress::Placement;
}
// L-288: a held queued press replays the use edge only while a build tick has
// a placement slot. A hold that starts off a block face sends it once the aim
// reaches one; a hold that loses the slot releases it, so vanilla's repeat
// never acts on the primary item. Instant holds keep their own lifecycle.
enum class HoldEdge { None, Down, Up };
inline HoldEdge queuedHoldEdge(bool held, bool sent, bool instantHold, bool placement) {
    if (!held || instantHold) return HoldEdge::None;
    if (placement && !sent) return HoldEdge::Down;
    if (!placement && sent) return HoldEdge::Up;
    return HoldEdge::None;
}
inline bool ownsInstantHold(int primary, int target, int selected, int configured, bool eligible) {
    return eligible && primary >= 0 && primary < 9 && target >= 0 && target < 9
        && primary != target && selected == primary && configured == target;
}
inline std::optional<int> instantUseSlot(bool enabled, bool triggered, int selected, int target,
    bool primaryIdle, bool targetInstant, bool entityTarget,
    bool interactive, bool sneaking) {
    if (!enabled || !triggered || selected < 0 || selected >= 9 || target < 0 || target >= 9
        || selected == target || !primaryIdle || !targetInstant || entityTarget
        || (interactive && !sneaking)) return {};
    return target;
}
inline std::optional<int> placementSlot(bool enabled, bool triggered, int selected, int target,
    bool hasBlockItem, bool hitBlock, bool interactive, bool sneaking,
    bool primaryIdle = true, bool primaryUsing = false) {
    if (!enabled || !triggered || selected < 0 || selected >= 9 || target < 0 || target >= 9
        || selected == target || !hasBlockItem || !hitBlock || !primaryIdle || primaryUsing
        || (interactive && !sneaking)) return {};
    return target;
}
}

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
        "lead", "honeycomb", "ender_eye", "ink_sac", "glow_ink_sac", "compass", "trial_key",
        "ominous_trial_key", "book", "enchanted_book", "writable_book", "written_book", "lodestone_compass",
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
// Whether the primary item's ordinary use cannot act on this aim. Interactive
// blocks and entities are decided before this and keep vanilla priority.
// `plainTarget`: a solid block without a block entity. Items act on special
// blocks (decorated pots, lecterns, signs, bookshelves, vaults) mostly through
// their block entity, so other materials pass only on plain blocks; an item
// the identity list misses cannot lose such a use (2026-10-07).
inline bool primaryPass(ItemTraits const& item, bool blockTarget, bool cannotEat, bool plainTarget = true) {
    if (item.empty) return true;
    auto id = detail::vanillaId(item.name);
    if (id.empty()) return false;
    // Seeds-like foods plant on blocks; full hunger only blocks eating.
    if (item.food) return !blockTarget && cannotEat;
    if (item.wearable || detail::airUse(id)) return false;
    if (item.idleTool) return true;
    if (item.timed || detail::throwable(item, id) || detail::bucket(item, id) || item.liquidClip) return false;
    if (!blockTarget) return true;
    return plainTarget && !item.block && !item.planter && !item.fertilizer && !item.dye && !item.damageable
        && !detail::blockUse(id);
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
    bool hasBlockItem, bool hitBlock, bool interactive, bool sneaking) {
    if (!enabled || !triggered || selected < 0 || selected >= 9 || target < 0 || target >= 9
        || selected == target || !hasBlockItem || !hitBlock || (interactive && !sneaking)) return {};
    return target;
}
}

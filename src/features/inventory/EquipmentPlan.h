#pragma once
#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>
#include <vector>

namespace lamium::inventory {
// L-62: a held item (or worn elytra) with this much durability left or less is
// about to break; the elytra stops working there instead of breaking.
inline constexpr int guardRemaining = 1;
inline bool aboutToBreak(int maxDamage, int damage) {
    return maxDamage > 0 && maxDamage - damage <= guardRemaining;
}
struct EnchantLevel {
    int type = 0, level = 0;
};
// How far two enchantment sets are apart: level differences of shared kinds
// plus the full level of kinds only one side has.
inline int enchantDistance(std::vector<EnchantLevel> const& a, std::vector<EnchantLevel> const& b) {
    int distance = 0;
    for (auto const& x : a) {
        int other = 0;
        for (auto const& y : b) if (y.type == x.type) other = y.level;
        distance += std::abs(x.level - other);
    }
    for (auto const& y : b) {
        bool shared = false;
        for (auto const& x : a) shared = shared || x.type == y.type;
        if (!shared) distance += y.level;
    }
    return distance;
}
struct ReplacementCandidate {
    int slot = -1;
    int enchantDistance = 0;
    int remaining = 0;
};
// Among the same item: closest enchantments, then most durability left, then
// the higher slot (lower rows). Never another item that is about to break.
inline std::optional<int> chooseReplacement(std::vector<ReplacementCandidate> const& candidates) {
    std::optional<ReplacementCandidate> best;
    for (auto const& c : candidates) {
        if (c.slot < 0 || c.remaining <= guardRemaining) continue;
        if (!best || c.enchantDistance < best->enchantDistance
            || (c.enchantDistance == best->enchantDistance && (c.remaining > best->remaining
                || (c.remaining == best->remaining && c.slot > best->slot))))
            best = c;
    }
    return best ? std::optional<int>{best->slot} : std::nullopt;
}
enum class GuardMining { Continue, Wait, Stop };
// Mining with a held tool about to break: wait for the swap when a replacement
// exists, otherwise stop. A new press after a stop mines on (overridden).
inline GuardMining guardMining(bool breaking, bool replacement, bool overridden) {
    if (!breaking || overridden) return GuardMining::Continue;
    return replacement ? GuardMining::Wait : GuardMining::Stop;
}

// L-70: put on an elytra by key or a firework jump; after the flight, wear a
// chestplate again.
struct ElytraSwapState {
    bool active = false;           // An elytra is worn and followed.
    std::optional<int> returnSlot; // Inventory slot holding what the elytra replaced.
    bool flown = false;            // Gliding (or a firework jump) since it went on.
    bool landed = false;           // Touched the ground or water after the flight.
    int ticks = 0;                 // Ticks since that landing.
    bool riptideWait = false;      // A Riptide launch waits for a flyable frame (L-280).
    int riptideStall = 0;          // Frames of that wait without a spin or a lift.
};
enum class ElytraStep { None, PutOn, TakeOff, WearChestplate, Forget };
struct ElytraInput {
    bool enabled = false;
    bool gliding = false;
    bool onGround = false;
    bool wearingElytra = false;
    bool returnSlotHoldsChest = false; // The remembered slot still holds what was worn.
    bool chestplateAvailable = false;  // A chestplate somewhere in the inventory.
    int landingTicks = 60;             // Time after landing before the chestplate returns.
    bool inWater = false;              // In water: no glide is possible there.
    bool spinning = false;             // The Riptide spin the launch started is still running.
    bool rising = false;               // The launch is still carrying the player up.
    bool riptideLaunch = false;        // A Riptide launch started this frame.
    int riptideStallTicks = 10;        // Stop waiting after this many stalled frames.
};
// The delay runs from the first landing after a flight and ignores later hops
// (sprint jumping kept resetting a grounded-time count); only a new glide
// restarts it. A landing in water counts too: without it an elytra glided into
// the sea stayed on until the player reached ground, because the delay never
// started. The swap waits for a moment on the ground. An elytra worn by hand
// is followed once it glides. What goes back on: the remembered chest item,
// else the best chestplate in the inventory, else the elytra stays.
inline ElytraStep elytraStep(ElytraSwapState& state, ElytraInput const& in) {
    // L-280: a Riptide launch does not swap on the spot. It waits for the first
    // frame the player can actually fly (airborne and out of the water), and
    // gives up once the launch stopped carrying them (no spin, no lift) long
    // enough that they are swimming or standing. Underwater fights therefore
    // never touch the chestplate.
    if (in.riptideLaunch) { state.riptideWait = true; state.riptideStall = 0; }
    if (state.riptideWait) {
        if (!in.enabled || in.wearingElytra) state.riptideWait = false;
        else if (!in.inWater && !in.onGround) {
            state.riptideWait = false;
            state.riptideStall = 0;
            return ElytraStep::PutOn;
        } else if (in.spinning || in.rising) state.riptideStall = 0;
        else if (++state.riptideStall >= std::max(in.riptideStallTicks,1)) state.riptideWait = false;
    }
    if (!state.active) {
        if (!in.enabled || !in.wearingElytra || !in.gliding) return ElytraStep::None;
        state = {true,std::nullopt,true,false,0};
    }
    if (!in.enabled || !in.wearingElytra) return ElytraStep::Forget;
    if (state.returnSlot && !in.returnSlotHoldsChest) state.returnSlot.reset();
    if (in.gliding) { state.flown = true; state.landed = false; state.ticks = 0; return ElytraStep::None; }
    if (!state.flown) return ElytraStep::None; // Put on by key on the ground: wait for the key.
    if (!state.landed) {
        if (!in.onGround && !in.inWater) return ElytraStep::None;
        state.landed = true;
    }
    // At least two ticks, so touching down for one tick is not yet landing.
    if (++state.ticks < std::max(in.landingTicks,2) || (!in.onGround && !in.inWater)) return ElytraStep::None;
    if (state.returnSlot) return ElytraStep::TakeOff;
    return in.chestplateAvailable ? ElytraStep::WearChestplate : ElytraStep::Forget;
}
// Whether this frame has anything to do at all: an elytra is followed, a
// Riptide launch waits for its flyable frame, or one is worn by hand and
// gliding. The caller uses this to decide whether to run the state machine;
// leaving the launch out of it is what kept the wait from ever starting.
inline bool followsElytra(ElytraSwapState const& state, ElytraInput const& in) {
    return state.active || state.riptideWait || in.riptideLaunch || (in.wearingElytra && in.gliding);
}
struct ChestplateCandidate {
    int slot = -1;
    int armor = 0, toughness = 0, enchantLevels = 0, remaining = 0;
};
// Highest protection first: armor, toughness, enchantment levels, durability
// left, then the higher slot.
inline std::optional<int> chooseChestplate(std::vector<ChestplateCandidate> const& candidates) {
    std::optional<ChestplateCandidate> best;
    auto key = [](ChestplateCandidate const& c) { return std::array<int,5>{c.armor,c.toughness,c.enchantLevels,c.remaining,c.slot}; };
    for (auto const& c : candidates)
        if (c.slot >= 0 && (!best || key(c) > key(*best))) best = c;
    return best ? std::optional<int>{best->slot} : std::nullopt;
}
// L-246: a firework jump also counts rockets in Fake Offhand's target slot
// while that feature is on, whatever the main hand holds.
inline bool jumpHasFireworks(bool mainHandRockets, bool fakeOffhandOn, bool fakeSlotRockets) {
    return mainHandRockets || (fakeOffhandOn && fakeSlotRockets);
}
}

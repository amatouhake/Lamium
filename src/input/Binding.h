#pragma once
#include <algorithm>
#include <array>
#include <compare>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace lamium::input {
enum class Device { Key, Mouse, Wheel };
struct Token {
    Device device;
    int code;
    auto operator<=>(Token const&) const = default;
};
enum class Action { Settings, Zoom, NightVision, Sort, ChunkBorders, HideOffhand, Hitboxes, ToolSwitch, InfoHud, TargetInfo, DebugView, BreakingRestriction, CaptureBreaking, ResetBreaking, CycleBreakingMode, Freelook, LightOverlay, HandRestock, PermanentSneak, PeriodicAttack, PeriodicUse, ToggleShapes, OpenShapes, FreeCamera, OpenHotkeys, OpenHudLayout, CycleAttackMode, CycleUseMode, AttackHeldOnly, UseHeldOnly, PermanentSprint, EdgeGuard, FakeOffhand, FakeOffhandUse, Transfer, ToolGuard, ElytraSwap, ElytraSwapKey, FreeCameraSpeedUp, FreeCameraSpeedDown, HideWeather, HideParticles, HideBossBars, HideNausea, HideWater, HideLava, HidePowderSnow, Minimap, MinimapZoomIn, MinimapZoomOut, MinimapView, MinimapEnlarge, AddWaypoint, HideWaypoints, OpenWaypoints, OpenWorldMap, TogglePreviews, ToggleDurability, ToggleSorting, ToggleHideEffects, ToggleDurabilityHud, ToggleAutomationStatus, ToggleRadar, ToggleWaypoints, ToggleWorldMap, RadarFaces, ToggleOffhandSlot, ToggleSaturation, ToggleFoodValues, WeaponSwitch, ToggleSchematic, OpenSchematics, ToggleSchematicHud, NearestMistake, SelectLookedPlacement, NextPlacement, MovePlacementForward, MovePlacementBack, MovePlacementHere, RotatePlacement, MirrorPlacement, LayerUp, LayerDown, MovePlacementLeft, MovePlacementRight, MovePlacementUp, MovePlacementDown, LayerHere, OpenSchematicFiles, OpenSchematicCheck, OpenSchematicMaterials, OpenSchematicPlaced, SwapOffhand, ToggleOffhandSwap, SchematicCorner1, SchematicCorner2, SaveSchematicArea, SchematicMenu, AdjustSchematic, ToggleDeathRestore, HideDistanceFog, ToggleLockedTrades, Count };
enum class Behavior { Press, Hold, Toggle };
// Ordinary chords are order-sensitive and yield to a more specific chord
// completed by the same press. Modifier-like chords (held camera keys) match
// in any order and stay active when a longer chord starts on top of them.
enum class Matching { Ordinary, Modifier };
struct ActionInfo { std::string_view id, feature; Behavior behavior; int defaultKey = 0; Matching matching = Matching::Ordinary; };
inline constexpr auto actions = std::to_array<ActionInfo>({
    {"settings", "settings", Behavior::Press, 0x4C},
    {"zoom", "zoom", Behavior::Hold, 0x43, Matching::Modifier},
    {"nightvision", "nightVision", Behavior::Toggle},
    {"sort", "sorting", Behavior::Press, 0x52},
    {"chunkborders", "chunkBorders", Behavior::Toggle},
    {"hideoffhand", "hideOffhand", Behavior::Toggle},
    {"hitboxes", "hitboxes", Behavior::Toggle},
    {"toolswitch", "toolSwitch", Behavior::Toggle},
    {"infohud", "infoHud", Behavior::Toggle},
    {"targetinfo", "targetInfo", Behavior::Toggle},
    {"debugview", "debugView", Behavior::Toggle, 0x72},
    {"breakingrestriction", "restrictions", Behavior::Toggle},
    // Retired by L-15 (the press anchors); kept because ids are saved.
    {"capturebreaking", "", Behavior::Press},
    {"resetbreaking", "", Behavior::Press},
    {"cyclebreakingmode", "restrictions", Behavior::Press},
    {"freelook", "freelook", Behavior::Hold, 0, Matching::Modifier},
    {"lightoverlay", "lightOverlay", Behavior::Toggle},
    {"handrestock", "handRestock", Behavior::Toggle},
    {"permanentsneak", "permanentSneak", Behavior::Toggle},
    {"periodicattack", "periodicAttack", Behavior::Toggle},
    {"periodicuse", "periodicUse", Behavior::Toggle},
    {"toggleshapes", "shapes", Behavior::Toggle},
    {"openshapes", "shapes", Behavior::Press},
    {"freecamera", "freecamera", Behavior::Toggle},
    {"openhotkeys", "settings", Behavior::Press},
    {"openhudlayout", "settings", Behavior::Press},
    // Auto Attack / Auto Use: the periodic* ids above are their on/off switch.
    {"cycleattackmode", "periodicAttack", Behavior::Press},
    {"cycleusemode", "periodicUse", Behavior::Press},
    // Fast click only while held; the ids predate it becoming a switch.
    {"cycleattacktrigger", "periodicAttack", Behavior::Toggle},
    {"cycleusetrigger", "periodicUse", Behavior::Toggle},
    {"permanentsprint", "permanentSprint", Behavior::Toggle},
    {"edgeguard", "edgeGuard", Behavior::Toggle},
    {"fakeoffhand", "fakeOffhand", Behavior::Toggle},
    {"fakeoffhanduse", "fakeOffhand", Behavior::Hold},
    {"transfer", "transfer", Behavior::Toggle},
    {"toolguard", "toolGuard", Behavior::Toggle},
    {"elytraswap", "elytraSwap", Behavior::Toggle},
    {"elytraswapkey", "elytraSwap", Behavior::Press},
    {"freecameraspeedup", "freecamera", Behavior::Press},
    {"freecameraspeeddown", "freecamera", Behavior::Press},
    {"hideweather", "hideEffects", Behavior::Toggle},
    {"hideparticles", "hideEffects", Behavior::Toggle},
    {"hidebossbars", "hideEffects", Behavior::Toggle},
    {"hidenausea", "hideEffects", Behavior::Toggle},
    {"hidewater", "hideEffects", Behavior::Toggle},
    {"hidelava", "hideEffects", Behavior::Toggle},
    {"hidepowdersnow", "hideEffects", Behavior::Toggle},
    {"minimap", "minimap", Behavior::Toggle},
    {"minimapzoomin", "minimap", Behavior::Press},
    {"minimapzoomout", "minimap", Behavior::Press},
    {"minimapview", "caveView", Behavior::Press},
    {"minimapenlarge", "minimap", Behavior::Hold},
    {"addwaypoint", "waypoints", Behavior::Press},
    {"hidewaypoints", "waypoints", Behavior::Hold},
    {"openwaypoints", "waypoints", Behavior::Press},
    {"openworldmap", "worldMap", Behavior::Press, 0x4D},
    // Every saved feature switch can be bound (SETTINGS-KEYMAP rule 1).
    {"previews", "previews", Behavior::Toggle},
    {"durability", "durability", Behavior::Toggle},
    {"sorting", "sorting", Behavior::Toggle},
    {"hideeffects", "hideEffects", Behavior::Toggle},
    {"durabilityhud", "durabilityHud", Behavior::Toggle},
    {"automationstatus", "automationStatus", Behavior::Toggle},
    {"radar", "radar", Behavior::Toggle},
    {"waypoints", "waypoints", Behavior::Toggle},
    {"worldmap", "worldMap", Behavior::Toggle},
    {"radarfaces", "radar", Behavior::Hold},
    {"offhandslot", "offhandSlot", Behavior::Toggle},
    {"saturation", "saturation", Behavior::Toggle},
    {"foodvalues", "foodValues", Behavior::Toggle},
    {"weaponswitch", "weaponSwitch", Behavior::Toggle},
    {"schematic", "schematic", Behavior::Toggle},
    {"openschematics", "schematic", Behavior::Press},
    {"schematichud", "schematicHud", Behavior::Toggle},
    {"nearestmistake", "schematicCheck", Behavior::Press},
    {"selectlookedplacement", "schematicPlacement", Behavior::Press},
    {"nextplacement", "schematicPlacement", Behavior::Press},
    {"moveplacementforward", "schematicPlacement", Behavior::Press},
    {"moveplacementback", "schematicPlacement", Behavior::Press},
    {"moveplacementhere", "schematicPlacement", Behavior::Press},
    {"rotateplacement", "schematicPlacement", Behavior::Press},
    {"mirrorplacement", "schematicPlacement", Behavior::Press},
    {"layerup", "schematicLayers", Behavior::Press},
    {"layerdown", "schematicLayers", Behavior::Press},
    {"moveplacementleft", "schematicPlacement", Behavior::Press},
    {"moveplacementright", "schematicPlacement", Behavior::Press},
    {"moveplacementup", "schematicPlacement", Behavior::Press},
    {"moveplacementdown", "schematicPlacement", Behavior::Press},
    {"layerhere", "schematicLayers", Behavior::Press},
    {"openschematicfiles", "schematic", Behavior::Press},
    {"openschematiccheck", "schematic", Behavior::Press},
    {"openschematicmaterials", "schematic", Behavior::Press},
    {"openschematicplaced", "schematic", Behavior::Press},
    // F as in Java: Bedrock has no swap key of its own (L-94, maintainer 2026-10-06).
    {"swapoffhand", "offhandSwap", Behavior::Press, 0x46},
    {"offhandswap", "offhandSwap", Behavior::Toggle},
    {"schematiccorner1", "schematicSave", Behavior::Press},
    {"schematiccorner2", "schematicSave", Behavior::Press},
    {"saveschematicarea", "schematicSave", Behavior::Press},
    {"schematicmenu", "schematicMenu", Behavior::Press},
    {"adjustschematic", "schematicMenu", Behavior::Hold},
    {"deathrestore", "deathRestore", Behavior::Toggle},
    {"hidedistancefog", "hideEffects", Behavior::Toggle},
    {"lockedtrades", "lockedTrades", Behavior::Toggle},
});
static_assert(actions.size() == static_cast<size_t>(Action::Count));
// Kept for its saved id but no longer shown or dispatched.
constexpr bool retired(Action action) { return actions[static_cast<size_t>(action)].feature.empty(); }
using Chord = std::vector<Token>;
// Absent means Lamium's default; an empty chord explicitly unbinds the
// action. Reset removes the override, restoring the default.
using Bindings = std::array<std::optional<Chord>, static_cast<size_t>(Action::Count)>;

inline Chord defaultChord(Action action) {
    if (action == Action::FakeOffhandUse) return Chord{Token{Device::Mouse, 2}};
    if (action == Action::ChunkBorders) return Chord{{Device::Key, 0x72}, {Device::Key, 0x47}};
    if (action == Action::Hitboxes) return Chord{{Device::Key, 0x72}, {Device::Key, 0x42}};
    int key = actions[static_cast<size_t>(action)].defaultKey;
    if (!key) return {};
    return Chord{Token{Device::Key, key}};
}
inline Chord effectiveChord(Bindings const& bindings, Action action) {
    auto const& override = bindings[static_cast<size_t>(action)];
    if (override) return *override;
    return defaultChord(action);
}
// Clearing the settings binding would lock the screen shut, so only Reset is
// offered for it. Loading also ignores a stored empty settings binding.
inline constexpr bool canClear(Action action) { return action != Action::Settings; }

// Order is the press order and is significant: the last input completes the
// chord. A wheel impulse is always last.
inline Chord canonicalChord(Chord chord, Behavior behavior) {
    if (chord.size() > 8) throw std::invalid_argument("A binding accepts at most eight inputs");
    int wheels = 0;
    for (auto const token : chord) {
        switch (token.device) {
        case Device::Key:
            if (token.code < 1 || token.code > 255) throw std::invalid_argument("Invalid keyboard code");
            break;
        case Device::Mouse:
            if (token.code < 1 || token.code > 5) throw std::invalid_argument("Invalid mouse button");
            break;
        case Device::Wheel:
            if (token.code != -1 && token.code != 1) throw std::invalid_argument("Invalid wheel direction");
            ++wheels;
            break;
        default: throw std::invalid_argument("Invalid input device");
        }
    }
    if (wheels > 1 || (wheels && behavior == Behavior::Hold))
        throw std::invalid_argument("Wheel impulses cannot be combined or held");
    Chord result;
    for (auto token : chord)
        if (std::find(result.begin(), result.end(), token) == result.end()) result.push_back(token);
    std::stable_partition(result.begin(), result.end(), [](Token t) { return t.device != Device::Wheel; });
    return result;
}
// Settings saved before chords kept their order were sorted by code. Restore
// the usual press order: modifiers, function keys, other keys, mouse, wheel.
inline Chord legacyChordOrder(Chord chord) {
    auto rank = [](Token t) {
        if (t.device == Device::Wheel) return 4;
        if (t.device == Device::Mouse) return 3;
        int c = t.code;
        if ((c >= 0x10 && c <= 0x12) || (c >= 0xA0 && c <= 0xA5) || c == 0x5B || c == 0x5C) return 0;
        if (c >= 0x70 && c <= 0x87) return 1;
        return 2;
    };
    std::stable_sort(chord.begin(), chord.end(), [&](Token a, Token b) { return rank(a) < rank(b); });
    return chord;
}
inline bool contains(Chord const& chord, Token token) { return std::find(chord.begin(), chord.end(), token) != chord.end(); }
inline bool coversAll(Chord const& outer, Chord const& inner) {
    return std::all_of(inner.begin(), inner.end(), [&](Token t) { return contains(outer, t); });
}
// Strictly more inputs, including every input of the shorter chord.
inline bool moreSpecific(Chord const& longer, Chord const& shorter) {
    return longer.size() > shorter.size() && coversAll(longer, shorter);
}

enum class Relation { None, Overlap, Shared };
// Shared: the same chord, so both actions fire together. Overlap: one chord's
// inputs include the other's (or the same inputs in another order).
inline Relation bindingRelation(Chord const& a, Chord const& b) {
    if (a.empty() || b.empty()) return Relation::None;
    if (a == b) return Relation::Shared;
    return coversAll(a, b) || coversAll(b, a) ? Relation::Overlap : Relation::None;
}
// Transfer can be toggled in gameplay or a container; Sort is container-only.
inline bool sameInputContext(Action a, Action b) {
    return a == Action::Transfer || b == Action::Transfer || (a == Action::Sort) == (b == Action::Sort);
}
inline bool actionAllowed(Action action, bool gameplay, bool container, bool freeCamera = false) {
    if (action == Action::FreeCameraSpeedUp || action == Action::FreeCameraSpeedDown) return gameplay && freeCamera;
    if (action == Action::Sort) return container;
    if (action == Action::Transfer) return gameplay || container;
    return gameplay;
}
// How another binding relates to this one; the Hotkeys tooltip groups by it.
enum class Link { Same, StartsWithThis, ContainsThis, InsideThis, Reordered };
struct Conflict { Action action; Relation relation; Link link; };
inline bool startsWith(Chord const& longer, Chord const& prefix) {
    return longer.size() > prefix.size() && std::equal(prefix.begin(), prefix.end(), longer.begin());
}
// Ordered by link, then by action.
inline std::vector<Conflict> bindingConflicts(Bindings const& bindings, Action action) {
    std::vector<Conflict> result;
    auto chord = effectiveChord(bindings, action);
    for (size_t i = 0; i < actions.size(); ++i) {
        auto other = static_cast<Action>(i);
        if (other == action || !sameInputContext(action, other)) continue;
        auto otherChord = effectiveChord(bindings, other);
        auto relation = bindingRelation(chord, otherChord);
        if (relation == Relation::None) continue;
        Link link = relation == Relation::Shared ? Link::Same
            : startsWith(otherChord, chord) ? Link::StartsWithThis
            : otherChord.size() > chord.size() ? Link::ContainsThis
            : chord.size() > otherChord.size() ? Link::InsideThis : Link::Reordered;
        result.push_back({other, relation, link});
    }
    std::stable_sort(result.begin(), result.end(), [](Conflict a, Conflict b) { return a.link < b.link; });
    return result;
}

using ChordSet = std::array<Chord, actions.size()>;
// Like Java's F3: a Press/Toggle action whose chord begins a longer bound
// chord waits for its release and fires then, unless the longer chord was used
// meanwhile. Hold actions cannot wait; they act while held.
inline std::optional<size_t> releaseLeader(ChordSet const& chords, size_t index) {
    auto const& chord = chords[index];
    auto const& info = actions[index];
    if (chord.empty() || chord.back().device == Device::Wheel || info.matching != Matching::Ordinary
        || info.behavior == Behavior::Hold) return {};
    for (size_t j = 0; j < chords.size(); ++j) {
        bool extends = actions[j].matching == Matching::Modifier ? moreSpecific(chords[j], chord) : startsWith(chords[j], chord);
        if (j != index && extends) return j;
    }
    return {};
}
// The action whose longer chord makes this one fire on release, for the
// Hotkeys description.
inline std::optional<Action> firesOnRelease(Bindings const& bindings, Action action) {
    ChordSet chords;
    for (size_t i = 0; i < chords.size(); ++i)
        if (sameInputContext(action, static_cast<Action>(i))) chords[i] = effectiveChord(bindings, static_cast<Action>(i));
    auto leader = releaseLeader(chords, static_cast<size_t>(action));
    if (!leader) return {};
    return static_cast<Action>(*leader);
}

class HeldInputs {
    Chord held, blocked;
public:
    // Press order is kept: ordinary chords compare against it.
    Chord const& value() const { return held; }
    // True for a fresh press that may complete a chord (every accepted wheel
    // impulse is one); key repeats and blocked inputs are not.
    bool observe(Token token, bool down, bool accepted) {
        if (token.device == Device::Wheel) return down && accepted;
        if (!down) { std::erase(held, token); std::erase(blocked, token); return false; }
        if (!accepted) {
            if (!contains(held, token) && !contains(blocked, token)) blocked.push_back(token);
            return false;
        }
        if (contains(blocked, token) || contains(held, token)) return false;
        held.push_back(token);
        return true;
    }
    void invalidate() {
        for (auto token : held)
            if (!contains(blocked, token)) blocked.push_back(token);
        held.clear();
    }
    void clear() { held.clear(); blocked.clear(); }
};

struct Transition {
    size_t action;
    bool pressed;
    bool operator==(Transition const&) const = default;
};
struct Dispatch {
    std::vector<Transition> transitions;
    // The input event belongs to Lamium and should not reach the game.
    bool consumed = false;
};
// Matches every action's chord against one input event. Actions activate on
// the press that completes their chord, or on its release when they lead a
// longer chord (Pending). A chord that yielded to a more specific one stays
// latched until one of its inputs is released, so it never fires late. Host
// code resets this on focus/world/screen changes, rebinding or input
// ownership changes and delivers the released edges; a pending action then
// never fires.
class ChordDispatch {
    enum class Phase { Idle, Active, Pending, Latched };
    std::array<Phase, actions.size()> phases{};
    static bool pulsed(Chord const& chord) { return !chord.empty() && chord.back().device == Device::Wheel; }
    static bool heldPart(Chord const& chord, Chord const& held) {
        return std::all_of(chord.begin(), chord.end(), [&](Token t) { return t.device == Device::Wheel || contains(held, t); });
    }
    static bool inOrder(Chord const& chord, Chord const& held) {
        std::ptrdiff_t last = -1;
        for (auto token : chord) {
            if (token.device == Device::Wheel) continue;
            auto at = std::find(held.begin(), held.end(), token) - held.begin();
            if (at <= last) return false;
            last = at;
        }
        return true;
    }
    static bool completes(Chord const& chord, Matching matching, Chord const& held, Token press) {
        if (chord.empty() || !heldPart(chord, held)) return false;
        if (press.device == Device::Wheel) return chord.back() == press && (matching == Matching::Modifier || inOrder(chord, held));
        if (pulsed(chord)) return false;
        if (matching == Matching::Modifier) return contains(chord, press);
        return chord.back() == press && inOrder(chord, held);
    }
public:
    bool isActive(Action action) const { return phases[static_cast<size_t>(action)] == Phase::Active; }
    // chords[i] is empty when the action is unbound or not allowed here.
    // down is a key/button press (repeats included) or a wheel impulse; fresh
    // says it newly entered the held set. Pass nothing for a release or for an
    // event another consumer cancelled: only releases are reported then.
    Dispatch update(ChordSet const& chords, Chord const& held, std::optional<Token> down = {}, bool fresh = false) {
        Dispatch result;
        for (size_t i = 0; i < chords.size(); ++i) {
            if (phases[i] == Phase::Idle) continue;
            if (!chords[i].empty() && heldPart(chords[i], held)) continue;
            if (phases[i] == Phase::Active) result.transitions.push_back({i, false});
            if (phases[i] == Phase::Pending && !chords[i].empty()) result.transitions.push_back({i, true});
            phases[i] = Phase::Idle;
        }
        if (!down) return result;
        std::vector<size_t> candidates, firing;
        if (fresh)
            for (size_t i = 0; i < chords.size(); ++i)
                if (phases[i] == Phase::Idle && completes(chords[i], actions[i].matching, held, *down)) candidates.push_back(i);
        for (auto i : candidates) {
            bool yields = std::any_of(candidates.begin(), candidates.end(), [&](size_t j) { return moreSpecific(chords[j], chords[i]); });
            if (!yields) firing.push_back(i);
            else if (!pulsed(chords[i])) phases[i] = Phase::Latched;
        }
        // A longer chord starting on top of an ordinary held or pending action
        // takes over; the shorter one does not resume or fire on release.
        for (size_t i = 0; i < chords.size(); ++i) {
            bool active = phases[i] == Phase::Active && actions[i].matching == Matching::Ordinary;
            if (!active && phases[i] != Phase::Pending) continue;
            if (std::any_of(firing.begin(), firing.end(), [&](size_t j) { return moreSpecific(chords[j], chords[i]); })) {
                if (active) result.transitions.push_back({i, false});
                phases[i] = Phase::Latched;
            }
        }
        bool acted = false;
        for (auto i : firing) {
            if (releaseLeader(chords, i)) { phases[i] = Phase::Pending; continue; }
            result.transitions.push_back({i, true});
            acted = true;
            if (!pulsed(chords[i])) phases[i] = Phase::Active;
        }
        result.consumed = acted;
        if (down->device != Device::Wheel)
            for (size_t i = 0; i < chords.size(); ++i)
                if (phases[i] != Phase::Idle && contains(chords[i], *down)) result.consumed = true;
        return result;
    }
    std::vector<Transition> reset() {
        std::vector<Transition> released;
        for (size_t i = 0; i < phases.size(); ++i) {
            if (phases[i] == Phase::Active) released.push_back({i, false});
            phases[i] = Phase::Idle;
        }
        return released;
    }
};
}

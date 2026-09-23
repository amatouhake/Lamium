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
enum class Action { Settings, Zoom, NightVision, Sort, ChunkBorders, HideOffhand, Hitboxes, ToolSwitch, InfoHud, TargetInfo, DebugView, BreakingRestriction, CaptureBreaking, ResetBreaking, CycleBreakingMode, Freelook, LightOverlay, HandRestock, PermanentSneak, PeriodicAttack, PeriodicUse, ToggleShapes, OpenShapes, Count };
enum class Behavior { Press, Hold, Toggle };
struct ActionInfo { std::string_view id, feature; Behavior behavior; int defaultKey = 0; };
inline constexpr auto actions = std::to_array<ActionInfo>({
    {"settings", "settings", Behavior::Press, 0x4C},
    {"zoom", "zoom", Behavior::Hold, 0x43},
    // N belongs to Minecraft notifications; retain J for NightVision.
    {"nightvision", "nightVision", Behavior::Toggle, 0x4a},
    {"sort", "sorting", Behavior::Press, 0x52},
    {"chunkborders", "chunkBorders", Behavior::Toggle},
    {"hideoffhand", "hideOffhand", Behavior::Toggle},
    {"hitboxes", "hitboxes", Behavior::Toggle},
    {"toolswitch", "toolSwitch", Behavior::Toggle},
    {"infohud", "infoHud", Behavior::Toggle},
    {"targetinfo", "targetInfo", Behavior::Toggle},
    {"debugview", "debugView", Behavior::Toggle},
    {"breakingrestriction", "restrictions", Behavior::Toggle},
    {"capturebreaking", "restrictions", Behavior::Press},
    {"resetbreaking", "restrictions", Behavior::Press},
    {"cyclebreakingmode", "restrictions", Behavior::Press},
    {"freelook", "freelook", Behavior::Hold},
    {"lightoverlay", "lightOverlay", Behavior::Toggle},
    {"handrestock", "handRestock", Behavior::Toggle},
    {"permanentsneak", "permanentSneak", Behavior::Toggle},
    {"periodicattack", "periodicAttack", Behavior::Toggle},
    {"periodicuse", "periodicUse", Behavior::Toggle},
    {"toggleshapes", "shapes", Behavior::Toggle},
    {"openshapes", "shapes", Behavior::Press},
});
static_assert(actions.size() == static_cast<size_t>(Action::Count));
using Chord = std::vector<Token>;
// Absent means Lamium's default; an empty chord explicitly unbinds the
// action. Reset removes the override, restoring the default.
using Bindings = std::array<std::optional<Chord>, static_cast<size_t>(Action::Count)>;

inline Chord defaultChord(Action action) {
    int key = actions[static_cast<size_t>(action)].defaultKey;
    if (!key) return {};
    return Chord{Token{Device::Key, key}};
}
inline Chord effectiveChord(Bindings const& bindings, Action action) {
    auto const& override = bindings[static_cast<size_t>(action)];
    if (override) return *override;
    return defaultChord(action);
}

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
    std::sort(chord.begin(), chord.end());
    chord.erase(std::unique(chord.begin(), chord.end()), chord.end());
    return chord;
}

struct Edge { bool pressed = false, released = false; };
class HeldInputs {
    Chord held, blocked;
public:
    Chord const& value() const { return held; }
    void observe(Token token, bool down, bool accepted) {
        if (token.device == Device::Wheel) return;
        if (!down) { std::erase(held, token); std::erase(blocked, token); return; }
        if (!accepted) {
            if (std::find(held.begin(), held.end(), token) == held.end()
                && std::find(blocked.begin(), blocked.end(), token) == blocked.end()) blocked.push_back(token);
            return;
        }
        if (std::find(blocked.begin(), blocked.end(), token) != blocked.end()) return;
        if (std::find(held.begin(), held.end(), token) == held.end()) held.push_back(token);
    }
    void invalidate() {
        for (auto token : held)
            if (std::find(blocked.begin(), blocked.end(), token) == blocked.end()) blocked.push_back(token);
        held.clear();
    }
    void clear() { held.clear(); blocked.clear(); }
};
// One state per action. Host code resets this on focus/world/screen changes,
// rebinding, or input ownership changes and delivers any released edge.
class BindingState {
    bool active = false;
public:
    bool isActive() const { return active; }
    Edge update(Chord const& chord, Chord const& held, std::optional<Token> impulse = {}) {
        bool matches = !chord.empty();
        for (auto const token : chord) {
            bool present = token.device == Device::Wheel ? impulse == token
                : std::find(held.begin(), held.end(), token) != held.end();
            matches = matches && present;
        }
        bool pulsed = std::any_of(chord.begin(), chord.end(), [](Token t) { return t.device == Device::Wheel; });
        if (pulsed) return {matches, false};
        Edge result{matches && !active, !matches && active};
        active = matches;
        return result;
    }
    Edge reset() { bool wasActive = active; active = false; return {false, wasActive}; }
};
}

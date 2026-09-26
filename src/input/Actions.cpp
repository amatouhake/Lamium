#include "input/Actions.h"
#include "features/interaction/PermanentSneak.h"
#include "features/interaction/PeriodicInput.h"
#include "input/ToggleAction.h"
#include "settings/Options.h"
#include "features/interaction/BreakingRestriction.h"
#include "app/Runtime.h"
#include "features/camera/Zoom.h"
#include "features/inventory/Inventory.h"
#include "ui/SettingsScreen.h"
#include "ui/SettingsRows.h"
#include "ui/Toast.h"
#include "ui/Localization.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/input/KeyboardRemappingLayout.h"
#include "mc/client/options/IOptionRegistry.h"

namespace lamium {
namespace {
// Auto Attack / Auto Use toasts and status name the mode: "Hold", or
// "Fast click (while held)".
std::optional<std::string> autoModeText(Settings const& value, input::Action action) {
    auto attack = input::actions[static_cast<size_t>(action)].feature == "periodicAttack";
    if (!attack && input::actions[static_cast<size_t>(action)].feature != "periodicUse") return std::nullopt;
    return interaction::autoModeText(attack ? value.interaction.attackMode : value.interaction.useMode,
        attack ? value.interaction.attackHeldOnly : value.interaction.useHeldOnly);
}
std::string toggleFeatureName(input::Action action) {
    auto id = input::actions[static_cast<size_t>(action)].feature;
    for (auto const& feature : ui::features)
        if (feature.id == id) return ui::translated(feature.name);
    return std::string(id);
}
bool toggleState(IClientInstance& client, Settings const& value, input::Action action) {
    if (action == input::Action::BreakingRestriction) return value.interaction.breaking;
    if (action == input::Action::PermanentSneak) return interaction::sneak::active(client);
    if (action == input::Action::PermanentSprint) return interaction::sprint::active(client);
    auto id = input::actions[static_cast<size_t>(action)].feature;
    for (auto const& feature : ui::features) {
        if (feature.id != id || feature.toggle.empty()) continue;
        if (auto option = settings::find(feature.toggle)) {
            auto current = option->read(value);
            if (auto state = std::get_if<bool>(&current)) return *state;
        }
    }
    return false;
}
void emitToggleToast(IClientInstance& client, input::Action action, Settings const& value) {
    auto name = toggleFeatureName(action);
    if (auto mode = autoModeText(value, action)) name += ": " + *mode;
    ui::showToggleToast(name, toggleState(client, value, action));
}
}
std::string bindingChordName(IClientInstance& client, input::Chord const& chord) {
    auto layout = client.getOptions().getCurrentKeyboardRemapping();
    if (chord.empty()) return ui::translated("unbound");
    std::string result;
    for (auto token : chord) {
        if (!result.empty()) result += " + ";
        if (token.device == input::Device::Wheel) result += ui::translated(token.code > 0 ? "wheelUp" : "wheelDown");
        else if (token.device == input::Device::Mouse) result += ui::translated("mouseButton", token.code);
        else result += layout ? layout->getMappedKeyName(token.code) : std::to_string(token.code);
    }
    return result;
}
std::string actionBindingName(IClientInstance& client, input::Action action) {
    return bindingChordName(client, input::effectiveChord(Runtime::instance().preferences().bindings, action));
}
void executeAction(IClientInstance& client, input::Action action) {
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || ui::ownsInput()) return;
    // Sorting validates its container/text-input context in requestSort.
    if (action == input::Action::Sort) { inventory::requestSort(client); return; }
    if (!gameplayScreen(client.getScreenName())) return;
    if (action == input::Action::PermanentSneak) {
        interaction::sneak::toggle(client);
        emitToggleToast(client, action, runtime.preferences());
        return;
    }
    if (action == input::Action::PermanentSprint) {
        interaction::sprint::toggle(client);
        emitToggleToast(client, action, runtime.preferences());
        return;
    }
    if (action == input::Action::CaptureBreaking) { interaction::breaking::capture(client); return; }
    if (action == input::Action::ResetBreaking) { interaction::breaking::reset(); return; }
    if (action == input::Action::Settings) { ui::open(client); return; }
    if (action == input::Action::OpenShapes) { ui::openShapes(client); return; }
    if (action == input::Action::OpenHotkeys) { ui::openHotkeys(client); return; }
    if (action == input::Action::OpenHudLayout) { ui::openHudLayout(client); return; }
    if (action == input::Action::Zoom) { Zoom::instance().press(client); return; }
    if (action == input::Action::Freelook) { Zoom::instance().pressLook(client); return; }
    if (action == input::Action::FreeCamera) { Zoom::instance().pressFreeCamera(client); return; }
    auto value = runtime.preferences();
    if (action == input::Action::CycleAttackMode || action == input::Action::CycleUseMode) {
        settings::find(action == input::Action::CycleAttackMode ? "interaction.attackMode" : "interaction.useMode")->adjust(value,1);
        if (!runtime.save(value)) { runtime.self().getLogger().error("Could not save auto mode"); return; }
        emitToggleToast(client, action, value);
        return;
    }
    if (action == input::Action::CycleBreakingMode) {
        settings::find("interaction.breakingMode")->adjust(value,1);
        if (!runtime.save(value)) runtime.self().getLogger().error("Could not save breaking mode");
        return;
    }
    if (input::toggleAction(value, action)) {
        if (!runtime.save(value)) {
            runtime.self().getLogger().error("Could not save action setting: {}", input::actions[static_cast<size_t>(action)].id);
            return;
        }
        // Emit only once the new state is persisted; a failed save keeps the
        // old settings, so a toast would report a change that never happened.
        emitToggleToast(client, action, value);
    }
}
void releaseAction(input::Action action) {
    if (action == input::Action::Zoom) Zoom::instance().release();
    if (action == input::Action::Freelook) Zoom::instance().releaseLookKey();
}
}

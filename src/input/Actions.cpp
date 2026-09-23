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
#include "ui/Localization.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/input/KeyboardRemappingLayout.h"
#include "mc/client/options/IOptionRegistry.h"

namespace lamium {
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
std::string gameplayKeyHint(IClientInstance& client) {
    auto layout = client.getOptions().getCurrentKeyboardRemapping();
    if (!layout) return ui::translated("controls");
    return ui::translated("gameplay",
        actionBindingName(client, input::Action::Settings), actionBindingName(client, input::Action::Zoom),
        actionBindingName(client, input::Action::NightVision));
}

void executeAction(IClientInstance& client, input::Action action) {
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || ui::ownsInput()) return;
    // Sorting validates its container/text-input context in requestSort.
    if (action == input::Action::Sort) { inventory::requestSort(client); return; }
    if (!gameplayScreen(client.getScreenName())) return;
    if (action == input::Action::PermanentSneak) { interaction::sneak::toggle(client); return; }
    if (action == input::Action::PeriodicAttack) { interaction::periodic::toggle(client, interaction::periodic::Action::Attack); return; }
    if (action == input::Action::PeriodicUse) { interaction::periodic::toggle(client, interaction::periodic::Action::Use); return; }
    if (action == input::Action::CaptureBreaking) { interaction::breaking::capture(client); return; }
    if (action == input::Action::ResetBreaking) { interaction::breaking::reset(); return; }
    if (action == input::Action::Settings) { ui::open(client); return; }
    if (action == input::Action::OpenShapes) { ui::openShapes(client); return; }
    if (action == input::Action::Zoom) { Zoom::instance().press(client); return; }
    if (action == input::Action::Freelook) { Zoom::instance().pressLook(client); return; }
    auto value = runtime.preferences();
    if (action == input::Action::CycleBreakingMode) {
        settings::find("interaction.breakingMode")->adjust(value,1);
        if (!runtime.save(value)) runtime.self().getLogger().error("Could not save breaking mode");
        return;
    }
    if (input::toggleAction(value, action) && !runtime.save(value))
        runtime.self().getLogger().error("Could not save action setting: {}", input::actions[static_cast<size_t>(action)].id);
}
void releaseAction(input::Action action) {
    if (action == input::Action::Zoom) Zoom::instance().release();
    if (action == input::Action::Freelook) Zoom::instance().releaseLookKey();
}
}

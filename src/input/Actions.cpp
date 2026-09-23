#include "input/Actions.h"
#include "features/interaction/PermanentSneak.h"
#include "input/ToggleAction.h"
#include "settings/Options.h"
#include "features/interaction/BreakingRestriction.h"
#include "app/Runtime.h"
#include "features/camera/Zoom.h"
#include "features/inventory/Inventory.h"
#include "ui/SettingsScreen.h"
#include "ui/Localization.h"
#include "ll/api/input/KeyRegistry.h"
#include "mc/client/input/KeyboardRemappingLayout.h"
#include "mc/client/options/IOptionRegistry.h"

namespace lamium {
namespace {
bool usesNative(input::Action action) {
    return !Runtime::instance().preferences().bindings[static_cast<size_t>(action)];
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
    auto layout = client.getOptions().getCurrentKeyboardRemapping();
    auto override = Runtime::instance().preferences().bindings[static_cast<size_t>(action)];
    if (override) return bindingChordName(client, *override);
    if (!layout) return ui::translated("unbound");
    auto const& mapping = layout->getKeymappingByAction("key.Lamium." + std::string(input::actions[static_cast<size_t>(action)].id));
    if (!mapping.isAssigned()) return ui::translated("unbound");
    return static_cast<RemappingLayout const&>(*layout).getMappedKeyName(mapping);
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
    if (action == input::Action::CaptureBreaking) { interaction::breaking::capture(client); return; }
    if (action == input::Action::ResetBreaking) { interaction::breaking::reset(); return; }
    if (action == input::Action::Settings) { ui::open(client); return; }
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
    if (action == input::Action::Freelook) Zoom::instance().releaseLook();
}
void registerActions() {
    auto& registry = ll::input::KeyRegistry::getInstance();
    for (size_t i=0; i<input::actions.size(); ++i) {
        auto action = static_cast<input::Action>(i);
        auto const& info = input::actions[i];
        std::vector<int> defaults;
        if (info.defaultKey) defaults.push_back(info.defaultKey);
        auto& key = registry.getOrCreateKey(info.id, defaults);
        key.registerButtonDownHandler([action](FocusImpact, IClientInstance& client) {
            if (usesNative(action)) executeAction(client, action);
        });
        if (info.behavior == input::Behavior::Hold) {
            key.registerButtonUpHandler([action](FocusImpact, IClientInstance&) {
                if (usesNative(action)) releaseAction(action);
            });
        }
    }
}
}

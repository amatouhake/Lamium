#include "settings/SettingsStore.h"
#include "settings/Options.h"
#include "ui/Translations.h"
#include <unordered_set>
#include <chrono>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif
void check(bool, char const*);
void settingsStoreTests() {
    using namespace lamium;
    {
        auto defaults = decodeSettings(R"({"interaction":{"breaking":false}})");
        check(defaults.interaction.attackInterval == .5f && defaults.interaction.useInterval == .5f,
              "older interaction settings preserve default periodic cadence");
        auto bounded = decodeSettings(R"({"interaction":{"attackInterval":0,"useInterval":999}})");
        check(bounded.interaction.attackInterval == .1f && bounded.interaction.useInterval == 60.f,
              "stored periodic intervals cannot become zero or unbounded");
        bounded.interaction.attackInterval = std::numeric_limits<float>::quiet_NaN();
        bounded.interaction.useInterval = std::numeric_limits<float>::infinity();
        bounded.normalize();
        check(bounded.interaction.attackInterval == .5f && bounded.interaction.useInterval == .5f,
              "non-finite periodic intervals recover a usable cadence");
    }
    {
        auto* mode = settings::find("interaction.breakingMode");
        Settings value;
        mode->adjust(value,-1);
        check(value.interaction.breakingMode == interaction::RestrictionMode::Layer, "choice wraps backward");
        mode->adjust(value,0);
        check(value.interaction.breakingMode == interaction::RestrictionMode::Plane, "click advances choice and wraps forward");
        check(!mode->numeric && std::get<settings::ChoiceValue>(mode->read(value)).label == "mode.plane",
              "choice exposes localized label and never opens numeric input");
        for (auto label : interaction::restrictionLabels)
            check(!ui::translations::find(label,"en_US").empty() && !ui::translations::find(label,"ja_JP").empty(),
                  "every restriction choice has both translations");
        bool rejected = false;
        try { (void)decodeSettings(R"({"interaction":{"breakingMode":"unknown"}})"); }
        catch (...) { rejected = true; }
        check(rejected, "unknown stored restriction mode is not silently reinterpreted");
    }
    auto old = decodeSettings(R"({"version":1,"camera":{"zoom":true,"magnification":3.5,"wheelStep":0.5}})");
    check(old.camera.magnification == 3.5f && !old.lighting.nightVision,
          "adding lighting must preserve existing camera settings");
    check(old.inventory.sorting && old.inventory.sortContainers, "old settings supply inventory defaults");
    check(!old.camera.freelook, "existing installations keep experimental Freelook disabled");
    check(!old.camera.freelookToggle, "Freelook activation defaults to holding the key");
    check(!old.camera.freecamera, "existing installations keep experimental FreeCamera disabled");
    {
        Settings toggled; toggled.camera.freelookToggle = true;
        check(decodeSettings(R"({"camera":{"freelookToggle":true}})").camera.freelookToggle, "Freelook activation is read from storage");
        auto activation = settings::find("camera.freelookActivation");
        check(activation && std::get<settings::ChoiceValue>(activation->read(toggled)).label == "activation.toggle",
              "activation choice reads the stored mode");
        activation->adjust(toggled, 1);
        check(!toggled.camera.freelookToggle, "activation choice cycles back to hold");
    }
    check(input::actions[static_cast<size_t>(input::Action::Freelook)].behavior == input::Behavior::Hold
          && input::actions[static_cast<size_t>(input::Action::Freelook)].defaultKey == 0,
          "Freelook is an independent unassigned hold action");
    check(input::actions[static_cast<size_t>(input::Action::FreeCamera)].behavior == input::Behavior::Toggle
          && input::actions[static_cast<size_t>(input::Action::FreeCamera)].defaultKey == 0,
          "FreeCamera is an independent unassigned toggle action");
    for (auto action : {input::Action::OpenHotkeys, input::Action::OpenShapes})
        check(input::actions[static_cast<size_t>(action)].behavior == input::Behavior::Press
              && input::actions[static_cast<size_t>(action)].feature == "settings",
              "screen openers group with the settings keys");
    check(input::actions[static_cast<size_t>(input::Action::OpenHotkeys)].defaultKey == 0
          && input::defaultChord(input::Action::OpenHotkeys).empty(),
          "OpenHotkeys is unbound by default");
    check(decodeSettings(R"({"interface":{"gameplayHints":false}})").ui.automationStatus,
          "removed gameplay hints key loads without error");
    check(!old.inspection.hideShulkerContents, "older settings retain vanilla Shulker contents text");
    check(old.inspection.shulkerPreviews && old.inspection.emptyShulkerPreviews
          && old.inspection.bundlePreviews && old.inspection.emptyBundlePreviews,
          "older files retain the existing preview behavior including empty containers");
    auto partial = decodeSettings(R"({"camera":{"magnification":6}})");
    check(!old.visuals.hideOffhand, "existing settings keep the offhand visible");
    check(partial.camera.magnification == 6 && partial.camera.zoom, "missing fields use defaults");
    for (auto invalid : {R"({"version":999})", R"({"version":4294967297})", R"({"version":1.5})",
                         R"({"version":true})", R"({"camera":{"zoom":"yes"}})", "[]", "{"}) {
        bool rejected = false;
        try { (void)decodeSettings(invalid); } catch (...) { rejected = true; }
        check(rejected, "invalid or future settings must be rejected");
    }
    auto path = std::filesystem::temp_directory_path() /
        ("lamium-settings-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); }
    } cleanup{path};
    // Exercise the actual UI accessors through disk persistence. This catches
    // options which appear editable but are omitted from encoding or decoding.
    std::unordered_set<std::string_view> ids;
    for (auto const& option : settings::options) {
        check(ids.insert(option.id).second, "option identities are unique");
        check(settings::find(option.id) == &option, "stable option lookup");
        check(!ui::translations::find(option.label, "en_US").empty()
              && !ui::translations::find(option.label, "ja_JP").empty(), "option labels exist");
        Settings edited;
        option.adjust(edited, 1);
        check(option.read(edited) != option.read(Settings{}), "editing changes the target value");
        for (auto const& other : settings::options)
            if (other.id != option.id)
                check(other.read(edited) == other.read(Settings{}), "editing preserves unrelated options");
        writeSettings(path, edited);
        auto restored = readSettings(path);
        for (auto const& other : settings::options)
            check(other.read(restored) == other.read(edited), "all options survive disk round trip");
    }
    check(settings::find("unknown") == nullptr, "unknown option lookup is safe");
    {
        auto legacy = decodeSettings("{}");
        for (auto const& binding : legacy.bindings) check(!binding, "legacy settings fall back to Lamium defaults");
        auto configured = decodeSettings(R"({"bindings":{"zoom":[{"device":"key","code":90},{"device":"key","code":51}],"sort":[],"nightvision":[{"device":"key","code":16},{"device":"wheel","code":1}]}})");
        writeSettings(path, configured);
        check(readSettings(path).bindings == configured.bindings, "chords, wheel and explicit unbound survive restart");
        auto const border = static_cast<size_t>(input::Action::ChunkBorders);
        input::Chord f3b{{input::Device::Key, 0x72}, {input::Device::Key, 0x42}}, bf3{f3b[1], f3b[0]};
        check(decodeSettings(R"({"bindings":{"chunkborders":[{"device":"key","code":66},{"device":"key","code":114}]}})")
            .bindings[border] == f3b, "sorted legacy chords regain F3-first press order");
        auto ordered = decodeSettings(R"({"orderedBindings":true,"bindings":{"chunkborders":[{"device":"key","code":66},{"device":"key","code":114}]}})");
        check(ordered.bindings[border] == bf3, "saved press order is kept as written");
        writeSettings(path, ordered);
        check(readSettings(path).bindings[border] == bf3, "press order survives restart");
        configured.bindings[static_cast<size_t>(input::Action::Zoom)].reset();
        writeSettings(path, configured);
        check(!readSettings(path).bindings[static_cast<size_t>(input::Action::Zoom)], "reset removes only the override");
        check(readSettings(path).bindings[static_cast<size_t>(input::Action::Sort)]->empty(), "reset preserves another unbound action");
        auto locked = decodeSettings(R"({"bindings":{"settings":[]}})"
        );
        check(!locked.bindings[static_cast<size_t>(input::Action::Settings)],
              "stored empty settings binding recovers the default");
        auto remapped = decodeSettings(R"({"bindings":{"settings":[{"device":"key","code":70}]}})"
        );
        check(remapped.bindings[static_cast<size_t>(input::Action::Settings)]
                  == input::Chord{input::Token{input::Device::Key, 70}},
              "non-empty settings binding still loads");
        for (auto invalid : {R"({"bindings":[]})", R"({"bindings":{"zoom":true}})",
                             R"({"bindings":{"zoom":[{"device":"wheel","code":1}]}})",
                             R"({"bindings":{"sort":[{"device":"key","code":4294967350}]}})",
                             R"({"bindings":{"sort":[{"device":"key","code":51.5}]}})"}) {
            bool rejected = false;
            try { (void)decodeSettings(invalid); } catch (...) { rejected = true; }
            check(rejected, "malformed binding files rejected without integer narrowing");
        }
    }
    {
        std::ofstream initial(path);
        initial << R"({"version":1,"extension":{"keep":true},"camera":{"futureField":42}})";
    }
    old.lighting.nightVision = true;
    old.inventory.sorting = false;
    old.inventory.sortContainers = false;
    old.ui.automationStatus = false;
    old.interaction.attackInterval = 1.2f;
    old.camera.freelookToggle = true;
    old.interaction.useInterval = 3.4f;
    writeSettings(path, old);
    auto loaded = readSettings(path);
    check(loaded.interaction.attackInterval == 1.2f && loaded.interaction.useInterval == 3.4f,
          "independent attack and use intervals survive disk round trip");
    check(loaded.camera.magnification == 3.5f && loaded.lighting.nightVision && loaded.camera.freelookToggle, "disk round trip");
    check(!loaded.inventory.sorting && !loaded.inventory.sortContainers, "inventory switches survive saves");
    check(!loaded.ui.automationStatus, "hidden automation status survives restart");
    auto contents = [&]() {
        std::ifstream file(path);
        return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    };
    auto saved = contents();
    check(saved.find("futureField") != std::string::npos && saved.find("extension") != std::string::npos,
          "unknown fields survive saves");
#ifdef _WIN32
    auto handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    check(handle != INVALID_HANDLE_VALUE, "lock destination for replacement failure test");
    bool rejected = false;
    try { writeSettings(path, Settings{}); } catch (...) { rejected = true; }
    CloseHandle(handle);
    check(rejected && contents() == saved, "failed replacement must preserve previous settings");
    auto temporary = path; temporary += ".tmp";
    check(!std::filesystem::exists(temporary), "failed save cleans its temporary file");
#endif
}

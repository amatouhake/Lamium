#include "settings/SettingsStore.h"
#include "settings/Options.h"
#include "ui/Translations.h"
#include <unordered_set>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif
void check(bool, char const*);
void settingsStoreTests() {
    using namespace lamium;
    auto old = decodeSettings(R"({"version":1,"camera":{"zoom":true,"magnification":3.5,"wheelStep":0.5}})");
    check(old.camera.magnification == 3.5f && !old.lighting.nightVision,
          "adding lighting must preserve existing camera settings");
    check(old.inventory.sorting && old.inventory.sortContainers, "old settings supply inventory defaults");
    check(old.ui.gameplayHints, "existing settings preserve visible gameplay hints by default");
    check(old.inspection.shulkerPreviews && old.inspection.emptyShulkerPreviews
          && old.inspection.bundlePreviews && old.inspection.emptyBundlePreviews,
          "older files retain the existing preview behavior including empty containers");
    auto partial = decodeSettings(R"({"camera":{"magnification":6}})");
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
        for (auto const& binding : legacy.bindings) check(!binding, "legacy settings use native remaps");
        auto configured = decodeSettings(R"({"bindings":{"zoom":[{"device":"key","code":90},{"device":"key","code":51}],"sort":[],"nightvision":[{"device":"key","code":16},{"device":"wheel","code":1}]}})");
        writeSettings(path, configured);
        check(readSettings(path).bindings == configured.bindings, "chords, wheel and explicit unbound survive restart");
        configured.bindings[static_cast<size_t>(input::Action::Zoom)].reset();
        writeSettings(path, configured);
        check(!readSettings(path).bindings[static_cast<size_t>(input::Action::Zoom)], "reset removes only native override");
        check(readSettings(path).bindings[static_cast<size_t>(input::Action::Sort)]->empty(), "reset preserves another unbound action");
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
    old.ui.gameplayHints = false;
    writeSettings(path, old);
    auto loaded = readSettings(path);
    check(loaded.camera.magnification == 3.5f && loaded.lighting.nightVision, "disk round trip");
    check(!loaded.inventory.sorting && !loaded.inventory.sortContainers, "inventory switches survive saves");
    check(!loaded.ui.gameplayHints, "hidden gameplay hints survive restart");
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

#include "settings/SettingsStore.h"
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

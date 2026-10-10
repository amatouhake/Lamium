#include "features/schematic/SchematicSession.h"
#include "features/map/WaypointSession.h"
#include "app/Runtime.h"
#include "app/SessionIds.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <map>
#include <mutex>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

namespace lamium::schematic::session {
namespace {
std::mutex mutex;
PlacementSet set;
std::optional<std::filesystem::path> destination;
unsigned world = 0;
bool loadFailed = false; // Never overwrite a file that could not be read.
std::uint64_t revision = 1;
std::uint64_t nextId = 1; // never reset: ids are not reused within the session
struct Cached {
    std::filesystem::file_time_type modified;
    std::chrono::steady_clock::time_point checked;
    std::shared_ptr<Structure const> structure;
};
std::map<std::string, Cached> cache;
constexpr size_t maxFiles = 2000;
constexpr std::uintmax_t maxFileSize = 256ull * 1024 * 1024;

void log(std::string const& text) {
    try { Runtime::instance().self().getLogger().info("Schematics: {}", text); } catch (...) {}
}
// Follows the joined world: reloads placements when it changes. Caller holds the lock.
void follow() {
    auto place = map::waypoints::place();
    if (place.world == world && (place.active || set.placements.empty())) return;
    world = place.world;
    set = {};
    destination.reset();
    loadFailed = false;
    ++revision;
    if (!place.active) return;
    destination = place.schematicFile;
    if (!destination || !std::filesystem::exists(*destination)) return;
    try {
        set = readPlacements(*destination);
        assignSessionIds(set.placements, nextId);
    }
    catch (std::exception const& error) {
        loadFailed = true;
        log(std::string("could not load placements: ") + error.what());
    }
}
bool commit(PlacementSet candidate) {
    if (loadFailed) return false;
    normalize(candidate);
    assignSessionIds(candidate.placements, nextId);
    if (destination) {
        try { writePlacements(*destination, candidate); }
        catch (std::exception const& error) { log(std::string("could not save placements: ") + error.what()); return false; }
    }
    set = std::move(candidate);
    ++revision;
    return true;
}
std::shared_ptr<Structure const> load(std::string const& relative, std::string* error) {
    auto fail = [&](std::string const& why) -> std::shared_ptr<Structure const> {
        if (error) *error = why;
        return nullptr;
    };
    if (!safeSchematicPath(relative)) return fail("unsafe path");
    // Snapshots run every frame: look at the file's time at most every two seconds.
    auto now = std::chrono::steady_clock::now();
    if (auto found = cache.find(relative); found != cache.end() && now - found->second.checked < std::chrono::seconds(2))
        return found->second.structure;
    auto path = folder() / std::filesystem::u8path(relative);
    std::error_code code;
    auto modified = std::filesystem::last_write_time(path, code);
    if (code) return fail("file not found");
    if (auto found = cache.find(relative); found != cache.end() && found->second.modified == modified) {
        found->second.checked = now;
        return found->second.structure;
    }
    auto size = std::filesystem::file_size(path, code);
    if (code || size > maxFileSize) return fail("file too large");
    std::ifstream file(path, std::ios::binary);
    std::vector<std::uint8_t> bytes(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return fail("could not read the file");
    try {
        auto structure = std::make_shared<Structure const>(parseStructure(bytes));
        cache[relative] = {modified, now, structure};
        return structure;
    } catch (std::exception const& problem) {
        log(relative + ": " + problem.what());
        return fail(problem.what());
    }
}
}

std::filesystem::path folder() { return Runtime::instance().self().getModDir() / "schematics"; }

std::vector<FileEntry> files() {
    std::vector<FileEntry> out;
    std::error_code code;
    auto root = folder();
    std::filesystem::create_directories(root, code);
    std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, code), end;
    for (; !code && it != end && out.size() < maxFiles; it.increment(code)) {
        if (it.depth() > 4) { it.disable_recursion_pending(); continue; }
        if (!it->is_regular_file(code) || it->path().extension() != ".mcstructure") continue;
        auto relative = std::filesystem::relative(it->path(), root, code).generic_u8string();
        std::string text(relative.begin(), relative.end());
        if (!safeSchematicPath(text)) continue;
        out.push_back({std::move(text), it->file_size(code)});
    }
    std::sort(out.begin(), out.end(), [](auto const& a, auto const& b) { return a.relative < b.relative; });
    return out;
}

bool openFolder() {
    std::error_code code;
    auto root = folder();
    std::filesystem::create_directories(root, code);
    auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", root.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) log("could not open the schematics folder");
    return result > 32;
}

std::shared_ptr<Structure const> structure(std::string const& relative, std::string* error) {
    std::lock_guard lock(mutex);
    return load(relative, error);
}

std::shared_ptr<Structure const> loaded(std::string const& relative) {
    std::lock_guard lock(mutex);
    auto found = cache.find(relative);
    return found == cache.end() ? nullptr : found->second.structure;
}

PlacementSet current() {
    std::lock_guard lock(mutex);
    follow();
    return set;
}

bool change(std::function<bool(PlacementSet&)> const& mutation) {
    std::lock_guard lock(mutex);
    follow();
    auto candidate = set;
    if (!mutation(candidate)) return false;
    return commit(std::move(candidate));
}

bool place(std::string const& relative, Point feet, int dimension, std::string* error) {
    std::lock_guard lock(mutex);
    follow();
    if (!load(relative, error)) return false;
    if (set.placements.size() >= maxPlacements) { if (error) *error = "too many placements"; return false; }
    auto candidate = set;
    SavedPlacement added;
    added.file = relative;
    auto slash = relative.find_last_of('/');
    added.name = relative.substr(slash == std::string::npos ? 0 : slash + 1);
    if (added.name.ends_with(".mcstructure")) added.name.resize(added.name.size() - std::string_view(".mcstructure").size());
    added.dimension = dimension;
    added.placement.origin = feet;
    candidate.placements.push_back(std::move(added));
    candidate.selected = static_cast<int>(candidate.placements.size()) - 1;
    return commit(std::move(candidate));
}

Snapshot snapshot() {
    std::lock_guard lock(mutex);
    follow();
    Snapshot out{revision, set.selected, {}};
    for (auto const& p : set.placements) out.placements.push_back({p, load(p.file, nullptr)});
    return out;
}

void start() {}
void stop() {
    std::lock_guard lock(mutex);
    set = {};
    destination.reset();
    cache.clear();
    world = 0;
    ++revision;
}
}

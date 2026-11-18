#include "features/recipes/RecipeState.h"
#include "app/AtomicFile.h"
#include "app/Runtime.h"
#include "features/map/WaypointSession.h"
#include "features/schematic/SchematicItems.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include <atomic>
#include <chrono>

namespace lamium::recipes::state {
namespace {
ll::event::ListenerPtr exitListener;
std::atomic<bool> left{false}; // the world was left: the wanted list is another world's next time
Notes held;
bool notesRead = false, notesKeep = false; // keep: the file was there and could not be read, so it is not written over
Wanted list;
unsigned listWorld = 0;
std::optional<std::filesystem::path> listFile;
bool listKeep = false;
unsigned revision = 1;
Places placed;
std::optional<std::filesystem::path> placedFile;
bool placedKeep = false;
std::map<std::string, Provider> providers;
std::vector<Handed> handedNow;
std::chrono::steady_clock::time_point handedAt{};
std::chrono::steady_clock::time_point placeAsked{};
struct Chosen {
    std::shared_ptr<service::Table const> table;
    unsigned revision = 0;
    std::shared_ptr<Choice const> choice;
} chosenNow;
struct Planned {
    unsigned revision = 0;
    std::chrono::steady_clock::time_point at{};
    Plan plan;
} plannedNow;
void log(std::string const& text) {
    try { Runtime::instance().self().getLogger().warn("Recipes: {}", text); } catch (...) {}
}
std::filesystem::path notesPath() { return Runtime::instance().self().getConfigDir() / "recipes.json"; }
void readNotes() {
    if (notesRead) return;
    notesRead = true;
    try {
        // A file that is there and cannot be read or understood is left alone, not replaced by an empty one.
        auto document = readDocument(notesPath(), notesKeep);
        if (document) {
            notesKeep = !documentReadable(*document);
            if (!notesKeep) held = decodeNotes(*document);
        }
        if (notesKeep) log("recipes.json could not be read; changes are kept for this session only");
    } catch (...) { notesKeep = true; }
}
void writeNotes() {
    ++revision;
    if (notesKeep) return;
    try { writeFileReplacing(notesPath(), encodeNotes(held), "recipe notes"); }
    catch (std::exception const& error) { log(std::string("could not save recipes.json: ") + error.what()); }
}
// The wanted list follows the joined world; asked a few times a second, not every frame.
void followWorld() {
    auto const now = std::chrono::steady_clock::now();
    bool const gone = left.exchange(false);
    if (!gone && now - placeAsked < std::chrono::milliseconds(400)) return;
    placeAsked = now;
    auto const place = map::waypoints::place();
    if (!gone && place.world == listWorld) return;
    listWorld = place.world;
    list = {};
    listFile = place.active ? place.wantedFile : std::nullopt;
    listKeep = false;
    placed = {};
    placedFile.reset();
    placedKeep = false;
    ++revision;
    if (!listFile) return;
    try {
        auto document = readDocument(*listFile, listKeep);
        if (document) {
            listKeep = !documentReadable(*document);
            if (!listKeep) list = decodeWanted(*document);
        }
        if (listKeep) log("the wanted items' file could not be read; changes are kept for this session only");
    } catch (...) { listKeep = true; }
    // The counted places are the world's too, in a file beside it.
    placedFile = listFile->parent_path() / "places.json";
    try {
        auto document = readDocument(*placedFile, placedKeep);
        if (document) {
            placedKeep = !documentReadable(*document);
            if (!placedKeep) placed = decodePlaces(*document);
        }
        if (placedKeep) log("the counted places' file could not be read; changes are kept for this session only");
    } catch (...) { placedKeep = true; }
}
void writePlaces() {
    ++revision;
    if (!placedFile || placedKeep) return;
    try {
        std::error_code ignored;
        std::filesystem::create_directories(placedFile->parent_path(), ignored);
        writeFileReplacing(*placedFile, encodePlaces(placed), "counted places");
    } catch (std::exception const& error) { log(std::string("could not save the counted places: ") + error.what()); }
}
std::int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
void writeWanted() {
    ++revision;
    if (!listFile || listKeep) return;
    try {
        std::error_code ignored;
        std::filesystem::create_directories(listFile->parent_path(), ignored);
        writeFileReplacing(*listFile, encodeWanted(list), "wanted items");
    } catch (std::exception const& error) { log(std::string("could not save the wanted items: ") + error.what()); }
}
}
Notes const& notes() {
    readNotes();
    return held;
}
Wanted const& wanted() {
    followWorld();
    return list;
}
Places const& places() {
    followWorld();
    return placed;
}
void seePlace(bool ender, Spot const& spot, std::optional<Spot> const& pair, std::string const& block, std::vector<Stored> items) {
    followWorld();
    if (placed.see(ender, spot, pair, block, std::move(items), nowSeconds())) writePlaces();
}
bool markStore(Spot const& spot, std::optional<Spot> const& pair, std::string const& block, std::vector<Stored> items) {
    followWorld();
    if (!placed.mark(spot, pair, block, std::move(items), nowSeconds())) return false;
    writePlaces();
    return true;
}
void unmarkStore(int index) {
    followWorld();
    if (placed.unmark(index)) writePlaces();
}
void setPlaceCounted(int index, bool counted) {
    followWorld();
    if (index < 0 || index >= static_cast<int>(placed.list.size()) || placed.list[static_cast<size_t>(index)].counted == counted) return;
    placed.list[static_cast<size_t>(index)].counted = counted;
    writePlaces();
}
void setCarriedCounted(bool carried, bool boxes) {
    followWorld();
    if (placed.carried == carried && placed.boxes == boxes) return;
    placed.carried = carried;
    placed.boxes = boxes;
    writePlaces();
}
std::map<std::string, std::uint64_t> heldByName(LocalPlayer& player) {
    auto out = places().carried ? schematic::items::carried(player, placed.boxes) : std::map<std::string, std::uint64_t>{};
    for (auto const& item : placeTotals(places())) out[item.name] += item.count;
    return out;
}
void provide(std::string const& source, Provider provider) {
    if (provider) providers[source] = provider; else providers.erase(source);
    handedAt = {};
}
std::vector<Handed> const& handed() {
    auto const now = std::chrono::steady_clock::now();
    if (now - handedAt < std::chrono::milliseconds(500)) return handedNow;
    handedAt = now;
    std::vector<Handed> next;
    for (auto const& [source, provider] : providers) {
        try {
            for (auto& [key, count] : provider())
                if (!key.empty() && count && next.size() < 2048) next.push_back({std::move(key), std::min<Count>(count, Wanted::most), source});
        } catch (...) {}
    }
    if (next != handedNow) handedNow = std::move(next), ++revision;
    return handedNow;
}
void toggleFavourite(std::string const& key) {
    readNotes();
    held.toggleFavourite(key);
    writeNotes();
}
void toggleWanted(std::string const& key) {
    followWorld();
    list.toggle(key);
    writeWanted();
}
void setWanted(std::string const& key, Count amount) {
    followWorld();
    list.set(key, amount);
    writeWanted();
}
void removeWanted(std::string const& key) {
    followWorld();
    list.remove(key);
    writeWanted();
}
void stepWanted(std::string const& key, bool up, bool shift, bool button) {
    followWorld();
    if (!list.has(key)) return;
    Count const stack = static_cast<Count>(std::max(1, schematic::items::shown(key).stack));
    if (Count const next = steppedWanted(list.count(key), up, shift ? stack : 1, button)) list.set(key, next);
    else list.remove(key);
    writeWanted();
}
void setChoice(std::string const& item, std::string const& recipe) {
    readNotes();
    if (recipe.empty()) held.choices.erase(item); else held.choices[item] = recipe;
    writeNotes();
}
void setPick(std::string const& group, std::string const& member) {
    readNotes();
    if (member.empty()) held.picks.erase(group); else held.picks[group] = member;
    writeNotes();
}
void setStop(std::string const& item, bool gathered) {
    readNotes();
    if (gathered) held.stops.insert(item); else held.stops.erase(item);
    writeNotes();
}
std::shared_ptr<Choice const> choice(std::shared_ptr<service::Table const> const& table) {
    if (!table) return nullptr;
    readNotes();
    if (chosenNow.table != table || chosenNow.revision != revision || !chosenNow.choice)
        chosenNow = {table, revision, std::make_shared<Choice const>(chosen(table->index, table->choice, held))};
    return chosenNow.choice;
}
Plan const* plan(LocalPlayer& player) {
    auto table = service::table();
    if (!table) return nullptr;
    followWorld();
    auto const& given = handed(); // first: a change of it is a new revision
    auto const now = std::chrono::steady_clock::now();
    auto& plan = plannedNow.plan;
    if (plan.table == table && plannedNow.revision == revision && now - plannedNow.at < std::chrono::milliseconds(500)) return &plan;
    plan.table = table;
    plan.choice = choice(table);
    plan.held.clear();
    if (placed.carried)
        for (auto const& stack : schematic::items::carriedStacks(player, placed.boxes))
            if (Item const id = table->index.find(stack.name, stack.aux); id >= 0) plan.held[id] += stack.count;
    plan.request = {};
    apply(table->index, list, given, plan.request);
    apply(table->index, held, plan.request);
    plan.stored.clear();
    for (auto const& item : placeTotals(placed))
        if (Item const id = table->index.find(item.name, item.aux); id >= 0) plan.stored[id] += item.count;
    auto both = plan.held;
    for (auto const& [id, count] : plan.stored) both[id] += count;
    plan.request.held.assign(both.begin(), both.end());
    plan.view = rawView(table->index, *plan.choice, plan.request);
    plannedNow.revision = revision;
    plannedNow.at = now;
    return &plan;
}
void start() {
    exitListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { left = true; });
}
void stop() {
    if (exitListener) { ll::event::EventBus::getInstance().removeListener(exitListener); exitListener.reset(); }
    left = true;
}
}

#include "features/recipes/RecipeState.h"
#include <format>
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
// A change of the wanted list is written once changes stop for a moment (a wheel turn steps a count many times, and
// a write takes milliseconds on the client thread), and before the list is left for another world or Lamium stops.
bool wantedDirty = false;
std::chrono::steady_clock::time_point wantedChangedAt{};
constexpr auto wantedSettle = std::chrono::milliseconds(800);
unsigned revision = 1;
Places placed;
std::atomic<std::shared_ptr<std::vector<StoreBox> const>> marks;
void publishMarks() { marks.store(std::make_shared<std::vector<StoreBox> const>(storeBoxes(placed))); }
std::optional<std::filesystem::path> placedFile;
bool placedKeep = false;
// The counted places change up to four times a second while a store is open: written once that settles, as the
// wanted list is.
bool placesDirty = false;
std::chrono::steady_clock::time_point placesChangedAt{};
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
// A save that took long is told in the log (a few times a session): files are written on the client thread.
struct SaveCost {
    char const* what;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    ~SaveCost() {
        static int told = 0;
        auto const us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
        if (us >= 4000 && told < 10) ++told, log(std::format("{} took {} us", what, us));
    }
};
void writeNotes() {
    SaveCost const saveCost{"writeNotes"};
    ++revision;
    if (notesKeep) return;
    try { writeFileReplacing(notesPath(), encodeNotes(held), "recipe notes"); }
    catch (std::exception const& error) { log(std::string("could not save recipes.json: ") + error.what()); }
}
void saveWanted() {
    SaveCost const saveCost{"writeWanted"};
    wantedDirty = false;
    if (!listFile || listKeep) return;
    try {
        std::error_code ignored;
        std::filesystem::create_directories(listFile->parent_path(), ignored);
        writeFileReplacing(*listFile, encodeWanted(list), "wanted items");
    } catch (std::exception const& error) { log(std::string("could not save the wanted items: ") + error.what()); }
}
void savePlaces();
// Writes the wanted list and the counted places once their changes have settled; `now` writes them at once.
void flushWanted(bool now) {
    auto const at = std::chrono::steady_clock::now();
    if (wantedDirty && (now || at - wantedChangedAt >= wantedSettle)) saveWanted();
    if (placesDirty && (now || at - placesChangedAt >= wantedSettle)) savePlaces();
}
// The wanted list follows the joined world; asked a few times a second, not every frame.
void followWorld() {
    flushWanted(false);
    auto const now = std::chrono::steady_clock::now();
    bool const gone = left.exchange(false);
    if (!gone && now - placeAsked < std::chrono::milliseconds(400)) return;
    placeAsked = now;
    auto const place = map::waypoints::place();
    if (!gone && place.world == listWorld) return;
    flushWanted(true); // into the world it belongs to, before the list is that of another
    listWorld = place.world;
    list = {};
    listFile = place.active ? place.wantedFile : std::nullopt;
    listKeep = false;
    placed = {};
    publishMarks();
    placedFile.reset();
    placedKeep = false;
    ++revision;
    if (listFile) try {
        auto document = readDocument(*listFile, listKeep);
        if (document) {
            listKeep = !documentReadable(*document);
            if (!listKeep) list = decodeWanted(*document);
        }
        if (listKeep) log("the wanted items' file could not be read; changes are kept for this session only");
    } catch (...) { listKeep = true; }
    placedFile = place.active ? place.placesFile : std::nullopt;
    if (!placedFile) { publishMarks(); return; }
    try {
        auto document = readDocument(*placedFile, placedKeep);
        if (document) {
            placedKeep = !documentReadable(*document);
            if (!placedKeep) placed = decodePlaces(*document);
        }
        if (placedKeep) log("the counted places' file could not be read; changes are kept for this session only");
    } catch (...) { placedKeep = true; }
    publishMarks();
}
void savePlaces() {
    SaveCost const saveCost{"writePlaces"};
    placesDirty = false;
    if (!placedFile || placedKeep) return;
    try {
        std::error_code ignored;
        std::filesystem::create_directories(placedFile->parent_path(), ignored);
        writeFileReplacing(*placedFile, encodePlaces(placed), "counted places");
    } catch (std::exception const& error) { log(std::string("could not save the counted places: ") + error.what()); }
}
// A change of the counted places: shown at once, written once changes settle.
void writePlaces() {
    ++revision;
    publishMarks();
    placesDirty = true;
    placesChangedAt = std::chrono::steady_clock::now();
}
std::int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
// A change of the wanted list: every screen shows it the next frame; the file follows once changes settle.
void writeWanted() {
    ++revision;
    wantedDirty = true;
    wantedChangedAt = std::chrono::steady_clock::now();
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
std::shared_ptr<std::vector<StoreBox> const> storeMarks() { return marks.load(); }
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
void setCarriedCounted(bool carried) {
    followWorld();
    if (placed.carried == carried) return;
    placed.carried = carried;
    writePlaces();
}
std::map<std::string, std::uint64_t> heldByName(LocalPlayer& player) {
    // Without the stores (L-373) what is carried counts; with them, the places switched on, carried among them.
    auto const settings = Runtime::instance().snapshot();
    if (!settings->inventory.stores) return schematic::items::carried(player);
    auto out = places().carried ? schematic::items::carried(player) : std::map<std::string, std::uint64_t>{};
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
void toggleFavorite(std::string const& key) {
    readNotes();
    held.toggleFavorite(key);
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
    if (!list.listed(key)) return;
    Count const stack = static_cast<Count>(std::max(1, schematic::items::shown(key).stack));
    if (Count const next = steppedWanted(list.count(key), up, shift ? stack : 1)) list.set(key, next);
    else if (button) list.remove(key);
    else list.zero(key);
    writeWanted();
}
void settleWanted(std::string_view keep) {
    followWorld();
    if (list.settle(keep)) writeWanted();
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
    held.setGathered(item, gathered);
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
        for (auto const& stack : schematic::items::carriedStacks(player))
            if (Item const id = table->index.find(stack.name, stack.aux); id >= 0) plan.held[id] += stack.count;
    plan.request = {};
    apply(table->index, list, given, plan.request);
    apply(table->index, held, plan.request);
    plan.stored.clear();
    for (auto const& item : placeTotals(placed))
        if (Item const id = table->index.find(item.name, item.aux); id >= 0) plan.stored[id] += item.count;
    std::map<std::string, Count> byName;
    if (placed.carried) for (auto const& stack : schematic::items::carriedStacks(player)) byName[stack.name] += stack.count;
    for (auto const& item : placeTotals(placed)) byName[item.name] += item.count;
    plan.loose = looseLines(table->index, list, given, byName);
    auto both = plan.held;
    for (auto const& [id, count] : plan.stored) both[id] += count;
    plan.request.held.assign(both.begin(), both.end());
    plan.view = rawView(table->index, *plan.choice, plan.request);
    plannedNow.revision = revision;
    plannedNow.at = now;
    return &plan;
}
Shares const* shares(LocalPlayer& player) {
    auto const* now = plan(player);
    if (!now) return nullptr;
    // The plan is worked out again twice a second, but its shares only change with what they are made from:
    // they stand until the entries, the stock or a choice changes. A long list costs tens of milliseconds, so
    // new shares are worked out a few milliseconds a frame while the last ones are still shown (L-390 1-12).
    struct From {
        std::shared_ptr<service::Table const> table;
        std::shared_ptr<Choice const> choice;
        std::vector<WantedEntry> entries;
        Request request;
        bool same(From const& o) const {
            return table == o.table && choice == o.choice && entries == o.entries && request.wanted == o.request.wanted
                && request.held == o.request.held && request.picks == o.request.picks && request.stops == o.request.stops;
        }
    };
    static Shares kept;
    static From keptFrom;
    static std::optional<From> workingFrom;
    static std::optional<TargetPlanning> working;
    static std::chrono::steady_clock::time_point keptFor{}, workingSince{};
    static bool newer = false;
    if (keptFor != plannedNow.at) {
        keptFor = plannedNow.at;
        From from{now->table, now->choice, prioritized(list, handed()), {}};
        std::vector<std::pair<Item, Count>> order;
        for (auto const& entry : from.entries) order.emplace_back(now->table->index.findKey(entry.key), entry.count);
        from.request = {order, now->request.held, now->request.picks, now->request.stops};
        // Shares being worked out finish first, unless the world's table went: inputs that change faster than a
        // long list is worked out would otherwise never show; the newer ones are taken up once these are shown.
        if (workingFrom && workingFrom->table == from.table && !from.same(*workingFrom)) newer = true;
        else if (from.same(keptFrom)) working.reset(), workingFrom.reset();
        else if (!workingFrom || !from.same(*workingFrom)) {
            // Shares of another table name its items by other ids: none are shown until the new ones are.
            if (from.table != keptFrom.table) kept = {}, keptFrom = {};
            working.emplace(order, now->request.held, now->request.picks, now->request.stops);
            workingFrom = std::move(from);
            workingSince = std::chrono::steady_clock::now();
        }
    }
    if (!working) return &kept;
    // One slice a frame: the panel and the window both ask, and rows built from the first answer must not meet
    // shares replaced by the second.
    static std::chrono::steady_clock::time_point sliced{};
    auto const started = std::chrono::steady_clock::now();
    if (started - sliced < std::chrono::milliseconds(8)) return &kept;
    sliced = started;
    do working->step(workingFrom->table->index, *workingFrom->choice);
    while (!working->done() && std::chrono::steady_clock::now() - started < std::chrono::milliseconds(3));
    if (!working->done()) return &kept;
    kept.entries = workingFrom->entries;
    kept.targets = std::move(working->out);
    keptFrom = std::move(*workingFrom);
    working.reset(), workingFrom.reset();
    if (std::exchange(newer, false)) keptFor = {}; // looked at again on the next call
    // Each entry is planned twice; say once how long a long list took from start to shown.
    static bool told = false;
    if (!told && kept.entries.size() >= 10) {
        told = true;
        auto const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - workingSince).count();
        log(std::format("the plan's shares of {} wanted items were shown after {} ms", kept.entries.size(), took));
    }
    return &kept;
}
void start() {
    // The stores' outlines are the world's: none in the next one until its places are read.
    exitListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) {
        left = true;
        marks.store(nullptr);
    });
}
void settle() {
    try { flushWanted(false); } catch (...) {}
}
void stop() {
    if (exitListener) { ll::event::EventBus::getInstance().removeListener(exitListener); exitListener.reset(); }
    try { flushWanted(true); } catch (...) {}
    left = true;
}
}

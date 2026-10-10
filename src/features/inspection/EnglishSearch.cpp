#include "features/inspection/EnglishSearch.h"
#include "app/Runtime.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/memory/Hook.h"
#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"
#include "mc/world/containers/managers/models/CraftingContainerManagerModel.h"
#include "mc/world/containers/models/FilterResult.h"
#include "mc/world/containers/models/TextSearchMode.h"
#include "mc/world/item/ItemInstance.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace lamium::inspection::englishSearch {
namespace {
// English display names by description id, from the game's en_US strings.
std::mutex namesMutex;
std::unordered_map<std::string, std::string> names;
std::shared_ptr<Localization const> english;
std::atomic<int> samplesLogged{0};
std::atomic<bool> unavailableLogged{false};

std::string englishName(ItemInstance const& item) {
    auto id = item.getDescriptionId();
    std::lock_guard lock{namesMutex};
    if (auto found = names.find(id); found != names.end()) return found->second;
    if (!english) english = getI18n().getLocaleFor("en_US");
    std::string name;
    // The description id already ends in ".name" (tile.bamboo_mosaic.name).
    auto key = id.ends_with(".name") ? id : id + ".name";
    if (!english || !english->get(key, name, {})) name.clear();
    if (!english && !unavailableLogged.exchange(true))
        Runtime::instance().self().getLogger().warn("English search: no en_US strings; identifiers only");
    if (samplesLogged.fetch_add(1) < 5)
        Runtime::instance().self().getLogger().info("English search: {} -> '{}'", id, name);
    names.emplace(std::move(id), name);
    return name;
}
LL_TYPE_INSTANCE_HOOK(SearchHook, ll::memory::HookPriority::Normal, CraftingContainerManagerModel,
    &CraftingContainerManagerModel::_filterByText, FilterResult, ItemInstance const& item, TextSearchMode mode) {
    auto result = origin(item, mode);
    if (result != FilterResult::Hide) return result;
    try {
        if (!Runtime::instance().snapshot()->inspection.englishSearch || item.isNull() || !item.mItem) return result;
        std::string const& query = this->mSearchString;
        if (query.empty()) return result;
        if (matches(query, englishName(item), item.getTypeName())) return FilterResult::Show;
    } catch (...) {}
    return result;
}
bool hooked = false;
ll::event::ListenerPtr exitListener;
}
bool start() {
    if (!hooked) hooked = SearchHook::hook(true) == 0;
    if (!hooked) return false;
    if (!exitListener)
        // Resource packs and the language can change between worlds.
        exitListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) {
            std::lock_guard lock{namesMutex};
            names.clear();
            english.reset();
        });
    return true;
}
void stop() {
    if (exitListener) {
        ll::event::EventBus::getInstance().removeListener(exitListener);
        exitListener.reset();
    }
    if (hooked && SearchHook::unhook(true)) hooked = false;
    std::lock_guard lock{namesMutex};
    names.clear();
    english.reset();
}
}

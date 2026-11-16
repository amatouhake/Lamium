#include "features/inspection/EnglishSearch.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/memory/Hook.h"
#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"
#include "mc/world/containers/managers/models/CraftingContainerManagerModel.h"
#include "mc/world/containers/models/FilterResult.h"
#include "mc/world/containers/models/TextSearchMode.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/enchanting/Enchant.h"
#include "mc/world/item/enchanting/EnchantmentInstance.h"
#include "mc/world/item/enchanting/ItemEnchants.h"
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
// An enchantment's English name and identifier by type (L-344).
std::unordered_map<int, std::string> enchantNames;
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
// Caller holds namesMutex and has tried to load `english`.
std::string const& enchantName(Enchant::Type type) {
    int const key = static_cast<int>(type);
    if (auto found = enchantNames.find(key); found != enchantNames.end()) return found->second;
    std::string text;
    for (auto const& enchant : Enchant::mEnchants()) {
        if (!enchant || static_cast<Enchant::Type>(enchant->mEnchantType) != type) continue;
        std::string name;
        std::string const& description = enchant->mDescription;
        if (!english || !english->get(enchantNameKey(description), name, {})) name.clear();
        text = name + "\n" + static_cast<HashedString const&>(enchant->mStringId).getString();
        if (samplesLogged.fetch_add(1) < 5)
            Runtime::instance().self().getLogger().info("English search: enchantment {} -> '{}'", description, name);
        break;
    }
    return enchantNames.emplace(key, std::move(text)).first->second;
}
// A Creative enchanted book is found by its stored enchantments; other
// items keep only their own name. Books are never in the recipe book, so
// this stays a Creative search addition. The enchantment data is read under
// its own capability: on a game it was not checked on a book is found by its
// own name only.
std::string storedEnchantNames(ItemInstance const& item) {
    if (item.getTypeName() != "minecraft:enchanted_book" || !versionSensitiveAllowed(capabilities::Id::EnglishSearchEnchants)) return {};
    auto const stored = item.constructItemEnchantsFromUserData().getAllEnchants();
    std::string out;
    std::lock_guard lock{namesMutex};
    if (!english) english = getI18n().getLocaleFor("en_US");
    for (auto const& e : stored) out += enchantName(e.mEnchantType) + "\n";
    return out;
}
LL_TYPE_INSTANCE_HOOK(SearchHook, ll::memory::HookPriority::Normal, CraftingContainerManagerModel,
    &CraftingContainerManagerModel::_filterByText, FilterResult, ItemInstance const& item, TextSearchMode mode) {
    auto result = origin(item, mode);
    if (result != FilterResult::Hide) return result;
    try {
        if (!Runtime::instance().snapshot()->inspection.englishSearch || item.isNull() || !item.mItem) return result;
        std::string const& query = this->mSearchString;
        if (query.empty()) return result;
        if (matches(query, englishName(item), item.getTypeName(), storedEnchantNames(item))) return FilterResult::Show;
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
        exitListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { forget(); });
    return true;
}
void stop() {
    if (exitListener) {
        ll::event::EventBus::getInstance().removeListener(exitListener);
        exitListener.reset();
    }
    if (hooked && SearchHook::unhook(true)) hooked = false;
    forget();
}
void forget() {
    std::lock_guard lock{namesMutex};
    names.clear();
    enchantNames.clear();
    english.reset();
}
}

#include "features/recipes/CreativeTake.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "features/recipes/ItemList.h"
#include "features/recipes/StackKey.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/world/containers/controllers/ContainerController.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/containers/models/ContainerModel.h"
#include "mc/world/item/ItemStack.h"
#include <algorithm>
#include <memory>
#include <optional>

namespace lamium::recipes::creativeTake {
namespace {
std::string const search = "recipe_search", cursor = "cursor_items";
// The crafting screens carry the creative list's output beside its search; the furnaces' search cells take
// nothing and a chest has neither (creative_trace, VALIDATION-LOG 2026-11-24).
bool hasList(ContainerManagerController& manager) {
    return manager.hasContainerController("creative_output_items") && manager.hasContainerController(search);
}
std::optional<int> cellOf(ContainerScreenController& screen, ContainerManagerController& manager, std::string const& key) {
    int const size = manager.getContainerSize(search);
    for (int i = 0; i < size && i < 8000; ++i) {
        auto const& stack = screen._getVisualItemStack(search, i);
        if (!stack.isNull() && stackKey(stack) == key) return i;
    }
    return std::nullopt;
}
// The model fills `recipe_search` with every entry the game's search lets through, also while the pane shows
// another tab or none; its cells stay for the screen's visit.
void refresh(ContainerManagerController& manager) {
    auto const& containers = manager.mContainers.get();
    auto const found = containers.find(search);
    if (found == containers.end() || !found->second) return;
    std::weak_ptr<ContainerModel> const& weak = found->second->mContainerModel;
    if (auto model = weak.lock()) model->refreshContainer(true);
}
void tell(bool refreshed) {
    static bool told[2]{};
    if (told[refreshed]) return;
    told[refreshed] = true;
    Runtime::instance().self().getLogger().info("Recipes: an item taken in creative, {}", refreshed ? "after a refresh of the creative search" : "from a cell shown");
}
}
bool available() { return versionSensitiveAllowed(capabilities::Id::CreativeTake); }
take::Refusal take(ScreenView& view, std::string const& key, bool stack, bool pointer) {
    if (!available() || key.empty() || !view.mController || !view.mController->_isContainerScreen()) return take::Refusal::NotHere;
    auto& screen = static_cast<ContainerScreenController&>(*view.mController);
    auto manager = screen.mContainerManagerController.get();
    if (!manager || manager->mContainersClosed) return take::Refusal::NotHere;
    bool const here = hasList(*manager);
    std::optional<int> cell;
    if (here) {
        cell = cellOf(screen, *manager, key);
        if (!cell) {
            refresh(*manager);
            cell = cellOf(screen, *manager, key);
            if (cell) tell(true);
        } else tell(false);
    }
    bool const listed = itemList::creativeRank(key) < (1 << 30);
    if (auto const why = take::refusal(here, listed, cell.has_value()); why != take::Refusal::None) return why;
    int const most = std::max(1, static_cast<int>(screen._getVisualItemStack(search, *cell).getMaxStackSize()));
    int const amount = stack ? most : 1;
    bool const empty = !manager->hasContainerController(cursor) || manager->getItemStack(cursor, 0).isNull();
    if (take::into(pointer, empty) == take::Into::Pointer) screen._handleTakeAmount(amount, search, *cell);
    else screen._handleAutoPlace(amount, search, *cell);
    return take::Refusal::None;
}
}

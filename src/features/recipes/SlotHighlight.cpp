#include "features/recipes/SlotHighlight.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ui/Widgets.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/item/ItemStack.h"
#include <chrono>
#include <string>
#include <vector>

namespace lamium::recipes::slotHighlight {
namespace {
struct Cell {
    float x = 0, y = 0, w = 0, h = 0;
    std::string collection;
    int index = 0;
};
std::vector<Cell> cells;
std::string cellsScreen;
std::chrono::steady_clock::time_point cellsAt{};
bool logged = false;

// Every shown slot under `control`: a small square control that sits in a collection's cell, as the game says
// (`findAllCollectionIndices`); its own children, parts of the same slot, are not looked at again.
void walk(UIControl& control, int depth, std::vector<Cell>& out) {
    if (depth > 20 || out.size() > 400) return;
    auto const& children = *control.mChildren;
    if (children.size() > 600) return; // not a child list
    for (auto const& child : children) {
        if (!child || !(static_cast<int>(child->mVisible) & 1)) continue;
        if (!child->mCachedPositionDirty) {
            glm::vec2 const at = *child->mCachedPosition, size = *child->mSize;
            if (size.x >= 14 && size.x <= 24 && std::abs(size.x - size.y) < 2) {
                std::string collection;
                int index = -1;
                child->findAllCollectionIndices([&](std::string const& name, int i) {
                    if (index < 0) collection = name, index = i;
                });
                if (index >= 0 && !collection.empty()) {
                    out.push_back({at.x, at.y, size.x, size.y, std::move(collection), index});
                    continue;
                }
            }
        }
        walk(*child, depth + 1, out);
    }
}
}
void forget() { cells.clear(), cellsScreen.clear(), cellsAt = {}; }
void draw(MinecraftUIRenderContext& context, ScreenView& screen, std::function<bool(ItemStack const&)> const& matches) {
    if (!versionSensitiveAllowed(capabilities::Id::SearchHighlight)) return;
    try {
        if (!screen.mController || !screen.mController->_isContainerScreen()) return;
        auto manager = static_cast<ContainerScreenController&>(*screen.mController).mContainerManagerController;
        if (!manager || manager->mContainersClosed) return;
        std::string const name = screen.getScreenName();
        auto const now = std::chrono::steady_clock::now();
        if (name != cellsScreen || now - cellsAt > std::chrono::milliseconds(500)) {
            cells.clear();
            if (auto* tree = screen.mVisualTree.get()) {
                std::shared_ptr<UIControl> const& root = tree->mRootControl;
                if (root) walk(*root, 0, cells);
            }
            // Only cells of a container the screen's manager knows: the inventory, a chest, a grid.
            std::erase_if(cells, [&](Cell const& c) { return c.index >= manager->getContainerSize(c.collection); });
            if (!logged && !cells.empty()) {
                logged = true;
                Runtime::instance().self().getLogger().info("Search highlight: {} slots on '{}', first in '{}'", cells.size(), name, cells.front().collection);
            }
            cellsScreen = name, cellsAt = now;
        }
        for (auto const& cell : cells) {
            auto const& stack = manager->getItemStack(cell.collection, cell.index);
            if (!stack.isNull() && matches(stack)) continue;
            ui::fill(context, cell.x, cell.y, cell.w, cell.h, ui::palette::shade, .55f);
        }
    } catch (...) { cells.clear(); }
}
}

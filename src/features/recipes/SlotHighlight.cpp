#include "features/recipes/SlotHighlight.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "features/recipes/ItemKey.h"
#include "ui/Widgets.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/item/ItemStack.h"
#include <chrono>
#include <format>
#include <set>
#include <string>
#include <vector>

namespace lamium::recipes::slotHighlight {
namespace {
struct Cell {
    float x = 0, y = 0, w = 0, h = 0;
    std::string collection;
    int index = 0;
    std::string seen; // the item last matched there, by key
    bool matched = false;
};
std::vector<Cell> cells;
std::string cellsScreen, cellsQuery;
float cellsX = 0, cellsY = 0; // where the window rested when they were found
std::chrono::steady_clock::time_point cellsAt{};
bool logged = false;

// Every shown slot under `control`: a small square control that sits in a collection's cell, as the game says;
// its own children, parts of the same slot, are not looked at again.
void walk(UIControl& control, int depth, std::vector<Cell>& out) {
    if (depth > 20 || out.size() > 400) return;
    auto const& children = *control.mChildren;
    if (children.size() > 600) return; // not a child list
    for (auto const& child : children) {
        if (!child || !(static_cast<int>(child->mVisible) & 1)) continue;
        if (!child->mCachedPositionDirty) {
            glm::vec2 const at = *child->mCachedPosition, size = *child->mSize;
            if (size.x >= 14 && size.x <= 24 && std::abs(size.x - size.y) < 2) {
                // The collection's name from `findAllCollectionIndices`; its index from `findCollectionIndex`: the
                // index the first hands its callback is not the cell's (slot_trace, 2026-11-21).
                std::string collection;
                child->findAllCollectionIndices([&](std::string const& name, int) {
                    if (collection.empty()) collection = name;
                });
                int const index = collection.empty() ? -1 : child->findCollectionIndex(collection);
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
#ifdef LAMIUM_SLOT_TRACE
// slot_trace: once per screen, every shown control down the tree with its size, and the cells the game names
// for it, so the slot controls can be told from the rest (L-378).
std::set<std::string> traced;
void trace(UIControl& control, int depth, int& budget) {
    if (depth > 24 || budget <= 0) return;
    auto const& children = *control.mChildren;
    if (children.size() > 600) return;
    for (auto const& child : children) {
        if (!child || budget <= 0) continue;
        glm::vec2 const at = *child->mCachedPosition, size = *child->mSize;
        std::string cells;
        child->findAllCollectionIndices([&](std::string const& name, int i) { if (cells.size() < 120) cells += std::format(" {}[{}]", name, i); });
        int const inv = child->findCollectionIndex("inventory_items");
        if (size.x > 0 && size.y > 0 && (size.x < 60 || !cells.empty() || depth < 6)) {
            --budget;
            Runtime::instance().self().getLogger().info("slot_trace {}{} '{}' vis={} dirty={} at {:.0f},{:.0f} size {:.0f}x{:.0f} inv={}{}",
                std::string(static_cast<size_t>(depth), ' '), depth, *child->mName, static_cast<int>(child->mVisible),
                static_cast<int>(child->mCachedPositionDirty), at.x, at.y, size.x, size.y, inv, cells);
        }
        trace(*child, depth + 1, budget);
    }
}
#endif
void forget() { cells.clear(), cellsScreen.clear(), cellsAt = {}, cellsQuery.clear(); }
void draw(MinecraftUIRenderContext& context, ScreenView& screen, bool settled, float windowX, float windowY, std::string const& query,
          std::function<bool(ItemStack const&)> const& matches) {
    if (!versionSensitiveAllowed(capabilities::Id::SearchHighlight)) return;
    // While the window slides in or out its slots are on their way: nothing is drawn, and they are looked for
    // once it rests (a walk of the screen's controls is not for every frame).
    if (!settled) { cells.clear(), cellsScreen.clear(); return; }
    try {
        if (!screen.mController || !screen.mController->_isContainerScreen()) return;
        auto manager = static_cast<ContainerScreenController&>(*screen.mController).mContainerManagerController;
        if (!manager || manager->mContainersClosed) return;
        std::string const name = screen.getScreenName();
        if (name != cellsScreen || windowX != cellsX || windowY != cellsY) {
            cells.clear();
#ifdef LAMIUM_SLOT_TRACE
            if (auto* tree = screen.mVisualTree.get(); tree && traced.insert(name).second) {
                std::shared_ptr<UIControl> const& root = tree->mRootControl;
                int budget = 600;
                if (root) trace(*root, 0, budget);
            }
#endif
            if (auto* tree = screen.mVisualTree.get()) {
                std::shared_ptr<UIControl> const& root = tree->mRootControl;
                if (root) walk(*root, 0, cells);
            }
            // Only where items are kept (the inventory, the hotbar, a container's), in cells the screen's manager
            // knows: a crafting grid, its output, armor or a furnace's slots are not searched.
            std::erase_if(cells, [&](Cell const& c) {
                bool const storage = c.collection.find("inventory") != std::string::npos || c.collection.find("hotbar") != std::string::npos
                    || c.collection.find("container") != std::string::npos;
                return !storage || c.index >= manager->getContainerSize(c.collection);
            });
            if (!logged && !cells.empty()) {
                logged = true;
                Runtime::instance().self().getLogger().info("Search highlight: {} slots on '{}', first in '{}'", cells.size(), name, cells.front().collection);
            }
            cellsScreen = name, cellsX = windowX, cellsY = windowY;
        }
        // A slot is matched again only when what lies there or the search changed.
        if (query != cellsQuery) {
            for (auto& cell : cells) cell.seen.clear();
            cellsQuery = query;
        }
        for (auto& cell : cells) {
            auto const& stack = manager->getItemStack(cell.collection, cell.index);
            std::string key = stack.isNull() ? std::string() : itemKey(stack.getTypeName(), stack.getAuxValue());
            if (key != cell.seen || key.empty()) cell.matched = !key.empty() && matches(stack), cell.seen = std::move(key);
            if (cell.matched) continue;
            ui::fill(context, cell.x, cell.y, cell.w, cell.h, ui::palette::shade, .55f);
        }
    } catch (...) { cells.clear(); }
}
}

#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace lamium::information::inventoryHud {
// The inventory grid and used-slot counter (BACKLOG L-127,
// docs/demos/inventory-hud.html). Pure parts; InfoHud draws.
inline constexpr int slotCount = 36;      // Player inventory: 0-8 hotbar, 9-35 main.
inline constexpr float slotSize = 18;     // One slot, as the hotbar at 100 %.
inline constexpr float dividerGap = 4;    // Between the main rows and the hotbar row.

// Container slots row by row: the main 27 as the inventory screen lays them
// out (9-17, 18-26, 27-35), then the hotbar as a fourth row when included.
inline std::vector<std::vector<int>> gridRows(bool hotbar) {
    std::vector<std::vector<int>> rows;
    for (int row = 0; row < 3; ++row) {
        rows.emplace_back();
        for (int column = 0; column < 9; ++column) rows.back().push_back(9 + row * 9 + column);
    }
    if (hotbar) {
        rows.emplace_back();
        for (int column = 0; column < 9; ++column) rows.back().push_back(column);
    }
    return rows;
}
// Occupied slots and the size of the counted range: the main 27, or all 36.
struct Usage { int used = 0, total = 0; };
inline Usage usedSlots(std::array<bool, slotCount> const& occupied, bool hotbar) {
    Usage usage;
    for (int slot = hotbar ? 0 : 9; slot < slotCount; ++slot) {
        ++usage.total;
        usage.used += occupied[static_cast<size_t>(slot)];
    }
    return usage;
}
// The top of each row inside the grid, in slots of `cell` units; the hotbar
// row sits one gap further down.
inline float rowTop(int row, float cell) { return row * cell + (row >= 3 ? dividerGap * cell / slotSize : 0); }
inline float gridHeight(bool hotbar, float cell) { return hotbar ? rowTop(3, cell) + cell : 3 * cell; }
}

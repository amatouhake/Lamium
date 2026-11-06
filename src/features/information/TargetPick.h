#pragma once
#include "features/schematic/RayCells.h"
#include <algorithm>
#include <optional>

// Which block the target card names (BACKLOG L-217). The game's own clip can
// name a cell that is not the face the ray reaches first (the block above the
// aimed one), so each cell the ray passes is clipped against the whole ray
// and the nearest hit wins. Pure; the card supplies the per-cell clip.
namespace lamium::information {
struct CellHit {
    schematic::Point cell;
    double distance = 0;
};
// The nearest hit among `game` (the game's pick, if any) and the cells along
// the ray. `clipCell(cell)` is the distance at which that cell's block meets
// the ray, or nothing (air, liquid, a miss); `stop(cell)` ends the walk (a
// cell that is not loaded). A shape may reach a little past its own cell
// (fences, walls), so the walk goes one block past the game's hit. A cell of
// the walk at about the game's distance wins: the distance agrees, the cell
// is checked.
template <class Clip, class Stop>
std::optional<CellHit> nearestCellHit(double x, double y, double z, double dx, double dy, double dz, double reach,
                                      std::optional<CellHit> game, Clip&& clipCell, Stop&& stop) {
    constexpr double same = 1e-3;
    std::optional<CellHit> walked;
    double limit = game ? std::min(reach, game->distance + 1) : reach;
    for (auto const& cell : schematic::CellWalk(x, y, z, dx, dy, dz, limit)) {
        if (stop(cell)) break;
        auto distance = clipCell(cell);
        if (!distance || *distance > reach) continue;
        if (!walked || *distance < walked->distance) walked = CellHit{cell, *distance};
    }
    if (walked && (!game || walked->distance <= game->distance + same)) return walked;
    return game;
}
}

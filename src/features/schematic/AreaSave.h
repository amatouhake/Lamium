#pragma once
// Saving an area (save() in GhostRenderer.h): the render thread reads the
// world's blocks in bounded steps and writes the file when done. The job
// belongs to the render thread; requests and status cross under a mutex.
#include "features/schematic/SaveArea.h"
#include <vector>
class BlockSource;
class LocalPlayer;
namespace lamium::schematic::ghosts {
void stepSave(BlockSource& region, LocalPlayer& player);
// A save in progress or waiting ends when the world is left, the feature is
// off or the player stops it.
void stopSave();
// While a save waits: the chunk columns it still has to read, nearest to
// (x, z) first, at most `limit`, standing on the area's floor.
struct WaitingColumns {
    int lowY = 0, height = 0;
    std::vector<Column> nearest;
};
WaitingColumns waitingColumns(double x, double z, size_t limit);
}

#pragma once
#include "ui/Widgets.h"
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
class MinecraftUIRenderContext;
// The map picture on the Saved data page (BACKLOG L-343): the Overworld
// surface around the region written last, read from the data's saved map a
// region per frame and shown as it fills in.
namespace lamium::ui::data_thumb {
// The data to picture from now on; the same key again changes nothing.
void show(std::filesystem::path const& mapFolder, std::string const& key);
// Reads one more region if any is left and draws the picture. False when this data has no Overworld map.
bool draw(MinecraftUIRenderContext&, ImageRect rect);
// Where a block lies in the picture, 0-1 both ways; none outside it or without a picture.
std::optional<std::pair<float, float>> at(int x, int z);
// Lets the texture go (the page closed).
void reset();
}

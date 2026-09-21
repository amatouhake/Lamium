#pragma once
#include "settings/Settings.h"
#include <filesystem>
#include <string_view>

namespace lamium {
Settings decodeSettings(std::string_view text);
Settings readSettings(std::filesystem::path const& path);
// Writes a complete sibling temporary file before replacing the destination.
// On error the existing file stays intact and an exception describes the error.
void writeSettings(std::filesystem::path const& path, Settings const& settings);
}

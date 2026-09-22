#pragma once
#include <format>
#include <string>
#include <string_view>

namespace lamium::ui {
std::string translated(std::string_view key);
template<class... Args>
std::string translated(std::string_view key, Args&&... args) {
    return std::vformat(translated(key), std::make_format_args(args...));
}
bool startLocalization();
void stopLocalization();
}

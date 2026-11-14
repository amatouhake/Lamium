#pragma once
#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include "ui/TextEdit.h"

namespace lamium::ui {
// A small decimal editor shared by settings and future tool screens. Editing
// text is separate from applied values: incomplete input never becomes zero.
class NumberInput : public TextEdit {
    bool accepts(std::string const& next) const override {
        if (next.size() > limit || next.find_first_not_of("0123456789.-") != std::string::npos) return false;
        auto minus = next.find('-');
        if (minus != std::string::npos && (minus != 0 || next.find('-', 1) != std::string::npos)) return false;
        auto dot = next.find('.');
        return dot == std::string::npos || next.find('.', dot + 1) == std::string::npos;
    }
    std::string origin;
public:
    NumberInput() : TextEdit(24, true) {}
    void beginPrecise(double value) {
        char buffer[64];
        auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed);
        origin.assign(buffer, result.ptr);
        set(origin);
        replace = true;
    }
    void begin(float value) {
        char buffer[32];
        auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        origin.assign(buffer, result.ptr);
        set(origin);
        replace = true;
    }
    // Typed numbers apply as they are typed; Esc puts the starting text back
    // so the next apply writes the value from before (exact: shortest round trip).
    void restore() { set(origin); }
    std::optional<float> parsed(float minimum, float maximum) const {
        if (text.empty() || text.back() == '.') return {};
        float result = 0;
        auto conversion = std::from_chars(text.data(), text.data()+text.size(), result);
        if (conversion.ec != std::errc{} || conversion.ptr != text.data()+text.size()
            || !std::isfinite(result) || result < minimum || result > maximum) return {};
        return result;
    }
    std::optional<double> parsedPrecise(double minimum, double maximum, bool integer = false) const {
        if (text.empty() || text.back() == '.') return {};
        double result = 0;
        auto conversion = std::from_chars(text.data(), text.data()+text.size(), result);
        if (conversion.ec != std::errc{} || conversion.ptr != text.data()+text.size()
            || !std::isfinite(result) || result < minimum || result > maximum
            || (integer && std::trunc(result) != result)) return {};
        return result;
    }
};
}

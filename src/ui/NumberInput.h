#pragma once
#include <algorithm>
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
    // A key of a whole number being typed: the digit row, the number pad, Backspace. False for any other.
    bool digitKey(int code) {
        if (code >= 0x30 && code <= 0x39) append(std::string(1, static_cast<char>(code)));
        else if (code >= 0x60 && code <= 0x69) append(std::string(1, static_cast<char>('0' + code - 0x60)));
        else if (code == 0x08) backspace();
        else return false;
        return true;
    }
    // Typed numbers apply as they are typed; Esc puts the starting text back
    // so the next apply writes the value from before (exact: shortest round trip).
    void restore() { set(origin); }
    // While typing, a value out of range is not a value. When the edit is
    // kept (`clampOut`, set around the commit), it becomes the nearest end.
    bool clampOut = false;
    std::optional<float> parsed(float minimum, float maximum) const {
        if (text.empty() || text.back() == '.') return {};
        float result = 0;
        auto conversion = std::from_chars(text.data(), text.data()+text.size(), result);
        if (conversion.ec != std::errc{} || conversion.ptr != text.data()+text.size() || !std::isfinite(result)) return {};
        if (result < minimum || result > maximum) {
            if (!clampOut) return {};
            result = std::clamp(result, minimum, maximum);
        }
        return result;
    }
    std::optional<double> parsedPrecise(double minimum, double maximum, bool integer = false) const {
        if (text.empty() || text.back() == '.') return {};
        double result = 0;
        auto conversion = std::from_chars(text.data(), text.data()+text.size(), result);
        if (conversion.ec != std::errc{} || conversion.ptr != text.data()+text.size() || !std::isfinite(result)
            || (integer && std::trunc(result) != result)) return {};
        if (result < minimum || result > maximum) {
            if (!clampOut) return {};
            result = std::clamp(result, minimum, maximum);
        }
        return result;
    }
    // The value as kept: out of range becomes the nearest end.
    std::optional<double> committedPrecise(double minimum, double maximum, bool integer = false) {
        clampOut = true;
        auto value = parsedPrecise(minimum, maximum, integer);
        clampOut = false;
        return value;
    }
};
}

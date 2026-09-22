#pragma once
#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

namespace lamium::ui {
// A small decimal editor shared by settings and future tool screens. Editing
// text is separate from applied values: incomplete input never becomes zero.
class NumberInput {
    std::string text;
    bool replace = true;
public:
    void beginPrecise(double value) {
        char buffer[64];
        auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed);
        text.assign(buffer, result.ptr);
        replace = true;
    }
    void begin(float value) {
        char buffer[32];
        auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        text.assign(buffer, result.ptr);
        replace = true;
    }
    std::string const& value() const { return text; }
    bool selectedAll() const { return replace; }
    void selectAll() { replace = true; }
    bool append(std::string_view input) {
        if (input.empty() || input.find_first_not_of("0123456789.-") != std::string_view::npos) return false;
        auto next = replace ? std::string(input) : text + std::string(input);
        if (next.size() > 24) return false;
        auto minus = next.find('-');
        if (minus != std::string::npos && (minus != 0 || next.find('-', 1) != std::string::npos)) return false;
        auto dot = next.find('.');
        if (dot != std::string::npos && next.find('.', dot+1) != std::string::npos) return false;
        text = std::move(next); replace = false;
        return true;
    }
    bool backspace() {
        if (text.empty()) return false;
        if (replace) text.clear(); else text.pop_back();
        replace = false;
        return true;
    }
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

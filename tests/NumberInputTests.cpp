#include "ui/NumberInput.h"
#include "settings/Options.h"
void check(bool, char const*);
void numberInputTests() {
    lamium::ui::NumberInput editor;
    editor.begin(3);
    check(editor.selectedAll() && editor.parsed(1,10) == 3, "number editor selects initial value");
    check(editor.append("2") && editor.parsed(1,10) == 2, "first input replaces initial value");
    check(editor.append(".") && !editor.parsed(1,10), "unfinished decimal does not apply");
    check(editor.append("75") && editor.parsed(1,10) == 2.75f, "fractional input preserves precision");
    check(!editor.append(".") && !editor.append("x") && editor.parsed(1,10) == 2.75f,
          "invalid characters do not alter number text");
    editor.selectAll(); editor.append("11");
    check(!editor.parsed(1,10), "out of range input is not clamped into a different setting");
    editor.selectAll(); editor.backspace();
    check(editor.value().empty() && !editor.parsed(1,10), "clearing selection does not write zero");
    editor.append("-");
    check(!editor.parsed(-10,10), "unfinished sign does not apply");
    editor.append("2.5");
    check(editor.parsed(-10,10) == -2.5f, "reusable editor supports signed values");
    check(!editor.append(std::string(30,'1')), "number input has a bounded length");
    editor.begin(.1f);
    check(editor.parsed(.1f,2) == .1f, "lower inclusive float bound round trips");
    lamium::Settings preferences;
    for (auto const& option : lamium::settings::options) {
        if (!option.numeric) continue;
        auto range = *option.numeric;
        editor.begin((range.minimum+range.maximum)/2);
        auto parsed = editor.parsed(range.minimum,range.maximum);
        check(parsed.has_value(), "numeric catalog range accepts its midpoint");
        range.write(preferences,*parsed);
        preferences.normalize();
        check(std::get<float>(option.read(preferences)) == *parsed, "numeric editor writes survive settings normalization");
    }
}

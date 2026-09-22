#include "ui/Translations.h"
#include <format>
#include <stdexcept>
#include <string>
#include <unordered_set>

void translationTests() {
    using namespace lamium::ui::translations;
    auto check = [](bool ok) { if (!ok) throw std::runtime_error("translation catalog invariant"); };
    std::unordered_set<std::string_view> keys;
    for (auto const& entry : entries) {
        check(keys.insert(entry.key).second);
        check(!entry.english.empty() && !entry.japanese.empty());
        check(find(entry.key, "de_DE") == entry.english);
        check(find(entry.key, "ja-JP") == entry.japanese);
        // Validate dynamic format strings with the same argument types used by
        // the UI. A malformed translation must not crash the render callback.
        for (auto locale : {"en_US", "ja_JP"}) {
            std::string key = "F8", zoom = "C", light = "J", on = "On";
            float number = 3.5f;
            auto pattern = find(entry.key, locale);
            std::string rendered;
            if (entry.key == "gameplay") rendered = std::vformat(pattern, std::make_format_args(key, zoom, light));
            else if (entry.key == "magnification" || entry.key == "wheelStep")
                rendered = std::vformat(pattern, std::make_format_args(number));
            else if (pattern.find("{}") != std::string_view::npos)
                rendered = std::vformat(pattern, std::make_format_args(on));
            else rendered = std::vformat(pattern, std::make_format_args());
            check(!rendered.empty());
        }
    }
    check(japanese("ja") && japanese("ja_JP") && !japanese("jargon"));
    check(find("key.jump", "ja_JP").empty());
}

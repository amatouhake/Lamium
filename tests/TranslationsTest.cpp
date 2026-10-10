#include "ui/Translations.h"
#include <format>
#include <stdexcept>
#include <string>
#include <unordered_set>

void translationTests() {
    using namespace lamium::ui::translations;
    auto check = [](bool ok) { if (!ok) throw std::runtime_error("translation catalog invariant"); };
    std::unordered_set<std::string_view> keys;
    check(alignedWithEntries(simplifiedChinese));
    for (std::size_t i = 0; i < entries.size(); ++i) {
        auto const& entry = entries[i];
        check(find(entry.key, "zh_CN") == simplifiedChinese[i].text);
        check(keys.insert(entry.key).second);
        for (auto locale : locales) check(!find(entry.key, locale).empty());
        check(find(entry.key, "de_DE") == entry.english);
        check(find(entry.key, "zh_TW") == entry.english);
        check(find(entry.key, "ja-JP") == entry.japanese);
        // Validate dynamic format strings with the same argument types used by
        // the UI. A malformed translation must not crash the render callback.
        for (auto locale : {"en_US", "ja_JP", "zh_CN"}) {
            std::string key = "F8", zoom = "C", light = "J", on = "On";
            float number = 3.5f;
            int remaining = 123, maximum = 1561;
            auto pattern = find(entry.key, locale);
            std::string rendered;
            if (entry.key == "shape.entry") rendered = std::vformat(pattern, std::make_format_args(key,on,remaining));
            else if (entry.key == "hudXYZ") rendered = std::vformat(pattern, std::make_format_args(number,number,number));
            else if (entry.key == "hudScaledCoordinates") rendered = std::vformat(pattern, std::make_format_args(key,number,number,number));
            else if (entry.key == "targetBlockPosition" || entry.key == "worldMap.teleported") rendered = std::vformat(pattern, std::make_format_args(remaining,remaining,remaining));
            else if (entry.key == "bindingRow" || entry.key == "numberInput") rendered = std::vformat(pattern, std::make_format_args(key, zoom));
            else if (entry.key == "numberRange" || entry.key == "integerRange" || entry.key == "numberControl") rendered = std::vformat(pattern, std::make_format_args(number, number));
            else if (entry.key == "hudLightValues") rendered = std::vformat(pattern, std::make_format_args(remaining,maximum));
            else if (entry.key == "hudBlock") rendered = std::vformat(pattern, std::make_format_args(remaining,remaining,remaining));
            else if (entry.key == "hudTime") rendered = std::vformat(pattern, std::make_format_args(remaining,key));
            else if (entry.key == "mouseButton") rendered = std::vformat(pattern, std::make_format_args(remaining));
            else if (entry.key == "autoInterval" || entry.key == "autoClicks")
                rendered = std::vformat(pattern, std::make_format_args(number, number));
            else if (entry.key == "hudEditor.anchorReadout") rendered = std::vformat(pattern, std::make_format_args(key, key));
            else if (entry.key == "worldMap.layer" || entry.key == "schematic.layerValue" || entry.key == "schematic.summary.correct"
                || entry.key == "schematic.materials.left")
                rendered = std::vformat(pattern, std::make_format_args(remaining, remaining));
            else if (entry.key == "debugEntityKinds.line")
                rendered = std::vformat(pattern, std::make_format_args(remaining, remaining, remaining, remaining, remaining));
            else if (entry.key == "debugEntityTypes.more") rendered = std::vformat(pattern, std::make_format_args(remaining, remaining));
            else if (entry.key == "schematic.size") rendered = std::vformat(pattern, std::make_format_args(remaining, remaining, remaining));
            else if (entry.key == "schematic.toast.moved")
                rendered = std::vformat(pattern, std::make_format_args(key, remaining, remaining, remaining));
            else if (entry.key == "schematic.toast.layer") rendered = std::vformat(pattern, std::make_format_args(key, remaining, remaining));
            else if (entry.key == "schematic.previewLayers") rendered = std::vformat(pattern, std::make_format_args(remaining, remaining, key));
            else if (entry.key == "schematic.toast.rotated" || entry.key == "schematic.toast.mirror" || entry.key == "schematic.toast.nearest")
                rendered = std::vformat(pattern, std::make_format_args(key, remaining));
            else if (entry.key == "schematic.toast.corner" || entry.key == "schematic.save.size")
                rendered = std::vformat(pattern, std::make_format_args(remaining, remaining, remaining, remaining));
            else if (entry.key == "schematic.toast.cornerArea")
                rendered = std::vformat(pattern, std::make_format_args(remaining, remaining, remaining, remaining, remaining, remaining, remaining));
            else if (entry.key == "schematic.save.done" || entry.key == "schematic.save.gameRejected")
                rendered = std::vformat(pattern, std::make_format_args(key, remaining, remaining, remaining));
            else if (entry.key == "schematic.save.waiting")
                rendered = std::vformat(pattern, std::make_format_args(key, remaining, key, remaining));
            else if (entry.key == "schematic.save.progress" || entry.key == "schematic.save.progressWaiting")
                rendered = std::vformat(pattern, std::make_format_args(key, remaining));
            else if (entry.key == "schematic.save.tooLarge") rendered = std::vformat(pattern, std::make_format_args(remaining, remaining));
            else if (entry.key == "schematic.withDetail") rendered = std::vformat(pattern, std::make_format_args(key, key));
            else if (entry.key == "durabilityValue") {
                rendered = std::vformat(pattern, std::make_format_args(remaining, maximum));
                check(rendered.find("123 / 1561") != std::string::npos);
            }
            else if (entry.key == "shape.x" || entry.key == "shape.y" || entry.key == "shape.z"
                || entry.key == "magnification" || entry.key == "hitboxDistance" || entry.key == "freeCameraSpeed"
                || entry.key == "mapSize" || entry.key == "mapWaypointDistance" || entry.key == "schematicOutlineDistance")
                rendered = std::vformat(pattern, std::make_format_args(number));
            else if (pattern.find("{}") != std::string_view::npos)
                rendered = std::vformat(pattern, std::make_format_args(on));
            else rendered = std::vformat(pattern, std::make_format_args());
            check(!rendered.empty());
        }
    }
    check(japanese("ja") && japanese("ja_JP") && !japanese("jargon"));
    check(localeFor("zh_CN") == Locale::SimplifiedChinese && localeFor("zh-Hans-CN") == Locale::SimplifiedChinese
          && localeFor("zh_TW") == Locale::English && localeFor("zh") == Locale::English);
    check(find("biome.beach.name", "ja_JP") == "ビーチ"
          && find("biome.plains.name", "en_US") == "Plains");
    check(find("biome.plains.name", "zh_CN") == "平原");
    check(find("key.jump", "ja_JP").empty() && find("key.jump", "zh_CN").empty());
}

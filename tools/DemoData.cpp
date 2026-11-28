// Writes docs/demos/kit/lamium-data.js: what the design mockups draw from, taken from the
// same headers the game build uses (palette, sizes, the settings screen's rows, every string),
// so a mockup cannot drift from the real screen. CI regenerates it and fails on a difference.
// Usage: LamiumDemoData [output path]  (default docs/demos/kit/lamium-data.js, run from the repository)
#include "input/Actions.h"
#include "input/Binding.h"
#include "input/FixedPresses.h"
#include "settings/Options.h"
#include "settings/Settings.h"
#include "ui/ListViewWidgets.h"
#include "ui/SettingsNavigation.h"
#include "ui/SettingsRows.h"
#include "ui/SettingsTable.h"
#include "ui/Translations.h"
#include "ui/Widgets.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstdio>
#include <format>
#include <fstream>
#include <iostream>

using namespace lamium;
using json = nlohmann::ordered_json;

namespace {
constexpr std::string_view locales[] = {"en_US", "ja_JP", "zh_CN", "zh_TW"};

std::string hex(ui::Rgb c) {
    auto byte = [](float v) { return static_cast<int>(std::lround(std::clamp(v, 0.f, 1.f) * 255)); };
    return std::format("#{:02x}{:02x}{:02x}", byte(c.r), byte(c.g), byte(c.b));
}
// The same text in every locale, in `locales` order.
template<class Text>
json each(Text text) {
    json out = json::array();
    for (auto locale : locales)
        out.push_back(text([locale](std::string_view key) { return std::string(ui::translations::find(key, locale)); }));
    return out;
}
json chord(input::Chord const& keys) {
    json out = json::array();
    for (auto token : keys) {
        char const* device = token.device == input::Device::Key ? "key" : token.device == input::Device::Mouse ? "mouse" : "wheel";
        out.push_back({{"device", device}, {"code", token.code}});
    }
    return out;
}
json palette() {
    using namespace ui::palette;
#define TOKEN(name) {#name, hex(name)}
    json lamium = {TOKEN(panel), TOKEN(white), TOKEN(text), TOKEN(dim), TOKEN(faint), TOKEN(accent), TOKEN(accentDeep), TOKEN(off),
        TOKEN(keyFill), TOKEN(keyEdge), TOKEN(experimental), TOKEN(warning), TOKEN(danger), TOKEN(dangerEdge), TOKEN(dangerText),
        TOKEN(info), TOKEN(knobOn), TOKEN(knobOff), TOKEN(shade), TOKEN(mismatch), TOKEN(wrongState), TOKEN(notPlaced), TOKEN(ghost),
        TOKEN(previewMismatch), TOKEN(previewWrongState), TOKEN(previewNotPlaced), TOKEN(previewDim),
        TOKEN(cornerOne), TOKEN(cornerTwo), TOKEN(controlEdge), TOKEN(gripOn), TOKEN(gripOff), TOKEN(syntaxOperator),
        TOKEN(syntaxUnit), TOKEN(syntaxVariable), TOKEN(syntaxFunction), TOKEN(shapeCyan), TOKEN(shapePink), TOKEN(shapeYellow),
        TOKEN(shapeWhite), TOKEN(mapGround)};
    using namespace ui::palette::vanilla;
    json game = {TOKEN(heart), TOKEN(heartEmpty), TOKEN(armor), TOKEN(curse), TOKEN(window), TOKEN(windowInk), TOKEN(windowFaint),
        TOKEN(windowEdge), TOKEN(windowLight), TOKEN(windowShadow), TOKEN(tooltipFill), TOKEN(tooltipLine), TOKEN(buttonShadow),
        TOKEN(switchTrack), TOKEN(tabBack), TOKEN(tabRim), {"controlEdge", hex(ui::palette::vanilla::controlEdge)}, TOKEN(brewingFuel),
        TOKEN(brewingFuelLight), TOKEN(windowSunk), TOKEN(closeHover), TOKEN(closeMark), TOKEN(slot), TOKEN(slotDark),
        TOKEN(slotLight), TOKEN(slotMissing)};
#undef TOKEN
    return {{"lamium", lamium}, {"vanilla", game}};
}
json sizes() {
    using T = ui::SettingsTable;
    // The settings panel at its largest (640 x 380): the layout every mockup of it starts from.
    auto t = T::fit(T::minWidth + T::margin + 400, 380 + T::marginY + 40, 0, 0, ui::nav::count);
    auto rel = [&](float x) { return x - t.left; };
    return {
        {"opacity", {{"screen", ui::opacity::screen}, {"floating", ui::opacity::floating}}},
        {"widgets", {{"switchWidth", ui::switchWidth}, {"switchHeight", ui::switchHeight}, {"capHeight", ui::capHeight},
            {"sliderKnobWidth", ui::sliderKnobWidth}, {"sliderKnobHeight", ui::sliderKnobHeight}, {"sliderTrackHeight", ui::sliderTrackHeight},
            {"tabHeight", ui::tabHeight}, {"tabPad", ui::tabPad}, {"rowButtonHeight", ui::rowButtonHeight}, {"tipPad", ui::tipPad}}},
        {"settings", {{"width", t.width}, {"height", t.height}, {"headerHeight", T::headerHeight}, {"theadHeight", T::theadHeight},
            {"rowHeight", T::rowHeight}, {"footerHeight", T::footerHeight}, {"navItemHeight", T::navItemHeight},
            {"sidebarWidth", T::sidebarWidth}, {"pad", T::pad}, {"gap", T::gap}, {"stateWidth", T::stateWidth},
            {"closeWidth", T::closeWidth}, {"descriptionLineHeight", T::descriptionLineHeight}, {"descriptionLines", T::descriptionLines},
            {"searchX", rel(t.searchX)}, {"searchWidth", t.searchWidth}, {"closeX", rel(t.closeX)}, {"nameX", rel(t.nameX)},
            {"stateX", rel(t.stateX)}, {"keyX", rel(t.keyX)}, {"keyWidth", t.keyWidth}, {"stepperX", rel(t.stepperX())},
            {"stepperWidth", t.stepperWidth()}, {"arrowWidth", T::arrowWidth}, {"sliderX", rel(t.sliderX())},
            {"sliderWidth", t.sliderWidth()}, {"sliderValueWidth", T::sliderValueWidth}, {"headActionWidth", T::headActionWidth},
            {"footerButtonWidth", T::footerButtonWidth}, {"footerButtonHeight", T::footerButtonHeight},
            {"pinnedItems", T::pinnedItems}, {"visibleRows", t.visible}}},
    };
}
json navigation() {
    json out = json::array();
    out.push_back({{"key", "nav.all"}});
    for (auto section : ui::sections) out.push_back({{"key", section}});
    for (auto key : {"nav.hotkeys", "nav.shapes", "nav.waypoints", "nav.schematics", "nav.worldMap", "nav.hudLayout"})
        out.push_back({{"key", key}, {"pinned", true}});
    return out;
}
// A fixed press's caps: named inputs (wheel, drag, drop key) as text, keys by code.
json fixedCaps(input::FixedPress const& press) {
    json caps = json::array();
    for (int i = 0; i < input::fixedCapCount(press); ++i) {
        auto const& cap = press.caps[static_cast<size_t>(i)];
        if (!cap.text.empty()) caps.push_back({{"text", each([&](auto tr) { return tr(cap.text); })}});
        else caps.push_back(chord(input::Chord{cap.token})[0]);
    }
    return caps;
}
// One row as the settings table draws it with default settings and default keys.
json row(ui::SettingsRow const& r, Settings const& value) {
    json out;
    switch (r.kind) {
    case ui::RowKind::Section:
        out = {{"kind", "group"}, {"name", each([&](auto tr) { return tr(r.group); })}};
        return out;
    case ui::RowKind::Feature: {
        auto const& f = *r.feature;
        out = {{"kind", "feature"}, {"id", f.id}, {"name", each([&](auto tr) { return tr(f.name); })},
            {"help", each([&](auto tr) { return tr(f.description); })}};
        if (f.experimental) out["experimental"] = true;
        if (r.children) out["children"] = r.children;
        if (auto option = settings::find(f.toggle)) out["control"] = {{"type", "switch"}, {"on", std::get<bool>(option->read(value))}};
        else if (isSessionFeature(f.id)) out["control"] = {{"type", "switch"}, {"on", false}};
        if (auto primary = ui::primaryAction(f)) {
            out["keys"] = chord(input::defaultChord(*primary));
            if (ui::opensTool(*primary)) out["open"] = true;
        }
        return out;
    }
    case ui::RowKind::Option: {
        auto const& o = *r.option;
        auto v = o.read(value);
        out = {{"kind", "option"}, {"id", o.id}, {"name", each([&](auto tr) { return ui::splitLabel(tr(o.label)).name; })}};
        auto help = "help." + std::string(o.id);
        out["help"] = each([&](auto tr) { auto h = tr(help); return h.empty() ? tr(r.feature->description) : h; });
        if (auto flag = std::get_if<bool>(&v)) out["control"] = {{"type", "switch"}, {"on", *flag}};
        else {
            bool slider = o.numeric && o.numeric->step > 0;
            out["control"] = {{"type", slider ? "slider" : "stepper"},
                {"value", each([&](auto tr) { return ui::optionValueText(o, v, tr); })}};
            if (slider) out["control"]["position"] = ui::SettingsTable::sliderPosition(std::get<float>(v), o.numeric->minimum, o.numeric->maximum);
        }
        if (auto linked = ui::optionAction(o.id)) out["keys"] = chord(input::defaultChord(*linked));
        // A fixed gesture's keys beside its switch, read-only (L-372, SettingsTableView).
        else if (auto press = ui::fixedPressOf(o.id)) { out["keys"] = fixedCaps(*press); out["keyTone"] = "fixed"; }
        break;
    }
    case ui::RowKind::Action:
        out = {{"kind", "action"}, {"id", "key." + std::string(input::actions[static_cast<size_t>(*r.action)].id)},
            {"name", each([&](auto tr) { return ui::actionName(tr(ui::actionTranslationKey(*r.action, true))); })},
            {"keys", chord(input::defaultChord(*r.action))}};
        if (ui::opensTool(*r.action)) out["open"] = true;
        break;
    case ui::RowKind::Fixed: {
        auto const& press = input::fixedPresses[static_cast<size_t>(r.press)];
        out = {{"kind", "fixed"}, {"id", std::format("fixed.{}", r.press)}, {"name", each([&](auto tr) {
            if (auto option = settings::find(press.option)) return ui::splitLabel(tr(option->label)).name;
            return tr(press.name);
        })}, {"keyTone", "fixed"}};
        out["keys"] = fixedCaps(press);
        break;
    }
    case ui::RowKind::Layout:
        out = {{"kind", "link"}, {"name", each([&](auto tr) { return tr(ui::layoutLinkLabel(*r.layout)); })},
            {"link", each([](auto tr) { return tr("layoutLinkValue"); })}};
        break;
    case ui::RowKind::BandLayout:
        out = {{"kind", "link"}, {"name", each([](auto tr) { return tr("band.layoutLink"); })},
            {"link", each([](auto tr) { return tr("band.layoutLinkValue"); })}};
        break;
    case ui::RowKind::SavedData:
        out = {{"kind", "link"}, {"name", each([&](auto tr) { return tr(r.feature->id == "savedData" ? "savedData.rowServer" : "savedData.row"); })},
            {"link", each([](auto tr) { return tr("savedData.rowOpen"); })}};
        break;
    case ui::RowKind::MapCache:
        out = {{"kind", "mapCache"}, {"name", each([](auto tr) { return tr("mapCache") + "  " + tr("mapCacheNone"); })}};
        break;
    }
    if (!out.contains("id") && r.feature) out["id"] = std::string(out["kind"].get<std::string>()) + "." + std::string(r.feature->id);
    out["child"] = true;
    if (r.lastChild) out["last"] = true;
    return out;
}
// Every category with every feature open: a mockup folds what it does not show.
json settingsRows() {
    std::set<std::string_view> all;
    for (auto const& f : ui::features) all.insert(f.id);
    Settings const value{};
    auto english = [](std::string_view key) { return std::string(ui::translations::find(key, "en_US")); };
    json out = json::object();
    for (auto section : ui::sections) {
        json rows = json::array();
        for (auto const& r : ui::buildSettingsRows(false, section, ui::SearchQuery{}, all, english)) rows.push_back(row(r, value));
        out[std::string(section)] = rows;
    }
    json hotkeys = json::array();
    for (auto const& r : ui::buildSettingsRows(true, {}, ui::SearchQuery{}, {}, english)) hotkeys.push_back(row(r, value));
    out["nav.hotkeys"] = hotkeys;
    return out;
}
json strings() {
    json out = json::object();
    for (auto const& entry : ui::translations::entries)
        out[std::string(entry.key)] = each([&](auto tr) { return tr(entry.key); });
    return out;
}
}

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cerr << "usage: LamiumDemoData [output.js]\n";
        return 2;
    }
    char const* path = argc == 2 ? argv[1] : "docs/demos/kit/lamium-data.js";
    json data = {{"source", "Generated by tools/DemoData.cpp from src/ (xmake build LamiumDemoData && xmake run LamiumDemoData). Do not edit."},
        {"locales", {"en", "ja", "zh_CN", "zh_TW"}}, {"palette", palette()}, {"sizes", sizes()}, {"navigation", navigation()},
        {"rows", settingsRows()}, {"strings", strings()}};
    // One line per row and per string, so a change in src/ shows as a small diff here.
    std::ofstream file(path, std::ios::binary);
    file << "// " << data["source"].get<std::string>() << "\nwindow.LAMIUM_DATA = {\n";
    bool firstKey = true;
    for (auto const& [key, value] : data.items()) {
        file << (firstKey ? "" : ",\n") << json(key).dump() << ": ";
        firstKey = false;
        if (key == "rows") {
            file << "{";
            bool firstList = true;
            for (auto const& [list, rows] : value.items()) {
                file << (firstList ? "\n " : ",\n ") << json(list).dump() << ": [";
                firstList = false;
                for (size_t i = 0; i < rows.size(); ++i) file << (i ? ",\n  " : "\n  ") << rows[i].dump();
                file << "\n ]";
            }
            file << "\n}";
        } else if (key == "strings") {
            file << "{";
            bool firstString = true;
            for (auto const& [name, text] : value.items()) {
                file << (firstString ? "\n " : ",\n ") << json(name).dump() << ": " << text.dump();
                firstString = false;
            }
            file << "\n}";
        } else {
            file << value.dump(1);
        }
    }
    file << "\n};\n";
    return file ? 0 : 1;
}

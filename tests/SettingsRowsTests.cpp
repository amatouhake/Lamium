#include "ui/SettingsRows.h"
#include "ui/Translations.h"
#include <set>
void check(bool, char const*);
void settingsRowsTests() {
    using namespace lamium;
    auto translate = [](std::string_view key) { return std::string(ui::translations::find(key, "en_US")); };
    ui::SearchQuery query;
    std::set<std::string_view> collapsed;
    auto rows = ui::buildSettingsRows(false, query, collapsed, translate);
    std::set<std::string_view> options, features;
    std::set<input::Action> actions;
    std::string_view parent;
    for (auto const& row : rows) {
        check(row.feature && !translate(row.feature->name).empty() && !translate(row.feature->description).empty(), "feature metadata resolves");
        if (row.heading()) { parent = row.feature->id; check(features.insert(parent).second, "one heading per feature"); }
        else {
            check(row.feature->id == parent, "children stay under their feature");
            if (row.option) check(options.insert(row.option->id).second, "option appears exactly once");
            if (row.action) check(actions.insert(*row.action).second, "binding appears exactly once");
        }
    }
    check(options.size() == settings::options.size() && actions.size() == input::actions.size(), "all settings and actions are reachable");
    for (auto const& feature : ui::features) collapsed.insert(feature.id);
    rows = ui::buildSettingsRows(false, query, collapsed, translate);
    check(rows.size() == ui::features.size(), "collapsed view exposes only feature headers");
    std::set<std::string_view> sections;
    std::string_view section;
    for (auto const& row : rows) {
        auto next = ui::featureSection(row.feature->id);
        check(translate(next) != next, "section name is localized");
        if (next != section) {
            check(sections.insert(next).second, "each section forms one contiguous group");
            section = next;
        }
    }
    check(sections.size() == 5, "all broad sections are represented");
    query.append("Camera & appearance");
    rows = ui::buildSettingsRows(false, query, collapsed, translate);
    check(!rows.empty(), "section search reveals collapsed features");
    for (auto const& row : rows)
        check(ui::featureSection(row.feature->id) == "section.camera", "section search stays in matching group");
    query.clear();
    query.append("magnification");
    rows = ui::buildSettingsRows(false, query, collapsed, translate);
    check(rows.size() == 2 && rows[0].heading() && rows[1].option->id == "camera.magnification", "search reveals matching setting inside collapsed feature");
    query.clear(); query.append("ズーム");
    rows = ui::buildSettingsRows(false, query, collapsed, [](std::string_view key) { return std::string(ui::translations::find(key, "ja_JP")); });
    check(rows.size() == 5 && rows.front().feature->id == "zoom", "Japanese feature search reveals settings and binding");
    query.clear();
    query.append("Shape Manager");
    rows = ui::buildSettingsRows(false, query, collapsed, translate);
    check(rows.size() == 2 && rows[0].heading() && rows[1].tool && !rows[1].heading(),
        "shape manager search exposes its dedicated tool entry while collapsed");
    check(ui::buildSettingsRows(true, query, collapsed, translate).empty(),
        "tool launch rows are not misrepresented as hotkey actions");
    query.clear();
    rows = ui::buildSettingsRows(true, query, collapsed, translate);
    check(rows.size() == input::actions.size(), "Hotkeys includes every action regardless of collapse");
    for (auto const& row : rows) check(row.action && !row.option && !row.heading(), "Hotkeys contains only bindings");
    query.append("not-a-real-setting");
    check(ui::buildSettingsRows(false, query, collapsed, translate).empty(), "unmatched query is empty");
}

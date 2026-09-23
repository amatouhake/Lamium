#include "ui/SettingsRows.h"
#include "ui/Translations.h"
#include <format>
#include <set>
void check(bool, char const*);
void settingsRowsTests() {
    using namespace lamium;
    using ui::RowKind;
    auto translate = [](std::string_view key) { return std::string(ui::translations::find(key, "en_US")); };
    ui::SearchQuery query;
    std::set<std::string_view> expanded;
    for (auto const& feature : ui::features) expanded.insert(feature.id);

    // Fully expanded "All": every setting and binding is reachable exactly once,
    // each feature's toggle is its row state rather than a duplicate child.
    auto rows = ui::buildSettingsRows(false, {}, query, expanded, translate);
    std::set<std::string_view> options, features, sectionsSeen;
    std::set<input::Action> actions;
    ui::FeatureInfo const* parent = nullptr;
    std::string_view section;
    for (size_t i = 0; i < rows.size(); ++i) {
        auto const& row = rows[i];
        if (row.kind == RowKind::Section) {
            check(sectionsSeen.insert(row.section).second, "each section heading appears once");
            check(translate(row.section) != row.section, "section name is localized");
            section = row.section;
            check(i + 1 < rows.size() && rows[i+1].heading(), "a section heading is followed by a feature");
            continue;
        }
        check(row.section == section && ui::featureSection(row.feature->id) == section, "rows stay in their section");
        check(!translate(row.feature->name).empty() && !translate(row.feature->description).empty(), "feature metadata resolves");
        if (row.heading()) {
            parent = row.feature;
            check(features.insert(row.feature->id).second, "one heading per feature");
            if (auto primary = ui::primaryAction(*row.feature)) check(actions.insert(*primary).second, "primary binding shown once");
            if (!row.feature->toggle.empty()) check(options.insert(row.feature->toggle).second, "toggle shown as feature state");
            check(row.expanded == (row.children > 0), "expanded features open when they have content");
            continue;
        }
        check(row.child() && row.feature == parent, "children stay under their feature");
        bool last = i + 1 == rows.size() || !rows[i+1].child() || rows[i+1].feature != parent;
        check(row.lastChild == last, "the last child ends the tree guide");
        if (row.option) check(options.insert(row.option->id).second, "option appears exactly once");
        if (row.action) check(actions.insert(*row.action).second, "binding appears exactly once");
    }
    check(sectionsSeen.size() == ui::sections.size(), "all broad sections are represented");
    check(features.size() == ui::features.size(), "every feature is listed");
    check(options.size() == settings::options.size() && actions.size() == input::actions.size(), "all settings and actions are reachable");

    // Collapsed: only headings and features; child counts remain visible.
    expanded.clear();
    rows = ui::buildSettingsRows(false, {}, query, expanded, translate);
    check(rows.size() == ui::features.size() + ui::sections.size(), "collapsed view exposes only sections and features");
    for (auto const& row : rows) check(!row.child() && !row.expanded, "collapsed features have no visible children");

    // A category shows only its features, without section headings.
    rows = ui::buildSettingsRows(false, "section.camera", query, expanded, translate);
    check(!rows.empty(), "category has features");
    for (auto const& row : rows) check(row.heading() && row.section == "section.camera", "category filters features");

    // Search spans all categories and opens features whose settings match.
    query.append("magnification");
    rows = ui::buildSettingsRows(false, "section.inventory", query, expanded, translate);
    check(rows.size() == 3 && rows[0].kind == RowKind::Section && rows[1].feature->id == "zoom" && rows[1].expanded
        && rows[2].option->id == "camera.magnification" && rows[2].lastChild, "search reveals a matching setting in any category");
    query.clear(); query.append("Camera & view");
    rows = ui::buildSettingsRows(false, {}, query, expanded, translate);
    for (auto const& row : rows) check(row.section == "section.camera", "section search stays in matching group");
    query.clear(); query.append("ズーム");
    rows = ui::buildSettingsRows(false, {}, query, expanded, [](std::string_view key) { return std::string(ui::translations::find(key, "ja_JP")); });
    check(rows.size() >= 2 && rows[1].feature->id == "zoom" && !rows[1].expanded, "Japanese feature search keeps a matched feature collapsed");
    query.clear(); query.append("Shape rendering");
    rows = ui::buildSettingsRows(false, {}, query, expanded, translate);
    check(rows.size() == 2 && rows[1].heading() && rows[1].feature->id == "shapes" && rows[1].children == 1,
        "shape rendering is a feature with its toggle and the open-shapes key");
    check(ui::buildSettingsRows(true, {}, query, expanded, translate).size() == 3, "both shape keys are listed in Hotkeys");
    query.clear(); query.append("not-a-real-setting");
    check(ui::buildSettingsRows(false, {}, query, expanded, translate).empty(), "unmatched query is empty");
    query.clear();

    // Hotkeys lists every action regardless of expansion, grouped by section.
    rows = ui::buildSettingsRows(true, {}, query, expanded, translate);
    size_t actionRows = 0;
    for (auto const& row : rows) {
        check(row.kind == RowKind::Section || (row.kind == RowKind::Action && row.action), "Hotkeys contains only bindings");
        if (row.action) ++actionRows;
    }
    check(actionRows == input::actions.size(), "Hotkeys includes every action");

    // Every option label splits into a column name and a formattable value.
    for (auto locale : {"en_US", "ja_JP"}) {
        for (auto const& option : settings::options) {
            auto parts = ui::splitLabel(ui::translations::find(option.label, locale));
            check(!parts.name.empty() && parts.name.find('{') == std::string::npos, "option name has no placeholder");
            check(parts.value.find('{') != std::string::npos, "option value keeps its placeholder");
            float number = 2.5f;
            check(!std::vformat(parts.value, std::make_format_args(number)).empty(), "value pattern formats");
        }
    }
    check(ui::splitLabel("Magnification: {}x").name == "Magnification" && ui::splitLabel("Magnification: {}x").value == "{}x",
        "label splits at the first separator");
    check(ui::actionName("Lamium: Hold to zoom") == "Hold to zoom" && ui::actionName("Toggle Periodic Use") == "Toggle Periodic Use",
        "action names drop the controls-screen prefix only");
}

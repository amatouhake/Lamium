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
    std::set<ui::HudElementId> layouts;
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
        if (row.layout) check(layouts.insert(*row.layout).second, "each HUD element has one layout link");
        if (row.action) check(actions.insert(*row.action).second, "binding appears exactly once");
        // A keyed option row also carries its action's binding.
        if (row.option)
            if (auto linked = ui::optionAction(row.option->id))
                check(actions.insert(*linked).second && row.feature->id == input::actions[static_cast<size_t>(*linked)].feature,
                      "a keyed option carries its own feature's binding exactly once");
    }
    check(sectionsSeen.size() == ui::sections.size(), "all broad sections are represented");
    check(features.size() == ui::features.size(), "every feature is listed");
    // HUD look options are edited in the layout editor, reached through the links.
    size_t listed = 0;
    for (auto const& option : settings::options) if (!option.id.starts_with("hud.")) ++listed;
    check(options.size() == listed && actions.size() == input::actions.size(), "all settings and actions are reachable");
    check(layouts.size() == 5, "every HUD element is reachable from the settings list");
    {
        Settings changed;
        changed.camera.zoom = false;
        changed.camera.magnification = 20;
        changed.camera.freelookToggle = true;
        changed.hud.magnification.dy = 90;
        changed.inventory.sorting = !Settings{}.inventory.sorting;
        changed.bindings[static_cast<size_t>(input::Action::Zoom)] = input::Chord{};
        ui::resetSection(changed, "section.camera");
        Settings defaults;
        check(changed.camera.zoom == defaults.camera.zoom && changed.camera.magnification == defaults.camera.magnification
            && changed.camera.freelookToggle == defaults.camera.freelookToggle
            && changed.hud.magnification.dy == defaults.hud.magnification.dy,
            "a category reset restores its switches, numbers, choices and HUD placement");
        check(changed.inventory.sorting != defaults.inventory.sorting && changed.bindings[static_cast<size_t>(input::Action::Zoom)].has_value(),
            "a category reset leaves other categories and key bindings alone");
    }

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
    check(rows.size() == 5 && rows[0].kind == RowKind::Section && rows[1].feature->id == "zoom" && rows[1].expanded
        && rows[2].option->id == "camera.magnification" && rows[3].option->id == "camera.showMagnification"
        && rows[4].layout == ui::HudElementId::Magnification && rows[4].lastChild,
        "search reveals matching settings in any category");
    query.clear(); query.append("Camera & view");
    rows = ui::buildSettingsRows(false, {}, query, expanded, translate);
    for (auto const& row : rows) check(row.section == "section.camera", "section search stays in matching group");
    query.clear(); query.append("ズーム");
    rows = ui::buildSettingsRows(false, {}, query, expanded, [](std::string_view key) { return std::string(ui::translations::find(key, "ja_JP")); });
    check(rows.size() >= 2 && rows[1].feature->id == "zoom" && !rows[1].expanded, "Japanese feature search keeps a matched feature collapsed");
    query.clear(); query.append("Shape rendering");
    rows = ui::buildSettingsRows(false, {}, query, expanded, translate);
    check(rows.size() == 2 && rows[1].heading() && rows[1].feature->id == "shapes" && rows[1].children == 0,
        "shape rendering is a feature with only its toggle; the shapes key lives with the settings keys");
    query.clear(); query.append("shapes");
    auto hotkeyRows = ui::buildSettingsRows(true, {}, query, expanded, translate);
    size_t shapeKeys = 0;
    for (auto const& row : hotkeyRows) {
        if (row.action == input::Action::ToggleShapes || row.action == input::Action::OpenShapes) ++shapeKeys;
        else check(!row.action, "shape search lists only shape keys");
    }
    check(shapeKeys == 2, "both shape keys are listed in Hotkeys");
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
    query.clear();
    {
        std::set<std::string_view> open = {"settings", "automationStatus"};
        auto view = ui::buildSettingsRows(false, "section.interface", query, open, translate);
        size_t heading = view.size();
        for (size_t i = 0; i < view.size(); ++i)
            if (view[i].heading() && view[i].feature->id == "settings") heading = i;
        check(heading + 3 < view.size(), "settings feature lists its rows");
        check(view[heading+1].option && view[heading+1].option->id == "interface.toggleToasts",
            "toggle toasts option first");
        std::vector<input::Action> openerKeys;
        for (size_t i = heading + 1; i < view.size() && view[i].child(); ++i)
            if (view[i].action) openerKeys.push_back(*view[i].action);
        check(openerKeys.size() == 3 && openerKeys[0] == input::Action::OpenHotkeys
            && openerKeys[1] == input::Action::OpenShapes && openerKeys[2] == input::Action::OpenHudLayout,
            "settings children follow the sidebar: Hotkeys, Shapes, HUD layout openers");
    }
    {
        auto hotkeys = ui::buildSettingsRows(true, {}, query, expanded, translate);
        size_t section = hotkeys.size();
        for (size_t i = 0; i < hotkeys.size(); ++i)
            if (hotkeys[i].kind == RowKind::Section && hotkeys[i].section == "section.interface") section = i;
        check(section + 4 < hotkeys.size() && hotkeys[section+1].action == input::Action::Settings
            && hotkeys[section+2].action == input::Action::OpenHotkeys
            && hotkeys[section+3].action == input::Action::OpenShapes
            && hotkeys[section+4].action == input::Action::OpenHudLayout,
            "Hotkeys lists the openers in sidebar order");
    }
    {
        // Info line rows follow the user-ordered list, not catalog order.
        std::vector<std::string> order = {"ping", "coordinates"};
        for (auto id : information::defaultLineOrder())
            if (id != "ping" && id != "coordinates") order.emplace_back(id);
        std::set<std::string_view> open = {"infoHud"};
        auto view = ui::buildSettingsRows(false, "section.information", query, open, translate, order);
        std::vector<std::string> seen;
        bool underInfo = false;
        for (auto const& row : view) {
            if (row.heading()) underInfo = row.feature->id == "infoHud";
            else if (underInfo && row.option && row.option->id.starts_with("information."))
                seen.emplace_back(row.option->id.substr(std::string_view{"information."}.size()));
        }
        check(seen == order, "settings rows list info lines in user order");
    }

    // Every option label splits into a column name and a formattable value.
    for (auto locale : {"en_US", "ja_JP"}) {
        for (auto const& option : settings::options) {
            auto parts = ui::splitLabel(ui::translations::find(option.label, locale));
            check(!parts.name.empty() && parts.name.find('{') == std::string::npos, "option name has no placeholder");
            check(parts.value.find('{') != std::string::npos, "option value keeps its placeholder");
            float number = 2.5f;
            check(!std::vformat(parts.value, std::make_format_args(number, number, number)).empty(), "value pattern formats");
        }
    }
    check(ui::splitLabel("Magnification: {}x").name == "Magnification" && ui::splitLabel("Magnification: {}x").value == "{}x",
        "label splits at the first separator");
    check(ui::actionName("Lamium: Hold to zoom") == "Hold to zoom" && ui::actionName("Toggle Periodic Use") == "Toggle Periodic Use",
        "action names drop the controls-screen prefix only");
}

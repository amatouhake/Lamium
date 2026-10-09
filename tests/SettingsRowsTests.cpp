#include "ui/SettingsRows.h"
#include "ui/SettingsNavigation.h"
#include "ui/Translations.h"
#include <format>
#include <set>
void check(bool, char const*);
void settingsRowsTests() {
    using namespace lamium;
    using ui::RowKind;
    {
        ui::SettingsNavigation navigation;
        navigation.select(2);
        navigation.select(7, true);
        navigation.reopenNormal();
        check(navigation.current == 2, "a dedicated screen does not replace the normal settings destination");
        navigation.select(7, true);
        navigation.select(4);
        navigation.reopenNormal();
        check(navigation.current == 4, "manual navigation from a dedicated screen becomes the normal destination");
    }
    auto translate = [](std::string_view key) { return std::string(ui::translations::find(key, "en_US")); };
    check(translate(ui::actionTranslationKey(input::Action::FreeCameraSpeedUp,true)) == "Increase speed"
          && translate(ui::actionTranslationKey(input::Action::FreeCameraSpeedDown,true)) == "Decrease speed"
          && translate(ui::actionTranslationKey(input::Action::FreeCameraSpeedUp)) == "Increase FreeCamera speed",
          "speed actions use concise detail labels and identify FreeCamera in Hotkeys");
    check(ui::translations::find(ui::actionTranslationKey(input::Action::FreeCameraSpeedUp,true),"ja_JP") == "速度を上げる"
          && ui::translations::find(ui::actionTranslationKey(input::Action::FreeCameraSpeedDown,true),"ja_JP") == "速度を下げる",
          "Japanese detail speed action labels are natural commands");
    ui::SearchQuery query;
    std::set<std::string_view> expanded;
    for (auto const& feature : ui::features) expanded.insert(feature.id);
    std::set<std::string_view> const sessions{
        "zoom", "freelook", "freecamera", "permanentSneak", "permanentSprint"};
    for (auto const& feature : ui::features)
        if (auto primary = ui::primaryAction(feature))
            check(input::actions[static_cast<size_t>(*primary)].feature == feature.id
                && (!feature.toggle.empty() || sessions.contains(feature.id) || feature.id == "settings"
                    || feature.id == "caveView" || feature.id == "schematicMenu"), // Named commands: a key without a switch.
                "a parent key belongs to the feature's own state or screen opener");
    for (auto id : {"restrictions", "mapText"}) {
        auto found = std::find_if(ui::features.begin(), ui::features.end(),
            [=](auto const& feature) { return feature.id == id; });
        check(found != ui::features.end() && !ui::primaryAction(*found),
            "a keyless parent does not borrow a child command's binding");
    }
    // SETTINGS-KEYMAP rule 1: every saved switch has a toggle on its parent
    // row; commands such as Sort now or Add a waypoint stay on child rows.
    for (auto const& feature : ui::features) {
        if (feature.toggle.empty()) continue;
        auto primary = ui::primaryAction(feature);
        check(primary && input::actions[static_cast<size_t>(*primary)].behavior == input::Behavior::Toggle,
            "a saved switch is toggled by its parent row's key");
    }

    // Fully expanded "All": every setting and binding is reachable exactly once,
    // each feature's toggle is its row state rather than a duplicate child.
    auto rows = ui::buildSettingsRows(false, {}, query, expanded, translate);
    auto effectGroup = std::find_if(rows.begin(), rows.end(), [](auto const& row) {
        return row.heading() && row.feature->id == "hideEffects";
    });
    check(effectGroup != rows.end() && effectGroup->feature->toggle == "visuals.hideEffects"
          && effectGroup->feature->primary == input::Action::ToggleHideEffects && effectGroup->children == 8,
          "Hide effects has a master switch with its toggle key and eight independent child switches");
    Settings effectSettings;
    check(ui::effectsPaused("hideEffects",effectSettings), "the master effect switch defaults off");
    effectSettings.visuals.hideEffects = true;
    check(!ui::effectsPaused("hideEffects",effectSettings) && !ui::effectsPaused("freecamera",effectSettings),
          "children show no paused state once the master is on");
    effectSettings.visuals.hideEffects = false;
    check(ui::effectsPaused("hideEffects",effectSettings) && !ui::effectsPaused("freecamera",effectSettings),
          "only Hide effects children display the paused state when their master is off");
    for (auto id : {"visuals.hideWeather", "visuals.hideParticles", "visuals.hideBossBars", "visuals.hideNausea",
                    "visuals.hideWater", "visuals.hideLava", "visuals.hidePowderSnow", "visuals.hideDistanceFog"}) {
        auto child = std::find_if(rows.begin(), rows.end(), [=](auto const& row) {
            return row.option && row.option->id == id;
        });
        check(child != rows.end() && child->feature->id == "hideEffects" && ui::optionAction(id).has_value(),
              "each effect has a switch and key on the same child row");
    }
    auto restockSource = std::find_if(rows.begin(), rows.end(), [](auto const& row) {
        return row.option && row.option->id == "inventory.restockFromHotbar";
    });
    check(restockSource != rows.end() && restockSource->child()
          && restockSource->feature->id == "handRestock" && !restockSource->action,
          "hotbar sourcing is a child of Hand Restock without an extra key binding");
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
    size_t live = 0;
    for (size_t i = 0; i < input::actions.size(); ++i) if (!input::retired(static_cast<input::Action>(i))) ++live;
    check(options.size() == listed && actions.size() == live, "all settings and actions are reachable");
    check(input::retired(input::Action::CaptureBreaking) && input::retired(input::Action::ResetBreaking)
          && !input::retired(input::Action::CycleBreakingMode), "only the L-15 capture and reset actions are retired");
    check(layouts.size() == 8, "every HUD element is reachable from the settings list");
    {
        auto sort = std::find_if(rows.begin(), rows.end(), [](auto const& row) {
            return row.heading() && row.feature->id == "sorting";
        });
        check(sort != rows.end() && sort + 1 != rows.end()
            && (sort + 1)->action == input::Action::Sort
            && input::defaultChord(input::Action::Sort) == input::Chord{{input::Device::Key, 0x52}},
            "Sort now keeps R as the first child of the sorting switch");
        auto breaking = std::find_if(rows.begin(), rows.end(), [](auto const& row) {
            return row.option && row.option->id == "interaction.breaking";
        });
        check(breaking != rows.end() && ui::optionAction(breaking->option->id) == input::Action::BreakingRestriction,
            "breaking restriction key appears beside its own switch");
    }
    for (auto id : {"inventory.transferWheelOne", "inventory.transferWheelStack",
                    "inventory.transferDragStack", "inventory.transferDragOne"})
        check(options.contains(id), "transfer gesture switches appear under Inventory Transfer");
    {
        Settings changed;
        changed.camera.zoomToggle = true;
        changed.camera.magnification = 20;
        changed.camera.freelookToggle = true;
        changed.camera.freelookStartPerspective = 2;
        changed.camera.freeCameraToggle = false;
        changed.hud.magnification.dy = 90;
        changed.inventory.sorting = !Settings{}.inventory.sorting;
        changed.bindings[static_cast<size_t>(input::Action::Zoom)] = input::Chord{};
        ui::resetSection(changed, "section.camera");
        Settings defaults;
        check(changed.camera.zoomToggle == defaults.camera.zoomToggle && changed.camera.magnification == defaults.camera.magnification
            && changed.camera.freelookToggle == defaults.camera.freelookToggle
            && changed.camera.freelookStartPerspective == defaults.camera.freelookStartPerspective
            && changed.camera.freeCameraToggle == defaults.camera.freeCameraToggle
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
    {
        std::set<std::string_view> collapsed = {"zoom"};
        auto folded = ui::buildSettingsRows(false, {}, query, expanded, translate, {}, collapsed);
        check(folded.size() == 2 && folded[1].feature->id == "zoom" && !folded[1].expanded,
              "a setting match can be collapsed while searching");
        collapsed.clear();
        query.clear(); query.append("zoom");
        auto byName = ui::buildSettingsRows(false, {}, query, expanded, translate);
        auto heading = std::find_if(byName.begin(), byName.end(), [](auto const& row) {
            return row.heading() && row.feature->id == "zoom";
        });
        check(heading != byName.end() && !heading->expanded,
              "a feature name match starts collapsed when it was collapsed before search");
        expanded.insert("zoom");
        byName = ui::buildSettingsRows(false, {}, query, expanded, translate);
        heading = std::find_if(byName.begin(), byName.end(), [](auto const& row) {
            return row.heading() && row.feature->id == "zoom";
        });
        check(heading != byName.end() && heading->expanded && heading->children > 0
              && std::any_of(byName.begin(), byName.end(), [](auto const& row) {
                  return row.feature && row.feature->id == "zoom" && row.option
                      && row.option->id == "camera.zoomActivation";
              }), "clicking a feature name match reveals its settings");
        expanded.erase("zoom");
    }
    query.clear(); query.append("Camera & view");
    rows = ui::buildSettingsRows(false, {}, query, expanded, translate);
    for (auto const& row : rows) check(row.section == "section.camera", "section search stays in matching group");
    query.clear(); query.append("ズーム");
    rows = ui::buildSettingsRows(false, {}, query, expanded, [](std::string_view key) { return std::string(ui::translations::find(key, "ja_JP")); });
    check(rows.size() >= 2 && rows[1].feature->id == "zoom" && !rows[1].expanded, "Japanese feature search keeps a matched feature collapsed");
    query.clear(); query.append("Shape rendering");
    rows = ui::buildSettingsRows(false, {}, query, expanded, translate);
    check(rows.size() >= 2 && rows[1].heading() && rows[1].feature->id == "shapes" && rows[1].children == 1,
        "shape rendering keeps the key that opens its screen (SETTINGS-KEYMAP: openers sit with their feature)");
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
    check(actionRows == live, "Hotkeys includes every action that is not retired");
    query.clear();
    {
        std::set<std::string_view> open = {"settings", "automationStatus", "toasts", "hudText"};
        auto view = ui::buildSettingsRows(false, "section.interface", query, open, translate);
        size_t heading = view.size(), toasts = view.size(), text = view.size();
        for (size_t i = 0; i < view.size(); ++i) {
            if (!view[i].heading()) continue;
            if (view[i].feature->id == "settings") heading = i;
            if (view[i].feature->id == "toasts") toasts = i;
            if (view[i].feature->id == "hudText") text = i;
        }
        check(heading + 3 < view.size(), "settings feature lists its rows");
        check(view[heading+1].option && view[heading+1].option->id == "interface.animations",
            "the settings screen heading starts with its animations");
        check(toasts < view.size() && view[toasts+1].option && view[toasts+1].option->id == "interface.toggleToasts"
            && std::any_of(view.begin() + toasts + 1, view.end(), [](auto const& row) { return row.kind == RowKind::Layout; }),
            "toggle toasts is its own heading with the switch first and the toast's layout link (L-98)");
        check(text < view.size() && view[text+1].option && view[text+1].option->id == "interface.hudRowHeight",
            "HUD text heading holds the line height");
        std::vector<input::Action> openerKeys;
        for (size_t i = heading + 1; i < view.size() && view[i].child(); ++i)
            if (view[i].action) openerKeys.push_back(*view[i].action);
        check(openerKeys.size() == 2 && openerKeys[0] == input::Action::OpenHotkeys
            && openerKeys[1] == input::Action::OpenHudLayout,
            "General keeps only the openers of screens no feature owns: Hotkeys, HUD layout");
    }
    {
        auto hotkeys = ui::buildSettingsRows(true, {}, query, expanded, translate);
        size_t section = hotkeys.size();
        for (size_t i = 0; i < hotkeys.size(); ++i)
            if (hotkeys[i].kind == RowKind::Section && hotkeys[i].section == "section.interface") section = i;
        check(section + 3 < hotkeys.size() && hotkeys[section+1].action == input::Action::Settings
            && hotkeys[section+2].action == input::Action::OpenHotkeys
            && hotkeys[section+3].action == input::Action::OpenHudLayout,
            "Hotkeys lists General's openers in sidebar order");
    }
    {
        // Info line rows follow the user-ordered list, not catalog order.
        std::vector<std::string> order = {"ping", "coordinates"};
        for (auto id : information::defaultLineOrder()) {
            if (id != "ping" && id != "coordinates") order.emplace_back(id);
            if (id == "biome") order.emplace_back("biomeDisplay");
            if (id == "realTime") order.emplace_back("realTimeDisplay");
        }
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
    for (auto locale : {"en_US", "ja_JP", "zh_CN"}) {
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

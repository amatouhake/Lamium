#include "ui/DataNotice.h"
#include "features/schematic/SchematicsView.h"
#include "features/schematic/SchematicsRows.h"
#include "features/schematic/PlacedStructure.h"
#include "ui/ScreenParts.h"
#include "ui/ListViewWidgets.h"
#include "ui/RowSelection.h"
#include "settings/Options.h"
#include "ui/SettingsRows.h"
#include "ui/SettingsNavigation.h"
#include "ui/SettingsTable.h"
#include "ui/ShapeEditor.h"
#include "ui/ShapesLayout.h"
#include "ui/WaypointPromptLayout.h"
#include "ui/SavePromptLayout.h"
#include "ui/RadialLayout.h"
#include "ui/Animations.h"
#include "ui/Toast.h"
#include "features/map/MapLayers.h"
#include "features/map/WaypointSession.h"
#include "app/SessionIds.h"
#include "features/schematic/SchematicSession.h"
#include "features/schematic/SchematicActions.h"
#include "features/schematic/Preview.h"
#include "features/schematic/GhostRenderer.h"
#include "features/schematic/GhostCommon.h"
#include "features/schematic/SchematicFiles.h"
#include "features/schematic/MaterialAmount.h"
#include "app/Desktop.h"
#include "features/information/SchematicTarget.h"
#include "features/schematic/SchematicItems.h"
#include "features/recipes/RecipeState.h"
#include "features/schematic/Selection.h"
#include "features/schematic/MenuModel.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/deps/nbt/Tag.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/ItemStack.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/actor/ItemRenderer.h"
#include "ui/SearchQuery.h"
#include "ui/TextFit.h"
#include "ui/NumberInput.h"
#include "ui/Widgets.h"
#include "ui/Localization.h"
#include "ui/GameLanguage.h"
#include "ui/HudEditorLayout.h"
#include "ui/PaneScroll.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "features/inspection/render/ItemIcon.h"
#include "features/information/InfoHud.h"
#include <chrono>
#include "mc/client/gui/controls/VisualTree.h"
#include "input/Actions.h"
#include "input/BindingCapture.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/input/KeyboardManager.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/client/gui/screens/SceneFactory.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/gui/screens/interfaces/ISceneStack.h"
#include "mc/deps/input/MouseAction.h"
#include "mc/world/phys/HitResult.h"
#include <array>
#include <mutex>
#include <set>
#include <tuple>
#include <stdexcept>
#include <vector>


namespace lamium::ui::schematics_view {
namespace {
using ShapeZone = ShapesLayout::Zone;
// A schematic preview (L-114): where it was drawn, how it is turned, and a
// drag in progress. The Files tab and the Check tab each have their own. The
// Files tab's turns by itself until the player drags or slices it, and again
// for a newly chosen file or once the screen is opened again; the Check
// tab's stays where it is turned. A new file starts whole, fitted and at the
// default tilt.
struct PreviewTurn {
    float x = 0, y = 0, w = 0, h = 0; // last drawn box, GUI units
    float yaw = 35, pitch = 30, zoom = 1;
    int peel = 0; // levels sliced off the top
    // The block clicked in the preview (structure cell) and its file.
    std::optional<schematic::preview::Cell> picked;
    std::string pickedFile;
    std::string file; // the structure shown
    int placedTurn = 0; // the Check tab's placement turn and mirror shown: a picked cell belongs to it
    bool manual = false, dragging = false, turning = false; // turning: this drag moved far enough to turn
    glm::vec2 from{};
    float fromYaw = 0, fromPitch = 0;
} filesTurn, checkTurn;
// The preview of the tab shown, which input goes to.
PreviewTurn& previewTurn();
// What a newly shown file starts with.
void startFile(PreviewTurn& t, std::string const& file) {
    t.file = file;
    t.peel = 0;
    t.zoom = 1;
    t.pitch = PreviewTurn{}.pitch;
    t.picked.reset();
}
// The clock the preview turns by, in seconds.
double previewClock() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
// The preview's corner line: the hint while it turns by itself, then the
// slice ("height 12 of 20", with the world's Y where it is placed: `worldY`
// is the placement's lowest level), which a click resets. Kept at the top
// edge: the preview's depth hides text drawn over the model.
void drawPreviewLabel(MinecraftUIRenderContext& context, PreviewTurn const& t, float x, float y, float w,
                      std::optional<int> worldY = std::nullopt) {
    if (t.peel > 0) {
        int const height = schematic::preview::last().layers, top = schematic::preview::sliceTop(height, t.peel);
        std::string text = worldY ? translated("schematic.previewLayersY", top, height, *worldY + top - 1)
                                  : translated("schematic.previewLayers", top, height);
        label(context,x+4,y+3,w-8,text,palette::warning);
    } else if (!t.manual) label(context,x+4,y+3,w-8,translated("schematic.previewHint"),palette::faint);
}
// Set when a pick selected a row: the next layout scrolls the list to it.
bool scrollToSelected = false;
// Inspect: the clicked block is remembered for the Files pane; in the Check
// tab its mistake row is selected (and scrolled to), if it has one.
void pickInPreview(glm::vec2 at);
// Schematics view (L-93): placements of this world above the files in the
// schematics folder, built like Waypoints. The file list is scanned when the
// view opens and on "Reload files", not every frame.
bool schematicsDocked = false;
enum class SchematicPick { None, Placement, File };
using SchematicTab = Tab;
SchematicTab schematicTab = SchematicTab::Placements;
SchematicPick schematicPick = SchematicPick::None;
PreviewTurn& previewTurn() { return schematicTab == SchematicTab::Verify ? checkTurn : filesTurn; }
// The Check tab's preview draws the checked placement as placed (L-114): its structure turned and mirrored like
// the world's ghosts, made again only when the file, the turn or the mirror changes. Unturned, the file's own.
std::shared_ptr<schematic::Structure const> placedView(std::shared_ptr<schematic::Structure const> const& structure,
                                                       schematic::Placement const& placement) {
    int const turns = schematic::quarterTurns(placement.rotation);
    if (!structure || (turns == 0 && placement.mirror == schematic::Mirror::None)) return structure;
    static std::weak_ptr<schematic::Structure const> from;
    static int fromTurns = 0;
    static schematic::Mirror fromMirror = schematic::Mirror::None;
    static std::shared_ptr<schematic::Structure const> placed;
    if (from.lock() != structure || fromTurns != turns || fromMirror != placement.mirror || !placed) {
        from = structure;
        fromTurns = turns;
        fromMirror = placement.mirror;
        placed = std::make_shared<schematic::Structure const>(schematic::placedStructure(*structure, placement));
    }
    return placed;
}
// A cell of the Check tab's preview in the world: the preview is drawn as placed, from the placement's corner.
schematic::Point checkCellWorld(schematic::SavedPlacement const& checked, schematic::preview::Cell const& cell) {
    auto const& o = checked.placement.origin;
    return {o.x + cell.x, o.y + cell.y, o.z + cell.z};
}
// The angle a preview is drawn at: the Files tab's adds its own turn until it is stopped.
float shownYaw(PreviewTurn const& t) {
    return t.yaw + (&t == &filesTurn && !t.manual ? schematics_rows::autoTurn(previewClock()) : 0.f);
}
int schematicIndex = -1; // into the placements or the files
schematic::PlacementSet schematicSet;
std::vector<schematic::session::FileEntry> schematicFiles;
std::vector<schematic_files::Row> schematicFileRows; // the Files list: folder headings and files
// What is remembered per loaded structure. A reloaded file's structure may
// stand at a freed one's address, so an entry also holds its structure
// weakly and is used only while that one lives.
template <class Value>
struct OfStructure {
    std::weak_ptr<schematic::Structure const> structure;
    Value value{};
    bool holds(std::shared_ptr<schematic::Structure const> const& asked) const { return !asked || !structure.expired(); }
};
// Blocks other than air and structure void, counted once per loaded structure.
std::map<schematic::Structure const*, OfStructure<std::uint64_t>> schematicBlockCounts;
std::uint64_t blockCount(std::shared_ptr<schematic::Structure const> const& structure) {
    if (auto found = schematicBlockCounts.find(structure.get()); found != schematicBlockCounts.end() && found->second.holds(structure))
        return found->second.value;
    if (schematicBlockCounts.size() > 256) schematicBlockCounts.clear();
    std::uint64_t count = 0;
    for (auto index : structure->blocks)
        if (index != schematic::voidCell && !structure->palette[static_cast<size_t>(index)].isAir()) ++count;
    schematicBlockCounts[structure.get()] = {structure, count};
    return count;
}
// Cells per block name, counted once per loaded structure (L-183: whether a
// rule's block is still in the file).
std::map<schematic::Structure const*, OfStructure<schematic::BlockCounts>> schematicNameCounts;
schematic::BlockCounts const& nameCounts(std::shared_ptr<schematic::Structure const> const& structure) {
    if (auto found = schematicNameCounts.find(structure.get()); found != schematicNameCounts.end() && found->second.holds(structure))
        return found->second.value;
    if (schematicNameCounts.size() > 64) schematicNameCounts.clear();
    auto& entry = schematicNameCounts[structure.get()];
    entry = {structure, schematic::blockCounts(*structure)};
    return entry.value;
}
// A block's display name and icon as the file names it (the first palette
// entry of that name), for the ignore view; remembered per structure and
// name until the language changes. A name the file lacks reads without
// "minecraft:".
schematic::ghosts::BlockLabel const& fileBlockLabel(std::shared_ptr<schematic::Structure const> const& structure,
                                                    std::string const& name) {
    static std::map<std::pair<schematic::Structure const*, std::string>, OfStructure<schematic::ghosts::BlockLabel>> labels;
    static std::string labelsLanguage;
    if (auto code = game_language::currentCode(); code != labelsLanguage) {
        labels.clear();
        labelsLanguage = std::move(code);
    }
    auto key = std::pair{structure.get(), name};
    if (auto found = labels.find(key); found != labels.end() && found->second.holds(structure)) return found->second.value;
    if (labels.size() > 512) labels.clear();
    schematic::ghosts::BlockLabel label{name.starts_with("minecraft:") ? name.substr(10) : name, {}};
    if (structure)
        for (auto const& entry : structure->palette)
            if (entry.name == name) {
                if (auto found = schematic::ghosts::blockLabel(entry); !found.name.empty()) label = std::move(found);
                break;
            }
    auto& entry = labels[key];
    entry = {structure, std::move(label)};
    return entry.value;
}
// The items a rule can name per block type of the file (L-183), counted
// again when the file or the block entity switches change.
schematic::ComparedItems const& comparedItemsOf(std::shared_ptr<schematic::Structure const> const& structure) {
    static std::shared_ptr<schematic::Structure const> at;
    static bool sign = false, entities = false;
    static schematic::ComparedItems items;
    bool const ignoreSign = schematic::ghosts::signTextIgnored(), ignoreEntities = schematic::ghosts::blockEntitiesIgnored();
    if (at != structure || sign != ignoreSign || entities != ignoreEntities) {
        at = structure;
        sign = ignoreSign;
        entities = ignoreEntities;
        items = structure ? schematic::comparedItems(*structure, ignoreSign, ignoreEntities) : schematic::ComparedItems{};
    }
    return items;
}
std::chrono::steady_clock::time_point schematicFilesScanned{};
std::string largeSchematicConfirmed; // a large file the player chose to load
ShapesLayout schematicsDisplayed;
int schematicListFirst = 0, schematicFieldFirst = 0, schematicFieldSelected = -1;
bool schematicDeleteArmed = false;
// The Placements tab's selection by session id (L-190). One row is the picked
// placement, which is also the schematic's selected one; while several are
// selected none is picked and the schematic's selected placement stays.
RowSelection<std::uint64_t> placementRows;
BulkArm placementBulk;
// The Placed list's own column: progress (L-190).
constexpr float progressColumn = 26;
int editingSchematicField = -1;
// The Layer field's cut when its edit began in All: that number again (Esc
// puts it back) returns to All, which typing another one left.
std::optional<int> layerEditAllCut;
void changeSchematic(std::function<void(schematic::SavedPlacement&)> const& apply);
// ---- Schematics view (L-93) ----
// Four tabs share the list-and-detail layout: placements, files, the
// verification of the selected placement, and its materials.
enum class SchematicField { X, Y, Z, Rotation, Mirror, MoveHere, Visible, LayerAxis, LayerMode, Layer, Keep, MatchLayer, Extras, Entities };
constexpr auto schematicFields = std::to_array<SchematicField>({SchematicField::X, SchematicField::Y, SchematicField::Z,
    SchematicField::Rotation, SchematicField::Mirror, SchematicField::MoveHere, SchematicField::Visible, SchematicField::LayerAxis,
    SchematicField::LayerMode, SchematicField::Layer, SchematicField::Keep, SchematicField::MatchLayer, SchematicField::Extras,
    SchematicField::Entities});
// The Check list's filter: kept while the game runs, not saved (L-183).
schematic::CheckFilter checkFilter = schematic::CheckFilter::All;
int verifySelected = -1; // the one selected Check row, -1 with none or several
// The Check list's rows by cell (L-183 with L-190): a row keeps its place in
// the selection across passes and filters while it is listed.
struct CheckKey {
    schematic::Point position;
    bool entity = false;
    bool operator==(CheckKey const&) const = default;
};
RowSelection<CheckKey> checkRows;
BulkArm checkBulk;
// L-183: the Check pane's ignore controls, placed while drawing and hit by
// the next click.
bool verifyUndoAllArmed = false;
// The Check tab's ignore view (L-183), toggled by "Ignore settings": the
// ignored places and the blocks whose differences can be ignored replace
// the list; the picked row is kept by what it names.
bool ignoreView = false;
std::vector<schematic::IgnoreRow> ignoreList;
struct IgnorePick {
    schematic::IgnoreRow::Kind kind;
    schematic::IgnoreEntry entry;
    std::string block;
    bool names(schematic::IgnoreRow const& row) const {
        return row.kind == kind && (kind == schematic::IgnoreRow::Kind::Place ? row.entry == entry : row.block == block);
    }
};
std::optional<IgnorePick> ignorePick;
// "Ignore settings" as last drawn, at the end of the Check pane's top line;
// 0 wide when not drawn.
struct { float x = 0, y = 0, w = 0, h = 0; } ignoreSettingsBox;
// The Check pane's item lines (a row's differing items, a block's compared
// items) scroll under the wheel when they do not fit; the offset starts
// over when what they list changes. The area is as last drawn, 0 wide
// when the lines fit.
struct {
    int first = 0, count = 0, visible = 0;
    std::string of;
    bool drawn = false; // this frame, else `of` is forgotten
    float x = 0, y = 0, w = 0, h = 0;
    ScrollBar bar;      // as drawn; a press holds it (screen::pressScrollbar)
} itemScroll;
schematic::IgnoreRow const* pickedIgnoreRow() {
    if (!ignorePick) return nullptr;
    for (auto const& row : ignoreList) if (ignorePick->names(row)) return &row;
    return nullptr;
}
void refreshIgnoreList();
// Add and Remove change the entries named; Show is "Show in world".
enum class IgnoreControlKind { Add, Remove, UndoAll, Show };
struct IgnoreControl {
    float x, y, w, h;
    IgnoreControlKind kind;
    std::vector<schematic::IgnoreEntry> entries;
};
void showSelectedMismatch();
schematic::SavedPlacement const* checkedPlacement();
std::vector<IgnoreControl> ignoreControls;
bool materialsShownOnly = false;
std::shared_ptr<schematic::Verification const> verification;
std::vector<schematic::Mismatch const*> verifyRows;
std::vector<schematic::MaterialLine const*> materialRows; // blocks first, then entities
// The Materials list: -1 a "Blocks" heading, -2 an "Entities" heading, else an
// index into materialRows. Headings only when both kinds are present.
std::vector<int> materialListRows;
int materialSelected = -1; // index into materialRows

// The last pass's row for a place, if it listed one.
schematic::Mismatch const* rowAt(schematic::IgnoreEntry const& e) {
    if (!verification) return nullptr;
    for (auto const& m : verification->mismatches)
        if (!m.entity && m.local == e.cell && m.fileBlock == e.block) return &m;
    return nullptr;
}
bool verifyMatches(schematic::Mismatch const& m) { return schematic::listedIn(checkFilter, m.state); }
CheckKey checkKeyOf(int row) {
    auto const* m = verifyRows[static_cast<size_t>(row)];
    return {m->position, m->entity};
}
std::vector<CheckKey> checkOrder() {
    std::vector<CheckKey> order;
    for (auto const* m : verifyRows) order.push_back({m->position, m->entity});
    return order;
}
// The selected rows in list order.
std::vector<schematic::Mismatch const*> selectedCheckRows() {
    std::set<std::tuple<int, int, int, bool>> chosen;
    for (auto const& key : checkRows.keys) chosen.insert({key.position.x, key.position.y, key.position.z, key.entity});
    std::vector<schematic::Mismatch const*> out;
    for (auto const* m : verifyRows)
        if (chosen.contains({m->position.x, m->position.y, m->position.z, m->entity})) out.push_back(m);
    return out;
}
// verifySelected names the row the detail pane shows: the one selected row.
void followCheckRows() {
    checkBulk.disarm();
    verifySelected = -1;
    if (auto one = checkRows.single())
        for (size_t i = 0; i < verifyRows.size(); ++i)
            if (CheckKey{verifyRows[i]->position, verifyRows[i]->entity} == *one) { verifySelected = static_cast<int>(i); break; }
}
void selectCheckRow(int row) {
    if (row >= 0 && row < static_cast<int>(verifyRows.size())) checkRows.only(checkKeyOf(row));
    else checkRows.clear();
    followCheckRows();
}
void refreshSchematics(bool files) {
    schematicSet = schematic::session::current();
    // Files copied in while the Files tab is open show up without a reload.
    auto now = std::chrono::steady_clock::now();
    if (schematicTab == SchematicTab::Files && now - schematicFilesScanned > std::chrono::seconds(2)) files = true;
    if (files) {
        schematicFiles = schematic::session::files();
        schematicFilesScanned = now;
        std::vector<std::string> paths;
        for (auto const& f : schematicFiles) paths.push_back(f.relative);
        schematicFileRows = schematic_files::rows(paths);
    }
    int placements = static_cast<int>(schematicSet.placements.size()), fileCount = static_cast<int>(schematicFiles.size());
    if ((schematicPick == SchematicPick::Placement && schematicIndex >= placements)
        || (schematicPick == SchematicPick::File && schematicIndex >= fileCount)) schematicPick = SchematicPick::None;
    // One row follows the picked placement; several stay until a row is picked.
    placementRows.retain([](std::uint64_t id) { return indexOfId(schematicSet.placements, id) >= 0; });
    placementRows.follow(schematicPick == SchematicPick::Placement && schematicIndex >= 0
        ? std::optional{schematicSet.placements[static_cast<size_t>(schematicIndex)].id} : std::nullopt);
    verification = schematic::ghosts::verification();
    refreshIgnoreList();
    verifyRows.clear();
    materialRows.clear();
    if (Runtime::instance().preferences().schematic.enabled && verification->placement >= 0 && verification->placement == schematicSet.selected) {
        for (auto const& m : verification->mismatches) if (verifyMatches(m)) verifyRows.push_back(&m);
        for (auto const& line : materialsShownOnly ? verification->visibleMaterials : verification->materials) materialRows.push_back(&line);
        std::stable_partition(materialRows.begin(), materialRows.end(), [](auto const* line) { return !line->entity; });
    }
    std::vector<bool> entityLines;
    for (auto const* line : materialRows) entityLines.push_back(line->entity);
    materialListRows = schematics_rows::materialList(entityLines);
    if (materialSelected >= static_cast<int>(materialRows.size())) materialSelected = -1;
    // The selected rows follow their cells when the rows change (a new pass,
    // a filter): the same index would be another cell.
    if (!checkRows.keys.empty()) {
        std::set<std::tuple<int, int, int, bool>> listed;
        for (auto const* m : verifyRows) listed.insert({m->position.x, m->position.y, m->position.z, m->entity});
        checkRows.retain([&](CheckKey const& key) { return listed.contains({key.position.x, key.position.y, key.position.z, key.entity}); });
    }
    followCheckRows();
}
// Changes the selected placement's ignored differences (L-183).
void changeIgnores(std::function<bool(schematic::IgnoreList&)> const& apply) {
    if (!schematic::session::change([&](schematic::PlacementSet& set) {
            if (set.selected < 0 || set.selected >= static_cast<int>(set.placements.size())) return false;
            return apply(set.placements[static_cast<size_t>(set.selected)].ignored);
        }))
        return;
    refreshSchematics(false);
}
bool pressIgnoreControl(float x, float y) {
    for (auto const& c : ignoreControls) {
        if (x < c.x || x >= c.x + c.w || y < c.y || y >= c.y + c.h) continue;
        auto const entries = c.entries;
        auto const kind = c.kind;
        if (kind != IgnoreControlKind::UndoAll) verifyUndoAllArmed = false;
        switch (kind) {
        case IgnoreControlKind::Add: {
            // All or nothing (L-183): over the cap nothing changes.
            bool full = false;
            changeIgnores([&](schematic::IgnoreList& list) {
                auto added = list.addAll(entries);
                full = added == schematic::IgnoreList::Added::Full;
                return added == schematic::IgnoreList::Added::Added;
            });
            if (full) screen::setMessage(translated("schematic.ignore.full", schematic::maxIgnored));
            else screen::clearMessage();
            break;
        }
        case IgnoreControlKind::Remove:
            changeIgnores([&](schematic::IgnoreList& list) {
                return list.removeIf([&](auto const& e) { return std::find(entries.begin(), entries.end(), e) != entries.end(); }) > 0;
            });
            screen::clearMessage();
            break;
        case IgnoreControlKind::UndoAll:
            if (!std::exchange(verifyUndoAllArmed, true)) return true;
            verifyUndoAllArmed = false;
            changeIgnores([](schematic::IgnoreList& list) { if (list.empty()) return false; list.clear(); return true; });
            break;
        case IgnoreControlKind::Show: showSelectedMismatch(); break;
        }
        return true;
    }
    return false;
}
// The ignore view's rows, built every refresh while it is open: the ignored
// places nearest first, then the file's block types with items in the
// Materials tab's order.
void refreshIgnoreList() {
    ignoreList.clear();
    auto const* checked = checkedPlacement();
    if (!ignoreView || schematicTab != SchematicTab::Verify || !checked) return;
    auto structure = schematic::session::loaded(checked->file);
    static schematic::BlockCounts const noCounts;
    auto const& counts = structure ? nameCounts(structure) : noCounts;
    std::vector<std::string> order;
    if (structure) {
        std::vector<std::pair<std::string, std::string>> blocks;
        for (auto const& [block, items] : comparedItemsOf(structure)) blocks.push_back({block, fileBlockLabel(structure, block).name});
        std::vector<std::string> names;
        if (verification && verification->placement == schematicSet.selected)
            for (auto const& line : verification->materials) names.push_back(line.name);
        order = schematic::inMaterialsOrder(std::move(blocks), names);
    }
    schematic::Point from{};
    if (auto* player = screen::client() ? screen::client()->getLocalPlayer() : nullptr) {
        Vec3 feet = player->getFeetPos();
        from = {static_cast<int>(std::floor(feet.x)), static_cast<int>(std::floor(feet.y)), static_cast<int>(std::floor(feet.z))};
    }
    ignoreList = schematic::ignoreRows(checked->ignored, structure.get(), counts, order, checked->placement, from);
    if (ignorePick && !pickedIgnoreRow()) ignorePick.reset();
}
void toggleIgnoreView() {
    ignoreView = !ignoreView;
    verifyUndoAllArmed = false;
    schematicListFirst = 0;
    screen::clearMessage();
    refreshSchematics(false);
}
// Page Up and Down step over headings.
void stepIgnorePick(int step) {
    int count = static_cast<int>(ignoreList.size()), at = -1;
    for (int i = 0; i < count; ++i) if (ignorePick && ignorePick->names(ignoreList[static_cast<size_t>(i)])) at = i;
    for (int i = at < 0 && step < 0 ? count + step : at + step; i >= 0 && i < count; i += step) {
        auto const& row = ignoreList[static_cast<size_t>(i)];
        if (!row.selectable()) continue;
        ignorePick = IgnorePick{row.kind, row.entry, row.block};
        verifyUndoAllArmed = false;
        return;
    }
}
int schematicRowCount() {
    switch (schematicTab) {
    case SchematicTab::Placements: return static_cast<int>(schematicSet.placements.size());
    case SchematicTab::Files: return static_cast<int>(schematicFileRows.size());
    case SchematicTab::Verify: return static_cast<int>(ignoreView ? ignoreList.size() : verifyRows.size());
    default: return static_cast<int>(materialListRows.size());
    }
}
schematic::SavedPlacement const* selectedPlacement() {
    return schematicTab == SchematicTab::Placements && !placementRows.several() && schematicPick == SchematicPick::Placement && schematicIndex >= 0
        && schematicIndex < static_cast<int>(schematicSet.placements.size())
        ? &schematicSet.placements[static_cast<size_t>(schematicIndex)] : nullptr;
}
schematic::SavedPlacement const* checkedPlacement() {
    return schematicSet.selected >= 0 && schematicSet.selected < static_cast<int>(schematicSet.placements.size())
        ? &schematicSet.placements[static_cast<size_t>(schematicSet.selected)] : nullptr;
}
void pickSchematic(SchematicPick pick, int index) {
    screen::finishEditing();
    schematicPick = pick;
    schematicIndex = index;
    // Picking a placement (a plain click, Page Up/Down, a new one) returns to that one row.
    if (pick == SchematicPick::Placement && index >= 0 && index < static_cast<int>(schematicSet.placements.size()))
        placementRows.only(schematicSet.placements[static_cast<size_t>(index)].id);
    schematicFieldFirst = 0;
    schematicFieldSelected = -1;
    schematicDeleteArmed = false;
    // The selected placement is the one keys, the HUD and the Verify tab act on.
    if (pick == SchematicPick::Placement && schematicSet.selected != index)
        if (schematic::session::change([&](schematic::PlacementSet& set) { set.selected = index; return true; }))
            schematic::actions::placementSelected();
    refreshSchematics(false);
}
void selectSchematicTab(SchematicTab tab) {
    screen::finishEditing();
    schematicTab = tab;
    schematicListFirst = schematicFieldFirst = 0;
    schematicFieldSelected = -1;
    schematicDeleteArmed = verifyUndoAllArmed = false;
    ignoreView = false;
    ignorePick.reset();
    checkRows.clear();
    followCheckRows();
    placementRows.clear();
    placementBulk.disarm();
    if (tab == SchematicTab::Placements) {
        schematicPick = schematicSet.selected >= 0 ? SchematicPick::Placement : SchematicPick::None;
        schematicIndex = schematicSet.selected;
    } else if (tab == SchematicTab::Files) {
        schematicPick = SchematicPick::None;
        schematicIndex = -1;
        refreshSchematics(true);
    }
}
void changeSchematic(std::function<void(schematic::SavedPlacement&)> const& apply) {
    int index = schematicPick == SchematicPick::Placement ? schematicIndex : -1;
    if (!schematic::session::change([&](schematic::PlacementSet& set) {
            if (index < 0 || index >= static_cast<int>(set.placements.size())) return false;
            apply(set.placements[static_cast<size_t>(index)]);
            return true;
        })) screen::setMessage(translated("schematic.saveError"));
    else screen::clearMessage();
    refreshSchematics(false);
}
int placedLayers(schematic::SavedPlacement const& p) {
    auto structure = schematic::session::structure(p.file);
    if (!structure) return 1;
    return std::max(1, schematic::layerCount(structure->size, schematic::resolvedLayers(p.layers, structure->size, p.placement).axis));
}
// The cut plane as stored, converted from the old six-direction form when the
// file predates layerView and the structure is loaded.
schematic::Layers shownLayers(schematic::SavedPlacement const& p) {
    auto structure = schematic::session::structure(p.file);
    return structure ? schematic::resolvedLayers(p.layers, structure->size, p.placement) : p.layers;
}
// A large file is loaded only after the player confirms it.
bool waitsForLoad(schematic::session::FileEntry const& f) {
    return f.bytes > schematic::session::largeFileBytes && largeSchematicConfirmed != f.relative;
}
void placeSelectedFile() {
    if (schematicPick != SchematicPick::File || schematicIndex < 0 || schematicIndex >= static_cast<int>(schematicFiles.size())) return;
    if (auto const& f = schematicFiles[static_cast<size_t>(schematicIndex)]; waitsForLoad(f)) {
        largeSchematicConfirmed = f.relative;
        return;
    }
    auto place = screen::standingPlace();
    if (!place) return;
    std::string problem;
    if (!schematic::session::place(schematicFiles[static_cast<size_t>(schematicIndex)].relative, {place->x, place->y, place->z},
            place->dimension, &problem)) {
        screen::setMessage(problem.empty() ? translated("schematic.saveError") : translated("schematic.notLoaded", problem));
        return;
    }
    screen::clearMessage();
    // The new placement is already selected, so pickSchematic's change check
    // would not run: reset the move target here.
    schematic::actions::placementSelected();
    refreshSchematics(false);
    selectSchematicTab(SchematicTab::Placements);
    pickSchematic(SchematicPick::Placement, static_cast<int>(schematicSet.placements.size()) - 1);
}
void deleteSelectedPlacement() {
    int index = schematicIndex;
    if (!schematic::session::change([&](schematic::PlacementSet& set) { return schematic::removePlacementAt(set, index); })) {
        screen::setMessage(translated("schematic.saveError"));
        return;
    }
    screen::clearMessage();
    refreshSchematics(false);
    int left = static_cast<int>(schematicSet.placements.size());
    if (left) pickSchematic(SchematicPick::Placement, std::min(index, left - 1));
    else pickSchematic(SchematicPick::None, -1);
}
std::vector<std::uint64_t> placementOrder() {
    std::vector<std::uint64_t> order;
    order.reserve(schematicSet.placements.size());
    for (auto const& p : schematicSet.placements) order.push_back(p.id);
    return order;
}
// The picked placement follows a row left alone, so keys, the HUD and the
// Check tab act on it; several rows pick none and leave the schematic's
// selected placement as it was.
void followPlacementRows() {
    placementBulk.disarm();
    schematicDeleteArmed = false;
    if (placementRows.several()) {
        if (schematicPick != SchematicPick::None) pickSchematic(SchematicPick::None, -1);
        return;
    }
    auto one = placementRows.single();
    int index = one ? indexOfId(schematicSet.placements, *one) : -1;
    if (index < 0) pickSchematic(SchematicPick::None, -1);
    else if (schematicPick != SchematicPick::Placement || index != schematicIndex) pickSchematic(SchematicPick::Placement, index);
}
// Ctrl + click toggles a row, Shift + click takes a range.
void pickPlacements(int row) {
    if (row < 0 || row >= static_cast<int>(schematicSet.placements.size())) return;
    placementRows.click(schematicSet.placements[static_cast<size_t>(row)].id, screen::heldCtrl(), screen::heldShift(), placementOrder());
    followPlacementRows();
}
void bulkPlacements(BulkAction action) {
    auto const ids = placementRows.keys;
    if (!schematic::session::change([&](schematic::PlacementSet& set) {
            if (action == BulkAction::Delete) schematic::removePlacements(set, ids);
            else schematic::showPlacements(set, ids, action == BulkAction::Show);
            return true;
        })) { screen::setMessage(translated("schematic.saveError")); return; }
    screen::clearMessage();
    if (action == BulkAction::Delete) placementRows.clear();
    refreshSchematics(false);
}
void showSelectedMismatch() {
    // The rows are from before a move or layer change: their cells are not
    // the ones shown now (L-177).
    if (verification && verification->stale) { screen::setMessage(translated("schematic.toast.counting")); return; }
    if (checkRows.several()) {
        std::vector<schematic::Point> cells;
        for (auto const* m : selectedCheckRows()) cells.push_back(m->position);
        schematic::ghosts::point(std::move(cells));
        screen::close();
        return;
    }
    if (verifySelected >= 0 && verifySelected < static_cast<int>(verifyRows.size())) {
        schematic::ghosts::point(verifyRows[static_cast<size_t>(verifySelected)]->position);
        screen::close();
        return;
    }
    // A block clicked in the preview that has no row (past the list's limit).
    auto const& t = checkTurn;
    auto const* checked = checkedPlacement();
    if (!t.picked || !checked || t.pickedFile != checked->file) return;
    schematic::ghosts::point(checkCellWorld(*checked, *t.picked));
    screen::close();
}
int schematicFieldCount() {
    switch (schematicTab) {
    case SchematicTab::Placements: return selectedPlacement() ? static_cast<int>(schematicFields.size()) : 0;
    case SchematicTab::Materials: return 3;
    default: return 0;
    }
}
// part: -1/1 step, 0 value (type a number or press), 2 label.
void activateSchematicField(int index, int part) {
    if (schematicTab == SchematicTab::Materials) {
        if (part == 2) return;
        if (index == 0) {
            materialsShownOnly = !materialsShownOnly;
            refreshSchematics(false);
        } else if (index == 1) {
            screen::toggleOption("schematic.hud");
        } else if (index == 2) {
            screen::toggleOption("schematic.planMaterials");
        }
        return;
    }
    auto const* p = selectedPlacement();
    if (!p || index < 0 || index >= static_cast<int>(schematicFields.size())) return;
    schematicFieldSelected = index;
    if (part == 2) return;
    int direction = part == -1 ? -1 : 1;
    switch (schematicFields[static_cast<size_t>(index)]) {
    case SchematicField::X: case SchematicField::Y: case SchematicField::Z: {
        int value = index == 0 ? p->placement.origin.x : index == 1 ? p->placement.origin.y : p->placement.origin.z;
        if (part == 0) { editingSchematicField = index; screen::number().beginPrecise(value); screen::clearMessage(); return; }
        changeSchematic([&](schematic::SavedPlacement& t) {
            (index == 0 ? t.placement.origin.x : index == 1 ? t.placement.origin.y : t.placement.origin.z) += direction * (screen::heldShift() ? 10 : 1);
        });
        return;
    }
    case SchematicField::Rotation: {
        auto structure = schematic::session::structure(p->file);
        if (!structure && schematic::turnNeedsStructure(p->layers)) { screen::setMessage(translated("schematic.rotationNeedsStructure")); return; }
        changeSchematic([&](schematic::SavedPlacement& t) {
            schematic::turnPlacement(t.placement, t.layers, structure ? &structure->size : nullptr,
                [&](schematic::Placement& turned) { turned.rotation = schematic::quarterTurns(turned.rotation + direction); });
        });
        return;
    }
    case SchematicField::Mirror: {
        auto structure = schematic::session::structure(p->file);
        if (!structure && schematic::turnNeedsStructure(p->layers)) { screen::setMessage(translated("schematic.rotationNeedsStructure")); return; }
        changeSchematic([&](schematic::SavedPlacement& t) {
            schematic::turnPlacement(t.placement, t.layers, structure ? &structure->size : nullptr, [&](schematic::Placement& turned) {
                turned.mirror = static_cast<schematic::Mirror>((static_cast<int>(turned.mirror) + direction + 3) % 3);
            });
        });
        return;
    }
    case SchematicField::MoveHere:
        if (auto place = screen::standingPlace())
            changeSchematic([&](schematic::SavedPlacement& t) {
                t.placement.origin = {place->x, place->y, place->z};
                t.dimension = place->dimension;
            });
        return;
    case SchematicField::Visible: changeSchematic([](schematic::SavedPlacement& t) { t.visible = !t.visible; }); return;
    case SchematicField::LayerAxis: {
        auto structure = schematic::session::structure(p->file);
        if (!structure) { screen::setMessage(translated("schematic.layerNeedsStructure")); return; }
        changeSchematic([&](schematic::SavedPlacement& t) {
            schematic::editLayers(t.layers, structure->size, t.placement, [&](schematic::Layers& layers) {
                auto axis = static_cast<schematic::LayerAxis>((static_cast<int>(layers.axis) + direction + 3) % 3);
                // Changing the axis keeps the cut; only Cut here moves it.
                layers = schematic::withAxis(layers, structure->size, axis);
            });
        });
        return;
    }
    case SchematicField::LayerMode: {
        auto structure = schematic::session::structure(p->file);
        if (!structure) { screen::setMessage(translated("schematic.layerNeedsStructure")); return; }
        changeSchematic([&](schematic::SavedPlacement& t) {
            schematic::editLayers(t.layers, structure->size, t.placement, [&](schematic::Layers& layers) {
                layers.mode = static_cast<schematic::LayerMode>((static_cast<int>(layers.mode) + direction + 3) % 3);
            }, false);
        });
        return;
    }
    case SchematicField::Layer: {
        int count = placedLayers(*p);
        auto structure = schematic::session::structure(p->file);
        if (!structure) { screen::setMessage(translated("schematic.layerNeedsStructure")); return; }
        auto view = schematic::resolvedLayers(p->layers, structure->size, p->placement);
        if (part == 0) {
            editingSchematicField = index;
            layerEditAllCut = view.mode == schematic::LayerMode::All ? std::optional{view.cut} : std::nullopt;
            screen::number().beginPrecise(view.cut + 1);
            screen::clearMessage();
            return;
        }
        changeSchematic([&](schematic::SavedPlacement& t) {
            schematic::editLayers(t.layers, structure->size, t.placement, [&](schematic::Layers& layers) {
                if (!schematic::leaveAll(layers)) layers.cut = std::clamp(layers.cut + direction * (screen::heldShift() ? 10 : 1), 0, count - 1);
            });
        });
        return;
    }
    case SchematicField::Keep: {
        auto structure = schematic::session::structure(p->file);
        if (!structure) { screen::setMessage(translated("schematic.layerNeedsStructure")); return; }
        changeSchematic([&](schematic::SavedPlacement& t) {
            schematic::editLayers(t.layers, structure->size, t.placement, [](schematic::Layers& layers) {
                layers.keep = layers.keep == schematic::LayerKeep::Low ? schematic::LayerKeep::High : schematic::LayerKeep::Low;
            });
        });
        return;
    }
    case SchematicField::MatchLayer: {
        auto place = screen::client() ? schematic::actions::viewFeet(*screen::client()) : std::nullopt;
        auto structure = schematic::session::structure(p->file);
        if (!structure) { screen::setMessage(translated("schematic.layerNeedsStructure")); return; }
        if (!place) return;
        changeSchematic([&](schematic::SavedPlacement& t) {
            schematic::editLayers(t.layers, structure->size, t.placement, [&](schematic::Layers& layers) {
                layers.cut = schematic::cutAlong(structure->size, t.placement, layers.axis, {place->x, place->y, place->z});
                schematic::leaveAll(layers);
            });
        });
        return;
    }
    case SchematicField::Extras: changeSchematic([](schematic::SavedPlacement& t) { t.countExtras = !t.countExtras; }); return;
    case SchematicField::Entities: changeSchematic([](schematic::SavedPlacement& t) { t.entities = !t.entities; }); return;
    }
}
void moveSchematicField(int step) {
    int count = schematicFieldCount();
    if (!count) return;
    schematicFieldSelected = std::clamp(schematicFieldSelected + step, 0, count - 1);
    schematicFieldFirst = firstShowing(schematicFieldSelected, schematicFieldFirst, schematicsDisplayed.fieldVisible);
}
void openSchematicKeySettings() { screen::showFeatureKeys("schematic"); }
// The tab strip sits in the list pane's toolbar.
constexpr int schematicTabCount = 4;
// Tabs stand side by side with no gap (DESIGN "Controls").
float schematicTabWidth(ShapesLayout const& l) { return schematics_rows::tabWidth(l, schematicTabCount); }
float schematicTabX(ShapesLayout const& l, int tab) { return schematics_rows::tabX(l, tab, schematicTabCount); }
int schematicTabAt(ShapesLayout const& l, float x, float y) { return schematics_rows::tabAt(l, x, y, schematicTabCount); }
// The Check tab's four filters, in the list's heading row (L-93 screen review):
// sized to their text and drawn as outlined pills, so they read as part of the
// list rather than more tabs. Their spans are kept from the last draw.
std::array<std::pair<float, float>, 4> verifyChipSpans{};
int verifyChipAt(ShapesLayout const& l, float x, float y) {
    if (schematicTab != SchematicTab::Verify || ignoreView || y < l.theadTop || y >= l.rowsTop) return -1;
    for (int i = 0; i < 4; ++i)
        if (x >= verifyChipSpans[static_cast<size_t>(i)].first && x < verifyChipSpans[static_cast<size_t>(i)].second) return i;
    return -1;
}
int stackSizeOf(schematic::MaterialLine const& line) {
    auto const* stack = schematic::items::iconStack(line.icon);
    return stack ? std::max(1, static_cast<int>(stack->getMaxStackSize())) : 64;
}
std::map<std::string, std::uint64_t> carriedItems();
// Materials still to gather: remaining minus what the player carries.
struct Missing { schematic::MaterialLine const* line; std::uint64_t missing; };
std::vector<Missing> missingMaterials() {
    std::vector<Missing> out;
    auto carried = carriedItems();
    for (auto const* line : materialRows) {
        bool noItem = line->item.empty() || (line->entity && !schematic::items::iconStack(line->icon));
        if (noItem || !line->remaining()) continue;
        auto have = carried[line->item];
        if (have < line->remaining()) out.push_back({line, line->remaining() - have});
    }
    return out;
}
void pickInPreview(glm::vec2 at) {
    auto& t = previewTurn();
    t.picked = schematic::preview::pickAt(at.x * screen::scale(), at.y * screen::scale());
    t.pickedFile = t.file;
    if (schematicTab != SchematicTab::Verify || ignoreView || checkRows.several() || !t.picked) return;
    auto const* checked = checkedPlacement();
    if (!checked) return;
    auto world = checkCellWorld(*checked, *t.picked);
    int row = -1;
    for (size_t i = 0; i < verifyRows.size(); ++i)
        if (!verifyRows[i]->entity && verifyRows[i]->position == world) { row = static_cast<int>(i); break; }
    selectCheckRow(row);
    if (verifySelected >= 0) scrollToSelected = true;
}
// The Check list's selection pane (L-183 with L-190): Show in world marks
// every selected cell; Ignore places adds the rows not ignored yet, all or
// nothing under the cap; Stop ignoring takes back the rows' places.
void bulkCheck(int at) {
    auto const actions = bulkActions(SelectionList::Check);
    auto const* checked = checkedPlacement();
    if (at < 0 || at >= static_cast<int>(actions.size()) || !checked || !checkBulk.press(actions[static_cast<size_t>(at)])) return;
    if (verification && verification->stale) { screen::setMessage(translated("schematic.toast.counting")); return; }
    auto rows = selectedCheckRows();
    switch (actions[static_cast<size_t>(at)]) {
    case BulkAction::ShowInWorld: showSelectedMismatch(); return;
    case BulkAction::IgnorePlaces: {
        auto wanted = schematic::placesToAdd(rows, checked->ignored);
        if (wanted.empty()) return;
        bool full = false;
        changeIgnores([&](schematic::IgnoreList& list) {
            auto added = list.addAll(wanted);
            full = added == schematic::IgnoreList::Added::Full;
            return added == schematic::IgnoreList::Added::Added;
        });
        if (full) screen::setMessage(translated("schematic.ignore.full", schematic::maxIgnored));
        else screen::clearMessage();
        return;
    }
    case BulkAction::Unignore: {
        auto own = schematic::placesToRemove(rows, checked->ignored);
        if (own.empty()) return;
        changeIgnores([&](schematic::IgnoreList& list) {
            return list.removeIf([&](auto const& e) { return std::find(own.begin(), own.end(), e) != own.end(); }) > 0;
        });
        screen::clearMessage();
        return;
    }
    default: return;
    }
}
void handleSchematicClick(float x, float y, bool right) {
    screen::finishEditing();
    // A right click steps a stepper down and presses nothing else (L-346).
    if (right) {
        if (schematicsDisplayed.usable() && schematicTabAt(schematicsDisplayed, x, y) < 0)
            if (auto hit = schematicsDisplayed.hit(x, y); hit.zone == ShapeZone::Field) activateSchematicField(hit.index, -1);
        return;
    }
    if (auto pressed = right ? data_notice::Pressed::Outside : data_notice::press(x, y); pressed != data_notice::Pressed::Outside) {
        if (pressed == data_notice::Pressed::Open) screen::showSavedData();
        else refresh(true);
        if (auto done = data_notice::takeResult()) done->warns ? screen::setMessage(done->text) : screen::setInfo(done->text);
        // "Use this data" swaps the placements under the rows: nothing armed for the old ones stays armed.
        schematicDeleteArmed = verifyUndoAllArmed = false;
        placementBulk.disarm();
        checkBulk.disarm();
        return;
    }
    if (schematicsDisplayed.hit(x, y).zone != ShapeZone::Bulk) placementBulk.disarm();
    // Too short for the body: only the sidebar, or docked Close and Expand, are drawn.
    if (!schematicsDisplayed.usable()) {
        if (!schematicsDocked && screen::navClick(x, y)) return;
        auto zone = schematicsDisplayed.hit(x, y).zone;
        if (zone == ShapeZone::Close) screen::close();
        else if (zone == ShapeZone::Dock) schematicsDocked = !schematicsDocked;
        return;
    }
    if (!right && screen::pressScrollbar(schematicsDisplayed, schematicListFirst, x, y)) return;
    // The Check pane's item lines have their own bar (L-183).
    if (!right && itemScroll.w > 0 && screen::pressScrollbar(itemScroll.bar, x, y,
            [](float pos) { itemScroll.first = static_cast<int>(std::lround(pos)); }, &itemScroll.first))
        return;
    auto& t = previewTurn();
    // The slice line across the top of the preview shows it whole again.
    if (!right && t.w > 0 && t.peel > 0 && x >= t.x && x < t.x + t.w && y >= t.y && y < t.y + 16) { t.peel = 0; return; }
    if (!right && t.w > 0 && x >= t.x && x < t.x + t.w && y >= t.y && y < t.y + t.h) {
        t.dragging = true;
        t.turning = false;
        t.from = {x, y};
        t.fromYaw = t.yaw;
        t.fromPitch = t.pitch;
        return;
    }
    if (!schematicsDocked && screen::navClick(x, y)) return;
    if (int tab = schematicTabAt(schematicsDisplayed, x, y); tab >= 0) { selectSchematicTab(static_cast<SchematicTab>(tab)); return; }
    if (auto const& b = ignoreSettingsBox; schematicTab == SchematicTab::Verify && checkedPlacement() && b.w > 0
        && x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h) {
        toggleIgnoreView();
        return;
    }
    if (int chip = verifyChipAt(schematicsDisplayed, x, y); chip >= 0) {
        checkFilter = static_cast<schematic::CheckFilter>(chip);
        checkRows.clear();
        followCheckRows();
        schematicListFirst = 0;
        refreshSchematics(false);
        return;
    }
    if (schematicTab == SchematicTab::Verify && pressIgnoreControl(x, y)) return;
    verifyUndoAllArmed = false;
    auto hit = schematicsDisplayed.hit(x, y);
    if (!(hit.zone == ShapeZone::Action && hit.index == 1)) schematicDeleteArmed = false;
    switch (hit.zone) {
    case ShapeZone::Close: screen::close(); return;
    case ShapeZone::Dock: schematicsDocked = !schematicsDocked; return;
    case ShapeZone::Keys: openSchematicKeySettings(); return;
    case ShapeZone::DrawAll:
        screen::toggleOption("schematic.enabled");
        return;
    case ShapeZone::ListRow: {
        int row = hit.index;
        auto const& l = schematicsDisplayed;
        switch (schematicTab) {
        case SchematicTab::Placements:
            if (listColumns(l.listLeft, l.contentWidth(), progressColumn, switchWidth).onShown(x)) {
                schematic::session::change([&](schematic::PlacementSet& set) {
                    if (row < 0 || row >= static_cast<int>(set.placements.size())) return false;
                    set.placements[static_cast<size_t>(row)].visible = !set.placements[static_cast<size_t>(row)].visible;
                    return true;
                });
                refreshSchematics(false);
                return;
            }
            if (screen::heldCtrl() || screen::heldShift()) { pickPlacements(row); return; }
            if (placementRows.several() || schematicPick != SchematicPick::Placement || row != schematicIndex) pickSchematic(SchematicPick::Placement, row);
            return;
        case SchematicTab::Files: {
            if (row < 0 || row >= static_cast<int>(schematicFileRows.size())) return;
            int file = schematicFileRows[static_cast<size_t>(row)].file;
            if (file >= 0 && (schematicPick != SchematicPick::File || file != schematicIndex)) pickSchematic(SchematicPick::File, file);
            return;
        }
        case SchematicTab::Verify:
            if (ignoreView) {
                if (row < 0 || row >= static_cast<int>(ignoreList.size()) || !ignoreList[static_cast<size_t>(row)].selectable()) return;
                auto const& picked = ignoreList[static_cast<size_t>(row)];
                ignorePick = IgnorePick{picked.kind, picked.entry, picked.block};
                return;
            }
            if (row < 0 || row >= static_cast<int>(verifyRows.size())) return;
            // Ctrl + click toggles a row, Shift + click takes a range (L-190).
            if (screen::heldCtrl() || screen::heldShift()) {
                checkRows.click(checkKeyOf(row), screen::heldCtrl(), screen::heldShift(), checkOrder());
                followCheckRows();
            } else selectCheckRow(row);
            return;
        default:
            if (row >= 0 && row < static_cast<int>(materialListRows.size()) && materialListRows[static_cast<size_t>(row)] >= 0)
                materialSelected = materialListRows[static_cast<size_t>(row)];
            return;
        }
    }
    case ShapeZone::Field: activateSchematicField(hit.index, right ? -1 : hit.part); return;
    case ShapeZone::Bulk: {
        auto const actions = bulkActions(SelectionList::Placements);
        int at = hit.index;
        if (schematicTab == SchematicTab::Placements && placementRows.several() && at >= 0 && at < static_cast<int>(actions.size())
            && placementBulk.press(actions[static_cast<size_t>(at)]))
            bulkPlacements(actions[static_cast<size_t>(at)]);
        if (schematicTab == SchematicTab::Verify && checkRows.several()) bulkCheck(at);
        return;
    }
    case ShapeZone::Action:
        if (schematicTab == SchematicTab::Files) {
            if (hit.index == 0) placeSelectedFile();
            else if (!schematic::session::openFolder()) screen::setMessage(translated("schematic.openFolderError"));
            return;
        }
        // The Check pane's buttons are its own controls (pressIgnoreControl).
        if (schematicTab == SchematicTab::Verify) return;
        if (schematicTab == SchematicTab::Materials) {
            if (hit.index != 0) return;
            auto items = missingMaterials();
            if (items.empty()) return;
            std::vector<std::pair<std::string, std::uint64_t>> wanted;
            for (auto const& m : items) wanted.push_back({m.line->item, m.missing});
            if (!openUrl(schematic::calculatorUrl(wanted))) screen::setMessage(translated("worldMap.linkFailed"));
            return;
        }
        if (schematicTab != SchematicTab::Placements || hit.index != 1 || !selectedPlacement()) return;
        if (!schematicDeleteArmed) { schematicDeleteArmed = true; return; }
        deleteSelectedPlacement();
        return;
    default: return;
    }
}
void handleSchematicKey(int key) {
    if (editingSchematicField >= 0) {
        screen::numberKey(key);
        return;
    }
    if (schematicTab == SchematicTab::Verify && ignoreView)
        switch (key) {
        case 0x1b: toggleIgnoreView(); return;
        case 0x21: case 0x22: stepIgnorePick(key == 0x22 ? 1 : -1); return;
        case 0x0d: case 0x20: case 0x41: return;
        default: break;
        }
    if (schematicTab == SchematicTab::Verify)
        switch (selectionKey(key, screen::heldCtrl(), checkRows.several())) {
        case SelectionKey::SelectAll: checkRows.selectAll(checkOrder()); followCheckRows(); return;
        case SelectionKey::Clear: checkRows.clear(); followCheckRows(); return;
        case SelectionKey::None: break;
        }
    if (schematicTab == SchematicTab::Placements)
        switch (selectionKey(key, screen::heldCtrl(), placementRows.several())) {
        case SelectionKey::SelectAll: placementRows.selectAll(placementOrder()); followPlacementRows(); return;
        case SelectionKey::Clear: placementRows.clear(); followPlacementRows(); return;
        case SelectionKey::None: break;
        }
    switch (auto const pressed = viewKey(key); pressed) {
    case ViewKey::Close: screen::close(); break;
    case ViewKey::FieldUp: moveSchematicField(-1); break;
    case ViewKey::FieldDown: moveSchematicField(1); break;
    case ViewKey::StepDown: activateSchematicField(schematicFieldSelected, -1); break;
    case ViewKey::StepUp: activateSchematicField(schematicFieldSelected, 1); break;
    case ViewKey::Activate:
        if (schematicTab == SchematicTab::Files) placeSelectedFile();
        else if (schematicTab == SchematicTab::Verify) showSelectedMismatch();
        else activateSchematicField(schematicFieldSelected, 0);
        break;
    case ViewKey::RowUp: case ViewKey::RowDown: {
        int count = schematicRowCount();
        if (!count) break;
        int current = schematicTab == SchematicTab::Verify ? verifySelected
            : (schematicTab == SchematicTab::Placements && schematicPick == SchematicPick::Placement)
                || (schematicTab == SchematicTab::Files && schematicPick == SchematicPick::File) ? schematicIndex : -1;
        int step = pressed == ViewKey::RowDown ? 1 : -1;
        if (schematicTab == SchematicTab::Files) {
            // Rows and files differ: step over folder headings.
            if (int file = schematic_files::stepFile(schematicFileRows, current, step); file >= 0) pickSchematic(SchematicPick::File, file);
            break;
        }
        int row = std::clamp(current + step, 0, count - 1);
        if (schematicTab == SchematicTab::Verify) selectCheckRow(row);
        else if (schematicTab == SchematicTab::Placements) pickSchematic(SchematicPick::Placement, row);
        break;
    }
    case ViewKey::NextNav: screen::nextNav(screen::heldShift()); break;
    case ViewKey::None: break;
    }
}
std::string fileTitle(std::string const& relative) {
    auto slash = relative.find_last_of('/');
    auto name = relative.substr(slash == std::string::npos ? 0 : slash + 1);
    if (name.ends_with(".mcstructure")) name.resize(name.size() - std::string_view(".mcstructure").size());
    return name;
}
std::string schematicDescription() {
    switch (schematicTab) {
    case SchematicTab::Placements:
        if (placementRows.several()) return translated("list.selectedCount", placementRows.keys.size());
        if (auto const* p = selectedPlacement()) {
            if (editingSchematicField >= 0) return translated("integerRange", editingSchematicField == 9 ? 1 : -30'000'000,
                editingSchematicField == 9 ? 4096 : 30'000'000);
            if (p->dimension != screen::playerDimension()) return p->name + ": " + translated("schematic.elsewhere");
            return translated("schematic.placedHint", p->name);
        }
        return translated(schematicSet.placements.empty() ? "schematic.noPlacements" : "schematic.selectHint");
    case SchematicTab::Files:
        if (schematicPick == SchematicPick::File && schematicIndex >= 0 && schematicIndex < static_cast<int>(schematicFiles.size()))
            return translated("schematic.fileHint", fileTitle(schematicFiles[static_cast<size_t>(schematicIndex)].relative));
        return translated(schematicFiles.empty() ? "schematic.empty" : "schematic.fileSelectHint");
    case SchematicTab::Verify:
        if (ignoreView) return translated("schematic.ignore.viewHint");
        if (checkRows.several()) return translated("list.selectedCount", checkRows.keys.size());
        return translated("schematic.verifyHint");
    default: return translated("schematic.materialsHint");
    }
}
std::string fieldValue(schematic::SavedPlacement const& p, SchematicField field) {
    bool layerField = field == SchematicField::LayerAxis || field == SchematicField::LayerMode
        || field == SchematicField::Layer || field == SchematicField::Keep;
    // Without the size a legacy view cannot be resolved, and even a new-form
    // cut has no extent; show no guessed values (L-242).
    if (layerField && !schematic::session::structure(p.file)) return "—";
    switch (field) {
    case SchematicField::X: return std::to_string(p.placement.origin.x);
    case SchematicField::Y: return std::to_string(p.placement.origin.y);
    case SchematicField::Z: return std::to_string(p.placement.origin.z);
    case SchematicField::Rotation: return std::format("{}°", p.placement.rotation * 90);
    case SchematicField::Mirror:
        return translated(p.placement.mirror == schematic::Mirror::X ? "schematic.mirror.x"
            : p.placement.mirror == schematic::Mirror::Z ? "schematic.mirror.z" : "schematic.mirror.none");
    case SchematicField::LayerAxis: return schematic::actions::axisText(shownLayers(p).axis);
    case SchematicField::LayerMode: return schematic::actions::modeText(shownLayers(p).mode);
    case SchematicField::Layer: return translated("schematic.layerValue", shownLayers(p).cut + 1, placedLayers(p));
    case SchematicField::Keep: return schematic::actions::keepText(shownLayers(p).keep);
    case SchematicField::Extras: return translated(p.countExtras ? "schematic.extras.show" : "schematic.extras.ignore");
    default: return {};
    }
}
// Drawn for every list row each frame: resolve the view once.
std::string layersText(schematic::SavedPlacement const& p) {
    if (schematic::showsAllLayers(p.layers)) return translated("schematic.mode.all");
    auto structure = schematic::session::structure(p.file);
    if (!structure) return translated("schematic.layerUnavailable");
    auto view = schematic::resolvedLayers(p.layers, structure->size, p.placement);
    int count = std::max(1, schematic::layerCount(structure->size, view.axis));
    std::string text = schematic::actions::axisText(view.axis) + " " + translated("schematic.layerValue", view.cut + 1, count)
        + " " + schematic::actions::modeText(view.mode);
    if (view.mode == schematic::LayerMode::UpTo) text += " " + schematic::actions::keepText(view.keep);
    return text;
}
void drawItemIcon(MinecraftUIRenderContext& context, std::string const& icon, float x, float y, float size) {
    auto const* stack = schematic::items::iconStack(icon);
    if (!stack) return;
    if (information::drawWaterIcon(context, *stack, x, y, size)) return; // L-294, in widget units
    // Item icons and the 3D preview draw in GUI units, outside the widgets' scale.
    float s = drawScale();
    inspection::render::drawItemIcon(context, {stack, std::round(x * s), std::round(y * s), size * s / 16}, 17);
}
std::map<std::string, std::uint64_t> carriedItems() {
    auto* player = screen::client() ? screen::client()->getLocalPlayer() : nullptr;
    // With what the counted places of Recipes hold: one count for the plan and for this tab (L-359).
    return player ? recipes::state::heldByName(*player) : std::map<std::string, std::uint64_t>{};
}
Rgb mismatchColor(schematic::CellState state) {
    switch (state) {
    case schematic::CellState::Wrong: case schematic::CellState::Extra: return palette::mismatch;
    case schematic::CellState::State: return palette::wrongState;
    default: return palette::notPlaced;
    }
}
// A row's kind as the filters name it: an extra is a wrong block (L-183).
std::string mismatchKind(schematic::CellState state) {
    switch (state) {
    case schematic::CellState::Wrong: case schematic::CellState::Extra: return translated("schematic.filter.wrong");
    case schematic::CellState::State: return translated("schematic.filter.state");
    default: return translated("schematic.filter.missing");
    }
}
// The map's placement color, for the Placed list's footprint marks (L-190).
Rgb placementRgb() {
    auto c = map::placementColor;
    return {map::channel(c, 0) / 255.f, map::channel(c, 1) / 255.f, map::channel(c, 2) / 255.f};
}
// Where the distance column measures to: the footprint's center as the map
// lists it, or the origin while the file is not read yet.
std::pair<int, int> placementCenter(schematic::SavedPlacement const& p) {
    auto const& origin = p.placement.origin;
    auto structure = schematic::session::loaded(p.file);
    if (!structure) return {origin.x, origin.z};
    auto area = schematic::footprint(structure->size, p.placement);
    return {static_cast<int>(std::floor((area.x0 + area.x1) / 2.0)), static_cast<int>(std::floor((area.z0 + area.z1) / 2.0))};
}
// One draw of the Schematics view's body (L-399): what its parts share this
// frame. Built by drawSchematicsBody, read and added to by the parts.
struct Body {
    MinecraftUIRenderContext& context;
    ShapesLayout const& l;
    glm::vec2 pointer;
    ShapesLayout::Hit hover;
    int tabHover = -1;
    float left = 0, listRight = 0;
    schematic::SavedPlacement const* checked = nullptr;
    // A placement whose file cannot be loaded is never counted: say why instead.
    std::string loadProblem;
    bool unloadable = false, checkOff = false, counting = false;
    schematics_rows::ListPlaces places;
    ListColumns placed;
    Vec3 feet{0, 0, 0};
    float dx = 0, dw = 0; // the detail pane's text column
    // A tip under the pointer: a Materials count in chests and stacks, or a
    // Check row's name that was cut.
    struct Tip { std::string text; float x = 0, y = 0; };
    std::optional<Tip> listTip;
    std::string fieldHelp; // what the field row under the pointer does, for the footer (2026-11-22)
    bool over(ShapeZone zone, int index = -1) const { return tabHover < 0 && hover.zone == zone && (index < 0 || hover.index == index); }
};
void drawSchematicTabs(Body& b) {
    auto& context = b.context;
    auto const& l = b.l;
    int const tabHover = b.tabHover;
    static constexpr std::array<std::string_view, schematicTabCount> tabNames{"schematic.tab.files", "schematic.tab.placements",
        "schematic.tab.verify", "schematic.tab.materials"};
    for (int i = 0; i < schematicTabCount; ++i) {
        bool active = static_cast<int>(schematicTab) == i;
        float x = schematicTabX(l, i), w = schematicTabWidth(l);
        drawTab(context,x,l.toolbarTop+2,w,translated(tabNames[static_cast<size_t>(i)]),active,tabHover == i,i == 0);
    }
    tabRule(context,schematicTabX(l,0),l.toolbarTop+2+tabHeight,schematicTabX(l,schematicTabCount-1)+schematicTabWidth(l)-schematicTabX(l,0));
    fill(context,l.listLeft,l.theadTop-1,l.listWidth,1,palette::white,.14f);
}
void drawSchematicHeadings(Body& b) {
    auto& context = b.context;
    auto const& l = b.l;
    auto const pointer = b.pointer;
    float const left = b.left, listRight = b.listRight;
    auto const& placed = b.placed;
    auto const& [blocksW, blocksX, sizeW, sizeX, showSize, distW, numW, carriedX, leftX, placedX, neededX, showPlaced] = b.places;
    auto heading = [&](float x, float w, std::string_view key, Align align = Align::Left) {
        label(context,x,l.theadTop+2,w,translated(key),palette::faint,align);
    };
    switch (schematicTab) {
    case SchematicTab::Placements:
        drawListHeadings(context,placed,l.theadTop,"schematic.column.progress");
        break;
    case SchematicTab::Files:
        heading(left, (showSize ? sizeX : blocksX) - left - 4, "shape.columnName");
        if (showSize) heading(sizeX, sizeW, "schematic.column.size", Align::Right);
        heading(blocksX, blocksW, "schematic.column.blockCount", Align::Right);
        break;
    case SchematicTab::Verify: {
        // The filters replace the column headings (L-183): one of four, the
        // kind's square as the rows' legend, no counts.
        static constexpr std::array<std::string_view, 4> chips{"schematic.filter.all", "schematic.filter.wrong",
            "schematic.filter.state", "schematic.filter.missing"};
        static constexpr std::array<schematic::CellState, 4> chipKinds{schematic::CellState::Correct, schematic::CellState::Wrong,
            schematic::CellState::State, schematic::CellState::Missing};
        float const chipsEnd = listRight - ShapesLayout::pad;
        verifyChipSpans = {};
        float cx = left;
        for (int i = 0; i < 4 && !ignoreView; ++i) {
            bool on = static_cast<int>(checkFilter) == i, hovered = verifyChipAt(l,pointer.x,pointer.y) == i;
            std::string text = translated(chips[static_cast<size_t>(i)]);
            constexpr float chipText = .8f;
            float square = i > 0 ? 8.f : 0.f;
            float w = std::min(textWidthScaled(context, text, chipText) + 8 + square, chipsEnd - cx);
            if (w <= 8) { verifyChipSpans[static_cast<size_t>(i)] = {}; continue; }
            float top = l.theadTop + 1, h = ShapesLayout::theadHeight - 2;
            if (on) fill(context,cx,top,w,h,palette::accent,.15f);
            if (hovered) fill(context,cx,top,w,h,palette::white,.08f);
            frame(context,cx,top,w,h,on ? palette::accent : palette::keyEdge);
            if (square > 0) drawKindSquare(context,cx+4,top+h/2,mismatchColor(chipKinds[static_cast<size_t>(i)]),false,5);
            labelScaled(context,cx+4+square,top+(h-8*chipText)/2-1+boxTextInset(),w-6-square,std::move(text),chipText,on ? palette::accent : palette::dim);
            verifyChipSpans[static_cast<size_t>(i)] = {cx, cx + w};
            cx += w + 4;
        }
        break;
    }
    case SchematicTab::Materials:
        heading(left + 14, neededX - left - 16, "schematic.column.material");
        heading(neededX, numW, "schematic.column.needed", Align::Right);
        if (showPlaced) heading(placedX, numW, "schematic.column.placed", Align::Right);
        heading(leftX, numW, "schematic.column.left", Align::Right);
        heading(carriedX, numW, "schematic.column.carried", Align::Right);
        break;
    }
}
void drawSchematicRows(Body& b) {
    auto& context = b.context;
    auto const& l = b.l;
    auto const pointer = b.pointer;
    auto const& hover = b.hover;
    int const tabHover = b.tabHover;
    auto over = [&](ShapeZone zone, int index = -1) { return b.over(zone, index); };
    float const left = b.left, listRight = b.listRight;
    auto const* checked = b.checked;
    auto const& loadProblem = b.loadProblem;
    bool const unloadable = b.unloadable, checkOff = b.checkOff, counting = b.counting;
    auto const& placed = b.placed;
    auto const& [blocksW, blocksX, sizeW, sizeX, showSize, distW, numW, carriedX, leftX, placedX, neededX, showPlaced] = b.places;
    auto const& feet = b.feet;
    auto& listTip = b.listTip;
    bool loadedOne = false; // At most one file is read per frame to fill the columns.
    fill(context,l.listLeft,l.rowsTop-1,l.listWidth,1,palette::white,.14f);
    std::string empty;
    if (l.listCount == 0) {
        if (schematicTab == SchematicTab::Placements) empty = translated("schematic.noPlacements");
        else if (schematicTab == SchematicTab::Files) empty = translated("schematic.empty");
        else if (!checked) empty = translated("schematic.noSelection");
        else if (unloadable) empty = translated("schematic.notLoaded", loadProblem);
        else if (checkOff) empty = translated("schematic.offCheck");
        else if (counting) empty = translated("schematic.counting");
        else empty = translated(schematicTab == SchematicTab::Verify ? "schematic.noMistakes" : "schematic.noMaterials");
        paragraph(context,left,l.rowsTop+3,l.listWidth-2*ShapesLayout::pad,empty,4,palette::faint);
    }
    auto carried = schematicTab == SchematicTab::Materials ? carriedItems() : std::map<std::string, std::uint64_t>{};
    // The ignore view's row (L-183): a heading, "None", a place (the row's
    // kind square, hollow; its block; a tag; the distance) or a block type
    // (its icon and name, tagged with the items ruled on it).
    auto const ignoreStructure = checked && ignoreView ? schematic::session::loaded(checked->file) : nullptr;
    auto distanceText = [&](schematic::Point p) {
        double ddx = p.x + .5 - feet.x, ddy = p.y + .5 - feet.y, ddz = p.z + .5 - feet.z;
        return translated("waypoint.meters", static_cast<int>(std::lround(std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz))));
    };
    auto drawIgnoreRow = [&](schematic::IgnoreRow const& row, float y, bool hovered) {
        using K = schematic::IgnoreRow::Kind;
        float nameX = left + 9, right = listRight - ShapesLayout::pad;
        if (row.kind == K::PlacesHeading || row.kind == K::BlocksHeading) {
            label(context,left,y+3,right-left,translated(row.kind == K::PlacesHeading ? "schematic.ignore.placesHeading"
                : "schematic.ignore.blocksHeading"),palette::faint);
            return;
        }
        if (row.kind == K::NoPlaces) { label(context,nameX,y+3,right-nameX,translated("schematic.ignore.none"),palette::faint); return; }
        std::string name, icon, tag;
        if (row.kind == K::Place) {
            auto const* m = row.stale ? nullptr : rowAt(row.entry);
            drawKindSquare(context,left,y+ShapesLayout::rowHeight/2,m ? mismatchColor(m->state) : palette::faint,true);
            if (row.entry.block == "minecraft:air") name = m && !m->actualName.empty() ? m->actualName : translated("schematic.air");
            else { auto const& label = fileBlockLabel(ignoreStructure, row.entry.block); name = label.name; icon = label.icon; }
            tag = translated(row.entry.kind == schematic::IgnoreKind::CellItem ? "schematic.ignore.oldForm"
                : row.stale ? "schematic.ignore.stale" : "schematic.ignore.tagPlace");
            if (ignoreStructure) {
                float distW = 28;
                label(context,right-distW,y+3,distW,distanceText(schematic::toWorld(ignoreStructure->size, checked->placement, row.entry.cell)),
                    palette::dim,Align::Right);
                right -= distW + 4;
            }
        } else {
            auto const& label = fileBlockLabel(ignoreStructure, row.block);
            name = label.name;
            if (!row.stale) icon = label.icon;
            if (!row.rules.empty()) {
                std::string items;
                for (auto const& item : row.rules)
                    items += (items.empty() ? "" : translated("schematic.ignore.itemJoin")) + schematic::ghosts::itemName(item, row.block);
                tag = translated("schematic.ignore.ruledTag", items);
            }
            if (row.stale) tag += (tag.empty() ? "" : " · ") + translated("schematic.ignore.stale");
        }
        if (!tag.empty()) {
            float full = tagWidth(context, tag), tagW = std::min(full, (right - nameX) / 2);
            if (hovered && full > tagW && pointer.x >= right - tagW && pointer.x < right) listTip = {tag, pointer.x, pointer.y};
            drawTag(context,right-tagW,y+2,tag,tagW);
            right -= tagW + 4;
        }
        if (!icon.empty()) drawItemIcon(context, icon, nameX, y + 1, 12);
        float tx = nameX + 14, tw = right - tx;
        label(context,tx,y+3,tw,name,row.stale ? palette::faint : palette::text);
        if (hovered && pointer.x >= tx && pointer.x < tx + tw && textWidth(context, name) > tw) listTip = {name, pointer.x, pointer.y};
    };
    for (int i = l.listFirst; i < l.listFirst + l.listVisible && i < l.listCount; ++i) {
        float y = l.listRowY(i);
        if (i % 2) fill(context,l.listLeft+1,y,l.rowWidth(),ShapesLayout::rowHeight,palette::white,.025f);
        bool chosen = schematicTab == SchematicTab::Verify ? (ignoreView ? ignorePick && ignorePick->names(ignoreList[static_cast<size_t>(i)])
                                                                         : checkRows.contains(checkKeyOf(i)))
            : schematicTab == SchematicTab::Placements ? placementRows.contains(schematicSet.placements[static_cast<size_t>(i)].id)
            : schematicTab == SchematicTab::Files ? schematicPick == SchematicPick::File
                && schematicFileRows[static_cast<size_t>(i)].file == schematicIndex : false;
        bool heading = (schematicTab == SchematicTab::Files && schematicFileRows[static_cast<size_t>(i)].file < 0)
            || (schematicTab == SchematicTab::Materials
                && materialListRows[static_cast<size_t>(i)] < 0)
            || (schematicTab == SchematicTab::Verify && ignoreView && !ignoreList[static_cast<size_t>(i)].selectable());
        if (schematicTab == SchematicTab::Materials && !heading)
            chosen = materialListRows[static_cast<size_t>(i)] == materialSelected;
        if (!heading) rowBackground(context,l.listLeft+1,y,l.rowWidth(),ShapesLayout::rowHeight,chosen,over(ShapeZone::ListRow,i));
        switch (schematicTab) {
        case SchematicTab::Placements: {
            auto const& p = schematicSet.placements[static_cast<size_t>(i)];
            bool here = p.dimension == screen::playerDimension();
            // The selected placement: the accent bar, like the sidebar's current entry.
            if (i == schematicSet.selected) fill(context,l.listLeft+1,y,2,ShapesLayout::rowHeight,palette::accent);
            drawFootprintGlyph(context,placed.markX+1,y+ShapesLayout::rowHeight/2,placementRgb(),p.visible && here ? 1.f : .4f);
            float nameEnd = placed.nameX + placed.nameWidth;
            float nameX = placed.nameX;
            bool unreadable = !schematic::session::loadError(p.file).empty();
            if (unreadable) {
                float markW = textWidth(context,"! ");
                label(context,nameX,y+3,markW,"!",palette::warning);
                nameX += markW;
            }
            label(context,nameX,y+3,nameEnd-nameX,p.name,unreadable ? palette::warning : here ? palette::text : palette::faint);
            if (placed.hasDistance()) {
                auto center = placementCenter(p);
                label(context,placed.distanceX,y+3,placed.distanceWidth,listDistanceText(center.first, center.second, p.dimension),palette::dim,Align::Right);
            }
            if (auto tally = placed.hasOwn() ? schematic::ghosts::progress(p) : std::nullopt; tally && tally->total()) {
                float fraction = static_cast<float>(tally->correct) / static_cast<float>(tally->total());
                label(context,placed.ownX,y+1,placed.ownWidth,std::format("{}%", static_cast<int>(fraction * 100)),
                    here ? palette::text : palette::faint,Align::Right);
                fill(context,placed.ownX,y+11,placed.ownWidth,1,palette::white,.12f);
                fill(context,placed.ownX,y+11,placed.ownWidth*fraction,1,palette::accent,here ? 1.f : .5f);
            } else if (placed.hasOwn()) label(context,placed.ownX,y+3,placed.ownWidth,"-",palette::faint,Align::Right);
            toggleSwitch(context,placed.shownX,y+(ShapesLayout::rowHeight-switchHeight)/2,p.visible);
            break;
        }
        case SchematicTab::Files: {
            auto const& row = schematicFileRows[static_cast<size_t>(i)];
            if (row.file < 0) {
                label(context,left,y+3,l.listWidth-2*ShapesLayout::pad,"/" + row.folder,
                      palette::faint);
                break;
            }
            auto const& f = schematicFiles[static_cast<size_t>(row.file)];
            float indent = !schematicFileRows.empty() && schematicFileRows.front().file < 0 ? 6.f : 0.f;
            label(context,left+indent,y+3,(showSize ? sizeX : blocksX)-left-indent-4,fileTitle(f.relative),palette::text);
            auto structure = schematic::session::loaded(f.relative);
            if (!structure && !loadedOne && f.bytes <= schematic::session::largeFileBytes) {
                loadedOne = true;
                structure = schematic::session::structure(f.relative);
            }
            if (structure) {
                if (showSize) label(context,sizeX,y+3,sizeW,std::format("{}x{}x{}", structure->size.x, structure->size.y, structure->size.z),
                    palette::dim,Align::Right);
                label(context,blocksX,y+3,blocksW,std::to_string(blockCount(structure)),palette::dim,Align::Right);
            } else label(context,blocksX,y+3,blocksW,"-",palette::faint,Align::Right);
            break;
        }
        case SchematicTab::Verify: {
            if (ignoreView) {
                drawIgnoreRow(ignoreList[static_cast<size_t>(i)], y, tabHover < 0 && hover.zone == ShapeZone::ListRow && hover.index == i);
                break;
            }
            auto const& m = *verifyRows[static_cast<size_t>(i)];
            // L-183: the kind's square (hollow while ignored), the file's block
            // (-> what is there for a wrong block), a framed tag, the distance.
            // No kind word (the filters are the legend) and no position.
            auto const mark = checked ? schematic::liveMark(m, checked->ignored) : m.ignore;
            bool const rowHovered = tabHover < 0 && hover.zone == ShapeZone::ListRow && hover.index == i;
            drawKindSquare(context,left,y+ShapesLayout::rowHeight/2,mismatchColor(m.state),mark != schematic::IgnoreMark::None);
            float bx = left + 9, bw = listRight - bx - distW - ShapesLayout::pad - 4;
            if (mark != schematic::IgnoreMark::None) {
                std::string tag = translated(mark == schematic::IgnoreMark::Place ? "schematic.ignore.tagPlace" : "schematic.ignore.tagItems");
                float full = tagWidth(context, tag), tagW = std::min(full, bw / 2);
                if (rowHovered && full > tagW && pointer.x >= bx + bw - tagW && pointer.x < bx + bw) listTip = {tag, pointer.x, pointer.y};
                drawTag(context,bx+bw-tagW,y+2,std::move(tag),tagW);
                bw -= tagW + 4;
            }
            // A name and its icon; a cut name shows whole under the pointer.
            auto block = [&](float x, float w, std::string const& icon, std::string const& name, Rgb color) {
                if (!icon.empty()) drawItemIcon(context, icon, x, y + 1, 12);
                label(context,x+14,y+3,w-14,name,color);
                if (rowHovered && pointer.x >= x && pointer.x < x + w && textWidth(context, name) > w - 14)
                    listTip = {name, pointer.x, pointer.y};
            };
            bool const changed = m.state == schematic::CellState::Wrong || m.state == schematic::CellState::Extra;
            if (m.entity) block(bx, bw, m.expected, translated("schematic.withDetail", m.expectedName, translated("schematic.entityTag")), palette::text);
            else if (!changed) block(bx, bw, m.expected, m.expectedName, palette::text);
            else {
                float half = (bw - 10) / 2;
                // An extra: air where the file's block would be.
                if (m.state == schematic::CellState::Extra) block(bx, half, {}, translated("schematic.air"), palette::faint);
                else block(bx, half, m.expected, m.expectedName, palette::text);
                changeArrow(context,bx+half+(10-changeArrowWidth)/2,y+3+1.5f+shapeTextDrop(),1,palette::faint);
                block(bx + half + 10, half, m.actual, m.actualName, palette::dim);
            }
            double dx = m.position.x + .5 - feet.x, dy = m.position.y + .5 - feet.y, dz = m.position.z + .5 - feet.z;
            label(context,listRight-ShapesLayout::pad-distW,y+3,distW,
                translated("waypoint.meters", static_cast<int>(std::lround(std::sqrt(dx*dx + dy*dy + dz*dz)))),palette::dim,Align::Right);
            break;
        }
        case SchematicTab::Materials: {
            int index = materialListRows[static_cast<size_t>(i)];
            if (index < 0) {
                label(context,left,y+3,l.listWidth-2*ShapesLayout::pad,translated(index == -1 ? "schematic.section.blocks"
                    : "schematic.section.entities"),palette::faint);
                break;
            }
            auto const& line = *materialRows[static_cast<size_t>(index)];
            // The counts convert to chests and stacks under the pointer.
            if (tabHover < 0 && hover.zone == ShapeZone::ListRow && hover.index == i) {
                bool noItemHere = line.item.empty() || (line.entity && !schematic::items::iconStack(line.icon));
                std::optional<std::pair<std::string_view, std::uint64_t>> column;
                if (pointer.x >= carriedX) { if (!noItemHere) column = {{"schematic.column.carried", carried[line.item]}}; }
                else if (pointer.x >= leftX) column = {{"schematic.column.left", line.remaining()}};
                else if (showPlaced && pointer.x >= placedX) column = {{"schematic.column.placed", line.placed}};
                else if (pointer.x >= neededX) column = {{"schematic.column.needed", line.needed}};
                if (column) listTip = {translated(column->first) + ": " + amountText(column->second, stackSizeOf(line)),
                    pointer.x, pointer.y};
            }
            // Entities: an icon and a carried count only when an item places them.
            bool noItem = line.item.empty() || (line.entity && !schematic::items::iconStack(line.icon));
            drawItemIcon(context, line.icon, left, y + 1, 12);
            // Under the Entities heading the name needs no "(entity)".
            bool sectioned = !materialListRows.empty() && materialListRows.front() < 0;
            std::string name = line.entity && !sectioned ? translated("schematic.withDetail", line.name, translated("schematic.entityTag")) : line.name;
            label(context,left+14,y+3,neededX-left-16,name,line.remaining() ? palette::text : palette::faint);
            auto have = noItem ? std::uint64_t{0} : carried[line.item];
            label(context,neededX,y+3,numW,std::to_string(line.needed),palette::dim,Align::Right);
            if (showPlaced) label(context,placedX,y+3,numW,std::to_string(line.placed),palette::dim,Align::Right);
            label(context,leftX,y+3,numW,std::to_string(line.remaining()),palette::text,Align::Right);
            Rgb haveColor = !line.remaining() ? palette::faint : have >= line.remaining() ? palette::accent : palette::warning;
            label(context,carriedX,y+3,numW,noItem ? "-" : std::to_string(have),noItem ? palette::faint : haveColor,Align::Right);
            break;
        }
        }
    }
    drawListScrollbar(context,l,screen::scrollHeld(&schematicListFirst) || l.onScrollbar(pointer.x,pointer.y));
    if (l.docked) fill(context,l.left,l.detailTop-1,l.width,1,palette::white,.14f);
    else fill(context,l.detailLeft-1,l.toolbarTop,1,l.footerTop-l.toolbarTop,palette::white,.14f);
}
// A file's facts in the detail pane: size, blocks, entities and its path.
void drawFileFacts(Body const& b, std::string const& relative, float y) {
    auto& context = b.context;
    float const dx = b.dx, dw = b.dw;
    std::string problem;
    auto structure = schematic::session::structure(relative, &problem);
    if (!structure) { paragraph(context,dx,y,dw,translated("schematic.notLoaded", problem),3,palette::warning); return; }
    std::uint64_t blocks = blockCount(structure);
    label(context,dx,y,dw,translated("schematic.size", structure->size.x, structure->size.y, structure->size.z),palette::dim);
    label(context,dx,y+11,dw,translated("schematic.blocks", blocks),palette::dim);
    label(context,dx,y+22,dw,translated("schematic.entities", structure->entities.size()),palette::dim);
    label(context,dx,y+33,dw,relative,palette::faint);
}
void drawPlacementDetail(Body& b) {
    auto& context = b.context;
    auto const& l = b.l;
    auto const pointer = b.pointer;
    auto over = [&](ShapeZone zone, int index = -1) { return b.over(zone, index); };
    float const dx = b.dx, dw = b.dw;
    auto info = [&](std::string const& relative, float y) { drawFileFacts(b, relative, y); };
    if (!l.selecting.empty()) {
        auto const pane = l.selectionPane(static_cast<int>(placementRows.keys.size()));
        std::vector<SelectedName> names;
        std::vector<schematic::SavedPlacement const*> marks;
        size_t shown = 0;
        for (auto const& p : schematicSet.placements) {
            if (!placementRows.contains(p.id)) continue;
            if (p.visible) ++shown;
            if (names.size() >= static_cast<size_t>(pane.fit.names)) continue;
            auto center = placementCenter(p);
            std::string progress = "-";
            if (auto tally = schematic::ghosts::progress(p); tally && tally->total())
                progress = std::format("{}%", static_cast<int>(100.0 * static_cast<double>(tally->correct) / static_cast<double>(tally->total())));
            names.push_back({p.name, p.dimension != screen::playerDimension(), listDistanceText(center.first, center.second, p.dimension), progress});
            marks.push_back(&p);
        }
        drawSelectionPane(context,pane,placementRows.keys.size(),names,[&](size_t i, float gx, float gy) {
            auto const& m = *marks[i];
            drawFootprintGlyph(context,gx-3,gy,placementRgb(),m.visible && m.dimension == screen::playerDimension() ? 1.f : .4f);
        },l.selecting,placementBulk,pointer.x,pointer.y,shown,progressColumn);
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
    } else if (auto const* p = selectedPlacement()) {
        label(context,dx,l.nameY+1+boxTextInset(),dw,p->name);
        info(p->file, l.previewY);
        if (auto tally = schematic::ghosts::progress(*p); tally && tally->total())
            label(context,dx,l.previewY+44,dw,translated("schematic.progress",
                static_cast<int>(100.0 * static_cast<double>(tally->correct) / static_cast<double>(tally->total()))),palette::dim);
        for (int i = l.fieldFirst; i < l.fieldFirst + l.fieldVisible && i < static_cast<int>(schematicFields.size()); ++i) {
            float y = l.fieldY(i);
            auto field = schematicFields[static_cast<size_t>(i)];
            rowBackground(context,l.detailLeft+1,y,l.detailWidth-2,ShapesLayout::rowHeight,schematicFieldSelected == i,over(ShapeZone::Field,i));
            static constexpr std::array<std::string_view, 14> labels{"X", "Y", "Z", "schematic.rotation", "schematic.mirror",
                "schematic.moveHere", "schematic.shown", "schematic.layerAxis", "schematic.layerMode", "schematic.layer",
                "schematic.keep", "schematic.matchLayer", "schematic.extras", "schematic.showEntities"};
            std::string text = i < 3 ? std::string(labels[static_cast<size_t>(i)]) : translated(labels[static_cast<size_t>(i)]);
            label(context,dx,y+3,l.stepperX()-dx-4,text,palette::dim);
            switch (field) {
            case SchematicField::Visible: case SchematicField::Entities:
                toggleSwitch(context,l.stepperX()+l.stepperWidth()-switchWidth,y+(ShapesLayout::rowHeight-switchHeight)/2,
                    field == SchematicField::Visible ? p->visible : p->entities);
                break;
            case SchematicField::MoveHere: case SchematicField::MatchLayer:
                drawButton(context,l.stepperX(),y+1,l.stepperWidth(),ShapesLayout::rowHeight-2,
                    translated(field == SchematicField::MoveHere ? "schematic.moveHere" : "schematic.matchLayerButton"),
                    over(ShapeZone::Field,i));
                break;
            default: {
                bool numeric = field == SchematicField::X || field == SchematicField::Y || field == SchematicField::Z
                    || field == SchematicField::Layer;
                drawShapeStepper(context,l,y,numeric,fieldValue(*p, field),editingSchematicField == i ? &screen::number() : nullptr,pointer.x,pointer.y);
                break;
            }
            }
        }
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
        drawDangerButton(context,l.deleteX(),l.actionsY+2,ShapesLayout::deleteWidth,12,
            armedLabel(context,translated("shape.delete"),confirmDelete(),schematicDeleteArmed,ShapesLayout::deleteWidth),over(ShapeZone::Action,1), schematicDeleteArmed);
    } else paragraph(context,dx,l.detailTop+6,dw,translated(schematicSet.placements.empty() ? "schematic.noPlacements"
        : "schematic.selectHint"),4,palette::faint);
}
void drawFileDetail(Body& b) {
    auto& context = b.context;
    auto const& l = b.l;
    auto over = [&](ShapeZone zone, int index = -1) { return b.over(zone, index); };
    float const dx = b.dx, dw = b.dw;
    auto info = [&](std::string const& relative, float y) { drawFileFacts(b, relative, y); };
    if (schematicPick == SchematicPick::File && schematicIndex >= 0 && schematicIndex < static_cast<int>(schematicFiles.size())) {
        auto const& f = schematicFiles[static_cast<size_t>(schematicIndex)];
        label(context,dx,l.nameY+1+boxTextInset(),dw,fileTitle(f.relative));
        bool waits = waitsForLoad(f);
        if (waits) {
            // Just above its "Load" button, so the warning and the choice read together.
            auto text = translated("schematic.largeWarning", std::format("{:.1f}", static_cast<double>(f.bytes) / (1024 * 1024)));
            int lines = static_cast<int>(paragraphLines(context, dw, text, 4));
            paragraph(context,dx,std::max(l.previewY,l.actionsY-4-12.f*lines),dw,text,4,palette::warning);
        } else {
            info(f.relative, l.previewY);
            // The 3D preview below the facts, turning slowly (L-114).
            float top = l.previewY + 48, bottom = l.actionsY - 6;
            if (bottom - top >= 24) {
                fill(context,dx,top,dw,bottom-top,palette::shade,.25f);
                frame(context,dx,top,dw,bottom-top,palette::white,.1f);
                auto& t = filesTurn;
                t.x = dx; t.y = top; t.w = dw; t.h = bottom - top;
                if (t.file != f.relative) { startFile(t, f.relative); t.manual = false; }
                float yaw = shownYaw(t);
                // Under the pointer the footer says how to work it (its corner says so only while it turns).
                if (b.pointer.x >= t.x && b.pointer.x < t.x + t.w && b.pointer.y >= t.y && b.pointer.y < t.y + t.h)
                    b.fieldHelp = translated("schematic.previewHint");
                auto structure = schematic::session::structure(f.relative);
                // The clicked block stays lit, the rest dims.
                std::optional<schematic::preview::Cell> pick = t.pickedFile == f.relative ? t.picked : std::nullopt;
                schematic::preview::Tint tint{pick ? static_cast<std::uint64_t>((pick->x * 4099 + pick->y) * 4099 + pick->z) + 7 : 0,
                    [pick](int x, int y, int z) -> std::uint32_t { return pick && !(*pick == schematic::preview::Cell{x, y, z}) ? packed(palette::previewDim) : 0xffffffffu; }};
                float s = drawScale();
                if (!schematic::preview::draw(context, structure, (dx + 1) * s, (top + 1) * s, (dw - 2) * s, (bottom - top - 2) * s,
                        schematic::preview::View{yaw, t.pitch, t.zoom, t.peel}, pick ? &tint : nullptr))
                    t.w = 0;
                else drawPreviewLabel(context, t, dx, top, dw);
                // What was clicked, beside the facts.
                if (pick && structure) {
                    auto index = structure->blocks[static_cast<size_t>(structure->cell(pick->x, pick->y, pick->z))];
                    if (index >= 0 && static_cast<size_t>(index) < structure->palette.size()) {
                        static std::string labelFor;
                        static schematic::ghosts::BlockLabel labelled;
                        auto key = std::format("{}:{}", f.relative, index);
                        if (key != labelFor) { labelFor = key; labelled = schematic::ghosts::blockLabel(structure->palette[static_cast<size_t>(index)]); }
                        float px = dx + dw / 2, pw = dw / 2;
                        drawItemIcon(context, labelled.icon, px, l.previewY - 1, 12);
                        label(context,px+14,l.previewY,pw-14,labelled.name);
                        label(context,px,l.previewY+11,pw,translated("schematic.pick.at", std::format("{}, {}, {}", pick->x, pick->y, pick->z)),palette::dim);
                    }
                }
            }
        }
        drawButton(context,l.actionX(0),l.actionsY+2,l.firstActionWidth,12,translated(waits ? "schematic.loadAnyway" : "schematic.place"),
            over(ShapeZone::Action,0),ButtonKind::Primary);
    } else paragraph(context,dx,l.detailTop+6,dw,translated(schematicFiles.empty() ? "schematic.empty" : "schematic.fileSelectHint"),
        4,palette::faint);
    fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
    drawButton(context,l.deleteX(),l.actionsY+2,ShapesLayout::deleteWidth,12,translated("schematic.openFolder"),over(ShapeZone::Action,1));
}
void drawCheckDetail(Body& b) {
    auto& context = b.context;
    auto const& l = b.l;
    auto const pointer = b.pointer;
    float const dx = b.dx, dw = b.dw;
    auto const* checked = b.checked;
    auto const& loadProblem = b.loadProblem;
    bool const unloadable = b.unloadable, checkOff = b.checkOff, counting = b.counting;
    auto const& feet = b.feet;
    if (!checked) { paragraph(context,dx,l.detailTop+6,dw,translated("schematic.noSelection"),4,palette::faint); return; }
    // Several rows (L-190): the selection pane's count stands in for the
    // name; the preview stays (L-183) and the names go below it.
    bool const several = !ignoreView && checkRows.several();
    using IgnoreKind = schematic::IgnoreRow::Kind;
    auto const* picked = ignoreView ? pickedIgnoreRow() : nullptr;
    auto const fileStructure = schematic::session::loaded(checked->file);
    // In the ignore view the title names what is picked.
    std::string title = checked->name, titleIcon;
    if (picked && picked->kind == IgnoreKind::Place) {
        auto const* m = picked->stale ? nullptr : rowAt(picked->entry);
        if (picked->entry.block == "minecraft:air") title = m && !m->actualName.empty() ? m->actualName : translated("schematic.air");
        else { auto const& label = fileBlockLabel(fileStructure, picked->entry.block); title = label.name; titleIcon = label.icon; }
    } else if (picked) {
        auto const& label = fileBlockLabel(fileStructure, picked->block);
        title = label.name;
        if (!picked->stale) titleIcon = label.icon;
    }
    // "Ignore settings" ends the top line in every state of the pane (one
    // row, several, the ignore view), so it never moves; pressed while
    // the view is open. The name is cut before it.
    float titleEnd = dx + dw;
    {
        std::string text = translated("schematic.ignore.settings");
        float w = std::min(textWidth(context, text) + 10, dw * .55f), x = dx + dw - w, top = l.nameY + 1, h = 12;
        bool hovered = pointer.x >= x && pointer.x < x + w && pointer.y >= top && pointer.y < top + h;
        drawButton(context,x,top,w,h,std::move(text),hovered,ignoreView ? ButtonKind::Primary : ButtonKind::Plain);
        ignoreSettingsBox = {x, top, w, h};
        titleEnd = x - 4;
    }
    if (!several) {
        float tx = dx;
        if (!titleIcon.empty()) { drawItemIcon(context, titleIcon, dx, l.nameY + 1, 12); tx += 14; }
        label(context,tx,l.nameY+1+boxTextInset(),titleEnd-tx,title);
    }
    auto const& t = verification->visible;
    auto const& ignored = checked->ignored;
    // Rows from before a move point elsewhere (L-177): nothing is changed from them.
    bool const editable = !verification->stale;
    schematic::Mismatch const* selected = !several && verifySelected >= 0 && verifySelected < static_cast<int>(verifyRows.size())
        ? verifyRows[static_cast<size_t>(verifySelected)] : nullptr;
    auto const chosenRows = several ? selectedCheckRows() : std::vector<schematic::Mismatch const*>{};
    // Layout A (Check tab review): the preview at a fixed size on top,
    // then the counts beside the selected row, then its items. Lines that
    // do not fit above the buttons are left out.
    // Item lines take room from the preview, up to four of them (L-183).
    int itemLines = 0;
    float itemsBelow = 0;
    if (ignoreView && picked && picked->kind == IgnoreKind::Block) {
        auto const& compared = comparedItemsOf(fileStructure);
        if (auto found = compared.find(picked->block); found != compared.end()) itemLines = static_cast<int>(found->second.size());
        itemsBelow = 12;
    } else if (!ignoreView && selected && !selected->states.empty()) {
        itemLines = static_cast<int>(selected->states.size());
        itemsBelow = 12 + 57 + 11;
    }
    float pvTop = l.previewY - 2, pvHeight = previewHeight(l.actionsY - pvTop, itemLines, itemsBelow, 12);
    float textTop = pvTop + pvHeight + 5, textEnd = l.actionsY - 6;
    auto fits = [&](float y) { return y + 10 <= textEnd; };
    auto pointed = [&](float x, float top, float w, float h) {
        return pointer.x >= x && pointer.x < x + w && pointer.y >= top && pointer.y < top + h;
    };
    // The preview marks a set of cells (white, the rest dimmed): the
    // selected rows, or the block clicked in it; the other rows of the
    // shown kinds keep their kind's tint.
    std::vector<schematic::Point> focus;
    std::string focusBlock;
    if (ignoreView) {
        if (picked && picked->kind == IgnoreKind::Place && !picked->stale && fileStructure)
            focus.push_back(schematic::toWorld(fileStructure->size, checked->placement, picked->entry.cell));
        else if (picked && picked->kind == IgnoreKind::Block) focusBlock = picked->block;
    } else if (several) {
        for (auto const* m : chosenRows) if (!m->entity) focus.push_back(m->position);
    } else if (selected && !selected->entity) focus.push_back(selected->position);
    auto drawPreview = [&](std::vector<schematic::Point> const& cells, std::string const& block, bool kinds) {
        fill(context,dx,pvTop,dw,pvHeight,palette::shade,.25f);
        frame(context,dx,pvTop,dw,pvHeight,palette::white,.1f);
        if (counting) { label(context,dx+4,pvTop+3,dw-8,translated("schematic.counting"),palette::dim); return; }
        auto structure = schematic::session::structure(checked->file);
        if (!structure) return;
        auto marked = std::make_shared<std::set<std::tuple<int, int, int>>>();
        std::uint64_t key = reinterpret_cast<std::uintptr_t>(verification.get()) * 31 + static_cast<std::uint64_t>(checkFilter) * 1000003
            + (kinds ? 17 : 5);
        auto mix = [&](std::uint64_t v) { key = (key ^ v) * 1099511628211ull; };
        for (auto const& c : cells) {
            marked->insert({c.x, c.y, c.z});
            mix(static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.x)) << 32 ^ static_cast<std::uint32_t>(c.z));
            mix(static_cast<std::uint32_t>(c.y));
        }
        if (!cells.empty() || !block.empty()) {}
        else if (!ignoreView && checkTurn.picked && checkTurn.pickedFile == checked->file) {
            auto w = checkCellWorld(*checked, *checkTurn.picked);
            marked->insert({w.x, w.y, w.z});
            mix(static_cast<std::uint64_t>(static_cast<std::uint32_t>(w.x)) << 32 ^ static_cast<std::uint32_t>(w.z));
            mix(static_cast<std::uint32_t>(w.y));
        }
        // A picked block type marks every cell holding it.
        std::vector<bool> blockCells;
        if (!block.empty()) {
            blockCells.resize(structure->palette.size());
            for (size_t i = 0; i < structure->palette.size(); ++i) blockCells[i] = structure->palette[i].name == block;
            for (char ch : block) mix(static_cast<unsigned char>(ch));
        }
        static std::map<std::tuple<int, int, int>, schematic::CellState> states;
        static std::uint64_t statesKey = 0;
        if (key != statesKey) {
            statesKey = key;
            states.clear();
            if (kinds)
                for (auto const& m : verification->mismatches)
                    if (!m.entity && verifyMatches(m) && schematic::liveMark(m, ignored) == schematic::IgnoreMark::None)
                        states[{m.position.x, m.position.y, m.position.z}] = m.state;
        }
        bool const any = !marked->empty() || std::find(blockCells.begin(), blockCells.end(), true) != blockCells.end();
        // Drawn as placed: a preview cell is the placement's corner plus the cell.
        auto const shown = placedView(structure, checked->placement);
        auto const origin = checked->placement.origin;
        schematic::preview::Tint tint{key, [origin, shown, marked, blockCells, any](int x, int y, int z) -> std::uint32_t {
            schematic::Point world{origin.x + x, origin.y + y, origin.z + z};
            bool hit = marked->contains({world.x, world.y, world.z});
            if (!hit && !blockCells.empty()) {
                auto index = shown->blocks[static_cast<size_t>(shown->cell(x, y, z))];
                hit = index != schematic::voidCell && static_cast<size_t>(index) < blockCells.size() && blockCells[static_cast<size_t>(index)];
            }
            if (any && hit) return 0xffffffffu;
            auto found = states.find({world.x, world.y, world.z});
            std::uint32_t color = 0xffffffffu;
            if (found != states.end()) switch (found->second) {
            case schematic::CellState::Missing: color = packed(palette::previewNotPlaced); break;
            case schematic::CellState::Wrong: case schematic::CellState::Extra: color = packed(palette::previewMismatch); break;
            case schematic::CellState::State: color = packed(palette::previewWrongState); break;
            default: break;
            }
            if (!any) return color;
            // Dim the rest to 45%.
            auto dim = [&](int shift) { return ((((color >> shift) & 255) * 115 / 255) << shift); };
            return dim(0) | dim(8) | dim(16) | 0xff000000u;
        }};
        auto& turn = checkTurn;
        turn.x = dx; turn.y = pvTop; turn.w = dw; turn.h = pvHeight;
        if (turn.file != checked->file) startFile(turn, checked->file);
        // Turned or mirrored since: the picked cell and the slice were the old shape's.
        if (int key = schematic::quarterTurns(checked->placement.rotation) * 3 + static_cast<int>(checked->placement.mirror); key != turn.placedTurn) {
            turn.placedTurn = key;
            turn.picked.reset();
            turn.peel = 0;
        }
        if (b.pointer.x >= turn.x && b.pointer.x < turn.x + turn.w && b.pointer.y >= turn.y && b.pointer.y < turn.y + turn.h)
            b.fieldHelp = translated("schematic.previewHint");
        float s = drawScale();
        if (!schematic::preview::draw(context, shown, (dx + 1) * s, (pvTop + 1) * s, (dw - 2) * s, (pvHeight - 2) * s,
                schematic::preview::View{turn.yaw, turn.pitch, turn.zoom, turn.peel}, &tint,
                &checked->placement))
            turn.w = 0;
        else drawPreviewLabel(context, turn, dx, pvTop, dw, checked->placement.origin.y);
    };
    if (unloadable) paragraph(context,dx,l.previewY,dw,translated("schematic.notLoaded", loadProblem),3,palette::warning);
    else if (checkOff) paragraph(context,dx,l.previewY,dw,translated("schematic.offCheck"),3,palette::faint);
    else drawPreview(focus, focusBlock, !ignoreView);
    // A line's button sized by its text, ending at `right`; returns its left edge.
    auto lineButton = [&](float right, float top, std::string text, IgnoreControlKind kind,
                          std::vector<schematic::IgnoreEntry> entries) {
        float w = textWidth(context, text) + 10, x = right - w;
        drawButton(context,x,top,w,10,std::move(text),pointed(x,top,w,10));
        ignoreControls.push_back({x, top, w, 10, kind, std::move(entries)});
        return x;
    };
    // The action row's two slots, the left action and the one at the right
    // edge, each widened for its text where the pane has room, never into
    // the other (long labels such as "Stop ignoring this place").
    struct Slots { float lx, lw, rx, rw; };
    auto actionSlots = [&](std::string_view leftText, std::string_view rightText) {
        float edge = l.detailLeft + l.detailWidth - ShapesLayout::pad, start = l.actionX(0), room = edge - start - 4;
        float rw = std::min(std::max(ShapesLayout::deleteWidth, textWidth(context, rightText) + 10),
                            std::max(ShapesLayout::deleteWidth, room - l.firstActionWidth));
        float lw = std::max(l.firstActionWidth, std::min(textWidth(context, leftText) + 10, room - rw));
        return Slots{start, lw, edge - rw, rw};
    };
    auto distanceTo = [&](schematic::Point p) {
        double ddx = p.x + .5 - feet.x, ddy = p.y + .5 - feet.y, ddz = p.z + .5 - feet.z;
        return translated("waypoint.meters", static_cast<int>(std::lround(std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz))));
    };
    // Item lines that do not fit scroll under the wheel (the heading and
    // the preview stay); `of` names what they list, and a new one starts
    // at the top. The bar is the lists' own, at the pane's right edge.
    auto scrollItems = [&](std::string of, size_t count, float top, float bottom, float pitch) {
        if (of != itemScroll.of) { itemScroll.of = std::move(of); itemScroll.first = 0; }
        itemScroll.drawn = true;
        auto s = lineScroll(static_cast<int>(count), itemScroll.first, top, bottom, pitch, 10);
        itemScroll.first = s.first;
        itemScroll.count = s.count;
        itemScroll.visible = s.visible;
        if (s.overflows() && s.visible > 0) {
            float track = s.visible * pitch, barX = dx + dw - 2;
            itemScroll.x = dx; itemScroll.y = top - 1; itemScroll.w = dw; itemScroll.h = track;
            auto const& bar = itemScroll.bar = rowScrollBar(barX, top - 1, track, s.count, s.visible, s.first, 6);
            drawScrollbar(context,bar.x,bar.top,bar.track,bar.thumbTop(),bar.thumb,screen::scrollHeld(&itemScroll.first) || bar.hit(pointer.x,pointer.y));
        }
        return s;
    };
    if (ignoreView) {
        // A place's facts, or a block's items each ruled on every such
        // block or given back; "pick one" with nothing picked.
        float y = textTop;
        auto measure = [&](std::string_view text) { return textWidth(context, text) + 10; };
        if (!picked) paragraph(context,dx,y,dw,translated("schematic.ignore.pick"),3,palette::faint);
        else if (picked->kind == IgnoreKind::Place) {
            auto const& e = picked->entry;
            if (fileStructure) {
                auto w = schematic::toWorld(fileStructure->size, checked->placement, e.cell);
                label(context,dx,y,dw,translated("schematic.check.here", std::format("{}, {}, {}", w.x, w.y, w.z)),palette::dim);
                y += 12;
            }
            if (e.kind == schematic::IgnoreKind::CellItem) {
                paragraph(context,dx,y,dw,translated("schematic.ignore.oldFormInfo",
                    schematic::ghosts::itemName(e.item, e.block)),3,palette::faint);
            } else if (picked->stale) paragraph(context,dx,y,dw,translated("schematic.ignore.placeStale"),3,palette::faint);
            else {
                if (auto const* m = rowAt(e)) {
                    label(context,dx,y,dw,mismatchKind(m->state),mismatchColor(m->state));
                    y += 12;
                    if (!m->expectedName.empty() && fits(y)) {
                        drawItemIcon(context, m->expected, dx, y - 2, 12);
                        label(context,dx+14,y,dw-14,translated("schematic.expected", m->expectedName),palette::dim);
                        y += 13;
                    }
                    if (!m->actualName.empty() && fits(y)) {
                        drawItemIcon(context, m->actual, dx, y - 2, 12);
                        label(context,dx+14,y,dw-14,translated("schematic.actual", m->actualName),palette::dim);
                        y += 13;
                    }
                }
                if (fits(y)) paragraph(context,dx,y,dw,translated("schematic.ignore.notCounted"),2,palette::faint);
            }
        } else if (picked->kind == IgnoreKind::StaleBlock) {
            paragraph(context,dx,y,dw,translated("schematic.ignore.blockStale"),3,palette::faint);
            y += 12.f * paragraphLines(context, dw, translated("schematic.ignore.blockStale"), 3) + 3;
            std::vector<schematic::IgnoreEntry> rules;
            for (auto const& item : picked->rules) rules.push_back(schematic::ruleEntry(picked->block, item));
            if (fits(y)) lineButton(dx + dw, y - 1, translated("schematic.ignore.undo"), IgnoreControlKind::Remove, std::move(rules));
        } else {
            // Every item the check compares on this block, those ruled and
            // those differing now included.
            std::vector<std::string> items;
            auto const& compared = comparedItemsOf(fileStructure);
            if (auto found = compared.find(picked->block); found != compared.end()) items = found->second;
            auto addItem = [&](std::string const& item) {
                if (std::find(items.begin(), items.end(), item) == items.end()) items.push_back(item);
            };
            for (auto const& item : picked->rules) addItem(item);
            for (auto const& m : verification->mismatches)
                if (m.state == schematic::CellState::State && m.fileBlock == picked->block)
                    for (auto const& d : m.states) addItem(schematic::itemId(d));
            label(context,dx,y,dw,translated("schematic.ignore.compared"),palette::faint);
            y += 12;
            auto const scroll = scrollItems("b" + picked->block, items.size(), y, textEnd, 12);
            float const edge = scroll.overflows() ? dx + dw - 1 - rowBarGutter : dx + dw;
            for (int i = scroll.first; i < scroll.first + scroll.visible; ++i) {
                auto const& item = items[static_cast<size_t>(i)];
                bool const ruled = ignored.ruled(picked->block, item);
                auto rule = schematic::ruleEntry(picked->block, item);
                float right = edge;
                if (ruled) right = lineButton(right, y - 1, translated("schematic.ignore.undo"), IgnoreControlKind::Remove, {rule});
                else {
                    std::string full = translated("schematic.ignore.onAll", title), brief = translated("schematic.ignore.onAllShort");
                    right = lineButton(right, y - 1, fittingText(full, brief, dw * .45f, measure), IgnoreControlKind::Add, {rule});
                }
                right -= 4;
                std::string name = schematic::ghosts::itemName(item, picked->block);
                float nameW = std::min((right - dx) * .5f, textWidth(context, name) + 6);
                label(context,dx,y,nameW-4,name,palette::dim);
                auto differing = schematic::differingRows(verification->mismatches, picked->block, item);
                if (ruled) label(context,dx+nameW,y,right-dx-nameW,translated("schematic.ignore.onAllActive", title),palette::dim);
                else if (differing) label(context,dx+nameW,y,right-dx-nameW,translated("schematic.ignore.differAt", differing),palette::warning);
                y += 12;
            }
        }
        // The action row: the picked place given back, and Undo all (two presses).
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
        float const ay = l.actionsY + 2;
        bool const placePicked = picked && picked->kind == IgnoreKind::Place;
        std::string const unplace = translated("schematic.ignore.unplace"), undoAll = translated("schematic.ignore.undoAll"),
                          undoConfirm = translated("confirm.undoAll");
        auto slots = actionSlots(unplace, textWidth(context, undoAll) >= textWidth(context, undoConfirm) ? undoAll : undoConfirm);
        float ux = slots.lx, uw = slots.lw;
        drawButton(context,ux,ay,uw,12,unplace,placePicked && pointed(ux,ay,uw,12),ButtonKind::Plain,placePicked);
        if (placePicked) ignoreControls.push_back({ux, ay, uw, 12, IgnoreControlKind::Remove, {picked->entry}});
        float ax = slots.rx, aw = slots.rw;
        if (ignored.empty()) drawButton(context,ax,ay,aw,12,undoAll,false,ButtonKind::Plain,false);
        else {
            drawDangerButton(context,ax,ay,aw,12,armedLabel(context,undoAll,undoConfirm,verifyUndoAllArmed,aw),pointed(ax,ay,aw,12),
                verifyUndoAllArmed);
            ignoreControls.push_back({ax, ay, aw, 12, IgnoreControlKind::UndoAll, {}});
        }
        return;
    }
    if (several) {
        // The names below the preview; Show in world, Ignore places, Stop
        // ignoring. No item is ignored from several rows.
        auto pane = l.selectionPane(static_cast<int>(chosenRows.size()), 0, textTop);
        pane.countW = titleEnd - pane.x;
        std::vector<SelectedName> names;
        for (auto const* m : chosenRows) {
            if (names.size() >= static_cast<size_t>(pane.fit.names)) break;
            std::string name = m->state == schematic::CellState::Extra ? m->actualName : m->expectedName;
            names.push_back({std::move(name), false, distanceTo(m->position), {}});
        }
        // A button that would change nothing is disabled.
        std::vector<bool> enabled{true, editable && !schematic::placesToAdd(chosenRows, ignored).empty(),
                                  editable && !schematic::placesToRemove(chosenRows, ignored).empty()};
        drawSelectionPane(context,pane,chosenRows.size(),names,[&](size_t i, float gx, float gy) {
            drawKindSquare(context,gx-3,gy,mismatchColor(chosenRows[i]->state),schematic::liveMark(*chosenRows[i], ignored) != schematic::IgnoreMark::None);
        },l.selecting,checkBulk,pointer.x,pointer.y,0,0,enabled);
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
        return;
    }
    // Counts on the left, the selected row on the right.
    float half = std::floor(dw * .42f), rx = dx + half + 8, rw = dw - half - 8;
    // What the counts cover spans the pane above both columns.
    label(context,dx,textTop,dw,translated("schematic.check.whole", layersText(*checked)),palette::faint);
    float y = textTop + 12;
    // After an entries change the counts wait for the next pass (L-183).
    if (!counting && !unloadable && verification->outdated) {
        if (fits(y)) label(context,dx,y,half,translated("schematic.counting"),palette::dim);
    } else if (!counting && !unloadable) {
        if (fits(y)) label(context,dx,y,half,translated("schematic.summary.correct", t.correct, t.total()),palette::accent);
        if (fits(y + 11)) label(context,dx,y+11,half,translated("schematic.summary.missing", t.missing),palette::dim);
        if (fits(y + 22)) label(context,dx,y+22,half,translated("schematic.summary.wrong", t.wrong + t.extra),palette::mismatch);
        if (fits(y + 33)) label(context,dx,y+33,half,translated("schematic.summary.state", t.state),palette::warning);
    }
    float bottom = y + 57;
    // The list keeps the nearest of each kind; say so when it is cut.
    // Ignored rows are listed but not counted.
    using F = schematic::CheckFilter;
    std::uint64_t kindTotal = checkFilter == F::WrongBlock ? t.wrong + t.extra : checkFilter == F::WrongState ? t.state
        : checkFilter == F::NotPlaced ? t.missing : t.wrong + t.extra + t.state + t.missing;
    auto countedRows = static_cast<std::uint64_t>(std::count_if(verifyRows.begin(), verifyRows.end(),
        [&](auto const* m) { return schematic::liveMark(*m, ignored) == schematic::IgnoreMark::None; }));
    if (!counting && !unloadable && !verification->outdated && countedRows < kindTotal && fits(y + 57)) {
        paragraph(context,dx,y+57,dw,translated("schematic.check.listCut", schematic::maxMismatches),3,palette::faint);
        bottom = y + 57 + 12.f * paragraphLines(context, dw, translated("schematic.check.listCut", schematic::maxMismatches), 3) + 2;
    }
    if (selected) {
        auto const& m = *selected;
        auto const mark = schematic::liveMark(m, ignored);
        bool const placeIgnored = mark == schematic::IgnoreMark::Place;
        Rgb const kindColor = mark != schematic::IgnoreMark::None ? palette::dim : mismatchColor(m.state);
        fill(context,rx-5,y,1,52,palette::white,.1f);
        label(context,rx,y,rw,translated("schematic.check.here", std::format("{}, {}, {}", m.position.x, m.position.y, m.position.z)),kindColor);
        // The kind is named here (the rows only show its square), with the
        // ignore beside it.
        std::string kind = mismatchKind(m.state);
        if (mark != schematic::IgnoreMark::None)
            kind += " · " + translated(placeIgnored ? "schematic.ignore.tagPlace" : "schematic.ignore.tagItems");
        if (fits(y + 11)) label(context,rx,y+11,rw,kind,kindColor);
        // What was confused with what: the expected block above the one in the world.
        if (!m.expectedName.empty() && fits(y + 24)) {
            drawItemIcon(context, m.expected, rx, y + 23, 12);
            label(context,rx+14,y+25,rw-14,translated("schematic.expected", m.expectedName),palette::dim);
        }
        if (!m.actualName.empty() && fits(y + 38)) {
            drawItemIcon(context, m.actual, rx, y + 37, 12);
            label(context,rx+14,y+39,rw-14,translated("schematic.actual", m.actualName),palette::dim);
        }
        // Every differing item: "<item>  <now> -> <should be>", full width
        // below, each ignored on every such block of the placement or
        // given back. A place ignored keeps its items without buttons.
        float sy = bottom;
        if (!m.states.empty() && fits(sy)) {
            fill(context,dx,sy-3,dw,1,palette::white,.1f);
            label(context,dx,sy,dw,translated("schematic.check.items", m.expectedName),palette::faint);
            sy += 11;
        }
        bool const canIgnore = editable && schematic::takesIgnores(m) && !placeIgnored;
        auto translate = [](std::string_view key) { return translated(key); };
        auto measure = [&](std::string_view text) { return textWidth(context, text) + 10; };
        float const pitch = canIgnore ? 12 : 11;
        // A place ignored keeps its note under the lines.
        auto const scroll = scrollItems(std::format("c{},{},{},{}", m.position.x, m.position.y, m.position.z, m.entity),
            m.states.size(), sy, placeIgnored ? textEnd - 24 : textEnd, pitch);
        float const edge = scroll.overflows() ? dx + dw - 1 - rowBarGutter : dx + dw;
        for (int i = scroll.first; i < scroll.first + scroll.visible; ++i) {
            auto const& d = m.states[static_cast<size_t>(i)];
            auto const& item = schematic::itemId(d);
            // The live entries, so a press shows at once, before the next pass.
            bool const ruled = schematic::takesIgnores(m) && ignored.ruled(m.fileBlock, item);
            bool const dimmed = placeIgnored || ruled;
            float right = edge;
            if (canIgnore) {
                auto rule = schematic::ruleEntry(m.fileBlock, item);
                if (ruled) right = lineButton(right, sy - 1, translated("schematic.ignore.undo"), IgnoreControlKind::Remove, {rule});
                else {
                    std::string full = translated("schematic.ignore.onAll", m.expectedName), brief = translated("schematic.ignore.onAllShort");
                    right = lineButton(right, sy - 1, fittingText(full, brief, dw * .45f, measure), IgnoreControlKind::Add, {rule});
                }
                right -= 4;
            }
            auto now = information::stateName(d.key, d.actual, m.identifier, translate);
            auto want = information::stateName(d.key, d.expected, m.identifier, translate);
            std::string name = schematic::ghosts::itemName(item, m.identifier);
            Rgb const valueColor = dimmed ? palette::faint : palette::warning;
            float lineW = right - dx, nameW = std::min(lineW * .4f, textWidth(context, name) + 6);
            label(context,dx,sy,nameW-4,name,dimmed ? palette::faint : palette::dim);
            float vx = dx + nameW, nowW = textWidth(context, now.value);
            label(context,vx,sy,std::max(0.f,std::min(nowW+2,right-vx)),now.value,valueColor);
            vx += nowW + 3;
            if (vx + changeArrowWidth < right) changeArrow(context,vx,sy+1.5f+shapeTextDrop(),1,valueColor);
            vx += changeArrowWidth + 3;
            float wantW = std::min(textWidth(context, want.value) + 2, std::max(0.f, right - vx));
            label(context,vx,sy,wantW,want.value,valueColor);
            vx += wantW + 6;
            if (ruled && vx < right) label(context,vx,sy,right-vx,translated("schematic.ignore.onAllActive", m.expectedName),palette::dim);
            sy += pitch;
        }
        if (placeIgnored && fits(sy)) paragraph(context,dx,sy,dw,translated("schematic.ignore.placeNotCounted"),2,palette::faint);
    }
    else if (auto structure = schematic::session::structure(checked->file);
             structure && checkTurn.picked && checkTurn.pickedFile == checked->file) {
        // A clicked block without a row (correct, filtered out, or past
        // the list's limit): checked again right now.
        auto world = checkCellWorld(*checked, *checkTurn.picked);
        auto* region = context.mClient.getRegion();
        auto now = region ? schematic::ghosts::mismatchAt(*region, world) : std::nullopt;
        fill(context,rx-5,y,1,52,palette::white,.1f);
        bool correct = !now || now->state == schematic::CellState::Correct || now->ignore == schematic::IgnoreMark::Items;
        bool placeIgnored = now && now->ignore == schematic::IgnoreMark::Place;
        Rgb color = correct || placeIgnored ? palette::text : mismatchColor(now->state);
        label(context,rx,y,rw,translated("schematic.check.here", std::format("{}, {}, {}", world.x, world.y, world.z)),color);
        if (fits(y + 11))
            label(context,rx,y+11,rw,placeIgnored ? translated("schematic.ignore.tagPlace") : correct ? translated("schematic.pick.correct")
                : mismatchKind(now->state),placeIgnored ? palette::dim : correct ? palette::accent : mismatchColor(now->state));
        if (now && !now->expectedName.empty() && fits(y + 24)) {
            drawItemIcon(context, now->expected, rx, y + 23, 12);
            label(context,rx+14,y+25,rw-14,translated("schematic.expected", now->expectedName),palette::dim);
        }
        if (now && !correct && !now->actualName.empty() && fits(y + 38)) {
            drawItemIcon(context, now->actual, rx, y + 37, 12);
            label(context,rx+14,y+39,rw-14,translated("schematic.actual", now->actualName),palette::dim);
        }
    }
    // The action row's two places: Show in world, and the selected row's
    // place ignored or given back (extras too).
    fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
    float const ay = l.actionsY + 2;
    bool canShow = selected || (checkTurn.picked && checkTurn.pickedFile == checked->file);
    bool const placeButton = selected && schematic::takesIgnores(*selected);
    bool const placeOn = placeButton && ignored.place(selected->local, selected->fileBlock);
    std::string const showText = translated("schematic.showInWorld"),
                      placeText = placeButton ? translated(placeOn ? "schematic.ignore.unplace" : "schematic.ignore.place") : std::string{};
    auto slots = actionSlots(showText, placeText);
    float sx = slots.lx, sw = slots.lw;
    drawButton(context,sx,ay,sw,12,showText,canShow && pointed(sx,ay,sw,12),ButtonKind::Primary,canShow);
    if (canShow) ignoreControls.push_back({sx, ay, sw, 12, IgnoreControlKind::Show, {}});
    if (placeButton) {
        auto const& m = *selected;
        float px = slots.rx, pw = slots.rw;
        drawButton(context,px,ay,pw,12,placeText,editable && pointed(px,ay,pw,12),ButtonKind::Plain,editable);
        if (editable)
            ignoreControls.push_back({px, ay, pw, 12, placeOn ? IgnoreControlKind::Remove : IgnoreControlKind::Add,
                                      {schematic::placeEntry(m.local, m.fileBlock)}});
    }
}
void drawMaterialsDetail(Body& b) {
    auto& context = b.context;
    auto const& l = b.l;
    auto const pointer = b.pointer;
    auto over = [&](ShapeZone zone, int index = -1) { return b.over(zone, index); };
    float const dx = b.dx, dw = b.dw;
    auto const* checked = b.checked;
    auto const& loadProblem = b.loadProblem;
    bool const unloadable = b.unloadable, checkOff = b.checkOff, counting = b.counting;
    auto& listTip = b.listTip;
    auto& fieldHelp = b.fieldHelp;
    // A row of the details: its name (whole in a tip where cut) and its stepper or switch; under the pointer its
    // `help` goes to the footer.
    auto stepperRow = [&](int i, std::string_view key, std::string value, bool isSwitch, bool on, char const* help = nullptr) {
        if (i < l.fieldFirst || i >= l.fieldFirst + l.fieldVisible) return;
        float y = l.fieldY(i);
        bool const hovered = over(ShapeZone::Field,i);
        rowBackground(context,l.detailLeft+1,y,l.detailWidth-2,ShapesLayout::rowHeight,schematicFieldSelected == i,hovered);
        std::string name = translated(key);
        float const nameW = l.stepperX()-dx-4;
        if (hovered && pointer.x < dx + nameW && textWidth(context, name) > nameW) listTip = {name, pointer.x, pointer.y};
        if (hovered && help) fieldHelp = translated(help);
        label(context,dx,y+3,nameW,std::move(name),palette::dim);
        if (isSwitch) toggleSwitch(context,l.stepperX()+l.stepperWidth()-switchWidth,y+(ShapesLayout::rowHeight-switchHeight)/2,on);
        else drawShapeStepper(context,l,y,false,std::move(value),nullptr,pointer.x,pointer.y);
    };
    if (!checked) { paragraph(context,dx,l.detailTop+6,dw,translated("schematic.noSelection"),4,palette::faint); return; }
    label(context,dx,l.nameY+1+boxTextInset(),dw,checked->name);
    if (unloadable) paragraph(context,dx,l.previewY,dw,translated("schematic.notLoaded", loadProblem),3,palette::warning);
    else if (checkOff) paragraph(context,dx,l.previewY,dw,translated("schematic.offCheck"),3,palette::faint);
    else if (counting) label(context,dx,l.previewY,dw,translated("schematic.counting"),palette::dim);
    else {
        auto carried = carriedItems();
        std::uint64_t needed = 0, remaining = 0;
        int kinds = 0, shortKinds = 0;
        for (auto const* line : materialRows) {
            ++kinds;
            needed += line->needed;
            remaining += line->remaining();
            bool noItem = line->item.empty() || (line->entity && !schematic::items::iconStack(line->icon));
            if (line->remaining() && !noItem && carried[line->item] < line->remaining()) ++shortKinds;
        }
        label(context,dx,l.previewY,dw,translated("schematic.materials.kinds", kinds),palette::text);
        label(context,dx,l.previewY+11,dw,translated("schematic.materials.left", remaining, needed),palette::dim);
        label(context,dx,l.previewY+22,dw,translated("schematic.materials.short", shortKinds),shortKinds ? palette::warning : palette::accent);
    }
    label(context,dx,l.previewY+35,dw,layersText(*checked),palette::faint);
    stepperRow(0, "schematic.shownLayersOnly", {}, true, materialsShownOnly, "help.schematic.shownLayersOnly");
    stepperRow(1, "schematic.materials.hud", {}, true, Runtime::instance().preferences().schematic.hud, "help.schematic.hudMaterials");
    // What is left goes to the plan of Recipes, which traces it to what to gather (L-359, L-383): this tab keeps
    // the blocks to place.
    stepperRow(2, "schematic.materials.plan", {}, true, Runtime::instance().preferences().schematic.planMaterials, "help.schematic.planMaterials");
    float y = l.fieldY(2) + ShapesLayout::rowHeight + 6;
    if (materialSelected >= 0 && materialSelected < static_cast<int>(materialRows.size())) {
        auto const& line = *materialRows[static_cast<size_t>(materialSelected)];
        drawItemIcon(context, line.icon, dx, y - 2, 12);
        label(context,dx+14,y,dw-14,line.name);
        label(context,dx,y+11,dw,translated("schematic.materials.leftAmount",
            amountText(line.remaining(), stackSizeOf(line))),palette::dim);
        y += 26;
    }
    bool missing = !missingMaterials().empty();
    fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
    drawButton(context,l.actionX(0),l.actionsY+2,l.firstActionWidth,12,translated("schematic.openCalculator"),
        over(ShapeZone::Action,0), ButtonKind::Plain, missing);
}
void drawSchematicFooter(Body& b) {
    auto& context = b.context;
    auto const& l = b.l;
    auto const& listTip = b.listTip;
    auto const& fieldHelp = b.fieldHelp;
    // Lines that went away and come back (a place picked between) start at the top.
    if (!itemScroll.drawn) itemScroll.of.clear();
    if (listTip) {
        float w = std::min(textWidth(context, listTip->text) + 8, l.width - 4);
        float x = std::max(l.left + 2, std::min(listTip->x + 8, l.left + l.width - w - 2));
        // Above the pointer where it would cover the footer's description (seen low in the Materials list).
        float const y = listTip->y + 10 + 13 > l.footerTop ? listTip->y - 16 : listTip->y + 10;
        drawTipBox(context,x,y,w,13);
        label(context,x+4,y+2,w-6,listTip->text,palette::text);
    }
    // Footer.
    fill(context,l.left,l.footerTop,l.width,1,palette::white,.14f);
    float textLeft = l.left + ShapesLayout::pad, available = l.width - 2 * ShapesLayout::pad;
    bool shortFooter = l.docked;
    std::string text = !screen::message().empty() ? screen::message() : !fieldHelp.empty() ? fieldHelp : schematicDescription();
    if (shortFooter) label(context,textLeft,l.footerTop+3,available,std::move(text),screen::message().empty() ? palette::text : screen::messageColor());
    else {
        paragraph(context,textLeft,l.footerTop+3,available,text,2,screen::message().empty() ? palette::text : screen::messageColor());
        bool menuUnbound = input::effectiveChord(Runtime::instance().preferences().bindings, input::Action::SchematicMenu).empty();
        bool const inIgnoreView = schematicTab == SchematicTab::Verify && ignoreView;
        label(context,textLeft,l.footerTop+30,available,translated(editingSchematicField >= 0 ? "shape.numberHint"
            : inIgnoreView ? "schematic.ignore.viewKeys" : menuUnbound ? "schematic.menuKeyHint" : "schematic.screenHint"),palette::faint);
    }
}
void drawSchematicsBody(MinecraftUIRenderContext& context, ShapesLayout const& l, glm::vec2 pointer) {
    if (l.noticeHeight > 0)
        data_notice::draw(context, l.listLeft + ShapesLayout::pad, l.noticeTop + 2, l.listWidth - 2 * ShapesLayout::pad,
            data_notice::Kind::Placements, pointer);
    else data_notice::hide();
    auto const preferences = Runtime::instance().preferences();
    ignoreControls.clear();
    ignoreSettingsBox = {};
    itemScroll.w = 0;
    itemScroll.drawn = false;
    auto hover = l.hit(pointer.x, pointer.y);
    int tabHover = schematicTabAt(l, pointer.x, pointer.y);
    Body b{context, l, pointer, hover, tabHover};
    auto over = [&](ShapeZone zone, int index = -1) { return b.over(zone, index); };
    drawToolHeader(context,l,translated("waypoint.showAll"),preferences.schematic.enabled,schematicsDocked,over(ShapeZone::DrawAll),over(ShapeZone::Keys),over(ShapeZone::Dock));

    drawSchematicTabs(b);

    // List.
    b.listRight = l.listLeft + l.contentWidth();
    b.left = l.listLeft + ShapesLayout::pad;
    b.checked = checkedPlacement();
    b.unloadable = b.checked && (schematicTab == SchematicTab::Verify || schematicTab == SchematicTab::Materials)
        && !schematic::session::structure(b.checked->file, &b.loadProblem);
    b.checkOff = !preferences.schematic.enabled;
    b.counting = b.checked && !b.unloadable && (b.checkOff || !verification->complete || verification->placement != schematicSet.selected);
    b.places = schematics_rows::listPlaces(b.left, b.listRight);
    // Placed: the lists' shared columns, progress as its own (the detail pane
    // shows the position).
    b.placed = listColumns(l.listLeft, l.contentWidth(), progressColumn, switchWidth);
    if (schematicTab == SchematicTab::Placements) schematic::ghosts::wantProgress();
    drawSchematicHeadings(b);
    auto* player = screen::client() ? screen::client()->getLocalPlayer() : nullptr;
    if (player) b.feet = player->getFeetPos();
    drawSchematicRows(b);

    // Detail.
    b.dx = l.detailLeft + ShapesLayout::pad;
    b.dw = l.detailWidth - 2 * ShapesLayout::pad;
    filesTurn.w = checkTurn.w = 0; // set again where a preview is drawn this frame
    switch (schematicTab) {
    case SchematicTab::Placements: drawPlacementDetail(b); break;
    case SchematicTab::Files: drawFileDetail(b); break;
    case SchematicTab::Verify: drawCheckDetail(b); break;
    case SchematicTab::Materials: drawMaterialsDetail(b); break;
    }
    drawSchematicFooter(b);
}
ShapesLayout fitSchematics(SettingsTable const& t, glm::vec2 size, bool docked) {
    // The tabs fill the toolbar, so docked, the "Show all" switch takes the row below.
    auto l = ShapesLayout::fit(t, size.x, size.y, docked, schematicRowCount(), schematicListFirst, schematicFieldCount(),
        schematicFieldFirst, false, false, true,
        // Placements, and Verify and Materials, which show what is placed; Files are not a world's.
        schematicTab != SchematicTab::Files ? data_notice::height(data_notice::Kind::Placements) : 0);
    // The first action never runs into the one at the right edge.
    l.firstActionWidth = std::clamp(l.detailWidth - 2 * ShapesLayout::pad - ShapesLayout::deleteWidth - 4, 40.f, 96.f);
    // Materials has one action: the calculator link, readable across the pane.
    if (schematicTab == SchematicTab::Materials) l.firstActionWidth = std::max(40.f, l.detailWidth - 2 * ShapesLayout::pad);
    if (schematicTab == SchematicTab::Placements && placementRows.several()) l.selecting = bulkActions(SelectionList::Placements);
    if (schematicTab == SchematicTab::Verify && !ignoreView && checkRows.several() && checkedPlacement()) {
        l.selecting = bulkActions(SelectionList::Check);
    }
    if (std::exchange(scrollToSelected, false) && schematicTab == SchematicTab::Verify && verifySelected >= 0
        && (verifySelected < l.listFirst || verifySelected >= l.listFirst + l.listVisible)) {
        schematicListFirst = std::max(0, verifySelected - l.listVisible / 2);
        return fitSchematics(t, size, docked);
    }
    schematicsDisplayed = l;
    schematicListFirst = l.listFirst;
    schematicFieldFirst = l.fieldFirst;
    return l;
}
void renderSchematicsContent(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer, SettingsTable const& t) {
    auto l = fitSchematics(t, size, false);
    if (l.usable()) drawSchematicsBody(context, l, pointer);
    else filesTurn.w = checkTurn.w = 0; // no preview drawn, so none to turn
    context.flushText(0,std::nullopt);
}
void renderSchematicsDocked(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) {
    auto l = fitSchematics(screen::table(), size, true);
    if (l.width <= 0) {
        label(context, 4, 4, std::max(1.0f, size.x - 8), translated("smallWindow"));
        context.flushText(0, std::nullopt);
        return;
    }
    drawDockedFrame(context,l,translated("nav.schematics"),translated("closeButton"),l.hit(pointer.x, pointer.y).zone == ShapeZone::Close);
    if (l.usable()) drawSchematicsBody(context, l, pointer);
    else {
        filesTurn.w = checkTurn.w = 0;
        drawDockedTooShort(context, l, pointer.x, pointer.y);
    }
    context.flushText(0,std::nullopt);
}
}

void refresh(bool files) { refreshSchematics(files); }
void show(Tab tab) { selectSchematicTab(tab); }
void showPlacement(std::uint64_t id) {
    for (size_t i = 0; i < schematicSet.placements.size(); ++i)
        if (schematicSet.placements[i].id == id) { pickSchematic(SchematicPick::Placement, static_cast<int>(i)); return; }
}
void click(float x, float y, bool right) { handleSchematicClick(x, y, right); }
void key(int key) { handleSchematicKey(key); }
void wheel(int step, glm::vec2 pointer, bool shift) {
    auto const& l = schematicsDisplayed;
    float px = pointer.x, py = pointer.y;
    // Over the 3D preview the wheel zooms it.
    if (auto& t = previewTurn(); t.w > 0 && px >= t.x && px < t.x + t.w && py >= t.y && py < t.y + t.h) {
        // Shift moves the slice with the wheel: up raises it (a level comes back), down lowers it.
        if (shift) {
            int const height = std::max(1, schematic::preview::last().layers);
            int const next = std::clamp(t.peel + (step < 0 ? -1 : 1), 0, height - 1);
            if (next != t.peel) {
                // Slicing stops the Files preview's own turn where it is, so the cut stays in view.
                if (!t.manual) { t.yaw = shownYaw(t); t.manual = true; }
                t.peel = next;
            }
        } else t.zoom = std::clamp(t.zoom * (step < 0 ? 1.2f : 1 / 1.2f), .5f, schematic::preview::last().maxZoom);
        return;
    }
    // Over the Check pane's item lines when they do not fit (L-183).
    if (auto& s = itemScroll; s.w > 0 && px >= s.x && px < s.x + s.w && py >= s.y && py < s.y + s.h) {
        s.first = std::clamp(s.first + step, 0, std::max(0, s.count - s.visible));
        return;
    }
    bool overList = px >= l.listLeft && px < l.listLeft + l.listWidth && (!l.docked || py < l.detailTop);
    if (overList) schematicListFirst = std::max(0, schematicListFirst + step);
    else schematicFieldFirst = std::max(0, schematicFieldFirst + step);
}
void release() {
    // A press that did not move is a click on a block (inspect).
    if (auto& t = previewTurn(); std::exchange(t.dragging, false) && !t.turning) {
        t.yaw = t.fromYaw;
        t.pitch = t.fromPitch;
        pickInPreview(t.from);
    }
}
void drag(glm::vec2 pointer) {
    // A drag turns only after the pointer moved a little, so a click on
    // a block does not nudge the view.
    if (auto& t = previewTurn(); t.dragging && (t.turning || glm::length(pointer - t.from) >= 6)) {
        t.turning = true;
        if (!t.manual) {
            // Pick up the turn the Files preview had while turning by itself.
            t.fromYaw = shownYaw(t);
            t.manual = true;
        }
        t.yaw = t.fromYaw - (pointer.x - t.from.x) * .7f;
        t.pitch = std::clamp(t.fromPitch + (pointer.y - t.from.y) * .7f, -60.f, 89.f);
    }
}
bool editingNumber() { return editingSchematicField >= 0; }
void applyNumber() {
    if (editingSchematicField < 0) return;
    int field = editingSchematicField;
    int limit = field == 9 ? 4096 : 30'000'000, low = field == 9 ? 1 : -limit;
    auto parsed = screen::number().parsedPrecise(low, limit, true);
    if (!parsed) { screen::warnRange(translated("integerRange", low, limit)); return; }
    int value = static_cast<int>(*parsed);
    // Fetch the structure before changeSchematic takes the session lock: the
    // structure lookup locks the same mutex.
    auto const* selected = selectedPlacement();
    if (!selected) return;
    std::shared_ptr<schematic::Structure const> structure;
    if (field == 9) structure = schematic::session::structure(selected->file);
    if (field == 9 && !structure) { screen::setMessage(translated("schematic.layerNeedsStructure")); return; }
    // Leaving the field applies it once more as it stands: a number that is
    // already the value changes nothing, so an untouched Layer stays in All.
    bool backToAll = false;
    if (field == 9) {
        auto view = schematic::resolvedLayers(selected->layers, structure->size, selected->placement);
        backToAll = layerEditAllCut && value - 1 == *layerEditAllCut && view.mode != schematic::LayerMode::All;
        bool same = value - 1 == view.cut || (!selected->layers.legacy && value - 1 == selected->layers.cut);
        if (same && !backToAll) { screen::clearMessage(); return; }
    } else if (auto const& o = selected->placement.origin; (field == 0 ? o.x : field == 1 ? o.y : o.z) == value) {
        screen::clearMessage();
        return;
    }
    changeSchematic([&](schematic::SavedPlacement& p) {
        if (field == 9) {
            schematic::editLayers(p.layers, structure->size, p.placement, [&](schematic::Layers& layers) {
                layers.cut = value - 1;
                if (backToAll) layers.mode = schematic::LayerMode::All;
                else schematic::leaveAll(layers);
            });
        }
        else (field == 0 ? p.placement.origin.x : field == 1 ? p.placement.origin.y : p.placement.origin.z) = value;
    });
}
void endEditing() { editingSchematicField = -1; layerEditAllCut.reset(); }
glm::vec2 caret() { return {schematicsDisplayed.stepperX(), schematicsDisplayed.fieldY(std::max(0, editingSchematicField))}; }
bool docked() { return schematicsDocked; }
void renderDocked(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) { renderSchematicsDocked(context, size, pointer); }
void renderContent(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer, SettingsTable const& table) {
    renderSchematicsContent(context, size, pointer, table);
}
void reset() {
    endEditing();
    schematicDeleteArmed = verifyUndoAllArmed = false;
    ignoreView = false;
    placementRows.clearSeveral(); // a closed screen keeps one row, not several; the picked one stays
    if (checkRows.clearSeveral()) followCheckRows();
    placementBulk.disarm();
    // Opened again, the Files preview turns by itself and both show their hint.
    filesTurn.manual = checkTurn.manual = false;
}
}

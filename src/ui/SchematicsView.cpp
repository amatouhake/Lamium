#include "ui/SchematicsView.h"
#include "ui/ScreenParts.h"
#include "ui/ListViewWidgets.h"
#include "settings/Options.h"
#include "ui/SettingsRows.h"
#include "ui/SettingsNavigation.h"
#include "ui/SettingsTable.h"
#include "ui/ShapeEditor.h"
#include "ui/ShapesLayout.h"
#include "ui/ShapesView.h"
#include "ui/WaypointsView.h"
#include "ui/ScreenParts.h"
#include "ui/ListViewWidgets.h"
#include "ui/WaypointPromptLayout.h"
#include "ui/SavePromptLayout.h"
#include "ui/RadialLayout.h"
#include "ui/Animations.h"
#include "ui/Toast.h"
#include "features/map/WaypointSession.h"
#include "app/SessionIds.h"
#include "features/schematic/SchematicSession.h"
#include "features/schematic/Preview.h"
#include "features/schematic/GhostRenderer.h"
#include "ui/SchematicFiles.h"
#include "features/schematic/MaterialAmount.h"
#include "app/Desktop.h"
#include "features/information/SchematicTarget.h"
#include "features/schematic/SchematicItems.h"
#include "features/schematic/Selection.h"
#include "features/schematic/SchematicActions.h"
#include "features/schematic/MenuModel.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/deps/nbt/Tag.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/ItemStack.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/actor/ItemRenderer.h"
#include "features/map/MapStore.h"
#include "features/map/WorldMap.h"
#include "ui/SearchQuery.h"
#include "ui/NumberInput.h"
#include "ui/Widgets.h"
#include "overlay/ShapeSession.h"
#include "ui/Localization.h"
#include "app/Runtime.h"
#include "app/Desktop.h"
#include "app/Versions.h"
#include "features/camera/CameraSessions.h"
#include "features/inspection/render/ItemIcon.h"
#include "features/information/InfoHud.h"
#include "features/information/HungerTrace.h"
#include "features/information/SaturationHud.h"
#include "features/information/InfoLines.h"
#include "ui/HudEditor.h"
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
#include <stdexcept>
#include <vector>


namespace lamium::ui::schematics_view {
namespace {
using ShapeZone = ShapesLayout::Zone;
// The schematic preview (L-114): where it was drawn, how it is turned, and a
// drag in progress. It turns by itself until the player first drags it.
struct PreviewTurn {
    float x = 0, y = 0, w = 0, h = 0; // last drawn box, GUI units
    float yaw = 35, pitch = 30, zoom = 1;
    int peel = 0, peelAxis = -1, peelSign = 1;
    // The block clicked in the preview (structure cell) and its file.
    std::optional<schematic::preview::Cell> picked;
    std::string pickedFile;
    std::string file; // the structure shown; a new one starts whole and fitted
    bool manual = false, dragging = false, turning = false; // turning: this drag moved far enough to turn
    glm::vec2 from{};
    float fromYaw = 0, fromPitch = 0;
} previewTurn;
// The preview's corner line: the hint while it turns by itself, then the
// peeled layers ("layer n/m"), which a click resets. Kept at the top edge:
// the preview's depth hides text drawn over the model.
void drawPreviewLabel(MinecraftUIRenderContext& context, float x, float y, float w) {
    auto const& t = previewTurn;
    if (t.peel > 0) {
        auto last = schematic::preview::last();
        static constexpr std::array<std::string_view, 3> axes{"schematic.previewAxis.x", "schematic.previewAxis.y", "schematic.previewAxis.z"};
        std::string text = translated("schematic.previewLayers", last.layers - t.peel, last.layers,
            last.axis >= 0 ? translated(axes[static_cast<size_t>(last.axis)]) : std::string());
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
int schematicIndex = -1; // into the placements or the files
schematic::PlacementSet schematicSet;
std::vector<schematic::session::FileEntry> schematicFiles;
std::vector<schematic_files::Row> schematicFileRows; // the Files list: folder headings and files
// Blocks other than air and structure void, counted once per loaded structure.
std::map<schematic::Structure const*, std::uint64_t> schematicBlockCounts;
std::uint64_t blockCount(schematic::Structure const& structure) {
    auto [found, fresh] = schematicBlockCounts.try_emplace(&structure, 0);
    if (fresh) {
        if (schematicBlockCounts.size() > 256) { schematicBlockCounts.clear(); return blockCount(structure); }
        for (auto index : structure.blocks)
            if (index != schematic::voidCell && !structure.palette[static_cast<size_t>(index)].isAir()) ++found->second;
    }
    return found->second;
}
std::chrono::steady_clock::time_point schematicFilesScanned{};
std::string largeSchematicConfirmed; // a large file the player chose to load
ShapesLayout schematicsDisplayed;
int schematicListFirst = 0, schematicFieldFirst = 0, schematicFieldSelected = -1;
bool schematicDeleteArmed = false;
int editingSchematicField = -1;
void changeSchematic(std::function<void(schematic::SavedPlacement&)> const& apply);
// ---- Schematics view (L-93) ----
// Four tabs share the list-and-detail layout: placements, files, the
// verification of the selected placement, and its materials.
enum class SchematicField { X, Y, Z, Rotation, Mirror, MoveHere, Visible, LayerAxis, LayerMode, Layer, MatchLayer, Extras, Entities };
constexpr auto schematicFields = std::to_array<SchematicField>({SchematicField::X, SchematicField::Y, SchematicField::Z,
    SchematicField::Rotation, SchematicField::Mirror, SchematicField::MoveHere, SchematicField::Visible, SchematicField::LayerAxis,
    SchematicField::LayerMode, SchematicField::Layer, SchematicField::MatchLayer, SchematicField::Extras, SchematicField::Entities});
int verifyFilter = 0; // 0 mistakes, 1 wrong or extra, 2 wrong state, 3 not placed
int verifySelected = -1;
bool materialsShownOnly = false;
std::shared_ptr<schematic::Verification const> verification;
std::vector<schematic::Mismatch const*> verifyRows;
std::vector<schematic::MaterialLine const*> materialRows; // blocks first, then entities
// The Materials list: -1 a "Blocks" heading, -2 an "Entities" heading, else an
// index into materialRows. Headings only when both kinds are present.
std::vector<int> materialListRows;
int materialSelected = -1; // index into materialRows

bool verifyMatches(schematic::Mismatch const& m) {
    using schematic::CellState;
    switch (verifyFilter) {
    case 1: return m.state == CellState::Wrong || m.state == CellState::Extra;
    case 2: return m.state == CellState::State;
    case 3: return m.state == CellState::Missing;
    default: return m.state != CellState::Missing;
    }
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
    verification = schematic::ghosts::verification();
    verifyRows.clear();
    materialRows.clear();
    if (verification->placement >= 0 && verification->placement == schematicSet.selected) {
        for (auto const& m : verification->mismatches) if (verifyMatches(m)) verifyRows.push_back(&m);
        for (auto const& line : materialsShownOnly ? verification->visibleMaterials : verification->materials) materialRows.push_back(&line);
        std::stable_partition(materialRows.begin(), materialRows.end(), [](auto const* line) { return !line->entity; });
    }
    materialListRows.clear();
    bool blocks = false, entities = false;
    for (auto const* line : materialRows) (line->entity ? entities : blocks) = true;
    for (int i = 0; i < static_cast<int>(materialRows.size()); ++i) {
        bool entity = materialRows[static_cast<size_t>(i)]->entity;
        if (blocks && entities && (i == 0 || entity != materialRows[static_cast<size_t>(i - 1)]->entity))
            materialListRows.push_back(entity ? -2 : -1);
        materialListRows.push_back(i);
    }
    if (materialSelected >= static_cast<int>(materialRows.size())) materialSelected = -1;
    if (verifySelected >= static_cast<int>(verifyRows.size())) verifySelected = -1;
}
int schematicRowCount() {
    switch (schematicTab) {
    case SchematicTab::Placements: return static_cast<int>(schematicSet.placements.size());
    case SchematicTab::Files: return static_cast<int>(schematicFileRows.size());
    case SchematicTab::Verify: return static_cast<int>(verifyRows.size());
    default: return static_cast<int>(materialListRows.size());
    }
}
schematic::SavedPlacement const* selectedPlacement() {
    return schematicTab == SchematicTab::Placements && schematicPick == SchematicPick::Placement && schematicIndex >= 0
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
    schematicFieldFirst = 0;
    schematicFieldSelected = -1;
    schematicDeleteArmed = false;
    // The selected placement is the one keys, the HUD and the Verify tab act on.
    if (pick == SchematicPick::Placement && schematicSet.selected != index)
        schematic::session::change([&](schematic::PlacementSet& set) { set.selected = index; return true; });
    refreshSchematics(false);
}
void selectSchematicTab(SchematicTab tab) {
    screen::finishEditing();
    schematicTab = tab;
    schematicListFirst = schematicFieldFirst = 0;
    schematicFieldSelected = -1;
    schematicDeleteArmed = false;
    verifySelected = -1;
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
    return std::max(1, schematic::layerCount(schematic::placedSize(structure->size, p.placement.rotation), p.layers.axis));
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
    refreshSchematics(false);
    selectSchematicTab(SchematicTab::Placements);
    pickSchematic(SchematicPick::Placement, static_cast<int>(schematicSet.placements.size()) - 1);
}
void deleteSelectedPlacement() {
    int index = schematicIndex;
    if (!schematic::session::change([&](schematic::PlacementSet& set) {
            if (index < 0 || index >= static_cast<int>(set.placements.size())) return false;
            set.placements.erase(set.placements.begin() + index);
            if (set.selected == index) set.selected = -1;
            else if (set.selected > index) --set.selected;
            return true;
        })) { screen::setMessage(translated("schematic.saveError")); return; }
    screen::clearMessage();
    refreshSchematics(false);
    int left = static_cast<int>(schematicSet.placements.size());
    if (left) pickSchematic(SchematicPick::Placement, std::min(index, left - 1));
    else pickSchematic(SchematicPick::None, -1);
}
void showSelectedMismatch() {
    if (verifySelected >= 0 && verifySelected < static_cast<int>(verifyRows.size())) {
        schematic::ghosts::point(verifyRows[static_cast<size_t>(verifySelected)]->position);
        screen::close();
        return;
    }
    // A block clicked in the preview that has no row (past the list's limit).
    auto const& t = previewTurn;
    auto const* checked = checkedPlacement();
    if (!t.picked || !checked || t.pickedFile != checked->file) return;
    auto structure = schematic::session::structure(checked->file);
    if (!structure) return;
    schematic::ghosts::point(schematic::toWorld(structure->size, checked->placement, {t.picked->x, t.picked->y, t.picked->z}));
    screen::close();
}
int schematicFieldCount() {
    switch (schematicTab) {
    case SchematicTab::Placements: return selectedPlacement() ? static_cast<int>(schematicFields.size()) : 0;
    case SchematicTab::Materials: return 2;
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
            (index == 0 ? t.placement.origin.x : index == 1 ? t.placement.origin.y : t.placement.origin.z) += direction;
        });
        return;
    }
    case SchematicField::Rotation:
        changeSchematic([&](schematic::SavedPlacement& t) { t.placement.rotation = schematic::quarterTurns(t.placement.rotation + direction); });
        return;
    case SchematicField::Mirror:
        changeSchematic([&](schematic::SavedPlacement& t) {
            t.placement.mirror = static_cast<schematic::Mirror>((static_cast<int>(t.placement.mirror) + direction + 3) % 3);
        });
        return;
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
        auto placed = structure ? schematic::placedSize(structure->size, p->placement.rotation) : schematic::Size{1, 1, 1};
        changeSchematic([&](schematic::SavedPlacement& t) {
            t.layers = schematic::withAxis(t.layers, placed,
                static_cast<schematic::LayerAxis>((static_cast<int>(t.layers.axis) + direction + 6) % 6));
        });
        return;
    }
    case SchematicField::LayerMode:
        changeSchematic([&](schematic::SavedPlacement& t) {
            t.layers.mode = static_cast<schematic::LayerMode>((static_cast<int>(t.layers.mode) + direction + 3) % 3);
        });
        return;
    case SchematicField::Layer: {
        int count = placedLayers(*p);
        if (part == 0) { editingSchematicField = index; screen::number().beginPrecise(p->layers.index + 1); screen::clearMessage(); return; }
        changeSchematic([&](schematic::SavedPlacement& t) { t.layers.index = std::clamp(t.layers.index + direction, 0, count - 1); });
        return;
    }
    case SchematicField::MatchLayer: {
        // The layer the player stands in, along the chosen direction; showing
        // all layers switches to "this layer only".
        auto place = screen::standingPlace();
        auto structure = schematic::session::structure(p->file);
        if (!place || !structure) return;
        auto placed = schematic::placedSize(structure->size, p->placement.rotation);
        schematic::Point offset{place->x - p->placement.origin.x, place->y - p->placement.origin.y, place->z - p->placement.origin.z};
        int layer = schematic::layerOf(placed, p->layers.axis, offset);
        int count = schematic::layerCount(placed, p->layers.axis);
        changeSchematic([&](schematic::SavedPlacement& t) {
            t.layers.index = std::clamp(layer, 0, count - 1);
            if (t.layers.mode == schematic::LayerMode::All) t.layers.mode = schematic::LayerMode::Only;
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
    int visible = schematicsDisplayed.fieldVisible;
    if (visible > 0) {
        if (schematicFieldSelected < schematicFieldFirst) schematicFieldFirst = schematicFieldSelected;
        if (schematicFieldSelected >= schematicFieldFirst + visible) schematicFieldFirst = schematicFieldSelected - visible + 1;
    }
}
void openSchematicKeySettings() { screen::showFeatureKeys("schematic"); }
// The tab strip sits in the list pane's toolbar.
constexpr int schematicTabCount = 4;
float schematicTabWidth(ShapesLayout const& l) { return (l.listWidth - 2 * ShapesLayout::pad - 3 * 2) / schematicTabCount; }
float schematicTabX(ShapesLayout const& l, int tab) { return l.listLeft + ShapesLayout::pad + tab * (schematicTabWidth(l) + 2); }
int schematicTabAt(ShapesLayout const& l, float x, float y) {
    if (!l.usable() || y < l.toolbarTop || y >= l.toolbarTop + ShapesLayout::toolbarHeight) return -1;
    for (int i = 0; i < schematicTabCount; ++i)
        if (x >= schematicTabX(l, i) && x < schematicTabX(l, i) + schematicTabWidth(l)) return i;
    return -1;
}
// The Check tab's four filters, in the list's heading row (L-93 screen review):
// sized to their text and drawn as outlined pills, so they read as part of the
// list rather than more tabs. Their spans are kept from the last draw.
std::array<std::pair<float, float>, 4> verifyChipSpans{};
int verifyChipAt(ShapesLayout const& l, float x, float y) {
    if (schematicTab != SchematicTab::Verify || y < l.theadTop || y >= l.rowsTop) return -1;
    for (int i = 0; i < 4; ++i)
        if (x >= verifyChipSpans[static_cast<size_t>(i)].first && x < verifyChipSpans[static_cast<size_t>(i)].second) return i;
    return -1;
}
int stackSizeOf(schematic::MaterialLine const& line) {
    auto const* stack = schematic::items::iconStack(line.icon);
    return stack ? std::max(1, static_cast<int>(stack->getMaxStackSize())) : 64;
}
// "1 chest + 4 stacks + 16", number first (L-93 screen review).
std::string amountText(std::uint64_t count, int maxStack) {
    auto a = schematic::amountOf(count, maxStack);
    std::vector<std::string> parts;
    if (a.chests) parts.push_back(translated(a.chests == 1 ? "amount.chest" : "amount.chests", a.chests));
    if (a.stacks) parts.push_back(translated(a.stacks == 1 ? "amount.stack" : "amount.stacks", a.stacks));
    if (a.items || parts.empty()) parts.push_back(std::to_string(a.items));
    std::string out;
    for (auto const& part : parts) out += (out.empty() ? "" : " + ") + part;
    return out;
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
    auto& t = previewTurn;
    t.picked = schematic::preview::pickAt(at.x, at.y);
    t.pickedFile = t.file;
    if (schematicTab != SchematicTab::Verify || !t.picked) return;
    auto const* checked = checkedPlacement();
    auto structure = checked ? schematic::session::structure(checked->file) : nullptr;
    if (!structure) return;
    auto world = schematic::toWorld(structure->size, checked->placement, {t.picked->x, t.picked->y, t.picked->z});
    verifySelected = -1;
    for (size_t i = 0; i < verifyRows.size(); ++i)
        if (!verifyRows[i]->entity && verifyRows[i]->position == world) { verifySelected = static_cast<int>(i); break; }
    if (verifySelected >= 0) scrollToSelected = true;
}
void handleSchematicClick(float x, float y, bool right) {
    screen::finishEditing();
    if (!right && screen::pressScrollbar(schematicsDisplayed, schematicListFirst, x, y)) return;
    auto& t = previewTurn;
    // The peel line across the top of the preview resets the cut.
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
    if (int chip = verifyChipAt(schematicsDisplayed, x, y); chip >= 0) {
        verifyFilter = chip;
        verifySelected = -1;
        schematicListFirst = 0;
        refreshSchematics(false);
        return;
    }
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
            if (x >= l.listLeft + l.listWidth - ShapesLayout::pad - switchWidth - 2) {
                schematic::session::change([&](schematic::PlacementSet& set) {
                    if (row < 0 || row >= static_cast<int>(set.placements.size())) return false;
                    set.placements[static_cast<size_t>(row)].visible = !set.placements[static_cast<size_t>(row)].visible;
                    return true;
                });
                refreshSchematics(false);
                return;
            }
            if (schematicPick != SchematicPick::Placement || row != schematicIndex) pickSchematic(SchematicPick::Placement, row);
            return;
        case SchematicTab::Files: {
            if (row < 0 || row >= static_cast<int>(schematicFileRows.size())) return;
            int file = schematicFileRows[static_cast<size_t>(row)].file;
            if (file >= 0 && (schematicPick != SchematicPick::File || file != schematicIndex)) pickSchematic(SchematicPick::File, file);
            return;
        }
        case SchematicTab::Verify: verifySelected = row; return;
        default:
            if (row >= 0 && row < static_cast<int>(materialListRows.size()) && materialListRows[static_cast<size_t>(row)] >= 0)
                materialSelected = materialListRows[static_cast<size_t>(row)];
            return;
        }
    }
    case ShapeZone::Field: activateSchematicField(hit.index, right ? -1 : hit.part); return;
    case ShapeZone::Action:
        if (schematicTab == SchematicTab::Files) {
            if (hit.index == 0) placeSelectedFile();
            else if (!schematic::session::openFolder()) screen::setMessage(translated("schematic.openFolderError"));
            return;
        }
        if (schematicTab == SchematicTab::Verify) { if (hit.index == 0) showSelectedMismatch(); return; }
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
        switch (key) {
        case 0x08: if (screen::number().backspace()) screen::numberTyped(); break;
        case 0x41: if (screen::heldCtrl()) screen::number().selectAll(); break;
        case 0x1b: case 0x0d: case 0x09: screen::finishEditing(); break;
        }
        return;
    }
    switch (key) {
    case 0x1b: screen::close(); break;
    case 0x26: moveSchematicField(-1); break;
    case 0x28: moveSchematicField(1); break;
    case 0x25: activateSchematicField(schematicFieldSelected, -1); break;
    case 0x27: activateSchematicField(schematicFieldSelected, 1); break;
    case 0x0d: case 0x20:
        if (schematicTab == SchematicTab::Files) placeSelectedFile();
        else if (schematicTab == SchematicTab::Verify) showSelectedMismatch();
        else activateSchematicField(schematicFieldSelected, 0);
        break;
    case 0x21: case 0x22: {
        int count = schematicRowCount();
        if (!count) break;
        int current = schematicTab == SchematicTab::Verify ? verifySelected
            : (schematicTab == SchematicTab::Placements && schematicPick == SchematicPick::Placement)
                || (schematicTab == SchematicTab::Files && schematicPick == SchematicPick::File) ? schematicIndex : -1;
        int step = key == 0x22 ? 1 : -1;
        if (schematicTab == SchematicTab::Files) {
            // Rows and files differ: step over folder headings.
            int at = current >= 0 ? schematic_files::rowOf(schematicFileRows, current) : (step > 0 ? -1 : count);
            for (int r = at + step; r >= 0 && r < count; r += step)
                if (int file = schematicFileRows[static_cast<size_t>(r)].file; file >= 0) { pickSchematic(SchematicPick::File, file); break; }
            break;
        }
        int row = std::clamp(current + step, 0, count - 1);
        if (schematicTab == SchematicTab::Verify) verifySelected = row;
        else if (schematicTab == SchematicTab::Placements) pickSchematic(SchematicPick::Placement, row);
        else if (schematicTab == SchematicTab::Files) pickSchematic(SchematicPick::File, row);
        break;
    }
    case 0x09: screen::nextNav(screen::heldShift()); break;
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
    case SchematicTab::Verify: return translated("schematic.verifyHint");
    default: return translated("schematic.materialsHint");
    }
}
std::string fieldValue(schematic::SavedPlacement const& p, SchematicField field) {
    switch (field) {
    case SchematicField::X: return std::to_string(p.placement.origin.x);
    case SchematicField::Y: return std::to_string(p.placement.origin.y);
    case SchematicField::Z: return std::to_string(p.placement.origin.z);
    case SchematicField::Rotation: return std::format("{}°", p.placement.rotation * 90);
    case SchematicField::Mirror:
        return translated(p.placement.mirror == schematic::Mirror::X ? "schematic.mirror.x"
            : p.placement.mirror == schematic::Mirror::Z ? "schematic.mirror.z" : "schematic.mirror.none");
    case SchematicField::LayerAxis: {
        static constexpr std::array<std::string_view, 6> axes{"schematic.axis.up", "schematic.axis.down", "schematic.axis.east",
            "schematic.axis.west", "schematic.axis.south", "schematic.axis.north"};
        return translated(axes[static_cast<size_t>(p.layers.axis)]);
    }
    case SchematicField::LayerMode:
        return translated(p.layers.mode == schematic::LayerMode::Only ? "schematic.mode.only"
            : p.layers.mode == schematic::LayerMode::UpTo ? "schematic.mode.upTo" : "schematic.mode.all");
    case SchematicField::Layer: return translated("schematic.layerValue", p.layers.index + 1, placedLayers(p));
    case SchematicField::Extras: return translated(p.countExtras ? "schematic.extras.show" : "schematic.extras.ignore");
    default: return {};
    }
}
std::string layersText(schematic::SavedPlacement const& p) {
    if (p.layers.mode == schematic::LayerMode::All) return translated("schematic.mode.all");
    return fieldValue(p, SchematicField::LayerAxis) + " " + fieldValue(p, SchematicField::Layer) + " "
        + fieldValue(p, SchematicField::LayerMode);
}
void drawItemIcon(MinecraftUIRenderContext& context, std::string const& icon, float x, float y, float size) {
    auto const* stack = schematic::items::iconStack(icon);
    if (!stack) return;
    inspection::render::drawItemIcon(context, {stack, std::round(x), std::round(y), size / 16}, 17);
}
std::map<std::string, std::uint64_t> carriedItems() {
    auto* player = screen::client() ? screen::client()->getLocalPlayer() : nullptr;
    return player ? schematic::items::carried(*player) : std::map<std::string, std::uint64_t>{};
}
std::string mismatchKind(schematic::CellState state) {
    switch (state) {
    case schematic::CellState::Wrong: return translated("schematic.kind.wrong");
    case schematic::CellState::Extra: return translated("schematic.kind.extra");
    case schematic::CellState::State: return translated("schematic.kind.state");
    default: return translated("schematic.kind.missing");
    }
}
Rgb mismatchColor(schematic::CellState state) {
    switch (state) {
    case schematic::CellState::Wrong: case schematic::CellState::Extra: return {1.f, .35f, .3f};
    case schematic::CellState::State: return {1.f, .8f, .25f};
    default: return {.75f, .85f, .9f};
    }
}
void drawSchematicsBody(MinecraftUIRenderContext& context, ShapesLayout const& l, glm::vec2 pointer) {
    auto const preferences = Runtime::instance().preferences();
    auto hover = l.hit(pointer.x, pointer.y);
    int tabHover = schematicTabAt(l, pointer.x, pointer.y);
    auto over = [&](ShapeZone zone, int index = -1) { return tabHover < 0 && hover.zone == zone && (index < 0 || hover.index == index); };
    float top = l.top + 4;
    label(context,l.drawAllX,l.drawAllY+1+boxTextInset(),l.drawAllWidth-switchWidth-4,translated("waypoint.showAll"),
        over(ShapeZone::DrawAll) ? palette::text : palette::dim,Align::Right);
    toggleSwitch(context,l.drawAllX+l.drawAllWidth-switchWidth,l.drawAllY+2,preferences.schematic.enabled);
    label(context,l.keysX,top+1+boxTextInset(),ShapesLayout::keysWidth,translated("shape.keys"),
        over(ShapeZone::Keys) ? palette::text : palette::accent,Align::Center);
    fill(context,l.keysX+6,top+11,ShapesLayout::keysWidth-12,1,palette::accent,over(ShapeZone::Keys) ? 1.f : .5f);
    drawSmallButton(context,l.dockX,top,ShapesLayout::dockWidth,12,translated(schematicsDocked ? "shape.undock" : "shape.dock"),over(ShapeZone::Dock));

    // Tabs.
    static constexpr std::array<std::string_view, schematicTabCount> tabNames{"schematic.tab.files", "schematic.tab.placements",
        "schematic.tab.verify", "schematic.tab.materials"};
    for (int i = 0; i < schematicTabCount; ++i) {
        bool active = static_cast<int>(schematicTab) == i;
        float x = schematicTabX(l, i), w = schematicTabWidth(l);
        drawSmallButton(context,x,l.toolbarTop+2,w,12,translated(tabNames[static_cast<size_t>(i)]),tabHover == i,
            active ? palette::accentDeep : palette::keyFill, active ? palette::accent : palette::keyEdge,
            active ? palette::text : palette::dim);
    }
    fill(context,l.listLeft,l.theadTop-1,l.listWidth,1,palette::white,.14f);

    // List.
    float listRight = l.listLeft + l.listWidth;
    float left = l.listLeft + ShapesLayout::pad;
    float shownX = listRight - ShapesLayout::pad - switchWidth - 2;
    auto heading = [&](float x, float w, std::string_view key, Align align = Align::Left) {
        label(context,x,l.theadTop+2,w,translated(key),palette::faint,align);
    };
    auto* checked = checkedPlacement();
    // A placement whose file cannot be loaded is never counted: say why instead.
    std::string loadProblem;
    bool unloadable = checked && (schematicTab == SchematicTab::Verify || schematicTab == SchematicTab::Materials)
        && !schematic::session::structure(checked->file, &loadProblem);
    bool counting = checked && !unloadable && (!verification->complete || verification->placement != schematicSet.selected);
    // Column positions for the Verify and Materials lists.
    // A narrow list (a large UI) drops the columns it can do without, so the
    // names keep their room: the position in Check, "placed" in Materials.
    // Files: size and block count on the right; size goes first when narrow.
    float blocksW = 40, blocksX = listRight - ShapesLayout::pad - blocksW, sizeW = 54, sizeX = blocksX - 4 - sizeW;
    bool showSize = sizeX - left >= 80;
    bool loadedOne = false; // At most one file is read per frame to fill the columns.
    // Placed: progress left of the switch; coordinates only while the name
    // keeps room (the detail pane always shows them).
    float progressW = 26, progressX = shownX - 6 - progressW;
    float coordsW = 66, coordsX = progressX - 4 - coordsW;
    bool showCoords = coordsX - left >= 70;
    if (schematicTab == SchematicTab::Placements) schematic::ghosts::wantProgress();
    float kindW = 38, distW = 28, posW = listRight - left - kindW - distW - ShapesLayout::pad - 78 >= 90 ? 78.f : 0.f;
    float numW = 34, carriedX = listRight - ShapesLayout::pad - numW, leftX = carriedX - numW - 2;
    bool showPlaced = leftX - 2 * (numW + 2) - left - 16 >= 70;
    float placedX = leftX - numW - 2, neededX = (showPlaced ? placedX : leftX) - numW - 2;
    switch (schematicTab) {
    case SchematicTab::Placements:
        heading(left, shownX - left - 74, "shape.columnName");
        heading(progressX - 2, progressW + 4, "schematic.column.progress", Align::Right);
        heading(shownX - 6, switchWidth + 12, "shape.columnShown", Align::Center);
        break;
    case SchematicTab::Files:
        heading(left, blocksX - left - 4, "shape.columnName");
        if (showSize) heading(sizeX, sizeW, "schematic.column.size", Align::Right);
        heading(blocksX, blocksW, "schematic.column.blockCount", Align::Right);
        break;
    case SchematicTab::Verify: {
        // The filters replace the column headings; each shows how many rows it has.
        static constexpr std::array<std::string_view, 4> chips{"schematic.filter.mistakes", "schematic.filter.wrong",
            "schematic.filter.state", "schematic.filter.missing"};
        auto const& t = verification->visible;
        bool counted = checked && !counting;
        std::array<std::uint64_t, 4> counts{t.wrong + t.state + t.extra, t.wrong + t.extra, t.state, t.missing};
        float cx = left;
        for (int i = 0; i < 4; ++i) {
            bool on = verifyFilter == i, hovered = verifyChipAt(l,pointer.x,pointer.y) == i;
            std::string text = translated(chips[static_cast<size_t>(i)]);
            if (counted) text += std::format(" {}", counts[static_cast<size_t>(i)]);
            constexpr float chipText = .8f;
            float w = std::min(textWidthScaled(context, text, chipText) + 8, listRight - ShapesLayout::pad - cx);
            if (w <= 8) break;
            float top = l.theadTop + 1, h = ShapesLayout::theadHeight - 2;
            if (on) fill(context,cx,top,w,h,palette::accent,.15f);
            if (hovered) fill(context,cx,top,w,h,palette::white,.08f);
            frame(context,cx,top,w,h,on ? palette::accent : palette::keyEdge);
            labelScaled(context,cx+4,top+(h-8*chipText)/2-1+boxTextInset(),w-6,std::move(text),chipText,on ? palette::accent : palette::dim);
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
    fill(context,l.listLeft,l.rowsTop-1,l.listWidth,1,palette::white,.14f);
    std::string empty;
    if (l.listCount == 0) {
        if (schematicTab == SchematicTab::Placements) empty = translated("schematic.noPlacements");
        else if (schematicTab == SchematicTab::Files) empty = translated("schematic.empty");
        else if (!checked) empty = translated("schematic.noSelection");
        else if (unloadable) empty = translated("schematic.notLoaded", loadProblem);
        else if (counting) empty = translated("schematic.counting");
        else empty = translated(schematicTab == SchematicTab::Verify ? "schematic.noMistakes" : "schematic.noMaterials");
        paragraph(context,left,l.rowsTop+3,l.listWidth-2*ShapesLayout::pad,empty,4,palette::faint);
    }
    auto* player = screen::client() ? screen::client()->getLocalPlayer() : nullptr;
    Vec3 feet = player ? player->getFeetPos() : Vec3{0, 0, 0};
    auto carried = schematicTab == SchematicTab::Materials ? carriedItems() : std::map<std::string, std::uint64_t>{};
    struct Tip { std::string text; float x = 0, y = 0; };
    std::optional<Tip> materialTip;
    for (int i = l.listFirst; i < l.listFirst + l.listVisible && i < l.listCount; ++i) {
        float y = l.listRowY(i);
        if (i % 2) fill(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,palette::white,.025f);
        bool chosen = schematicTab == SchematicTab::Verify ? i == verifySelected
            : schematicTab == SchematicTab::Placements ? schematicPick == SchematicPick::Placement && i == schematicIndex
            : schematicTab == SchematicTab::Files ? schematicPick == SchematicPick::File
                && schematicFileRows[static_cast<size_t>(i)].file == schematicIndex : false;
        bool heading = (schematicTab == SchematicTab::Files && schematicFileRows[static_cast<size_t>(i)].file < 0)
            || (schematicTab == SchematicTab::Materials && materialListRows[static_cast<size_t>(i)] < 0);
        if (schematicTab == SchematicTab::Materials && !heading) chosen = materialListRows[static_cast<size_t>(i)] == materialSelected;
        if (!heading) rowBackground(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,chosen,over(ShapeZone::ListRow,i));
        switch (schematicTab) {
        case SchematicTab::Placements: {
            auto const& p = schematicSet.placements[static_cast<size_t>(i)];
            bool here = p.dimension == screen::playerDimension();
            // The selected placement: the accent bar, like the sidebar's current entry.
            if (i == schematicSet.selected) fill(context,l.listLeft+1,y,2,ShapesLayout::rowHeight,palette::accent);
            float nameEnd = showCoords ? coordsX - 4 : progressX - 4;
            label(context,left,y+3,nameEnd-left,p.name,here ? palette::text : palette::faint);
            if (showCoords)
                label(context,coordsX,y+3,coordsW,here ? std::format("{}, {}, {}", p.placement.origin.x, p.placement.origin.y, p.placement.origin.z)
                    : screen::dimensionName(p.dimension),palette::dim,Align::Right);
            if (auto tally = schematic::ghosts::progress(p); tally && tally->total()) {
                float fraction = static_cast<float>(tally->correct) / static_cast<float>(tally->total());
                label(context,progressX,y+1,progressW,std::format("{}%", static_cast<int>(fraction * 100)),
                    here ? palette::text : palette::faint,Align::Right);
                fill(context,progressX,y+11,progressW,1,palette::white,.12f);
                fill(context,progressX,y+11,progressW*fraction,1,palette::accent,here ? 1.f : .5f);
            } else label(context,progressX,y+3,progressW,"-",palette::faint,Align::Right);
            toggleSwitch(context,shownX,y+(ShapesLayout::rowHeight-switchHeight)/2,p.visible);
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
            bool grouped = !schematicFileRows.empty() && schematicFileRows.front().file < 0;
            label(context,left+(grouped ? 6.f : 0.f),y+3,
                  blocksX-left-10,fileTitle(f.relative),palette::text);
            auto structure = schematic::session::loaded(f.relative);
            if (!structure && !loadedOne && f.bytes <= schematic::session::largeFileBytes) {
                loadedOne = true;
                structure = schematic::session::structure(f.relative);
            }
            if (structure) {
                if (showSize) label(context,sizeX,y+3,sizeW,std::format("{}x{}x{}", structure->size.x, structure->size.y, structure->size.z),
                    palette::dim,Align::Right);
                label(context,blocksX,y+3,blocksW,std::to_string(blockCount(*structure)),palette::dim,Align::Right);
            } else label(context,blocksX,y+3,blocksW,"-",palette::faint,Align::Right);
            break;
        }
        case SchematicTab::Verify: {
            auto const& m = *verifyRows[static_cast<size_t>(i)];
            fill(context,left,y+4,6,6,mismatchColor(m.state));
            label(context,left+9,y+3,kindW-10,mismatchKind(m.state),palette::dim);
            if (posW > 0) label(context,left+kindW,y+3,posW-2,std::format("{}, {}, {}", m.position.x, m.position.y, m.position.z),palette::dim);
            float bx = left + kindW + posW, bw = listRight - bx - distW - ShapesLayout::pad - 4;
            // The schematic's block, then what is there, each with its icon.
            auto block = [&](float x, float w, std::string const& icon, std::string const& name, Rgb color) {
                if (!icon.empty()) drawItemIcon(context, icon, x, y + 1, 12);
                label(context,x+14,y+3,w-14,name,color);
            };
            if (m.entity) block(bx, bw, m.expected, translated("schematic.withDetail", m.expectedName, translated("schematic.entityTag")), palette::text);
            else if (m.state == schematic::CellState::Missing || m.state == schematic::CellState::State)
                block(bx, bw, m.expected, m.expectedName, palette::text); // A wrong state: which states, in the right pane.
            else if (m.state == schematic::CellState::Extra) {
                // Same columns as a wrong block: air where the schematic's block would be.
                float half = (bw - 10) / 2;
                label(context,bx+14,y+3,half-14,translated("schematic.air"),palette::faint);
                label(context,bx+half,y+3,10,">",palette::faint,Align::Center);
                block(bx + half + 10, half, m.actual, m.actualName, palette::dim);
            } else {
                float half = (bw - 10) / 2;
                block(bx, half, m.expected, m.expectedName, palette::text);
                label(context,bx+half,y+3,10,">",palette::faint,Align::Center);
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
                if (column) materialTip = {translated(column->first) + ": " + amountText(column->second, stackSizeOf(line)),
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
    drawListScrollbar(context,l);
    if (l.docked) fill(context,l.left,l.detailTop-1,l.width,1,palette::white,.14f);
    else fill(context,l.detailLeft-1,l.toolbarTop,1,l.footerTop-l.toolbarTop,palette::white,.14f);

    // Detail.
    float dx = l.detailLeft + ShapesLayout::pad, dw = l.detailWidth - 2 * ShapesLayout::pad;
    auto info = [&](std::string const& relative, float y) {
        std::string problem;
        auto structure = schematic::session::structure(relative, &problem);
        if (!structure) { paragraph(context,dx,y,dw,translated("schematic.notLoaded", problem),3,palette::warning); return; }
        std::uint64_t blocks = blockCount(*structure);
        label(context,dx,y,dw,translated("schematic.size", structure->size.x, structure->size.y, structure->size.z),palette::dim);
        label(context,dx,y+11,dw,translated("schematic.blocks", blocks),palette::dim);
        label(context,dx,y+22,dw,translated("schematic.entities", structure->entities.size()),palette::dim);
        label(context,dx,y+33,dw,relative,palette::faint);
    };
    auto stepperRow = [&](int i, std::string_view key, std::string value, bool isSwitch, bool on) {
        if (i < l.fieldFirst || i >= l.fieldFirst + l.fieldVisible) return;
        float y = l.fieldY(i);
        rowBackground(context,l.detailLeft+1,y,l.detailWidth-2,ShapesLayout::rowHeight,schematicFieldSelected == i,over(ShapeZone::Field,i));
        label(context,dx,y+3,l.stepperX()-dx-4,translated(key),palette::dim);
        if (isSwitch) toggleSwitch(context,l.stepperX()+l.stepperWidth()-switchWidth,y+(ShapesLayout::rowHeight-switchHeight)/2,on);
        else drawShapeStepper(context,l,y,false,std::move(value),nullptr);
    };
    previewTurn.w = 0; // set again where the preview is drawn this frame
    switch (schematicTab) {
    case SchematicTab::Placements:
        if (auto const* p = selectedPlacement()) {
            label(context,dx,l.nameY+1+boxTextInset(),dw,p->name);
            info(p->file, l.previewY);
            if (auto tally = schematic::ghosts::progress(*p); tally && tally->total())
                label(context,dx,l.previewY+44,dw,translated("schematic.progress",
                    static_cast<int>(100.0 * static_cast<double>(tally->correct) / static_cast<double>(tally->total()))),palette::dim);
            for (int i = l.fieldFirst; i < l.fieldFirst + l.fieldVisible && i < static_cast<int>(schematicFields.size()); ++i) {
                float y = l.fieldY(i);
                auto field = schematicFields[static_cast<size_t>(i)];
                rowBackground(context,l.detailLeft+1,y,l.detailWidth-2,ShapesLayout::rowHeight,schematicFieldSelected == i,over(ShapeZone::Field,i));
                static constexpr std::array<std::string_view, 13> labels{"X", "Y", "Z", "schematic.rotation", "schematic.mirror",
                    "schematic.moveHere", "schematic.shown", "schematic.layerAxis", "schematic.layerMode", "schematic.layer",
                    "schematic.matchLayer", "schematic.extras", "schematic.showEntities"};
                std::string text = i < 3 ? std::string(labels[static_cast<size_t>(i)]) : translated(labels[static_cast<size_t>(i)]);
                label(context,dx,y+3,l.stepperX()-dx-4,text,palette::dim);
                switch (field) {
                case SchematicField::Visible: case SchematicField::Entities:
                    toggleSwitch(context,l.stepperX()+l.stepperWidth()-switchWidth,y+(ShapesLayout::rowHeight-switchHeight)/2,
                        field == SchematicField::Visible ? p->visible : p->entities);
                    break;
                case SchematicField::MoveHere: case SchematicField::MatchLayer:
                    drawSmallButton(context,l.stepperX(),y+1,l.stepperWidth(),ShapesLayout::rowHeight-2,
                        translated(field == SchematicField::MoveHere ? "schematic.moveHere" : "schematic.matchLayerButton"),
                        over(ShapeZone::Field,i));
                    break;
                default: {
                    bool numeric = field == SchematicField::X || field == SchematicField::Y || field == SchematicField::Z
                        || field == SchematicField::Layer;
                    drawShapeStepper(context,l,y,numeric,fieldValue(*p, field),editingSchematicField == i ? &screen::number() : nullptr);
                    break;
                }
                }
            }
            fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
            drawSmallButton(context,l.deleteX(),l.actionsY+2,ShapesLayout::deleteWidth,12,
                translated(schematicDeleteArmed ? "shape.deleteConfirm" : "shape.delete"),over(ShapeZone::Action,1),
                schematicDeleteArmed ? Rgb{.54f,.18f,.16f} : palette::keyFill,Rgb{.54f,.23f,.2f},
                schematicDeleteArmed ? palette::text : Rgb{1.f,.7f,.68f});
        } else paragraph(context,dx,l.detailTop+6,dw,translated(schematicSet.placements.empty() ? "schematic.noPlacements"
            : "schematic.selectHint"),4,palette::faint);
        break;
    case SchematicTab::Files:
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
                    fill(context,dx,top,dw,bottom-top,Rgb{0,0,0},.25f);
                    frame(context,dx,top,dw,bottom-top,palette::white,.1f);
                    auto& t = previewTurn;
                    t.x = dx; t.y = top; t.w = dw; t.h = bottom - top;
                    float yaw = t.yaw;
                    if (!t.manual) yaw += std::fmod(static_cast<float>(std::chrono::duration<double>(
                        std::chrono::steady_clock::now().time_since_epoch()).count()) * 12.f, 360.f);
                    if (t.file != f.relative) { t.file = f.relative; t.peel = 0; t.zoom = 1; t.picked.reset(); }
                    auto structure = schematic::session::structure(f.relative);
                    // The clicked block stays lit, the rest dims.
                    std::optional<schematic::preview::Cell> pick = t.pickedFile == f.relative ? t.picked : std::nullopt;
                    schematic::preview::Tint tint{pick ? static_cast<std::uint64_t>((pick->x * 4099 + pick->y) * 4099 + pick->z) + 7 : 0,
                        [pick](int x, int y, int z) -> std::uint32_t { return pick && !(*pick == schematic::preview::Cell{x, y, z}) ? 0xff737373u : 0xffffffffu; }};
                    if (!schematic::preview::draw(context, structure, dx + 1, top + 1, dw - 2, bottom - top - 2,
                            schematic::preview::View{yaw, t.pitch, t.zoom, t.peel, t.peelAxis, t.peelSign}, pick ? &tint : nullptr))
                        t.w = 0;
                    else drawPreviewLabel(context, dx, top, dw);
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
            drawSmallButton(context,l.actionX(0),l.actionsY+2,l.firstActionWidth,12,translated(waits ? "schematic.loadAnyway" : "schematic.place"),
                over(ShapeZone::Action,0),palette::accentDeep,palette::accent);
        } else paragraph(context,dx,l.detailTop+6,dw,translated(schematicFiles.empty() ? "schematic.empty" : "schematic.fileSelectHint"),
            4,palette::faint);
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
        drawSmallButton(context,l.deleteX(),l.actionsY+2,ShapesLayout::deleteWidth,12,translated("schematic.openFolder"),over(ShapeZone::Action,1));
        break;
    case SchematicTab::Verify: {
        if (!checked) { paragraph(context,dx,l.detailTop+6,dw,translated("schematic.noSelection"),4,palette::faint); break; }
        label(context,dx,l.nameY+1+boxTextInset(),dw,checked->name);
        auto const& t = verification->visible;
        // Layout A (Check tab review): the preview at a fixed size on top,
        // then the counts beside the selected mistake, then its states.
        // Lines that do not fit above the buttons are left out.
        float pvTop = l.previewY - 2, pvHeight = std::clamp((l.actionsY - pvTop) * .55f, 60.f, 240.f);
        float textTop = pvTop + pvHeight + 5, textEnd = l.actionsY - 6;
        auto fits = [&](float y) { return y + 10 <= textEnd; };
        if (unloadable) paragraph(context,dx,pvTop,dw,translated("schematic.notLoaded", loadProblem),3,palette::warning);
        else {
            fill(context,dx,pvTop,dw,pvHeight,Rgb{0,0,0},.25f);
            frame(context,dx,pvTop,dw,pvHeight,palette::white,.1f);
            if (counting) label(context,dx+4,pvTop+3,dw-8,translated("schematic.counting"),palette::dim);
            else if (auto structure = schematic::session::structure(checked->file)) {
                schematic::Point here{INT_MIN, INT_MIN, INT_MIN};
                if (verifySelected >= 0 && verifySelected < static_cast<int>(verifyRows.size()) && !verifyRows[static_cast<size_t>(verifySelected)]->entity)
                    here = verifyRows[static_cast<size_t>(verifySelected)]->position;
                else if (previewTurn.picked && previewTurn.pickedFile == checked->file)
                    here = schematic::toWorld(structure->size, checked->placement, {previewTurn.picked->x, previewTurn.picked->y, previewTurn.picked->z});
                // Only the kinds the chips show are marked.
                static std::map<std::tuple<int, int, int>, schematic::CellState> states;
                static std::uint64_t statesKey = 0;
                std::uint64_t key = reinterpret_cast<std::uintptr_t>(verification.get()) * 31 + static_cast<std::uint64_t>(here.x) * 7
                    + static_cast<std::uint64_t>(here.y) * 13 + static_cast<std::uint64_t>(here.z) + static_cast<std::uint64_t>(verifyFilter) * 1000003 + 1;
                if (key != statesKey) {
                    statesKey = key;
                    states.clear();
                    for (auto const& m : verification->mismatches)
                        if (!m.entity && verifyMatches(m)) states[{m.position.x, m.position.y, m.position.z}] = m.state;
                }
                bool focus = here.x != INT_MIN;
                auto placement = checked->placement;
                auto size = structure->size;
                schematic::preview::Tint tint{key, [placement, size, here, focus](int x, int y, int z) -> std::uint32_t {
                    auto world = schematic::toWorld(size, placement, {x, y, z});
                    if (focus && world == here) return 0xffffffffu;
                    auto found = states.find({world.x, world.y, world.z});
                    std::uint32_t color = 0xffffffffu;
                    if (found != states.end()) switch (found->second) {
                    case schematic::CellState::Missing: color = 0xffffcc8cu; break; // light blue
                    case schematic::CellState::Wrong: case schematic::CellState::Extra: color = 0xff5a64ffu; break; // red
                    case schematic::CellState::State: color = 0xff5ad7ffu; break; // yellow
                    default: break;
                    }
                    if (!focus) return color;
                    // Dim the rest to 45%.
                    auto dim = [&](int shift) { return ((((color >> shift) & 255) * 115 / 255) << shift); };
                    return dim(0) | dim(8) | dim(16) | 0xff000000u;
                }};
                auto& turn = previewTurn;
                turn.x = dx; turn.y = pvTop; turn.w = dw; turn.h = pvHeight;
                if (turn.file != checked->file) { turn.file = checked->file; turn.peel = 0; turn.zoom = 1; turn.picked.reset(); }
                if (!schematic::preview::draw(context, structure, dx + 1, pvTop + 1, dw - 2, pvHeight - 2,
                        schematic::preview::View{turn.yaw, turn.pitch, turn.zoom, turn.peel, turn.peelAxis, turn.peelSign}, &tint))
                    turn.w = 0;
                else drawPreviewLabel(context, dx, pvTop, dw);
            }
        }
        // Counts on the left, the selected mistake on the right.
        float half = std::floor(dw * .42f), rx = dx + half + 8, rw = dw - half - 8;
        float y = textTop;
        label(context,dx,y,half,translated("schematic.check.whole", layersText(*checked)),palette::faint);
        if (!counting && !unloadable) {
            if (fits(y + 11)) label(context,dx,y+11,half,translated("schematic.summary.correct", t.correct, t.total()),palette::accent);
            if (fits(y + 22)) label(context,dx,y+22,half,translated("schematic.summary.missing", t.missing),palette::dim);
            if (fits(y + 33)) label(context,dx,y+33,half,translated("schematic.summary.wrong", t.wrong + t.extra),Rgb{1.f,.45f,.4f});
            if (fits(y + 44)) label(context,dx,y+44,half,translated("schematic.summary.state", t.state),Rgb{1.f,.8f,.3f});
        }
        float bottom = y + 57;
        // The list keeps the nearest of each kind; say so when it is cut.
        std::uint64_t kindTotal = verifyFilter == 1 ? t.wrong + t.extra : verifyFilter == 2 ? t.state : verifyFilter == 3 ? t.missing
            : t.wrong + t.extra + t.state;
        if (!counting && !unloadable && verifyRows.size() < kindTotal && fits(y + 57)) {
            paragraph(context,dx,y+57,dw,translated("schematic.check.listCut", schematic::maxMismatches),3,palette::faint);
            bottom = y + 57 + 12.f * paragraphLines(context, dw, translated("schematic.check.listCut", schematic::maxMismatches), 3) + 2;
        }
        if (verifySelected >= 0 && verifySelected < static_cast<int>(verifyRows.size())) {
            auto const& m = *verifyRows[static_cast<size_t>(verifySelected)];
            fill(context,rx-5,y,1,52,palette::white,.1f);
            label(context,rx,y,rw,translated("schematic.check.here", std::format("{}, {}, {}", m.position.x, m.position.y, m.position.z)),mismatchColor(m.state));
            if (fits(y + 11)) label(context,rx,y+11,rw,mismatchKind(m.state),mismatchColor(m.state));
            // What was confused with what: the expected block above the one in the world.
            if (!m.expectedName.empty() && fits(y + 24)) {
                drawItemIcon(context, m.expected, rx, y + 23, 12);
                label(context,rx+14,y+25,rw-14,translated("schematic.expected", m.expectedName),palette::dim);
            }
            if (!m.actualName.empty() && fits(y + 38)) {
                drawItemIcon(context, m.actual, rx, y + 37, 12);
                label(context,rx+14,y+39,rw-14,translated("schematic.actual", m.actualName),palette::dim);
            }
            // Every differing state: "<state>  <now> -> <should be>", full width below.
            float sy = bottom;
            if (!m.states.empty() && fits(sy)) { fill(context,dx,sy-3,dw,1,palette::white,.1f); label(context,dx,sy,dw,translated("schematic.check.states"),palette::faint); sy += 11; }
            Rgb stateColor{1.f, .8f, .25f};
            auto const& identifier = m.identifier;
            for (auto const& d : m.states) {
                if (!fits(sy)) break;
                auto translate = [](std::string_view key) { return translated(key); };
                auto now = information::stateName(d.key, d.actual, identifier, translate);
                auto want = information::stateName(d.key, d.expected, identifier, translate);
                auto const& named = now.labelIsKey ? now : want;
                std::string name = named.labelIsKey ? translated(named.label) : named.label;
                float nameW = dw * .45f;
                label(context,dx,sy,nameW-4,name,palette::dim);
                float vx = dx + nameW, nowW = textWidth(context, now.value);
                label(context,vx,sy,nowW+2,now.value,stateColor);
                vx += nowW + 3;
                changeArrow(context,vx,sy+1.5f+shapeTextDrop(),1,stateColor);
                vx += changeArrowWidth + 3;
                label(context,vx,sy,dx+dw-vx,want.value,stateColor);
                sy += 11;
            }
        }
        else if (auto structure = schematic::session::structure(checked->file);
                 structure && previewTurn.picked && previewTurn.pickedFile == checked->file) {
            // A clicked block without a row (correct, filtered out, or past
            // the list's limit): checked again right now.
            auto const& c = *previewTurn.picked;
            auto world = schematic::toWorld(structure->size, checked->placement, {c.x, c.y, c.z});
            auto* region = context.mClient.getRegion();
            auto now = region ? schematic::ghosts::mismatchAt(*region, world) : std::nullopt;
            fill(context,rx-5,y,1,52,palette::white,.1f);
            bool correct = !now || now->state == schematic::CellState::Correct;
            label(context,rx,y,rw,translated("schematic.check.here", std::format("{}, {}, {}", world.x, world.y, world.z)),
                correct ? palette::text : mismatchColor(now->state));
            if (fits(y + 11)) label(context,rx,y+11,rw,correct ? translated("schematic.pick.correct") : mismatchKind(now->state),
                correct ? palette::accent : mismatchColor(now->state));
            if (now && !now->expectedName.empty() && fits(y + 24)) {
                drawItemIcon(context, now->expected, rx, y + 23, 12);
                label(context,rx+14,y+25,rw-14,translated("schematic.expected", now->expectedName),palette::dim);
            }
            if (now && !correct && !now->actualName.empty() && fits(y + 38)) {
                drawItemIcon(context, now->actual, rx, y + 37, 12);
                label(context,rx+14,y+39,rw-14,translated("schematic.actual", now->actualName),palette::dim);
            }
        }
        bool canShow = verifySelected >= 0 || (previewTurn.picked && previewTurn.pickedFile == checked->file);
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
        drawSmallButton(context,l.actionX(0),l.actionsY+2,l.firstActionWidth,12,translated("schematic.showInWorld"),
            over(ShapeZone::Action,0),canShow ? palette::accentDeep : palette::keyFill,
            canShow ? palette::accent : palette::keyEdge, canShow ? palette::text : palette::faint);
        break;
    }
    case SchematicTab::Materials: {
        if (!checked) { paragraph(context,dx,l.detailTop+6,dw,translated("schematic.noSelection"),4,palette::faint); break; }
        label(context,dx,l.nameY+1+boxTextInset(),dw,checked->name);
        if (unloadable) paragraph(context,dx,l.previewY,dw,translated("schematic.notLoaded", loadProblem),3,palette::warning);
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
        stepperRow(0, "schematic.shownLayersOnly", {}, true, materialsShownOnly);
        stepperRow(1, "schematic.materials.hud", {}, true, Runtime::instance().preferences().schematic.hud);
        float y = l.fieldY(1) + ShapesLayout::rowHeight + 6;
        if (materialSelected >= 0 && materialSelected < static_cast<int>(materialRows.size())) {
            auto const& line = *materialRows[static_cast<size_t>(materialSelected)];
            drawItemIcon(context, line.icon, dx, y - 2, 12);
            label(context,dx+14,y,dw-14,line.name);
            label(context,dx,y+11,dw,translated("schematic.materials.leftAmount",
                amountText(line.remaining(), stackSizeOf(line))),palette::dim);
            y += 26;
        }
        // The rest of the pane is kept for raw materials from the game's recipes (L-116).
        bool missing = !missingMaterials().empty();
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
        drawSmallButton(context,l.actionX(0),l.actionsY+2,l.firstActionWidth,12,translated("schematic.openCalculator"),
            over(ShapeZone::Action,0),missing ? palette::accentDeep : palette::keyFill,
            missing ? palette::accent : palette::keyEdge, missing ? palette::text : palette::faint);
        break;
    }
    }

    if (materialTip) {
        // Text is batched: draw what is under the tip first, or it lands on top.
        context.flushText(0, std::nullopt);
        float w = textWidth(context, materialTip->text) + 8, x = std::min(materialTip->x + 8, l.left + l.width - w - 2);
        fill(context,x,materialTip->y+10,w,13,palette::panel);
        frame(context,x,materialTip->y+10,w,13,palette::keyEdge);
        label(context,x+4,materialTip->y+12,w-6,materialTip->text,palette::text);
    }
    // Footer.
    fill(context,l.left,l.footerTop,l.width,1,palette::white,.14f);
    float textLeft = l.left + ShapesLayout::pad, available = l.width - 2 * ShapesLayout::pad;
    bool shortFooter = l.docked || screen::table().shortFooter;
    std::string text = !screen::message().empty() ? screen::message() : schematicDescription();
    if (shortFooter) label(context,textLeft,l.footerTop+3,available,std::move(text),screen::message().empty() ? palette::text : palette::warning);
    else {
        paragraph(context,textLeft,l.footerTop+3,available,text,2,screen::message().empty() ? palette::text : palette::warning);
        bool menuUnbound = input::effectiveChord(Runtime::instance().preferences().bindings, input::Action::SchematicMenu).empty();
        label(context,textLeft,l.footerTop+30,available,translated(editingSchematicField >= 0 ? "shape.numberHint"
            : menuUnbound ? "schematic.menuKeyHint" : "schematic.screenHint"),palette::faint);
    }
}
ShapesLayout fitSchematics(SettingsTable const& t, glm::vec2 size, bool docked) {
    auto l = ShapesLayout::fit(t, size.x, size.y, docked, schematicRowCount(), schematicListFirst, schematicFieldCount(),
        schematicFieldFirst, false, false);
    // The first action never runs into the one at the right edge.
    l.firstActionWidth = std::clamp(l.detailWidth - 2 * ShapesLayout::pad - ShapesLayout::deleteWidth - 4, 40.f, 96.f);
    // Materials has one action: the calculator link, readable across the pane.
    if (schematicTab == SchematicTab::Materials) l.firstActionWidth = std::max(40.f, l.detailWidth - 2 * ShapesLayout::pad);
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
    if (l.width > 0) drawSchematicsBody(context, l, pointer);
    context.flushText(0,std::nullopt);
}
void renderSchematicsDocked(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) {
    auto l = fitSchematics(screen::table(), size, true);
    if (l.width <= 0) {
        label(context, 4, 4, std::max(1.0f, size.x - 8), translated("smallWindow"));
        context.flushText(0, std::nullopt);
        return;
    }
    panel(context,l.left,l.top,l.width,l.height,.82f);
    frame(context,l.left,l.top,l.width,l.height,palette::white,.14f);
    label(context,l.left+ShapesLayout::pad,l.top+6,l.drawAllX-l.left-10,translated("nav.schematics"));
    bool closeHover = l.hit(pointer.x, pointer.y).zone == ShapeZone::Close;
    drawSmallButton(context,l.closeX,l.top+4,ShapesLayout::closeWidth,12,translated("closeButton"),
        closeHover,palette::keyFill,palette::keyEdge,closeHover ? palette::text : palette::dim);
    fill(context,l.left,l.top+ShapesLayout::headerHeight-1,l.width,1,palette::white,.14f);
    drawSchematicsBody(context, l, pointer);
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
    if (auto& t = previewTurn; t.w > 0 && px >= t.x && px < t.x + t.w && py >= t.y && py < t.y + t.h) {
        // Shift peels layers off the side the view looks down on.
        if (shift) {
            // The side is chosen from the view when peeling starts
            // and then kept, so the cut can be looked at from the side.
            if (t.peel == 0) {
                schematic::preview::View now{t.yaw, t.pitch};
                auto cut = schematic::preview::cutFor(now, 2, 2, 2, 1);
                t.peelAxis = cut.axis;
                t.peelSign = cut.sign;
            }
            auto last = schematic::preview::last();
            int layers = std::max(1, last.layers ? last.layers : 256);
            t.peel = std::clamp(t.peel + (step < 0 ? 1 : -1), 0, layers - 1);
        } else t.zoom = std::clamp(t.zoom * (step < 0 ? 1.2f : 1 / 1.2f), .5f, schematic::preview::last().maxZoom);
        return;
    }
    bool overList = px >= l.listLeft && px < l.listLeft + l.listWidth && (!l.docked || py < l.detailTop);
    if (overList) schematicListFirst = std::max(0, schematicListFirst + step);
    else schematicFieldFirst = std::max(0, schematicFieldFirst + step);
}
void release() {
    // A press that did not move is a click on a block (inspect).
    if (std::exchange(previewTurn.dragging, false) && !previewTurn.turning) {
        previewTurn.yaw = previewTurn.fromYaw;
        previewTurn.pitch = previewTurn.fromPitch;
        pickInPreview(previewTurn.from);
    }
}
void drag(glm::vec2 pointer) {
    // A drag turns only after the pointer moved a little, so a click on
    // a block does not nudge the view.
    if (auto& t = previewTurn; t.dragging && (t.turning || glm::length(pointer - t.from) >= 6)) {
        t.turning = true;
        if (!t.manual) {
            // Pick up the turn the preview had while turning by itself.
            t.fromYaw = t.yaw + std::fmod(static_cast<float>(std::chrono::duration<double>(
                std::chrono::steady_clock::now().time_since_epoch()).count()) * 12.f, 360.f);
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
    changeSchematic([&](schematic::SavedPlacement& p) {
        if (field == 9) p.layers.index = value - 1;
        else (field == 0 ? p.placement.origin.x : field == 1 ? p.placement.origin.y : p.placement.origin.z) = value;
    });
}
void endEditing() { editingSchematicField = -1; }
glm::vec2 caret() { return {schematicsDisplayed.stepperX(), schematicsDisplayed.fieldY(std::max(0, editingSchematicField))}; }
bool docked() { return schematicsDocked; }
void renderDocked(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) { renderSchematicsDocked(context, size, pointer); }
void renderContent(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer, SettingsTable const& table) {
    renderSchematicsContent(context, size, pointer, table);
}
void reset() {
    editingSchematicField = -1;
    schematicDeleteArmed = false;
}
}

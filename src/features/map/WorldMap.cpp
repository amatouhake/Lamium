#include "features/map/WorldMap.h"
#include "features/map/MapImage.h"
#include "features/map/MapRadar.h"
#include "features/map/MapStore.h"
#include "features/map/RadarFaces.h"
#include "features/map/Minimap.h"
#include "features/map/WaypointSession.h"
#include "app/SessionIds.h"
#include "features/map/MapLayers.h"
#include "features/map/WorldMapView.h"
#include "features/map/SeedLink.h"
#include "features/map/Teleport.h"
#include "features/camera/CameraSessions.h"
#include "app/Desktop.h"
#include "ll/api/Versions.h"
#include "mc/world/level/LevelSeed64.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"
#include "features/information/InfoHud.h"
#include "features/information/SlimeChunk.h"
#include <tuple>
#include "app/Runtime.h"
#include "ui/Localization.h"
#include "ui/NumberInput.h"
#include "ui/SearchQuery.h"
#include "ui/ListViewWidgets.h"
#include "ui/Widgets.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/TextureGroup.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/container/Blob.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/image/Image.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core_graphics/ImageBuffer.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/biome/Biome.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/network/packet/CommandRequestPacket.h"
#include "mc/network/packet/CommandRequestPacketPayload.h"
#include "mc/server/commands/AutomationPlayerCommandOrigin.h"
#include "mc/server/commands/CommandContext.h"
#include "mc/server/commands/CommandOriginData.h"
#include "mc/server/commands/CommandOriginType.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/server/commands/CurrentCmdVersion.h"
#include <chrono>
#include <cmath>
#include <format>
#include <limits>

namespace lamium::map {
std::vector<Dot> collectDots(IClientInstance& client, double centerX, double centerZ, double reach, double playerY,
                             bool invisible, bool withFaces);
}
namespace lamium::map::world {
namespace {
using ui::Rgb;
constexpr float rowHeight = 15, bottomHeight = 13, buttonHeight = 11;
constexpr Rgb ground{11 / 255.f, 12 / 255.f, 13 / 255.f};

double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}
void log(std::string const& text) {
    static int lines = 0;
    if (++lines > 20) return;
    try { Runtime::instance().self().getLogger().info("World map screen: {}", text); } catch (...) {}
}

// Runtime textures: a fixed pool of locations, never destroyed (their
// destructor is game code, which must not run while the process tears down).
constexpr int slotCount = 96;
// Uploads per frame; the first frames after opening may upload more.
constexpr int uploadsPerFrame = 6, uploadsWhileOpening = 16;
// Slime chunk images (L-232) after the tiles and the two arrows.
constexpr int slimeSlotCount = 48, slimeSlotBase = slotCount + 2;
ResourceLocation const& slotLocation(int index) {
    static std::array<ResourceLocation*, slimeSlotBase + slimeSlotCount> all{};
    auto& location = all[static_cast<size_t>(index)];
    if (!location) {
        auto name = index == slotCount ? std::string("lamium/worldmap/arrow")
            : index == slotCount + 1 ? std::string("lamium/worldmap/camera_arrow")
            : index >= slimeSlotBase ? std::format("lamium/worldmap/slime/{}", index - slimeSlotBase)
                                     : std::format("lamium/worldmap/{}", index);
        location = new ResourceLocation(Core::PathView(name), ResourceFileSystem::Raw);
    }
    return *location;
}
struct Slot {
    std::optional<std::pair<MapLayer, TileKey>> tile;
    int focus = std::numeric_limits<int>::min(); // Cave layer image variant.
    unsigned version = 0;
    bool uploaded = false;
    double used = 0;
    // A slot given another tile still shows the old tile's image for a few
    // frames (the in-place update lands later): drawn from this frame on.
    unsigned readyAt = 0;
};
unsigned tileFrame = 0;
std::array<Slot, slotCount> slots;
bool upload(IClientInstance& client, ResourceLocation const& location, std::vector<std::uint32_t> const& pixels, int side,
            bool replace) {
    auto group = client.getTextureGroup();
    if (!group) return false;
    mce::Image image(side, side, mce::ImageFormat::RGBA8Unorm, mce::ImageUsage::SRGB);
    image.mAlphaUsage = mce::AlphaUsage::Transparent;
    image.setRawImage(mce::Blob(reinterpret_cast<std::uint8_t const*>(pixels.data()), pixels.size() * sizeof(std::uint32_t)));
    cg::ImageBuffer buffer(std::move(image));
    if (replace) return group->updateTextureInPlace(location, std::move(buffer));
    group->uploadTexture(location, std::move(buffer));
    return true;
}
void unload(IClientInstance& client, ResourceLocation const& location) {
    try {
        if (auto group = client.getTextureGroup()) group->unloadTexture(location, false);
    } catch (...) {}
}

enum class Target {
    None, Overworld, Nether, End,
    CaveButton, CavePanel, CaveToggle, BroadToggle, HeightDown, HeightUp, HeightField, MatchHeight,
    Center, Waypoints, Close, Menu,
    LayerWaypoints, LayerSchematics, LayerShapes, // The layer switches (L-146).
    LayerSlime, // Slime chunks, Overworld only (L-232).
    // The side panel: its background, header buttons, list and editor.
    Panel, PanelAdd, PanelClose, Row, RowVisible, Name, Step, Coordinate, MoveHere, Temporary, Swatch, Keep, Delete, OpenScreen,
    PanelKind, PanelKindItem // The list's layer menu (L-204) and its items.
};
struct Hit {
    float x, y, w, h;
    Target target;
    int item = -1;
    MarkKey mark{}; // List rows and their switches: the entry they stand for.
};
enum class MenuKind { Ground, Waypoint, Death, Placement, Shape };
struct Menu {
    MenuKind kind;
    float x, y;    // Screen, where it was opened.
    MarkKey mark{}; // Any but Ground: the mark it was opened on.
    int worldX = 0, worldY = 0, worldZ = 0;
    bool known = false; // Ground: the height came from the saved map.
    bool armed = false; // Delete pressed once.
};
MenuKind menuKind(MarkKey mark) {
    switch (mark.layer) {
    case MarkLayer::Death: return MenuKind::Death;
    case MarkLayer::Placement: return MenuKind::Placement;
    case MarkLayer::Shape: return MenuKind::Shape;
    default: return MenuKind::Waypoint;
    }
}
// The waypoint a key names, or null when it is gone (or not a waypoint).
Waypoint const* waypointOf(WaypointSet const& set, MarkKey mark) {
    if (mark.layer != MarkLayer::Waypoint) return nullptr;
    int index = indexOfId(set.waypoints, mark.id);
    return index < 0 ? nullptr : &set.waypoints[static_cast<size_t>(index)];
}
// The footprint a key names in this dimension, or null when it is gone.
AreaMark const* areaOf(MarkKey mark);
struct State {
    bool open = false;
    IClientInstance* client = nullptr;
    WorldView view;
    int dimension = 0;
    // The cave/surface state button and its panel (L-228).
    bool cave = false, broad = false, following = true, cavePanel = false;
    bool cameraReference = false; // FreeCamera is the height/center source.
    int height = 64;
    bool editingHeight = false;
    ui::NumberInput heightInput;
    struct Rect { float x = 0, y = 0, w = 0, h = 0; } caveButton;
    struct Drag {
        float x, y;
        double centerX, centerZ;
        bool moved = false;
        std::optional<MarkKey> marker; // Pressed on a mark: a click selects it.
    };
    std::optional<Drag> drag;
    std::optional<Menu> menu;
    std::vector<Hit> hits;
    std::vector<ScreenMark> markers; // every mark as last drawn, for the cursor
    std::vector<AreaMark> areas;     // placements and shapes drawn in this dimension (L-139)
    std::vector<AreaMark> allAreas;  // the same before the layer switches, for the side panel (L-204)
    bool kindMenu = false;           // The side panel's layer menu is open.
    float kindX = 0, kindY = 0, kindW = 0;
    std::array<int, 3> kindCounts{};
    glm::vec2 pointer{};
    std::string notice;
    double noticeAt = 0, openedAt = 0;
    float top = rowHeight; // The top bar, one or two rows.
    double pixelsPerUnit = 1; // Screen pixels per GUI unit, as last drawn.
    float panelWidth = 0;  // The side panel as last drawn; 0 when closed.
    std::optional<MarkKey> selected; // Side panel: the death point, a waypoint, a placement or a shape.
    int listFirst = 0;
    bool deleteArmed = false;
    bool editingName = false;
    ui::SearchQuery name;
    // A coordinate typed into its field (0-2 for X, Y, Z; -1 when none).
    int editingAxis = -1;
    ui::NumberInput coordinate;
    glm::vec2 namePosition{};
    float listTop = 0, listBottom = 0;
    float arrowAngle = NAN;
    bool arrowUploaded = false;
    float cameraArrowAngle = NAN;
    bool cameraArrowUploaded = false;
} state;

struct PlayerSpot {
    double x, y, z;
    float yaw;
    int dimension;
};
std::optional<PlayerSpot> playerSpot() {
    auto* player = state.client ? state.client->getLocalPlayer() : nullptr;
    if (!player) return std::nullopt;
    auto feet = player->getFeetPos();
    if (!std::isfinite(feet.x) || !std::isfinite(feet.y) || !std::isfinite(feet.z)) return std::nullopt;
    auto rotation = player->getRotation();
    return PlayerSpot{feet.x, feet.y, feet.z, std::isfinite(rotation.z) ? rotation.z : 0.f,
                      static_cast<int>(player->getDimensionId())};
}
// FreeCamera's eye while it owns the view; the cave map should open and
// match at what the camera sees, not at the body left behind (L-228).
std::optional<PlayerSpot> freeCameraSpot() {
    auto body = playerSpot();
    if (!state.client) return std::nullopt;
    auto pose = CameraSessions::instance().freeCameraPose(*state.client);
    if (!pose) return std::nullopt;
    return PlayerSpot{pose->x, pose->y, pose->z, pose->yaw, body ? body->dimension : 0};
}
std::optional<PlayerSpot> sourceSpot() {
    if (state.cameraReference)
        if (auto camera = freeCameraSpot()) return camera;
    return playerSpot();
}
bool cameraAvailable() { return freeCameraSpot().has_value(); }
// The player's place seen from another dimension: the Nether is 1/8 scale.
std::optional<std::pair<double, double>> playerOn(int dimension) {
    auto spot = playerSpot();
    if (!spot) return std::nullopt;
    double k = spot->dimension == dimension ? 1 : spot->dimension == 0 && dimension == 1 ? 1 / 8. :
        spot->dimension == 1 && dimension == 0 ? 8. : 1;
    return std::pair{spot->x * k, spot->z * k};
}
void center() {
    if (auto spot = sourceSpot(); spot && spot->dimension == state.dimension) {
        state.view.centerX = spot->x;
        state.view.centerZ = spot->z;
        return;
    }
    if (auto at = playerOn(state.dimension)) {
        state.view.centerX = at->first;
        state.view.centerZ = at->second;
    }
}
int heightMin(int dimension) { return dimension == 1 ? 0 : -64; }
int heightMax(int dimension) { return dimension == 1 ? 127 : 319; }
int clampHeight(int dimension, int y) { return std::clamp(y, heightMin(dimension), heightMax(dimension)); }
bool playerCave(PlayerSpot const& spot) {
    if (spot.dimension == 1) return true;
    if (spot.dimension != 0) return false;
    if (state.cameraReference) return map::automaticView() == ViewMode::Cave;
    return state.client && map::bodyCave(*state.client);
}
bool canMatchHeight() {
    auto spot = sourceSpot();
    return spot && state.cave && spot->dimension == state.dimension && playerCave(*spot);
}
int recordedCaveHeight(PlayerSpot const& spot, int dimension) {
    auto layer = map::caveLayerY();
    return clampHeight(dimension, map::caveReferenceHeight(layer.value_or(0), blockFloor(spot.y), layer.has_value()));
}
std::optional<int> referenceHeight() {
    auto spot = sourceSpot();
    if (!state.cave || !spot || spot->dimension != state.dimension) return std::nullopt;
    return recordedCaveHeight(*spot, state.dimension);
}
// A manual edit that lands exactly on the reference height is a return to the
// player/camera, not a fixed height that still offers a match action.
void syncFollowing() {
    auto reference = referenceHeight();
    state.following = reference && *reference == state.height && canMatchHeight();
}
MapLayer shownLayer() {
    if (state.dimension == 1) return caveLayer(1, state.height);
    if (state.dimension == 0 && state.cave) return caveLayer(0, state.height);
    return surfaceLayer(state.dimension);
}
std::optional<int> focusHeight() {
    return state.cave && !state.broad ? std::optional<int>(state.height) : std::nullopt;
}
std::string stateText() {
    if (!state.cave) return ui::translated("worldMap.surface");
    auto detail = ui::translated(state.broad ? "worldMap.broadShort" : "worldMap.layerShort");
    return ui::translated("worldMap.caveState", ui::translated("worldMap.cave"), detail, state.height);
}
void toggleCave() {
    if (state.dimension == 2) return;
    state.cave = !state.cave;
    state.cavePanel = true;
    if (!state.cave) {
        state.broad = false;
        state.following = false;
        return;
    }
    auto spot = sourceSpot();
    if (spot && spot->dimension == state.dimension) state.height = recordedCaveHeight(*spot, state.dimension);
    state.broad = state.dimension == 1;
    syncFollowing();
}
void adjustHeight(int delta) {
    if (!state.cave) return;
    state.height = clampHeight(state.dimension, state.height + delta);
    syncFollowing();
}
void matchHeight() {
    if (!canMatchHeight()) return;
    auto spot = sourceSpot();
    if (!spot) return;
    state.height = recordedCaveHeight(*spot, state.dimension);
    state.following = true;
}
void returnToPlayer() {
    auto spot = playerSpot();
    if (!spot) return;
    state.cameraReference = false;
    state.dimension = spot->dimension;
    state.cave = playerCave(*spot);
    state.broad = state.dimension == 1;
    state.height = state.cave ? recordedCaveHeight(*spot, state.dimension)
                              : clampHeight(state.dimension, blockFloor(spot->y));
    state.following = true;
    state.cavePanel = false;
    center();
}
void showDimension(int dimension) {
    if (state.dimension == dimension) return;
    state.dimension = dimension;
    state.menu.reset();
    state.cavePanel = false;
    auto body = playerSpot();
    if (!body || body->dimension != dimension) state.cameraReference = false;
    auto spot = sourceSpot();
    if (spot && spot->dimension == dimension) {
        state.cave = playerCave(*spot);
        state.height = state.cave ? recordedCaveHeight(*spot, dimension)
                                  : clampHeight(dimension, blockFloor(spot->y));
        state.following = state.cave;
    } else {
        state.cave = dimension == 1;
        state.height = clampHeight(dimension, state.height);
        state.following = false;
    }
    state.broad = dimension == 1;
    center();
}
void say(std::string text) {
    state.notice = std::move(text);
    state.noticeAt = now();
}

void act(Target target) {
    switch (target) {
    case Target::Overworld: showDimension(0); break;
    case Target::Nether: showDimension(1); break;
    case Target::End: showDimension(2); break;
    case Target::CaveButton:
        state.cavePanel = !state.cavePanel;
        state.kindMenu = false;
        break;
    case Target::CaveToggle: toggleCave(); break;
    case Target::BroadToggle:
        if (state.cave) state.broad = !state.broad;
        break;
    case Target::HeightDown: adjustHeight(-1); break;
    case Target::HeightUp: adjustHeight(1); break;
    case Target::HeightField:
        state.editingHeight = true;
        state.editingName = false;
        state.editingAxis = -1;
        state.heightInput.begin(static_cast<float>(state.height));
        break;
    case Target::MatchHeight: matchHeight(); break;
    case Target::Center: returnToPlayer(); break;
    default: break;
    }
}
LayerSwitches layers() {
    auto const& map = Runtime::instance().preferences().map;
    return {map.worldMapWaypoints, map.worldMapSchematics, map.worldMapShapes};
}
void toggleLayer(Target target) {
    auto value = Runtime::instance().preferences();
    bool& on = target == Target::LayerWaypoints ? value.map.worldMapWaypoints
        : target == Target::LayerSchematics ? value.map.worldMapSchematics
        : target == Target::LayerSlime ? value.map.worldMapSlime : value.map.worldMapShapes;
    on = !on;
    if (!Runtime::instance().save(value)) log("could not save the layer switches");
}
bool panelOpen() { return Runtime::instance().preferences().map.worldMapPanel; }
int shownList() { return clampListKind(Runtime::instance().preferences().map.worldMapList); }
void setShownList(int kind) {
    auto value = Runtime::instance().preferences();
    if (value.map.worldMapList == kind) return;
    value.map.worldMapList = kind;
    state.listFirst = 0;
    if (!Runtime::instance().save(value)) log("could not save the side panel list");
}
void setPanelOpen(bool open) {
    auto value = Runtime::instance().preferences();
    if (value.map.worldMapPanel == open) return;
    value.map.worldMapPanel = open;
    if (!Runtime::instance().save(value)) log("could not save the side panel state");
}
bool changeSelected(std::function<void(Waypoint&)> const& apply);
void commitHeight() {
    if (!std::exchange(state.editingHeight, false)) return;
    auto value = state.heightInput.parsedPrecise(static_cast<float>(heightMin(state.dimension)),
                                                 static_cast<float>(heightMax(state.dimension)), true);
    if (!value) return; // Incomplete or out of range: the height stays.
    state.height = static_cast<int>(*value);
    syncFollowing();
}
void commitCoordinate() {
    int axis = std::exchange(state.editingAxis, -1);
    if (axis < 0) return;
    auto value = state.coordinate.parsedPrecise(-coordinateLimit, coordinateLimit, true);
    if (!value) return; // Incomplete or out of range: the waypoint keeps its place.
    changeSelected([&](Waypoint& w) { (axis == 0 ? w.x : axis == 1 ? w.y : w.z) = static_cast<int>(*value); });
}
// Ends whatever is being typed in the side panel, keeping it.
void commitName() {
    commitHeight();
    commitCoordinate();
    if (!std::exchange(state.editingName, false)) return;
    auto text = state.name.value();
    auto mark = state.selected;
    if (text.find_first_not_of(' ') == std::string::npos || !mark || mark->layer != MarkLayer::Waypoint) return;
    if (!waypoints::change([&](WaypointSet& set) {
            int index = indexOfId(set.waypoints, mark->id);
            if (index < 0) return false;
            set.waypoints[static_cast<size_t>(index)].name = text;
            return true;
        })) say(ui::translated("waypoint.saveError"));
}
// Brings a place to the middle of the map left of the side panel.
void focus(double x, double z) {
    state.view.centerX = x + state.panelWidth / 2 / state.view.scale();
    state.view.centerZ = z;
}
void select(std::optional<MarkKey> mark, bool move) {
    commitName();
    state.selected = mark;
    state.deleteArmed = false;
    if (mark) setShownList(listKind(mark->layer));
    if (!move || !mark) return;
    auto set = waypoints::current();
    if (mark->layer == MarkLayer::Death && set.death) focus(set.death->x + .5, set.death->z + .5);
    else if (auto const* w = waypointOf(set, *mark)) {
        if (auto at = shownPosition(w->x, w->y, w->z, w->dimension, state.dimension, true)) focus(at->x, at->z);
    } else if (auto const* area = areaOf(*mark))
        focus((area->area.x0 + area->area.x1) / 2.0, (area->area.z0 + area->area.z1) / 2.0);
}
AreaMark const* areaOf(MarkKey mark) {
    for (auto const& area : state.allAreas) if (area.key == mark) return &area;
    return nullptr;
}
// A change to one waypoint, found by its id when the change runs: an
// entry added or removed meanwhile cannot redirect it.
bool changeWaypoint(MarkKey mark, std::function<void(Waypoint&)> const& apply) {
    bool saved = waypoints::change([&](WaypointSet& set) {
        int index = mark.layer == MarkLayer::Waypoint ? indexOfId(set.waypoints, mark.id) : -1;
        if (index < 0) return false;
        apply(set.waypoints[static_cast<size_t>(index)]);
        return true;
    });
    if (!saved) say(ui::translated("waypoint.saveError"));
    return saved;
}
bool changeSelected(std::function<void(Waypoint&)> const& apply) {
    return state.selected && changeWaypoint(*state.selected, apply);
}
// Keeps the death point as an ordinary waypoint; its new key, if saved.
std::optional<MarkKey> keepDeath(WaypointSet const& set) {
    if (!set.death) return std::nullopt;
    auto death = *set.death;
    Waypoint w{ui::translated("waypoint.deathName"), nextColor(set.lastColor), death.x, death.y, death.z, death.dimension, true};
    bool saved = waypoints::change([&](WaypointSet& s) {
        s.waypoints.push_back(w);
        s.lastColor = w.color;
        s.death.reset();
        return true;
    });
    if (!saved) { say(ui::translated("waypoint.saveError")); return std::nullopt; }
    auto now = waypoints::current();
    return now.waypoints.empty() ? std::nullopt : std::optional(waypointKey(now.waypoints.back().id));
}
// Deletes the death point or a waypoint by its key.
bool deleteMark(MarkKey mark) {
    return waypoints::change([&](WaypointSet& s) {
        if (mark.layer == MarkLayer::Death) { s.death.reset(); return true; }
        int index = indexOfId(s.waypoints, mark.id);
        if (mark.layer != MarkLayer::Waypoint || index < 0) return false;
        s.waypoints.erase(s.waypoints.begin() + index);
        return true;
    });
}
Request panelAction(Hit const& hit) {
    Request request;
    // A click on the field being typed in keeps typing (L-172 moves the caret).
    bool sameField = (hit.target == Target::Name && state.editingName) || (hit.target == Target::Coordinate && hit.item == state.editingAxis);
    if (!sameField) commitName();
    if (hit.target != Target::Delete) state.deleteArmed = false;
    auto set = waypoints::current();
    switch (hit.target) {
    case Target::PanelClose: setPanelOpen(false); break;
    case Target::PanelAdd: {
        auto spot = playerSpot();
        if (!spot) break;
        Waypoint w;
        w.x = blockFloor(spot->x); w.y = blockFloor(spot->y); w.z = blockFloor(spot->z);
        w.dimension = spot->dimension;
        w.color = nextColor(set.lastColor);
        w.name = defaultWaypointName(set.waypoints, [](int n) { return ui::translated("waypoint.defaultName", n); });
        if (auto id = waypoints::add(w)) {
            showDimension(w.dimension);
            select(waypointKey(id), false);
        } else say(ui::translated("waypoint.saveError"));
        break;
    }
    case Target::Row:
        if (hit.mark.layer == MarkLayer::Placement) selectPlacement(hit.mark);
        select(hit.mark, true);
        break;
    case Target::RowVisible:
        if (pointLayer(hit.mark.layer)) changeWaypoint(hit.mark, [](Waypoint& w) { w.visible = !w.visible; });
        else toggleVisible(hit.mark);
        break;
    case Target::Name:
        if (auto const* w = state.selected ? waypointOf(set, *state.selected) : nullptr; w && !state.editingName) {
            state.editingName = true;
            state.name.clear();
            state.name.append(w->name);
            state.name.selectAll();
        }
        break;
    case Target::Coordinate:
        if (auto const* w = state.selected ? waypointOf(set, *state.selected) : nullptr; w && state.editingAxis != hit.item) {
            state.editingAxis = hit.item;
            state.coordinate.begin(static_cast<float>(hit.item == 0 ? w->x : hit.item == 1 ? w->y : w->z));
        }
        break;
    case Target::Step: {
        int axis = hit.item / 2, step = hit.item % 2 ? 1 : -1;
        changeSelected([&](Waypoint& w) {
            int& value = axis == 0 ? w.x : axis == 1 ? w.y : w.z;
            value = std::clamp(value + step, -coordinateLimit, coordinateLimit);
        });
        break;
    }
    case Target::MoveHere:
        if (auto spot = playerSpot())
            changeSelected([&](Waypoint& w) {
                w.x = blockFloor(spot->x); w.y = blockFloor(spot->y); w.z = blockFloor(spot->z);
                w.dimension = spot->dimension;
            });
        break;
    case Target::Temporary: changeSelected([](Waypoint& w) { w.temporary = !w.temporary; }); break;
    case Target::Swatch: changeSelected([&](Waypoint& w) { w.color = clampColor(hit.item); }); break;
    case Target::Keep:
        if (auto kept = keepDeath(set)) select(kept, false);
        break;
    case Target::Delete: {
        auto const* chosen = state.selected ? waypointOf(set, *state.selected) : nullptr;
        if (!state.deleteArmed && !(chosen && chosen->temporary)) { state.deleteArmed = true; break; }
        state.deleteArmed = false;
        if (!state.selected) break;
        if (!deleteMark(*state.selected)) say(ui::translated("waypoint.saveError"));
        else state.selected.reset();
        break;
    }
    case Target::OpenScreen:
        request.kind = !state.selected || pointLayer(state.selected->layer) ? Request::Kind::OpenWaypoints
            : state.selected->layer == MarkLayer::Placement ? Request::Kind::OpenSchematic : Request::Kind::OpenShape;
        request.mark = state.selected;
        break;
    default: break;
    }
    return request;
}
// This world's seed as the client received it; none when it is zero, which
// servers that hide the seed send.
std::optional<std::uint64_t> worldSeed() {
    auto* player = state.client ? state.client->getLocalPlayer() : nullptr;
    if (!player) return std::nullopt;
    auto seed = static_cast<std::uint64_t>(player->getLevel().getLevelSeed64().mValue);
    if (!seed) return std::nullopt;
    return seed;
}
bool seedLinks() { return Runtime::instance().preferences().map.seedLink; }
void openSeedMap(int x, int z) {
    auto seed = worldSeed();
    if (!seed) { say(ui::translated("worldMap.noSeed")); return; }
    auto version = ll::getGameVersion();
    // The same scale as the map: screen pixels per block.
    double zoom = seedMapZoom(state.view.scale() * state.pixelsPerUnit);
    auto url = seedMapUrl(*seed, seedMapPlatform(version.major, version.minor, version.patch), state.dimension, x, z, zoom);
    if (!openUrl(url)) say(ui::translated("worldMap.linkFailed"));
}
void copySeed() {
    auto seed = worldSeed();
    if (!seed) { say(ui::translated("worldMap.noSeed")); return; }
    say(ui::translated(copyText(seedText(*seed)) ? "worldMap.seedCopied" : "worldMap.copyFailed", seedText(*seed)));
}
// Where the menu's teleport goes, when the world lets this player use /tp.
struct TeleportTarget { int x, y, z; };
std::optional<TeleportTarget> teleportTarget(Menu const& menu, WaypointSet const& set) {
    auto* player = state.client ? state.client->getLocalPlayer() : nullptr;
    if (!player || menu.kind == MenuKind::Placement || menu.kind == MenuKind::Shape) return std::nullopt;
    int dimension = state.dimension, x = menu.worldX, y = menu.worldY, z = menu.worldZ;
    if (menu.kind == MenuKind::Death) {
        if (!set.death) return std::nullopt;
        dimension = set.death->dimension; x = set.death->x; y = set.death->y; z = set.death->z;
    } else if (menu.kind == MenuKind::Waypoint) {
        auto const* w = waypointOf(set, menu.mark);
        if (!w) return std::nullopt;
        dimension = w->dimension; x = w->x; y = w->y; z = w->z;
    }
    auto& level = player->getLevel();
    bool cheats = level.getLevelData().mCheatsEnabled, commands = level.hasCommandsEnabled();
    int permission = static_cast<int>(player->getCommandPermissionLevel());
    auto listed = teleportListed();
    // What the world reports, once per world and list, to confirm in game.
    static std::pair<void const*, int> reported{};
    if (reported != std::pair<void const*, int>{&level, listed ? *listed : -1}) {
        reported = {&level, listed ? *listed : -1};
        log(std::format("teleport flags: cheats {} commands {} permission {} listed {}", cheats, commands, permission,
                        listed ? (*listed ? "yes" : "no") : "unknown"));
    }
    if (!canTeleport(listed, commands, permission, static_cast<int>(player->getDimensionId()), dimension))
        return std::nullopt;
    return TeleportTarget{x, y, z};
}
// The ordinary command request a typed /tp sends; the server checks it.
void teleport(TeleportTarget target) {
    auto* player = state.client ? state.client->getLocalPlayer() : nullptr;
    if (!player) return;
    try {
        auto command = teleportCommand(target.x, target.y, target.z);
        CommandContext context(command, std::make_unique<AutomationPlayerCommandOrigin>("", *player),
                               static_cast<int>(CurrentCmdVersion::Latest));
        CommandRequestPacketPayload payload(context, false);
        payload.mOrigin->mType = CommandOriginType::Player;
        CommandRequestPacket packet(payload);
        player->sendNetworkPacket(packet);
        say(ui::translated("worldMap.teleported", target.x, target.y, target.z));
    } catch (std::exception const& error) {
        log(std::format("teleport failed: {}", error.what()));
    }
}
std::vector<std::string> menuItems(Menu const& menu, WaypointSet const& set) {
    std::vector<std::string> items;
    if (menu.kind == MenuKind::Ground) {
        items = {ui::translated("worldMap.addHere")};
        if (seedLinks()) {
            items.push_back(ui::translated("worldMap.openSeedMap"));
            items.push_back(ui::translated("worldMap.copySeed"));
        }
    } else if (menu.kind == MenuKind::Death) {
        items = {ui::translated("waypoint.keep"), ui::translated(menu.armed ? "worldMap.deleteArmed" : "worldMap.delete")};
    } else if (menu.kind == MenuKind::Placement || menu.kind == MenuKind::Shape) {
        auto const* area = areaOf(menu.mark);
        auto visibility = ui::translated(area && area->visible ? "worldMap.hide" : "worldMap.show");
        if (menu.kind == MenuKind::Placement) items = {ui::translated("worldMap.select"), visibility, ui::translated("worldMap.openPlaced")};
        else items = {visibility, ui::translated("worldMap.openShapes")};
    } else {
        auto const* w = waypointOf(set, menu.mark);
        bool visible = w && w->visible;
        items = {ui::translated("worldMap.edit"), ui::translated(visible ? "worldMap.hide" : "worldMap.show"),
                 ui::translated(menu.armed ? "worldMap.deleteArmed" : "worldMap.delete")};
    }
    // Last, so the other items keep their positions.
    if (teleportTarget(menu, set)) items.push_back(ui::translated(menu.kind == MenuKind::Ground ? "worldMap.teleportHere" : "worldMap.teleport"));
    return items;
}
Request chooseMenu(int item) {
    auto menu = *state.menu;
    auto set = waypoints::current();
    Request request;
    if (auto target = teleportTarget(menu, set);
        target && item == static_cast<int>(menuItems(menu, set).size()) - 1) {
        state.menu.reset();
        teleport(*target);
        return request;
    }
    if (menu.kind == MenuKind::Ground && item > 0) {
        state.menu.reset();
        if (item == 1) openSeedMap(menu.worldX, menu.worldZ);
        else copySeed();
        return request;
    }
    if (menu.kind == MenuKind::Ground) {
        state.menu.reset();
        request.kind = Request::Kind::AddWaypoint;
        auto& d = request.draft;
        d.x = menu.worldX; d.y = menu.worldY; d.z = menu.worldZ;
        d.dimension = state.dimension;
        d.color = nextColor(set.lastColor);
        d.name = defaultWaypointName(set.waypoints, [](int n) { return ui::translated("waypoint.defaultName", n); });
        return request;
    }
    if (menu.kind == MenuKind::Placement || menu.kind == MenuKind::Shape) {
        state.menu.reset();
        if (!areaOf(menu.mark)) return request; // Gone meanwhile: nothing to act on.
        int action = menu.kind == MenuKind::Shape ? item + 1 : item; // 0 select, 1 show/hide, 2 open
        if (action == 0) {
            selectPlacement(menu.mark);
            setPanelOpen(true);
            select(menu.mark, false);
        } else if (action == 1) {
            toggleVisible(menu.mark);
        } else {
            request.kind = menu.kind == MenuKind::Placement ? Request::Kind::OpenSchematic : Request::Kind::OpenShape;
            request.mark = menu.mark;
        }
        return request;
    }
    if (menu.kind == MenuKind::Death) {
        if (item == 0 && set.death) {
            state.menu.reset();
            if (keepDeath(set)) say(ui::translated("waypoint.added", ui::translated("waypoint.deathName")));
        } else if (item == 1) {
            if (!menu.armed) { state.menu->armed = true; return request; }
            state.menu.reset();
            if (!waypoints::change([](WaypointSet& s) { s.death.reset(); return true; }))
                say(ui::translated("waypoint.saveError"));
        }
        return request;
    }
    auto const* w = waypointOf(set, menu.mark);
    if (!w) { state.menu.reset(); return request; }
    auto name = w->name;
    if (item == 0) {
        state.menu.reset();
        setPanelOpen(true);
        select(menu.mark, false);
    } else if (item == 1) {
        state.menu.reset();
        changeWaypoint(menu.mark, [](Waypoint& v) { v.visible = !v.visible; });
    } else if (item == 2) {
        if (!menu.armed && !w->temporary) { state.menu->armed = true; return request; }
        state.menu.reset();
        bool saved = deleteMark(menu.mark);
        say(ui::translated(saved ? "worldMap.deleted" : "waypoint.saveError", name));
    }
    return request;
}
Hit const* hitAt(float x, float y) {
    // Later entries are drawn on top.
    for (auto it = state.hits.rbegin(); it != state.hits.rend(); ++it)
        if (x >= it->x && y >= it->y && x < it->x + it->w && y < it->y + it->h) return &*it;
    return nullptr;
}
std::optional<MarkKey> markerAt(float x, float y) { return markAt(state.markers, x, y); }
void openMenu(float x, float y) {
    if (auto mark = markerAt(x, y)) {
        state.menu = Menu{menuKind(*mark), x, y, *mark};
        return;
    }
    int wx = blockFloor(state.view.worldX(x)), wz = blockFloor(state.view.worldZ(y));
    Menu menu{MenuKind::Ground, x, y};
    menu.worldX = wx;
    menu.worldZ = wz;
    if (auto column = store::column(shownLayer(), wx, wz)) {
        menu.worldY = column->height + 1;
        menu.known = true;
    } else if (auto spot = playerSpot()) menu.worldY = blockFloor(spot->y);
    state.menu = menu;
}

// ---- Drawing ----
Rgb rgb(std::uint32_t color) { return {channel(color, 0) / 255.f, channel(color, 1) / 255.f, channel(color, 2) / 255.f}; }
// Hollow for a temporary waypoint (L-261): a black middle.
void diamond(MinecraftUIRenderContext& context, float cx, float cy, int size, Rgb color, float opacity, bool hollow = false) {
    auto rows = diamondRows(size);
    float top = cy - static_cast<float>(rows.size()) / 2;
    for (size_t i = 0; i < rows.size(); ++i) {
        float half = static_cast<float>(rows[i]) + .5f;
        ui::fill(context, cx - half, top + i, 2 * half, 1, Rgb{0, 0, 0}, .85f * opacity);
    }
    auto inner = diamondRows(size - 2);
    top = cy - static_cast<float>(inner.size()) / 2;
    for (size_t i = 0; i < inner.size(); ++i) {
        float half = static_cast<float>(inner[i]) + .5f;
        ui::fill(context, cx - half, top + i, 2 * half, 1, color, opacity);
    }
    if (!hollow) return;
    auto hole = diamondRows(hollowSize(size - 2));
    top = cy - static_cast<float>(hole.size()) / 2;
    for (size_t i = 0; i < hole.size(); ++i) {
        float half = static_cast<float>(hole[i]) + .5f;
        ui::fill(context, cx - half, top + i, 2 * half, 1, Rgb{0, 0, 0}, .85f * opacity);
    }
}
void cross(MinecraftUIRenderContext& context, float cx, float cy) {
    for (int pass = 0; pass < 2; ++pass)
        for (int i = -2; i <= 2; ++i)
            for (int sign : {1, -1}) {
                float x = cx + i - .5f, y = cy + sign * i - .5f;
                if (pass == 0) ui::fill(context, x - 1, y - 1, 3, 3, Rgb{0, 0, 0}, .85f);
                else ui::fill(context, x, y, 1, 1, rgb(deathColor));
            }
}
void smallLabel(MinecraftUIRenderContext& context, float cx, float y, std::string const& text, Rgb color) {
    float scale = .75f, w = ui::textWidthScaled(context, text, scale);
    // A dark plate keeps small names readable on bright terrain (sand, snow).
    if (!text.empty()) ui::fill(context, std::round(cx - w / 2) - 2, y - 1, std::round(w) + 4, 10 * scale + 2, Rgb{0, 0, 0}, .5f);
    ui::labelScaled(context, cx - w / 2, y, w + 2, text, scale, color, ui::Align::Left, true);
}
float button(MinecraftUIRenderContext& context, float x, float y, std::string const& text, Target target, bool on,
             bool hover, int item = -1) {
    float w = ui::textWidth(context, text) + 8;
    ui::fill(context, x, y, w, buttonHeight, on ? ui::palette::accentDeep : hover ? Rgb{.23f, .23f, .24f} : ui::palette::keyFill, .9f);
    ui::frame(context, x, y, w, buttonHeight, on ? ui::palette::accent : ui::palette::keyEdge);
    ui::label(context, x, y + ui::boxTextInset(), w, text, on || hover ? ui::palette::text : ui::palette::dim, ui::Align::Center);
    state.hits.push_back({x, y, w, buttonHeight, target, item});
    return w;
}
bool hovering(float x, float y, float w, float h) {
    return state.pointer.x >= x && state.pointer.y >= y && state.pointer.x < x + w && state.pointer.y < y + h;
}

void drawTiles(MinecraftUIRenderContext& context, glm::vec2 size, double pixelsPerUnit, MapLayer layer, int& empty,
               int& shown) {
    auto& client = context.mClient;
    auto& view = state.view;
    int lod = lodFor(view.scale(), pixelsPerUnit);
    auto tiles = visibleTiles(view, lod);
    auto focus = focusHeight();
    int const focusKey = focus ? caveFocusKey(*focus) : std::numeric_limits<int>::min();
    store::want(layer, tiles, focus);
    double time = now();
    int budget = time - state.openedAt < 1 ? uploadsWhileOpening : uploadsPerFrame;
    ++tileFrame;
    auto ready = [&](int index) {
        auto const& slot = slots[static_cast<size_t>(index)];
        return slot.uploaded && tileFrame >= slot.readyAt;
    };
    std::vector<bool> usedNow(slotCount, false);
    auto findSlot = [&](TileKey tile) -> int {
        for (int i = 0; i < slotCount; ++i)
            if (slots[static_cast<size_t>(i)].tile && slots[static_cast<size_t>(i)].tile->first == layer
                && slots[static_cast<size_t>(i)].tile->second == tile
                && slots[static_cast<size_t>(i)].focus == focusKey) return i;
        return -1;
    };
    auto rectOf = [&](TileKey tile) {
        double blocks = tileBlocks(tile.lod);
        float x0 = static_cast<float>(snapToPixel(view.screenX(tile.x * blocks), pixelsPerUnit));
        float x1 = static_cast<float>(snapToPixel(view.screenX((tile.x + 1) * blocks), pixelsPerUnit));
        float y0 = static_cast<float>(snapToPixel(view.screenY(tile.z * blocks), pixelsPerUnit));
        float y1 = static_cast<float>(snapToPixel(view.screenY((tile.z + 1) * blocks), pixelsPerUnit));
        return ui::ImageRect{x0, y0, x1 - x0, y1 - y0};
    };
    auto drawSlot = [&](int index, ui::ImageRect rect, float u, float v, float span) {
        auto& slot = slots[static_cast<size_t>(index)];
        slot.used = time;
        usedNow[static_cast<size_t>(index)] = true;
        if (!ui::runtimeImage(context, slotLocation(index), rect, u, v, span, span)) slot.uploaded = false;
    };
    // Ready slots of this layer, and the tiles in view without their own.
    auto onGpu = [&](std::vector<int>& at) {
        std::vector<TileKey> keys;
        at.clear();
        for (int i = 0; i < slotCount; ++i)
            if (auto const& slot = slots[static_cast<size_t>(i)]; ready(i) && slot.tile && slot.tile->first == layer
                && slot.focus == focusKey) {
                keys.push_back(slot.tile->second);
                at.push_back(i);
            }
        return keys;
    };
    std::vector<TileKey> missing;
    for (auto tile : tiles)
        if (int index = findSlot(tile); index < 0 || !ready(index)) missing.push_back(tile);
    // Finer images standing in after zooming out are taken for new tiles last.
    std::vector<bool> standing(slotCount, false);
    std::vector<int> gpuSlots;
    auto gpuTiles = onGpu(gpuSlots);
    for (auto i : finerStandIns(lod, missing, gpuTiles)) standing[static_cast<size_t>(gpuSlots[i])] = true;
    missing.clear();
    for (auto tile : tiles) {
        auto image = store::image(layer, tile, focus);
        // An image with nothing in it is not uploaded, but a coarser image
        // may still stand in for it (L-215).
        bool hollow = image && !image->pixels;
        if (hollow) ++empty;
        int index = findSlot(tile);
        if ((!image || hollow) && index >= 0) {
            slots[static_cast<size_t>(index)] = Slot{};
            index = -1;
        }
        bool current = image && index >= 0 && slots[static_cast<size_t>(index)].uploaded
            && slots[static_cast<size_t>(index)].version == image->version;
        if (image && !hollow && !current && budget > 0) {
            if (index < 0) {
                std::pair<bool, double> oldest{true, INFINITY};
                for (int i = 0; i < slotCount; ++i) {
                    auto const& s = slots[static_cast<size_t>(i)];
                    // Shown this frame, or still waiting for its image to land.
                    if (usedNow[static_cast<size_t>(i)] || (s.uploaded && tileFrame < s.readyAt)) continue;
                    std::pair<bool, double> age{standing[static_cast<size_t>(i)], s.tile ? s.used : -INFINITY};
                    if (index < 0 || age < oldest) { oldest = age; index = i; }
                }
            }
            if (index >= 0) {
                auto& slot = slots[static_cast<size_t>(index)];
                --budget;
                try {
                    bool moved = !slot.tile || slot.tile->first != layer || slot.tile->second != tile
                        || slot.focus != focusKey;
                    if (upload(client, slotLocation(index), *image->pixels, regionBlocks, slot.uploaded)) {
                        if (moved) slot.readyAt = tileFrame + 3;
                        // Newest now, so it is not taken again before it shows.
                        slot.used = time;
                        usedNow[static_cast<size_t>(index)] = true;
                        slot.uploaded = true;
                        slot.tile = std::pair{layer, tile};
                        slot.focus = focusKey;
                        slot.version = image->version;
                    } else slot = Slot{};
                } catch (...) { slot = Slot{}; }
            }
        }
        if (index >= 0 && ready(index) && slots[static_cast<size_t>(index)].tile
            && slots[static_cast<size_t>(index)].tile->second == tile) {
            drawSlot(index, rectOf(tile), 0, 0, 1);
            ++shown;
            continue;
        }
        // Not ready: a coarser image already on the GPU stands in.
        for (int up = 1; tile.lod + up <= maxLod; ++up) {
            TileKey parent{tile.lod + up, tile.x >> up, tile.z >> up};
            int at = findSlot(parent);
            if (at < 0 || !ready(at)) continue;
            float span = 1.f / static_cast<float>(1 << up);
            drawSlot(at, rectOf(tile), (tile.x - (parent.x << up)) * span, (tile.z - (parent.z << up)) * span, span);
            break;
        }
        missing.push_back(tile);
    }
    // Zooming out: finer images on the GPU draw over the coarser stand-in.
    gpuTiles = onGpu(gpuSlots);
    for (auto i : finerStandIns(lod, missing, gpuTiles)) drawSlot(gpuSlots[i], rectOf(gpuTiles[i]), 0, 0, 1);
    (void)size;
}

// A mark's name, drawn after every mark when it is wanted (drawNames).
struct MarkName {
    MarkKey key;
    float cx, y;
    std::string text;
    Rgb color;
};
// Slime chunks (L-232): a green wash over the Overworld terrain, under every
// mark. Near, a rectangle per row run and the areas' outline; farther out,
// prepared images of whole squares of chunks (SlimeImageLevel). Too far to
// tell chunks apart it is not drawn; the place under the pointer names a
// slime chunk at any zoom.
constexpr std::uint32_t slimeColor = waypointColors[3];
information::SlimeChunkCache slimeCache;
struct SlimeSlot {
    std::optional<std::tuple<int, int, int>> key; // chunks per side, image x, image z
    bool uploaded = false;
    double used = 0;
    unsigned readyAt = 0; // as Slot: a reused slot shows its old image a few frames
};
std::array<SlimeSlot, slimeSlotCount> slimeSlots;
bool slimeShown() { return state.dimension == 0 && Runtime::instance().preferences().map.worldMapSlime; }
double slimeUnitsPerChunk() { return state.view.scale() * 16; }
bool slimeDrawable() {
    return slimeUnitsPerChunk() >= information::slimeRectMinUnits || information::slimeImageLevel(slimeUnitsPerChunk());
}
void drawSlimeImages(MinecraftUIRenderContext& context, information::SlimeImageLevel level, int x0, int x1, int z0, int z1) {
    auto const& view = state.view;
    auto floorDiv = [](int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
    int bx0 = floorDiv(x0, level.chunks), bx1 = floorDiv(x1, level.chunks);
    int bz0 = floorDiv(z0, level.chunks), bz1 = floorDiv(z1, level.chunks);
    if ((bx1 - bx0 + 1) * (bz1 - bz0 + 1) > slimeSlotCount) return; // never at the levels' zooms
    double time = now();
    int budget = 4;
    std::array<bool, slimeSlotCount> usedNow{};
    for (int bz = bz0; bz <= bz1; ++bz)
        for (int bx = bx0; bx <= bx1; ++bx) {
            std::tuple key{level.chunks, bx, bz};
            int index = -1;
            for (int i = 0; i < slimeSlotCount; ++i)
                if (slimeSlots[static_cast<size_t>(i)].key == key) { index = i; break; }
            if (index < 0) {
                if (budget <= 0) continue;
                // An empty slot first, then the one shown longest ago.
                auto age = [](SlimeSlot const& s) { return std::pair{s.key.has_value(), s.used}; };
                for (int i = 0; i < slimeSlotCount; ++i)
                    if (!usedNow[static_cast<size_t>(i)]
                        && (index < 0 || age(slimeSlots[static_cast<size_t>(i)]) < age(slimeSlots[static_cast<size_t>(index)])))
                        index = i;
                if (index < 0) continue;
                --budget;
                auto& slot = slimeSlots[static_cast<size_t>(index)];
                auto pixels = information::slimeImage(bx * level.chunks, bz * level.chunks, level, packColor(127, 214, 90, 77),
                                                      packColor(127, 214, 90, 140), slimeCache);
                try {
                    if (upload(context.mClient, slotLocation(slimeSlotBase + index), pixels, level.chunks * level.perChunk, slot.uploaded)) {
                        slot.readyAt = tileFrame + 3;
                        slot.uploaded = true;
                        slot.key = key;
                    } else slot = SlimeSlot{};
                } catch (...) { slot = SlimeSlot{}; }
                if (!slot.key) continue;
            }
            auto& slot = slimeSlots[static_cast<size_t>(index)];
            slot.used = time;
            usedNow[static_cast<size_t>(index)] = true;
            if (tileFrame < slot.readyAt) continue;
            double blocks = level.chunks * 16.0;
            float sx0 = static_cast<float>(snapToPixel(view.screenX(bx * blocks), state.pixelsPerUnit));
            float sx1 = static_cast<float>(snapToPixel(view.screenX((bx + 1) * blocks), state.pixelsPerUnit));
            float sy0 = static_cast<float>(snapToPixel(view.screenY(bz * blocks), state.pixelsPerUnit));
            float sy1 = static_cast<float>(snapToPixel(view.screenY((bz + 1) * blocks), state.pixelsPerUnit));
            if (!ui::runtimeImage(context, slotLocation(slimeSlotBase + index), {sx0, sy0, sx1 - sx0, sy1 - sy0}))
                slot = SlimeSlot{};
        }
}
void drawSlimeChunks(MinecraftUIRenderContext& context) {
    if (!slimeShown() || !slimeDrawable()) return;
    auto const& view = state.view;
    auto chunk = [](double block) { return static_cast<int>(std::floor(block / 16)); };
    int x0 = chunk(view.worldX(0)), x1 = chunk(view.worldX(view.width));
    int z0 = chunk(view.worldZ(state.top)), z1 = chunk(view.worldZ(view.height));
    if (auto level = information::slimeImageLevel(slimeUnitsPerChunk())) {
        drawSlimeImages(context, *level, x0, x1, z0, z1);
        return;
    }
    auto color = rgb(slimeColor);
    auto sx = [&](int edge) { return std::round(static_cast<float>(view.screenX(edge * 16.0))); };
    auto sy = [&](int edge) { return std::round(static_cast<float>(view.screenY(edge * 16.0))); };
    for (auto const& run : information::slimeRuns(x0, x1, z0, z1, slimeCache))
        ui::fill(context, sx(run.x0), sy(run.z), sx(run.x1 + 1) - sx(run.x0), sy(run.z + 1) - sy(run.z), color, .3f);
    // The outline of each area, not of each run, so chunks touching up or
    // down read as one area like those side by side.
    for (auto const& edge : information::slimeEdges(x0, x1, z0, z1, slimeCache)) {
        float inside = edge.before ? -1.f : 0.f;
        if (edge.horizontal) ui::fill(context, sx(edge.from), sy(edge.line) + inside, sx(edge.to) - sx(edge.from), 1, color, .55f);
        else ui::fill(context, sx(edge.line) + inside, sy(edge.from), 1, sy(edge.to) - sy(edge.from), color, .55f);
    }
}
// Footprints (L-139): shapes, then schematic placements over them, each a
// faint fill, a frame and its name, under the other markers. Hidden ones
// stay faint, like hidden waypoints; the selected one is framed white.
void drawAreas(MinecraftUIRenderContext& context, std::vector<MarkName>& names) {
    auto& view = state.view;
    auto on = layers();
    state.allAreas = shapeMarks(state.dimension);
    auto placements = placementMarks(state.dimension);
    state.allAreas.insert(state.allAreas.end(), placements.begin(), placements.end());
    state.areas.clear();
    for (auto const& mark : state.allAreas)
        if (layerShown(on, mark.key.layer)) state.areas.push_back(mark);
    for (auto const& mark : state.areas) {
        auto const& a = mark.area;
        float x0 = std::round(static_cast<float>(view.screenX(a.x0))), y0 = std::round(static_cast<float>(view.screenY(a.z0)));
        float x1 = std::round(static_cast<float>(view.screenX(a.x1))), y1 = std::round(static_cast<float>(view.screenY(a.z1)));
        // Never smaller than a few units, so a far-out view still shows it.
        float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
        float w = std::max(x1 - x0, 4.f), h = std::max(y1 - y0, 4.f);
        x0 = cx - w / 2; y0 = cy - h / 2;
        if (x0 + w < -20 || y0 + h < -20 || x0 > view.width + 20 || y0 > view.height + 20) continue;
        auto color = rgb(mark.color);
        bool chosen = mark.selected || (state.panelWidth > 0 && state.selected == mark.key);
        float opacity = mark.visible ? 1.f : .35f;
        ui::fill(context, x0, y0, w, h, color, .16f * opacity);
        ui::frame(context, x0 - 1, y0 - 1, w + 2, h + 2, Rgb{0, 0, 0}, .6f * opacity);
        ui::frame(context, x0, y0, w, h, chosen ? ui::palette::white : color, opacity);
        auto name = mark.visible ? mark.name : mark.name + " " + ui::translated("worldMap.hidden");
        names.push_back({mark.key, cx, y0 + h + 2, std::move(name), mark.visible ? ui::palette::text : ui::palette::faint});
        state.markers.push_back({mark.key, x0, y0, x0 + w, y0 + h});
    }
}
// Many names overlap where marks crowd, so by default a name shows only for
// the mark under the pointer, the selected one and the one with a menu open.
void drawNames(MinecraftUIRenderContext& context, std::vector<MarkName> const& names, bool always) {
    auto p = state.pointer;
    bool onMap = p.y > state.top && p.y < state.view.height - bottomHeight && p.x < state.view.width - state.panelWidth
        && !(state.drag && state.drag->moved);
    auto hovered = onMap ? markerAt(p.x, p.y) : std::nullopt;
    for (auto const& name : names) {
        bool wanted = always || name.key == hovered || (state.panelWidth > 0 && state.selected == name.key)
            || (state.menu && state.menu->kind != MenuKind::Ground && state.menu->mark == name.key);
        if (wanted) smallLabel(context, name.cx, name.y, name.text, name.color);
    }
}
void drawMarkers(MinecraftUIRenderContext& context, Settings::Map const& settings) {
    auto& view = state.view;
    state.markers.clear();
    std::vector<MarkName> names;
    drawSlimeChunks(context);
    drawAreas(context, names);
    // A footprint whose provider turned off, or whose entry is gone, takes
    // its selection and menu with it; one only switched off on the map stays
    // selectable from the side panel (L-204).
    if (state.selected && !pointLayer(state.selected->layer) && !areaOf(*state.selected)) state.selected.reset();
    if (state.menu && (state.menu->kind == MenuKind::Placement || state.menu->kind == MenuKind::Shape) && !areaOf(state.menu->mark))
        state.menu.reset();
    auto spot = playerSpot();
    if (settings.radar && settings.radarPlayers && spot && spot->dimension == state.dimension) {
        faces::frame();
        for (auto const& dot : collectDots(context.mClient, spot->x, spot->z, 3.0e7, spot->y, settings.radarInvisible,
                                           settings.radarFaces)) {
            if (dot.kind != DotKind::Player) continue;
            float x = static_cast<float>(view.screenX(dot.x)), y = static_cast<float>(view.screenY(dot.z));
            // A head is about 10 units with its outline, a dot 4.
            float alpha = dot.distant ? distantAlpha : 1.f;
            bool head = dot.face >= 0 && faces::draw(context, dot.face, x, y, 8, alpha);
            if (!head) {
                ui::fill(context, x - 2, y - 2, 4, 4, Rgb{0, 0, 0}, .85f * alpha);
                ui::fill(context, x - 1.5f, y - 1.5f, 3, 3, rgb(dotColor(DotKind::Player)), alpha);
            }
            if (!dot.name.empty())
                smallLabel(context, x, y + (head ? 6 : 3), dot.name, dot.distant ? ui::palette::dim : Rgb{.59f, .88f, 1.f});
        }
    }
    if (settings.waypoints && layers().waypoints) {
        auto set = waypoints::current();
        for (size_t i = 0; i < set.waypoints.size(); ++i) {
            auto const& w = set.waypoints[i];
            auto at = shownPosition(w.x, w.y, w.z, w.dimension, state.dimension, settings.waypointsCrossScale);
            if (!at) continue;
            float x = std::round(static_cast<float>(view.screenX(at->x))), y = std::round(static_cast<float>(view.screenY(at->z)));
            if (x < -20 || y < -20 || x > view.width + 20 || y > view.height + 20) continue;
            // Hidden ones stay faint here so they can be shown again.
            float opacity = w.visible ? 1.f : .35f;
            if (state.panelWidth > 0 && state.selected == waypointKey(w.id)) diamond(context, x, y, 13, ui::palette::white, 1);
            diamond(context, x, y, 9, rgb(waypointColors[static_cast<size_t>(clampColor(w.color))]), opacity, w.temporary);
            auto name = w.visible ? w.name : w.name + " " + ui::translated("worldMap.hidden");
            names.push_back({waypointKey(w.id), x, y + 6, std::move(name), w.visible ? ui::palette::text : ui::palette::faint});
            state.markers.push_back({waypointKey(w.id), x, y, x, y});
        }
        if (set.death && set.death->dimension == state.dimension) {
            float x = std::round(static_cast<float>(view.screenX(set.death->x + .5)));
            float y = std::round(static_cast<float>(view.screenY(set.death->z + .5)));
            if (state.panelWidth > 0 && state.selected == deathKey) ui::frame(context, x - 5, y - 5, 10, 10, ui::palette::white);
            cross(context, x, y);
            names.push_back({deathKey, x, y + 5, ui::translated("waypoint.death"), ui::palette::text});
            state.markers.push_back({deathKey, x, y, x, y});
        }
    }
    drawNames(context, names, settings.worldMapNames);
    if (spot && spot->dimension == state.dimension) {
        // The arrow is drawn into a small texture, uploaded again when it turns.
        constexpr int side = 32;
        float angle = static_cast<float>(northUpArrowAngle(spot->yaw));
        auto& client = context.mClient;
        if (!state.arrowUploaded || !(std::abs(angle - state.arrowAngle) < .01f)) {
            std::vector<std::uint32_t> pixels(side * side, 0);
            drawArrow(pixels, side, side / 2.0, side / 2.0, angle, 22);
            try {
                if (upload(client, slotLocation(slotCount), pixels, side, state.arrowUploaded)) {
                    state.arrowUploaded = true;
                    state.arrowAngle = angle;
                } else state.arrowUploaded = false;
            } catch (...) { state.arrowUploaded = false; }
        }
        float x = static_cast<float>(view.screenX(spot->x)), y = static_cast<float>(view.screenY(spot->z));
        if (state.arrowUploaded && !ui::runtimeImage(context, slotLocation(slotCount), {x - 8, y - 8, 16, 16}))
            state.arrowUploaded = false;
    }
    // FreeCamera: the same arrow shape in green, so its heading stays easy to
    // read without inventing a second marker vocabulary (L-228).
    if (auto camera = freeCameraSpot(); camera && camera->dimension == state.dimension) {
        constexpr int side = 32;
        float x = static_cast<float>(view.screenX(camera->x)), y = static_cast<float>(view.screenY(camera->z));
        if (x >= 0 && x <= view.width - state.panelWidth && y > state.top && y < view.height - bottomHeight) {
            float angle = static_cast<float>(northUpArrowAngle(camera->yaw));
            if (!state.cameraArrowUploaded || !(std::abs(angle - state.cameraArrowAngle) < .01f)) {
                std::vector<std::uint32_t> pixels(side * side, 0);
                drawArrow(pixels, side, side / 2.0, side / 2.0, angle, 22, cameraArrowColor);
                try {
                    if (upload(context.mClient, slotLocation(slotCount + 1), pixels, side, state.cameraArrowUploaded)) {
                        state.cameraArrowUploaded = true;
                        state.cameraArrowAngle = angle;
                    } else state.cameraArrowUploaded = false;
                } catch (...) { state.cameraArrowUploaded = false; }
            }
            if (state.cameraArrowUploaded
                && !ui::runtimeImage(context, slotLocation(slotCount + 1), {x - 8, y - 8, 16, 16}))
                state.cameraArrowUploaded = false;
        }
    }
}

// A footprint as the map draws it, small: a faint fill in its frame.
void footprintIcon(MinecraftUIRenderContext& context, float x, float cy, Rgb color, float alpha) {
    ui::fill(context, x, cy - 3, 7, 6, color, .16f * alpha);
    ui::frame(context, x, cy - 3, 7, 6, color, alpha);
}
constexpr std::uint32_t shapeSymbolColor = packColor(242, 204, 61);
// A layer's symbol in the top bar and the list menu.
void layerSymbol(MinecraftUIRenderContext& context, int kind, float x, float cy, float alpha) {
    if (kind == 0) diamond(context, x + 4, cy, 7, rgb(waypointColors[0]), alpha);
    else if (kind == 3) {
        // Slime chunks: two filled cells on a diagonal, like the map's.
        ui::fill(context, x, cy - 3, 3, 3, rgb(slimeColor), alpha);
        ui::fill(context, x + 4, cy, 3, 3, rgb(slimeColor), alpha);
    } else footprintIcon(context, x, cy, rgb(kind == 1 ? placementColor : shapeSymbolColor), alpha);
}
void drawBars(MinecraftUIRenderContext& context, glm::vec2 size, size_t pending, bool recording) {
    auto spot = playerSpot();
    float inset = ui::boxTextInset();
    auto widthOf = [&](std::string const& text) { return ui::textWidth(context, text) + 8; };
    // Top bar (docs/demos/worldmap-review.html, B): one row; the Nether layer
    // moves to a second row only on a screen too narrow for it.
    constexpr std::array<std::pair<Target, std::string_view>, 3> dims{{
        {Target::Overworld, "worldMap.overworld"}, {Target::Nether, "worldMap.nether"}, {Target::End, "worldMap.end"}}};
    std::array<std::string, 3> dimTexts;
    float dimsW = 8;
    for (int d = 0; d < 3; ++d) {
        dimTexts[static_cast<size_t>(d)] = (spot && spot->dimension == d ? "* " : "") + ui::translated(dims[static_cast<size_t>(d)].second);
        dimsW += widthOf(dimTexts[static_cast<size_t>(d)]) - 1;
    }
    auto centerText = ui::translated("worldMap.center"), waypointsText = ui::translated("worldMap.list");
    float closeW = 13, rightW = widthOf(centerText) + 3 + widthOf(waypointsText) + 3 + closeW + 3;
    auto terrainText = stateText();
    float terrainW = state.dimension == 2 ? 0 : widthOf(terrainText);
    // Layer switches (L-146): the mark's own symbol and the layer's name;
    // slime chunks (L-232) only where there are any.
    constexpr std::array<std::pair<Target, std::string_view>, 4> switches{{
        {Target::LayerWaypoints, "nav.waypoints"}, {Target::LayerSchematics, "nav.schematics"}, {Target::LayerShapes, "nav.shapes"},
        {Target::LayerSlime, "worldMap.slimeChunks"}}};
    size_t const switchCount = state.dimension == 0 ? 4 : 3;
    auto shownLayers = layers();
    std::array<bool, 4> switchOn{shownLayers.waypoints, shownLayers.schematics, shownLayers.shapes,
                                 Runtime::instance().preferences().map.worldMapSlime};
    std::array<std::string, 4> switchTexts;
    float namedW = 1;
    for (size_t i = 0; i < switchCount; ++i) {
        switchTexts[i] = ui::translated(switches[i].second);
        namedW += 12 + widthOf(switchTexts[i]) - 1;
    }
    float brandW = ui::textWidth(context, "Lamium") + 6;
    constexpr float iconW = 15;
    float const iconsW = 1 + static_cast<float>(switchCount) * (iconW - 1);
    auto fits = [&](float width) { return 4 + brandW + dimsW + terrainW + 8 + width + 8 + rightW + 4 <= size.x; };
    bool const namedFits = fits(namedW), iconFits = fits(iconsW);
    bool compact = !namedFits;
    bool oneRow = namedFits || iconFits;
    float switchesW = namedFits ? namedW : iconsW;
    if (!oneRow) {
        // The switches alone move to a second row; keep their names if they
        // fit there, otherwise use the symbols.
        compact = 4 + namedW > size.x;
        switchesW = compact ? iconsW : namedW;
    }
    state.top = oneRow ? rowHeight : 2 * rowHeight;
    ui::fill(context, 0, 0, size.x, state.top, ui::palette::panel, .85f);
    ui::fill(context, 0, state.top - 1, size.x, 1, ui::palette::white, .14f);
    float x = 4, y = 2;
    ui::label(context, x, y + inset, brandW, "Lamium");
    x += brandW;
    // Neighbours share a border column, so the selected one is drawn last to keep its accent frame whole.
    std::array<float, 3> dimX{};
    for (int d = 0; d < 3; ++d) {
        dimX[static_cast<size_t>(d)] = x;
        x += widthOf(dimTexts[static_cast<size_t>(d)]) - 1;
    }
    for (int pass = 0; pass < 2; ++pass)
        for (int d = 0; d < 3; ++d) {
            if ((state.dimension == d) != (pass == 1)) continue;
            auto const& text = dimTexts[static_cast<size_t>(d)];
            float bx = dimX[static_cast<size_t>(d)];
            button(context, bx, y, text, dims[static_cast<size_t>(d)].first, state.dimension == d,
                hovering(bx, y, widthOf(text), buttonHeight));
        }
    x += 8;
    if (state.dimension != 2) {
        // The one terrain control: its panel opens directly under it.
        float w = widthOf(terrainText);
        bool over = hovering(x, y, w, buttonHeight);
        ui::fill(context, x, y, w, buttonHeight, state.cavePanel ? ui::palette::accentDeep : over ? Rgb{.23f, .23f, .24f} : ui::palette::keyFill, .9f);
        ui::frame(context, x, y, w, buttonHeight, state.cavePanel ? ui::palette::accent : ui::palette::keyEdge);
        ui::label(context, x, y + inset, w, terrainText, ui::palette::text, ui::Align::Center);
        state.hits.push_back({x, y, w, buttonHeight, Target::CaveButton});
        state.caveButton = {x, y, w, buttonHeight};
    } else {
        state.caveButton = {};
    }
    float right = size.x - 4 - closeW;
    {
        bool over = hovering(right, 2, closeW, buttonHeight);
        ui::fill(context, right, 2, closeW, buttonHeight, over ? Rgb{.23f, .23f, .24f} : ui::palette::keyFill, .9f);
        ui::frame(context, right, 2, closeW, buttonHeight, ui::palette::keyEdge);
        for (int i = 0; i < 5; ++i) {
            ui::fill(context, right + 4 + i, 5 + i, 1, 1, over ? ui::palette::text : ui::palette::dim);
            ui::fill(context, right + 8 - i, 5 + i, 1, 1, over ? ui::palette::text : ui::palette::dim);
        }
        state.hits.push_back({right, 2, closeW, buttonHeight, Target::Close});
    }
    right -= 3 + widthOf(waypointsText);
    button(context, right, 2, waypointsText, Target::Waypoints, panelOpen(), hovering(right, 2, widthOf(waypointsText), buttonHeight));
    right -= 3 + widthOf(centerText);
    button(context, right, 2, centerText, Target::Center, false, hovering(right, 2, widthOf(centerText), buttonHeight));
    if (oneRow) { x = right - 8 - switchesW; y = 2; }
    else { x = 4; y = rowHeight + 2; }
    std::array<float, 4> switchX{};
    for (size_t i = 0; i < switchCount; ++i) {
        switchX[i] = x;
        x += (compact ? iconW : 12 + widthOf(switchTexts[i])) - 1;
    }
    std::optional<size_t> overSwitch;
    // Neighbours share a border column: switches that are on are drawn last, keeping their accent frames whole.
    for (int pass = 0; pass < 2; ++pass)
        for (size_t i = 0; i < switchCount; ++i) {
            bool on = switchOn[i];
            if (on != (pass == 1)) continue;
            float bx = switchX[i], bw = compact ? iconW : 12 + widthOf(switchTexts[i]);
            bool over = hovering(bx, y, bw, buttonHeight);
            if (over) overSwitch = i;
            ui::fill(context, bx, y, bw, buttonHeight, on ? ui::palette::accentDeep : over ? Rgb{.23f, .23f, .24f} : ui::palette::keyFill, .9f);
            ui::frame(context, bx, y, bw, buttonHeight, on ? ui::palette::accent : ui::palette::keyEdge);
            float alpha = on ? 1.f : .4f, iy = y + buttonHeight / 2;
            layerSymbol(context, static_cast<int>(i), bx + 4, iy, alpha);
            if (!compact)
                ui::label(context, bx + 12, y + inset, bw - 12, switchTexts[i], on || over ? ui::palette::text : ui::palette::faint,
                          ui::Align::Center);
            state.hits.push_back({bx, y, bw, buttonHeight, switches[i].first});
        }

    // Bottom bar: the place under the cursor on the left; the scale bar on
    // the right with the hints before it, dropped when they would meet.
    float top = size.y - bottomHeight;
    ui::fill(context, 0, top, size.x, bottomHeight, ui::palette::panel, .85f);
    ui::fill(context, 0, top, size.x, 1, ui::palette::white, .14f);
    float textY = top + 1 + inset;
    std::string where;
    if (state.pointer.y > state.top && state.pointer.y < top) {
        int wx = blockFloor(state.view.worldX(state.pointer.x)), wz = blockFloor(state.view.worldZ(state.pointer.y));
        auto column = store::column(shownLayer(), wx, wz);
        where = column ? std::format("X {}  Y {}  Z {}", wx, column->height, wz) : std::format("X {}  Z {}", wx, wz);
        std::string biome;
        if (spot && spot->dimension == state.dimension)
            if (auto* player = state.client ? state.client->getLocalPlayer() : nullptr) {
                auto& region = player->getDimensionBlockSource();
                BlockPos pos{wx, column ? column->height : blockFloor(spot->y), wz};
                if (region.getChunkAt(pos)) biome = information::biomeName(region.getBiome(pos).mHash->getString());
            }
        if (!biome.empty()) where += "  " + biome;
        else if (!column) where += "  " + ui::translated("worldMap.unrecorded");
        if (slimeShown() && slimeCache(blockFloor(wx / 16.0), blockFloor(wz / 16.0))) where += "  " + ui::translated("debugSlimeChunk");
    } else if (overSwitch) {
        where = ui::translated("worldMap.layerShown", switchTexts[*overSwitch], ui::translated(switchOn[*overSwitch] ? "on" : "off"));
        if (switches[*overSwitch].first == Target::LayerSlime && switchOn[*overSwitch] && !slimeDrawable())
            where += "  " + ui::translated("worldMap.slimeZoomIn");
    }
    float whereW = where.empty() ? 0 : ui::textWidth(context, where);
    ui::label(context, 4, textY, whereW + 2, where, ui::palette::dim);
    int blocks = scaleBarBlocks(state.view.scale(), 30);
    auto scaleText = ui::translated("worldMap.blocks", blocks);
    float barW = static_cast<float>(blocks * state.view.scale());
    float textW = ui::textWidth(context, scaleText);
    float barX = size.x - 4 - textW - 4 - barW;
    ui::fill(context, barX, top + 8, barW, 1, ui::palette::white);
    ui::fill(context, barX, top + 5, 1, 4, ui::palette::white);
    ui::fill(context, barX + barW - 1, top + 5, 1, 4, ui::palette::white);
    ui::label(context, size.x - 4 - textW, textY, textW + 2, scaleText, ui::palette::dim);
    float end = barX - 10, start = 4 + whereW + 10;
    if (!recording) {
        auto off = ui::translated("worldMap.recordingOff");
        float w = ui::textWidth(context, off);
        if (end - w >= start) { ui::label(context, end - w, textY, w + 2, off, ui::palette::warning); end -= w + 10; }
    }
    auto hint = ui::translated("worldMap.hint");
    float hintW = ui::textWidthScaled(context, hint, .75f);
    if (end - hintW >= start) {
        ui::labelScaled(context, end - hintW, top + 3, hintW + 2, hint, .75f, ui::palette::faint, ui::Align::Left, false);
        end -= hintW + 10;
    }
    if (pending) {
        auto loading = ui::translated("worldMap.loading", pending);
        float w = ui::textWidth(context, loading);
        if (end - w >= start) ui::label(context, end - w, textY, w + 2, loading, ui::palette::dim);
    }
}

void drawCavePanel(MinecraftUIRenderContext& context, glm::vec2 size) {
    if (!state.cavePanel || state.dimension == 2) return;
    constexpr float w = 212;
    bool const camera = cameraAvailable();
    float h = state.cave ? 96 : 52;
    float available = size.x - state.panelWidth - 3;
    float left = std::clamp(state.caveButton.x, 3.f, std::max(3.f, available - w));
    float top = std::max(state.caveButton.y + state.caveButton.h + 1, state.top + 1);
    if (top + h > size.y - bottomHeight) top = std::max(state.top + 1.f, size.y - bottomHeight - h);
    ui::fill(context, left, top, w, h, ui::palette::panel, .92f);
    ui::frame(context, left, top, w, h, ui::palette::white, .14f);
    // The panel background swallows clicks between its rows.
    state.hits.push_back({left, top, w, h, Target::CavePanel});
    float inset = ui::boxTextInset(), x = left + 5, y = top + 4;
    ui::label(context, x, y, w - 10, ui::translated("worldMap.cavePanel"), ui::palette::faint);
    if (state.cave) {
        auto reference = ui::translated("worldMap.reference",
            ui::translated(camera && state.cameraReference ? "worldMap.referenceCamera" : "worldMap.referenceBody"));
        ui::label(context, x, y, w - 10, reference, ui::palette::faint, ui::Align::Right);
    }
    y += 10;
    auto labelAt = [&](float rowY, std::string const& text) {
        ui::label(context, x, rowY + 2, 52, text, ui::palette::dim);
    };
    labelAt(y, ui::translated("worldMap.cave"));
    if (state.dimension == 1) {
        ui::label(context, x + 54, y + 2, w - 64, ui::translated("worldMap.alwaysCave"), ui::palette::faint);
    } else {
        float tx = x + 54, ty = y + 4;
        ui::toggleSwitch(context, tx, ty, state.cave);
        state.hits.push_back({tx, ty, ui::switchWidth, ui::switchHeight, Target::CaveToggle});
        ui::label(context, x + 76, y + 2, w - 86,
                  ui::translated(state.cave ? "worldMap.caveOn" : "worldMap.surfaceOn"), ui::palette::faint);
    }
    y += 18;
    if (state.cave) {
        labelAt(y, ui::translated("worldMap.broadToggle"));
        float tx = x + 54, ty = y + 4;
        ui::toggleSwitch(context, tx, ty, state.broad);
        state.hits.push_back({tx, ty, ui::switchWidth, ui::switchHeight, Target::BroadToggle});
        ui::label(context, x + 76, y + 2, w - 86,
                  ui::translated(state.broad ? "worldMap.broadOn" : "worldMap.layerOn"), ui::palette::faint);
        y += 18;
        ui::fill(context, x, y, w - 10, 1, ui::palette::white, .14f);
        y += 4;
        labelAt(y, ui::translated("worldMap.height"));
        float bx = x + 54, by = y + 1;
        auto tiny = [&](float tx, char const* text, Target target) {
            bool over = hovering(tx, by, 14, buttonHeight);
            ui::fill(context, tx, by, 14, buttonHeight, over ? Rgb{.23f, .23f, .24f} : ui::palette::keyFill, .9f);
            ui::frame(context, tx, by, 14, buttonHeight, ui::palette::keyEdge);
            ui::label(context, tx, by + inset, 14, text, ui::palette::text, ui::Align::Center);
            state.hits.push_back({tx, by, 14, buttonHeight, target});
        };
        tiny(bx, "-", Target::HeightDown);
        float fx = bx + 16, fw = 44, fh = 13, fy = y;
        ui::fill(context, fx, fy, fw, fh, ui::palette::keyFill);
        ui::frame(context, fx, fy, fw, fh, state.editingHeight ? ui::palette::accent : ui::palette::keyEdge);
        if (state.editingHeight) {
            ui::drawEditField(context, fx, fy, fh, fw, state.heightInput, ui::Align::Center);
            state.namePosition = {fx + 3, fy + 2};
        } else {
            ui::label(context, fx, fy + 1 + inset, fw, std::to_string(state.height), ui::palette::text, ui::Align::Center);
            state.hits.push_back({fx, fy, fw, fh, Target::HeightField});
        }
        tiny(bx + 62, "+", Target::HeightUp);
        auto tracking = ui::translated(state.following
                ? (state.cameraReference && camera ? "worldMap.cameraY" : "worldMap.followingY")
                : "worldMap.fixedY", state.height);
        ui::label(context, x + 132, y + 2, w - 142, tracking, ui::palette::faint, ui::Align::Right);
        y += 18;
        bool enabled = canMatchHeight() && !state.following;
        bool over = enabled && hovering(x, y, w - 10, 15);
        bool const cameraMatch = state.cameraReference && camera;
        auto text = !canMatchHeight() ? ui::translated("worldMap.matchUnavailable")
            : state.following ? (cameraMatch ? ui::translated("worldMap.matchingCameraHeight", state.height)
                                             : ui::translated("worldMap.matchingHeight", state.height))
                              : (cameraMatch ? ui::translated("worldMap.matchCameraHeight")
                                             : ui::translated("worldMap.matchHeight"));
        ui::fill(context, x, y, w - 10, 15, enabled ? (over ? ui::palette::accent : ui::palette::accentDeep) : ui::palette::keyFill, .9f);
        ui::frame(context, x, y, w - 10, 15, enabled ? ui::palette::accent : ui::palette::keyEdge);
        ui::label(context, x, y + 2 + inset, w - 10, text, enabled ? ui::palette::text : ui::palette::faint, ui::Align::Center);
        if (enabled) state.hits.push_back({x, y, w - 10, 15, Target::MatchHeight});
    } else {
        ui::label(context, x, y + 2, w - 10, ui::translated("worldMap.caveOffHint"), ui::palette::faint);
    }
}

void drawPanel(MinecraftUIRenderContext& context, glm::vec2 size) {
    if (!panelOpen()) { state.panelWidth = 0; return; }
    auto set = waypoints::current();
    auto spot = playerSpot();
    float w = std::clamp(size.x * .3f, 124.f, 160.f), x0 = size.x - w, top = state.top, bottom = size.y - bottomHeight;
    state.panelWidth = w;
    ui::fill(context, x0, top, w, bottom - top, ui::palette::panel, .92f);
    ui::fill(context, x0, top, 1, bottom - top, ui::palette::white, .14f);
    state.hits.push_back({x0, top, w, bottom - top, Target::Panel});
    constexpr float pad = 4, rowH = 11;
    float inset = ui::boxTextInset(), inner = w - 2 * pad, x = x0 + pad, y = top + 2;

    // Rows: the death point here, then this dimension's waypoints, nearest first.
    std::vector<int> entries; // -1 the death point, else into the set
    if (set.death && set.death->dimension == state.dimension) entries.push_back(-1);
    std::vector<int> listed;
    for (size_t i = 0; i < set.waypoints.size(); ++i)
        if (set.waypoints[i].dimension == state.dimension) listed.push_back(static_cast<int>(i));
    auto distance = [&](int i) {
        auto const& p = set.waypoints[static_cast<size_t>(i)];
        return spot && spot->dimension == state.dimension ? std::hypot(p.x + .5 - spot->x, p.z + .5 - spot->z) : 0.0;
    };
    std::stable_sort(listed.begin(), listed.end(), [&](int a, int b) { return distance(a) < distance(b); });
    entries.insert(entries.end(), listed.begin(), listed.end());
    if (state.selected && pointLayer(state.selected->layer)
        && (state.selected->layer == MarkLayer::Death ? !set.death : !waypointOf(set, *state.selected)))
        state.selected.reset();
    // Or this dimension's placements or shapes (L-204), nearest first.
    int const kind = shownList();
    auto areaDistance = [&](AreaMark const& area) {
        return spot && spot->dimension == state.dimension
            ? std::hypot((area.area.x0 + area.area.x1) / 2.0 - spot->x, (area.area.z0 + area.area.z1) / 2.0 - spot->z) : 0.0;
    };
    std::vector<AreaMark const*> areaRows;
    state.kindCounts = {static_cast<int>(listed.size()), 0, 0};
    for (auto const& area : state.allAreas) {
        int k = listKind(area.key.layer);
        ++state.kindCounts[static_cast<size_t>(k)];
        if (k == kind) areaRows.push_back(&area);
    }
    std::stable_sort(areaRows.begin(), areaRows.end(),
                     [&](AreaMark const* a, AreaMark const* b) { return areaDistance(*a) < areaDistance(*b); });
    int const rows = kind == 0 ? static_cast<int>(entries.size()) : static_cast<int>(areaRows.size());

    // The header names the listed layer and opens the menu of layers.
    constexpr std::array<std::string_view, 3> kindNames{"nav.waypoints", "nav.schematics", "nav.shapes"};
    auto title = ui::translated(kindNames[static_cast<size_t>(kind)]);
    auto count = std::to_string(state.kindCounts[static_cast<size_t>(kind)]);
    float headW = std::min(ui::textWidth(context, title) + 4 + ui::textWidth(context, count) + 14, inner - 30);
    bool headOver = hovering(x - 2, y, headW, buttonHeight);
    if (headOver || state.kindMenu) ui::fill(context, x - 2, y, headW, buttonHeight, Rgb{.23f, .23f, .24f}, .9f);
    ui::frame(context, x - 2, y, headW, buttonHeight, state.kindMenu ? ui::palette::accent : ui::palette::keyEdge);
    ui::label(context, x, y + inset, headW - 12, title);
    float countX = x + ui::textWidth(context, title) + 4;
    if (countX < x - 2 + headW - 12) ui::label(context, countX, y + inset, x - 2 + headW - 12 - countX, count, ui::palette::faint);
    for (int i = 0; i < 3; ++i) // A small triangle pointing down.
        ui::fill(context, x - 2 + headW - 9 + i, y + 4 + i, 5 - 2 * i, 1, headOver || state.kindMenu ? ui::palette::text : ui::palette::dim);
    state.hits.push_back({x - 2, y, headW, buttonHeight, Target::PanelKind});
    state.kindX = x - 2; state.kindY = y + buttonHeight; state.kindW = inner + 4;
    float bx = x0 + w - pad - 11;
    button(context, bx, y, ">", Target::PanelClose, false, hovering(bx, y, 11, buttonHeight));
    bx -= 3 + ui::textWidth(context, "+") + 8;
    if (kind == 0) button(context, bx, y, "+", Target::PanelAdd, true, false);
    y += buttonHeight + 3;
    ui::fill(context, x0 + 1, y, w - 1, 1, ui::palette::white, .14f);
    y += 1;
    // A layer switched off on the map keeps its list, where it is found.
    if (!layerShown(layers(), kind == 0 ? MarkLayer::Waypoint : kind == 1 ? MarkLayer::Placement : MarkLayer::Shape)) {
        // Its own line, clear of the first row's highlight.
        ui::labelScaled(context, x, y + 3, inner, ui::translated("worldMap.hiddenOnMap"), .75f, ui::palette::faint);
        y += 13;
    }

    int maxRows = std::max(3, static_cast<int>((bottom - y) * .4f / rowH));
    state.listFirst = std::clamp(state.listFirst, 0, std::max(0, rows - maxRows));
    state.listTop = y;
    state.listBottom = y + maxRows * rowH;
    if (!rows) {
        constexpr std::array<std::string_view, 3> emptyTexts{"worldMap.noWaypoints", "worldMap.noPlacements", "worldMap.noShapes"};
        ui::label(context, x, y + 2, inner, ui::translated(emptyTexts[static_cast<size_t>(kind)]), ui::palette::faint);
    }
    for (int r = 0; kind != 0 && r < maxRows && state.listFirst + r < rows; ++r) {
        auto const& area = *areaRows[static_cast<size_t>(state.listFirst + r)];
        float ry = y + r * rowH;
        bool selected = area.key == state.selected, over = hovering(x0, ry, w, rowH);
        if (selected) {
            ui::fill(context, x0 + 1, ry, w - 1, rowH, ui::palette::accent, .16f);
            ui::fill(context, x0 + 1, ry, 2, rowH, ui::palette::accent);
        } else if (over) ui::fill(context, x0 + 1, ry, w - 1, rowH, ui::palette::white, .07f);
        state.hits.push_back({x0, ry, w, rowH, Target::Row, -2, area.key});
        footprintIcon(context, x + 1, ry + rowH / 2, rgb(area.color), area.visible ? 1.f : .4f);
        float switchX = x0 + w - pad - ui::switchWidth;
        std::string far;
        if (spot && spot->dimension == state.dimension)
            far = ui::translated("waypoint.meters", static_cast<int>(std::lround(areaDistance(area))));
        float farW = far.empty() ? 0 : ui::textWidthScaled(context, far, .75f);
        ui::label(context, x + 11, ry + inset, switchX - farW - 4 - x - 11, area.name, area.visible ? ui::palette::text : ui::palette::faint);
        if (!far.empty()) ui::labelScaled(context, switchX - 3 - farW, ry + 3, farW + 2, far, .75f, ui::palette::dim);
        ui::toggleSwitch(context, switchX, ry + 1, area.visible);
        state.hits.push_back({switchX - 1, ry, ui::switchWidth + 2, rowH, Target::RowVisible, -2, area.key});
    }
    for (int r = 0; kind == 0 && r < maxRows && state.listFirst + r < rows; ++r) {
        int index = entries[static_cast<size_t>(state.listFirst + r)];
        auto mark = index == -1 ? deathKey : waypointKey(set.waypoints[static_cast<size_t>(index)].id);
        float ry = y + r * rowH;
        bool selected = mark == state.selected, over = hovering(x0, ry, w, rowH);
        if (selected) {
            ui::fill(context, x0 + 1, ry, w - 1, rowH, ui::palette::accent, .16f);
            ui::fill(context, x0 + 1, ry, 2, rowH, ui::palette::accent);
        } else if (over) ui::fill(context, x0 + 1, ry, w - 1, rowH, ui::palette::white, .07f);
        state.hits.push_back({x0, ry, w, rowH, Target::Row, index, mark});
        if (index == -1) {
            cross(context, x + 4, ry + rowH / 2);
            ui::label(context, x + 11, ry + inset, inner - 11, ui::translated("waypoint.death"), ui::palette::text);
            continue;
        }
        auto const& p = set.waypoints[static_cast<size_t>(index)];
        diamond(context, x + 4, ry + rowH / 2, 7, rgb(waypointColors[static_cast<size_t>(clampColor(p.color))]), p.visible ? 1.f : .4f,
                p.temporary);
        float switchX = x0 + w - pad - ui::switchWidth;
        std::string far;
        if (spot && spot->dimension == state.dimension) far = ui::translated("waypoint.meters", static_cast<int>(std::lround(distance(index))));
        float farW = far.empty() ? 0 : ui::textWidthScaled(context, far, .75f);
        ui::label(context, x + 11, ry + inset, switchX - farW - 4 - x - 11, p.name, p.visible ? ui::palette::text : ui::palette::faint);
        if (!far.empty()) ui::labelScaled(context, switchX - 3 - farW, ry + 3, farW + 2, far, .75f, ui::palette::dim);
        ui::toggleSwitch(context, switchX, ry + 1, p.visible);
        state.hits.push_back({switchX - 1, ry, ui::switchWidth + 2, rowH, Target::RowVisible, index, mark});
    }
    y = state.listBottom + 2;
    ui::fill(context, x0 + 1, y, w - 1, 1, ui::palette::white, .14f);
    y += 4;

    auto small = [&](float sx, float sy, float width, std::string text, Target target, int item, bool danger = false) {
        float bw = std::min(width, ui::textWidth(context, text) + 8);
        bool over = hovering(sx, sy, bw, buttonHeight);
        Rgb fill = danger && state.deleteArmed ? Rgb{.54f, .18f, .16f} : over ? Rgb{.23f, .23f, .24f} : ui::palette::keyFill;
        ui::fill(context, sx, sy, bw, buttonHeight, fill);
        ui::frame(context, sx, sy, bw, buttonHeight, danger ? Rgb{.54f, .23f, .2f} : ui::palette::keyEdge);
        ui::label(context, sx, sy + inset, bw, std::move(text), danger && !state.deleteArmed ? Rgb{1.f, .7f, .68f} : ui::palette::text,
                  ui::Align::Center);
        state.hits.push_back({sx, sy, bw, buttonHeight, target, item});
        return bw;
    };
    auto deleteText = ui::translated(state.deleteArmed ? "worldMap.deleteArmed" : "worldMap.delete");
    if (state.selected == deathKey && set.death) {
        auto const& d = *set.death;
        ui::labelScaled(context, x, y, inner, std::format("{}, {}, {}", d.x, d.y, d.z), .75f, ui::palette::dim);
        y += 10;
        float used = small(x, y, inner, ui::translated("waypoint.keep"), Target::Keep, 0);
        small(x + used + 3, y, inner - used - 3, deleteText, Target::Delete, 0, true);
        return;
    }
    if (auto const* area = state.selected ? areaOf(*state.selected) : nullptr) {
        // A placement or a shape: what it covers, shown or not, and its own screen.
        ui::label(context, x, y + inset, inner, area->name);
        y += 12;
        auto const& a = area->area;
        ui::labelScaled(context, x, y, inner, std::format("X {}..{}  Z {}..{}", a.x0, a.x1 - 1, a.z0, a.z1 - 1), .75f, ui::palette::dim);
        y += 9;
        ui::labelScaled(context, x, y, inner, ui::translated("worldMap.areaSize", a.x1 - a.x0, a.z1 - a.z0), .75f, ui::palette::dim);
        y += 11;
        if (y + 12 < bottom) {
            ui::label(context, x, y + inset, inner - 24, ui::translated("waypoint.shown"), ui::palette::dim);
            float sx = x0 + w - pad - ui::switchWidth;
            ui::toggleSwitch(context, sx, y + 1, area->visible);
            state.hits.push_back({sx - 1, y, ui::switchWidth + 2, rowH, Target::RowVisible, 0, area->key});
            y += 14;
        }
        if (y + 12 < bottom)
            small(x, y, inner, ui::translated(area->key.layer == MarkLayer::Placement ? "worldMap.openPlaced" : "worldMap.openShapes"),
                  Target::OpenScreen, 0);
        return;
    }
    auto const* selectedWaypoint = state.selected ? waypointOf(set, *state.selected) : nullptr;
    if (!selectedWaypoint) {
        ui::labelScaled(context, x, y, inner, ui::translated("worldMap.selectHint"), .75f, ui::palette::faint);
        return;
    }
    auto const& p = *selectedWaypoint;
    // Name: click to type; Enter or a click elsewhere keeps it, Esc drops it.
    ui::fill(context, x, y, inner, 12, Rgb{0, 0, 0}, .4f);
    ui::frame(context, x, y, inner, 12, state.editingName ? ui::palette::accent : ui::palette::keyEdge);
    if (state.editingName) ui::drawEditField(context, x + 3, y, 12, inner - 6, state.name);
    else ui::label(context, x + 3, y + 1 + inset, inner - 6, p.name);
    state.hits.push_back({x, y, inner, 12, Target::Name});
    state.namePosition = {x + 3, y};
    y += 14;
    std::string info = std::format("{}, {}, {}", p.x, p.y, p.z);
    if (p.dimension != state.dimension) info += "  " + ui::translated(p.dimension == 1 ? "worldMap.nether" : p.dimension == 2 ? "worldMap.end" : "worldMap.overworld");
    ui::labelScaled(context, x, y, inner, info, .75f, ui::palette::dim);
    y += 9;
    constexpr std::array<std::string_view, 3> axes{"X", "Y", "Z"};
    for (int axis = 0; axis < 3 && y + 12 < bottom; ++axis) {
        int value = axis == 0 ? p.x : axis == 1 ? p.y : p.z;
        ui::label(context, x, y + inset, 12, std::string(axes[static_cast<size_t>(axis)]), ui::palette::dim);
        float sx = x0 + w - pad - 70;
        for (int side = 0; side < 2; ++side) {
            float bx2 = side ? sx + 61 : sx;
            bool over = hovering(bx2, y, 9, buttonHeight);
            ui::fill(context, bx2, y, 9, buttonHeight, over ? Rgb{.23f, .23f, .24f} : ui::palette::keyFill);
            ui::frame(context, bx2, y, 9, buttonHeight, ui::palette::keyEdge);
            ui::label(context, bx2, y + inset, 9, side ? "+" : "-", ui::palette::text, ui::Align::Center);
            state.hits.push_back({bx2, y, 9, buttonHeight, Target::Step, axis * 2 + side});
        }
        // The value: click to type it (L-172 follow-up, 2026-10-13).
        if (state.editingAxis == axis) {
            ui::frame(context, sx + 9, y, 52, buttonHeight, ui::palette::accent);
            ui::drawEditField(context, sx + 10, y, buttonHeight, 50, state.coordinate, ui::Align::Center);
        } else ui::label(context, sx + 9, y + inset, 52, std::to_string(value), ui::palette::text, ui::Align::Center);
        state.hits.push_back({sx + 9, y, 52, buttonHeight, Target::Coordinate, axis});
        y += 12;
    }
    if (y + 12 < bottom) { small(x, y, inner, ui::translated("waypoint.moveHere"), Target::MoveHere, 0); y += 13; }
    if (y + 12 < bottom) {
        ui::label(context, x, y + inset, inner - 24, ui::translated("waypoint.shown"), ui::palette::dim);
        float sx = x0 + w - pad - ui::switchWidth;
        ui::toggleSwitch(context, sx, y + 1, p.visible);
        state.hits.push_back({sx - 1, y, ui::switchWidth + 2, rowH, Target::RowVisible, 0, *state.selected});
        y += 12;
    }
    if (y + 12 < bottom) {
        ui::label(context, x, y + inset, inner - 24, ui::translated("waypoint.temporary"), ui::palette::dim);
        float sx = x0 + w - pad - ui::switchWidth;
        ui::toggleSwitch(context, sx, y + 1, p.temporary);
        state.hits.push_back({sx - 1, y, ui::switchWidth + 2, rowH, Target::Temporary, 0});
        y += 12;
    }
    if (y + 10 < bottom) {
        constexpr float swatch = 7, gap = 2;
        for (int i = 0; i < static_cast<int>(waypointColors.size()); ++i) {
            float sx = x + i * (swatch + gap);
            if (sx + swatch > x0 + w - pad) break;
            if (i == clampColor(p.color)) ui::frame(context, sx - 1, y - 1, swatch + 2, swatch + 2, ui::palette::white);
            ui::fill(context, sx, y, swatch, swatch, rgb(waypointColors[static_cast<size_t>(i)]));
            state.hits.push_back({sx - 1, y - 1, swatch + 2, swatch + 2, Target::Swatch, i});
        }
        y += swatch + 5;
    }
    if (y + 12 < bottom) {
        float used = small(x, y, inner, ui::translated("worldMap.openScreen"), Target::OpenScreen, 0);
        small(x + used + 3, y, inner - used - 3, deleteText, Target::Delete, 0, true);
    }
}
// The side panel's menu of layers, under its header, with each one's count.
void drawKindMenu(MinecraftUIRenderContext& context) {
    if (!state.kindMenu || state.panelWidth <= 0) return;
    constexpr float rowH = 12;
    constexpr std::array<std::string_view, 3> names{"nav.waypoints", "nav.schematics", "nav.shapes"};
    float x = state.kindX, y = state.kindY + 1, w = state.kindW;
    ui::fill(context, x, y, w, 3 * rowH + 2, ui::palette::panel, .98f);
    ui::frame(context, x, y, w, 3 * rowH + 2, ui::palette::keyEdge);
    for (int i = 0; i < 3; ++i) {
        float ry = y + 1 + i * rowH;
        bool current = i == shownList(), over = hovering(x, ry, w, rowH);
        if (current) ui::fill(context, x + 1, ry, w - 2, rowH, ui::palette::accent, .16f);
        else if (over) ui::fill(context, x + 1, ry, w - 2, rowH, ui::palette::white, .07f);
        layerSymbol(context, i, x + 3, ry + rowH / 2, 1.f);
        auto count = std::to_string(state.kindCounts[static_cast<size_t>(i)]);
        float countW = ui::textWidth(context, count);
        ui::label(context, x + 14, ry + 1 + ui::boxTextInset(), w - 20 - countW, ui::translated(names[static_cast<size_t>(i)]),
                  current || over ? ui::palette::text : ui::palette::dim);
        ui::label(context, x + w - 4 - countW, ry + 1 + ui::boxTextInset(), countW + 2, count, ui::palette::faint);
        state.hits.push_back({x, ry, w, rowH, Target::PanelKindItem, i});
    }
}
void drawMenu(MinecraftUIRenderContext& context, glm::vec2 size) {
    if (!state.menu) return;
    auto set = waypoints::current();
    auto items = menuItems(*state.menu, set);
    std::string head;
    auto const& m = *state.menu;
    if (m.kind == MenuKind::Ground)
        head = m.known ? std::format("{}, {}, {}", m.worldX, m.worldY, m.worldZ)
                       : std::format("{}, ?, {}  {}", m.worldX, m.worldZ, ui::translated("worldMap.unrecorded"));
    else if (m.kind == MenuKind::Placement || m.kind == MenuKind::Shape) {
        auto const* area = areaOf(m.mark);
        if (!area) { state.menu.reset(); return; }
        head = area->name + "  " + ui::translated("worldMap.areaSize", area->area.x1 - area->area.x0, area->area.z1 - area->area.z0);
    } else if (m.kind == MenuKind::Death) {
        if (!set.death) { state.menu.reset(); return; }
        head = ui::translated("waypoint.death") + std::format("  {}, {}, {}", set.death->x, set.death->y, set.death->z);
    } else {
        auto const* w = waypointOf(set, m.mark);
        if (!w) { state.menu.reset(); return; }
        head = w->name + std::format("  {}, {}, {}", w->x, w->y, w->z);
    }
    constexpr float itemHeight = 11, pad = 4;
    float width = ui::textWidthScaled(context, head, .75f) + 2 * pad;
    for (auto const& item : items) width = std::max(width, ui::textWidth(context, item) + 2 * pad);
    float height = 10 + items.size() * itemHeight + 3;
    float x = std::min(m.x, size.x - width - 2), y = std::min(m.y, size.y - bottomHeight - height - 2);
    ui::fill(context, x, y, width, height, ui::palette::panel, .94f);
    ui::frame(context, x, y, width, height, ui::palette::white, .14f);
    ui::labelScaled(context, x + pad, y + 2, width - 2 * pad, head, .75f, ui::palette::faint);
    ui::fill(context, x + 1, y + 9, width - 2, 1, ui::palette::white, .14f);
    for (size_t i = 0; i < items.size(); ++i) {
        float iy = y + 11 + i * itemHeight;
        bool danger = (m.kind == MenuKind::Waypoint && i == 2) || (m.kind == MenuKind::Death && i == 1);
        if (danger && m.armed) ui::fill(context, x + 1, iy, width - 2, itemHeight, Rgb{.54f, .18f, .16f});
        else if (hovering(x, iy, width, itemHeight)) ui::fill(context, x + 1, iy, width - 2, itemHeight, ui::palette::white, .07f);
        ui::label(context, x + pad, iy + 1, width - 2 * pad, items[i],
                  danger && !m.armed ? Rgb{1.f, .7f, .68f} : ui::palette::text);
        state.hits.push_back({x, iy, width, itemHeight, Target::Menu, static_cast<int>(i)});
    }
    // The menu swallows clicks on its padding too.
    state.hits.push_back({x, y, width, 11, Target::None});
}
}

void open(IClientInstance& client, bool resume) {
    state.client = &client;
    state.open = true;
    state.drag.reset();
    state.menu.reset();
    state.hits.clear();
    state.markers.clear();
    state.notice.clear();
    state.editingName = false;
    state.editingAxis = -1;
    state.editingHeight = false;
    state.cavePanel = false;
    state.openedAt = now();
    if (resume) return;
    state.selected.reset();
    state.deleteArmed = false;
    state.cameraReference = cameraAvailable();
    if (auto spot = sourceSpot()) {
        state.dimension = spot->dimension;
        state.cave = playerCave(*spot);
        state.broad = state.dimension == 1;
        state.height = state.cave ? recordedCaveHeight(*spot, state.dimension)
                                  : clampHeight(state.dimension, blockFloor(spot->y));
        state.following = true;
    } else {
        state.dimension = 0;
        state.cave = false;
        state.broad = false;
        state.height = 64;
        state.following = true;
    }
    center();
}
void close() {
    store::cancelWanted();
    if (state.client) {
        for (int i = 0; i < slotCount; ++i)
            if (slots[static_cast<size_t>(i)].uploaded) unload(*state.client, slotLocation(i));
        if (state.arrowUploaded) unload(*state.client, slotLocation(slotCount));
        if (state.cameraArrowUploaded) unload(*state.client, slotLocation(slotCount + 1));
        for (int i = 0; i < slimeSlotCount; ++i)
            if (slimeSlots[static_cast<size_t>(i)].uploaded) unload(*state.client, slotLocation(slimeSlotBase + i));
    }
    slots = {};
    slimeSlots = {};
    state.arrowUploaded = false;
    state.cameraArrowUploaded = false;
    state.open = false;
    state.client = nullptr;
    state.drag.reset();
    state.menu.reset();
    commitName();
}
bool cavePanelTarget(Target target) {
    switch (target) {
    case Target::CavePanel:
    case Target::CaveToggle:
    case Target::BroadToggle:
    case Target::HeightDown:
    case Target::HeightUp:
    case Target::HeightField:
    case Target::MatchHeight: return true;
    default: return false;
    }
}
Request press(float x, float y, bool right) {
    Request request;
    auto const* hit = hitAt(x, y);
    // Leaving a field for the cave panel or another control keeps its value;
    // only a press on the field being edited keeps it in edit mode.
    bool const heightField = hit && hit->target == Target::HeightField;
    bool const sameSideField = hit && ((hit->target == Target::Name && state.editingName)
        || (hit->target == Target::Coordinate && hit->item == state.editingAxis));
    if ((state.editingHeight || state.editingName || state.editingAxis >= 0) && !heightField && !sameSideField) commitName();
    // The cave panel sits above the map; its controls consume the press.
    if (hit && (hit->target == Target::CaveButton || cavePanelTarget(hit->target))) {
        if (!right) act(hit->target);
        return request;
    }
    if (state.cavePanel) {
        state.cavePanel = false;
        // A map press only closes the panel; a bar or side-panel press goes on.
        if (!hit || hit->target == Target::None) return request;
    }
    // The side panel's layer menu: any press closes it, the header toggles it.
    bool const kindWasOpen = std::exchange(state.kindMenu, false);
    if (hit && hit->target == Target::PanelKind) {
        if (!right) { commitName(); state.kindMenu = !kindWasOpen; }
        return request;
    }
    if (hit && hit->target == Target::PanelKindItem) {
        if (!right && hit->item != shownList()) {
            setShownList(hit->item);
            if (state.selected && listKind(state.selected->layer) != hit->item) select(std::nullopt, false);
        }
        return request;
    }
    if (kindWasOpen) return request;
    if (hit && hit->target == Target::Menu && state.menu) {
        if (right) return request;
        return chooseMenu(hit->item);
    }
    bool menuWasOpen = state.menu.has_value();
    if (!(hit && hit->target == Target::None)) state.menu.reset();
    if (hit && hit->target >= Target::Panel) {
        if (right) return request;
        return panelAction(*hit);
    }
    commitName();
    bool onMap = y > state.top && y < state.view.height - bottomHeight && !hit;
    if (right) {
        if (onMap) openMenu(x, y);
        return request;
    }
    if (hit) {
        switch (hit->target) {
        case Target::Close: request.kind = Request::Kind::Close; return request;
        case Target::Waypoints: setPanelOpen(!panelOpen()); return request;
        case Target::LayerWaypoints:
        case Target::LayerSchematics:
        case Target::LayerShapes:
        case Target::LayerSlime: toggleLayer(hit->target); return request;
        default: act(hit->target); return request;
        }
    }
    if (onMap && !menuWasOpen) state.drag = State::Drag{x, y, state.view.centerX, state.view.centerZ, false, markerAt(x, y)};
    return request;
}
void release() {
    // A click on a marker (no drag) selects it in the side panel.
    if (state.drag && !state.drag->moved && state.drag->marker) {
        setPanelOpen(true);
        auto mark = *state.drag->marker;
        if (mark.layer == MarkLayer::Placement) selectPlacement(mark);
        select(mark, false);
    }
    state.drag.reset();
}
void wheel(int direction) {
    state.menu.reset();
    auto p = state.pointer;
    if (state.panelWidth > 0 && p.x >= state.view.width - state.panelWidth) {
        if (p.y >= state.listTop && p.y < state.listBottom) state.listFirst = std::max(0, state.listFirst - direction * 3);
        return;
    }
    // Wheel events carry no position (it reads as the top-left corner), so
    // zoom about the pointer as last drawn.
    state.view.zoomAt(p.x, p.y, direction);
}
bool editingName() { return state.open && (state.editingName || state.editingAxis >= 0 || state.editingHeight); }
ui::TextEdit* nameField() {
    if (!editingName()) return nullptr;
    if (state.editingHeight) return &state.heightInput;
    return state.editingAxis >= 0 ? static_cast<ui::TextEdit*>(&state.coordinate) : &state.name;
}
void typeText(std::string const& text) { if (auto* field = nameField()) field->type(text); }
void backspace() { if (auto* field = nameField()) field->backspace(); }
void selectAllName() { if (auto* field = nameField()) field->selectAll(); }
glm::vec2 namePosition() { return state.namePosition; }
Request key(int key, bool openKey) {
    Request request;
    if (state.editingHeight) {
        if (key == 0x0d || key == 0x09) commitName();
        else if (key == 0x1b) state.editingHeight = false;
        return request;
    }
    if (state.editingName || state.editingAxis >= 0) {
        if (key == 0x0d || key == 0x09) commitName();
        else if (key == 0x1b) { state.editingName = false; state.editingAxis = -1; }
        return request;
    }
    if (key == 0x1b) {
        if (state.cavePanel) state.cavePanel = false;
        else if (state.menu) state.menu.reset();
        else request.kind = Request::Kind::Close;
    } else if (key == 0x20) {
        returnToPlayer();
    } else if (openKey) {
        request.kind = Request::Kind::Close;
    }
    return request;
}
void render(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer, Settings::Map const& settings) {
    auto& client = context.mClient;
    if (!state.open) open(client);
    state.client = &client;
    // Recording goes on while the map is open: the HUD that drives it is
    // hidden. With the feature off, only the saved map is shown.
    map::record(client, settings, settings.worldMap);
    state.pointer = pointer;
    if (state.cameraReference && !cameraAvailable()) state.cameraReference = false;
    // Follow the same stable reference the height controls use. Using the raw
    // eye height here made a + that landed on the stable band snap back.
    if (state.following && state.cave)
        if (auto reference = referenceHeight()) state.height = *reference;
    auto& view = state.view;
    view.width = size.x;
    view.height = size.y;
    auto layer = shownLayer();
    if (auto explored = store::exploredBounds(layer))
        view.farthest = farthestScaleFor(explored->x1 - explored->x0, explored->z1 - explored->z0,
                                         size.x - state.panelWidth, size.y - state.top - bottomHeight);
    else view.farthest = alwaysFarthestScale;
    if (state.drag) {
        double dx = pointer.x - state.drag->x, dy = pointer.y - state.drag->y;
        if (std::abs(dx) + std::abs(dy) > 2) state.drag->moved = true;
        if (state.drag->moved) {
            view.centerX = state.drag->centerX;
            view.centerZ = state.drag->centerZ;
            view.pan(dx, dy);
        }
    }
    double inverse = client.getGuiData()->mInvGuiScale;
    double pixelsPerUnit = std::isfinite(inverse) && inverse > 0 ? 1 / inverse : 1;
    state.pixelsPerUnit = pixelsPerUnit;
    state.hits.clear();
    ui::fill(context, 0, 0, size.x, size.y, ground);
    int empty = 0, shown = 0;
    drawTiles(context, size, pixelsPerUnit, layer, empty, shown);
    size_t pending = store::pending();
    drawMarkers(context, settings);
    // Text is batched: flush each layer so the bars and the menu cover it.
    context.flushText(0, std::nullopt);
    if (!shown && !pending && empty) {
        auto text = ui::translated("worldMap.empty");
        float w = ui::textWidth(context, text) + 12;
        ui::fill(context, (size.x - w) / 2, size.y / 2 - 7, w, 14, ui::palette::panel, .8f);
        ui::label(context, (size.x - w) / 2, size.y / 2 - 4, w, text, ui::palette::dim, ui::Align::Center);
    }
    drawBars(context, size, pending, settings.worldMap);
    context.flushText(0, std::nullopt);
    drawPanel(context, size);
    context.flushText(0, std::nullopt);
    drawCavePanel(context, size);
    context.flushText(0, std::nullopt);
    drawKindMenu(context);
    context.flushText(0, std::nullopt);
    drawMenu(context, size);
    if (!state.notice.empty() && now() - state.noticeAt < 2.6) {
        float w = ui::textWidth(context, state.notice) + 12, y = size.y - bottomHeight - 16;
        ui::fill(context, (size.x - w) / 2, y, w, 12, ui::palette::panel, .85f);
        ui::label(context, (size.x - w) / 2, y + 2, w, state.notice, ui::palette::text, ui::Align::Center);
    }
    context.flushText(0, std::nullopt);
}
}

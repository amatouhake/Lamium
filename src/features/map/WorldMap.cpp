#include "features/map/WorldMap.h"
#include "features/map/MapImage.h"
#include "features/map/MapLighting.h"
#include "features/map/MapRadar.h"
#include "features/map/MapStore.h"
#include "features/map/RadarFaces.h"
#include "features/map/Minimap.h"
#include "features/map/WaypointSession.h"
#include "features/map/SchematicMarks.h"
#include "features/map/WorldMapView.h"
#include "features/map/SeedLink.h"
#include "features/map/Teleport.h"
#include "app/Desktop.h"
#include "ll/api/Versions.h"
#include "mc/world/level/LevelSeed64.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"
#include "features/information/InfoHud.h"
#include "app/Runtime.h"
#include "ui/Localization.h"
#include "ui/SearchQuery.h"
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
ResourceLocation const& slotLocation(int index) {
    static std::array<ResourceLocation*, slotCount + 1> all{};
    auto& location = all[static_cast<size_t>(index)];
    if (!location)
        location = new ResourceLocation(Core::PathView(index == slotCount ? std::string("lamium/worldmap/arrow")
                                                                         : std::format("lamium/worldmap/{}", index)),
                                        ResourceFileSystem::Raw);
    return *location;
}
struct Slot {
    std::optional<std::pair<MapLayer, TileKey>> tile;
    unsigned version = 0;
    bool uploaded = false;
    double used = 0;
};
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
    None, Overworld, Nether, End, BandDown, BandUp, Center, Waypoints, Close, Menu,
    // The side panel: its background, header buttons, list and editor.
    Panel, PanelAdd, PanelClose, Row, RowVisible, Name, Step, MoveHere, Swatch, Keep, Delete, OpenScreen
};
struct Hit {
    float x, y, w, h;
    Target target;
    int item = -1;
};
enum class MenuKind { Ground, Waypoint, Death };
struct Menu {
    MenuKind kind;
    float x, y;    // Screen, where it was opened.
    int index = -1; // Waypoint: into the set.
    int worldX = 0, worldY = 0, worldZ = 0;
    bool known = false; // Ground: the height came from the saved map.
    bool armed = false; // Delete pressed once.
};
struct Marker {
    float x, y;
    MenuKind kind;
    int index;
};
struct State {
    bool open = false;
    IClientInstance* client = nullptr;
    WorldView view;
    int dimension = 0, band = 4;
    struct Drag {
        float x, y;
        double centerX, centerZ;
        bool moved = false;
        std::optional<Marker> marker; // Pressed on a marker: a click selects it.
    };
    std::optional<Drag> drag;
    std::optional<Menu> menu;
    std::vector<Hit> hits;
    std::vector<Marker> markers;
    glm::vec2 pointer{};
    std::string notice;
    double noticeAt = 0, openedAt = 0;
    float top = rowHeight; // The top bar, one or two rows.
    double pixelsPerUnit = 1; // Screen pixels per GUI unit, as last drawn.
    float panelWidth = 0;  // The side panel as last drawn; 0 when closed.
    int selected = -2;     // Side panel: -2 none, -1 the death point, else into the set.
    int listFirst = 0;
    bool deleteArmed = false;
    bool editingName = false;
    ui::SearchQuery name;
    glm::vec2 namePosition{};
    float listTop = 0, listBottom = 0;
    float arrowAngle = NAN;
    bool arrowUploaded = false;
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
// The player's place seen from another dimension: the Nether is 1/8 scale.
std::optional<std::pair<double, double>> playerOn(int dimension) {
    auto spot = playerSpot();
    if (!spot) return std::nullopt;
    double k = spot->dimension == dimension ? 1 : spot->dimension == 0 && dimension == 1 ? 1 / 8. :
        spot->dimension == 1 && dimension == 0 ? 8. : 1;
    return std::pair{spot->x * k, spot->z * k};
}
void center() {
    if (auto at = playerOn(state.dimension)) {
        state.view.centerX = at->first;
        state.view.centerZ = at->second;
    }
}
bool netherAuto() { return Runtime::instance().preferences().map.worldMapNetherAuto; }
int shownBand() {
    auto spot = playerSpot();
    if (state.dimension == 1 && netherAuto() && spot && spot->dimension == 1)
        return std::clamp(floorDiv(blockFloor(spot->y), bandHeight), 0, netherBands - 1);
    return state.band;
}
MapLayer shownLayer() { return state.dimension == 1 ? MapLayer{1, shownBand()} : MapLayer{state.dimension, 0}; }
void setNetherAuto(bool on) {
    auto value = Runtime::instance().preferences();
    if (value.map.worldMapNetherAuto == on) return;
    value.map.worldMapNetherAuto = on;
    if (!Runtime::instance().save(value)) log("could not save the Nether layer setting");
}
void say(std::string text) {
    state.notice = std::move(text);
    state.noticeAt = now();
}
void showDimension(int dimension) {
    if (state.dimension == dimension) return;
    state.dimension = dimension;
    state.menu.reset();
    center();
}

void act(Target target) {
    switch (target) {
    case Target::Overworld: showDimension(0); break;
    case Target::Nether: showDimension(1); break;
    case Target::End: showDimension(2); break;
    case Target::BandDown:
    case Target::BandUp:
        state.band = std::clamp(shownBand() + (target == Target::BandUp ? 1 : -1), 0, netherBands - 1);
        setNetherAuto(false);
        break;
    case Target::Center:
        center();
        if (state.dimension == 1) setNetherAuto(true);
        break;
    default: break;
    }
}
bool panelOpen() { return Runtime::instance().preferences().map.worldMapPanel; }
void setPanelOpen(bool open) {
    auto value = Runtime::instance().preferences();
    if (value.map.worldMapPanel == open) return;
    value.map.worldMapPanel = open;
    if (!Runtime::instance().save(value)) log("could not save the side panel state");
}
void commitName() {
    if (!std::exchange(state.editingName, false)) return;
    auto text = state.name.value();
    int index = state.selected;
    if (text.find_first_not_of(' ') == std::string::npos || index < 0) return;
    if (!waypoints::change([&](WaypointSet& set) {
            if (index >= static_cast<int>(set.waypoints.size())) return false;
            set.waypoints[static_cast<size_t>(index)].name = text;
            return true;
        })) say(ui::translated("waypoint.saveError"));
}
// Brings a place to the middle of the map left of the side panel.
void focus(double x, double z) {
    state.view.centerX = x + state.panelWidth / 2 / state.view.scale();
    state.view.centerZ = z;
}
void select(int index, bool move) {
    commitName();
    state.selected = index;
    state.deleteArmed = false;
    if (!move) return;
    auto set = waypoints::current();
    if (index == -1 && set.death) focus(set.death->x + .5, set.death->z + .5);
    else if (index >= 0 && index < static_cast<int>(set.waypoints.size())) {
        auto const& w = set.waypoints[static_cast<size_t>(index)];
        if (auto at = shownPosition(w.x, w.y, w.z, w.dimension, state.dimension, true)) focus(at->x, at->z);
    }
}
bool changeSelected(std::function<void(Waypoint&)> const& apply) {
    int index = state.selected;
    bool saved = waypoints::change([&](WaypointSet& set) {
        if (index < 0 || index >= static_cast<int>(set.waypoints.size())) return false;
        apply(set.waypoints[static_cast<size_t>(index)]);
        return true;
    });
    if (!saved) say(ui::translated("waypoint.saveError"));
    return saved;
}
Request panelAction(Hit const& hit) {
    Request request;
    if (hit.target != Target::Name) commitName();
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
        if (waypoints::add(w)) {
            showDimension(w.dimension);
            select(static_cast<int>(set.waypoints.size()), false);
        } else say(ui::translated("waypoint.saveError"));
        break;
    }
    case Target::Row: select(hit.item, true); break;
    case Target::RowVisible: {
        int keep = state.selected;
        state.selected = hit.item;
        changeSelected([](Waypoint& w) { w.visible = !w.visible; });
        state.selected = keep;
        break;
    }
    case Target::Name:
        if (state.selected >= 0 && state.selected < static_cast<int>(set.waypoints.size()) && !state.editingName) {
            state.editingName = true;
            state.name.clear();
            state.name.append(set.waypoints[static_cast<size_t>(state.selected)].name);
            state.name.selectAll();
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
    case Target::Swatch: changeSelected([&](Waypoint& w) { w.color = clampColor(hit.item); }); break;
    case Target::Keep:
        if (set.death) {
            auto death = *set.death;
            Waypoint w{ui::translated("waypoint.deathName"), nextColor(set.lastColor), death.x, death.y, death.z,
                       death.dimension, true};
            if (waypoints::change([&](WaypointSet& s) {
                    s.waypoints.push_back(w);
                    s.lastColor = w.color;
                    s.death.reset();
                    return true;
                })) select(static_cast<int>(set.waypoints.size()), false);
            else say(ui::translated("waypoint.saveError"));
        }
        break;
    case Target::Delete: {
        if (!state.deleteArmed) { state.deleteArmed = true; break; }
        state.deleteArmed = false;
        int index = state.selected;
        bool saved = waypoints::change([&](WaypointSet& s) {
            if (index == -1) { s.death.reset(); return true; }
            if (index < 0 || index >= static_cast<int>(s.waypoints.size())) return false;
            s.waypoints.erase(s.waypoints.begin() + index);
            return true;
        });
        if (!saved) say(ui::translated("waypoint.saveError"));
        else state.selected = -2;
        break;
    }
    case Target::OpenScreen:
        request.kind = Request::Kind::OpenWaypoints;
        request.index = state.selected;
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
    if (!player) return std::nullopt;
    int dimension = state.dimension, x = menu.worldX, y = menu.worldY, z = menu.worldZ;
    if (menu.kind == MenuKind::Death) {
        if (!set.death) return std::nullopt;
        dimension = set.death->dimension; x = set.death->x; y = set.death->y; z = set.death->z;
    } else if (menu.kind == MenuKind::Waypoint) {
        if (menu.index < 0 || menu.index >= static_cast<int>(set.waypoints.size())) return std::nullopt;
        auto const& w = set.waypoints[static_cast<size_t>(menu.index)];
        dimension = w.dimension; x = w.x; y = w.y; z = w.z;
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
    } else {
        bool visible = menu.index >= 0 && menu.index < static_cast<int>(set.waypoints.size())
            && set.waypoints[static_cast<size_t>(menu.index)].visible;
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
    if (menu.kind == MenuKind::Death) {
        if (item == 0 && set.death) {
            state.menu.reset();
            auto death = *set.death;
            Waypoint w{ui::translated("waypoint.deathName"), nextColor(set.lastColor), death.x, death.y, death.z,
                       death.dimension, true};
            bool saved = waypoints::change([&](WaypointSet& s) {
                s.waypoints.push_back(w);
                s.lastColor = w.color;
                s.death.reset();
                return true;
            });
            say(ui::translated(saved ? "waypoint.added" : "waypoint.saveError", w.name));
        } else if (item == 1) {
            if (!menu.armed) { state.menu->armed = true; return request; }
            state.menu.reset();
            if (!waypoints::change([](WaypointSet& s) { s.death.reset(); return true; }))
                say(ui::translated("waypoint.saveError"));
        }
        return request;
    }
    int index = menu.index;
    if (index < 0 || index >= static_cast<int>(set.waypoints.size())) { state.menu.reset(); return request; }
    auto name = set.waypoints[static_cast<size_t>(index)].name;
    if (item == 0) {
        state.menu.reset();
        setPanelOpen(true);
        select(index, false);
    } else if (item == 1) {
        state.menu.reset();
        if (!waypoints::change([&](WaypointSet& s) {
                if (index >= static_cast<int>(s.waypoints.size())) return false;
                auto& w = s.waypoints[static_cast<size_t>(index)];
                w.visible = !w.visible;
                return true;
            })) say(ui::translated("waypoint.saveError"));
    } else if (item == 2) {
        if (!menu.armed) { state.menu->armed = true; return request; }
        state.menu.reset();
        bool saved = waypoints::change([&](WaypointSet& s) {
            if (index >= static_cast<int>(s.waypoints.size())) return false;
            s.waypoints.erase(s.waypoints.begin() + index);
            return true;
        });
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
std::optional<Marker> markerAt(float x, float y) {
    std::optional<Marker> best;
    float bestDistance = 6;
    for (auto const& m : state.markers) {
        float d = std::hypot(m.x - x, m.y - y);
        if (d < bestDistance) { bestDistance = d; best = m; }
    }
    return best;
}
void openMenu(float x, float y) {
    if (auto marker = markerAt(x, y)) {
        state.menu = Menu{marker->kind, x, y, marker->index};
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
void diamond(MinecraftUIRenderContext& context, float cx, float cy, int size, Rgb color, float opacity) {
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
               int& shown, Rgb mapTint) {
    auto& client = context.mClient;
    auto& view = state.view;
    int lod = lodFor(view.scale(), pixelsPerUnit);
    auto tiles = visibleTiles(view, lod);
    store::want(layer, tiles);
    double time = now();
    int budget = time - state.openedAt < 1 ? uploadsWhileOpening : uploadsPerFrame;
    std::vector<bool> usedNow(slotCount, false);
    auto findSlot = [&](TileKey tile) -> int {
        for (int i = 0; i < slotCount; ++i)
            if (slots[static_cast<size_t>(i)].tile && slots[static_cast<size_t>(i)].tile->first == layer
                && slots[static_cast<size_t>(i)].tile->second == tile) return i;
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
        if (!ui::runtimeImage(context, slotLocation(index), rect, u, v, span, span, 1.f, mapTint)) slot.uploaded = false;
    };
    for (auto tile : tiles) {
        auto image = store::image(layer, tile);
        if (image && !image->pixels) { ++empty; continue; }
        int index = findSlot(tile);
        bool current = index >= 0 && slots[static_cast<size_t>(index)].uploaded
            && slots[static_cast<size_t>(index)].version == image->version;
        if (image && !current && budget > 0) {
            if (index < 0) {
                double oldest = INFINITY;
                for (int i = 0; i < slotCount; ++i) {
                    auto const& s = slots[static_cast<size_t>(i)];
                    if (usedNow[static_cast<size_t>(i)]) continue;
                    double age = s.tile ? s.used : -INFINITY;
                    if (age < oldest) { oldest = age; index = i; }
                }
            }
            if (index >= 0) {
                auto& slot = slots[static_cast<size_t>(index)];
                --budget;
                try {
                    if (upload(client, slotLocation(index), *image->pixels, regionBlocks, slot.uploaded)) {
                        slot.uploaded = true;
                        slot.tile = std::pair{layer, tile};
                        slot.version = image->version;
                    } else slot = Slot{};
                } catch (...) { slot = Slot{}; }
            }
        }
        if (index >= 0 && slots[static_cast<size_t>(index)].uploaded && slots[static_cast<size_t>(index)].tile
            && slots[static_cast<size_t>(index)].tile->second == tile) {
            drawSlot(index, rectOf(tile), 0, 0, 1);
            ++shown;
            continue;
        }
        // Not ready: a coarser image already on the GPU stands in.
        for (int up = 1; up <= 2; ++up) {
            TileKey parent{tile.lod + up, tile.x >> up, tile.z >> up};
            int at = findSlot(parent);
            if (at < 0 || !slots[static_cast<size_t>(at)].uploaded) continue;
            float span = 1.f / static_cast<float>(1 << up);
            drawSlot(at, rectOf(tile), (tile.x - (parent.x << up)) * span, (tile.z - (parent.z << up)) * span, span);
            break;
        }
    }
    (void)size;
}

// Schematic placements (L-93): footprint, faint fill and name, under the
// other markers. Hidden ones stay faint, like hidden waypoints.
void drawPlacements(MinecraftUIRenderContext& context) {
    auto& view = state.view;
    auto color = rgb(placementColor);
    for (auto const& mark : placementMarks(state.dimension)) {
        auto const& a = mark.area;
        float x0 = std::round(static_cast<float>(view.screenX(a.x0))), y0 = std::round(static_cast<float>(view.screenY(a.z0)));
        float x1 = std::round(static_cast<float>(view.screenX(a.x1))), y1 = std::round(static_cast<float>(view.screenY(a.z1)));
        // Never smaller than a few units, so a far-out view still shows it.
        float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
        float w = std::max(x1 - x0, 4.f), h = std::max(y1 - y0, 4.f);
        x0 = cx - w / 2; y0 = cy - h / 2;
        if (x0 + w < -20 || y0 + h < -20 || x0 > view.width + 20 || y0 > view.height + 20) continue;
        float opacity = mark.visible ? 1.f : .35f;
        ui::fill(context, x0, y0, w, h, color, .16f * opacity);
        ui::frame(context, x0 - 1, y0 - 1, w + 2, h + 2, Rgb{0, 0, 0}, .6f * opacity);
        ui::frame(context, x0, y0, w, h, mark.selected ? ui::palette::white : color, opacity);
        auto name = mark.visible ? mark.name : mark.name + " " + ui::translated("worldMap.hidden");
        smallLabel(context, cx, y0 + h + 2, name, mark.visible ? ui::palette::text : ui::palette::faint);
    }
}
void drawMarkers(MinecraftUIRenderContext& context, Settings::Map const& settings) {
    auto& view = state.view;
    state.markers.clear();
    drawPlacements(context);
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
    if (settings.waypoints) {
        auto set = waypoints::current();
        for (size_t i = 0; i < set.waypoints.size(); ++i) {
            auto const& w = set.waypoints[i];
            auto at = shownPosition(w.x, w.y, w.z, w.dimension, state.dimension, settings.waypointsCrossScale);
            if (!at) continue;
            float x = std::round(static_cast<float>(view.screenX(at->x))), y = std::round(static_cast<float>(view.screenY(at->z)));
            if (x < -20 || y < -20 || x > view.width + 20 || y > view.height + 20) continue;
            // Hidden ones stay faint here so they can be shown again.
            float opacity = w.visible ? 1.f : .35f;
            if (state.panelWidth > 0 && state.selected == static_cast<int>(i)) diamond(context, x, y, 13, ui::palette::white, 1);
            diamond(context, x, y, 9, rgb(waypointColors[static_cast<size_t>(clampColor(w.color))]), opacity);
            auto name = w.visible ? w.name : w.name + " " + ui::translated("worldMap.hidden");
            smallLabel(context, x, y + 6, name, w.visible ? ui::palette::text : ui::palette::faint);
            state.markers.push_back({x, y, MenuKind::Waypoint, static_cast<int>(i)});
        }
        if (set.death && set.death->dimension == state.dimension) {
            float x = std::round(static_cast<float>(view.screenX(set.death->x + .5)));
            float y = std::round(static_cast<float>(view.screenY(set.death->z + .5)));
            if (state.panelWidth > 0 && state.selected == -1) ui::frame(context, x - 5, y - 5, 10, 10, ui::palette::white);
            cross(context, x, y);
            if (hovering(x - 5, y - 5, 10, 10)) smallLabel(context, x, y + 5, ui::translated("waypoint.death"), ui::palette::text);
            state.markers.push_back({x, y, MenuKind::Death, -1});
        }
    }
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
}

void drawBars(MinecraftUIRenderContext& context, glm::vec2 size, MapLayer layer, size_t pending, bool recording) {
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
    auto centerText = ui::translated("worldMap.center"), waypointsText = ui::translated("nav.waypoints");
    float closeW = 13, rightW = widthOf(centerText) + 3 + widthOf(waypointsText) + 3 + closeW + 3;
    auto layerText = ui::translated("worldMap.layer", layer.band * bandHeight, layer.band * bandHeight + bandHeight - 1);
    float layerW = state.dimension == 1 ? 10 + ui::textWidth(context, layerText) + 10 + 10 + 8 : 0;
    float brandW = ui::textWidth(context, "Lamium") + 6;
    bool oneRow = 4 + brandW + dimsW + layerW + rightW + 4 <= size.x;
    state.top = oneRow || state.dimension != 1 ? rowHeight : 2 * rowHeight;
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
    if (state.dimension == 1) {
        if (!oneRow) { x = 4; y = rowHeight + 2; }
        float w = 10;
        ui::fill(context, x, y, w, buttonHeight, ui::palette::keyFill);
        ui::frame(context, x, y, w, buttonHeight, ui::palette::keyEdge);
        ui::arrow(context, x + 3, y + 2.5f, true, hovering(x, y, w, buttonHeight) ? ui::palette::text : ui::palette::dim);
        state.hits.push_back({x, y, w, buttonHeight, Target::BandDown});
        x += w;
        float tw = ui::textWidth(context, layerText) + 10;
        ui::frame(context, x - 1, y, tw + 2, buttonHeight, ui::palette::keyEdge);
        // Accent while the layer follows the player's height.
        bool following = netherAuto() && spot && spot->dimension == 1;
        ui::label(context, x, y + inset, tw, layerText, following ? ui::palette::accent : ui::palette::text, ui::Align::Center);
        x += tw;
        ui::fill(context, x, y, w, buttonHeight, ui::palette::keyFill);
        ui::frame(context, x, y, w, buttonHeight, ui::palette::keyEdge);
        ui::arrow(context, x + 3, y + 2.5f, false, hovering(x, y, w, buttonHeight) ? ui::palette::text : ui::palette::dim);
        state.hits.push_back({x, y, w, buttonHeight, Target::BandUp});
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

    // Bottom bar: the place under the cursor on the left; the scale bar on
    // the right with the hints before it, dropped when they would meet.
    float top = size.y - bottomHeight;
    ui::fill(context, 0, top, size.x, bottomHeight, ui::palette::panel, .85f);
    ui::fill(context, 0, top, size.x, 1, ui::palette::white, .14f);
    float textY = top + 1 + inset;
    std::string where;
    if (state.pointer.y > state.top && state.pointer.y < top) {
        int wx = blockFloor(state.view.worldX(state.pointer.x)), wz = blockFloor(state.view.worldZ(state.pointer.y));
        auto column = store::column(layer, wx, wz);
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
    std::vector<int> entries;
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
    if (state.selected >= static_cast<int>(set.waypoints.size()) || (state.selected == -1 && !set.death)) state.selected = -2;

    auto title = ui::translated("nav.waypoints");
    ui::label(context, x, y + inset, inner - 30, title);
    ui::label(context, x + ui::textWidth(context, title) + 4, y + inset, 20, std::to_string(listed.size()), ui::palette::faint);
    float bx = x0 + w - pad - 11;
    button(context, bx, y, ">", Target::PanelClose, false, hovering(bx, y, 11, buttonHeight));
    bx -= 3 + ui::textWidth(context, "+") + 8;
    button(context, bx, y, "+", Target::PanelAdd, true, false);
    y += buttonHeight + 3;
    ui::fill(context, x0 + 1, y, w - 1, 1, ui::palette::white, .14f);
    y += 1;

    int maxRows = std::max(3, static_cast<int>((bottom - y) * .4f / rowH));
    state.listFirst = std::clamp(state.listFirst, 0, std::max(0, static_cast<int>(entries.size()) - maxRows));
    state.listTop = y;
    state.listBottom = y + maxRows * rowH;
    if (entries.empty()) ui::label(context, x, y + 2, inner, ui::translated("worldMap.noWaypoints"), ui::palette::faint);
    for (int r = 0; r < maxRows && state.listFirst + r < static_cast<int>(entries.size()); ++r) {
        int index = entries[static_cast<size_t>(state.listFirst + r)];
        float ry = y + r * rowH;
        bool selected = index == state.selected, over = hovering(x0, ry, w, rowH);
        if (selected) {
            ui::fill(context, x0 + 1, ry, w - 1, rowH, ui::palette::accent, .16f);
            ui::fill(context, x0 + 1, ry, 2, rowH, ui::palette::accent);
        } else if (over) ui::fill(context, x0 + 1, ry, w - 1, rowH, ui::palette::white, .07f);
        state.hits.push_back({x0, ry, w, rowH, Target::Row, index});
        if (index == -1) {
            cross(context, x + 4, ry + rowH / 2);
            ui::label(context, x + 11, ry + inset, inner - 11, ui::translated("waypoint.death"), ui::palette::text);
            continue;
        }
        auto const& p = set.waypoints[static_cast<size_t>(index)];
        diamond(context, x + 4, ry + rowH / 2, 7, rgb(waypointColors[static_cast<size_t>(clampColor(p.color))]), p.visible ? 1.f : .4f);
        float switchX = x0 + w - pad - ui::switchWidth;
        std::string far;
        if (spot && spot->dimension == state.dimension) far = ui::translated("waypoint.meters", static_cast<int>(std::lround(distance(index))));
        float farW = far.empty() ? 0 : ui::textWidthScaled(context, far, .75f);
        ui::label(context, x + 11, ry + inset, switchX - farW - 4 - x - 11, p.name, p.visible ? ui::palette::text : ui::palette::faint);
        if (!far.empty()) ui::labelScaled(context, switchX - 3 - farW, ry + 3, farW + 2, far, .75f, ui::palette::dim);
        ui::toggleSwitch(context, switchX, ry + 1, p.visible);
        state.hits.push_back({switchX - 1, ry, ui::switchWidth + 2, rowH, Target::RowVisible, index});
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
    if (state.selected == -1 && set.death) {
        auto const& d = *set.death;
        ui::labelScaled(context, x, y, inner, std::format("{}, {}, {}", d.x, d.y, d.z), .75f, ui::palette::dim);
        y += 10;
        float used = small(x, y, inner, ui::translated("waypoint.keep"), Target::Keep, 0);
        small(x + used + 3, y, inner - used - 3, deleteText, Target::Delete, 0, true);
        return;
    }
    if (state.selected < 0) {
        ui::labelScaled(context, x, y, inner, ui::translated("worldMap.selectHint"), .75f, ui::palette::faint);
        return;
    }
    auto const& p = set.waypoints[static_cast<size_t>(state.selected)];
    // Name: click to type; Enter or a click elsewhere keeps it, Esc drops it.
    ui::fill(context, x, y, inner, 12, Rgb{0, 0, 0}, .4f);
    ui::frame(context, x, y, inner, 12, state.editingName ? ui::palette::accent : ui::palette::keyEdge);
    std::string shown = !state.editingName ? p.name : state.name.selectedAll() ? "[" + state.name.value() + "]" : state.name.value() + "_";
    ui::label(context, x + 3, y + 1 + inset, inner - 6, std::move(shown));
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
        ui::label(context, sx + 9, y + inset, 52, std::to_string(value), ui::palette::text, ui::Align::Center);
        y += 12;
    }
    if (y + 12 < bottom) { small(x, y, inner, ui::translated("waypoint.moveHere"), Target::MoveHere, 0); y += 13; }
    if (y + 12 < bottom) {
        ui::label(context, x, y + inset, inner - 24, ui::translated("waypoint.shown"), ui::palette::dim);
        float sx = x0 + w - pad - ui::switchWidth;
        ui::toggleSwitch(context, sx, y + 1, p.visible);
        state.hits.push_back({sx - 1, y, ui::switchWidth + 2, rowH, Target::RowVisible, state.selected});
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
void drawMenu(MinecraftUIRenderContext& context, glm::vec2 size) {
    if (!state.menu) return;
    auto set = waypoints::current();
    auto items = menuItems(*state.menu, set);
    std::string head;
    auto const& m = *state.menu;
    if (m.kind == MenuKind::Ground)
        head = m.known ? std::format("{}, {}, {}", m.worldX, m.worldY, m.worldZ)
                       : std::format("{}, ?, {}  {}", m.worldX, m.worldZ, ui::translated("worldMap.unrecorded"));
    else if (m.kind == MenuKind::Death) {
        if (!set.death) { state.menu.reset(); return; }
        head = ui::translated("waypoint.death") + std::format("  {}, {}, {}", set.death->x, set.death->y, set.death->z);
    } else {
        if (m.index < 0 || m.index >= static_cast<int>(set.waypoints.size())) { state.menu.reset(); return; }
        auto const& w = set.waypoints[static_cast<size_t>(m.index)];
        head = w.name + std::format("  {}, {}, {}", w.x, w.y, w.z);
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
    state.openedAt = now();
    if (resume) return;
    state.selected = -2;
    state.deleteArmed = false;
    if (auto spot = playerSpot()) {
        state.dimension = spot->dimension;
        if (spot->dimension == 1) state.band = std::clamp(floorDiv(blockFloor(spot->y), bandHeight), 0, netherBands - 1);
    }
    center();
}
void close() {
    if (state.client) {
        for (int i = 0; i < slotCount; ++i)
            if (slots[static_cast<size_t>(i)].uploaded) unload(*state.client, slotLocation(i));
        if (state.arrowUploaded) unload(*state.client, slotLocation(slotCount));
    }
    slots = {};
    state.arrowUploaded = false;
    state.open = false;
    state.client = nullptr;
    state.drag.reset();
    state.menu.reset();
    commitName();
}
Request press(float x, float y, bool right) {
    Request request;
    auto const* hit = hitAt(x, y);
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
        select(state.drag->marker->kind == MenuKind::Death ? -1 : state.drag->marker->index, false);
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
bool editingName() { return state.open && state.editingName; }
void typeText(std::string const& text) { if (state.editingName) state.name.type(text); }
void backspace() { if (state.editingName) state.name.backspace(); }
void selectAllName() { if (state.editingName) state.name.selectAll(); }
glm::vec2 namePosition() { return state.namePosition; }
Request key(int key, bool openKey) {
    Request request;
    if (state.editingName) {
        if (key == 0x0d || key == 0x09) commitName();
        else if (key == 0x1b) state.editingName = false;
        return request;
    }
    if (key == 0x1b) {
        if (state.menu) state.menu.reset();
        else request.kind = Request::Kind::Close;
    } else if (key == 0x20) {
        center();
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
    auto& view = state.view;
    view.width = size.x;
    view.height = size.y;
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
    auto layer = shownLayer();
    state.hits.clear();
    ui::fill(context, 0, 0, size.x, size.y, ground);
    int empty = 0, shown = 0;
    int worldTime = -1;
    if (auto* player = client.getLocalPlayer()) {
        try {
            worldTime = player->getLevel().getTime();
        } catch (...) {}
    }
    bool isCave = (layer.dimension != 0);
    auto mapTint = daylightTint(worldTime, layer.dimension, isCave, settings.daylightTint);
    drawTiles(context, size, pixelsPerUnit, layer, empty, shown, mapTint);
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
    drawBars(context, size, layer, pending, settings.worldMap);
    context.flushText(0, std::nullopt);
    drawPanel(context, size);
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

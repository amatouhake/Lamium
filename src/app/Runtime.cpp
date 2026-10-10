#include "app/Runtime.h"
#ifdef LAMIUM_FEATURE_SKIP
#include <filesystem>
#include <fstream>
#include <set>
#endif
#include "features/interaction/PermanentSneak.h"
#include "features/research/ResearchTrace.h"
#include "features/schematic/GhostProbe.h"
#include "features/schematic/GhostRenderer.h"
#include "features/schematic/SchematicActions.h"
#include "features/map/PlayerLocationTrace.h"
#include "features/inspection/render/IconTrace.h"
#include "features/research/TradeTrace.h"
#include "features/interaction/InventoryMoveTrace.h"
#include "features/visuals/ConnectedTexturesHooks.h"
#include "features/inspection/LockedTrades.h"
#include "features/inspection/EnglishSearch.h"
#include "features/interaction/EdgeGuard.h"
#include "features/interaction/ToolGuard.h"
#include "features/interaction/MiningSessionHooks.h"
#include "features/inventory/DeathRestore.h"
#include "features/interaction/ElytraSwap.h"
#include "features/interaction/PeriodicInput.h"
#include "features/interaction/AutomationTrace.h"
#include "features/camera/CameraSessions.h"
#include "features/camera/CameraTrace.h"
#include "features/lighting/NightVision.h"
#include "features/inspection/Inspection.h"
#include "features/inventory/Inventory.h"
#include "features/inventory/game/ConsumptionTrace.h"
#include "features/inventory/game/LegacyFlowTrace.h"
#include "features/inventory/ToolSwitch.h"
#include "features/inventory/WeaponSwitch.h"
#include "features/inventory/FakeOffhand.h"
#include "features/inventory/FakeOffhandTrace.h"
#include "features/inventory/OffhandUseTrace.h"
#include "features/information/FrameTiming.h"
#include "features/information/TargetInfo.h"
#include "features/map/Minimap.h"
#include "features/map/WaypointSession.h"
#include "features/map/MapStore.h"
#include "features/map/WaypointMarkers.h"
#include "features/interaction/BreakingRestriction.h"
#include "features/interaction/PlacementTrace.h"
#include "features/visuals/HideOffhand.h"
#include "features/visuals/HideEffects.h"
#include "features/visuals/EffectTrace.h"
#include "input/CustomInput.h"
#include "overlay/WorldOverlay.h"
#include "ui/SettingsScreen.h"
#include "settings/SettingsStore.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/io/FileSink.h"
#include "ll/api/io/PatternFormatter.h"
#include "ll/api/io/RotatePolicy.h"

namespace lamium {
Runtime& Runtime::instance() { static Runtime value; return value; }
bool Runtime::load() {
    try {
        ll::io::RotatePolicy rotation;
        rotation.maxFileSize = 4 * 1024 * 1024;
        rotation.maxFiles = 7;
        rotation.totalSizeCap = 32 * 1024 * 1024;
        rotation.compress = false;
        auto sink = std::make_shared<ll::io::FileSink>(
            mod.getModDir() / "logs" / "lamium.log",
            ll::makePolymorphic<ll::io::PatternFormatter>("[{3:.3%F %T.} {2}][{1}] {0}", false), rotation);
        sink->setFlushLevel(ll::io::LogLevel::Info);
        mod.getLogger().addSink(std::move(sink));
    } catch (std::exception const& error) {
        mod.getLogger().warn("Could not create Lamium log: {}", error.what());
    }
    auto path = mod.getConfigDir() / "settings.json";
    try {
        if (std::filesystem::exists(path)) settings = readSettings(path);
        else writeSettings(path, settings);
    } catch (std::exception const& error) {
        mod.getLogger().error("Settings load failed: {}", error.what());
        settings = {};
    }
    settings.normalize();
    published.store(std::make_shared<Settings const>(settings));
    // These capture the client's button handlers as they are registered, which
    // happens between load and enable; started later they never see them.
    try { interaction::periodic::start(); }
    catch (std::exception const& error) {
        mod.getLogger().warn("Periodic input unavailable: {}", error.what());
    }
    try { interaction::automationTrace::start(); }
    catch (std::exception const& error) {
        mod.getLogger().warn("Automation diagnostics unavailable: {}", error.what());
    }
    CameraSessions::instance().configure(settings);
    NightVision::instance().configure(settings.lighting.nightVision, settings.lighting.nightVisionEven);
    inventory::fakeOffhand::configure(settings);
    visuals::effects::configure(settings);
    return true;
}
namespace {
// Every client feature started by enable(), in start order; they stop in
// reverse. A feature that fails rolls back itself and the ones before it.
struct Feature {
    char const* name;
    bool (*start)();
    void (*stop)();
};
template <auto Start>
bool started() { Start(); return true; }
Feature const features[] = {
    // Installed before Camera, in the order the shared hook list used.
    {"Camera diagnostics", started<camera::trace::start>, camera::trace::stop},
    {"Camera", [] { return CameraSessions::instance().start(); }, [] { CameraSessions::instance().stop(); }},
    {"Lighting", [] { return NightVision::instance().start(); }, [] { NightVision::instance().stop(); }},
    {"Inspection", inspection::start, inspection::stop},
    {"Locked trades", inspection::lockedTrades::start, inspection::lockedTrades::stop},
    {"English search", inspection::englishSearch::start, inspection::englishSearch::stop},
    {"Inventory", inventory::start, inventory::stop},
    {"Settings screen", started<ui::start>, ui::stop},
    {"Custom input", started<input::startCustomInput>, input::stopCustomInput},
    {"World overlay", started<overlay::start>, overlay::stop},
    {"Offhand visibility", started<visuals::start>, visuals::stop},
    {"Connected Textures", visuals::connected::start, visuals::connected::stop},
    {"Tool Switch", started<inventory::tools::start>, inventory::tools::stop},
    {"Weapon Switch", started<inventory::weapons::start>, inventory::weapons::stop},
    {"Death layout", started<inventory::death::start>, inventory::death::stop},
    {"Fake offhand", started<inventory::fakeOffhand::start>, inventory::fakeOffhand::stop},
    {"Fake offhand diagnostics", started<inventory::fakeOffhand::startTrace>, inventory::fakeOffhand::stopTrace},
    {"Offhand use diagnostics", started<inventory::offhandUseTrace::start>, inventory::offhandUseTrace::stop},
    {"Frame timing", started<information::startFrameTiming>, information::stopFrameTiming},
    {"Target icons", started<information::startTargetIcons>, information::stopTargetIcons},
    {"Minimap", started<map::start>, map::stop},
    {"Waypoints", started<map::waypoints::start>, map::waypoints::stop},
    {"World map store", started<map::store::start>, map::store::stop},
    {"Waypoint markers", started<map::markers::start>, map::markers::stop},
    {"Schematic ghosts", started<schematic::ghosts::start>, schematic::ghosts::stop},
    {"Schematic adjust key", started<schematic::actions::startAdjust>, schematic::actions::stopAdjust},
    {"Breaking Restriction", started<interaction::breaking::start>, interaction::breaking::stop},
    {"Edge guard", started<interaction::edgeGuard::start>, interaction::edgeGuard::stop},
    {"Tool Protection", started<interaction::toolGuard::start>, interaction::toolGuard::stop},
    {"Mining session", started<interaction::mining::start>, interaction::mining::stop},
    {"Auto Elytra", started<interaction::elytraSwap::start>, interaction::elytraSwap::stop},
    {"Placement diagnostics", started<interaction::placementTrace::start>, interaction::placementTrace::stop},
    {"Research diagnostics", started<researchTrace::start>, researchTrace::stop},
    {"Ghost probe", started<schematic::ghostProbe::start>, schematic::ghostProbe::stop},
    {"Player location diagnostics", started<map::locationTrace::start>, map::locationTrace::stop},
    {"Icon diagnostics", started<inspection::iconTrace::start>, inspection::iconTrace::stop},
    {"Trade diagnostics", started<researchTrace::trade::start>, researchTrace::trade::stop},
    {"Inventory move diagnostics", started<interaction::inventoryMoveTrace::start>, interaction::inventoryMoveTrace::stop},
    {"Legacy flow diagnostics", started<inventory::game::legacyFlowTrace::start>, inventory::game::legacyFlowTrace::stop},
    {"Consumption diagnostics", started<inventory::game::consumptionTrace::start>, inventory::game::consumptionTrace::stop},
    {"Sneak", started<interaction::sneak::start>, interaction::sneak::stop},
    {"Effect visibility", started<visuals::effects::start>, visuals::effects::stop},
    {"Effect diagnostics", started<visuals::effectTrace::start>, visuals::effectTrace::stop},
};
}
#ifdef LAMIUM_FEATURE_SKIP
// Diagnostics (Vibrant Visuals regression, 2026-10-11): features named one per
// line in mods/Lamium/skip-features.txt are not started, to narrow down which
// one changes the game without a rebuild. Never in a normal build.
std::set<std::string> skippedFeatures(std::filesystem::path const& file) {
    std::set<std::string> names;
    std::ifstream in(file);
    for (std::string line; std::getline(in, line);) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (!line.empty() && line.front() != '#') names.insert(line);
    }
    return names;
}
#endif
bool Runtime::enable() {
    if (running) return true;
#ifdef LAMIUM_FEATURE_SKIP
    auto skipped = skippedFeatures(mod.getModDir() / "skip-features.txt");
    std::string all;
    for (auto const& feature : features) all += std::string("\n  ") + feature.name;
    mod.getLogger().warn("Feature skip diagnostics: {} skipped; feature names:{}", skipped.size(), all);
#endif
    for (auto feature = std::begin(features); feature != std::end(features); ++feature) {
#ifdef LAMIUM_FEATURE_SKIP
        if (skipped.contains(feature->name)) {
            mod.getLogger().warn("Skipped feature: {}", feature->name);
            continue;
        }
#endif
        bool ok = false;
        std::string reason = "failed";
        try { ok = feature->start(); }
        catch (std::exception const& error) { reason = error.what(); }
        if (ok) continue;
        mod.getLogger().error("{} could not start: {}", feature->name, reason);
        // The failed feature may have installed part of itself; stop it too.
        for (;; --feature) {
            feature->stop();
            if (feature == std::begin(features)) break;
        }
        return false;
    }
    running = true;
    mod.getLogger().info("Lamium enabled. Configure features and bindings in Lamium Settings (default: L), using Features or Hotkeys.");
    return true;
}
bool Runtime::disable() {
    running = false;
    for (auto feature = std::rbegin(features); feature != std::rend(features); ++feature) feature->stop();
    // Started in load(); Fake offhand above still releases through periodic input.
    interaction::automationTrace::stop();
    interaction::periodic::stop();
    return true;
}
bool Runtime::save(Settings value) {
    std::lock_guard lock(settingsMutex);
    value.normalize();
    try {
        writeSettings(mod.getConfigDir() / "settings.json", value);
        bool cameraChanged = !(settings.camera == value.camera);
        if (settings.interaction.breaking != value.interaction.breaking
            || settings.interaction.breakingMode != value.interaction.breakingMode) interaction::breaking::reset();
        settings = value;
        published.store(std::make_shared<Settings const>(settings));
        if (cameraChanged) CameraSessions::instance().configure(settings);
        NightVision::instance().configure(settings.lighting.nightVision, settings.lighting.nightVisionEven);
        inventory::fakeOffhand::configure(settings);
        visuals::effects::configure(settings);
        return true;
    } catch (std::exception const& error) {
        mod.getLogger().error("Settings save failed: {}", error.what());
        return false;
    }
}
}
LL_REGISTER_MOD(lamium::Runtime, lamium::Runtime::instance());

#include "app/PausedFeatures.h"
#include "settings/SettingsStore.h"
#include "settings/Options.h"
#include "features/inventory/FakeOffhandPlan.h"
#include "features/visuals/EffectVisibility.h"
#include "input/ToggleAction.h"
#include "ui/Translations.h"
#include <unordered_set>
#include <chrono>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif
void check(bool, char const*);
void settingsStoreTests() {
    using namespace lamium;
    check(decodeSettings("{}").information.targetSchematic
          && !decodeSettings(R"({"information":{"targetSchematic":false}})").information.targetSchematic,
          "schematic information on the target card is on unless turned off");
    check(!decodeSettings(R"({"information":{"targetSchematic":0}})").information.targetSchematic
          && decodeSettings(R"({"information":{"targetSchematic":2}})").information.targetSchematic,
          "a display mode number from a test build reads as on or off");
    {
        check(decodeSettings("{}").visuals.hideNausea
              && !decodeSettings(R"({"visuals":{"hideNausea":false}})").visuals.hideNausea,
              "nausea color is selected by default and loads independently");
        Settings value;
        value.visuals.hideNausea = false;
        check(input::defaultChord(input::Action::HideNausea).empty()
              && input::toggleAction(value,input::Action::HideNausea) && value.visuals.hideNausea
              && !value.visuals.hideEffects && value.visuals.hideBossBars && value.visuals.hideParticles,
              "the unbound nausea key changes only its selection while the master is off");
        using visuals::hideNauseaMesh;
        check(hideNauseaMesh(visuals::effectMask(true,false,false,false,true),true,
                  "ui_texture_and_color_blur_additive","textures/misc/nausea")
              && !hideNauseaMesh(visuals::effectMask(false,true,true,true,true),true,
                  "ui_texture_and_color_blur_additive","textures/misc/nausea")
              && !hideNauseaMesh(visuals::effectMask(true,true,true,true,false),true,
                  "ui_texture_and_color_blur_additive","textures/misc/nausea"),
              "only the nausea child and active master hide its observed draw route");
        check(!hideNauseaMesh(15,false,"ui_texture_and_color_blur_additive","textures/misc/nausea")
              && !hideNauseaMesh(15,true,"ui_textured_and_glcolor","textures/ui/nausea_effect")
              && !hideNauseaMesh(15,true,"on_screen_effect","textures/ui/frozen_effect")
              && !hideNauseaMesh(15,true,"ui_texture_and_color_blur_additive","textures/misc/nausea_extra")
              && !hideNauseaMesh(15,true,"other","textures/misc/nausea"),
              "other owners, status icons, frozen effects and partial route matches stay visible");
    }
    {
        auto none = decodeSettings("{}").visuals;
        check(none.hideWater && none.hideLava && none.hidePowderSnow && !none.hideEffects,
              "immersion effects are selected by default under a master that starts off");
        auto loaded = decodeSettings(R"({"visuals":{"hidePumpkin":true,"hideLava":false}})").visuals;
        check(!loaded.hideLava && loaded.hideWater && loaded.hidePowderSnow,
              "each immersion effect loads independently; a parked frame key is ignored");
        for (auto action : {input::Action::HideWater, input::Action::HideLava, input::Action::HidePowderSnow})
            check(input::defaultChord(action).empty(), "new effect keys are unbound");
        Settings value;
        value.visuals.hideWater = false;
        check(input::toggleAction(value,input::Action::HideWater) && value.visuals.hideWater
              && !value.visuals.hideEffects && value.visuals.hideLava,
              "an effect key edits only its own selection while the master is off");
        using namespace visuals;
        EffectSelection all{true, true, true, true, true, true, true};
        check(effectMask(false, all) == 0 && effectMask(true, all) == (15 | mediumBits)
              && effectMask(true, EffectSelection{.lava = true}) == lavaBit
              && effectMask(true, EffectSelection{.powderSnow = true}) == powderSnowBit,
              "the master gates every effect and each child sets only its own bit");
        check(overlayMeshBit("on_screen_effect","textures/ui/frozen_effect") == powderSnowBit
              && overlayMeshBit("ui_texture_and_color_blur_additive","textures/misc/nausea") == nauseaBit,
              "each overlay is identified by its own vanilla resource");
        check(overlayMeshBit("on_screen_effect","textures/misc/pumpkinblur") == 0
              && overlayMeshBit("on_screen_effect","textures/ui/spyglass_scope") == 0
              && overlayMeshBit("ui_textured_and_glcolor","textures/ui/frozen_effect") == 0
              && overlayMeshBit("on_screen_effect","textures/misc/pumpkinblur_extra") == 0
              && overlayMeshBit("on_screen_effect","") == 0,
              "parked frames, other frozen draws and partial names stay visible");
        check(!hideOverlayMesh(powderSnowBit,false,"on_screen_effect","textures/ui/frozen_effect")
              && !hideOverlayMesh(nauseaBit,true,"on_screen_effect","textures/ui/frozen_effect")
              && !hideOverlayMesh(waterBit | lavaBit,true,"on_screen_effect","textures/ui/frozen_effect")
              && hideOverlayMesh(powderSnowBit,true,"on_screen_effect","textures/ui/frozen_effect"),
              "an overlay hides only for its own switch on the local gameplay screen");
        CameraMedium water{true, true, false, false}, lava{false, true, true, false}, snow{false, false, false, true};
        check(visibleMedium(water, waterBit) == CameraMedium{} && visibleMedium(water, lavaBit | powderSnowBit) == water,
              "hiding water fog clears water and liquid; other switches leave water fog");
        check(visibleMedium(lava, lavaBit) == CameraMedium{} && visibleMedium(lava, waterBit) == lava,
              "lava fog, including fire resistance, follows only the lava switch");
        check(visibleMedium(snow, powderSnowBit) == CameraMedium{} && visibleMedium(snow, waterBit | lavaBit) == snow,
              "powder snow fog follows only its own switch");
        check(visibleMedium(CameraMedium{true, true, true, false}, waterBit) == CameraMedium{false, true, true, false},
              "a liquid that stays selected keeps the liquid flag");
        check(visibleMedium(CameraMedium{}, mediumBits) == CameraMedium{}, "outside any medium nothing changes");
        check(decodeSettings("{}").visuals.hideDistanceFog && input::defaultChord(input::Action::HideDistanceFog).empty(),
              "distance fog is selected by default like the other effects, with an unbound key");
        check(effectMask(true, EffectSelection{.distanceFog = true}) == distanceFogBit
              && (effectMask(true, all) & distanceFogBit) == 0,
              "distance fog has its own bit, apart from the medium fogs");
        check(hidesDistanceFog(distanceFogBit, CameraMedium{})
              && hidesDistanceFog(distanceFogBit | waterBit, visibleMedium(water, waterBit))
              && !hidesDistanceFog(distanceFogBit, water) && !hidesDistanceFog(distanceFogBit, snow)
              && !hidesDistanceFog(mediumBits, CameraMedium{}),
              "distance fog hides only where no medium fog is shown, and only for its own switch");
        check(!hidesDistanceFog(distanceFogBit, CameraMedium{}, true), "distance fog stays vanilla under Vibrant Visuals");
        check(farFog(FogRange{416, 512}, 512) == FogRange{512, 512 * farFogMargin}
              && farFog(FogRange{0, 24}, 512) == FogRange{512, 512 * farFogMargin},
              "air and rain fog start at the render distance and end just past it");
        check(farFog(FogRange{416, 600}, 512) == FogRange{600, 600 * farFogMargin},
              "vanilla fog ending beyond the render distance is the edge");
        check(!farFog(FogRange{512, 512}, 512) && !farFog(FogRange{0, 0}, 512)
              && !farFog(FogRange{std::numeric_limits<float>::quiet_NaN(), 512}, 512)
              && !farFog(FogRange{0, std::numeric_limits<float>::infinity()}, 512)
              && !farFog(FogRange{0, 512}, std::numeric_limits<float>::quiet_NaN()),
              "fog with nothing to hide and unreadable values stay vanilla");
    }
    {
        check(decodeSettings("{}").visuals.hideBossBars
              && !decodeSettings(R"({"visuals":{"hideBossBars":false}})").visuals.hideBossBars,
              "boss bars are selected by default and load their independent selection");
        Settings value;
        value.visuals.hideBossBars = false;
        check(input::defaultChord(input::Action::HideBossBars).empty()
              && input::toggleAction(value,input::Action::HideBossBars) && value.visuals.hideBossBars
              && !value.visuals.hideEffects && value.visuals.hideWeather && value.visuals.hideParticles,
              "the unbound boss key changes only its selection while the master is off");
        check(visuals::effectMask(false,true,true,true) == 0
              && visuals::effectMask(true,false,false,true) == visuals::bossBarsBit
              && visuals::effectMask(true,true,true,true) == 7,
              "boss visibility is independent of weather and particles under the common master");
        visuals::BossBarRoute boss;
        check(!boss.visit("boss_name") && !boss.visit("boss_health_grid") && !boss.visit("boss_hud_panel")
              && !boss.visit("boss_health_panel") && boss.visit("hud_screen"),
              "boss text and sprites require both observed panels below the HUD root");
        visuals::BossBarRoute inventory;
        check(!inventory.visit("boss_hud_panel") && !inventory.visit("boss_health_panel")
              && !inventory.visit("inventory_screen"), "similarly named menu controls are never boss HUD drawing");
        visuals::BossBarRoute unrelated;
        check(!unrelated.visit("boss_health_panel_extra") && !unrelated.visit("boss_hud_panel")
              && !unrelated.visit("hud_screen"), "partial or substring matches leave unrelated HUD controls visible");
    }
    {
        auto defaults = decodeSettings(R"({"visuals":{"hideOffhand":true}})");
        check(!defaults.visuals.hideEffects && defaults.visuals.hideWeather && defaults.visuals.hideParticles,
              "files without effect keys start with the master off and every effect selected");
        auto older = decodeSettings(R"({"visuals":{"hideWeather":false,"hideParticles":true}})");
        check(!older.visuals.hideEffects && !older.visuals.hideWeather && older.visuals.hideParticles,
              "saved effect selections load as saved");
        check(visuals::hiddenWeatherLayers(true,false) == std::array<bool,7>{true,true,false,false,false,false,false}
              && visuals::hiddenWeatherLayers(false,true) == std::array<bool,7>{false,false,true,true,true,true,true}
              && visuals::hiddenWeatherLayers(false,false) == std::array<bool,7>{}
              && visuals::hiddenWeatherLayers(true,true) == std::array<bool,7>{true,true,true,true,true,true,true},
              "rain and snow are independent of ambient particles in every visibility combination");
        check(input::toggleAction(defaults,input::Action::HideWeather) && !defaults.visuals.hideWeather
              && defaults.visuals.hideParticles && defaults.visuals.hideOffhand,
              "the weather key toggles only its own switch");
        check(input::toggleAction(defaults,input::Action::HideWeather) && defaults.visuals.hideWeather
              && defaults.visuals.hideParticles, "the weather key turns its switch back on");
        auto* master = settings::find("visuals.hideEffects");
        master->adjust(defaults,1);
        check(defaults.visuals.hideEffects && defaults.visuals.hideWeather && defaults.visuals.hideParticles,
              "one master switch turns every selected effect on");
        auto saved = decodeSettings(R"({"visuals":{"hideEffects":false,"hideWeather":true,"hideParticles":true}})");
        check(!saved.visuals.hideEffects && saved.visuals.hideWeather && saved.visuals.hideParticles,
              "the master and selections load independently");
        input::toggleAction(saved,input::Action::HideWeather);
        check(!saved.visuals.hideEffects && !saved.visuals.hideWeather && saved.visuals.hideParticles,
              "individual keys edit selections without enabling the master");
        master->adjust(defaults,1);
        check(!defaults.visuals.hideEffects && defaults.visuals.hideWeather && defaults.visuals.hideParticles,
              "master off keeps the same selection");
        for (bool enabled : {false,true}) for (bool weather : {false,true}) for (bool particles : {false,true}) {
            auto mask = visuals::effectMask(enabled,weather,particles);
            auto layers = visuals::hiddenWeatherLayers((mask & visuals::weatherBit) != 0,(mask & visuals::particlesBit) != 0);
            check(layers[0] == (enabled && weather) && layers[2] == (enabled && particles),
                  "the master gates both precipitation and ambient particle drawing");
            check(visuals::hideParticle(mask,true) == (enabled && (weather || particles))
                  && visuals::hideParticle(mask,false) == (enabled && particles),
                  "rain splash follows either hide switch while other water splashes follow only Particles");
        }
        check(visuals::rainEffectMatches("pack:rain","pack:rain")
              && visuals::rainEffectMatches(std::string(192,'a'),std::string(192,'a'))
              && !visuals::rainEffectMatches("pack:rain","pack:rain_extra")
              && !visuals::rainEffectMatches("pack:rain","other:rain")
              && !visuals::rainEffectMatches("","")
              && !visuals::rainEffectMatches(std::string(193,'a'),std::string(193,'a')),
              "only an exact bounded identifier learned from the rain mapping can hide an emitter");
    }
    {
        check(decodeSettings("{}").camera.freeCameraSpeed == 20, "older settings retain the original FreeCamera speed");
        check(!decodeSettings("{}").camera.freeCameraWorldFixed,
              "older settings keep player-relative FreeCamera motion");
        Settings reference;
        auto* option = settings::find("camera.freeCameraWorldFixed");
        check(option && std::get<settings::ChoiceValue>(option->read(reference)).label == "cameraReference.player",
              "the FreeCamera reference row defaults to Player");
        option->adjust(reference, 1);
        check(reference.camera.freeCameraWorldFixed
              && std::get<settings::ChoiceValue>(option->read(reference)).label == "cameraReference.world"
              && decodeSettings(R"({"camera":{"freeCameraWorldFixed":true}})").camera.freeCameraWorldFixed,
              "the reference choice selects and loads world fixation");
        option->adjust(reference, -1);
        check(!reference.camera.freeCameraWorldFixed, "the reference choice can return to Player");
        check(decodeSettings(R"({"camera":{"freeCameraSpeed":999}})").camera.freeCameraSpeed == 100
              && decodeSettings(R"({"camera":{"freeCameraSpeed":1}})").camera.freeCameraSpeed == 5
              && decodeSettings(R"({"camera":{"freeCameraSpeed":23}})").camera.freeCameraSpeed == 25,
              "flight speed loads within bounds and snaps to five-block steps");
        check(camera::normalizeFlightSpeed(std::numeric_limits<float>::infinity()) == 20
              && camera::adjustFlightSpeed(100,1) == 100 && camera::adjustFlightSpeed(5,-1) == 5,
              "invalid flight speed falls back and speed keys stop at the limits");
        check(input::defaultChord(input::Action::FreeCameraSpeedUp).empty()
              && input::defaultChord(input::Action::FreeCameraSpeedDown).empty()
              && !input::actionAllowed(input::Action::FreeCameraSpeedUp,true,false)
              && input::actionAllowed(input::Action::FreeCameraSpeedUp,true,false,true)
              && !input::actionAllowed(input::Action::FreeCameraSpeedDown,false,true,true),
              "speed keys are unbound and only apply in FreeCamera gameplay");
    }
    {
        auto defaults = decodeSettings(R"({"interaction":{"breaking":false}})");
        check(defaults.interaction.attackTicks == 10 && defaults.interaction.useTicks == 10
              && defaults.interaction.attackClicks == 1 && defaults.interaction.useClicks == 1,
              "older interaction settings keep the 0.5 s periodic cadence and one click per tick");
        auto seconds = decodeSettings(R"({"interaction":{"attackInterval":1.23,"useInterval":0.12}})");
        check(seconds.interaction.attackTicks == 25 && seconds.interaction.useTicks == 2,
              "intervals saved in seconds migrate to the nearest whole tick");
        auto modes = decodeSettings(R"({"interaction":{"attackMode":"fast","useMode":"sideways"}})");
        check(modes.interaction.attackMode == interaction::AutoMode::Fast && modes.interaction.useMode == interaction::AutoMode::Periodic,
              "auto modes load by name and unknown names fall back to periodic");
        check(!modes.interaction.attackHeldOnly
              && decodeSettings(R"({"interaction":{"useHeldOnly":true}})").interaction.useHeldOnly
              && decodeSettings(R"({"interaction":{"useTrigger":"held"}})").interaction.useHeldOnly,
              "fast click defaults to always; held-only loads, including the short-lived trigger name");
        check(!decodeSettings(R"({"interaction":{"autoAttack":true,"autoUse":true}})").interaction.autoAttack,
              "a hand-edited switch never starts auto attack on load");
        check(decodeSettings(R"({"overlays":{"skyLight":true}})").overlays.lightValue == overlay::LightValue::Sky
              && decodeSettings(R"({"overlays":{"skyLight":true,"lightValue":"both"}})").overlays.lightValue == overlay::LightValue::Both
              && decodeSettings("{}").overlays.lightValue == overlay::LightValue::Block
              && decodeSettings("{}").overlays.lightRange == 16
              && decodeSettings(R"({"overlays":{"lightRange":400}})").overlays.lightRange == 64,
              "the old sky-light switch migrates, the value choice wins, and the range stays in 4-64");
        check(decodeSettings("{}").overlays.lightFacing == overlay::LightFacing::View
              && decodeSettings(R"({"overlays":{"lightFacing":"east"}})").overlays.lightFacing == overlay::LightFacing::East,
              "numbers follow the view by default and a fixed direction loads by name");
        auto fresh = decodeSettings("{}");
        check(fresh.information.coordinates && fresh.information.facing && fresh.information.biome
              && fresh.information.fps && !fresh.information.dimension,
              "a fresh file shows coordinates, facing, biome and fps, matching DESIGN");
        check(!fresh.information.scaledCoordinates && !fresh.information.biomeId
              && !fresh.information.biomeIdOnly
              && !fresh.information.difficulty && !fresh.information.yaw && !fresh.information.pitch
              && !fresh.information.sprinting && !fresh.information.horizontalSpeed
              && !fresh.information.verticalSpeed && !fresh.information.realTime
              && !fresh.information.realTimeDate,
              "new wave-one Info HUD lines default off");
        check(!fresh.information.targetBlock
              && decodeSettings(R"({"information":{"targetBlock":true,"block":true}})").information.targetBlock
              && !decodeSettings(R"({"information":{"block":true}})").information.targetBlock,
              "the aimed-block line defaults off, loads when saved and is not the standing-block switch");
        check(!fresh.information.slimeChunk && !fresh.map.worldMapSlime
                  && !decodeSettings(R"({"information":{"chunk":true}})").information.slimeChunk,
              "the slime chunk line and map layer default off");
        check(fresh.information.debugHideHud && fresh.information.debugHideTarget && fresh.information.debugShadow
              && fresh.information.debugLabels == 0 && !fresh.information.debug,
              "a fresh file hides the Info HUD and Target while Debug is on, with shadows and game-standard names");
        auto armor = decodeSettings(R"({"information":{"targetArmor":7}})");
        check(fresh.information.targetArmor == 0 && armor.information.targetArmor == 2,
              "the armor meter defaults to icons and clamps to the known modes");
        auto both = decodeSettings(R"({"interaction":{"attackInterval":3,"attackTicks":7}})");
        check(both.interaction.attackTicks == 7, "a saved tick interval wins over an old seconds value");
        auto bounded = decodeSettings(R"({"interaction":{"attackTicks":0,"useTicks":99999,"attackClicks":0,"useClicks":50}})");
        check(bounded.interaction.attackTicks == 1 && bounded.interaction.useTicks == 1200
              && bounded.interaction.attackClicks == 1 && bounded.interaction.useClicks == 10,
              "stored tick intervals and click rates stay in range");
        bounded.interaction.attackTicks = std::numeric_limits<float>::quiet_NaN();
        bounded.interaction.useClicks = std::numeric_limits<float>::infinity();
        bounded.normalize();
        check(bounded.interaction.attackTicks == 10 && bounded.interaction.useClicks == 1,
              "non-finite values recover usable defaults");
    }
    {
        auto* mode = settings::find("interaction.breakingMode");
        Settings value;
        mode->adjust(value,-1);
        check(value.interaction.breakingMode == interaction::RestrictionMode::HeightBand, "choice wraps backward");
        mode->adjust(value,0);
        check(value.interaction.breakingMode == interaction::RestrictionMode::Plane, "click advances choice and wraps forward");
        check(!mode->numeric && std::get<settings::ChoiceValue>(mode->read(value)).label == "mode.plane",
              "choice exposes localized label and never opens numeric input");
        for (auto label : interaction::restrictionLabels)
            for (auto locale : ui::translations::locales)
                check(!ui::translations::find(label, locale).empty(), "every restriction choice is translated");
        auto unknown = decodeSettings(R"({"interaction":{"breakingMode":"unknown"},
            "camera":{"magnification":7}})");
        check(unknown.interaction.breakingMode == interaction::RestrictionMode::Plane
                  && unknown.camera.magnification == 7,
              "an unknown stored restriction mode falls back without discarding other settings");
    }
    {
        // L-15: Height band is a breaking mode only; old files keep their mode.
        auto* placement = settings::find("interaction.placementMode");
        Settings value;
        placement->adjust(value,-1);
        check(value.interaction.placementMode == interaction::RestrictionMode::Layer, "placement does not offer Height band");
        value.interaction.placementMode = interaction::RestrictionMode::HeightBand;
        value.normalize();
        check(value.interaction.placementMode == interaction::RestrictionMode::Plane, "placement drops Height band");
        auto decoded = decodeSettings(R"({"interaction":{"breakingMode":"heightBand","breakingBand":3}})");
        check(decoded.interaction.breakingMode == interaction::RestrictionMode::HeightBand
              && decoded.interaction.breakingBand == 3, "Height band and its rows load");
        decoded = decodeSettings(R"({"interaction":{"breakingMode":"layer"}})");
        check(decoded.interaction.breakingMode == interaction::RestrictionMode::Layer && decoded.interaction.breakingBand == 2,
              "an older file keeps its mode and gets the default band");
        decoded = decodeSettings(R"({"interaction":{"breakingBand":99}})");
        check(decoded.interaction.breakingBand == 16, "the band is clamped");
    }
    {
        Settings value;
        auto* biome = settings::find("information.biomeDisplay");
        check(biome && std::get<settings::ChoiceValue>(biome->read(value)).label == "biomeDisplay.name",
              "biome display defaults to the localized name");
        biome->adjust(value, 1);
        check(value.information.biomeId && !value.information.biomeIdOnly
              && std::get<settings::ChoiceValue>(biome->read(value)).label == "biomeDisplay.nameAndId",
              "biome display can append the registry id");
        biome->adjust(value, 1);
        check(value.information.biomeId && value.information.biomeIdOnly
              && std::get<settings::ChoiceValue>(biome->read(value)).label == "biomeDisplay.id",
              "biome display can show only the registry id");
        auto* clock = settings::find("information.realTimeDisplay");
        clock->adjust(value, 1);
        check(value.information.realTimeDate
              && std::get<settings::ChoiceValue>(clock->read(value)).label == "realTimeDisplay.dateAndTime",
              "real-time display can include the date");
    }
    auto old = decodeSettings(R"({"version":1,"camera":{"zoom":true,"magnification":3.5,"wheelStep":0.5}})");
    check(old.inventory.repeatTrades, "repeated trading defaults on in existing settings");
    auto disabledTrading = decodeSettings(R"({"version":1,"inventory":{"repeatTrades":false}})");
    check(!disabledTrading.inventory.repeatTrades, "repeated trading loads an explicit disabled setting");
    check(old.camera.magnification == 3.5f && !old.lighting.nightVision,
          "adding lighting must preserve existing camera settings");
    check(old.inventory.sorting && old.inventory.sortContainers && !old.inventory.transfer
          && old.inventory.transferWheelOne && old.inventory.transferWheelStack
          && old.inventory.transferDragStack && old.inventory.transferDragOne,
          "missing inventory transfer settings: the feature off, every gesture on once enabled");
    check(old.inventory.restockOffhand && !old.inventory.toolSwitchInventory
          && old.interaction.toolGuard && old.interaction.toolGuardStrict && !old.interaction.elytraSwap
          && old.interaction.elytraFireworkJump && old.interaction.elytraRiptide && old.interaction.elytraReturnSeconds == 3,
          "offhand restock and tool protection default on; inventory tool fetch and auto elytra off");
    check(!old.map.minimap && old.map.zoom == map::defaultZoomIndex && !old.map.rotate && !old.map.round && old.map.debugHide
          && old.hud.minimap.anchor == ui::Anchor::TopRight, "the minimap starts off, at 128 blocks, top right");
    check(old.map.minimapLayers, "map layers show on the minimap by default (L-295)");
    check(old.camera.freeCameraUndergroundHold == 2
              && decodeSettings(R"({"camera":{"freeCameraUndergroundHold":6}})").camera.freeCameraUndergroundHold == 6
              && decodeSettings(R"({"camera":{"freeCameraUndergroundHold":9}})").camera.freeCameraUndergroundHold == 6,
          "the underground hold defaults to 3 s, keeps a saved choice and clamps unknown ones");
    check(old.camera.freeCameraHide == 1 && decodeSettings(R"({"camera":{"freeCameraHide":3}})").camera.freeCameraHide == 3
              && decodeSettings(R"({"camera":{"freeCameraHide":8}})").camera.freeCameraHide == 3,
          "FreeCamera hides the hand by default; the setting loads and is clamped (L-191)");
    check(old.schematic.ghostTint == 70 && old.schematic.ghostLines == 1,
          "ghosts default to a 70% tint and normal outlines (L-186)");
    {
        auto look = decodeSettings(R"({"schematic":{"ghostTintPercent":65,"ghostLines":2}})").schematic;
        auto wild = decodeSettings(R"({"schematic":{"ghostTintPercent":190,"ghostLines":-3}})").schematic;
        check(look.ghostTint == 65 && look.ghostLines == 2 && wild.ghostTint == 100 && wild.ghostLines == 0,
              "the ghost tint and outline width load and are clamped");
        check(decodeSettings(R"({"schematic":{"ghostLines":3}})").schematic.ghostLines == 3
                  && decodeSettings(R"({"schematic":{"ghostLines":9}})").schematic.ghostLines == 3,
              "None is the fourth outline width, after the saved three (L-254)");
        check(old.schematic.mistakeMarks && !decodeSettings(R"({"schematic":{"mistakeMarks":false}})").schematic.mistakeMarks,
              "mistake marks show by default and can be turned off (L-252)");
        auto relaxed = decodeSettings(R"({"schematic":{"ignoreSignal":true,"litAsState":true,"ignoreGrowth":true}})").schematic;
        check(!old.schematic.ignoreSignal && !old.schematic.litAsState && !old.schematic.ignoreGrowth
                  && relaxed.ignoreSignal && relaxed.litAsState && relaxed.ignoreGrowth,
              "the check is strict by default and the relaxations load (L-262)");
        check(decodeSettings(R"({"schematic":{"ghostTint":0}})").schematic.ghostTint == 100
              && decodeSettings(R"({"schematic":{"ghostTint":1}})").schematic.ghostTint == 40
              && decodeSettings(R"({"schematic":{"ghostTint":2}})").schematic.ghostTint == 0
              && decodeSettings(R"({"schematic":{"ghostTint":2,"ghostTintPercent":55}})").schematic.ghostTint == 55,
              "the earlier strong, weak and none choices become 100%, 40% and 0%");
    }
    check(!decodeSettings(R"({"map":{"waypointsMinimap":false}})").map.minimapLayers
              && decodeSettings(R"({"map":{"waypointsMinimap":true,"minimapShapes":false}})").map.minimapLayers
              && !decodeSettings(R"({"map":{"minimapLayers":false,"waypointsMinimap":true}})").map.minimapLayers,
          "the minimap's layer switch takes an old waypoints-off file as off, and its own key wins (L-295)");
    check(!old.map.worldMapNames && decodeSettings(R"({"map":{"worldMapNames":true}})").map.worldMapNames,
          "world map names show on hover until Always is saved on");
    check(old.map.worldMapWaypoints && old.map.worldMapSchematics && old.map.worldMapShapes
              && !decodeSettings(R"({"map":{"worldMapShapes":false}})").map.worldMapShapes
              && decodeSettings(R"({"map":{"worldMapShapes":false}})").map.worldMapSchematics,
          "the world map's layer switches start on and load one by one (L-146)");
    check(old.map.worldMapList == 0 && decodeSettings(R"({"map":{"worldMapList":2}})").map.worldMapList == 2
              && decodeSettings(R"({"map":{"worldMapList":9}})").map.worldMapList == 2,
          "the side panel's list starts on waypoints and loads clamped (L-204)");
    check(!old.camera.zoomWideAngle && decodeSettings(R"({"camera":{"zoomWideAngle":true}})").camera.zoomWideAngle,
          "Wide-angle view stays off until saved on (L-140)");
    check(!old.visuals.skipServerPacks && decodeSettings(R"({"visuals":{"skipServerPacks":true}})").visuals.skipServerPacks,
          "server resource packs apply until skipping is saved on (L-167)");
    check(old.information.debugHideVanilla && !decodeSettings(R"({"information":{"debugHideVanilla":false}})").information.debugHideVanilla,
          "Debug View hides the game's position and paper doll unless saved off");
    check(old.information.hudHidePaperDoll && !decodeSettings(R"({"information":{"hudHidePaperDoll":false}})").information.hudHidePaperDoll,
          "the Info HUD hides a covered paper doll unless saved off (L-145)");
    {
        // L-137: what players see of the gates on an unverified game.
        using lamium::paused::Level;
        for (auto feature : lamium::paused::gatedFeatures)
            check(lamium::paused::onUnverified(feature) != Level::No, "every gated feature is marked paused");
        check(lamium::paused::onUnverified("nightVision") == Level::Fully && lamium::paused::onUnverified("freecamera") == Level::Partly
              && lamium::paused::onUnverified("zoom") == Level::No, "fully, partly or not paused");
        lamium::Settings s;
        s.inventory.repeatTrades = false;
        s.inventory.transfer = false;
        s.lighting.nightVision = true;
        s.visuals.hideEffects = true;
        s.visuals.hideWater = s.visuals.hideLava = s.visuals.hidePowderSnow = s.visuals.hideDistanceFog = false;
        s.visuals.hidePortal = false;
        s.visuals.hideWeather = s.visuals.hideNausea = false;
        auto used = lamium::paused::inUse(s);
        check(used.size() == 1 && used[0] == "nightVision", "the join toast names switched-on gated features, not particles alone");
        s.visuals.hideWeather = true;
        check(lamium::paused::inUse(s).size() == 2, "a gated hide effect counts");
        s.inventory.repeatTrades = true;
        check(lamium::paused::inUse(s).size() == 3
              && lamium::paused::onUnverified("repeatTrades") == Level::Fully,
              "repeated trades are named and fully paused on unverified games");
        s.inventory.transfer = true;
        check(lamium::paused::inUse(s).size() == 4
              && lamium::paused::onUnverified("transfer") == Level::Partly,
              "transfer native routes are partly paused while the old storage gestures remain usable");
    }
    {
        auto loaded = decodeSettings(R"({"map":{"zoom":9,"rotate":true},"hud":{"minimap":{"dx":-30}}})");
        loaded.normalize();
        check(map::blocksAcross(loaded.map.zoom) == 512 && loaded.map.rotate && loaded.hud.minimap.dx == -30,
              "map settings load; an old out-of-range zoom index is clamped");
        check(map::blocksAcross(decodeSettings(R"({"map":{"zoom":1}})").map.zoom) == 64
              && map::blocksAcross(decodeSettings(R"({"map":{"range":96,"zoom":4}})").map.zoom) == 96,
              "an old zoom index keeps its width; the saved range wins");
    }
    auto equipment = decodeSettings(R"({"inventory":{"restockOffhand":false,"toolSwitchInventory":true},"interaction":{"toolGuard":false,"elytraSwap":true}})");
    check(!equipment.inventory.restockOffhand && equipment.inventory.toolSwitchInventory
          && !equipment.interaction.toolGuard && equipment.interaction.elytraSwap,
          "equipment follow-up switches load from disk");
    check(!old.inventory.handRestock && old.inventory.restockFromHotbar,
          "restock defaults off and hotbar sources on in older files");
    auto restock = decodeSettings(R"({"inventory":{"handRestock":true,"restockFromHotbar":false}})");
    check(restock.inventory.handRestock && !restock.inventory.restockFromHotbar,
          "restock switch and hotbar source preference load independently");
    auto olderRestock = decodeSettings(R"({"inventory":{"handRestock":true}})").inventory;
    check(olderRestock.restockShulkerBoxes && olderRestock.restockBuckets,
          "a file written before the shulker and bucket switches defaults both on");
    auto olderTransfer = decodeSettings(R"({"inventory":{"transfer":false}})");
    check(!olderTransfer.inventory.transfer && olderTransfer.inventory.transferWheelOne
          && olderTransfer.inventory.transferWheelStack && olderTransfer.inventory.transferDragStack
          && olderTransfer.inventory.transferDragOne,
          "a saved master switch survives while absent gesture switches default on");
    check(!old.inventory.fakeOffhand && old.inventory.fakeOffhandSlot == 9
          && input::defaultChord(input::Action::FakeOffhandUse) == input::Chord{{input::Device::Mouse, 2}},
          "Fake Offhand defaults off, with right-click activation and slot nine");
    check(decodeSettings(R"({"inventory":{"fakeOffhand":true,"fakeOffhandSlot":3}})").inventory.fakeOffhandSlot == 3
          && decodeSettings(R"({"inventory":{"fakeOffhandSlot":3.0}})").inventory.fakeOffhandSlot == 3
          && decodeSettings(R"({"inventory":{"fakeOffhandSlot":99}})").inventory.fakeOffhandSlot == 9,
          "Fake Offhand loads a selected slot and bounds invalid values");
    using inventory::fakeOffhand::placementSlot;
    check(placementSlot(true,true,0,8,true,true,false,false) == 8
          && placementSlot(true,true,0,8,true,true,true,true) == 8,
          "Fake Offhand selects blocks against ordinary or sneaked interactive targets");
    check(!placementSlot(true,true,0,8,true,true,true,false)
          && !placementSlot(true,true,0,8,false,true,false,false)
          && !placementSlot(true,true,0,8,true,false,false,false)
          && !placementSlot(false,true,0,8,true,true,false,false),
          "Fake Offhand preserves ordinary interaction and non-placement uses");
    check(!old.camera.freelookToggle, "Freelook activation defaults to holding the key");
    check(!old.camera.zoomToggle, "Zoom activation defaults to holding the key");
    check(old.camera.freelookStartPerspective == 1 && old.camera.freeCameraToggle,
          "Freelook starts in rear third person and FreeCamera defaults to Toggle");
    {
        Settings toggled; toggled.camera.freelookToggle = true;
        check(decodeSettings(R"({"camera":{"freelookToggle":true}})").camera.freelookToggle, "Freelook activation is read from storage");
        auto activation = settings::find("camera.freelookActivation");
        check(activation && std::get<settings::ChoiceValue>(activation->read(toggled)).label == "activation.toggle",
              "activation choice reads the stored mode");
        activation->adjust(toggled, 1);
        check(!toggled.camera.freelookToggle, "activation choice cycles back to hold");
    }
    {
        Settings camera;
        auto view = settings::find("camera.freelookStartPerspective");
        check(view && std::get<settings::ChoiceValue>(view->read(camera)).label == "perspective.rear",
              "Freelook starting-view row shows the default");
        view->adjust(camera, 1);
        check(camera.camera.freelookStartPerspective == 2, "Freelook starting view can select front third person");
        view->adjust(camera, 1);
        check(camera.camera.freelookStartPerspective == 0, "Freelook starting view wraps to first person");
        auto activation = settings::find("camera.freecameraActivation");
        activation->adjust(camera, 1);
        check(!camera.camera.freeCameraToggle, "FreeCamera activation can select Hold");
        check(decodeSettings(R"({"camera":{"freelookStartPerspective":2,"freeCameraToggle":false}})").camera.freelookStartPerspective == 2,
              "stored Freelook starting view is read");
        check(!decodeSettings(R"({"camera":{"freeCameraToggle":false}})").camera.freeCameraToggle,
              "stored FreeCamera activation is read");
        check(decodeSettings(R"({"camera":{"freelookStartPerspective":99}})").camera.freelookStartPerspective == 2,
              "invalid Freelook starting view is bounded");
    }
    check(input::actions[static_cast<size_t>(input::Action::Freelook)].behavior == input::Behavior::Hold
          && input::actions[static_cast<size_t>(input::Action::Freelook)].defaultKey == 0,
          "Freelook is an independent unassigned hold action");
    check(input::actions[static_cast<size_t>(input::Action::FreeCamera)].behavior == input::Behavior::Toggle
          && input::actions[static_cast<size_t>(input::Action::FreeCamera)].defaultKey == 0,
          "FreeCamera is an independent unassigned toggle action");
    for (auto action : {input::Action::OpenHotkeys, input::Action::OpenHudLayout})
        check(input::actions[static_cast<size_t>(action)].behavior == input::Behavior::Press
              && input::actions[static_cast<size_t>(action)].feature == "settings",
              "openers of screens no feature owns group with the settings keys");
    check(input::actions[static_cast<size_t>(input::Action::OpenShapes)].feature == "shapes"
          && input::actions[static_cast<size_t>(input::Action::OpenWaypoints)].feature == "waypoints"
          && input::actions[static_cast<size_t>(input::Action::OpenWorldMap)].feature == "worldMap",
          "a screen opener sits with the feature that owns the screen");
    check(input::actions[static_cast<size_t>(input::Action::OpenHotkeys)].defaultKey == 0
          && input::defaultChord(input::Action::OpenHotkeys).empty(),
          "OpenHotkeys is unbound by default");
    check(decodeSettings(R"({"interface":{"gameplayHints":false}})").ui.automationStatus,
          "removed gameplay hints key loads without error");
    check(!old.inspection.hideShulkerContents, "older settings retain vanilla Shulker contents text");
    check(old.inspection.shulkerPreviews && old.inspection.emptyShulkerPreviews
          && old.inspection.bundlePreviews && old.inspection.emptyBundlePreviews,
          "older files retain the existing preview behavior including empty containers");
    auto partial = decodeSettings(R"({"camera":{"magnification":6}})");
    check(!old.visuals.hideOffhand, "existing settings keep the offhand visible");
    check(partial.camera.magnification == 6 && !partial.camera.zoomToggle, "missing fields use defaults");
    for (auto invalid : {R"({"version":999})", R"({"version":4294967297})", R"({"version":1.5})",
                         R"({"version":true})", "[]", "{"}) {
        bool rejected = false;
        try { (void)decodeSettings(invalid); } catch (...) { rejected = true; }
        check(rejected, "invalid or future settings must be rejected");
    }
    {
        auto mixed = decodeSettings(R"({"camera":true,"map":{"minimap":true,"size":"large","rotate":false},
            "inventory":{"sorting":false,"restockThreshold":[]},"orderedBindings":true,
            "bindings":{"sort":[{"device":"key","code":75}]},"hud":{"info":{"dx":123,"scale":"huge","shadow":false}}})");
        Settings defaults;
        check(mixed.camera.magnification == defaults.camera.magnification && mixed.map.minimap && !mixed.map.rotate
              && mixed.map.size == defaults.map.size && !mixed.inventory.sorting
              && mixed.inventory.restockThreshold == defaults.inventory.restockThreshold,
              "wrong section and field types keep defaults without discarding valid settings");
        check(mixed.bindings[static_cast<size_t>(input::Action::Sort)] == input::Chord{{input::Device::Key, 75}}
              && mixed.hud.info.dx == 123 && !mixed.hud.info.shadow && mixed.hud.info.scale == defaults.hud.info.scale,
              "bindings and valid HUD fields survive unrelated type errors");
        for (auto const* section : {"interaction", "information", "visuals", "overlays", "bindings", "camera", "map",
                                   "schematic", "lighting", "inspection", "inventory", "interface", "hud"}) {
            auto skipped = decodeSettings(std::string("{\"") + section + "\":[],\"extension\":{\"keep\":true}}");
            check(skipped.camera.magnification == defaults.camera.magnification,
                  "every non-object settings section is skipped");
        }
        auto fields = decodeSettings(R"({"camera":{"magnification":"6","zoomToggle":1,"freeCameraSpeed":null},
            "map":{"worldMapList":4294967297,"waypointsWorld":18446744073709551615},
            "interaction":{"breakingBand":"3"},"hud":{"info":{"dx":1e300}}})");
        check(fields.camera.magnification == defaults.camera.magnification && !fields.camera.zoomToggle
              && fields.camera.freeCameraSpeed == defaults.camera.freeCameraSpeed
              && fields.map.worldMapList == defaults.map.worldMapList && fields.map.waypointsWorld == defaults.map.waypointsWorld
              && fields.interaction.breakingBand == defaults.interaction.breakingBand && fields.hud.info.dx == defaults.hud.info.dx,
              "wrong scalar types and overflowing numbers never narrow into settings");
        auto unknownMode = decodeSettings(R"({"interaction":{"breakingMode":"mystery","placementMode":"plane"},
            "camera":{"magnification":7}})");
        check(unknownMode.interaction.breakingMode == interaction::RestrictionMode::Plane
              && unknownMode.interaction.placementMode == interaction::RestrictionMode::Plane
              && unknownMode.camera.magnification == 7,
              "an unknown restriction mode falls back without discarding other settings");
    }
    auto path = std::filesystem::temp_directory_path() /
        ("lamium-settings-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
            auto broken = path; broken += ".broken";
            std::filesystem::remove(broken, ignored);
        }
    } cleanup{path};
    {
        auto broken = path; broken += ".broken";
        { std::ofstream file(broken); file << "older backup"; }
        { std::ofstream file(path); file << "{invalid settings"; }
        bool recovered = false;
        auto restored = readSettings(path, &recovered);
        std::ifstream backup(broken);
        std::string bytes{std::istreambuf_iterator<char>(backup), {}};
        check(recovered && bytes == "{invalid settings" && restored.camera.magnification == Settings{}.camera.magnification
              && readSettings(path).bindings == Settings{}.bindings,
              "invalid JSON replaces the old broken backup before saving defaults");
    }
    writeSettings(path, disabledTrading);
    check(!readSettings(path).inventory.repeatTrades, "disabled repeated trading survives saving");
    {
        Settings target;
        target.information.targetSchematic = false;
        writeSettings(path, target);
        check(!readSettings(path).information.targetSchematic, "hidden schematic information survives restart");
    }
    {
        Settings target;
        target.information.slimeChunk = true;
        target.map.worldMapSlime = true;
        writeSettings(path, target);
        auto loaded = readSettings(path);
        check(loaded.information.slimeChunk && loaded.map.worldMapSlime, "the slime chunk line and map layer survive a restart");
    }
    {
        auto encoded = [&](Settings const& value) {
            writeSettings(path, value);
            std::ifstream file(path);
            return std::string(std::istreambuf_iterator<char>(file), {});
        };
        // A present but empty section must give the same values as a missing one,
        // so load fallbacks cannot drift from the defaults in Settings.h.
        auto defaults = encoded(Settings{});
        for (auto section : {"bindings", "camera", "hud", "information", "inspection", "interaction",
                             "interface", "inventory", "lighting", "map", "overlays", "visuals"})
            check(encoded(decodeSettings(std::string(R"({")") + section + R"(":{}})")) == defaults,
                  "an empty settings section decodes to the defaults");
    }
    {
        Settings selected;
        selected.camera.freeCameraWorldFixed = true;
        selected.visuals.hideEffects = false;
        selected.visuals.hideWeather = selected.visuals.hideParticles = true;
        writeSettings(path,selected);
        auto restored = readSettings(path);
        check(restored.camera.freeCameraWorldFixed, "world reference survives saving and restarting");
        check(!restored.visuals.hideEffects && restored.visuals.hideWeather && restored.visuals.hideParticles,
              "saving a disabled master retains both selected effects across restart");
    }
    // Exercise the actual UI accessors through disk persistence. This catches
    // options which appear editable but are omitted from encoding or decoding.
    std::unordered_set<std::string_view> ids;
    for (auto const& option : settings::options) {
        check(ids.insert(option.id).second, "option identities are unique");
        check(settings::find(option.id) == &option, "stable option lookup");
        for (auto locale : ui::translations::locales)
            check(!ui::translations::find(option.label, locale).empty(), "option labels exist");
        Settings edited;
        option.adjust(edited, 1);
        check(option.read(edited) != option.read(Settings{}), "editing changes the target value");
        for (auto const& other : settings::options)
            if (other.id != option.id)
                check(other.read(edited) == other.read(Settings{}), "editing preserves unrelated options");
        writeSettings(path, edited);
        auto restored = readSettings(path);
        // The Auto Attack/Use switches are session state: a new game must not
        // start clicking, so they load switched off.
        bool session = option.id == "interaction.autoAttack" || option.id == "interaction.autoUse";
        for (auto const& other : settings::options)
            check(session ? other.read(restored) == other.read(Settings{}) : other.read(restored) == other.read(edited),
                  "all options survive disk round trip; session switches load off");
    }
    check(settings::find("unknown") == nullptr, "unknown option lookup is safe");
    {
        auto legacy = decodeSettings("{}");
        for (auto const& binding : legacy.bindings) check(!binding, "legacy settings fall back to Lamium defaults");
        auto configured = decodeSettings(R"({"bindings":{"zoom":[{"device":"key","code":90},{"device":"key","code":51}],"sort":[],"nightvision":[{"device":"key","code":16},{"device":"wheel","code":1}]}})");
        writeSettings(path, configured);
        check(readSettings(path).bindings == configured.bindings, "chords, wheel and explicit unbound survive restart");
        auto const border = static_cast<size_t>(input::Action::ChunkBorders);
        input::Chord f3b{{input::Device::Key, 0x72}, {input::Device::Key, 0x42}}, bf3{f3b[1], f3b[0]};
        check(decodeSettings(R"({"bindings":{"chunkborders":[{"device":"key","code":66},{"device":"key","code":114}]}})")
            .bindings[border] == f3b, "sorted legacy chords regain F3-first press order");
        auto ordered = decodeSettings(R"({"orderedBindings":true,"bindings":{"chunkborders":[{"device":"key","code":66},{"device":"key","code":114}]}})");
        check(ordered.bindings[border] == bf3, "saved press order is kept as written");
        writeSettings(path, ordered);
        check(readSettings(path).bindings[border] == bf3, "press order survives restart");
        configured.bindings[static_cast<size_t>(input::Action::Zoom)].reset();
        writeSettings(path, configured);
        check(!readSettings(path).bindings[static_cast<size_t>(input::Action::Zoom)], "reset removes only the override");
        check(readSettings(path).bindings[static_cast<size_t>(input::Action::Sort)]->empty(), "reset preserves another unbound action");
        auto locked = decodeSettings(R"({"bindings":{"settings":[]}})"
        );
        check(!locked.bindings[static_cast<size_t>(input::Action::Settings)],
              "stored empty settings binding recovers the default");
        auto remapped = decodeSettings(R"({"bindings":{"settings":[{"device":"key","code":70}]}})"
        );
        check(remapped.bindings[static_cast<size_t>(input::Action::Settings)]
                  == input::Chord{input::Token{input::Device::Key, 70}},
              "non-empty settings binding still loads");
        for (auto invalid : {R"({"bindings":[]})", R"({"bindings":{"zoom":true}})",
                             R"({"bindings":{"zoom":[{"device":"wheel","code":1}]}})",
                             R"({"bindings":{"sort":[{"device":"key","code":4294967350}]}})",
                             R"({"bindings":{"sort":[{"device":"key","code":51.5}]}})"}) {
            auto skipped = decodeSettings(invalid);
            check(!skipped.bindings[static_cast<size_t>(input::Action::Zoom)]
                  && !skipped.bindings[static_cast<size_t>(input::Action::Sort)],
                  "malformed bindings keep their defaults without integer narrowing");
        }
        auto mixed = decodeSettings(R"({"bindings":{"zoom":[{"device":true,"code":4}],
            "sort":[{"device":"key","code":75}]},"camera":{"magnification":6}})");
        check(!mixed.bindings[static_cast<size_t>(input::Action::Zoom)]
              && mixed.bindings[static_cast<size_t>(input::Action::Sort)] == input::Chord{{input::Device::Key, 75}}
              && mixed.camera.magnification == 6, "one malformed binding leaves other bindings and settings intact");
    }
    {
        std::ofstream initial(path);
        initial << R"({"version":1,"extension":{"keep":true},"camera":{"futureField":42}})";
    }
    old.lighting.nightVision = true;
    old.inventory.sorting = false;
    old.inventory.sortContainers = false;
    old.inventory.transfer = true;
    old.inventory.handRestock = true;
    old.inventory.restockFromHotbar = false;
    old.inventory.transferWheelOne = false;
    old.inventory.transferWheelStack = false;
    old.inventory.transferDragStack = false;
    old.inventory.transferDragOne = false;
    old.inventory.transferWheelMatching = false;
    old.inventory.transferWheelAll = false;
    old.inventory.transferClickMatching = false;
    old.inventory.transferClickAll = false;
    old.inventory.transferDragKeepOne = false;
    old.inventory.transferDropMatching = false;
    old.ui.automationStatus = false;
    old.interaction.attackTicks = 12;
    old.camera.freelookToggle = true;
    old.camera.freelookStartPerspective = 2;
    old.camera.freeCameraToggle = false;
    old.interaction.useTicks = 34;
    old.interaction.attackClicks = 3;
    old.interaction.useClicks = 4;
    writeSettings(path, old);
    auto loaded = readSettings(path);
    check(loaded.inventory.handRestock && !loaded.inventory.restockFromHotbar,
          "restock switch and hotbar source preference survive disk round trip");
    check(loaded.interaction.attackTicks == 12 && loaded.interaction.useTicks == 34
          && loaded.interaction.attackClicks == 3 && loaded.interaction.useClicks == 4,
          "independent attack and use intervals and click rates survive disk round trip");
    check(loaded.camera.magnification == 3.5f && loaded.lighting.nightVision && loaded.camera.freelookToggle
          && loaded.camera.freelookStartPerspective == 2 && !loaded.camera.freeCameraToggle, "disk round trip");
    check(!loaded.inventory.sorting && !loaded.inventory.sortContainers && loaded.inventory.transfer,
          "inventory switches survive saves");
    check(!loaded.inventory.transferWheelOne && !loaded.inventory.transferWheelStack
          && !loaded.inventory.transferDragStack && !loaded.inventory.transferDragOne,
          "transfer gesture switches survive saves");
    check(!loaded.inventory.transferWheelMatching && !loaded.inventory.transferWheelAll
          && !loaded.inventory.transferClickMatching && !loaded.inventory.transferClickAll
          && !loaded.inventory.transferDragKeepOne && !loaded.inventory.transferDropMatching
          && loaded.inventory.transferDropOne, "new transfer switches survive saves");
    auto legacyTransfer = decodeSettings(R"({"inventory":{"transfer":false,"transferDragOne":false}})");
    check(!legacyTransfer.inventory.transfer && !legacyTransfer.inventory.transferDragOne
          && legacyTransfer.inventory.transferWheelMatching && legacyTransfer.inventory.transferWheelAll
          && legacyTransfer.inventory.transferClickMatching && legacyTransfer.inventory.transferClickAll
          && legacyTransfer.inventory.transferDragKeepOne && legacyTransfer.inventory.transferCursorMatching
          && legacyTransfer.inventory.transferDropOne && legacyTransfer.inventory.transferDropKeepOne
          && legacyTransfer.inventory.transferDropAll && legacyTransfer.inventory.transferDropMatching, "older settings retain old choices and default new gestures on");
    check(!loaded.ui.automationStatus, "hidden automation status survives restart");
    auto contents = [&]() {
        std::ifstream file(path);
        return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    };
    auto saved = contents();
    check(saved.find("futureField") != std::string::npos && saved.find("extension") != std::string::npos,
          "unknown fields survive saves");
#ifdef _WIN32
    auto handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    check(handle != INVALID_HANDLE_VALUE, "lock destination for replacement failure test");
    bool rejected = false;
    try { writeSettings(path, Settings{}); } catch (...) { rejected = true; }
    CloseHandle(handle);
    check(rejected && contents() == saved, "failed replacement must preserve previous settings");
    auto temporary = path; temporary += ".tmp";
    check(!std::filesystem::exists(temporary), "failed save cleans its temporary file");
#endif
}

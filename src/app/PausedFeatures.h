#pragma once
// Pure projections of the shared capability catalog (L-202). User intent is
// separate from compatibility and runtime readiness; no second failure map.
#include "settings/Settings.h"
#include "app/Capabilities.h"
#include <array>
#include <string_view>
#include <vector>

namespace lamium::paused {
enum class Level { No, Partly, Fully };
// Feature ids as in ui::features.
inline constexpr auto gatedFeatures = [] {
    constexpr size_t featureCount = [] {
        size_t result = 0;
        for (size_t i = 0; i < capabilities::count; ++i) {
            auto feature = capabilities::definitions[i].feature;
            if (feature.empty()) continue;
            bool found = false;
            for (size_t j = 0; j < i; ++j) found |= capabilities::definitions[j].feature == feature;
            if (!found) ++result;
        }
        return result;
    }();
    std::array<std::string_view,featureCount> features{};
    size_t count = 0;
    for (auto const& d : capabilities::definitions) {
        if (d.feature.empty()) continue;
        bool found = false;
        for (auto feature : features) found |= feature == d.feature;
        if (!found) features[count++] = d.feature;
    }
    return features;
}();
// An unverified game on a supported loader, for the static projections.
inline constexpr capabilities::Environment unverifiedEnvironment{false, capabilities::loaderMajor,
    capabilities::loaderMinor, capabilities::minimumLoaderPatch};
inline Level onUnverified(std::string_view feature) {
    return static_cast<Level>(capabilities::featureImpact(feature,[](capabilities::Id id) {
        return capabilities::Status{capabilities::compatibility(id,unverifiedEnvironment),{capabilities::Stage::Waiting,capabilities::Reason::None}};
    }));
}
// This is only a projection of user intent for notices, never a capability's
// operational state. Session commands (FreeCamera/language) explain themselves
// in their toggle or settings help instead of a join toast.
inline bool requested(capabilities::Id id, Settings const& s) {
    using capabilities::Id;
    auto const& v = s.visuals;
    switch (id) {
    case Id::NightVision: return s.lighting.nightVision;
    case Id::EdgeGuardMovement: case Id::EdgeGuardReplay: return s.interaction.edgeGuard;
    case Id::SneakPlacement: return s.interaction.sneakPlacement;
    case Id::ConnectedTextures: return v.connectedTextures;
    case Id::BlockHighlight: return v.blockHighlight && !v.highlightBlocks.empty();
    case Id::ChatPosition: return s.information.placesChat();
    case Id::PaperDollPosition: return s.information.placesPaperDoll();
    case Id::ScoreboardPosition: return s.information.placesScoreboard();
    case Id::BossBarPosition: return s.information.placesBossBars();
    case Id::RepeatedTrades: return s.inventory.repeatTrades;
    case Id::TransferRoutes: return s.inventory.transfer;
    case Id::SkipServerPacks: return v.skipServerPacks;
    case Id::HideFog: return v.hideEffects && (v.hideWater || v.hideLava || v.hidePowderSnow || v.hideDistanceFog);
    case Id::HideWeather: return v.hideEffects && v.hideWeather;
    case Id::HideNausea: return v.hideEffects && (v.hideNausea || v.hidePowderSnow);
    case Id::HidePortal: return v.hideEffects && v.hidePortal;
    case Id::HideParticles: return v.hideEffects && v.hideParticles;
    case Id::HideBossBars: return s.information.hidesBossBars();
    // Both are used by the card's rows and by the check itself, which reads a
    // note block's data whenever the schematic is on (L-303).
    case Id::NoteBlockSounds: case Id::NoteBlockInstrument:
        return s.information.targetStates || s.schematic.enabled;
    case Id::SchematicBlocks: case Id::SchematicPiston: case Id::SchematicShulker: case Id::SchematicEntities:
    case Id::SchematicBlend: case Id::SchematicActorLight: case Id::SchematicCreativePick: return s.schematic.enabled;
    case Id::FakeOffhandTargets: return s.inventory.fakeOffhand;
    case Id::BookPastePages: return s.ui.bookPaste;
    default: return false;
    }
}
template<class Read>
std::vector<std::string_view> affected(Settings const& s, Read read) {
    std::vector<std::string_view> out;
    for (auto const& d : capabilities::definitions) {
        if (!requested(d.id,s) || !read(d.id).paused()) continue;
        bool found = false;
        for (auto feature : out) found |= feature == d.feature;
        if (!found) out.push_back(d.feature);
    }
    return out;
}
inline std::vector<std::string_view> inUse(Settings const& s) {
    return affected(s,[](capabilities::Id id) {
        return capabilities::Status{capabilities::compatibility(id,unverifiedEnvironment),{}};
    });
}
}

#pragma once
#include <array>
#include <optional>
#include <string_view>

namespace lamium::visuals {
// 16 and 32 were the carved pumpkin and spyglass frames, parked (L-79).
inline constexpr unsigned weatherBit = 1, particlesBit = 2, bossBarsBit = 4, nauseaBit = 8,
    waterBit = 64, lavaBit = 128, powderSnowBit = 256, distanceFogBit = 512;
// Effects drawn as on-screen meshes during the gameplay screen render.
inline constexpr unsigned overlayBits = nauseaBit | powderSnowBit;
inline constexpr unsigned mediumBits = waterBit | lavaBit | powderSnowBit;
struct EffectSelection {
    bool weather = false, particles = false, bossBars = false, nausea = false;
    bool water = false, lava = false, powderSnow = false, distanceFog = false;
};
inline constexpr unsigned effectMask(bool master, EffectSelection const& s) {
    if (!master) return 0;
    return (s.weather ? weatherBit : 0) | (s.particles ? particlesBit : 0) | (s.bossBars ? bossBarsBit : 0)
        | (s.nausea ? nauseaBit : 0) | (s.water ? waterBit : 0) | (s.lava ? lavaBit : 0) | (s.powderSnow ? powderSnowBit : 0)
        | (s.distanceFog ? distanceFogBit : 0);
}
inline constexpr unsigned effectMask(bool master, bool weather, bool particles, bool bossBars = false, bool nausea = false) {
    return effectMask(master, EffectSelection{weather, particles, bossBars, nausea});
}
// The effect a gameplay-screen mesh belongs to, from its exact observed
// vanilla material/resource pair; anything else is 0 and stays visible.
inline constexpr unsigned overlayMeshBit(std::string_view material, std::string_view resource) {
    if (material == "ui_texture_and_color_blur_additive" && resource == "textures/misc/nausea") return nauseaBit;
    if (material == "on_screen_effect" && resource == "textures/ui/frozen_effect") return powderSnowBit;
    return 0;
}
inline constexpr bool hideOverlayMesh(unsigned mask, bool localScreen, std::string_view material, std::string_view resource) {
    return localScreen && (mask & overlayMeshBit(material, resource) & overlayBits);
}
inline constexpr bool hideNauseaMesh(unsigned mask, bool localScreen, std::string_view material, std::string_view resource) {
    return localScreen && (mask & nauseaBit) && overlayMeshBit(material, resource) == nauseaBit;
}
// The renderer's camera-medium flags, as fog setup reads them. Hiding a
// medium makes fog setup see the camera outside it; the flags are restored
// right after, so nothing else observes the change.
struct CameraMedium {
    bool water = false, liquid = false, lava = false, powderSnow = false;
    constexpr bool operator==(CameraMedium const&) const = default;
};
inline constexpr CameraMedium visibleMedium(CameraMedium medium, unsigned mask) {
    CameraMedium out = medium;
    if (mask & waterBit) out.water = false;
    if (mask & lavaBit) out.lava = false;
    if (mask & powderSnowBit) out.powderSnow = false;
    // "Liquid" means water or lava; keep it only while one of them stays.
    if ((medium.water && !out.water) || (medium.lava && !out.lava)) out.liquid = medium.liquid && (out.water || out.lava);
    return out;
}
// Distance fog is the air or weather fog left once no medium fog is shown
// (land, Nether, End). It moves far beyond any render distance; a finite value
// keeps the shaders' fog division well defined (L-118).
inline constexpr bool hidesDistanceFog(unsigned mask, CameraMedium shown) {
    return (mask & distanceFogBit) && !shown.water && !shown.lava && !shown.powderSnow;
}
struct FogRange {
    float start = 0, end = 0;
    constexpr bool operator==(FogRange const&) const = default;
};
inline constexpr float farFogStart = 16384, farFogEnd = 32768;
// Unreadable values stay vanilla; fog already farther away is left alone.
inline constexpr std::optional<FogRange> farFog(FogRange vanilla) {
    auto finite = [](float v) { return v == v && v > -3.0e38f && v < 3.0e38f; };
    if (!finite(vanilla.start) || !finite(vanilla.end) || vanilla.end >= farFogEnd) return std::nullopt;
    return FogRange{vanilla.start > farFogStart ? vanilla.start : farFogStart, farFogEnd};
}
class BossBarRoute {
    bool healthPanel = false, hudPanel = false;
public:
    bool visit(std::string_view name) {
        healthPanel = healthPanel || name == "boss_health_panel";
        hudPanel = hudPanel || name == "boss_hud_panel";
        return name == "hud_screen" && healthPanel && hudPanel;
    }
};
inline constexpr bool hideParticle(unsigned mask, bool rainSplash) {
    return (mask & particlesBit) || (rainSplash && (mask & weatherBit));
}
inline constexpr bool rainEffectMatches(std::string_view learned, std::string_view candidate) {
    return !learned.empty() && learned.size() <= 192 && learned == candidate;
}
inline constexpr std::array<bool, 7> hiddenWeatherLayers(bool weather, bool particles) {
    return {weather, weather, particles, particles, particles, particles, particles};
}
}

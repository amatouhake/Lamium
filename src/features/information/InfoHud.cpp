#include "features/information/InfoHud.h"
#include "features/information/PlayerInfo.h"
#include "features/information/FrameTiming.h"
#include "features/information/NetworkInfo.h"
#include "features/information/TargetInfo.h"
#include "features/information/TargetRows.h"
#include "features/information/DebugView.h"
#include "features/interaction/BreakingRestriction.h"
#include "features/interaction/PeriodicInput.h"
#include "features/interaction/PermanentSneak.h"
#include "app/Runtime.h"
#include "ui/HudElement.h"
#include "ui/Toast.h"
#include "ui/Widgets.h"
#include "ui/Localization.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/client/game/IClientInstance.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace lamium::ui {
namespace {
Toast activeToast;
}
void showToggleToast(std::string feature, bool on) { activeToast.show(std::move(feature), on, toastNow()); }
std::optional<Toast::Visible> currentToggleToast(double now) { return activeToast.current(now); }
}
namespace lamium::information {
namespace {
SpeedSampler speedSampler;
// One element row: text with an optional leading marker square.
struct ElementLine { std::string text; std::optional<ui::Rgb> marker; };
float elementZoom(ui::HudElement const& element) {
    return std::clamp(std::isfinite(element.scale) ? element.scale : 100.f, 75.f, 150.f) / 100;
}
// Draw lines through the element model: card background, shadow, scale.
void drawElement(MinecraftUIRenderContext& context, float width, float height, ui::HudElement const& element,
                 std::vector<ElementLine> const& lines) {
    if (lines.empty()) return;
    float zoom = elementZoom(element);
    float rowHeight = 14 * zoom;
    float contentWidth = 0;
    std::vector<float> textWidths;
    textWidths.reserve(lines.size());
    for (auto const& line : lines) {
        float textWidth = ui::textWidthScaled(context, line.text, zoom);
        textWidths.push_back(textWidth);
        contentWidth = std::max(contentWidth, (line.marker ? 8 + 4 : 0) + textWidth);
    }
    contentWidth = std::min(contentWidth, 230 * zoom);
    float padX = element.background == ui::ElementBackground::Card ? 5 : 0;
    float padY = element.background == ui::ElementBackground::Card ? 3 : 0;
    float boxWidth = contentWidth + 2 * padX, boxHeight = static_cast<float>(lines.size()) * rowHeight + 2 * padY;
    auto placement = ui::placeElement(width, height, boxWidth, boxHeight, element);
    if (element.background == ui::ElementBackground::Card)
        ui::panel(context, placement.x, placement.y, boxWidth, boxHeight);
    for (size_t i = 0; i < lines.size(); ++i) {
        float x = placement.x + padX, y = placement.y + padY + i * rowHeight;
        float textX = x;
        if (lines[i].marker) {
            float markerY = y + (rowHeight - 8 * zoom) / 2;
            ui::fill(context, x, markerY, 8 * zoom, 8 * zoom, *lines[i].marker);
            textX += 8 * zoom + 4;
        }
        float textWidth = std::min(textWidths[i], contentWidth - (textX - x - padX));
        if (element.shadow)
            ui::labelScaled(context, textX + zoom, y + zoom, textWidth + 2, lines[i].text, zoom, {0, 0, 0});
        ui::labelScaled(context, textX, y, textWidth + 2, lines[i].text, zoom);
    }
    context.flushText(0, std::nullopt);
}
bool infoLineEnabled(Settings::Information const& settings, std::string_view id) {
    if (id == "coordinates") return settings.coordinates;
    if (id == "dimension") return settings.dimension;
    if (id == "biome") return settings.biome;
    if (id == "facing") return settings.facing;
    if (id == "fps") return settings.fps;
    if (id == "frameTime") return settings.frameTime;
    if (id == "light") return settings.light;
    if (id == "ping") return settings.ping;
    if (id == "rotation") return settings.rotation;
    if (id == "block") return settings.block;
    if (id == "chunk") return settings.chunk;
    if (id == "speed") return settings.speed;
    if (id == "time") return settings.time;
    if (id == "weather") return settings.weather;
    if (id == "moon") return settings.moon;
    return false;
}
std::optional<std::string> infoLineText(std::string_view id, PlayerInfo const& info,
                                        std::optional<FrameStatistics> timing, std::optional<std::int64_t> ping,
                                        std::optional<double> speed) {
    if (id == "coordinates") {
        if (info.position) {
            auto const& p = *info.position;
            return ui::translated("hudXYZ", p.x, p.y, p.z);
        }
        return ui::translated("hudCoordinates", ui::translated("unavailable"));
    }
    if (id == "dimension")
        return ui::translated("hudDimensionValue", info.dimension.value_or(ui::translated("unavailable")));
    if (id == "biome") return ui::translated("hudBiome", info.biome.value_or(ui::translated("unavailable")));
    if (id == "facing") {
        auto key = info.yaw ? facingKey(*info.yaw) : std::nullopt;
        return ui::translated("hudFacing", key ? ui::translated(*key) : ui::translated("unavailable"));
    }
    if (id == "fps")
        return ui::translated("hudFps", timing ? std::format("{:.0f}", timing->fps) : ui::translated("unavailable"));
    if (id == "frameTime")
        return ui::translated("hudFrameTime",
            timing ? std::format("{:.1f} ms", timing->milliseconds) : ui::translated("unavailable"));
    if (id == "light")
        return ui::translated("hudLight", info.light ? ui::translated("hudLightValues", info.light->sky, info.light->block)
                                                     : ui::translated("unavailable"));
    if (id == "ping") return ui::translated("hudPing", ping ? std::format("{} ms", *ping) : ui::translated("unavailable"));
    if (id == "rotation")
        return ui::translated("hudRotation", info.yaw && info.pitch ? formatRotation(*info.yaw, *info.pitch)
                                                                      : ui::translated("unavailable"));
    if (id == "block") {
        if (!info.position) {
            auto na = ui::translated("unavailable");
            return ui::translated("hudBlock", na, na, na);
        }
        auto const& p = *info.position;
        return ui::translated("hudBlock", static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y)),
            static_cast<int>(std::floor(p.z)));
    }
    if (id == "chunk") {
        if (!info.position) return ui::translated("hudChunk", ui::translated("unavailable"));
        return ui::translated("hudChunk", formatChunk(chunkPosition(info.position->x, info.position->z)));
    }
    if (id == "speed")
        return ui::translated("hudSpeed", speed ? formatSpeed(*speed) : ui::translated("unavailable"));
    if (id == "time") {
        if (!info.worldTime) return ui::translated("hudTime", ui::translated("unavailable"), "");
        return ui::translated("hudTime", dayCount(*info.worldTime), formatClock(*info.worldTime));
    }
    if (id == "weather") {
        if (!info.raining) return ui::translated("hudWeather", ui::translated("unavailable"));
        return ui::translated("hudWeather", ui::translated(*info.raining ? "weatherRain" : "weatherClear"));
    }
    if (id == "moon") {
        if (!info.worldTime) return ui::translated("hudMoon", ui::translated("unavailable"));
        return ui::translated("hudMoon", ui::translated(moonPhaseKey(moonPhase(*info.worldTime))));
    }
    return {};
}
}
void drawHud(MinecraftUIRenderContext& context, float width, float height, Settings::Information const& preferences) {
    auto settings = debugProfile(preferences);
    auto const& runtime = Runtime::instance().preferences();
    if (runtime.ui.automationStatus || runtime.interaction.breaking) {
        std::vector<ElementLine> lines;
        if (runtime.ui.automationStatus) {
            if (interaction::periodic::active(context.mClient, interaction::periodic::Action::Attack))
                lines.push_back({ui::translated("status.periodicAttack"), ui::palette::accent});
            if (interaction::periodic::active(context.mClient, interaction::periodic::Action::Use))
                lines.push_back({ui::translated("status.periodicUse"), ui::palette::accent});
            if (interaction::sneak::active(context.mClient))
                lines.push_back({ui::translated("status.permanentSneak"), ui::palette::accent});
        }
        if (runtime.interaction.breaking && context.mClient.getLocalPlayer()) {
            auto mode = runtime.interaction.breakingMode;
            auto anchor = interaction::breaking::region();
            auto modeText = ui::translated("breakingMode", ui::translated(interaction::restrictionLabels[static_cast<size_t>(mode)]));
            if (anchor) {
                auto axis = anchor->effectiveAxis();
                modeText += " | " + ui::translated("restrictionAxis",
                    axis == interaction::Axis::X ? "X" : axis == interaction::Axis::Y ? "Y" : "Z");
            }
            auto text = anchor ? ui::translated("breakingAnchor",
                                    std::format("{}, {}, {}", anchor->anchor.x, anchor->anchor.y, anchor->anchor.z))
                               : ui::translated("breakingNeedsAnchor");
            lines.push_back({std::move(text), ui::palette::warning});
            lines.push_back({std::move(modeText), ui::palette::warning});
        }
        drawElement(context, width, height, runtime.hud.status, lines);
    }
    if (settings.target) {
        if (auto target = collectTargetInfo(context.mClient, settings.targetStates)) {
            std::string coordinates;
            if (settings.targetCoordinates && target->blockPosition) {
                auto const& p = *target->blockPosition;
                coordinates = ui::translated("targetBlockPosition", p.x, p.y, p.z);
            }
            int capacity = std::max(1, std::min(10, static_cast<int>((height - 8) / 14)));
            auto rows = targetRows(*target, settings.targetIdentifier, capacity, coordinates);
            auto& targetLines = rows.lines;
            if (rows.showOmitted) targetLines.push_back(ui::translated("targetMore", std::to_string(rows.omittedStates)));
            std::vector<ElementLine> lines;
            for (auto& line : targetLines) lines.push_back({std::move(line), {}});
            drawElement(context, width, height, runtime.hud.target, lines);
        }
    }
    if (runtime.ui.toggleToasts) {
        if (auto toast = ui::currentToggleToast(ui::toastNow())) {
            float zoom = elementZoom(runtime.hud.toast);
            float textWidth = ui::textWidthScaled(context, toast->text, zoom);
            float total = ui::switchWidth + 6 + textWidth;
            auto placement = ui::placeElement(width, height, total, 14 * zoom, runtime.hud.toast);
            ui::toggleSwitch(context, placement.x, placement.y + (14 * zoom - ui::switchHeight) / 2, toast->on);
            if (runtime.hud.toast.shadow)
                ui::labelScaled(context, placement.x + ui::switchWidth + 6 + zoom, placement.y + zoom,
                    textWidth + 2, std::string(toast->text), zoom, {0, 0, 0});
            ui::labelScaled(context, placement.x + ui::switchWidth + 6, placement.y, textWidth + 2,
                std::string(toast->text), zoom, toast->opacity < 1 ? ui::palette::dim : ui::palette::text);
            context.flushText(0, std::nullopt);
        }
    }
    if (!settings.hud) return;
    auto info = collectPlayerInfo(context.mClient,
        {settings.coordinates || settings.block || settings.chunk || settings.speed, settings.dimension,
         settings.biome, settings.facing, settings.light, settings.rotation, settings.time || settings.moon,
         settings.weather});
    if (!info.present) return;
    auto timing = (settings.fps || settings.frameTime) ? frameStatistics() : std::optional<FrameStatistics>{};
    auto ping = settings.ping ? connectionPing(context.mClient) : std::optional<std::int64_t>{};
    if (settings.speed && info.position)
        speedSampler.sample(info.position->x, info.position->y, info.position->z, ui::toastNow());
    auto speed = settings.speed ? speedSampler.read() : std::optional<double>{};
    std::vector<ElementLine> lines;
    int capacity = std::max(1, static_cast<int>((height - 8) / (14 * elementZoom(runtime.hud.info))));
    for (auto const& id : settings.lineOrder) {
        if (static_cast<int>(lines.size()) >= capacity) break;
        if (!infoLineEnabled(settings, id)) continue;
        if (auto text = infoLineText(id, info, timing, ping, speed)) lines.push_back({std::move(*text), {}});
    }
    drawElement(context, width, height, runtime.hud.info, lines);
}
}

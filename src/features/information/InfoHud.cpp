#include "features/information/InfoHud.h"
#include "features/information/PlayerInfo.h"
#include "features/information/FrameTiming.h"
#include "features/information/NetworkInfo.h"
#include "features/information/TargetInfo.h"
#include "features/information/TargetCard.h"
#include "features/information/DebugView.h"
#include "features/camera/Zoom.h"
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
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/actor/ItemRenderer.h"
#include "mc/world/item/ItemStack.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/client/player/LocalPlayer.h"
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
std::optional<ui::hud_editor::Box> drawElement(MinecraftUIRenderContext& context, float width, float height,
                                               ui::HudElement const& element, std::vector<ElementLine> const& lines) {
    if (lines.empty()) return std::nullopt;
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
        ui::card(context, placement.x, placement.y, boxWidth, boxHeight);
    for (size_t i = 0; i < lines.size(); ++i) {
        float x = placement.x + padX, y = placement.y + padY + i * rowHeight;
        float textX = x;
        if (lines[i].marker) {
            float markerY = y + (rowHeight - 8 * zoom) / 2;
            ui::fill(context, x, markerY, 8 * zoom, 8 * zoom, *lines[i].marker);
            textX += 8 * zoom + 4;
        }
        float textWidth = std::min(textWidths[i], contentWidth - (textX - x - padX));
        ui::labelScaled(context, textX, y, textWidth + 2, lines[i].text, zoom, ui::palette::text, ui::Align::Left,
            element.shadow);
    }
    context.flushText(0, std::nullopt);
    return ui::hud_editor::Box{placement.x, placement.y, boxWidth, boxHeight};
}
// ---- Target card ----
// Hearts use the game's own health-bar sprites (9x9, overlapping by one).
void heartRow(MinecraftUIRenderContext& context, float x, float y, float unit, std::array<Heart, 10> const& icons) {
    std::vector<ui::ImageRect> backs, fulls, halves;
    for (int h = 0; h < 10; ++h) {
        ui::ImageRect r{x + h * 8 * unit, y, 9 * unit, 9 * unit};
        backs.push_back(r);
        if (icons[h] == Heart::Full) fulls.push_back(r);
        else if (icons[h] == Heart::Half) halves.push_back(r);
    }
    ui::images(context, "textures/ui/heart_background", backs);
    ui::images(context, "textures/ui/heart", fulls);
    ui::images(context, "textures/ui/heart_half", halves);
}
bool animationsOn(IClientInstance& client) {
    auto mode = Runtime::instance().preferences().ui.animations;
    if (mode == 1) return true;
    if (mode == 2) return false;
    try { return client.getOptions().getScreenAnimations(); } catch (...) { return true; }
}
struct CardMorph {
    std::string identity;
    std::optional<ui::hud_editor::Box> shown, from;
    double start = 0;
};
CardMorph cardMorph;
ItemStack iconStack(TargetInfo const& target) {
    ItemStack stack;
    if (!target.iconItem.empty()) {
        try { stack.reinit(target.iconItem, 1, target.iconAux); } catch (...) { stack = ItemStack(); }
    }
    // A fresh stack counts as just picked up, and the renderer would keep
    // playing the pickup squash on it.
    stack.mShowPickUp = false;
    stack.mWasPickedUp = false;
    return stack;
}
std::optional<ui::hud_editor::Box> drawTargetCard(MinecraftUIRenderContext& context, float width, float height,
    ui::HudElement const& element, TargetInfo const& target, Settings::Information const& settings, bool animate) {
    float z = elementZoom(element);
    bool card = element.background == ui::ElementBackground::Card;
    float padX = card ? 5 * z : 0, padY = card ? 4 * z : 0;
    CardOptions options;
    options.details = settings.targetStates;
    options.coordinates = settings.targetCoordinates;
    options.health = settings.targetHealth == 1 ? Meter::Bar : settings.targetHealth == 2 ? Meter::Number : Meter::Hearts;
    options.growth = settings.targetGrowth == 1 ? Meter::Number : Meter::Bar;
    int capacity = std::max(0, static_cast<int>((height - 40) / (12 * z)));
    auto rows = cardRows(target, options, static_cast<size_t>(std::min(capacity, 10)));
    auto stack = settings.targetIcon ? iconStack(target) : ItemStack();
    bool icon = !stack.isNull();
    float iconSize = icon ? 16 * z : 0, iconGap = icon ? 5 * z : 0;
    float lineH = 11 * z, rowH = 12 * z;
    // Measure.
    std::vector<std::string> labels, values;
    float labelW = 0, valuesW = 0;
    constexpr float barUnits = 48;
    for (auto const& row : rows) {
        labels.push_back(row.labelIsKey ? ui::translated(row.label) : row.label);
        values.push_back(row.valueIsKey ? ui::translated(row.value) : row.value);
        labelW = std::max(labelW, ui::textWidthScaled(context, labels.back(), z));
        float valueW = ui::textWidthScaled(context, values.back(), z);
        if (row.progress && row.meter == Meter::Bar) valueW += (barUnits + 4) * z;
        if (row.progress && row.meter == Meter::Hearts) valueW += (10 * 8 + 1 + 4) * z;
        valuesW = std::max(valuesW, valueW);
    }
    float nameW = ui::textWidthScaled(context, target.name, z);
    float idW = settings.targetIdentifier ? ui::textWidthScaled(context, target.identifier, z) : 0;
    float headerTextH = (settings.targetIdentifier ? 2 : 1) * lineH;
    float headerH = std::max(iconSize, headerTextH);
    float headerW = iconSize + iconGap + std::max(nameW, idW);
    float rowsW = rows.empty() ? 0 : labelW + 6 * z + valuesW;
    float contentW = std::min(std::max(headerW, rowsW), 260 * z);
    float boxW = contentW + 2 * padX;
    float boxH = headerH + (rows.empty() ? 0 : 3 * z + rows.size() * rowH) + 2 * padY;
    auto placement = ui::placeElement(width, height, boxW, boxH, element);
    ui::hud_editor::Box finalBox{placement.x, placement.y, boxW, boxH};
    // Ease the card between targets; the content appears once it settles.
    auto identity = target.identifier + "|" + target.name;
    std::optional<ui::hud_editor::Box> background = finalBox;
    if (animate && card && animationsOn(context.mClient)) {
        double now = ui::toastNow();
        if (identity != cardMorph.identity) {
            cardMorph.from = cardMorph.shown;
            cardMorph.start = now;
            cardMorph.identity = identity;
        }
        float t = cardMorph.from ? morphProgress(now - cardMorph.start) : 1.f;
        if (t < 1) {
            auto const& a = *cardMorph.from;
            background = ui::hud_editor::Box{a.x + (finalBox.x - a.x) * t, a.y + (finalBox.y - a.y) * t,
                                             a.w + (finalBox.w - a.w) * t, a.h + (finalBox.h - a.h) * t};
        } else cardMorph.from.reset();
        cardMorph.shown = background;
    }
    if (card) ui::card(context, background->x, background->y, background->w, background->h);
    float left = finalBox.x + padX, top = finalBox.y + padY;
    if (icon) {
        if (auto* renderer = context.mClient.getItemRenderer()) {
            BaseActorRenderContext renderContext(context.mScreenContext, context.mClient,
                                                 context.mClient.getMinecraftGame_DEPRECATED());
            renderer->renderGuiItemNew(renderContext, stack, 0, left, top + (headerH - iconSize) / 2, false, 1.f, 1.f, z, 17);
        }
    }
    float textX = left + iconSize + iconGap, textW = contentW - iconSize - iconGap;
    float textTop = top + (headerH - headerTextH) / 2;
    ui::labelScaled(context, textX, textTop, textW + 2, target.name, z, ui::palette::text, ui::Align::Left, element.shadow);
    if (settings.targetIdentifier)
        ui::labelScaled(context, textX, textTop + lineH, textW + 2, target.identifier, z, ui::palette::faint,
                        ui::Align::Left, element.shadow);
    float y = top + headerH + 3 * z;
    float valueX = left + labelW + 6 * z;
    for (size_t i = 0; i < rows.size(); ++i, y += rowH) {
        auto const& row = rows[i];
        ui::labelScaled(context, left, y, labelW + 2, labels[i], z, ui::palette::faint, ui::Align::Left, element.shadow);
        float x = valueX;
        if (row.progress && row.meter == Meter::Bar) {
            float barY = y + 3 * z, barH = 5 * z, barW = barUnits * z;
            ui::fill(context, x, barY, barW, barH, ui::palette::white, .12f);
            bool health = row.label == "target.health";
            ui::fill(context, x, barY, barW * std::clamp(*row.progress, 0.f, 1.f), barH,
                     health ? ui::palette::heart : ui::palette::accent);
            x += barW + 4 * z;
        } else if (row.progress && row.meter == Meter::Hearts) {
            heartRow(context, x, y + 1 * z, z, hearts(*row.progress));
            x += (10 * 8 + 1 + 4) * z;
        }
        ui::labelScaled(context, x, y, left + contentW - x + 2, values[i], z, ui::palette::text, ui::Align::Left,
                        element.shadow);
    }
    context.flushText(0, std::nullopt);
    return finalBox;
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
ui::hud_editor::Boxes drawHud(MinecraftUIRenderContext& context, float width, float height,
                              Settings::Information const& preferences, HudPreview const* preview) {
    ui::hud_editor::Boxes boxes;
    auto box = [&](ui::HudElementId id) -> auto& { return boxes[static_cast<size_t>(id)]; };
    auto settings = debugProfile(preferences);
    auto const& runtime = Runtime::instance().preferences();
    auto const& hud = preview ? preview->layout : runtime.hud;
    if (preview || runtime.ui.automationStatus || runtime.interaction.breaking) {
        std::vector<ElementLine> lines;
        if (runtime.ui.automationStatus) {
            // One line per switched-on button: "Auto Attack: Periodic", marked
            // when gameplay input is not Lamium's to drive right now.
            bool paused = interaction::periodic::paused(context.mClient);
            auto const& value = runtime.interaction;
            for (auto [on, mode, trigger, feature] : {
                     std::tuple{value.autoAttack, value.attackMode, value.attackTrigger, "feature.periodicAttack"},
                     std::tuple{value.autoUse, value.useMode, value.useTrigger, "feature.periodicUse"}}) {
                if (!on) continue;
                auto text = ui::translated(feature) + ": " + interaction::autoModeText(mode, trigger);
                if (paused) text += " " + ui::translated("autoPaused");
                lines.push_back({std::move(text), paused ? ui::palette::dim : ui::palette::accent});
            }
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
        if (preview && lines.empty()) lines.push_back({ui::translated("status.permanentSneak"), ui::palette::accent});
        box(ui::HudElementId::Status) = drawElement(context, width, height, hud.status, lines);
    }
    if (preview || settings.target) {
        // One distance for every viewpoint: the body normally, the camera
        // during Freelook and FreeCamera (it looks elsewhere than the body).
        std::optional<ViewRay> ray;
        if (auto* player = context.mClient.getLocalPlayer()) {
            double reach = settings.targetDistance;
            if (auto view = Zoom::instance().detachedViewRay(context.mClient)) {
                ray = ViewRay{view->x, view->y, view->z, view->dx, view->dy, view->dz, reach};
            } else {
                auto eye = player->getEyePos();
                auto direction = player->getViewVector();
                ray = ViewRay{eye.x, eye.y, eye.z, direction.x, direction.y, direction.z, reach};
            }
        }
        auto target = collectTargetInfo(context.mClient, true, ray);
        if (!target && preview) {
            TargetInfo sample{ui::translated("feature.targetInfo"), "minecraft:grass_block", "minecraft:grass_block"};
            box(ui::HudElementId::Target) = drawTargetCard(context, width, height, hud.target, sample, settings, false);
        }
        if (target) box(ui::HudElementId::Target) = drawTargetCard(context, width, height, hud.target, *target, settings, !preview);
        else if (!preview) cardMorph = {};
    }
    if (preview || runtime.ui.toggleToasts) {
        auto toast = ui::currentToggleToast(ui::toastNow());
        if (!toast && preview) toast = ui::Toast::Visible{ui::translated("feature.toolSwitch"), true, 1.f};
        if (toast) {
            float zoom = elementZoom(hud.toast);
            float textWidth = ui::textWidthScaled(context, toast->text, zoom);
            bool card = hud.toast.background == ui::ElementBackground::Card;
            float padX = card ? 6 : 0, padY = card ? 3 : 0;
            float total = ui::switchWidth + 6 + textWidth + 2 * padX;
            auto frame = ui::placeElement(width, height, total, 14 * zoom + 2 * padY, hud.toast);
            if (card) ui::card(context, frame.x, frame.y, total, 14 * zoom + 2 * padY, .72f * toast->opacity);
            box(ui::HudElementId::Toast) = ui::hud_editor::Box{frame.x, frame.y, total, 14 * zoom + 2 * padY};
            ui::ElementPlacement placement{frame.x + padX, frame.y + padY};
            ui::toggleSwitch(context, placement.x, placement.y + (14 * zoom - ui::switchHeight) / 2, toast->on);
            ui::labelScaled(context, placement.x + ui::switchWidth + 6, placement.y, textWidth + 2,
                std::string(toast->text), zoom, toast->opacity < 1 ? ui::palette::dim : ui::palette::text,
                ui::Align::Left, hud.toast.shadow);
            context.flushText(0, std::nullopt);
        }
    }
    if (!preview && !settings.hud) return boxes;
    auto info = collectPlayerInfo(context.mClient,
        {settings.coordinates || settings.block || settings.chunk || settings.speed, settings.dimension,
         settings.biome, settings.facing, settings.light, settings.rotation, settings.time || settings.moon,
         settings.weather});
    if (!info.present) return boxes;
    auto timing = (settings.fps || settings.frameTime) ? frameStatistics() : std::optional<FrameStatistics>{};
    auto ping = settings.ping ? connectionPing(context.mClient) : std::optional<std::int64_t>{};
    if (settings.speed && info.position)
        speedSampler.sample(info.position->x, info.position->y, info.position->z, ui::toastNow());
    auto speed = settings.speed ? speedSampler.read() : std::optional<double>{};
    std::vector<ElementLine> lines;
    int capacity = std::max(1, static_cast<int>((height - 8) / (14 * elementZoom(hud.info))));
    for (auto const& id : settings.lineOrder) {
        if (static_cast<int>(lines.size()) >= capacity) break;
        if (!infoLineEnabled(settings, id)) continue;
        if (auto text = infoLineText(id, info, timing, ping, speed)) lines.push_back({std::move(*text), {}});
    }
    if (preview && lines.empty()) lines.push_back({ui::translated("feature.infoHud"), {}});
    box(ui::HudElementId::Info) = drawElement(context, width, height, hud.info, lines);
    return boxes;
}
}

#include "features/information/InfoHud.h"
#include "features/information/PlayerInfo.h"
#include "features/information/FrameTiming.h"
#include "features/information/NetworkInfo.h"
#include "features/information/TargetInfo.h"
#include "features/information/TargetRows.h"
#include "features/information/DebugView.h"
#include "features/interaction/BreakingRestriction.h"
#include "app/Runtime.h"
#include "ui/HudLayout.h"
#include "ui/Widgets.h"
#include "ui/Localization.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/client/game/IClientInstance.h"
#include <vector>

namespace lamium::information {
void drawHud(MinecraftUIRenderContext& context, float width, float height, Settings::Information const& preferences) {
    auto settings = debugProfile(preferences);
    if (Runtime::instance().preferences().interaction.breaking && context.mClient.getLocalPlayer()) {
        auto anchor = interaction::breaking::region();
        auto text = anchor ? ui::translated("breakingAnchor",std::format("{}, {}, {}",anchor->anchor.x,anchor->anchor.y,anchor->anchor.z))
                           : ui::translated("breakingNeedsAnchor");
        auto layout = ui::HudLayout::fit(width,height,100,100,1);
        if (layout.lines) { ui::label(context,layout.x,layout.y,layout.width,text); context.flushText(0,std::nullopt); }
    }
    float columnWidth = settings.debug ? std::min(230.f,width/2-8) : 230.f;
    if (settings.target) {
        if (auto target = collectTargetInfo(context.mClient,settings.targetStates)) {
            auto capacity = ui::HudLayout::fit(width,height,settings.targetHorizontal,settings.targetVertical,9,columnWidth).lines;
            auto rows = targetRows(*target,settings.targetIdentifier,capacity);
            auto& targetLines = rows.lines;
            if (rows.showOmitted) targetLines.push_back(ui::translated("targetMore",std::to_string(rows.omittedStates)));
            auto layout = ui::HudLayout::fit(width,height,settings.targetHorizontal,settings.targetVertical,static_cast<int>(targetLines.size()),columnWidth);
            for (int i=0;i<layout.lines;++i) ui::label(context,layout.x,layout.y+i*14,layout.width,targetLines[i]);
            if (layout.lines) context.flushText(0,std::nullopt);
        }
    }
    if (!settings.hud) return;
    auto info = collectPlayerInfo(context.mClient,{settings.coordinates,settings.dimension,settings.biome,settings.facing,settings.light});
    if (!info.present) return;
    std::vector<std::string> lines;
    if (settings.coordinates) {
        if (info.position) {
            auto const& p = *info.position;
            lines.push_back(ui::translated("hudXYZ",p.x,p.y,p.z));
        } else lines.push_back(ui::translated("hudCoordinates",ui::translated("unavailable")));
    }
    if (settings.dimension)
        lines.push_back(ui::translated("hudDimensionValue",info.dimension.value_or(ui::translated("unavailable"))));
    if (settings.biome)
        lines.push_back(ui::translated("hudBiome",info.biome.value_or(ui::translated("unavailable"))));
    if (settings.facing) {
        auto key = info.yaw ? facingKey(*info.yaw) : std::nullopt;
        lines.push_back(ui::translated("hudFacing",key ? ui::translated(*key) : ui::translated("unavailable")));
    }
    if (settings.fps || settings.frameTime) {
        auto timing = frameStatistics();
        if (settings.fps) lines.push_back(ui::translated("hudFps", timing ? std::format("{:.0f}",timing->fps) : ui::translated("unavailable")));
        if (settings.frameTime) lines.push_back(ui::translated("hudFrameTime", timing ? std::format("{:.1f} ms",timing->milliseconds) : ui::translated("unavailable")));
    }
    if (settings.light) lines.push_back(ui::translated("hudLight", info.light
        ? ui::translated("hudLightValues",info.light->sky,info.light->block) : ui::translated("unavailable")));
    if (settings.ping) {
        auto ping = connectionPing(context.mClient);
        lines.push_back(ui::translated("hudPing",ping ? std::format("{} ms",*ping) : ui::translated("unavailable")));
    }
    auto layout = ui::HudLayout::fit(width,height,settings.horizontal,settings.vertical,static_cast<int>(lines.size()),columnWidth);
    for (int i=0;i<layout.lines;++i) ui::label(context,layout.x,layout.y+i*14,layout.width,lines[i]);
    if (layout.lines) context.flushText(0,std::nullopt);
}
}

#include "features/information/InfoHud.h"
#include "features/information/PlayerInfo.h"
#include "features/information/FrameTiming.h"
#include "features/information/NetworkInfo.h"
#include "features/information/TargetInfo.h"
#include "ui/HudLayout.h"
#include "ui/Widgets.h"
#include "ui/Localization.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/client/game/IClientInstance.h"
#include <vector>

namespace lamium::information {
void drawHud(MinecraftUIRenderContext& context, float width, float height, Settings::Information const& settings) {
    if (settings.target) {
        if (auto target = collectTargetInfo(context.mClient)) {
            auto layout = ui::HudLayout::fit(width,height,50,2,settings.targetIdentifier ? 2 : 1);
            if (layout.lines > 0) ui::label(context,layout.x,layout.y,layout.width,target->name);
            if (layout.lines > 1) ui::label(context,layout.x,layout.y+14,layout.width,target->identifier);
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
    auto layout = ui::HudLayout::fit(width,height,settings.horizontal,settings.vertical,static_cast<int>(lines.size()));
    for (int i=0;i<layout.lines;++i) ui::label(context,layout.x,layout.y+i*14,layout.width,lines[i]);
    if (layout.lines) context.flushText(0,std::nullopt);
}
}

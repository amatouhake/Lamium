#include "features/information/InfoHud.h"
#include "ui/HudLayout.h"
#include "ui/Widgets.h"
#include "ui/Localization.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/dimension/Dimension.h"
#include <vector>

namespace lamium::information {
void drawHud(MinecraftUIRenderContext& context, float width, float height, Settings::Information const& settings) {
    if (!settings.hud) return;
    auto* player = context.mClient.getLocalPlayer();
    if (!player) return;
    std::vector<std::string> lines;
    if (settings.coordinates) {
        auto const& p = player->getPosition();
        if (std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z))
            lines.push_back(ui::translated("hudXYZ",p.x,p.y,p.z));
    }
    if (settings.dimension)
        lines.push_back(ui::translated("hudDimensionValue",player->getDimension().mName.get()));
    auto layout = ui::HudLayout::fit(width,height,settings.horizontal,settings.vertical,static_cast<int>(lines.size()));
    for (int i=0;i<layout.lines;++i) ui::label(context,layout.x,layout.y+i*14,layout.width,lines[i]);
    if (layout.lines) context.flushText(0,std::nullopt);
}
}

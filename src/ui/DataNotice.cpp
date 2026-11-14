#include "ui/DataNotice.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/SavedDataLayout.h"
#include "ui/Toast.h"
#include "ui/Widgets.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include <chrono>

namespace lamium::ui::data_notice {
namespace {
struct Button { float x = 0, y = 0, w = 0; };
struct Drawn {
    double at = -1;
    float x = 0, y = 0, w = 0, h = 0;
    Button use, open, never;
    std::string key, name;
} drawn;
constexpr float buttonHeight = 12, pad = 5;
double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
bool over(Button const& b, float x, float y) { return b.w > 0 && x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + buttonHeight; }
}
float height(Kind kind) { return map::waypoints::fullerData(kind) ? bandHeight : 0; }
void draw(MinecraftUIRenderContext& context, float x, float y, float width, Kind kind, glm::vec2 pointer, bool solid) {
    auto data = map::waypoints::fullerData(kind);
    if (!data || width < 120) return;
    float const h = bandHeight - 4;
    fill(context, x, y, width, h, solid ? palette::panel : palette::warning, solid ? .96f : .08f);
    frame(context, x, y, width, h, palette::warning);
    std::string const what = kind == Kind::Waypoints ? translated("savedData.waypoints", data->waypoints)
        : kind == Kind::Placements ? translated("savedData.placements", data->placements)
        : translated("savedData.map", sizeText(data->mapBytes, data->mapMore));
    std::string const name = data->name.empty() ? translated("savedData.unnamed") : data->name;
    paragraph(context, x + pad, y + 3, width - 2 * pad, translated("savedData.notice", name, what), 2, palette::dim);
    drawn = {now(), x, y, width, h, {}, {}, {}, data->key, name};
    float bx = x + pad;
    float const by = y + h - buttonHeight - 3;
    auto button = [&](Button& slot, std::string text, bool primary) {
        float const w = textWidth(context, text) + 10;
        if (bx + w > x + width - pad) return;
        slot = {bx, by, w};
        bool const hovered = over(slot, pointer.x, pointer.y);
        if (primary) drawSmallButton(context, bx, by, w, buttonHeight, std::move(text), hovered, palette::accentDeep, palette::accent);
        else drawSmallButton(context, bx, by, w, buttonHeight, std::move(text), hovered);
        bx += w + 4;
    };
    button(drawn.use, translated("savedData.use"), true);
    button(drawn.open, translated("savedData.noticeOpen"), false);
    button(drawn.never, translated("savedData.noticeNever"), false);
}
Pressed press(float x, float y) {
    // Only the notice of this frame or the last: a screen that stopped drawing it takes its clicks back.
    if (drawn.at < 0 || now() - drawn.at > .5 || x < drawn.x || x >= drawn.x + drawn.w || y < drawn.y || y >= drawn.y + drawn.h)
        return Pressed::Outside;
    if (over(drawn.use, x, y)) {
        bool const used = map::waypoints::useData(drawn.key);
        showMessageToast(translated(used ? "savedData.switched" : "savedData.failed", drawn.name));
        drawn.at = -1;
        return Pressed::Inside;
    }
    if (over(drawn.never, x, y)) {
        map::waypoints::quietFullerData();
        showMessageToast(translated("savedData.noticeQuiet"));
        drawn.at = -1;
        return Pressed::Inside;
    }
    if (over(drawn.open, x, y)) { drawn.at = -1; return Pressed::Open; }
    return Pressed::Inside;
}
}

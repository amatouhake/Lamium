#include "ui/DataNotice.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/SavedDataLayout.h"
#include "ui/Toast.h"
#include "ui/Widgets.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include <array>
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
constexpr float buttonHeight = 12, pad = 5, lineHeight = 12, buttonGap = 4;
constexpr size_t maxLines = 4;
// What the notice needed when it was drawn last, per kind: a screen leaves that much room on the next frame.
// The words and the room for them differ by screen and language, so the height is measured, not assumed.
std::array<float, 3> needed{bandHeight, bandHeight, bandHeight};
double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
bool over(Button const& b, float x, float y) { return b.w > 0 && x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + buttonHeight; }
}
float height(Kind kind) { return map::waypoints::fullerData(kind) ? needed[static_cast<size_t>(kind)] : 0; }
void draw(MinecraftUIRenderContext& context, float x, float y, float width, Kind kind, glm::vec2 pointer, bool solid) {
    auto data = map::waypoints::fullerData(kind);
    if (!data || width < 120) return;
    std::string const what = kind == Kind::Waypoints ? translated("savedData.waypoints", data->waypoints)
        : kind == Kind::Placements ? translated("savedData.placements", data->placements)
        : translated("savedData.map", sizeText(data->mapBytes, data->mapMore));
    std::string const name = data->name.empty() ? translated("savedData.unnamed") : data->name;
    std::string const text = translated("savedData.notice", name, what);
    float const inner = width - 2 * pad;
    // Every line of the words, then the buttons, wrapping to another row where the room ends.
    float const lines = static_cast<float>(std::max<size_t>(1, paragraphLines(context, inner, text, maxLines)));
    struct Wanted { Button* slot; std::string text; bool primary; float w; };
    drawn = {now(), x, y, width, 0, {}, {}, {}, data->key, name};
    std::array<Wanted, 3> buttons{{{&drawn.use, translated("savedData.use"), true, 0},
                                   {&drawn.open, translated("savedData.noticeOpen"), false, 0},
                                   {&drawn.never, translated("savedData.noticeNever"), false, 0}}};
    float bx = x + pad, by = y + 3 + lines * lineHeight + 2;
    for (auto& b : buttons) {
        b.w = std::min(inner, textWidth(context, b.text) + 10);
        if (bx > x + pad && bx + b.w > x + pad + inner) { bx = x + pad; by += buttonHeight + buttonGap; }
        *b.slot = {bx, by, b.w};
        bx += b.w + buttonGap;
    }
    float const h = by + buttonHeight + 4 - y;
    drawn.h = h;
    needed[static_cast<size_t>(kind)] = h + 4;
    fill(context, x, y, width, h, solid ? palette::panel : palette::warning, solid ? .96f : .08f);
    frame(context, x, y, width, h, palette::warning);
    paragraph(context, x + pad, y + 3, inner, text, maxLines, palette::dim);
    for (auto& b : buttons) {
        bool const hovered = over(*b.slot, pointer.x, pointer.y);
        if (b.primary) drawSmallButton(context, b.slot->x, b.slot->y, b.w, buttonHeight, b.text, hovered, palette::accentDeep, palette::accent);
        else drawSmallButton(context, b.slot->x, b.slot->y, b.w, buttonHeight, b.text, hovered);
    }
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

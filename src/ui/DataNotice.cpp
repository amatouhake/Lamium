#include "ui/DataNotice.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/SavedDataLayout.h"
#include "ui/Toast.h"
#include "ui/Widgets.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include <algorithm>
#include <array>
#include <chrono>

namespace lamium::ui::data_notice {
namespace {
// What a button does: the fuller-data notice's three, or an answer to the world question (L-365).
enum class Action { None, Use, Open, Never, Keep, New, UseOther, NoSplit };
struct Button { float x = 0, y = 0, w = 0; Action action = Action::None; };
struct Drawn {
    double at = -1;
    float x = 0, y = 0, w = 0, h = 0;
    std::array<Button, 3> buttons;
    std::string key, name;
} drawn;
constexpr float buttonHeight = 12, pad = 5, lineHeight = 12, buttonGap = 4;
constexpr size_t maxLines = 4;
// What the notice needed when it was drawn last, per kind: a screen leaves that much room on the next frame.
// The words and the room for them differ by screen and language, so the height is measured, not assumed.
std::array<float, 4> needed{bandHeight, bandHeight, bandHeight, bandHeight};
double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
bool over(Button const& b, float x, float y) { return b.w > 0 && x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + buttonHeight; }
}
void hide() { drawn.at = -1; }
float height(Kind kind) {
    return map::waypoints::worldQuestion() || map::waypoints::fullerData(kind) ? needed[static_cast<size_t>(kind)] : 0;
}
namespace {
std::string differences(unsigned diff) {
    std::string out;
    for (auto [bit, key] : {std::pair{map::prints::Seed, "savedData.diffSeed"}, std::pair{map::prints::Name, "savedData.diffName"},
                            std::pair{map::prints::Mode, "savedData.diffMode"}})
        if (diff & bit) out += (out.empty() ? "" : " / ") + translated(key);
    return out;
}
}
void draw(MinecraftUIRenderContext& context, float x, float y, float width, Kind kind, glm::vec2 pointer, bool solid) {
    // The world question comes first: until it is answered, what this screen shows may be another world's.
    auto question = map::waypoints::worldQuestion();
    auto data = question ? std::nullopt : map::waypoints::fullerData(kind);
    if ((!question && !data) || width < 120) { hide(); return; }
    std::string text, name;
    struct Wanted { std::string text; Action action; bool primary; float w; };
    std::array<Wanted, 3> buttons;
    if (question) {
        name = question->otherName.empty() ? translated("savedData.unnamed") : question->otherName;
        text = question->fresh ? translated("savedData.worldFresh", name, differences(question->diff))
                               : translated("savedData.worldSame", differences(question->diff));
        buttons = question->fresh
            ? std::array<Wanted, 3>{{{translated("savedData.keepNew"), Action::Keep, true, 0},
                                     {translated("savedData.useSaved"), Action::UseOther, false, 0},
                                     {translated("savedData.noSplit"), Action::NoSplit, false, 0}}}
            : std::array<Wanted, 3>{{{translated("savedData.keepThis"), Action::Keep, true, 0},
                                     {translated("savedData.startNew"), Action::New, false, 0},
                                     {translated("savedData.noSplit"), Action::NoSplit, false, 0}}};
    } else {
        std::string const what = kind == Kind::Waypoints ? translated("savedData.waypoints", data->waypoints)
            : kind == Kind::Placements ? translated("savedData.placements", data->placements)
            : kind == Kind::Shapes ? translated("savedData.shapes", data->shapes)
            : translated("savedData.map", sizeText(data->mapBytes, data->mapMore));
        name = data->name.empty() ? translated("savedData.unnamed") : data->name;
        text = translated("savedData.notice", name, what);
        buttons = {{{translated("savedData.use"), Action::Use, true, 0},
                    {translated("savedData.noticeOpen"), Action::Open, false, 0},
                    {translated("savedData.noticeNever"), Action::Never, false, 0}}};
    }
    float const inner = width - 2 * pad;
    // Every line of the words, then the buttons, wrapping to another row where the room ends.
    float const lines = static_cast<float>(std::max<size_t>(1, paragraphLines(context, inner, text, maxLines)));
    drawn = {now(), x, y, width, 0, {}, data ? data->key : std::string(), name};
    float bx = x + pad, by = y + 3 + lines * lineHeight + 2;
    for (size_t i = 0; i < buttons.size(); ++i) {
        auto& b = buttons[i];
        b.w = std::min(inner, textWidth(context, b.text) + 10);
        if (bx > x + pad && bx + b.w > x + pad + inner) { bx = x + pad; by += buttonHeight + buttonGap; }
        drawn.buttons[i] = {bx, by, b.w, b.action};
        bx += b.w + buttonGap;
    }
    float const h = by + buttonHeight + 4 - y;
    drawn.h = h;
    needed[static_cast<size_t>(kind)] = h + 4;
    fill(context, x, y, width, h, solid ? palette::panel : palette::warning, solid ? .96f : .08f);
    frame(context, x, y, width, h, palette::warning);
    paragraph(context, x + pad, y + 3, inner, text, maxLines, palette::dim);
    for (size_t i = 0; i < buttons.size(); ++i) {
        auto const& b = buttons[i];
        auto const& slot = drawn.buttons[i];
        bool const hovered = over(slot, pointer.x, pointer.y);
        if (b.primary) drawButton(context, slot.x, slot.y, b.w, buttonHeight, b.text, hovered, ButtonKind::Primary);
        else drawButton(context, slot.x, slot.y, b.w, buttonHeight, b.text, hovered);
    }
}
std::optional<Result> result;
std::optional<Result> takeResult() { return std::exchange(result, std::nullopt); }
Pressed press(float x, float y) {
    // Only the notice of this frame or the last (a press comes between frames): a screen that stopped drawing
    // it takes its clicks back, at once when it says so with hide().
    if (drawn.at < 0 || now() - drawn.at > .5 || x < drawn.x || x >= drawn.x + drawn.w || y < drawn.y || y >= drawn.y + drawn.h)
        return Pressed::Outside;
    auto hit = std::find_if(drawn.buttons.begin(), drawn.buttons.end(), [&](Button const& b) { return over(b, x, y); });
    if (hit == drawn.buttons.end()) return Pressed::Inside;
    using map::waypoints::WorldAnswer;
    auto answer = [&](WorldAnswer how, std::string done) {
        bool const ok = map::waypoints::answerWorld(how);
        result = Result{ok ? std::move(done) : translated("savedData.failed"), !ok};
    };
    switch (hit->action) {
    case Action::Use: {
        bool const used = map::waypoints::useData(drawn.key);
        result = Result{translated(used ? "savedData.switched" : "savedData.failed", drawn.name), !used};
        break;
    }
    case Action::Never:
        map::waypoints::quietFullerData();
        result = Result{translated("savedData.noticeQuiet"), false};
        break;
    case Action::Open: drawn.at = -1; return Pressed::Open;
    case Action::Keep: answer(WorldAnswer::Keep, translated("savedData.kept")); break;
    case Action::New: answer(WorldAnswer::New, translated("savedData.newStarted")); break;
    case Action::UseOther: answer(WorldAnswer::UseOther, translated("savedData.switched", drawn.name)); break;
    case Action::NoSplit: answer(WorldAnswer::NoSplit, translated("savedData.noSplitDone")); break;
    case Action::None: return Pressed::Inside;
    }
    drawn.at = -1;
    return Pressed::Inside;
}
}

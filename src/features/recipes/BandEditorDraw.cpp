#include "features/recipes/BandEditorDraw.h"
#include "app/Runtime.h"
#include "features/recipes/BandEditor.h"
#include "ui/HudEditorLayout.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/Widgets.h"
#include <algorithm>
#include <string>

namespace lamium::recipes::bandEditor {
namespace {
using band::Box;
bool on = false;
bool armed = false; // "Reset layout" pressed once
struct Dragging {
    band::editor::Drag drag;
    band::Layout start;
    std::vector<band::Placed> startPlaced; // hits are taken on these, so the preview does not move the targets
    band::Regions regions;
    Box tray;
    band::editor::Target target;
    std::optional<band::Layout> preview;
};
std::optional<Dragging> dragging;
struct Sizing {
    int region = 0, first = 0;
    float startShare = 0, startAt = 0, length = 0;
    band::Layout start, now;
};
std::optional<Sizing> sizing;
// The last frame's controls, for this frame's press.
Box trayBox, resetBox, doneBox;
std::vector<std::pair<band::Page, Box>> chips;
struct Line { int region, first; Box box; float length; };
std::vector<Line> lines;

std::string pageName(band::Page page) { return ui::translated("band.page." + std::string(band::pageNames[static_cast<size_t>(page)])); }
void save(band::Layout const& layout) {
    auto preferences = Runtime::instance().preferences();
    preferences.inventory.bands = band::normalized(layout);
    Runtime::instance().save(preferences);
}
void dashed(MinecraftUIRenderContext& context, Box box, ui::Rgb color, float opacity) {
    for (auto const& dash : ui::hud_editor::dashedFrame({box.x, box.y, box.w, box.h}, 3, 2))
        ui::fill(context, dash.x, dash.y, dash.horizontal ? dash.length : 1, dash.horizontal ? 1 : dash.length, color, opacity);
}
}
bool active() { return on; }
void start(band::Page) {
    on = true, armed = false;
    dragging.reset(), sizing.reset();
}
void stop() {
    on = false, armed = false;
    dragging.reset(), sizing.reset();
}
band::Layout layout(band::Layout const& saved) {
    if (dragging && dragging->preview) return *dragging->preview;
    if (sizing) return sizing->now;
    return saved;
}
void draw(MinecraftUIRenderContext& context, Input const& input, band::Layout const& saved, band::Regions const& regions,
          std::array<bool, band::pageCount> const& pageOn, std::vector<band::Placed> const& placed, Box window, view::Late& late) {
    using namespace lamium::ui;
    if (!on) return;
    float const px = input.px, py = input.py;
    // Esc lets go of what is dragged; with nothing dragged it ends the editor.
    if (input.close) {
        if (dragging || sizing) dragging.reset(), sizing.reset();
        else { stop(); return; }
    }
    if (dragging) {
        if (input.held) {
            dragging->target = band::editor::hit(dragging->startPlaced, dragging->regions, dragging->start, dragging->drag, px, py, dragging->tray);
            dragging->preview = band::editor::apply(dragging->start, dragging->drag, dragging->target);
        } else {
            if (dragging->preview) save(*dragging->preview);
            dragging.reset();
        }
    }
    if (sizing) {
        auto const region = sizing->region;
        float const at = band::across(region) ? px : py;
        if (input.held) sizing->now = band::editor::resize(sizing->start, region, sizing->first, sizing->startShare, at - sizing->startAt, sizing->length);
        else {
            save(sizing->now);
            sizing.reset();
        }
    }
    if (input.press && !dragging && !sizing) {
        bool const onReset = resetBox.has(px, py);
        if (doneBox.has(px, py)) { stop(); return; }
        if (onReset) {
            if (armed) save(band::defaultLayout());
            armed = !armed;
        } else {
            armed = false;
            auto beginDrag = [&](band::editor::Drag drag) {
                dragging = Dragging{drag, saved, placed, regions, trayBox, {}, std::nullopt};
            };
            auto chip = std::find_if(chips.begin(), chips.end(), [&](auto const& c) { return c.second.has(px, py); });
            auto line = std::find_if(lines.begin(), lines.end(), [&](Line const& l) { return l.box.has(px, py); });
            if (chip != chips.end()) beginDrag({chip->first});
            else if (line != lines.end()) {
                float const share = static_cast<float>(saved.regions[static_cast<size_t>(line->region)][static_cast<size_t>(line->first)].share);
                sizing = Sizing{line->region, line->first, share, band::across(line->region) ? px : py, line->length, saved, saved};
            } else
                for (auto const& panel : placed)
                    if (panel.shownIn == panel.region && panel.box.has(px, py)) { beginDrag({panel.page, panel.region, panel.index}); break; }
        }
    }
    band::Layout const shown = layout(saved);

    // The game's window under it is not what is being arranged.
    fill(context, window.x, window.y, window.w, window.h, palette::shade, .45f);
    // Regions: an empty one is dotted (it takes a panel whole); one too low for a row says where its panels went.
    for (int r = 0; r < band::regionCount; ++r) {
        Box const box = regions.boxes[static_cast<size_t>(r)];
        bool const any = std::any_of(placed.begin(), placed.end(), [&](band::Placed const& p) { return p.shownIn == r; });
        if (regions.usable(r) && !any) {
            dashed(context, {box.x + 1, box.y + 1, box.w - 2, box.h - 2}, palette::white, .3f);
            label(context, box.x, box.y + box.h / 2 - 4, box.w, translated("band.empty"), palette::faint, Align::Center);
        }
        bool const lends = !regions.usable(r) && std::any_of(placed.begin(), placed.end(), [&](band::Placed const& p) { return p.region == r && p.shownIn != r; });
        if (lends) {
            float const y = r == band::Top ? std::max(0.f, window.y - 10) : window.y + window.h + 1;
            label(context, window.x, y, window.w, translated(r == band::Top ? "band.lentLeft" : "band.lentRight"), palette::warning, Align::Center);
        }
    }
    // Panels: each outlined; one lent on this screen in the warning color; the one being dragged in the accent.
    for (auto const& panel : placed) {
        bool const moving = dragging && panel.page == dragging->drag.page;
        if (moving) {
            frame(context, panel.box.x, panel.box.y, panel.box.w, panel.box.h, palette::accent, 1);
            frame(context, panel.box.x + 1, panel.box.y + 1, panel.box.w - 2, panel.box.h - 2, palette::accent, 1);
        } else if (panel.shownIn != panel.region) frame(context, panel.box.x, panel.box.y, panel.box.w, panel.box.h, palette::warning, .9f);
        else dashed(context, panel.box, palette::white, .55f);
    }
    // The lines between panels of a region, which size them.
    lines.clear();
    if (!dragging)
        for (size_t i = 0; i + 1 < placed.size(); ++i) {
            auto const& a = placed[i];
            auto const& b = placed[i + 1];
            if (a.shownIn != b.shownIn || a.region != a.shownIn || b.region != b.shownIn || b.index != a.index + 1) continue;
            bool const side = band::across(a.region);
            Box const box = side ? Box{b.box.x - 1, a.box.y + 2, 3, a.box.h - 4} : Box{a.box.x + 2, b.box.y - 1, a.box.w - 4, 3};
            Box const area = regions.boxes[static_cast<size_t>(a.region)];
            lines.push_back({a.region, a.index, box, side ? area.w : area.h});
            bool const lit = box.has(px, py) || (sizing && sizing->region == a.region && sizing->first == a.index);
            fill(context, box.x, box.y, box.w, box.h, lit ? palette::accent : palette::white, lit ? 1.f : .35f);
        }
    // The editor's own panel over the game's window: what to do, the pages not placed, Reset layout and Done.
    float const w = std::min(window.w - 8, 230.f), x = window.x + (window.w - w) / 2, y = window.y + std::min(30.f, window.h / 4);
    constexpr float h = 74;
    panel(context, x, y, w, h, opacity::floating);
    frame(context, x, y, w, h, palette::white, .14f);
    label(context, x, y + 4, w, translated("band.title"), palette::text, Align::Center);
    label(context, x, y + 14, w, translated("band.hint"), palette::faint, Align::Center);
    trayBox = {x + 5, y + 25, w - 10, 26};
    bool const trayLit = dragging && dragging->target.kind == band::editor::Kind::Tray && dragging->preview;
    frame(context, trayBox.x, trayBox.y, trayBox.w, trayBox.h, trayLit ? palette::dangerEdge : palette::white, trayLit ? 1.f : .2f);
    label(context, trayBox.x + 3, trayBox.y + 2, trayBox.w - 6, translated("band.tray"), palette::faint);
    chips.clear();
    float chipX = trayBox.x + 3;
    for (auto page : band::editor::unplaced(shown)) {
        std::string name = pageName(page);
        float const cw = textWidth(context, name) + 10;
        Box const chip{chipX, trayBox.y + 13, cw, 11};
        drawButton(context, chip.x, chip.y, chip.w, chip.h, std::move(name), chip.has(px, py));
        chips.push_back({page, chip});
        chipX += cw + 3;
    }
    std::string const resetLabel = translated("hudEditor.resetAll"), resetConfirm = translated("confirm.resetLayout");
    std::string done = translated("hudEditor.done");
    float const resetW = armedWidth(context, resetLabel, resetConfirm, 10), doneW = textWidth(context, done) + 10;
    float const bx = x + (w - resetW - 4 - doneW) / 2;
    resetBox = {bx, y + 57, resetW, 12};
    doneBox = {bx + resetW + 4, y + 57, doneW, 12};
    drawDangerButton(context, resetBox.x, resetBox.y, resetBox.w, resetBox.h, armedLabel(context, resetLabel, resetConfirm, armed, resetW),
                     resetBox.has(px, py), armed);
    drawButton(context, doneBox.x, doneBox.y, doneBox.w, doneBox.h, std::move(done), doneBox.has(px, py), ButtonKind::Primary);
    // What is dragged follows the pointer; where a drop takes nothing it says so (nothing over its own place).
    if (dragging) {
        auto const kind = dragging->target.kind;
        bool const warn = !dragging->preview && kind != band::editor::Kind::Self && !(kind == band::editor::Kind::Tray && dragging->drag.fromTray());
        std::string text = pageName(dragging->drag.page);
        if (warn) text = translated("band.cannot", text);
        float const tw = textWidth(context, text) + 8;
        fill(context, px + 6, py + 6, tw, 11, warn ? palette::danger : palette::accentDeep, .95f);
        label(context, px + 10, py + 7, tw - 8, std::move(text), palette::text);
    }
    late.tip.clear(), late.tipLines.clear();
    (void)pageOn;
}
}

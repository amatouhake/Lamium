#include "features/information/InfoHud.h"
#include "features/information/PlayerInfo.h"
#include "features/information/FrameTiming.h"
#include "features/information/NetworkInfo.h"
#include "features/information/TargetInfo.h"
#include "features/information/TargetCard.h"
#include "features/information/DebugLines.h"
#include "features/information/SystemInfo.h"
#include "features/camera/CameraSessions.h"
#include "features/interaction/BreakingRestriction.h"
#include "features/interaction/PeriodicInput.h"
#include "features/interaction/PermanentSneak.h"
#include "features/map/Minimap.h"
#include "features/map/WaypointSession.h"
#include "features/map/WaypointMarkers.h"
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
#include "features/schematic/GhostRenderer.h"
#include "features/schematic/SchematicItems.h"
#include "features/schematic/SchematicSession.h"
#include "features/schematic/SchematicActions.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/Item.h"
#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include "features/information/DurabilityHud.h"
#include "features/information/ClientCounters.h"
#include "features/information/OffhandSlot.h"
#include "features/inspection/render/PreviewLayout.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/FontRepository.h"
#include "mc/client/gui/TextAlignment.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/client/gui/controls/MeasureResult.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/UIMeasureStrategy.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/utility/NonOwnerPointer.h"
#include "mc/deps/input/RectangleArea.h"
#include "features/inspection/render/DurabilityBar.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/Level.h"
#include "mc/deps/shared_types/legacy/Difficulty.h"
#include "mc/locale/I18n.h"
#include "app/Versions.h"
#include "ui/Animations.h"
#include "ui/HudLines.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <vector>

namespace lamium::ui {
namespace {
Toast activeToast;
}
void showToggleToast(std::string feature, bool on) { activeToast.show(std::move(feature), on, toastNow()); }
void showMessageToast(std::string message) { activeToast.show(std::move(message), false, toastNow(), true); }
std::optional<Toast::Visible> currentToggleToast(double now) { return activeToast.current(now); }
}
namespace lamium::information {
namespace {
// HUD background opacity for this frame (settings, L-98); set by drawHud.
float cardOpacity = .72f;
SpeedSampler speedSampler;
// One element row: text with an optional leading marker square.
struct ElementLine { std::string text; std::optional<ui::Rgb> marker; ui::Rgb color = ui::palette::text; };
float elementZoom(ui::HudElement const& element) {
    return std::clamp(std::isfinite(element.scale) ? element.scale : 100.f, 75.f, 150.f) / 100;
}
// Draw lines through the element model: card background, shadow, scale.
std::optional<ui::hud_editor::Box> drawElement(MinecraftUIRenderContext& context, float width, float height,
                                               ui::HudElement const& element, std::vector<ElementLine> const& lines,
                                               float rowUnits = 14) {
    if (lines.empty()) return std::nullopt;
    float zoom = elementZoom(element);
    float rowHeight = rowUnits * zoom;
    bool band = element.background == ui::ElementBackground::Line;
    float contentWidth = 0;
    std::vector<float> textWidths;
    textWidths.reserve(lines.size());
    for (auto const& line : lines) {
        float textWidth = ui::labelWidth(context, line.text, zoom);
        textWidths.push_back(textWidth);
        contentWidth = std::max(contentWidth, (line.marker ? 8 + 4 : 0) + textWidth);
    }
    contentWidth = std::min(contentWidth, 230 * zoom);
    float padX = element.background == ui::ElementBackground::Card ? 5 : band ? ui::lineSidePadding * zoom : 0;
    float padY = element.background == ui::ElementBackground::Card ? 3 : 0;
    float boxWidth = contentWidth + 2 * padX, boxHeight = static_cast<float>(lines.size()) * rowHeight + 2 * padY;
    auto placement = ui::placeElement(width, height, boxWidth, boxHeight, element);
    if (element.background == ui::ElementBackground::Card)
        ui::card(context, placement.x, placement.y, boxWidth, boxHeight, cardOpacity);
    // Per-line bands on the right side of the screen line up on the right edge.
    bool alignRight = band && ui::anchorFactors(element.anchor).x == 1;
    for (size_t i = 0; i < lines.size(); ++i) {
        float markerWidth = lines[i].marker ? 8 * zoom + 4 : 0;
        float textWidth = std::min(textWidths[i], contentWidth - markerWidth);
        float x = alignRight ? placement.x + boxWidth - padX - markerWidth - textWidth : placement.x + padX;
        float y = placement.y + padY + i * rowHeight;
        float textX = x;
        if (band) {
            auto line = ui::lineBox(x, y, markerWidth + textWidth, rowHeight, zoom);
            ui::fill(context, line.x, line.y, line.width, line.height, ui::palette::panel, cardOpacity);
        }
        if (lines[i].marker) {
            float markerY = y + (rowHeight - 8 * zoom) / 2;
            ui::fill(context, x, markerY, 8 * zoom, 8 * zoom, *lines[i].marker);
            textX += markerWidth;
        }
        ui::labelScaled(context, textX, ui::lineTextTop(y, rowHeight, zoom, band), textWidth + 2, lines[i].text, zoom,
            lines[i].color, ui::Align::Left, element.shadow);
    }
    context.flushText(0, std::nullopt);
    return ui::hud_editor::Box{placement.x, placement.y, boxWidth, boxHeight};
}
// ---- Offhand slot ----
// The box the game laid the hotbar out in this frame. hud_screen places it as
// "desktop_hotbar" or "pocket_hotbar" (hotbar_chooser), and the Pocket UI
// layout as "hotbar_panel".
struct HotbarLookup { std::string name; std::optional<offhand::Box> box; };
HotbarLookup hotbarBox(ScreenView const& view) {
    VisualTree* tree = view.mVisualTree.get();
    if (!tree) return {};
    for (auto name : {"desktop_hotbar", "pocket_hotbar", "hotbar_panel"}) {
        auto control = tree->getControlByName(name, true);
        if (!control) continue;
        if (control->mCachedPositionDirty) return {name};
        glm::vec2 position = *control->mCachedPosition, size = *control->mSize;
        return {name, offhand::Box{position.x, position.y, size.x, size.y}};
    }
    return {};
}
// One line whenever what the slot was placed from changes.
void logPlacement(HotbarLookup const& hotbar, std::optional<offhand::Box> const& slot, glm::vec2 screen) {
    static std::string last;
    std::string line = hotbar.name.empty() ? std::string("no hotbar control")
        : !hotbar.box ? hotbar.name + " position not laid out yet"
        : std::format("{} at {:.1f},{:.1f} size {:.1f}x{:.1f} on {:.1f}x{:.1f}: {}", hotbar.name, hotbar.box->x,
                      hotbar.box->y, hotbar.box->w, hotbar.box->h, screen.x, screen.y,
                      slot ? std::format("slot at {:.1f},{:.1f}", slot->x, slot->y) : std::string("slot off screen"));
    if (line == last) return;
    last = line;
    Runtime::instance().self().getLogger().info("Offhand slot: {}", line);
}
// Stack count as the hotbar draws it: the "default" UI font, right-aligned at
// the bottom-right of the 18x18 cell, one unit lower, with the label's shadow.
// drawText ignores renderShadow, so the shadow is a darker copy one unit away.
void slotCount(MinecraftUIRenderContext& context, offhand::Box icon, float unit, int count) {
    auto const& handle = context.mClient.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default");
    Bedrock::NotNullNonOwnerPtr<FontHandle const> const fontRef{Bedrock::NonOwnerPointer<FontHandle const>{handle}};
    TextMeasureData const textData{unit, 0.0f, false, false, false, ::ui::TextAlignment::Right};
    CaretMeasureData const caret{-1, false};
    std::string text = std::to_string(count);
    glm::vec2 size = context.getMeasureStrategy().measureText(fontRef, text, 1000, 1000, textData, caret).mSize;
    float right = icon.x + 17 * unit, bottom = icon.y + 18 * unit;
    auto draw = [&](float offset, mce::Color const& color) {
        context.drawText(handle.getFont(), RectangleArea{right - size.x + offset, right + offset,
                         bottom - size.y + offset, bottom + offset}, std::string(text), color, 1.0f,
                         ::ui::TextAlignment::Right, textData, caret);
    };
    draw(unit, mce::Color{.25f, .25f, .25f, 1.f});
    draw(0, mce::Color{1.f, 1.f, 1.f, 1.f});
    context.flushText(0, std::nullopt);
}
// ---- Target card ----
// Hearts use the game's own health-bar sprites (9x9, overlapping by one), ten
// per line; further lines stack downward with the same one-unit overlap.
void heartRows(MinecraftUIRenderContext& context, float x, float y, float unit, std::vector<Heart> const& icons) {
    std::vector<ui::ImageRect> backs, fulls, halves;
    for (int h = 0; h < static_cast<int>(icons.size()); ++h) {
        ui::ImageRect r{x + h % heartsPerLine * 8 * unit, y + h / heartsPerLine * 8 * unit, 9 * unit, 9 * unit};
        backs.push_back(r);
        if (icons[h] == Heart::Full) fulls.push_back(r);
        else if (icons[h] == Heart::Half) halves.push_back(r);
    }
    ui::images(context, "textures/ui/heart_background", backs);
    ui::images(context, "textures/ui/heart", fulls);
    ui::images(context, "textures/ui/heart_half", halves);
}
// Armor points use the vanilla armor-bar sprites, ten icons for 0-20 points.
void armorRow(MinecraftUIRenderContext& context, float x, float y, float unit, std::array<Heart, 10> const& icons) {
    std::vector<ui::ImageRect> backs, fulls, halves;
    for (int slot = 0; slot < 10; ++slot) {
        ui::ImageRect r{x + slot * 8 * unit, y, 9 * unit, 9 * unit};
        backs.push_back(r);
        if (icons[slot] == Heart::Full) fulls.push_back(r);
        else if (icons[slot] == Heart::Half) halves.push_back(r);
    }
    ui::images(context, "textures/ui/armor_empty", backs);
    ui::images(context, "textures/ui/armor_full", fulls);
    ui::images(context, "textures/ui/armor_half", halves);
}
struct CardMorph {
    std::string identity;
    std::optional<ui::hud_editor::Box> shown, from;
    double start = 0;
};
CardMorph cardMorph;
ItemStack iconStack(TargetInfo const& target) {
    ItemStack stack;
    if (target.icon.kind == IconKind::Item && !target.icon.name.empty()) {
        try { stack.reinit(target.icon.name, 1, target.icon.aux); } catch (...) { stack = ItemStack(); }
    }
    // A fresh stack counts as just picked up, and the renderer would keep
    // playing the pickup squash on it.
    stack.mShowPickUp = false;
    stack.mWasPickedUp = false;
    return stack;
}
// ---- Schematic HUD (L-93) ----
// The selected placement in a fixed-width card: name and layers, the check
// counts as a two-by-two grid with color marks, then the materials with the
// most left, with right-aligned "left" and "have" columns. Off unless the
// player turns it on.
std::optional<ui::hud_editor::Box> drawSchematicHud(MinecraftUIRenderContext& context, float width, float height,
    ui::HudElement const& element, Settings::Schematic const& prefs, bool preview) {
    auto set = schematic::session::current();
    auto result = schematic::ghosts::verification();
    auto* player = context.mClient.getLocalPlayer();
    schematic::SavedPlacement const* selected = set.selected >= 0 && set.selected < static_cast<int>(set.placements.size())
        ? &set.placements[static_cast<size_t>(set.selected)] : nullptr;
    if (!selected && !preview) return std::nullopt;
    bool ready = selected && result->placement == set.selected && result->complete;
    schematic::Tally tally = ready ? result->visible : schematic::Tally{};
    if (!selected) { tally.correct = 99; tally.missing = 18; tally.wrong = 2; tally.state = 1; }
    std::optional<int> nearest;
    if (ready && player) {
        auto p = player->getFeetPos();
        double best = -1;
        for (auto const& m : result->mismatches) {
            if (m.state == schematic::CellState::Missing) continue;
            double dx = m.position.x + .5 - p.x, dy = m.position.y + .5 - p.y, dz = m.position.z + .5 - p.z;
            double d = dx * dx + dy * dy + dz * dz;
            if (best < 0 || d < best) best = d;
        }
        if (best >= 0) nearest = static_cast<int>(std::lround(std::sqrt(best)));
    }
    struct Material { std::string icon, name; std::uint64_t left, have; bool unknown; };
    std::vector<Material> materials;
    if (prefs.hudMaterials && ready) {
        auto have = player ? schematic::items::carried(*player) : std::map<std::string, std::uint64_t>{};
        for (auto const& line : result->visibleMaterials) {
            if (!line.remaining()) continue;
            // An entity counts as carried only when an item places it (armor stand).
            bool unknown = line.item.empty() || (line.entity && !schematic::items::iconStack(line.icon));
            materials.push_back({line.icon, line.name, line.remaining(), unknown ? 0 : have[line.item], unknown});
            if (materials.size() == 5) break;
        }
    }

    float z = elementZoom(element);
    bool card = element.background == ui::ElementBackground::Card;
    float pad = card ? 4 * z : 0, rowH = 10 * z, small = .8f * z, icon = 9 * z, gap = 6 * z;
    auto widthOf = [&](std::string_view value, float scale) { return ui::textWidthScaled(context, value, scale); };
    bool verify = prefs.hudVerify || !selected;

    // Texts first, so the card is only as wide as its content.
    std::string name = selected ? selected->name : ui::translated("feature.schematicHud");
    std::string layers;
    if (selected && selected->layers.mode != schematic::LayerMode::All) {
        auto structure = schematic::session::structure(selected->file);
        int count = structure ? schematic::layerCount(schematic::placedSize(structure->size, selected->placement.rotation),
            selected->layers.axis) : 1;
        layers = ui::translated("schematic.layerValue", selected->layers.index + 1, std::max(1, count));
    }
    struct Cell { ui::Rgb mark; std::string text; };
    std::array<Cell, 4> cells{{
        {ui::palette::accent, ui::translated("schematic.hud.correct") + " " + std::format("{}/{}", tally.correct, tally.total())},
        {ui::Rgb{.75f, .85f, .9f}, ui::translated("schematic.kind.missing") + " " + std::to_string(tally.missing)},
        {ui::Rgb{1.f, .35f, .3f}, ui::translated("schematic.hud.wrong") + " " + std::to_string(tally.wrong + tally.extra)},
        {ui::Rgb{1.f, .8f, .25f}, ui::translated("schematic.kind.state") + " " + std::to_string(tally.state)}}};
    std::string nearText = nearest ? ui::translated("schematic.hud.nearest", *nearest) : std::string{};
    float mark = 7 * z;
    float columnA = std::max(widthOf(cells[0].text, small), widthOf(cells[2].text, small)) + mark;
    float columnB = std::max(widthOf(cells[1].text, small), widthOf(cells[3].text, small)) + mark;
    std::string leftHead = ui::translated("schematic.column.left"), haveHead = ui::translated("schematic.column.carried");
    float numW = std::max(widthOf(leftHead, small), widthOf(haveHead, small)), nameW = widthOf(ui::translated("schematic.hud.left"), small);
    for (auto const& m : materials) {
        numW = std::max({numW, widthOf(std::to_string(m.left), small), widthOf(std::to_string(m.have), small)});
        nameW = std::max(nameW, icon + 2 * z + widthOf(m.name, small));
    }
    float contentW = widthOf(name, z) + (layers.empty() ? 0 : gap + widthOf(layers, small));
    if (ready && verify) contentW = std::max({contentW, columnA + gap + columnB, widthOf(nearText, small)});
    if (!materials.empty()) contentW = std::max(contentW, nameW + 2 * (numW + gap));
    if (!ready && selected) contentW = std::max(contentW, widthOf(ui::translated("schematic.counting"), small));
    contentW = std::clamp(contentW, 80 * z, 170 * z);

    float h = rowH + 4 * z;
    if (!ready && selected) h += rowH;
    if (ready && verify) h += 2 * rowH + (nearest ? rowH : 0) + 2 * z;
    if (!materials.empty()) h += 2 * z + rowH * (1 + static_cast<float>(materials.size()));
    float boxW = contentW + 2 * pad, boxH = h + 2 * pad;
    auto at = ui::placeElement(width, height, boxW, boxH, element);
    if (card) ui::card(context, at.x, at.y, boxW, boxH, cardOpacity);
    float x = at.x + pad, y = at.y + pad;
    auto text = [&](float tx, float ty, float w, std::string value, ui::Rgb color, float scale, ui::Align align = ui::Align::Left) {
        ui::labelScaled(context, tx, ty, w, std::move(value), scale, color, align, element.shadow);
    };
    auto rule = [&](float ry) { ui::fill(context, x, ry, contentW, 1, ui::palette::white, .14f); };

    float layersW = layers.empty() ? 0 : widthOf(layers, small) + 2 * z;
    text(x, y, contentW - layersW - (layers.empty() ? 0 : gap), name, ui::palette::text, z);
    if (!layers.empty()) text(x + contentW - layersW, y + 1 * z, layersW, layers, ui::palette::dim, small, ui::Align::Right);
    y += rowH + 1 * z;
    rule(y);
    y += 3 * z; // room under the rule before the counts
    if (!ready && selected) {
        text(x, y, contentW, ui::translated("schematic.counting"), ui::palette::dim, small);
        y += rowH;
    }
    if (ready && verify) {
        float secondX = x + columnA + gap;
        for (int i = 0; i < 4; ++i) {
            float cx = i % 2 ? secondX : x, cy = y + (i / 2) * rowH;
            ui::fill(context, cx, cy + 2.5f * z, 4.5f * z, 4.5f * z, cells[static_cast<size_t>(i)].mark);
            text(cx + mark, cy, (i % 2 ? columnB : columnA), cells[static_cast<size_t>(i)].text, ui::palette::text, small);
        }
        y += 2 * rowH;
        if (nearest) {
            text(x, y, contentW, nearText, ui::palette::dim, small);
            y += rowH;
        }
        y += 2 * z;
    }
    if (!materials.empty()) {
        rule(y);
        y += 2 * z;
        float haveX = x + contentW - numW, leftX = haveX - gap - numW;
        text(x, y, leftX - x, ui::translated("schematic.hud.left"), ui::palette::dim, small);
        text(leftX, y, numW, leftHead, ui::palette::dim, small, ui::Align::Right);
        text(haveX, y, numW, haveHead, ui::palette::dim, small, ui::Align::Right);
        y += rowH;
        auto* renderer = context.mClient.getItemRenderer();
        for (auto const& m : materials) {
            if (auto const* stack = schematic::items::iconStack(m.icon); stack && renderer) {
                BaseActorRenderContext renderContext(context.mScreenContext, context.mClient, context.mClient.getMinecraftGame_DEPRECATED());
                renderer->renderGuiItemNew(renderContext, *stack, 0, std::round(x), std::round(y), false, 1.f, 1.f, icon / 16, 17);
            }
            text(x + icon + 2 * z, y, leftX - x - icon - 4 * z, m.name, ui::palette::text, small);
            text(leftX, y, numW, std::to_string(m.left), ui::palette::text, small, ui::Align::Right);
            text(haveX, y, numW, m.unknown ? "-" : std::to_string(m.have),
                m.unknown ? ui::palette::dim : m.have >= m.left ? ui::palette::accent : ui::palette::warning, small, ui::Align::Right);
            y += rowH;
        }
    }
    context.flushText(0, std::nullopt);
    return ui::hud_editor::Box{at.x, at.y, boxW, boxH};
}
// ---- Durability HUD (L-61) ----
std::optional<ui::hud_editor::Box> drawDurability(MinecraftUIRenderContext& context, float width, float height,
    ui::HudElement const& element, Settings::Information const& settings, bool preview) {
    namespace dur = durability;
    auto look = static_cast<dur::Look>(std::clamp(settings.durabilityLook, 0, 2));
    // Copies for this frame only: the renderer must not replay a pickup squash.
    std::array<ItemStack, 6> stacks;
    dur::Samples samples{};
    auto sample = [&](dur::Slot slot, ItemStack const& source) {
        auto i = static_cast<size_t>(slot);
        if (source.isNull() || source.mCount <= 0 || !source.mItem || !source.isDamageableItem()) return;
        stacks[i] = source;
        stacks[i].mShowPickUp = false;
        stacks[i].mWasPickedUp = false;
        samples[i] = {true, source.getDamageValue(), static_cast<int>(source.mItem->getMaxDamage())};
    };
    if (auto* player = context.mClient.getLocalPlayer()) {
        using ArmorSlot = SharedTypes::Legacy::ArmorSlot;
        sample(dur::Slot::MainHand, player->getSelectedItem());
        sample(dur::Slot::Offhand, player->getOffhandSlot());
        sample(dur::Slot::Head, player->getArmor(ArmorSlot::Head));
        sample(dur::Slot::Chest, player->getArmor(ArmorSlot::Torso));
        sample(dur::Slot::Legs, player->getArmor(ArmorSlot::Legs));
        sample(dur::Slot::Feet, player->getArmor(ArmorSlot::Feet));
    }
    auto rows = dur::rows(samples, settings.durabilityOffhand, settings.durabilityArmor);
    if (rows.empty() && preview) {
        // The layout editor needs something to place even with bare hands.
        ItemStack pick;
        try { pick.reinit("minecraft:diamond_pickaxe", 1, 0); } catch (...) { pick = ItemStack(); }
        if (!pick.isNull()) {
            sample(dur::Slot::MainHand, pick);
            samples[0].damage = samples[0].max / 4;
            rows = dur::rows(samples, false, false);
        }
    }
    if (rows.empty()) return std::nullopt;
    float z = elementZoom(element);
    bool card = element.background == ui::ElementBackground::Card;
    float padX = card ? 5 * z : 0, padY = card ? 3 * z : 0;
    float icon = 16 * z, gap = 4 * z, barW = 32 * z, barH = 3 * z, rowH = 18 * z;
    std::vector<std::string> texts;
    float contentW = 0;
    for (auto const& row : rows) {
        texts.push_back(dur::showsNumber(look, row) ? dur::numberText(look, row) : std::string{});
        float w = icon + (dur::showsBar(look) ? gap + barW : 0);
        if (!texts.back().empty()) w += gap + ui::textWidthScaled(context, texts.back(), z);
        contentW = std::max(contentW, w + 2 * z);
    }
    float boxW = contentW + 2 * padX, boxH = rows.size() * rowH + 2 * padY;
    auto placement = ui::placeElement(width, height, boxW, boxH, element);
    if (card) ui::card(context, placement.x, placement.y, boxW, boxH, cardOpacity);
    auto* renderer = context.mClient.getItemRenderer();
    for (size_t i = 0; i < rows.size(); ++i) {
        auto const& row = rows[i];
        float x = placement.x + padX + z, y = placement.y + padY + i * rowH;
        if (renderer) {
            BaseActorRenderContext renderContext(context.mScreenContext, context.mClient,
                                                 context.mClient.getMinecraftGame_DEPRECATED());
            // Whole GUI units, as in inventory slots: layered icons (dyed
            // leather) show seams between their layers at fractional positions.
            renderer->renderGuiItemNew(renderContext, stacks[static_cast<size_t>(row.slot)], 0, std::round(x),
                                       std::round(y + z), false, 1.f, 1.f, z, 17);
        }
        float cx = x + icon;
        if (dur::showsBar(look)) {
            cx += gap;
            float by = y + (rowH - barH) / 2;
            ui::fill(context, cx, by, barW, barH, ui::Rgb{0, 0, 0});
            auto color = inspection::render::durabilityColor(row.ratio());
            float fillW = dur::barFill(row, barW / z) * z;
            if (fillW > 0) ui::fill(context, cx, by, fillW, barH - z, ui::Rgb{color.r, color.g, color.b});
            cx += barW;
        }
        if (!texts[i].empty())
            ui::labelScaled(context, cx + gap, y + (rowH - 10 * z) / 2, contentW, texts[i], z, ui::palette::text,
                            ui::Align::Left, element.shadow);
    }
    context.flushText(0, std::nullopt);
    return ui::hud_editor::Box{placement.x, placement.y, boxW, boxH};
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
    options.armor = settings.targetArmor == 1 ? Meter::Bar : settings.targetArmor == 2 ? Meter::Number : Meter::Icons;
    options.growth = settings.targetGrowth == 1 ? Meter::Number : Meter::Bar;
    int capacity = std::max(0, static_cast<int>((height - 40) / (12 * z)));
    auto rows = cardRows(target, options, static_cast<size_t>(std::min(capacity, 10)));
    auto stack = settings.targetIcon ? iconStack(target) : ItemStack();
    bool icon = !stack.isNull();
    bool texture = settings.targetIcon && !icon && target.icon.kind == IconKind::Texture
        && !target.icon.name.empty();
    float iconSize = icon || texture ? 16 * z : 0, iconGap = icon || texture ? 5 * z : 0;
    float lineH = 11 * z, rowH = 12 * z;
    // A Hearts row grows by one heart line per further ten hearts.
    auto heightOf = [&](CardRow const& row) {
        return row.meter == Meter::Hearts ? rowH + (std::max(heartLines(row.maximum), 1) - 1) * 8 * z : rowH;
    };
    auto heartsWidth = [&](CardRow const& row) {
        return (std::min(heartSlots(row.maximum), heartsPerLine) * 8 + 1 + 4) * z;
    };
    // A row's item icon (the schematic's expected block), as tall as the text.
    float rowIcon = 10 * z;
    auto rowStack = [](CardRow const& row) {
        return row.icon.empty() ? nullptr : schematic::items::iconStack(row.icon);
    };
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
        if (row.progress && row.meter == Meter::Hearts) valueW += heartsWidth(row);
        if (row.progress && row.meter == Meter::Icons) valueW += (10 * 8 + 1 + 4) * z;
        if (rowStack(row)) valueW += rowIcon + 2 * z;
        if (!row.before.empty())
            valueW += ui::textWidthScaled(context, row.before, z) + (ui::changeArrowWidth + 6) * z;
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
    float rowsH = 0;
    for (auto const& row : rows) rowsH += heightOf(row);
    float boxH = headerH + (rows.empty() ? 0 : 3 * z + rowsH) + 2 * padY;
    auto placement = ui::placeElement(width, height, boxW, boxH, element);
    ui::hud_editor::Box finalBox{placement.x, placement.y, boxW, boxH};
    // Ease the card between targets; the content appears once it settles.
    auto identity = target.identifier + "|" + target.name;
    std::optional<ui::hud_editor::Box> background = finalBox;
    if (animate && card && ui::animationsOn(context.mClient)) {
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
    if (card) ui::card(context, background->x, background->y, background->w, background->h, cardOpacity);
    if (!cardContentFits(background->x, background->y, background->w, background->h, finalBox.x, finalBox.y,
                         finalBox.w, finalBox.h))
        return finalBox;
    float left = finalBox.x + padX, top = finalBox.y + padY;
    if (icon) {
        if (auto* renderer = context.mClient.getItemRenderer()) {
            BaseActorRenderContext renderContext(context.mScreenContext, context.mClient,
                                                 context.mClient.getMinecraftGame_DEPRECATED());
            renderer->renderGuiItemNew(renderContext, stack, 0, left, top + (headerH - iconSize) / 2, false, 1.f, 1.f, z, 17);
        }
    } else if (texture) {
        ui::imageUv(context, target.icon.name, {left, top + (headerH - iconSize) / 2, iconSize, iconSize},
                    target.icon.u0, target.icon.v0, target.icon.u1, target.icon.v1);
    }
    float textX = left + iconSize + iconGap, textW = contentW - iconSize - iconGap;
    float textTop = top + (headerH - headerTextH) / 2;
    ui::labelScaled(context, textX, textTop, textW + 2, target.name, z, ui::palette::text, ui::Align::Left, element.shadow);
    if (settings.targetIdentifier)
        ui::labelScaled(context, textX, textTop + lineH, textW + 2, target.identifier, z, ui::palette::faint,
                        ui::Align::Left, element.shadow);
    float y = top + headerH + 3 * z;
    float valueX = left + labelW + 6 * z;
    for (size_t i = 0; i < rows.size(); y += heightOf(rows[i]), ++i) {
        auto const& row = rows[i];
        ui::labelScaled(context, left, y, labelW + 2, labels[i], z, ui::palette::faint, ui::Align::Left, element.shadow);
        float x = valueX;
        if (row.progress && row.meter == Meter::Bar) {
            float barY = y + 3 * z, barH = 5 * z, barW = barUnits * z;
            ui::fill(context, x, barY, barW, barH, ui::palette::white, .12f);
            bool health = row.label == "target.health";
            bool armor = row.label == "target.armor";
            ui::fill(context, x, barY, barW * std::clamp(*row.progress, 0.f, 1.f), barH,
                     health ? ui::palette::heart : armor ? ui::palette::armor : ui::palette::accent);
            x += barW + 4 * z;
        } else if (row.progress && row.meter == Meter::Hearts) {
            heartRows(context, x, y + 1 * z, z, healthHearts(row.current, row.maximum));
            x += heartsWidth(row);
        } else if (row.progress && row.meter == Meter::Icons) {
            armorRow(context, x, y + 1 * z, z, hearts(*row.progress));
            x += (10 * 8 + 1 + 4) * z;
        }
        if (auto const* stack = rowStack(row)) {
            if (auto* renderer = context.mClient.getItemRenderer()) {
                BaseActorRenderContext renderContext(context.mScreenContext, context.mClient,
                                                     context.mClient.getMinecraftGame_DEPRECATED());
                renderer->renderGuiItemNew(renderContext, *stack, 0, std::round(x), std::round(y), false, 1.f, 1.f,
                                           rowIcon / 16, 17);
            }
            x += rowIcon + 2 * z;
        }
        // Schematic rows in the verifier's colors (the HUD and Check tab use the same).
        auto color = row.tone == Tone::Wrong ? ui::Rgb{1.f, .35f, .3f} : row.tone == Tone::State ? ui::Rgb{1.f, .8f, .25f}
            : row.tone == Tone::Missing ? ui::Rgb{.75f, .85f, .9f} : ui::palette::text;
        if (!row.before.empty()) {
            float w = ui::textWidthScaled(context, row.before, z);
            ui::labelScaled(context, x, y, w + 2, row.before, z, color, ui::Align::Left, element.shadow);
            x += w + 3 * z;
            ui::changeArrow(context, x, y + (1.5f + ui::shapeTextDrop()) * z, z, color);
            x += (ui::changeArrowWidth + 3) * z;
        }
        ui::labelScaled(context, x, y, left + contentW - x + 2, values[i], z, color, ui::Align::Left, element.shadow);
    }
    context.flushText(0, std::nullopt);
    return finalBox;
}
// ---- Debug view (BACKLOG L-54) ----
std::string debugHeader() {
    return std::format("Minecraft {} \u00b7 LeviLamina {} \u00b7 Lamium {}", runningGameVersion(), runningLoaderVersion(),
        lamiumVersion());
}
std::string onOffText(bool on) { return ui::translated(on ? "animations.on" : "animations.off"); }
std::string difficultyName(int difficulty) {
    constexpr std::string_view keys[]{"difficulty.peaceful", "difficulty.easy", "difficulty.normal", "difficulty.hard"};
    return ui::translated(keys[static_cast<size_t>(std::clamp(difficulty, 0, 3))]);
}
std::string localizedBiomeName(std::string const& identifier) {
    auto key = biomeTranslationKey(identifier);
    if (key.empty()) return identifier;
    auto name = getI18n().get(key, getI18n().getCurrentLanguage());
    if (!name.empty() && name != key) return name;
    name = ui::translated(key);
    return name == key ? identifier : name;
}
std::optional<std::string> localClock(bool includeDate) {
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) return {};
#else
    if (!localtime_r(&now, &local)) return {};
#endif
    auto text = includeDate
        ? formatRealDateTime(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min)
        : formatRealTime(local.tm_hour, local.tm_min);
    return text.empty() ? std::nullopt : std::optional{std::move(text)};
}
BiomeDisplay biomeDisplay(Settings::Information const& settings) {
    if (!settings.biomeId) return BiomeDisplay::Name;
    return settings.biomeIdOnly ? BiomeDisplay::Id : BiomeDisplay::NameAndId;
}
DebugTarget describeTarget(TargetInfo const& info) {
    DebugTarget target;
    target.identifier = info.identifier;
    std::string java;
    for (auto const& detail : info.details) {
        if (detail.kind == DetailKind::Health) java = "Health: " + detail.value;
        else if (detail.kind == DetailKind::Armor)
            java += (java.empty() ? std::string() : " | ") + "Armor: " + detail.value;
    }
    if (java.empty() && !info.states.empty()) {
        java = info.identifier + "[";
        for (size_t i = 0; i < info.states.size(); ++i) java += (i ? ", " : "") + info.states[i];
        java += "]";
    }
    if (!java.empty()) target.javaLines.push_back(std::move(java));
    for (auto const& detail : info.details) {
        std::string line = ui::translated(detail.label) + ": ";
        line += detail.valueIsKey ? ui::translated(detail.value) : detail.value;
        target.gameLines.push_back(std::move(line));
    }
    if (target.gameLines.empty() && !info.states.empty()) {
        std::string line;
        for (size_t i = 0; i < info.states.size(); ++i) line += (i ? ", " : "") + info.states[i];
        target.gameLines.push_back(std::move(line));
    }
    return target;
}
void appendPart(std::string& line, std::string part, std::string_view separator) {
    if (part.empty()) return;
    if (!line.empty()) line += separator;
    line += part;
}
// FreeCamera's pose for readouts that follow the camera (L-124).
std::optional<CameraPose> cameraPose(IClientInstance& client) {
    auto pose = CameraSessions::instance().freeCameraPose(client);
    if (!pose) return {};
    return CameraPose{pose->x, pose->y, pose->z, pose->yaw, pose->pitch};
}
std::optional<DebugValues> collectDebugValues(IClientInstance& client, std::optional<ViewRay> const& ray) {
    auto* player = client.getLocalPlayer();
    if (!player) return std::nullopt;
    DebugValues value;
    value.header = debugHeader();
    value.timing = frameStatistics();
    value.ping = connectionPing(client);
    auto counts = clientCounters(client);
    value.entities = counts.entities;
    value.chunks = counts.chunks;
    value.particles = counts.particles;
    auto const& options = client.getOptions();
    value.renderDistance = options.getViewDistanceChunks();
    value.maxRenderDistance = options.getMaxViewDistanceChunksRaw();
    value.rayTracing = options.getRayTracing();
    value.vibrantVisuals = options.isVibrantVisualsUserEnabled();
    value.clouds = options.getRenderClouds();
    value.fancySkies = options.getFancySkies();
    value.fullscreen = options.getFullscreen();
    if (int maxFps = options.getDeferredTargetFrameRate(); maxFps > 0) value.maxFps = maxFps;
    auto info = collectPlayerInfo(client, {true, true, true, true, true, true, true, true}, cameraPose(client));
    if (info.present) {
        if (info.camera && info.bodyPosition && info.bodyYaw && info.bodyPitch)
            value.body = DebugValues::Body{info.bodyPosition->x, info.bodyPosition->y, info.bodyPosition->z,
                                           *info.bodyYaw, *info.bodyPitch};
        if (info.position) {
            value.x = info.position->x;
            value.y = info.position->y;
            value.z = info.position->z;
        }
        if (info.yaw && info.pitch) {
            value.yaw = info.yaw;
            value.pitch = info.pitch;
        }
        value.dimension = info.dimension.value_or("");
        value.biome = info.biome.value_or("");
        if (info.light) {
            value.skyLight = info.light->sky;
            value.blockLight = info.light->block;
        }
        value.worldTime = info.worldTime;
        value.raining = info.raining;
    }
    if (int difficulty = static_cast<int>(player->getLevel().getDifficulty()); difficulty >= 0 && difficulty <= 3)
        value.difficulty = difficulty;
    if (auto target = collectTargetInfo(client, true, ray)) value.target = describeTarget(*target);
    value.memory = systemMemoryText();
    value.cpu = systemCpuText();
    value.gpu = systemGpuText();
    value.display = systemDisplayText();
    value.os = systemOsText();
    return value;
}
GameText debugGameText(DebugValues const& value) {
    GameText text;
    std::string perf;
    if (value.ping) perf = ui::translated("hudPing", std::format("{} ms", *value.ping));
    if (value.renderDistance) {
        std::string distance = std::format("{}", *value.renderDistance);
        if (value.maxRenderDistance) distance += std::format(" / {}", *value.maxRenderDistance);
        appendPart(perf, ui::translated("debugRenderDistance", distance), " | ");
    }
    text.perf = std::move(perf);
    if (value.entities) appendPart(text.counts, ui::translated("debugEntities", *value.entities), " | ");
    if (value.chunks) appendPart(text.counts, ui::translated("debugChunks", *value.chunks), " | ");
    if (value.particles) appendPart(text.counts, ui::translated("debugParticles", *value.particles), " | ");
    if (value.x && value.y && value.z) {
        text.coordinates = ui::translated("hudXYZ", *value.x, *value.y, *value.z);
        text.blockChunk = ui::translated("hudBlock", static_cast<int>(std::floor(*value.x)),
                             static_cast<int>(std::floor(*value.y)), static_cast<int>(std::floor(*value.z)))
            + " | " + ui::translated("hudChunk", formatChunk(chunkPosition(*value.x, *value.z)));
    }
    auto facing = [](float yaw, float pitch) {
        auto key = facingKey(yaw);
        return ui::translated("hudFacing", key ? ui::translated(*key) : ui::translated("unavailable"))
            + " | " + ui::translated("hudRotation", formatRotation(yaw, pitch));
    };
    if (value.yaw && value.pitch) text.facing = facing(*value.yaw, *value.pitch);
    if (auto const& b = value.body) {
        auto camera = ui::translated("hudCameraTag") + " ", player = ui::translated("hudPlayerTag") + " ";
        if (!text.coordinates.empty()) text.coordinates = camera + text.coordinates;
        if (!text.facing.empty()) text.facing = camera + text.facing;
        text.bodyCoordinates = player + ui::translated("hudXYZ", b->x, b->y, b->z);
        text.bodyFacing = player + facing(b->yaw, b->pitch);
    }
    if (value.skyLight && value.blockLight)
        text.light = ui::translated("hudLight", ui::translated("hudLightValues", *value.skyLight, *value.blockLight));
    if (!value.biome.empty()) {
        std::string line = ui::translated("hudBiome", value.biome);
        if (value.difficulty)
            appendPart(line, ui::translated("debugDifficulty", difficultyName(*value.difficulty)), " | ");
        text.biome = std::move(line);
    }
    if (value.worldTime) {
        std::string line = ui::translated("hudTime", dayCount(*value.worldTime), formatClock(*value.worldTime));
        if (value.raining)
            appendPart(line, ui::translated("hudWeather",
                ui::translated(*value.raining ? "weatherRain" : "weatherClear")), " | ");
        appendPart(line, ui::translated("hudMoon", ui::translated(moonPhaseKey(moonPhase(*value.worldTime)))), " | ");
        text.time = std::move(line);
    }
    text.lookAt = ui::translated("debugLook");
    text.client = ui::translated("debugClient");
    text.system = ui::translated("debugSystem");
    if (!value.dimension.empty()) text.dimension = ui::translated("hudDimensionValue", value.dimension);
    if (value.renderDistance) {
        std::string distance = std::format("{}", *value.renderDistance);
        if (value.maxRenderDistance) distance += std::format(" / {}", *value.maxRenderDistance);
        text.renderDistance = ui::translated("debugRenderDistance", distance);
    }
    if (value.rayTracing)
        appendPart(text.visuals, ui::translated("debugRay", onOffText(*value.rayTracing)), " \u00b7 ");
    if (value.vibrantVisuals)
        appendPart(text.visuals, ui::translated("debugVibrantVisuals", onOffText(*value.vibrantVisuals)), " \u00b7 ");
    if (value.fullscreen)
        appendPart(text.screen, ui::translated("debugFullscreen", onOffText(*value.fullscreen)), " \u00b7 ");
    if (value.maxFps) appendPart(text.screen, ui::translated("debugMaxFps", *value.maxFps), " \u00b7 ");
    if (value.clouds) appendPart(text.screen, ui::translated("debugClouds", onOffText(*value.clouds)), " \u00b7 ");
    if (value.fancySkies) appendPart(text.screen, ui::translated("debugSkies", onOffText(*value.fancySkies)), " \u00b7 ");
    if (value.memory) text.memory = ui::translated("debugMemory", *value.memory);
    if (value.cpu) text.cpu = ui::translated("debugCpu", *value.cpu);
    if (value.gpu) text.gpu = ui::translated("debugGpu", *value.gpu);
    if (value.display) text.display = ui::translated("debugDisplay", *value.display);
    if (value.os) text.os = ui::translated("debugOs", *value.os);
    return text;
}
// Fixed to the screen edges like Java's debug screen: the left column hangs
// from the top-left, the right column from the top-right. The panel is not a
// HUD element and is never moved or styled by the layout editor.
void drawDebugColumns(MinecraftUIRenderContext& context, float width, float height,
    std::vector<DebugLine> const& left, std::vector<DebugLine> const& right, bool shadow, float rowHeight, bool band) {
    if (left.empty() && right.empty()) return;
    constexpr float gap = 12;
    float leftW = 0, rightW = 0;
    std::vector<float> leftWidths, rightWidths;
    for (auto const& line : left) leftW = std::max(leftW, leftWidths.emplace_back(ui::labelWidth(context, line.text, 1)));
    for (auto const& line : right) rightW = std::max(rightW, rightWidths.emplace_back(ui::labelWidth(context, line.text, 1)));
    float x = ui::hudInset, y = ui::hudInset;
    // Blank spacer lines get no background, like the empty rows they are.
    auto blank = [](std::string const& text) { return text.find_first_not_of(' ') == std::string::npos; };
    if (band) {
        for (size_t i = 0; i < left.size(); ++i) {
            if (blank(left[i].text)) continue;
            auto line = ui::lineBox(x, y + i * rowHeight, leftWidths[i], rowHeight, 1);
            ui::fill(context, line.x, line.y, line.width, line.height, ui::palette::panel, cardOpacity);
        }
    }
    for (size_t i = 0; i < left.size(); ++i)
        ui::labelScaled(context, x, ui::lineTextTop(y + i * rowHeight, rowHeight, 1, band), leftW + 2, left[i].text, 1,
                        ui::palette::text, ui::Align::Left, shadow);
    if (!right.empty()) {
        float rightX = width - ui::hudInset - rightW - 2;
        auto offset = rightColumnOffset(leftWidths, rightWidths, width - 2 * ui::hudInset - 2, gap);
        if (band) {
            // Right-aligned text ends at the column's right edge.
            for (size_t i = 0; i < right.size(); ++i) {
                if (blank(right[i].text)) continue;
                auto line = ui::lineBox(rightX + rightW + 2 - rightWidths[i], y + (i + offset) * rowHeight, rightWidths[i],
                    rowHeight, 1);
                ui::fill(context, line.x, line.y, line.width, line.height, ui::palette::panel, cardOpacity);
            }
        }
        for (size_t i = 0; i < right.size(); ++i)
            ui::labelScaled(context, rightX, ui::lineTextTop(y + (i + offset) * rowHeight, rowHeight, 1, band), rightW + 2,
                            right[i].text, 1, ui::palette::text, ui::Align::Right, shadow);
    }
    context.flushText(0, std::nullopt);
}
bool infoLineEnabled(Settings::Information const& settings, std::string_view id) {
    if (id == "coordinates") return settings.coordinates;
    if (id == "scaledCoordinates") return settings.scaledCoordinates;
    if (id == "dimension") return settings.dimension;
    if (id == "biome") return settings.biome;
    if (id == "difficulty") return settings.difficulty;
    if (id == "facing") return settings.facing;
    if (id == "yaw") return settings.yaw;
    if (id == "pitch") return settings.pitch;
    if (id == "sprinting") return settings.sprinting;
    if (id == "fps") return settings.fps;
    if (id == "frameTime") return settings.frameTime;
    if (id == "light") return settings.light;
    if (id == "ping") return settings.ping;
    if (id == "rotation") return settings.rotation;
    if (id == "block") return settings.block;
    if (id == "chunk") return settings.chunk;
    if (id == "speed") return settings.speed;
    if (id == "horizontalSpeed") return settings.horizontalSpeed;
    if (id == "verticalSpeed") return settings.verticalSpeed;
    if (id == "time") return settings.time;
    if (id == "realTime") return settings.realTime;
    if (id == "weather") return settings.weather;
    if (id == "moon") return settings.moon;
    return false;
}
// Lines that read the camera's place and angles during FreeCamera (L-124).
bool followsCamera(std::string_view id) {
    for (std::string_view line : {"coordinates", "scaledCoordinates", "block", "chunk", "facing", "yaw", "pitch",
                                  "rotation", "biome", "light", "weather"})
        if (id == line) return true;
    return false;
}
std::optional<std::string> infoLineText(std::string_view id, PlayerInfo const& info,
                                        std::optional<FrameStatistics> timing, std::optional<std::int64_t> ping,
                                        std::optional<SpeedValues> speed, std::optional<std::string> const& realTime,
                                        BiomeDisplay biomeStyle) {
    if (id == "coordinates") {
        if (info.position) {
            auto const& p = *info.position;
            return ui::translated("hudXYZ", p.x, p.y, p.z);
        }
        return ui::translated("hudCoordinates", ui::translated("unavailable"));
    }
    if (id == "scaledCoordinates") {
        if (info.position && info.dimensionId) {
            auto const& p = *info.position;
            if (auto scaled = scaledPosition(p.x, p.y, p.z, *info.dimensionId)) {
                auto dimension = scaled->destination == ScaledDimension::Nether ? "dimension.nether" : "dimension.overworld";
                return ui::translated("hudScaledCoordinates", ui::translated(dimension), scaled->x, scaled->y, scaled->z);
            }
        }
        return ui::translated("hudScaledCoordinatesRow", ui::translated("unavailable"));
    }
    if (id == "dimension")
        return ui::translated("hudDimensionValue", info.dimension.value_or(ui::translated("unavailable")));
    if (id == "biome") {
        if (!info.biome) return ui::translated("hudBiome", ui::translated("unavailable"));
        return ui::translated("hudBiome", formatBiomeValue(localizedBiomeName(*info.biome), *info.biome, biomeStyle));
    }
    if (id == "difficulty")
        return ui::translated("debugDifficulty", info.difficulty ? difficultyName(*info.difficulty)
                                                                  : ui::translated("unavailable"));
    if (id == "facing") {
        auto key = info.yaw ? facingKey(*info.yaw) : std::nullopt;
        return ui::translated("hudFacing", key ? ui::translated(*key) : ui::translated("unavailable"));
    }
    if (id == "yaw") return ui::translated("hudYaw", info.yaw ? formatAngle(*info.yaw) : ui::translated("unavailable"));
    if (id == "pitch") return ui::translated("hudPitch", info.pitch ? formatAngle(*info.pitch) : ui::translated("unavailable"));
    if (id == "sprinting") return info.sprinting && *info.sprinting ? std::optional{ui::translated("hudSprinting")} : std::nullopt;
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
        return ui::translated("hudSpeed", speed ? formatSpeed(speed->total) : ui::translated("unavailable"));
    if (id == "horizontalSpeed")
        return ui::translated("hudHorizontalSpeed", speed ? formatSpeed(speed->horizontal) : ui::translated("unavailable"));
    if (id == "verticalSpeed")
        return ui::translated("hudVerticalSpeed", speed ? formatSpeed(speed->vertical) : ui::translated("unavailable"));
    if (id == "time") {
        if (!info.worldTime) return ui::translated("hudTime", ui::translated("unavailable"), "");
        return ui::translated("hudTime", dayCount(*info.worldTime), formatClock(*info.worldTime));
    }
    if (id == "realTime")
        return ui::translated("hudRealTime", realTime.value_or(ui::translated("unavailable")));
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
void drawOffhandSlot(MinecraftUIRenderContext& context, ScreenView const& view, Settings::Information const& settings) {
    if (!settings.offhandSlot) return;
    auto* player = context.mClient.getLocalPlayer();
    if (!player) return;
    ItemStack const& source = player->getOffhandSlot();
    bool holding = !source.isNull() && source.mCount > 0 && source.mItem;
    if (!offhand::shown(settings.offhandSlot, holding, settings.offhandSlotEmpty)) return;
    auto hotbar = hotbarBox(view);
    glm::vec2 screen = *view.mSize;
    auto slot = hotbar.box ? offhand::slotBox(*hotbar.box, screen.x, screen.y) : std::nullopt;
    logPlacement(hotbar, slot, screen);
    if (!slot) return;
    // The hotbar's own pieces: a cap on each side of one slot image.
    float unit = slot->h / offhand::slotUnits;
    ui::images(context, "textures/ui/hotbar_start_cap", {{slot->x, slot->y, unit, slot->h}}, .65f);
    ui::images(context, "textures/ui/hotbar_0", {{slot->x + unit, slot->y, 20 * unit, slot->h}});
    ui::images(context, "textures/ui/hotbar_end_cap", {{slot->x + 21 * unit, slot->y, unit, slot->h}}, .65f);
    if (!holding) return;
    // A copy for this frame only: the renderer must not replay a pickup squash.
    ItemStack stack = source;
    stack.mShowPickUp = false;
    stack.mWasPickedUp = false;
    auto icon = offhand::iconBox(*slot);
    if (auto* renderer = context.mClient.getItemRenderer()) {
        BaseActorRenderContext renderContext(context.mScreenContext, context.mClient,
                                             context.mClient.getMinecraftGame_DEPRECATED());
        float x = std::round(icon.x), y = std::round(icon.y);
        // Compasses and clocks pick their frame as in an inventory slot.
        int frame = stack.mItem->getAnimationFrameFor(player, false, &stack, true);
        renderer->renderGuiItemNew(renderContext, stack, frame, x, y, false, 1.f, 1.f, unit, 17);
        // The glint pass and its strength as in container previews.
        if (stack.mItem->isGlint(stack))
            renderer->renderGuiItemNew(renderContext, stack, frame, x, y, true, 1.35f, 1.f, unit, 17);
    }
    int maxDamage = static_cast<int>(stack.mItem->getMaxDamage());
    if (inspection::render::shouldShowDurabilityBar(stack.isDamageableItem(), stack.getDamageValue(), maxDamage)) {
        // Vanilla's bar geometry in icon units, scaled with the slot.
        float ratio = inspection::render::durabilityRatio(stack.getDamageValue(), maxDamage);
        auto back = inspection::render::durabilityBackground({0, 0, 16, 16});
        auto front = inspection::render::durabilityForeground(back, ratio);
        auto color = inspection::render::durabilityColor(ratio);
        ui::fill(context, icon.x + back.x0 * unit, icon.y + back.y0 * unit, back.width() * unit, back.height() * unit,
                 ui::Rgb{0, 0, 0});
        if (front.width() > 0)
            ui::fill(context, icon.x + front.x0 * unit, icon.y + front.y0 * unit, front.width() * unit,
                     front.height() * unit, ui::Rgb{color.r, color.g, color.b});
    }
    if (stack.mCount > 1) slotCount(context, icon, unit, stack.mCount);
}
std::string biomeName(std::string const& identifier) { return localizedBiomeName(identifier); }
ui::hud_editor::Boxes drawHud(MinecraftUIRenderContext& context, float width, float height,
                              Settings::Information const& preferences, HudPreview const* preview) {
    ui::hud_editor::Boxes boxes;
    auto box = [&](ui::HudElementId id) -> auto& { return boxes[static_cast<size_t>(id)]; };
    auto settings = preferences;
    auto const snapshot = Runtime::instance().snapshot();
    auto const& runtime = *snapshot;
    cardOpacity = static_cast<float>(runtime.ui.hudBackgroundOpacity) / 100.f;
    auto const& hud = preview ? preview->layout : runtime.hud;
    auto viewRay = [&](double reach) -> std::optional<ViewRay> {
        auto* player = context.mClient.getLocalPlayer();
        if (!player) return std::nullopt;
        if (auto view = CameraSessions::instance().detachedViewRay(context.mClient))
            return ViewRay{view->x, view->y, view->z, view->dx, view->dy, view->dz, reach};
        auto eye = player->getEyePos();
        auto direction = player->getViewVector();
        return ViewRay{eye.x, eye.y, eye.z, direction.x, direction.y, direction.z, reach};
    };
    // Vanilla Hide HUD (F1) hides every element; recording and death
    // tracking below keep running (L-107).
    bool hidden = !preview && context.mClient.getOptions().getHideHud();
    if (settings.debug && !hidden && !preview) {
        // No player, no panel: invented values would read as real ones.
        if (auto values = collectDebugValues(context.mClient, viewRay(settings.targetDistance))) {
            auto style = settings.debugLabels == 1 ? DebugLabel::JavaF3 : DebugLabel::GameStandard;
            auto columns = buildDebugColumns(*values, style, debugGameText(*values));
            drawDebugColumns(context, width, height, columns.left, columns.right, settings.debugShadow,
                             static_cast<float>(runtime.ui.hudRowHeight), settings.debugBackground == 1);
        }
    }
    if (!preview) {
        map::record(context.mClient, runtime.map);
        map::waypoints::frame(runtime.map.waypointsDeath);
        // Under every HUD element: they point into the world.
        map::markers::draw(context, width, height, runtime.map);
        if (auto message = schematic::ghosts::takeSaveMessage()) ui::showMessageToast(std::move(*message));
        if (runtime.schematic.enabled) schematic::actions::adjustFrame(context, width, height);
    }
    if (hidden) {
        cardMorph = {};
        return boxes;
    }
    // Drawn first so every other element sits on top of the map.
    if (preview || (runtime.map.minimap && !(settings.debug && runtime.map.debugHide)))
        box(ui::HudElementId::Minimap) = map::drawMinimap(context, width, height, hud.minimap, runtime.map, preview != nullptr, cardOpacity);
    else if (!runtime.map.minimap) map::drawMinimap(context, width, height, hud.minimap, runtime.map, false);
    if (preview || runtime.ui.automationStatus || runtime.interaction.breaking) {
        std::vector<ElementLine> lines;
        if (runtime.ui.automationStatus) {
            // One line per switched-on button: "Auto Attack: Periodic", marked
            // when gameplay input is not Lamium's to drive right now.
            bool paused = interaction::periodic::paused(context.mClient);
            auto const& value = runtime.interaction;
            for (auto [on, mode, trigger, feature] : {
                     std::tuple{value.autoAttack, value.attackMode, value.attackHeldOnly, "feature.periodicAttack"},
                     std::tuple{value.autoUse, value.useMode, value.useHeldOnly, "feature.periodicUse"}}) {
                if (!on) continue;
                auto text = ui::translated(feature) + ": " + interaction::autoModeText(mode, trigger);
                if (paused) text += " " + ui::translated("autoPaused");
                lines.push_back({std::move(text), paused ? ui::palette::dim : ui::palette::accent});
            }
            if (interaction::sneak::active(context.mClient))
                lines.push_back({ui::translated("status.permanentSneak"), ui::palette::accent});
            if (interaction::sprint::active(context.mClient))
                lines.push_back({ui::translated("status.permanentSprint"), ui::palette::accent});
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
            // The anchor line only while a press holds a region (L-15).
            if (anchor)
                lines.push_back({ui::translated("breakingAnchor",
                    std::format("{}, {}, {}", anchor->anchor.x, anchor->anchor.y, anchor->anchor.z)), ui::palette::warning});
            lines.push_back({std::move(modeText), ui::palette::warning});
        }
        if (preview && lines.empty()) lines.push_back({ui::translated("status.permanentSneak"), ui::palette::accent});
        box(ui::HudElementId::Status) = drawElement(context, width, height, hud.status, lines, runtime.ui.hudRowHeight);
    }
    if (preview || (settings.target && !(settings.debug && settings.debugHideTarget))) {
        // One distance for every viewpoint: the body normally, the camera
        // during Freelook and FreeCamera (it looks elsewhere than the body).
        auto ray = viewRay(settings.targetDistance);
        auto target = collectTargetInfo(context.mClient, true, ray);
        if (!target && preview) {
            TargetInfo sample{ui::translated("feature.targetInfo"), "minecraft:grass_block"};
            sample.icon = {IconKind::Item, "minecraft:grass_block", 0};
            box(ui::HudElementId::Target) = drawTargetCard(context, width, height, hud.target, sample, settings, false);
        }
        if (target) box(ui::HudElementId::Target) = drawTargetCard(context, width, height, hud.target, *target, settings, !preview);
        else if (!preview) cardMorph = {};
    }
    if (preview || settings.durabilityHud)
        box(ui::HudElementId::Durability) = drawDurability(context, width, height, hud.durability, settings, preview != nullptr);
    if (preview || (runtime.schematic.enabled && runtime.schematic.hud))
        box(ui::HudElementId::Schematic) = drawSchematicHud(context, width, height, hud.schematic, runtime.schematic, preview != nullptr);
    if (preview || runtime.camera.showMagnification) {
        auto level = CameraSessions::instance().magnification(context.mClient);
        if (!level && preview) level = runtime.camera.magnification;
        if (level)
            box(ui::HudElementId::Magnification) = drawElement(context, width, height, hud.magnification,
                {{std::format("\u00d7{:.1f}", *level), std::nullopt, ui::palette::dim}});
    }
    if (preview || runtime.ui.toggleToasts) {
        auto toast = ui::currentToggleToast(ui::toastNow());
        if (!toast && preview) toast = ui::Toast::Visible{ui::translated("feature.toolSwitch"), true, 1.f};
        if (toast) {
            float zoom = elementZoom(hud.toast);
            // A message may hold several lines ("\n"), each centered in the box.
            std::vector<std::string> parts;
            for (size_t start = 0;;) {
                auto end = toast->text.find('\n', start);
                parts.push_back(toast->text.substr(start, end == std::string::npos ? std::string::npos : end - start));
                if (end == std::string::npos) break;
                start = end + 1;
            }
            float textWidth = 0;
            for (auto const& part : parts) textWidth = std::max(textWidth, ui::textWidthScaled(context, part, zoom));
            bool card = hud.toast.background == ui::ElementBackground::Card;
            float padX = card ? 6 : 0, padY = card ? 3 : 0;
            float lead = toast->plain ? 0 : ui::switchWidth + 6;
            float total = lead + textWidth + 2 * padX, rows = 14 * zoom * static_cast<float>(parts.size());
            auto frame = ui::placeElement(width, height, total, rows + 2 * padY, hud.toast);
            if (card) ui::card(context, frame.x, frame.y, total, rows + 2 * padY, cardOpacity * toast->opacity);
            box(ui::HudElementId::Toast) = ui::hud_editor::Box{frame.x, frame.y, total, rows + 2 * padY};
            ui::ElementPlacement placement{frame.x + padX, frame.y + padY};
            if (!toast->plain) ui::toggleSwitch(context, placement.x, placement.y + (14 * zoom - ui::switchHeight) / 2, toast->on);
            for (size_t i = 0; i < parts.size(); ++i)
                ui::labelScaled(context, placement.x + lead, placement.y + 14 * zoom * static_cast<float>(i), textWidth + 2,
                    parts[i], zoom, toast->opacity < 1 ? ui::palette::dim : ui::palette::text,
                    parts.size() > 1 ? ui::Align::Center : ui::Align::Left, hud.toast.shadow);
            context.flushText(0, std::nullopt);
        }
    }
    if (!preview && (!settings.hud || (settings.debug && settings.debugHideHud))) return boxes;
    PlayerInfoRequest request;
    request.coordinates = settings.coordinates || settings.scaledCoordinates || settings.block || settings.chunk
        || settings.speed || settings.horizontalSpeed || settings.verticalSpeed;
    request.dimension = settings.dimension || settings.scaledCoordinates;
    request.biome = settings.biome;
    request.facing = settings.facing || settings.yaw;
    request.light = settings.light;
    request.rotation = settings.rotation || settings.yaw || settings.pitch;
    request.time = settings.time || settings.moon;
    request.weather = settings.weather;
    request.difficulty = settings.difficulty;
    request.sprinting = settings.sprinting;
    auto info = collectPlayerInfo(context.mClient, request, cameraPose(context.mClient));
    if (!info.present) return boxes;
    auto timing = (settings.fps || settings.frameTime) ? frameStatistics() : std::optional<FrameStatistics>{};
    auto ping = settings.ping ? connectionPing(context.mClient) : std::optional<std::int64_t>{};
    bool anySpeed = settings.speed || settings.horizontalSpeed || settings.verticalSpeed;
    // Speed stays the body's; during FreeCamera position is the camera's.
    if (anySpeed && info.bodyPosition)
        speedSampler.sample(info.bodyPosition->x, info.bodyPosition->y, info.bodyPosition->z, ui::toastNow());
    else if (!anySpeed)
        speedSampler.reset();
    auto speed = anySpeed ? speedSampler.read() : std::optional<SpeedValues>{};
    auto realTime = settings.realTime ? localClock(settings.realTimeDate) : std::optional<std::string>{};
    std::vector<ElementLine> lines;
    int capacity = std::max(1, static_cast<int>((height - 8) / (14 * elementZoom(hud.info))));
    for (auto const& id : settings.lineOrder) {
        if (static_cast<int>(lines.size()) >= capacity) break;
        if (!infoLineEnabled(settings, id)) continue;
        if (auto text = infoLineText(id, info, timing, ping, speed, realTime, biomeDisplay(settings))) {
            if (info.camera && followsCamera(id)) *text = ui::translated("hudCameraTag") + " " + *text;
            lines.push_back({std::move(*text), {}});
        }
    }
    if (preview && lines.empty()) lines.push_back({ui::translated("feature.infoHud"), {}});
    box(ui::HudElementId::Info) = drawElement(context, width, height, hud.info, lines, runtime.ui.hudRowHeight);
    return boxes;
}
}

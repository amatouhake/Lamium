#pragma once
#include "input/Binding.h"
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>

// The schematic menu (BACKLOG L-93, L-238; docs/SCHEMATIC-CONTROLS.md): six
// categories, each a ring of items. A stepper changes a value with the wheel
// (a click steps once); a command runs on a click. What they do is glue.
// Each item names the keys that do the same (L-238: the menu and the keys
// reach the same operations), so the menu can show them.
namespace lamium::schematic::menu {
enum class Stepper : std::uint8_t { ForwardBack, LeftRight, UpDown, Rotate, Mirror, LayerAxis, LayerMode, Layer, LayerKeep, Placement, Target };
enum class Command : std::uint8_t {
    ToFeet, ResetTurn, LayerHere, ToggleShown, ToggleExtras, ToggleEntities, ToggleHud, ToggleFeature, ToggleMarks,
    Corner1Here, Corner2Here, MoveArea, MovePlacement, SaveArea, ClearArea,
    SelectLooked, PlaceFile, PlacedList, NearestMistake, CheckTab, MaterialsTab, KeySettings,
};
// What the Move category moves.
enum class Target : std::uint8_t { Placement, Corner1, Corner2, Area };

using input::Action;
inline constexpr Action noKey = Action::Count;
struct Item {
    std::string_view label; // translation key
    std::variant<Stepper, Command> what;
    // The keys for the same operation: a stepper's forward and back, or one.
    Action key = noKey, keyBack = noKey;
    bool stepper() const { return std::holds_alternative<Stepper>(what); }
};
struct Category {
    std::string_view label;
    std::span<Item const> items;
};

namespace detail {
using S = Stepper;
using C = Command;
using A = Action;
inline constexpr std::array<Item, 6> move{{
    {"schematic.menu.forwardBack", S::ForwardBack, A::MovePlacementForward, A::MovePlacementBack},
    {"schematic.menu.leftRight", S::LeftRight, A::MovePlacementLeft, A::MovePlacementRight},
    {"schematic.menu.upDown", S::UpDown, A::MovePlacementUp, A::MovePlacementDown},
    {"schematic.menu.toFeet", C::ToFeet, A::MovePlacementHere},
    {"schematic.menu.moveTarget", S::Target, A::CycleMoveTarget},
    {"schematic.menu.selectLooked", C::SelectLooked, A::SelectLookedPlacement}}};
inline constexpr std::array<Item, 5> placement{{
    {"schematic.menu.placement", S::Placement, A::NextPlacement, A::PreviousPlacement},
    {"schematic.menu.rotate", S::Rotate, A::RotatePlacement, A::RotatePlacementBack},
    {"schematic.menu.mirror", S::Mirror, A::MirrorPlacement},
    {"schematic.menu.resetTurn", C::ResetTurn, A::ResetPlacementTurn},
    {"schematic.menu.movePlacement", C::MovePlacement}}};
inline constexpr std::array<Item, 5> layers{{
    {"schematic.menu.layerAxis", S::LayerAxis, A::CycleLayerAxis},
    {"schematic.menu.layerCut", S::Layer, A::LayerUp, A::LayerDown},
    {"schematic.menu.layerMode", S::LayerMode, A::CycleLayerMode},
    {"schematic.menu.layerKeep", S::LayerKeep, A::ToggleLayerKeep},
    {"schematic.menu.layerHere", C::LayerHere, A::LayerHere}}};
// L-266: the HUD moved to Screen to make room for the mistake marks.
inline constexpr std::array<Item, 6> show{{
    {"schematic.menu.toggleFeature", C::ToggleFeature, A::ToggleSchematic},
    {"schematic.menu.toggleMarks", C::ToggleMarks, A::ToggleMistakeMarks},
    {"schematic.menu.toggleShown", C::ToggleShown, A::TogglePlacementShown},
    {"schematic.menu.toggleEntities", C::ToggleEntities, A::TogglePlacementEntities},
    {"schematic.menu.toggleExtras", C::ToggleExtras, A::TogglePlacementExtras},
    {"schematic.menu.nearestMistake", C::NearestMistake, A::NearestMistake}}};
inline constexpr std::array<Item, 5> area{{
    {"schematic.menu.corner1Here", C::Corner1Here, A::SchematicCorner1},
    {"schematic.menu.corner2Here", C::Corner2Here, A::SchematicCorner2},
    {"schematic.menu.moveArea", C::MoveArea},
    {"schematic.menu.saveArea", C::SaveArea, A::SaveSchematicArea},
    {"schematic.menu.clearArea", C::ClearArea, A::ClearSchematicArea}}};
inline constexpr std::array<Item, 6> screen{{
    {"schematic.menu.placeFile", C::PlaceFile, A::OpenSchematicFiles},
    {"schematic.menu.placedList", C::PlacedList, A::OpenSchematicPlaced},
    {"schematic.menu.checkTab", C::CheckTab, A::OpenSchematicCheck},
    {"schematic.menu.materialsTab", C::MaterialsTab, A::OpenSchematicMaterials},
    {"schematic.menu.toggleHud", C::ToggleHud, A::ToggleSchematicHud},
    {"schematic.menu.keySettings", C::KeySettings, A::OpenSchematicKeys}}};
}
inline constexpr int moveCategory = 0;
inline constexpr int showCategory = 3; // Holds the feature switch.
// With Schematics off, only what turns it on or opens a screen runs; the rest
// would act on placements and an area nothing draws.
inline constexpr bool worksWhileOff(Item const& item) {
    if (item.stepper()) return false;
    switch (std::get<Command>(item.what)) {
    case Command::ToggleFeature: case Command::ToggleHud: case Command::PlaceFile: case Command::PlacedList:
    case Command::CheckTab: case Command::MaterialsTab: case Command::KeySettings:
        return true;
    default: return false;
    }
}
inline constexpr std::array<Category, 6> categories{{
    {"schematic.menu.cat.move", detail::move}, {"schematic.menu.cat.placement", detail::placement},
    {"schematic.menu.cat.layers", detail::layers}, {"schematic.menu.cat.show", detail::show},
    {"schematic.menu.cat.area", detail::area}, {"schematic.menu.cat.screen", detail::screen},
}};
// Keys that only speed up one path and need no menu item (L-238): opening
// the menu or the screen, and Repeat (it repeats menu steps). Also the
// Check rules (L-266): set once rather than while building, they stay in
// the settings, with an optional key (maintainer 2026-10-26).
inline constexpr bool keyOnlyAccelerator(Action action) {
    return action == Action::SchematicMenu || action == Action::AdjustSchematic || action == Action::OpenSchematics
        || action == Action::ToggleIgnoreSignal || action == Action::ToggleLitAsState || action == Action::ToggleIgnoreGrowth;
}
// The "move ... ->" commands: which target they choose before opening Move.
inline constexpr bool choosesTarget(Command command, Target& target) {
    switch (command) {
    case Command::MovePlacement: target = Target::Placement; return true;
    case Command::MoveArea: target = Target::Area; return true;
    default: return false;
    }
}
// Steppers the adjust key may repeat: movement, rotation/mirror and the layer
// index. Target and placement pick something, and the layer axis/mode are
// view changes, not adjustments (the option `repeatAllSteps` adds them).
inline constexpr bool repeatable(Stepper stepper) {
    switch (stepper) {
    case Stepper::ForwardBack: case Stepper::LeftRight: case Stepper::UpDown:
    case Stepper::Rotate: case Stepper::Mirror: case Stepper::Layer: return true;
    default: return false;
    }
}
inline constexpr bool repeatableAll(Stepper stepper) {
    return repeatable(stepper) || stepper == Stepper::LayerAxis || stepper == Stepper::LayerMode
        || stepper == Stepper::LayerKeep;
}
// Where the menu opens: the category list, or (an option) the level shown
// when it was last closed.
inline constexpr int openAt(bool whereClosed, int closedAt) {
    return whereClosed && closedAt >= 0 && closedAt < static_cast<int>(categories.size()) ? closedAt : -1;
}
}

# Game updates: dependencies and playbook

What Lamium relies on in Minecraft Bedrock and LeviLamina, and what to do
when either updates (BACKLOG L-137). The inventory below is checked against
the code by `scripts/Check-GameInventory.ps1` (CI runs it): every file with a
hook is listed with exactly its hooks, so a new hook cannot land without an
entry here.

## What breaks on an update

- **Caught by a rebuild.** Hooks name game functions through the SDK, and
  member layouts come from the SDK headers, regenerated for each game
  version. Rebuilding against the new SDK follows moved members; removed or
  retyped functions and members fail to compile. An old Lamium on a new game
  does not load: `tooth.json` pins LeviLamina `26.51.*`, and LeviLamina is
  tied to one game version.
- **Not caught by a rebuild.** A member or field whose *meaning* changed
  (vertex streams, light texture fields, fog values, packet contents), a
  different render or input order, material and texture names looked up as
  strings, SDK header mistakes, and a different game executable under the
  same loader. These are listed as **Meaning** below and checked in game.

Only one place hard-codes a layout outside the SDK: `StackClassifier.cpp`
compares an item's first pointer with `ShulkerBoxBlockItem`'s exported
vtable (the MSVC object layout; checked in game when sorting).

## Version-sensitive capabilities (gates)

A path that writes game memory or relies on a meaning a rebuild cannot check
asks `versionSensitiveAllowed("<capability>")` (`app/Versions.h`) at that
path. On anything but the verified executable (today 1.26.51.01) it answers
no, logs the first no per capability, and the path stays vanilla; the rest
of the feature keeps working. A build configured with
`xmake f ... --unverified_game=y` answers no everywhere, so the fail-open
paths can be checked in game on the verified game.

| Capability (log name) | Where | Stays vanilla | Still works |
|---|---|---|---|
| FreeCamera terrain (native culler request) | `FreeCameraCulling.cpp` | underground terrain hidden as vanilla culls it | FreeCamera itself |
| Connected Textures (chunk mesh vertices) | `ConnectedTexturesHooks.cpp` | glass with borders | the setting (no effect) |
| Night Vision (light texture data) | `NightVision.cpp` | vanilla light | the key and setting (no effect) |
| Edge Guard (movement request) | `EdgeGuard.cpp` | vanilla movement | the setting (no effect) |
| Hide effects (fog and camera medium) | `HideEffects.cpp` `MediumFogVisibility` | water/lava/powder snow fog, distance fog | the other hide effects |
| Hide effects (weather densities) | `HideEffects.cpp` `WeatherVisibility` | rain and snow drawn | the other hide effects |
| Hide nausea (overlay mesh) | `HideEffects.cpp` nausea hooks | nausea swirl drawn | the effect icon and the rest |
| Schematic ghost blocks (block tessellation) | `GhostRenderer.cpp` | no ghost blocks or block actors | frames, mistakes list, materials, saving, map marks |
| Schematic entity models (actor renderers) | `GhostMarks.cpp` | missing entities as dashed frames | everything else |

## Playbook for a new Minecraft or LeviLamina version

1. **SDK bump.** Update `packages/levilamina-client-sdk.lua` and the
   `add_requires` version in `xmake.lua`, `tooth.json`'s LeviLamina range
   and the game version in DISTRIBUTION.md; refresh `xmake-requires.lock`.
2. **Build.** `xmake f ... -y`, `xmake build Lamium`, `LamiumTests`,
   `LamiumNativeTests`. Fix compile errors: they are the removed or retyped
   functions and members. Run `scripts/Check-GameInventory.ps1`.
3. **Fail-open first.** Before widening anything, run the new game with the
   normal build: every gated capability must log "stays vanilla" and the
   game must not crash. Smoke-test the features without gates (the inventory
   below) in a local world.
4. **Check the meanings.** For each **Meaning** entry below, do its in-game
   check on the new version (VALIDATION.md names the cases). Record results
   in VALIDATION-LOG.md with the game version.
5. **Widen the verified version** in `app/Versions.cpp` only after the
   gated capabilities' checks pass; if one fails, keep the old version, or
   split that capability's gate so the passing ones can be widened.
6. **Release** as usual (DISTRIBUTION.md); the release notes name the game
   version.

## Inventory

Per file with hooks: the hooks (checked by the script), and what it relies
on that a rebuild cannot catch (**Meaning**), or **Rebuild** when the SDK
signatures are all it relies on. *Trace* files compile only with a trace
or probe build option (`xmake.lua`) and never ship in a release.

### `src/features/camera/CameraInteraction.cpp`
Hooks: `LookStartBreak`, `LookContinueBreak`, `LookFinishBreak`, `LookStartBuild`, `LookContinueBuild`, `LookBuild`, `LookUse`, `LookUseAttack`, `LookUseOn`, `LookInteract`, `LookAttack`, `SurvivalStartBreak`, `SurvivalFinishBreak`, `SurvivalStartBuild`, `SurvivalBuild`, `SurvivalUse`, `SurvivalUseAttack`, `SurvivalUseOn`, `SurvivalInteract`, `SurvivalAttack`, `BodyHit`
Meaning: Freelook and FreeCamera redirect interactions to the body's view;
relies on GameMode and SurvivalMode calling these in the same order and on
the entity event id for being hit (FreeCamera "leave on hit").

### `src/features/camera/CameraSessions.cpp`
Hooks: `FovHook`, `FreeCameraSetupHook`, `FreeCameraInterpolatedPosition`, `TurnHook`, `DimensionHook`, `FocusHook`, `PerspectiveLockHook`, `ExtractFreeCameraInput`
Meaning: the camera matrices `setupCamera` produces, the interpolated actor
position used for rendering, turn deltas in degrees, the input extraction
order (movement read before the player moves). Checked by Zoom, Freelook
and FreeCamera smoke tests.

### `src/features/camera/CameraTrace.cpp`
Hooks: `CameraDependenciesTraceHook`, `CameraTraceHook`
Trace.

### `src/features/camera/FreeCameraCulling.cpp`
Hooks: `TerrainCullerRequest`, `TerrainPreRender`
Meaning: the native culler type values (3 requested, 5 retained) and the
virtual slot of `updateLevelCullerType`. Gated (FreeCamera terrain); also
checks the loader version and that the slot points into game code.

### `src/features/information/FrameTiming.cpp`
Hooks: `FrameCompleted`
Rebuild.

### `src/features/inspection/EnglishSearch.cpp`
Hooks: `SearchHook`
Meaning: the crafting screen filters by the text passed to `_filterByText`.

### `src/features/inspection/hover/HoverTracker.cpp`
Hooks: `ContainerSlotHoveredHook`, `CraftingSlotHoveredHook`, `ContainerSlotUnhoveredHook`, `ContainerScreenLeaveHook`
Meaning: hover events arrive per slot with the container's collection names.

### `src/features/inspection/Inspection.cpp`
Hooks: `ShulkerContentsText`, `DurabilityHovertext`, `TooltipFontDraw`, `TooltipPainter`
Meaning: the hover text's line layout and the placeholder glyph line the
painter covers with hunger-bar icons; fonts' sheet scales.

### `src/features/inspection/LockedTrades.cpp`
Hooks: `TierVisibleHook`, `OfferHook`
Meaning: the trade screen's binding names and the trade offer packet's NBT
(tiers, locked flags).

### `src/features/inspection/render/IconTrace.cpp`
Hooks: `SlotRenderHook`, `RenderTypeHook`, `ChunkHook`, `NewHook`, `BlockTypeHook`, `DataDrivenHook`, `EntityBlockHook`, `BatchKeyHook`, `ContextRenderHook`, `SharedBatchHook`, `TileHook`, `BlitHook`
Trace.

### `src/features/interaction/AutomationTrace.cpp`
Hooks: `Down`, `Up`
Trace.

### `src/features/interaction/BreakingRestriction.cpp`
Hooks: `FinishBreak`, `StopBreak`, `ChangeDimension`
Meaning: breaking goes through `GameMode::destroyBlock` once per block.

### `src/features/interaction/EdgeGuard.cpp`
Hooks: `EdgeGuardHook`
Meaning: the movement request's fields and collision shapes in
`fetchCollisionShapes`. Gated (Edge Guard).

### `src/features/interaction/InventoryMoveTrace.cpp`
Hooks: `InventoryMoveExtract`, `InventoryMoveClear`, `InventoryMoveLocks`, `InventoryMoveCalc`, `InventoryMoveUpdate`, `InventoryMoveSend`, `InventoryMoveCorrection`, `InventoryMoveAbsorb`, `InventoryMoveMapping`
Trace.

### `src/features/interaction/MiningSession.cpp`
Hooks: `SessionStart`, `SessionContinue`, `SessionStop`
Meaning: the start, continue and stop order of block breaking.

### `src/features/interaction/PeriodicInput.cpp`
Hooks: `RegisterDown`, `RegisterUp`, `DestroyOwner`, `Update`, `ChangeDimension`
Meaning: the button handler names Lamium presses through
(`button.destroy_or_attack`, `button.build_or_interact`) and
`InputHandler::tick` running once per client tick.

### `src/features/interaction/PermanentSneak.cpp`
Hooks: `ExtractSneakInput`, `SneakDimensionChange`
Meaning: the sneak bit in the extracted input.

### `src/features/interaction/PlacementTrace.cpp`
Hooks: `Calculate`, `TryPlace`
Trace.

### `src/features/inventory/FakeOffhand.cpp`
Hooks: `BuildAction`, `ReportUse`, `ReportOn`, `EquipmentEcho`, `HotbarEcho`, `ChangeDimension`
Meaning: the build action tick, the order of the server's equipment and
hotbar echoes after a slot change, and their packet fields.

### `src/features/inventory/FakeOffhandTrace.cpp`
Hooks: `TickBuild`, `HandleBuild`, `PressButton`, `ResetBai`, `ClearBai`
Trace.

### `src/features/inventory/game/ConsumptionTrace.cpp`
Hooks: `UseItem`, `UseItemOn`, `PlayerUseItem`, `CompleteUse`, `StartUse`, `StopUse`, `UseSelected`, `HurtAndBreak`
Trace.

### `src/features/inventory/game/InventoryMove.cpp`
Hooks: `MoveAddAction`
Meaning: inventory transactions are built from `addAction` calls the
server validates (Lamium only sends what vanilla could).

### `src/features/inventory/game/LegacyFlowTrace.cpp`
Hooks: `UpdateTransactions`, `PopulateLegacy`, `AddAction`, `SetItem`, `SetItemForceBalance`, `PickBlock`, `SwapSlots`, `SelectSlot`, `SendComplex`, `SendInventory`, `ItemStackResponse`, `LegacySlotUpdate`, `LegacyContentUpdate`
Trace.

### `src/features/inventory/game/RequestTracker.cpp`
Hooks: `ResponseHook`
Meaning: item stack responses report a request's id and result.

### `src/features/inventory/game/RestockTrace.cpp`
Hooks: `CreateHud`
Trace.

### `src/features/inventory/game/ScreenTracker.cpp`
Hooks: `ContainerScreenLeaveHook`
Rebuild.

### `src/features/inventory/game/TextInputTracker.cpp`
Hooks: `TextEditSelectedHook`
Rebuild.

### `src/features/inventory/HandRestock.cpp`
Hooks: `CaptureHud`, `Use`, `UseOn`, `StartUse`, `CompleteUse`, `FocusLost`, `ComplexSend`, `Drop`, `SlotUpdate`, `ContentUpdate`, `EntityEvent`
Meaning: the HUD container controller, the order of use, consume and the
server's slot updates, and the entity event for a totem pop. Restocks are
confirmed from the inventory, not from these events alone.

### `src/features/inventory/OffhandUseTrace.cpp`
Hooks: `Build`, `HandleBuild`, `Name`, `StartUse`
Trace (`Name` is the macro that makes several hooks).

### `src/features/inventory/WeaponSwitch.cpp`
Hooks: `WeaponAttack`, `WeaponSurvivalAttack`
Meaning: an attack runs `GameMode::attack` before the swing reaches the server.

### `src/features/lighting/NightVision.cpp`
Hooks: `BaseLightHook`, `NetherLightHook`
Meaning: `BaseLightData`'s night vision fields. Gated (Night Vision).

### `src/features/map/Minimap.cpp`
Hooks: `FrameAlphaHook`, `CommandListHook`
Meaning: the available commands packet lists `tp` or `teleport` when the
player may use it (world map teleport).

### `src/features/map/PlayerLocationTrace.cpp`
Hooks: `UpdateHook`, `HideHook`
Trace.

### `src/features/map/WaypointMarkers.cpp`
Hooks: `CameraCopyHook`, `CameraPositionHook`
Meaning: the camera copied from `setupCamera` is the one the world is drawn
with this frame.

### `src/features/research/ResearchTrace.cpp`
Hooks: `FovSampleHook`
Trace.

### `src/features/research/TradeTrace.cpp`
Hooks: `CollectionBindHook`, `GlobalBindHook`, `TradeHook`
Trace.

### `src/features/schematic/GhostActors.cpp`
Hooks: `GhostActorShader`, `GhostActorLightPair`
Meaning: block actor renderers set their light through these two
overloads. Used only for ghost block actors (gated with ghost blocks).

### `src/features/schematic/GhostProbe.cpp`
Hooks: `EffectsPassHook`
Trace (ghost probe).

### `src/features/schematic/GhostRenderer.cpp`
Hooks: `GhostBlendPass`, `GhostPass`
Meaning: the block tessellator's private fields Lamium sets on its own
tessellator (render layer, shape set, region), the mesh data streams
(positions, colors, light UVs) it edits, the alpha pass order, and the
material names `beacon_beam_transparent`, `holo_hand_pointer`. Gated
(Schematic ghost blocks; entity models in `GhostMarks.cpp`).

### `src/features/visuals/ConnectedTexturesHooks.cpp`
Hooks: `ConnectedBlock`, `Name`, `ConnectedPane`, `ConnectedPaneGlass`, `ConnectedRebuild`
Meaning: the chunk mesh's vertex positions and UVs for glass and panes,
`BlockGraphics` texture slots, and when chunks rebuild. Gated (Connected
Textures). `Name` is the macro that makes the per-shape hooks.

### `src/features/visuals/EffectTrace.cpp`
Hooks: `EffectSpriteTrace`, `EffectTextTrace`, `EffectCustomTrace`, `EffectFogTrace`, `EffectDensityTrace`, `EffectResolvedFogTrace`, `EffectMeshTrace`, `EffectMetadataMeshTrace`, `EffectSpanMeshTrace`, `EffectTextureBlitTrace`, `EffectVariantBlitTrace`, `EffectRectBlitTrace`, `EffectScreenStageTrace`, `EffectPostStageTrace`, `EffectVignetteTrace`, `UiTextureTrace`, `UiImageTrace`, `UiFlushTrace`, `ChunkItemTrace`, `EffectTessellatorTrace`
Trace.

### `src/features/visuals/HideEffects.cpp`
Hooks: `EffectLocalScreen`, `NauseaMeshVisibility`, `NauseaMetadataVisibility`, `BossBarSpriteVisibility`, `BossBarTextVisibility`, `LegacyParticleVisibility`, `DataParticleVisibility`, `RainParticleVisibility`, `RainEffectMapping`, `RainEmitterVisibility`, `MediumFogVisibility`, `WeatherVisibility`
Meaning: the nausea overlay's material and texture, the boss bar's control
names, particle effect names, the camera medium flags and fog values that
`setupFog` reads, and the weather density fields. Gated: fog and camera
medium, weather densities, the nausea mesh.

### `src/features/visuals/HideOffhand.cpp`
Hooks: `OffhandVisibility`, `OffhandAttachable`, `OffhandAttachableNoChecks`
Meaning: the off hand is drawn through these calls in first person.

### `src/input/CustomInput.cpp`
Hooks: `CustomInputFocusLost`
Rebuild.

### `src/overlay/WorldOverlay.cpp`
Hooks: `WorldLines`
Meaning: the overlay materials `debug`, `lightning`, `holo_hand_pointer`
and `selection_box` (LineColor.h), and that `renderEntityEffects` runs once
per frame with the world matrix stack set up.

### `src/ui/SettingsScreen.cpp`
Hooks: `SettingsSceneRender`, `SettingsWorldBackground`, `SettingsSceneExit`, `SettingsSceneEntrance`, `SettingsSearchText`
Meaning: the common dialog screen owns focus and the cursor while Lamium
draws over it, and text arrives through `handleTextChar`.

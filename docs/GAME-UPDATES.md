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
asks `versionSensitiveAllowed(capabilities::Id::...)` (`app/Versions.h`).
`app/Capabilities.h` owns the typed definitions, feature/partial-loss mapping,
compatibility policy, runtime stage and reason. All native adapters use one
loader policy: series 26.51, minimum patch 5; later patches do not need an
allowlist edit. This is not a promise that a patch cannot break an ABI: each
adapter retains its target, ownership and contract checks.

The minimum may stay below the SDK the DLL is built against only while every
LeviLamina symbol Lamium imports exists in that older loader:
`scripts/Check-LoaderImports.ps1 -Loader <LeviLamina.dll> ...` compares
them, and `Check-Capabilities.ps1` requires the record below for the current
SDK and minimum. The check covers symbol names, not the layout of
LeviLamina's own types.

- 2026-10-26: SDK 26.51.8 imports checked against loader 26.51.5, and
  26.51.7 (official client release DLLs; 68 imports, none missing).

The shared verified Minecraft executable remains 1.26.51.01. An unverified
executable or unsupported loader prevents probing and use of sensitive paths.
The catalog can name a separately reviewed executable build per capability,
without widening the others. No such exception is currently enabled.
`--unverified_game=y` forces all game-sensitive capabilities to stay vanilla,
including reviewed exceptions; version-independent paths remain available.

Settings are intent, not availability. Runtime stages are Waiting (first
applicable context), Ready, Stopped and Failed, with typed reasons. Hook-based
adapters publish readiness after attachment; lazy adapters such as FreeCamera
terrain wait for their native check. Direct SDK paths do not invent an extra
render prerequisite. Missing world/player/target data merely skips that call;
it does not create a permanent failure. Failure latches until an explicit
restart, and failed hook cleanup requires a process restart. The registry
describes adapter availability, not the outcome of every operation: a failed
language-change request, an unreadable texture or a failed individual draw
keeps its existing local result/fallback and does not permanently disable a
valid adapter. Durable hook/target/contract failures do.
The registry
keeps only owned atomic values, no game pointers; successful hot-path checks
do not log, allocate strings or take a mutex.
Night Vision, repeated trading, Connected Textures and server-pack skipping
can roll back a partial start behind their shared gate: a startup failure
leaves those adapters unavailable while Lamium and its settings UI continue.
Failures of critical input/render foundations retain the existing full
startup rollback; this catalog does not make an unsafe partial foundation
usable.

Execution and UI read this same registry. `PausedFeatures.h` derives its
feature list and loss level from the catalog; it only projects selected
settings for notices. Settings suffixes/header, full description help and
version tooltips reflect real failures even on the verified game. Waiting
appears as waiting in help, not as a paused warning. World-entry and new
runtime-failure notices explain requested affected features, without repeating
unchanged results. Every production capability needs a translated name/reason
and a row below. `scripts/Check-Capabilities.ps1` checks this inventory and
the package/SDK loader-policy alignment in CI.

| Capability (log name) | Where | Stays vanilla | Still works |
|---|---|---|---|
| FreeCamera terrain (native culler request) | `FreeCameraCulling.cpp` | underground terrain hidden as vanilla culls it | FreeCamera itself |
| Connected Textures (chunk mesh vertices) | `ConnectedTexturesHooks.cpp`, `ConnectedSprites.cpp` | glass with borders | the setting (no effect) |
| Night Vision (light texture data) | `NightVision.cpp` | vanilla light | the key and setting (no effect) |
| Edge Guard (movement request) | `EdgeGuard.cpp` (movement request and move vector), `EdgeGuardHistory.cpp` (replay input capture) | vanilla movement and history capture | the setting (no effect) |
| Edge Guard (replay capture) | `EdgeGuardHistory.cpp` | vanilla replay capture | ordinary movement guarding |
| Repeated trades (trade row handlers) | `RepeatTrades.cpp` | vanilla single trading | the setting and key |
| Item Transfer (native slot routes) | `TransferRoutes.cpp`, `TransferSession.cpp` | no new gestures, drops or additional screens; no native hover/route filtering | the original four gestures in ordinary storage and inventory screens |
| Hide effects (fog and camera medium) | `HideEffects.cpp` `MediumFogVisibility` | water/lava/powder snow fog, distance fog | the other hide effects |
| Hide effects (weather densities) | `HideEffects.cpp` `WeatherVisibility` | rain and snow drawn | the other hide effects |
| Hide effects (portal screen) | `HideEffects.cpp` `PortalScreenVisibility` | the purple Nether portal screen | the other hide effects |
| Hide nausea (overlay mesh) | `HideEffects.cpp` nausea hooks | nausea swirl drawn | the effect icon and the rest |
| Schematic ghost blocks (block tessellation) | `GhostRenderer.cpp`, `Preview.cpp` | no ghost blocks, block actors or 3D preview, no in-world mistake outlines (built with the sections) | frames, the Check list, materials, saving, map marks |
| Schematic piston ghosts (piston actor state) | `GhostActors.cpp` (`PistonBlockActor` progress and state) | piston ghosts as loaded from the file | the rest of the ghosts |
| Schematic check (shulker box facing) | `GhostVerify.cpp` (`ShulkerBoxBlockActor::mFacing`) | a shulker box judged by its block alone | the rest of the check |
| Schematic entity models (actor renderers) | `GhostMarks.cpp`, `Preview.cpp` | missing entities as dashed frames, no preview entity models | everything else |
| Game language (native apply) | `GameLanguage.cpp` | no in-world language list; vanilla title-screen Settings > Language | everything else |
| Skip server resource packs (pack stack packet) | `ServerPacks.cpp` | server packs applied as vanilla | the setting and key (no effect) |
| FreeCamera hide (hotbar control) | `FreeCameraHide.cpp` `FreeCameraHud` (the hotbar control's visibility, restored after the draw) | the hotbar stays drawn in FreeCamera | hiding the hand and the whole HUD |
| Fake Offhand (item use targets) | `FakeOffhand.cpp` `targetUse` (planter component by name, copper fields, `canBeFertilized`, `isRailBlock`) | the primary item keeps priority on blocks as before L-189 | air checks, the rest of Fake Offhand |
| Chat position (HUD control positions) | `ChatPosition.cpp` | vanilla chat position | the setting, key and layout preview |
| FreeCamera hide (hand pass) | `FreeCameraHide.cpp` | hand drawn | FreeCamera and independent HUD paths |
| FreeCamera hide (HUD pass) | `FreeCameraHide.cpp` | whole HUD drawn | FreeCamera and hand hiding |
| Hide effects (particle passes) | `HideEffects.cpp` | particles drawn | independent hide selections |
| Hide effects (boss bar passes) | `HideEffects.cpp` | boss bars drawn | independent hide selections |
| Schematic ghosts (blended pass) | `GhostRenderer.cpp` | no blended ghost pass | ordinary ghosts and checking |
| Schematic ghosts (actor lighting) | `GhostActors.cpp` | vanilla ghost actor lighting | ghost rendering and checking |

## Playbook for a new Minecraft or LeviLamina version

1. **SDK bump.** Update `packages/levilamina-client-sdk.lua` and the
   `add_requires` version in `xmake.lua`, `tooth.json`'s LeviLamina range
   and the game versions in README.md ("Supported: Minecraft Bedrock" with
   the full build number, install steps, Target); refresh
   `xmake-requires.lock`.
2. **Build.** `xmake f ... -y`, `xmake build Lamium`, `LamiumTests`,
   `LamiumNativeTests`. Fix compile errors: they are the removed or retyped
   functions and members. Run `scripts/Check-GameInventory.ps1` and
   `scripts/Check-Capabilities.ps1`. Update the shared loader series/minimum
   together with the SDK and package, never a feature-local patch list; to
   keep an older minimum, run `scripts/Check-LoaderImports.ps1` against that
   loader's DLL and record the result under the gates above.
3. **Fail-open first.** Before widening anything, run the new game with the
   normal build: every gated capability must log "stays vanilla" and the
   game must not crash. Smoke-test the features without gates (the inventory
   below) in a local world.
4. **Check the meanings.** For each **Meaning** entry below, do its in-game
   check on the new version (VALIDATION.md names the cases). Record results
   in VALIDATION-LOG.md with the game version.
5. **Widen the verified version** (`verifiedGame` in `app/Versions.cpp`)
   only after the gated capabilities' checks pass; if one fails, keep the
   old version, or record a separately reviewed `verifiedBuild` for the passing
   capabilities in the shared catalog. Until then `scripts/New-ReleaseArchive.ps1` warns on every build
   and refuses a tagged release: the SDK's game version (LeviLamina
   `26.51.x` targets Minecraft `1.26.51`) and README's full supported version
   must match it.
6. **Release** as usual (DISTRIBUTION.md); the release notes name the game
   version.

## Inventory

Per file with hooks: the hooks (checked by the script), and what it relies
on that a rebuild cannot catch (**Meaning**), or **Rebuild** when the SDK
signatures are all it relies on. *Trace* files compile only with a trace
or probe build option (`xmake.lua`) and never ship in a release.

### `src/app/ClientTasks.cpp`
Hooks: `ClientTasksMenuUpdate`
Meaning: `MinecraftGame::update` runs on the client thread every frame, also
before a world is joined, where LeviLamina's client thread executor does not
run (L-175, 2026-10-14).

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
uses the common loader policy and checks that the slot points into game code.

### `src/features/camera/FreeCameraHide.cpp`
Hooks: `FreeCameraHand`, `FreeCameraHud`
Meaning: the first-person hand pass (`ItemInHandRenderer::renderFirstPerson`)
and the HUD screen's draw (`ScreenView::render`, the `.hud_screen` root
name, the hotbar control names `desktop_hotbar`, `pocket_hotbar`,
`hotbar_panel` and their `mVisible` flag, set back after the draw). The
hotbar part is gated (FreeCamera hide).

### `src/features/information/DebugVanillaHud.cpp`
Hooks: `DebugPositionSprite`, `DebugPositionText`, `DebugPaperDoll`
Meaning: the vanilla HUD's position panel is named `player_position` (label
`player_position_text`) and the HUD paper doll is drawn by
`HudPlayerRenderer`, its control named `hud_player` (where the Info HUD
covers it, L-145). A renamed control only stops the hiding.

### `src/features/information/StatusEffectsVanilla.cpp`
Hooks: `VanillaEffectIcons`
Meaning: `HudMobEffectsRenderer` draws the HUD's effect icons on the right
(the hud screen's `mob_effects_renderer`). Skipped only while the Status
Effects HUD read the effects that frame; a change only stops the hiding or
hides those icons with the HUD showing the same effects (L-147).

### `src/features/information/ChatPosition.cpp`
Hooks: `ChatHudRender`, `ChatTextRender`, `ChatBackgroundRender`
Meaning: the HUD has `chat_panel` immediately under an unclipped
`chat_stack`, with native `chat_text`/`chat_background` components. The
screen hook establishes the render scope; each component uses the panel's
current measured size after native layout, translates only its cached
position while drawing, then restores it without invalidating the tree.
Gated (Chat position); renamed controls, clipping, invalid geometry or an
ancestor walk beyond 16 controls leave it vanilla. Basic scope and the
message-expiry/long-message regression passed on `b28a40d` (L-174);
broader coverage remains in VALIDATION.

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
Hooks: `EdgeGuardHook`, `EdgeGuardInputHook`, `EdgeGuardSendHook`, `EdgeGuardPlaceHook`, `EdgeGuardSurvivalPlaceHook`, `EdgeGuardBuildHook`, `EdgeGuardSurvivalBuildHook`, `EdgeGuardStartBuildHook`, `EdgeGuardSurvivalStartBuildHook`, `EdgeTraceSend`, `EdgeTraceCorrection`, `EdgeTraceBuild`, `EdgeTraceSurvivalBuild`, `EdgeTraceHistoryFrame`, `EdgeTracePrepare`, `EdgeTraceApply`, `EdgeTraceReplay`, `EdgeTraceSelectedInput`, `EdgeTraceCapture`, `EdgeTracePreApply`, `EdgeTracePostApply`
Meaning: the movement request's fields and collision shapes in
`fetchCollisionShapes`; `PlayerMovement::calculateMoveVector` giving the move
vector the client both simulates and sends (move x positive to the left), called
with the local player's own input states; `startBuildBlock`, `buildBlock` and
`useItemOn` (base and survival) before a slab is placed, with the clicked block
and face (the held item read from the building hand). Gated (Edge Guard).
L-162 history mutations are removed: guarding only `ActorHistory::addFrame`
did not prevent full physical keys reappearing during replay, and persisting
processed horizontal input in `mRawInputState` broke normal WASD operation.
The live input guard remains as before these experiments. The actual capture
copy candidate is isolated in `EdgeGuardHistory.cpp`; live physical input
and the prepared entity snapshot are never substituted.
The outgoing `PlayerAuthInputPacket` raw move vector is aligned with guarded
analog input at `LoopbackPacketSender::send`, before the send diagnostic.
Require the client thread, matching sender subclient, enabled/gated walking
player, no direction keys, no jump/teleport/vehicle exit, and finite packet
move/analog values matching the exact live component calculation. Only the
packet's raw vector changes; device/component input, packet position, flags,
look and placement transactions remain untouched. The analog vector is used
unscaled (sneak movement is already scaled separately). BDS `e8b0a52`
verifies matching outgoing raw/analog, normal controls and slab bridging,
with twelve correction windows and no fall dump. The server's internal use
of the raw field is not established; incoming corrections are not clamped
and already unsupported positions are not recovered (L-162).
Trace-only (L-162): `ActorHistory::addFrame`,
`ActorHistory::prepareEntityForRewindFromCapture`,
`ClientPlayerRewindSystem::applyHistoryChangesToLiveEntity` and
`EntitySystems::tickMovementCorrectionReplay`. Observe the local player's
history frame, captured/live/rewind inputs, boxes and motion, and the result
applied to live state. All use exported SDK methods; reads are gated and
restricted to the client thread. No history pointers or replay writes.
The selected-input diagnostic also observes exported
`ReplayStateComponent::getCurrentCapturedInput`. For the exact local
component it binds read-only capture/preApply/postApply diagnostics to the
selected input's implementation. Slots are decoded from bounded local
compiler-generated SDK call thunks (measured clang-cl load-then-jump forms),
not assumed from declaration order; native tests exercise the actual SDK
thunks. Table entries must be readable game data and targets executable
game code; unsupported forms, changed implementations or attachment failure
are logged and left alone. Only function addresses persist, no input/entity
pointers. Capture records local processed/raw inputs; application records
the actual replay entity before and after vanilla applies stored input.

### `src/features/interaction/EdgeGuardHistory.cpp`
Hooks: `EdgeGuardCaptureHook`
Meaning: the actual selected `IReplayableActorInput::capture` copies the
local player's `MoveInputComponent` for later correction replay. Under the
Edge Guard capability, exact local component, client-thread and walking
checks, pass an owned component copy whose raw horizontal flags and analog
vector agree with its already processed input. Jumping stays vanilla;
non-horizontal flags, look and other component fields are preserved. Live
input is never written. This avoids the rejected persistent-input feedback.
Resolve the selected implementation through `getCurrentCapturedInput`,
using the same bounded compiler-thunk decoder and readable/executable game
page checks as the diagnostics. Unsupported or changed targets and hook
failures leave capture vanilla. Only code addresses persist. The capture
must own the supplied values rather than retain a component reference;
two BDS correction replays verify that guarded raw/processed values survive
preApply, calculation and postApply (`4fe7b29`, L-162). This does not guard
against a correction that already moves the feet off support: a fall while
slab bridging is recorded despite preserved input, and L-162 stays open.

### `src/features/visuals/PortalTrace.cpp`
Hooks: `PortalPackets`, `PortalInsideCheck`, `PortalScreenValid`, `PortalCraftingScreenValid`, `PortalModelValid`, `PortalCraftingValid`, `PortalSchedulePop`, `PortalInventoryKey`, `PortalOpenInventory`, `PortalCanOpen`, `PortalInside`, `PortalTick`
Trace.

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

### `src/features/inventory/RepeatTrades.cpp`
Hooks: `RepeatTradeRow`, `RepeatTradeInput`, `RepeatTradeTick`, `RepeatTradeLeave`, `RepeatTradeRefresh`
Meaning: the global Shift + right-click button ID, remapped to the row's own
right-click mapping only on components that map it to the trade-row event;
Trade2 row event IDs and collection indexes, ingredient preparation,
the native auto-trade callback, and predicted offer uses reconciled against
successful owned request responses and resulting inventory. Gated (Repeated
trades); unknown routes stay vanilla. No transaction is constructed by Lamium.

### `src/features/inventory/game/RestockTrace.cpp`
Hooks: `CreateHud`
Trace.

### `src/features/inventory/game/ScreenTracker.cpp`
Hooks: `ContainerScreenLeaveHook`
Rebuild.

### `src/features/inventory/game/TransferRoutes.cpp`
Hooks: `TransferAutoPlaceOrder`, `TransferAutoPlaceStackOrder`, `TransferAutoPlaceOutputsOrder`, `TransferDropSelected`
Meaning: the screen's registered collection/slot-role order and native hover
slot. Native orders were observed in the L-169 e3f9276 diagnostic checkpoint;
the additional-screen hover-field mapping awaits the new-gesture smoke test.
Only the manager
call inside Lamium's synchronous auto-place invocation gets a filtered
order; vanilla input is unchanged. Gated at discovery/invocation; SDK
layouts and method signatures rebuild. Verify amount, role restrictions,
request IDs and authoritative slot counts after a game update.
Drops (L-169): `_handleDropItem` and `_handleDropSelectedItem` take
ItemTransferAmount, whose fields the SDK does not name; Lamium passes the
bytes traced on 1.26.51.01 (`dropAmountBytes`: take type 1, then 2 for one
item or 0 for the stack) by address, as the 12-byte argument is passed.
`TransferDropSelected` observes the drop of the cursor's items (a click
outside the window) and only reads the cursor first. Re-trace with
`transfer_trace` after a game update: the drop key and Ctrl + drop key
over a slot, left and right click outside with items held.

### `src/features/inventory/game/TransferTrace.cpp`
Hooks: `TransferTakeAllTrace`, `TransferPlaceAllTrace`, `TransferAutoPlaceTrace`, `TransferAddAllTrace`, `TransferTakeAmountTrace`, `TransferTakePlaceTrace`, `TransferPlaceOneTrace`, `TransferTakeHalfTrace`, `TransferButtonTrace`, `TransferDropItemTrace`, `TransferDropSelectedTrace`, `TransferManagerDropTrace`, `TransferResponseTrace`
Trace (`transfer_trace`): read-only L-169 input, cursor, collection and native
auto-place order observations; screen button ids, cursor handlers, the
open request and unsent batch size, and item stack responses with times. Protected by the game-version gate. The
after-handler snapshot is prediction, not authoritative completion.

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

### `src/features/research/LanguageTrace.cpp`
Hooks: `LanguageSettingsTick`, `LanguageSelected`, `LanguageChosen`, `LanguageGameChanged`, `LanguageFontChanged`, `LanguagePacksChanged`, `LanguagePackStack`, `LanguageServerPacks`, `LanguageServerStack`
Trace. L-167 observations; read-only except the research-only `server_pack_skip` option, which empties the server stack's resource packs before the client applies it.

### `src/features/research/ResearchTrace.cpp`
Hooks: `FovSampleHook`
Trace.

### `src/features/research/TradeTrace.cpp`
Hooks: `CollectionBindHook`, `GlobalBindHook`, `TradeHook`
Trace.

### `src/features/research/RepeatTradeTrace.cpp`
Hooks: `RepeatOffer`, `RepeatButton`, `RepeatSlotPressed`, `RepeatTick`, `RepeatLeave`, `RepeatRequestBatch`, `RepeatResponse`, `RepeatInventorySlot`, `RepeatInventoryContent`, `RepeatNotify`, `RepeatActorEvent`, `RepeatAutoPlace`, `RepeatViewButton`, `RepeatComponentButton`, `RepeatPressedMapping`
Trace (L-166, read-only; `repeat_trade_trace`, independent of `trade_trace`).

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

### `src/features/visuals/ConnectedSprites.cpp`
Hooks: `SpriteAtlas`, `SpritePackIndex`, `SpritePackLoad`
Meaning: the terrain atlas's `texture_data` entries (`texture_name`
"atlas.terrain"), the resource pack lookup (`getPackStackIndexOfResource`:
-1 makes the atlas skip an image; the string is the matched extension) and
the pair overload of `load` the atlas reads images through; the sprite
places from `BlockGraphics::getTextureUVCoordinateSet`. Gated (Connected
Textures): on an unverified game nothing is added.

### `src/features/visuals/ConnectedTexturesHooks.cpp`
Hooks: `ConnectedBlock`, `Name`, `ConnectedPane`, `ConnectedPaneGlass`, `ConnectedRebuild`
Meaning: the chunk mesh's vertex positions and UVs for glass and panes,
`BlockGraphics` texture slots, and when chunks rebuild. Gated (Connected
Textures). `Name` is the macro that makes the per-shape hooks.

### `src/features/visuals/EffectTrace.cpp`
Hooks: `EffectSpriteTrace`, `EffectTextTrace`, `EffectCustomTrace`, `EffectFogTrace`, `EffectDensityTrace`, `EffectResolvedFogTrace`, `EffectMeshTrace`, `EffectMetadataMeshTrace`, `EffectSpanMeshTrace`, `EffectTextureBlitTrace`, `EffectVariantBlitTrace`, `EffectRectBlitTrace`, `EffectScreenStageTrace`, `EffectPostStageTrace`, `EffectVignetteTrace`, `UiTextureTrace`, `UiImageTrace`, `UiFlushTrace`, `ChunkItemTrace`, `EffectTessellatorTrace`
Trace.

### `src/features/visuals/HideEffects.cpp`
Hooks: `EffectLocalScreen`, `NauseaMeshVisibility`, `NauseaMetadataVisibility`, `BossBarSpriteVisibility`, `BossBarTextVisibility`, `LegacyParticleVisibility`, `DataParticleVisibility`, `RainParticleVisibility`, `RainEffectMapping`, `RainEmitterVisibility`, `MediumFogVisibility`, `WeatherVisibility`, `PortalScreenVisibility`
Meaning: the local player's portal effect time (`mPortalEffectTime`, 0 to 1 in
a portal; the purple screen follows it); the nausea overlay's material and texture, the boss bar's control
names, particle effect names, the camera medium flags and fog values that
`setupFog` reads, and the weather density fields. Gated: fog and camera
medium, weather densities, the nausea mesh.

### `src/features/visuals/HideOffhand.cpp`
Hooks: `OffhandVisibility`, `OffhandAttachable`, `OffhandAttachableNoChecks`
Meaning: the off hand is drawn through these calls in first person.

### `src/features/visuals/ServerPacks.cpp`
Hooks: `ServerPacksJoin`, `ServerPacksStack`
Meaning: `onStartJoinGame`'s first argument says whether the join is a local
world; `ResourcePackStackPacket` arrives after the download/acceptance
handshake, and its `mTexturePackIdsAndVersions` is the resource stack the
client applies (emptying it leaves vanilla textures, checked on BDS
2026-10-14). Gated: the packet change.

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

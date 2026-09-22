# Development validation

Baseline: Minecraft 1.26.51.01, LeviLamina Client 26.51.3, Windows x64.

## Info HUD prototype

Info HUD is off by default, with an initially unbound Toggle action. Coordinates
and dimension name can be enabled separately. Horizontal/vertical positions
use 0–100 percent anchors within available screen space, including margins and
line height; both support numeric editing. The HUD draws minimal text without
a card, in gameplay and behind Lamium settings for live placement feedback.
It reads current local-player values only, retains no entity pointers, and
draws no content without a local player. Pure layout, settings round-trip,
translation, and action tests pass. Actual HUD visibility, text fit, position
editing, GUI scaling, dimension changes, and resource packs are unverified.
Biome and cardinal facing are now optional lines, disabled by default. A shared
player-information collector returns owned optional values rather than retaining
game pointers, and queries only requested fields. Biome is read at the floored
player block position only when a client chunk exists; unavailable values are
shown explicitly. Biome names are engine identifiers, not localized display
names. Direction tests cover cardinal yaw, wraparound, sector boundaries, and
invalid input. The actual yaw-axis convention and biome results remain runtime
checks. Ping, light, WAILA/F3 consumers, line ordering, and additional display
controls remain unfinished. UI callback frequency is not used as FPS.

Client FPS and mean frame interval are now optional HUD lines. A hook samples
steady-clock timestamps after `MinecraftGame::endFrame`; windows of at least
half a second publish completed intervals divided by elapsed time and the
reciprocal mean interval. These are frame-completion cadence measurements, not
GPU execution time, server TPS, or MSPT. A gap over two seconds clears the window
and stale values become unavailable; disable/re-enable resets it too. Tests
cover 60/30 Hz, unequal intervals, stale data, duplicate/backward timestamps,
and suspension recovery. Runtime validation must establish one callback per
actual frame and compare the readings against an independent frame counter,
including menus, minimized windows, low frame rates, and loading transitions.

## Tool Switch prototype

Tool Switch is off by default, with a configurable initially unbound Toggle
action. Before vanilla `GameMode::startDestroyBlock`, it evaluates only slots
0–8 for the local player, excluding Creative/Spectator and settings ownership.
A held item with finite destroy speed above 1 and the required harvesting
capability is retained even if another hotbar tool is faster. Otherwise it picks
the fastest eligible hotbar tool (first slot on ties) through the existing
`PlayerInventory::selectSlot` API. It never moves, drops, or replaces stacks.
Selection tests cover retaining effective tools, wrong tiers, ties, invalid
speeds, and invalid selected slots; catalog persistence tests cover its setting.
Actual mining, continuous mining between blocks, special tools/blocks,
enchantments, selected-slot synchronization, Adventure restrictions, and remote
servers remain unverified. The eligibility rule uses Item destroy speed and
the block's correct-tool-for-drops flag, not a prediction of final break time.

## Offhand visibility prototype

Hide Offhand Item is an opt-in setting with an initially unbound Toggle action
in Features/Hotkeys. Its hook skips `ItemInHandRenderer::renderOffhandItem` only
when the SDK FirstPersonPass flag is present and WorldPass/UIPass are absent.
It writes no equipment, item stacks, use state, or network messages. Existing
settings keep the offhand visible. Catalog/persistence/localization tests pass.
Runtime behavior is unverified: check shields while blocking, totems, maps,
main-hand rendering, third-person/paper-doll views, toggling, and world changes.
Special item render paths may need additional coverage after observation.

## Overlay geometry foundation

Hitboxes is an opt-in consumer of the world-line renderer, with an unbound
Toggle action and editable display distance (8–128 blocks, default 64). It reads
the local player's client level actor list during the render pass, skips the
local player and other dimensions, rejects invalid/degenerate bounds, and draws
white AABB edges within the configured camera-to-box distance. No actor pointer
is retained across frames and no server data is requested. Unit tests cover
nearest-face distance, inclusive boundaries, invalid boxes/camera, persistence,
and settings/action reachability. Actual actor enumeration lifetime, render
placement/depth, moving-entity jitter, crowded-world performance, dimension
changes, and unload remain unverified. Eye/look-direction markers and the local
player's third-person box are not implemented yet.

The game-independent geometry component now provides continuous lines/wire boxes,
block-grid circle/cylinder/sphere cells, rectangular planes/grids, exposed faces,
and outward face vertices. Geometry tests and the existing unit suite passed.
A world-line render hook and an opt-in Chunk Borders setting now use the line
geometry. DLL compilation/linking, the complete unit suite, and package checks
passed. Tests cover negative chunk coordinates and dimension-height section
lines, plus settings persistence and feature-list reachability. The hook has
not been exercised in Minecraft: visible output, correct camera transforms,
depth, mesh lifetime, world exit, and dimension changes remain unverified.
Block-grid shapes are not connected to the renderer or an editor yet. See
[overlay conventions](OVERLAYS.md) for sampling and remaining integration work.

Chunk Borders also has a Toggle action in Features and Hotkeys. Its native
default is unbound; custom chords and native remaps toggle the same persisted
setting, only during gameplay and outside Lamium input ownership. The action
catalog, localized labels, and settings-row coverage pass the unit suite.
Native unbound registration, rebinding, and actual toggle behavior still need
Minecraft validation alongside the renderer.

## Settings foundation in progress

Panel, row-background, and label drawing now live in shared UI widgets rather
than the settings screen. Labels use native font widths to shorten overflowing
text with an ellipsis while preserving UTF-8 codepoints. Tests cover exact fits,
ASCII/Japanese/four-byte characters, and very narrow or invalid widths. Tests
use a deterministic width function; actual Bedrock font metrics, UI scale,
resource-pack fonts, and visual readability remain runtime checks.

Magnification and wheel step now have an inline decimal editor, opened by click
or Enter. The initial value is selected for replacement; Ctrl+A reselects it,
Backspace edits, and Enter/Escape finish editing without rolling back values
already saved. Valid in-range input applies on the next render; incomplete or
out-of-range input leaves the last saved value unchanged. Left/right adjustment
remains available outside text editing. Pure tests cover replacement, decimal
precision, intermediate signs/decimal points, invalid characters, bounds, and
numeric catalog setters. The full unit suite passes. Actual text delivery,
focus, layout, and autosave interaction still require Minecraft validation.

Features now starts as a collapsed list of feature headers showing state and
binding. Expand a feature to edit its options and binding together. Search
temporarily reveals matching children even in collapsed groups; clearing search
restores the collapse choices. Hotkeys ignores feature collapse. A short feature
description follows the selected row. Row generation is a game-independent
component with tests proving that all options/actions remain reachable exactly
once, children stay under the correct feature, English/Japanese search reveals
collapsed matches, and unmatched queries produce no rows. These tests do not
verify in-game text fit, click targets, focus, or scrolling after expansion.

The binding model and storage format now distinguish native Minecraft mappings,
explicit Unbound, and custom chords. Action metadata owns Press/Hold/Toggle
semantics. Pure tests cover arbitrary chord order, repeated key-down suppression,
release of any chord member, reset release, modified wheel impulses, invalid
inputs, and persistence/reset without losing unrelated bindings. These are
components that now feed native key/mouse event dispatch for custom overrides;
actions without overrides still use Minecraft registrations. Explicit Unbound
suppresses the native handler too. Gameplay hints show the effective binding.
Native registrations and custom chords share the action executor and setting
toggle logic. Native defaults are recorded in action metadata; tests preserve
F8/C/J/R and verify that each Toggle action edits only one setting in its owning
feature, while Press/Hold actions do not edit toggle settings. Both paths retain
input ownership/gameplay checks; Sort delegates its context checks to inventory
handling. Callback delivery and remapping still need runtime validation.
The in-game binding editor is connected in source, pending runtime validation.
Features places each action binding after its related options; Hotkeys lists
all actions. Clicking a binding captures keys or mouse buttons until a captured
input is released; wheel impulses complete immediately. The opening click/Enter
is excluded until released. Clear selects Unbound; Reset restores the existing
Minecraft mapping. Escape cancels, and app focus loss abandons the capture.
Changes persist on the next render; a failed write preserves the old binding.
Leaving binding capture now restores selection to the edited action and reuses
the previous list scroll position. This applies to successful edits, Clear,
Reset, Escape, and focus-loss cancellation. DLL build validation covers the
change; navigation behavior still requires an in-game check.

The latest Computer Use retry still could not capture Minecraft. The first
snapshot failed with `foreground window did not report a process id`; recovery
by refreshing the window list and rehydrating its returned Minecraft handle
failed because that window was not found. No game input or installation was
performed during this attempt.
Exact duplicates among explicit Lamium overrides are marked Shared. This does
not detect native Minecraft or other-mod conflicts or overlapping subset chords.
Pure capture tests cover arbitrary chords, opener suppression, mouse buttons,
and modified wheel input. Layout, hit targets, input routing, focus loss, and
the full capture/save/dispatch cycle still require Minecraft verification.

Custom input resets on screen/assignment changes, world exit, and app focus loss.
Held inputs are blocked until release after invalidation, preventing key repeats
from reactivating an action. Consumed Zoom wheel events preserve the custom hold,
and key-up is observed even for cancelled events. Pure regression tests cover
these state transitions. Native text focus and the settings scene suppress
custom actions; Sort retains the container/text-input checks. Build and unit
checks do not prove event ordering, live focus handling, mouse codes, binding
display, or interaction with Minecraft mappings; all still need runtime checks.

The current source replaces the owned native dialog's drawing through a scoped
BeforeUIRenderEvent handler and requests world rendering behind that scene.
Other scenes use their original rendering. The panel and backdrop use alpha;
the native dialog continues to supply focus/cursor ownership. Rendering hooks
compile and link, but actual world visibility, input isolation, and restoration
after closing require runtime checks.

A search row filters option IDs, feature IDs, and localized option/feature labels.
Click/Enter focuses it; Backspace edits, Enter/Tab/Down leaves text editing, and
Escape leaves text editing before a subsequent Escape closes the screen.
Native UIScene text events supply UTF-8. Pure tests cover ASCII case folding,
multiple required words, Japanese matching/deletion, rejected controls, and
the byte limit without splitting text events. Native text delivery, IME behavior,
search-result hit testing, and visual layout remain unverified in Minecraft.

The runtime verification attempt could enumerate the running game window, but
screen capture failed twice with `foreground window did not report a process id`.
No game inputs, instance installation, or restart were performed in that attempt.

Setting rows now use a shared catalog with stable IDs, feature ownership, typed
values, and editing accessors. Shulker and Bundle previews can each be disabled,
and each has an empty-container visibility toggle. Missing fields default to
enabled to retain the existing Lamium behavior. Empty visibility applies only
when no items were decoded and no undecodable slots were reported.

Automated storage checks exercise every catalog editor through a disk round
trip and verify that unrelated settings stay unchanged. Layout checks cover
one row, the current catalog, and 100 rows at multiple window heights. This
does not establish usability of the eventual search/feature navigation UI.
Preview switches, scrolling the expanded panel, and empty/nonempty Shulker and
Bundle behavior still need Minecraft verification. Vanilla Shulker contents
text suppression is now an editable option, off by default to preserve previous
Lamium behavior. The hook uses generic item hover text only while both the
master preview switch and Shulker previews are enabled. This leaves the vanilla
Shulker path intact when either switch is off. Automated tests cover its
default, editing, and persistence; actual contents suppression, preservation of
custom names/lore, and immediate restoration still need Minecraft validation.

The current source replaces draft/Save/Cancel with per-edit persistence and
application. Escape/Close only dismisses the screen. Each edit reads current
preferences, and a failed write leaves both runtime state and displayed values
unchanged with an error message. Gameplay hints now have a visibility setting.

Release build, all automated tests (including 3,000 sort planner layouts), and
package checks passed. Storage tests cover defaults for older files, persistence
of hidden hints, and preservation of the previous file on replacement failure.
These checks do not verify the new interaction in Minecraft. Installation,
live NightVision changes, hiding/restoring hints, Escape persistence, and the
in-game failure message still require runtime validation. Historical Save/Cancel
observations below apply to earlier builds, not this interaction.

## Confirmed for the initial camera/settings prototype

- Release DLL compilation and mod packaging completed.
- Camera state tests passed: inactive pass-through, held projection scaling,
  sensitivity, wheel limits, transient reset, invalid configuration, small FOV.
- User-reported runtime validation confirms Zoom hold, wheel adjustment and
  release work correctly. Sensitivity and focus/dimension transitions remain
  separate checks; this report does not establish those behaviors.
- Minecraft loaded Lamium with the older feature mods disabled.
- F8 opened the local settings panel from a creative world.
- Arrow keys changed magnification from 3.0 to 3.5.
- Clicking Save closed the panel and wrote 3.5 to the settings file.
- Reopening the panel showed the saved value.
- Toggling Zoom off and pressing Escape discarded the change; reopening still
  showed Zoom on.
- The panel rendered with the Deesse UI 1.3.9 resource pack enabled.

These observations validate the UI prototype, not the whole feature suite.

Settings input consumes presses and wheel actions but passes key and mouse-button
releases through to vanilla. The mouse path previously consumed releases too;
it now mirrors the existing key-release behavior for buttons held before opening
the panel. This change builds; opening settings during a held mouse action still
needs runtime verification.

Settings now use a viewport that keeps the selected row visible in short windows.
Arrow keys and the wheel navigate all rows, including Save/Cancel. Queued actions
retain their original target row when selection moves before the next render.
Layout tests cover 100–480 GUI-unit heights, row hit testing, navigation wrapping,
footer separation, and tiny-window fallback. In game, resizing the window to
263 pixels high changed the list to six visible rows; wheel/Tab navigation
reached Save, and Enter persisted a changed preview setting. Mouse toggling also
worked at that size. The window was subsequently maximized for ordinary use.
Keyboard selection and pointer hover use separate colors. In a later runtime
check, leaving the pointer over Zoom and pressing Down highlighted Magnification
with the stronger selection color while Zoom retained the weaker hover color.

## Key bindings and hints

- Gameplay hints read Minecraft's current keyboard remapping and native display
  names rather than the registered default key codes.
- Changing the settings binding from F8 to F7 updated the hint immediately after
  returning to the world; F7 successfully opened Lamium settings. F8 was restored
  after the check.
- The original NightVision default N collided with Minecraft's notification
  binding. Editing the settings binding caused Minecraft to clear both N
  assignments. The HUD correctly displayed NightVision as Unbound.
- Restoring notification N and assigning NightVision J resolved the observed
  collision. The HUD displayed J, and pressing J changed NightVision from Off to
  On in the settings panel and saved configuration, then back to Off.
- New registrations now default NightVision to J. Existing saved bindings are
  not rewritten. The changed default builds; a fresh profile's initial mapping
  still needs verification. The runtime J check used a manual remap.
- These checks used Deesse UI 1.3.9.

## Localization

- A shared Japanese/English catalog supplies settings, HUD hints, and Lamium's
  four Minecraft key-binding labels. Other languages fall back to English.
- Native action-label lookup is scoped to the four Lamium translation keys;
  unrelated lookups call the original implementation. The hook is installed and
  removed with the UI lifecycle.
- Catalog tests validate nonempty/unique entries, fallback, locale matching,
  unknown-key pass-through, and format patterns with the UI's argument types.
- In game, all four action names displayed in English. Switching Minecraft to
  Japanese without restarting updated all four names, while vanilla labels and
  saved key assignments remained visible.
- In a local world, the HUD and all ten settings rows rendered in Japanese with
  no observed overlap at the maximized window size. Small Japanese windows,
  save-error text, and other resource packs still need visual checks.

## Lighting and settings persistence

- NightVision toggled on with N in an Overworld night scene, visibly brightened
  the same terrain, then returned to normal lighting when toggled off.
- The View settings panel displayed the NightVision state.
- An additive-settings regression was found during restart verification: the
  initial SDK deserializer rejected a missing lighting section and reset camera
  preferences. Settings now use a backward-compatible decoder.
- Regression tests preserve a 3.5x preference from the original camera-only
  schema, supply defaults for missing sections, reject invalid/future schemas,
  preserve unknown fields, and round-trip through a real settings file.
- A locked-destination test confirms failed replacement preserves the previous
  file and removes the temporary file.
- A full Minecraft restart with the revised decoder preserved the saved 3.5x
  magnification from a file without the new inspection section. F8 showed 3.5x,
  with container previews and durability correctly defaulting to enabled.

## Item inspection prototype

- Shulker/Bundle preview providers and numeric durability display compile in
  the release DLL; layout, bundle fingerprint, and durability-bar tests pass.
- The updated DLL loaded in Minecraft. In the creative inventory, hovering a
  diamond sword displayed `Durability: 1561 / 1561` above the vanilla tooltip.
  Moving to a renamed diamond pickaxe updated the tooltip without a crash.
- A filled Shulker displayed its 9x3 contents grid, counts, and empty slots above
  the vanilla tooltip. Moving to another UI control removed the preview.
- A Bundle updated from empty to 32 bricks, then 32 bricks plus 32 slimeballs,
  then back to 32 bricks after extraction, without closing the inventory.
  Its grid matched the vanilla tooltip's contents and counts at each step.
- Cache keys now include live Bundle entries' metadata and Shulker NBT hashes,
  covering updates that keep item IDs/counts and tag addresses unchanged.
  Metadata-only invalidation tests and the release build pass; that specific
  mutation scenario still needs runtime validation.
- Saving previews Off removed Lamium's Bundle grid while retaining the vanilla
  tooltip. The setting was also confirmed in the saved configuration.
- Saving previews On restored the Bundle grid and the filled Shulker grid in
  the build with metadata hashing. Both rendered after a full game restart.
- Hovering two damaged diamond pickaxes in a large chest displayed `961 / 1561`
  and `161 / 1561` respectively. The text updated when moving between them and
  stayed above the vanilla tooltip without overlap at the maximized window size.
  The background now uses native font measurement instead of a fixed width.
- Numeric durability uses the Japanese/English catalog; both numeric format
  patterns pass tests. The measured tooltip has been checked in English;
  Japanese rendering and larger Bundles still require runtime checks.

## Inventory sorting prototype

- Integrated a pure consolidation/ordering planner, vanilla item classification,
  ordinary container transfers, text-focus tracking, and an R binding.
- Settings independently enable sorting and storage-container targeting.
- Release DLL builds. Planner/key tests cover consolidation, fixed slots,
  region bounds, full inventories, deterministic/idempotent ordering, custom
  names, enchantments, damage, and Shulker content signatures.
- A reproducible 3,000-layout property suite adds an independent operation
  interpreter, checking conservation after every transfer, slot bounds, fixed
  slots, capacities, minimum movable stack counts, and repeated sorting after
  first-appearance group reclassification. It found an equal-key ordering defect:
  fixed slots could change group numbering so a second sort reordered movable
  stacks. Equal-key groups now use first movable appearance, not numeric IDs.
  A four-slot regression and the generated suite pass. These synthetic checks do
  not establish vanilla stackability or server transaction behavior.
- Runtime execution now issues one vanilla transfer at a time, captures its
  new request IDs from the client's pending batch, and waits for matching server
  responses before checking the whole region and issuing the next transfer.
- Response-barrier tests cover multiple IDs, unrelated/old responses, duplicate
  replies, rejection, absent capture, and a five-second timeout. The release DLL
  links against exported container and packet-handler functions.
- Screen exit, loss of UI focus, changed contents, text editing, and disabling
  sorting cancel the remaining plan. Already-issued transfers remain owned by
  vanilla; Lamium does not synthesize a rollback.
- The response-aware build loaded in game. An R press in the creative inventory
  planned five operations around one locked slot, applied the first swap, then
  stopped with an untracked-request result. The update callback did not capture
  that swap's request ID. Capture now compares the client's pending request batch
  immediately before and after each vanilla transfer. It refuses to begin while
  another request scope is active.
- With pending-batch capture, the remaining four swaps completed in a local
  creative world: the log recorded four acknowledged operations. The 12 occupied
  slots retained their displayed counts; the locked five-log stack and hotbar
  remained in place. Repeating R planned zero operations.
- Splitting an unlocked 64-log stack into two stacks of 32 then pressing R
  completed one acknowledged merge and restored 64. Pressing R while the split
  stack was held on the cursor was refused without issuing a transfer.
- Typing R into the creative search field entered a search character and issued
  no sort operation (confirmed from the live log).
- A large chest selected `container_items` with 54 slots and 39 occupied slots.
  Splitting its 64 stone into two stacks of 32 and pressing R completed one
  acknowledged merge back to 64. Swapping the stone and a filled pink Shulker
  manually, then pressing R, completed one acknowledged swap restoring their
  order. The player's 12 occupied inventory slots and hotbar remained unchanged.
  These runtime checks used the localization build, before the lifetime guard below.
- The pending job now stays alive across calls into vanilla transfer code, and
  execution checks that it is still the current job before writing the resulting
  state. This prevents a synchronous screen-exit callback from leaving a dangling
  job reference. The release build and existing tests pass; synchronous cancellation
  during a transfer has not been reproduced in game.
- Cancellation now also clears Lamium's request capture pointer, previous request
  IDs and response barrier. Screen-exit/world-exit paths invoke this before
  returning to vanilla. An exceptional transfer discards capture without reading
  the possibly invalidated request manager, and a synchronously cancelled job
  returns before collecting request IDs. The container manager itself is retained
  across the vanilla call. Build, existing tests and package validation pass;
  the synchronous teardown/exception paths still need runtime reproduction.
- Screen close, focus loss, screen replacement, world exit and feature shutdown
  now log cancellation only while a sort is pending, with its region, operation
  position and response-wait state. Request capture and the pending job are
  cleared before logging. This makes an interrupted runtime run distinguishable
  from one that finished before the screen closed; it does not by itself prove
  the interruption paths. Build, existing tests and package validation pass.
- With the cancellation and equal-key stability fixes (`29ab144`), a local
  survival inventory selected the 27-slot player region. Splitting 64 oak logs
  into 32 + 32 and pressing R completed one acknowledged merge back to 64;
  repeating R issued zero operations. Manually swapping an iron helmet and
  three diamonds, then sorting, completed one acknowledged swap restoring their
  order. The separate item-locked five-log stack, all nine hotbar slots and empty
  equipment/offhand slots remained unchanged. The final inventory again had
  12 occupied main slots. The original creative game mode was restored and the
  world saved normally. This verifies ordinary survival transfers on the latest
  build, not the synchronous teardown or equal-key collision edge cases.
- These checks used Deesse UI 1.3.9. Other storage types,
  other text-input screens, live cancellation, rejected requests, and remote-server
  latency still require runtime checks.
- A separate rotating Lamium log flushes informational messages while the game
  is running; request completion was verified from this log as well as the UI.

## Vanilla UI smoke check

With `e6392ea`, Deesse UI was temporarily deactivated in Global Resources,
leaving only the default Minecraft Texture Pack. The native title menu and
creative inventory appeared without the pack's controls. In a local creative
world, Lamium's gameplay hints and all ten F8 settings rows rendered in English
at 1920 x 1032; clicking Save returned to gameplay. An initial F8 attempt showed
the pause menu, but after Resume Game the same key opened Lamium normally; the
cause of that first transition was not established.

Splitting 64 oak logs into 32 + 32 in the main inventory and pressing R restored
64 with one acknowledged operation in the live log. The locked five-log stack
and hotbar stayed unchanged. Hovering a full diamond pickaxe displayed
`Durability: 1561 / 1561` above its native tooltip. The world was saved normally.
This is limited to settings, a player-inventory merge and one durability tooltip;
previews, other storage screens and live cancellation still need vanilla-UI
coverage. Deesse UI was reactivated after the check.

## Outstanding release gates

Build automation: a Windows CI workflow, dependency lock, and package checker
have been added. The local locked configuration builds, passes the test suite,
and passes the package check. A hosted CI run has not yet been verified.

A separate clone with no project build output also completed configuration,
DLL compilation/packaging, test compilation/execution, and the package check.
Its dependency lock remained unchanged. This reused the machine's downloaded
dependency cache, so it is evidence for a clean checkout build, not a clean
dependency restore.

A subsequent isolated dependency-cache restore exposed an upstream runtime
build failure: the defaulted `MinecraftCommands` destructor uses an incomplete
`CommandRegistry` with MSVC 14.44 headers. Lamium now provides a client SDK recipe
using checksum-pinned official source headers and release exports. In the isolated
environment, this recipe installed successfully and Lamium's DLL and test suite
built and passed. Dependencies were downloaded during this validation, including
upstream precompiled packages where available. The normal development build also
passes all tests and the package/notice check with the new SDK. An untouched clone
of `a6e58dc`, with separate initially empty xmake configuration, package install,
download-cache and temporary directories, also completed dependency restore,
DLL build, all tests and package checks. Its working tree remained clean and its
dependency lock hash matched the source checkout. This used the existing system
compiler/Windows SDK; it was not a fresh operating-system installation.

The development DLL built against the new SDK loaded through LeviLauncher with
Client 26.51.3. A local world displayed the gameplay hints and F8 settings, showed
`1561 / 1561` for a full-durability diamond pickaxe, and logged an already-sorted
27-slot inventory with 12 occupied slots and one locked slot. No inventory
transfer was issued in this smoke test.

`dumpbin /dependents` confirms a normal import of `LeviLamina.dll` and a delayed
import of `bedrock_runtime.dll`. Xmake's earlier LGPL warning came from classifying
the DLL-less SDK import library as static. The package fetch metadata now reports
shared linkage, and configure/build/package validation succeeds without that
warning or disabling license checks. The dependency lock remains unchanged.
A deliberate extra `LeviLamina.dll` in the package is rejected by the package
checker; removing the probe restores a passing result. Additional DLLs and linker
inputs are not allowed in the package. The distribution review below remains
open; these are technical linkage and packaging checks, not a legal conclusion.

- Verify zoom sensitivity and transition behavior; hold, wheel adjustment and
  release have passed user-reported runtime validation.
- Verify menu transitions, focus loss, dimension changes, disconnect/rejoin.
- Verify all settings survive restart and in-game errors preserve existing files.
- Verify localized layout in small windows and gamepad/touch behavior;
  verify new default bindings on a fresh profile.
- Extend vanilla UI coverage beyond the smoke check above and verify additional
  UI resource packs.
- Verify NightVision underwater, in Nether/End, and across restart/dimension changes.
- Complete runtime validation of previews and durability; verify the remaining
  inventory scenarios listed above.
- Verify hosted CI; local clean dependency restore/build/package now passes.
- Dependency notices now include the locked SDK's header libraries, link inputs,
  and LeviLamina's GPL/LGPL texts. Package validation checks referenced notice
  files as well as the hashes of all copied notices. Complete the remaining
  distribution review, including generated runtime import libraries, before a
  public release. SymbolProvider's referenced MinGW disclaimer is now included.

### Info HUD light levels (runtime validation pending)

The optional Light at feet line reports separate stored sky and block light from
client chunk data at the floored player position. It defaults off and participates
in the common settings UI and automatic persistence. It is not a night-adjusted
brightness value or a server spawning prediction. Missing chunks, out-of-height
positions, and values outside 0–15 produce Unavailable rather than zero.

Validation: release DLL links against the current client SDK; settings catalog
round-trip tests, English/Japanese formatting, and light range tests pass.
Minecraft verification remains pending: compare torch placement/removal, open sky
versus roof, day/night, Nether/End, chunk boundaries and world transitions. Confirm
that the SDK pair represents stored sky/block light at the intended feet cell.

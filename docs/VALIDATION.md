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

### Info HUD connection ping (runtime validation pending)

The optional Ping line reads the current transport ping from the client's sole
active remote NetworkConnection. It does not issue server-list probes or packets.
Local connections, absent/closing connections, ambiguous multiple connections,
negative measurements and a busy connection mutex produce Unavailable. The read
uses a nonblocking lock and retains no connection or peer across frames. The
setting defaults off and uses the common settings UI and persistence.

SDK evidence: IClientInstance exposes ClientNetworkSystem; NetworkSystem exposes
mConnectionsMutex and owned NetworkConnection entries; NetworkPeer::NetworkStatus
contains mCurrentPing as chrono::milliseconds. This is transport latency, not
server tick time. Runtime validation must confirm populated statistics and peer
identity on BDS, LAN and NetherNet/Realms, including reconnect/server transfer,
world exit and local hosting. Header layout and a successful link alone do not
prove transport implementations report valid or fresh ping samples; do not treat
this prototype as multiplayer-validated.

### Deployment smoke attempt — 2026-09-23

The d79b037 development build was deployed to the existing Minecraft 1.26.51.01
instance after preserving the previous Lamium installation, including config.
The deployed DLL SHA-256 matched the build output:
`0FB005EFE678359D14D3DAA10A032661EEBB89B98E4A22E24A9F42BBBBCE49FB`.
LeviLauncher started a fresh Minecraft process. The process module inventory
contained Lamium.dll, LeviLamina.dll and LeviSchematic.dll, and reported responding.
This establishes DLL loading only, not successful feature initialization.

Launcher screenshots worked, but Minecraft state capture failed twice with
`foreground window did not report a process id`, including after fresh window
selection and activation. No in-game input was issued. The inspected loader log
still belonged to an earlier run, so its enable messages are not evidence for
this build. Settings, input, HUD and overlay runtime checks remain pending.

### Target information foundation (runtime validation pending)

Target Info is a separate default-off feature with an unbound toggle action and
an optional identifier line. The first provider reads the client's latest block
hit, rejects missing chunks, out-of-height positions and air, and returns owned
name/identifier strings. The minimal HUD uses shared text/layout rendering at the
top center, independently of Info HUD. No server requests or block entity data are
used. Settings and Hotkeys expose the feature through the common catalog.

This is the initial block identity provider, not the completed WAILA subsystem.
Entity identity, block state/direction, progress/redstone providers, configurable
placement and target icons remain future work. Runtime checks must cover target
changes, empty sky, entities occluding blocks, chunk loading, world/dimension exit,
language/resource-pack names, small UI scales and settings input ownership.

### Target entity identity (runtime validation pending)

The target snapshot now also resolves entity hits through HitResult::getEntity.
Null, removed, local-player and other-dimension actors are excluded. It copies the
client actor type ID and filtered name tag; absent names use the native entity
localization lookup, with an identifier fallback. No actor pointer survives the
collection call and no entity metadata is requested from the server.

Release build/link, shared settings/translation tests and package checks pass.
These checks do not execute actor lookup. Runtime verification still needs named
and unnamed mobs, players with text filtering, item/vehicle entities, despawn,
dimension changes and resource-pack language overrides. Confirm native entity
localization-key semantics before considering this provider validated. Block
states and dedicated detail providers remain unfinished.

### Target block coordinates (runtime validation pending)

Target Info also offers a default-off block-coordinate line. Its owned snapshot
copies the validated tile hit's integer position, rather than rounding the player
position or the hit's world-space intersection. Entity hits do not fabricate a
block coordinate. Debug View enables this line in its temporary rendering profile
without overwriting the normal Target Info setting.

The coordinate line follows the name/optional identifier and precedes state
details. Unit tests cover capacities zero through ten, retaining coordinates when
space permits, and budgeting the remaining-state indicator. Shared option tests
cover persistence; translation tests format the new line with integer arguments
in English and Japanese. Native targeting accuracy and in-game text fit remain
pending runtime validation.

### Target block-state provider (runtime validation pending)

An optional, default-off block-state section now reads only the `states` compound
from the targeted block's existing serialization identity. Byte and integer values
remain numeric; string values remain strings. State keys retain their engine names
and ordered-map ordering. No block entity/container data is queried. The snapshot
owns its strings; when disabled, the provider does not enumerate state tags.

The minimal target HUD shows up to six state rows and an additional remaining-count
row, subject to available screen height and shared text clipping. This exposes
client-known direction, open/powered flags and other states without claiming
access to server-only progress or redstone simulation. Dedicated semantic providers
and a full detail view remain future work. Runtime checks: logs/pillars, doors,
stairs, redstone wire, state transitions and resource-pack/custom block states.

### Target HUD positioning and limited-height rows

Target Info now has independent horizontal/vertical percentage settings, defaulting
to top center. Both use the common numeric editor, normalization and persistence.
The target row builder reserves space for the omitted-state count when details
exceed available rows; name and optional identifier retain priority. At extremely
small heights there may be room only for identity or no rows at all. Tests cover
zero capacity, exact fit, six-state limit and limited-height omission counts;
shared layout tests cover screen margins. Runtime UI-scale and placement checks
remain pending. This build has not replaced the running validation DLL.

### Basic debug view (runtime validation pending)

Debug View adds a default-off, initially unbound toggle that displays all existing
client information providers on the left and target identity/states on the right.
It uses a rendering-only profile: normal Info HUD/Target Info selections and
positions are preserved. Columns shrink on narrow screens rather than overlap;
shared clipping and state omission counts still apply. Settings expose the toggle
and key binding through the same feature catalog. No profiler, TPS/MSPT estimates
or server-only information is claimed. Further debug providers and a fuller target
detail view remain unfinished.

Tests cover profile restoration, column separation and common settings/action
catalog behavior. Runtime input, layout, world transitions and provider values
still need Minecraft validation. The new build is not yet deployed.

Follow-up: the mod-specific `logs/lamium.log` contains a successful enable entry
at 04:15:00 on 2026-09-23, matching the fresh process started at 04:14:45 for the
d79b037 deployment. This upgrades that attempt from DLL-load-only evidence to
successful Runtime initialization. It does not verify HUD/input/render behavior,
and does not cover subsequent undeployed builds.

### Settings runtime failure and RTTI repair (2026-09-23)

The normal 6990f95 build was deployed to Minecraft 1.26.51.01 with
LeviLamina Client 26.51.3 and Deesse UI 1.3.9. Runtime initialization and local
creative-world loading succeeded. F8 displayed an empty native Lamium dialog,
not the custom settings list; Escape did not dismiss it. This is a reproduced
failure, superseding build-only evidence for the settings screen.

The client log identified `std::__non_rtti_object` / `Access violation - no RTTI
data!` in Lamium's BeforeUIRenderEvent listener. The renderer used C++
`dynamic_cast` on a Bedrock scene object. It now identifies the owned scene
inside the actual UIScene render call and scopes its ScreenView to that call,
restoring the prior view on both normal return and exceptions. Temporary
diagnostic logging was removed.

The repaired build passed compilation, LamiumTests and the package/license-copy
check. Installed DLL SHA-256:
`8FFC46744F7FAB0011029E5934F1E279BA5F44258306B748E103B52C79DA013D`.
In the same local creative world, F8 now displayed the custom translucent list
over the visible world, and Enter switched from Features to Hotkeys. This does
not establish the rest of the input/settings acceptance criteria: **Escape left
the panel visible**, so closing/scene lifecycle remains a reproduced unresolved
issue. Both failed sessions were ended through the normal window-close action.
Key capture, search, automatic saving, individual new features and full visual
polish remain unverified. Existing instance mods and resource packs were retained.
### Settings entrance/exit lifecycle repair (2026-09-23)

The custom renderer cancels the native dialog rendering, so the owned dialog
must not wait for native visual transitions. Lamium now disables transitions
for both UIScene entrance and exit, only when the scene is its settings owner;
other scenes retain their original arguments. Disabling exit transitions alone
was insufficient in a runtime trial.

The combined repair was tested in Minecraft 1.26.51.01 / LeviLamina Client
26.51.3 with Deesse UI 1.3.9. Build and installed DLL SHA-256 matched:
`A1C4912335570CAA5F6594D3C58A832C63ECB6541C59DA37D5255107EC560C17`.
In a local creative world, two consecutive F8 -> Escape cycles displayed the
translucent custom settings list and returned to gameplay without a lingering
panel. A subsequent Escape opened Minecraft's normal pause screen, confirming
that the settings input owner no longer trapped that input. This supersedes the
unresolved close result above for this build and scenario. It does not verify
focus loss, world exit while editing, binding capture, search, saving, or all
settings/features. These remain separate runtime acceptance work.
### Search text input runtime failure (2026-09-23)

On the ed906fa runtime build above, selecting Search showed its caret, but
entering `hints` produced no text or filtered results. Refocusing and retrying,
then pressing an ordinary `h` key, also left the query empty. Search is therefore
not runtime-validated despite the model-level search tests passing.

An isolated trial moved text handling from UIScene::handleTextChar to
ClientInputCallbacks::handleTextChar, gated by the settings client's top-scene
ownership. It compiled and initialized successfully, but entering `hints` in
the focused search field still produced no text. The trial was reverted; it is
not a fix. Its installed DLL hash was
`F57619CA8A53D8D93D5BF254E44CB4E989D1B59818D4CBDC239DB4C8E0B90E86`.

The next investigation is native text-edit focus: KeyboardManager exposes
tryEnableKeyboard/disableKeyboard and ownership APIs, while the current custom
search/number editor only sets local focus flags. Connecting that lifecycle and
verifying character delivery is still required. Do not replace native UTF-8
input with a hard-coded virtual-key-to-ASCII mapping. HUD visibility toggling
and persistence were not reached in this search-led test.

### Native text focus integration trial (2026-09-23)

A candidate connected the search/number fields to KeyboardManager ownership
and tryEnableKeyboard, releasing on field exit, close, clear, and focus loss.
Build, existing LamiumTests and package checks passed. The candidate DLL
`0D5674D89A7D42B0E855804E39C6282975A72635384D2DEB7121F97FBD950CBB`
was installed and loaded into the same local creative-world scenario. Search
still displayed an empty query after entering `hints`; this is not a verified
fix. The focus integration remains work in progress. Bounded diagnostics are
being prepared to distinguish failed ownership/enable calls from missing text
event delivery, logging state only and no entered text.

### Native text delivery repair and normal-build confirmation (2026-09-23)

Bounded diagnostics confirmed that KeyboardManager ownership and enable both
succeeded, but text callbacks were absent. The settings key listener cancelled
key-down before the native HID path generated text. The repair allows native
key processing while a text field owns the keyboard, retaining local handling
for editing commands. It keeps native character delivery rather than mapping
virtual keys to ASCII. Ownership is released on field exit and screen cleanup.

The diagnostic candidate delivered `hints` to the search field, filtered to the
gameplay key hints option, and allowed that option to be switched off. Closing
the screen removed the gameplay guide. The saved `interface.gameplayHints`
value was independently checked as false.

All temporary diagnostic logging was then removed. The normal DLL SHA-256 is
`E90A5D89C386139455647D7277F77EDC8884BEB4BB2F2999665DBDC1B3AE10FD`.
Existing LamiumTests, package/license checks, and diff whitespace checks passed.
After installing this build and restarting through LeviLauncher, the local
creative world retained the hidden gameplay guide. F8 opened settings; entering
`hints` displayed the query and filtered results with the option still Off;
Backspace changed the query to `hint`; clicking Close returned to gameplay.
The fresh mod log recorded successful enable at 05:43:22.

This verifies basic Latin search entry, deletion, restart persistence for the
guide setting, and closing with text focus. It does not verify IME composition,
numeric editing, every focus-loss transition, or comprehensive gameplay input
isolation. The dense settings visual design and wider input validation remain
unfinished.

### Numeric editor runtime smoke (2026-09-23)

On the same normal build `E90A5D89C386139455647D7277F77EDC8884BEB4BB2F2999665DBDC1B3AE10FD`,
settings was reopened and searched for `zoom`. Clicking Magnification opened
the numeric editor with the current 3.5 selected. Entering `4.5` updated the
displayed value, and a read of settings.json while the editor remained open
confirmed `camera.magnification` was already 4.5, without a save/close action.
Ctrl+A selected the entered value. Replacing it with `0` displayed the allowed
range (1 to 10) and retained the displayed applied value of 4.5. Ctrl+A followed
by `3.5` restored the original setting and cleared the range error. Enter
finished editing; Esc returned to gameplay. A final settings-file read
confirmed the original 3.5 was saved and the wheel step remained 0.5.

This adds evidence for decimal entry, replace-selection, immediate persistence,
range rejection, recovery, and numeric-editor exit in the local creative
scenario. It does not establish IME support, arbitrary keyboard layouts,
numeric-field switching, or complete gameplay-input isolation.

### Search replacement controls (2026-09-23)

Search now accepts Ctrl+A to select the query for replacement, with a visible
selection marker and localized input hint. Typing replaces the selection;
Backspace clears it. Rejected input retains both the existing query and its
selection. Tests cover replacement of a near-limit query with UTF-8 text,
control-character rejection, selection deletion, and clearing replacement state.
The rebuilt LamiumTests passed, and the client build/package checks passed.
These new search-selection controls have not yet been installed or exercised
in Minecraft; the running instance still uses the preceding numeric-tested DLL.

### Search replacement and chord editor runtime smoke (2026-09-23)

The search-selection build was installed through the existing validation
instance (DLL SHA-256
`226AFE4215489E02E7378395EA2FEAF86B498A350FD21B588877C34CF0F60502`).
The fresh mod log recorded enable at 05:52:50. In the local creative world,
F8 opened settings and the new search hint appeared. Entering `hints`, pressing
Ctrl+A, then entering `zoom` replaced the query and displayed the Zoom options.
The selected-query marker was visible before replacement.

From the filtered feature view, clicking the Zoom binding opened capture.
Ctrl+J returned to the edited row and displayed CONTROL + J. Reading the saved
configuration confirmed a two-key chord (key codes 17 and 74). Reopening the
editor and clicking Reset restored the Minecraft mapping C; the saved bindings
object was empty again. Esc returned to gameplay. The temporary binding was
not left installed.

This verifies the feature-to-binding editor path, modifier chord capture,
automatic persistence, Reset, and return selection. It does not verify Zoom
activation using the temporary chord, arbitrary non-modifier chords, mouse or
wheel capture, or the separate Hotkeys view.

### Compact settings layout (2026-09-23)

Reduced row pitch from 22 to 16 GUI units and widened the maximum list width
from 330 to 460. Drawing and hit testing share the row-height constant. The
panel bottom now follows the visible result count while the header/search
position stays anchored. An overflow indicator shows the visible portion of
the list. Layout tests cover short screens, scrolling selection, row gaps,
panel bounds, and stable header placement during filtering. LamiumTests,
client build, package/license checks, and whitespace checks passed.

Installed DLL SHA-256:
`5BE5F6998F607C72C8C0D6B0A893F94A93FD5EC866C5F299A9EEBCAD89C6BABC`.
In the existing local creative scenario at 1920x1080, F8 displayed 14 rows
instead of the preceding 10. Text was readable without overlap, with the
overflow indicator visible at the right. Clicking the search row focused it;
entering `hints` filtered to five rows, kept the search position, and shortened
the panel to the footer rather than covering the lower world view. Clicking
Close returned to gameplay. Other GUI scales and resource packs remain pending.
This is an incremental density improvement, not completion of the planned
feature-centric visual design and broader navigation work.

### Hotkeys activation and settings ownership (2026-09-23)

On the same installed compact-layout build and local creative scenario,
opened Hotkeys and captured Ctrl+K for Toggle debug view. The list showed
CONTROL + K. After Esc, the chord displayed the debug information overlay.
Reopened settings: the same chord did not toggle the overlay, and a W key
press left the displayed XYZ unchanged. After closing settings, Ctrl+K hid
the overlay again. Reset in Hotkeys restored Unbound before leaving settings.
This verifies one edited Toggle action across menu transitions, not Hold
semantics, sustained movement, attack/use isolation, or focus-loss recovery.

One later click on Switch to Hotkeys only highlighted the row; a fresh
snapshot still showed Features, and Enter then switched successfully.
The mouse handler currently uses the hover row from rendering, so stale
hover at click time is a candidate cause to investigate, not a confirmed
diagnosis. Broader click-target validation remains open.

### Event-coordinate click targeting (2026-09-23)

Mouse button handling now hits the last displayed layout with the event's
pixel coordinates converted by the GUI scale used for drawing. It no longer
uses the previous render's hover row to choose an action. Filter changes
invalidate that layout until the new rows are drawn. Unit coverage includes
scrolled rows, row gaps, scales 1 through 4, and invalid coordinates/scales.
Client build, LamiumTests, package/license checks, and diff checks passed.

Installed DLL SHA-256:
`43A0A70800C92C4D2CB85CF4A791021547BF1266CF47875991265276624A94E9`.
In Minecraft 1.26.51.01 / LeviLamina 26.51.3 with DeesseUI 1.3.9 at
1920x1080, opened settings in the local creative scenario. Single clicks
expanded Zoom, switched from the lower rows to Hotkeys at the top, opened
the bottom Zoom binding row, and cancelled capture using its upper control.
All selected the intended target without a second click or Enter. Zoom's
binding remained C; Esc returned to gameplay. Other GUI scales, split-screen
viewports, and resizing during input still need runtime validation.

### Mouse binding and Clear (2026-09-23)

On the event-coordinate build above, captured a middle click for Debug View
through Hotkeys. The row displayed Mouse 3. After closing settings, one middle
click displayed the debug overlay and the next hid it. Reopened Hotkeys and
clicked Clear: the row displayed Unbound. After closing settings, another
middle click did not display the overlay. The saved bindings object contained
`"debugview": []`, confirming explicit unbinding rather than a native fallback.
Debug View was left off and unbound. This covers mouse button 3 and Clear for
one Toggle action; side buttons, mouse/key chords, wheel bindings, and native
pick-block conflict behavior on an in-range target remain unverified.

### Binding editor guidance (2026-09-23; runtime pending)

The capture screen now shows the current binding in its subtitle and an
explicit waiting message before new input is pressed. It no longer labels
an empty pending capture as Unbound. The secondary hint explains the action's
Press, Toggle, or Hold behavior, including mouse/wheel support and the Hold
wheel restriction. English and Japanese strings are included. Client build,
existing translation/settings tests, and package checks passed. The new
wording and fit have not yet been checked in Minecraft; the running instance
still uses the preceding event-coordinate build.

### Binding guidance and wheel Toggle runtime check (2026-09-23)

Installed the e316c31 build, DLL SHA-256
`D3D53260E420DB0CB46D037137A911BD4218108EA4D1CD912EAD8695F01C1569`.
On Minecraft 1.26.51.01 / LeviLamina 26.51.3, DeesseUI 1.3.9,
1920x1080, the Debug View capture screen displayed the current binding,
waiting message, and full Toggle guidance without clipping. Capturing a
downward wheel input returned to Hotkeys with Wheel down displayed.
After closing settings, two separate downward wheel inputs switched Debug
View on and off respectively; the selected hotbar slot remained unchanged.
Reopening capture showed Current binding: Wheel down. Clear returned the
action to Unbound, leaving Debug View off. This verifies an unmodified wheel
direction for one Toggle action, not modified wheel chords, wheel Hold
rejection, opposite direction behavior, or other screen sizes/languages.

### Hold wheel rejection and non-modifier chord (2026-09-23)

On the same e316c31 runtime and display configuration, Zoom capture showed
the complete Hold guidance. A downward wheel input displayed Unsupported
binding for this action, kept Current binding: C, and stayed in capture.
Pressing C next successfully returned to Hotkeys with C; Reset then restored
the native mapping. This verifies recovery from an invalid Hold candidate.

Captured Z+3 for Debug View; the UI displayed the canonical order 3 + Z.
In gameplay, separate Z and 3 presses did not activate Debug View (3 selected
hotbar slot 3 normally). Z+3 displayed the overlay, and a second Z+3 hid it.
Clear restored Debug View to Unbound and it remained off. This covers one
two-key non-modifier chord; longer chords, reverse press order, partial-release
retrigger behavior and modified wheel inputs still need runtime coverage.

### Broad feature grouping (2026-09-23; runtime pending)

Features are ordered in contiguous Camera & appearance, Inventory,
Interaction, Information & overlays, and Interface groups. The selected
feature's group appears in the subtitle, and localized group names participate
in search in both Features and Hotkeys. No new top-level tabs are introduced.
All options and actions remain reachable once, verified by the settings-row
tests; additional checks cover contiguous groups and searching a group while
features are collapsed. Client build, unit tests and package checks passed.
This build is not installed yet; group labels and search still need visual
runtime verification. Separate group header rows are not implemented.

### Feature grouping runtime check (2026-09-23)

Installed 99effd8 with DLL SHA-256
`F6DA4BB9BA481BF47E9C4B5BBEEBAF98F2EBCC72ECBEEF15E6E41E46D2F850B7`.
Minecraft 1.26.51.01 / LeviLamina 26.51.3 / DeesseUI 1.3.9 launched
and entered the local creative scenario. At 1920x1080, Features began with
Zoom, NightVision, Hide Offhand, then inventory features. Expanding Zoom
displayed Camera & appearance in the subtitle and retained C / 3.5x / 0.5.
Searching appearance showed Zoom, NightVision and Hide Offhand with their
settings and bindings expanded. Switching to Hotkeys retained the query and
showed exactly their three actions, with the panel shrinking to fit. No
preference values were changed. Japanese group search, other GUI scales and
the remaining group subtitles are not covered by this runtime check.

### Settings keyboard navigation (2026-09-23; runtime pending)

Added Ctrl+F to focus search and select the current query, including from
scrolled results or numeric editing. Outside text editing, Page Up/Down move
by the visible row count minus one (clamped at the ends), Home/End select
the first/last row, and Shift+Tab moves backward. Binding capture retains
priority over these shortcuts. The navigation hint advertises page movement
and search in English and Japanese. Client build, existing unit tests,
package checks and diff checks passed. These do not verify native key-event
routing; runtime shortcut behavior and hint fit remain pending. The running
instance still uses the preceding grouped-feature build.

### Settings keyboard navigation runtime check (2026-09-23)

Installed 4fef3e2 with DLL SHA-256
`C3975682629D957C488AD5323D0EA9D2434A3437870D83E80EECCF4E697BA3ED`.
On Minecraft 1.26.51.01 / LeviLamina 26.51.3 / DeesseUI 1.3.9,
1920x1080, the complete navigation hint fit inside the settings panel.
End selected Close at the bottom of the collapsed Features list. Ctrl+F
brought search into view and accepted zoom, filtering the list correctly.
A second Ctrl+F selected the entire query; Backspace cleared it in one press.
Escape left search editing without closing the panel. Page Down moved from
search to Hitboxes (13 rows), and Page Up returned to search. Home selected
the first row; Shift+Tab wrapped backward to Close. No preference values
were changed. Ctrl+F during numeric editing or binding capture, Japanese
labels and other GUI scales remain outside this runtime check.

### Inline section captions (2026-09-23; runtime pending)

Features and Hotkeys now display a section caption on the first row of each
broad group, plus a thin separator between groups. The first visible feature
row repeats its section when scrolling starts inside a group. Captions use a
reserved right column on panels at least 360 GUI units wide; narrower panels
keep the full setting-label width and the existing selected-row subtitle.
No rows or navigation stops are added, and capture has no section captions.
Client build and package checks passed. Caption fit in both languages,
truncation of long binding summaries and scrolled presentation need runtime
verification. This build has not yet been installed in the test instance.

### Inline section caption runtime check (2026-09-23)

Installed c1187d7 with DLL SHA-256
`AD190AA077C25944D19106EBAF1B953C4438C4CED4794D2FA846BD6254FE9541`.
At 1920x1080 in English on Minecraft 1.26.51.01 / LeviLamina 26.51.3 /
DeesseUI 1.3.9, all five section captions fit in the collapsed Features list.
The long Block Restrictions binding summary was ellipsized before the caption,
with no overlap. End scrolled to Close and repeated Camera & appearance on
the now-first-visible Hide Offhand row. Searching appearance showed the
expanded matching features with one section caption; switching to Hotkeys
retained the query, showed its three actions, and fitted the caption beside
Zoom. No preference values were changed. Japanese captions, narrower panels,
other GUI scales and long custom chord summaries remain unverified.

### Contextual option guidance (2026-09-23; runtime pending)

Numeric rows now show their accepted range and entry/adjustment controls in
the description area; active numeric editing keeps the range visible.
Container-preview options have individual English/Japanese descriptions for
the master switch, per-container switches, empty previews and vanilla Shulker
text suppression. Options without dedicated help retain the feature description.
The descriptions match the preview enable guards in Inspection.cpp. Client
build, unit tests (including translated numeric format strings), package and
diff checks passed. The current test instance still runs c1187d7; text fit and
selection-dependent guidance need runtime verification on this new build.

### Mixed input lifecycle sequences (2026-09-23)

Added event-sequence coverage for a three-member keyboard/middle-mouse chord:
all six press orders, each possible released member, partial-release rearming,
focus invalidation, partial recovery while other members remain stale, and
full release/repress recovery. Added modified-wheel sequences across focus
loss, verifying that a stale modifier cannot activate a wheel binding and a
fresh press restores it. All unit tests passed without production changes.
These validate HeldInputs and BindingState; native event routing, physical
press order and focus callbacks still require separate runtime coverage.

### First Shape Manager runtime check (2026-09-23)

Installed 03d4ff6, DLL SHA-256
`461FD1C4A923AFC7E0227C38C52725E302717C84E6679B8DAE206C904DDEA1CD`.
In the local creative scenario on Minecraft 1.26.51.01 / LeviLamina 26.51.3 /
DeesseUI 1.3.9, 1920x1080 English, searching shape revealed the dedicated
manager entry. Opening it showed all creation controls and the session-only
notice. Adding a sphere opened its editor with radius 4 and Block Center snap;
cyan block-grid lines appeared in the world behind the translucent panel.
Clicking radius changed it to 4.5 and visibly rebuilt the outline. Switching
Visible off removed the lines; Escape returned to the manager with one Sphere,
Off, Dimension 0 entry. This proves the first sphere UI-to-render path only:
other shape types, exact projection/depth correctness, camera movement,
world-exit clearing, dimension transitions, performance and other locales/scales
remain unverified. The session currently contains that one hidden sphere.

The check exposed excessive coordinate decimal digits. Source now formats
coordinates to three decimal places without rounding stored values, and shows
session IDs in list/editor titles to distinguish same-named shapes. These
presentation fixes are not installed yet.

### Shape direct numeric input (2026-09-23; runtime pending)

The dedicated editor now shares native text input and NumberInput with Settings.
Enter/click on a numeric row selects its value for replacement; valid changes
apply immediately to the session. Ctrl+A, Backspace and Enter/Escape use the
same editing lifecycle. Shape coordinates/radius parse as double; plane block
origins and grid dimensions reject fractional input. The footer shows range
requirements and does not claim disk saving. Unit tests verify sub-block values
beyond the exact float integer range, negative integer positions and bounds;
client build and existing tests passed. Native shape text entry, integer error
recovery and drawing updates during typing remain runtime-pending. The running
instance still uses 03d4ff6.

### Shape names (2026-09-23; runtime pending)

Shape Editor now has a Name row using native UTF-8 text input with select-all,
replacement, Backspace and Enter/Escape completion. Valid name changes apply
immediately to the session; empty, ASCII-space-only, control-character and
over-128-byte names are rejected without replacing the last valid name.
Renaming updates metadata without regenerating grid lines. Unit tests verify
Japanese names, cache identity and failed-rename preservation. Client build and
the full unit suite passed; native name editing, IME and text fit are unverified.
Shapes still do not persist across world exit.

### Shape workspace persistence boundary (2026-09-23; game integration pending)

The full unit suite passed with real temporary-file tests for a new workspace,
automatic persistence of a candidate change, replacement blocked by a Windows
file handle, live/file rollback, subsequent successful visibility save, separate
world files, restoration with fresh session IDs, corrupt-file load rejection,
prevention of edits overwriting an unreadable file, deletion of the last shape,
and clearing the destination on departure. The test directories are exclusively
created under the resolved system temporary directory and cleaned up afterward.

`ShapeWorkspace` is not wired into `WorldOverlay` yet. These tests establish the
storage transaction boundary, not Minecraft world identity or lifecycle behavior.
The installed game build and its session-only Shape behavior are unchanged.

### Local world identity probe (2026-09-23)

A diagnostic client build (`xmake f --shape_trace=y`) was installed and launched
through the existing launcher. DLL SHA-256:
`9302E60303A2E1FF44E9B99F9ECD7E0887DBF499F19BDC9A8112274D0F364FDC`.
`ClientStartJoinLevelEvent::isJoiningLocalServer()` reported true and
`GameConnectionInfo::mType` reported Local. At `ClientJoinLevelEvent`, the
primary player's `Level::getLevelId()` matched the selected local world's
storage directory name. Saving, leaving and reentering the same world produced
the same ID. No shape file is loaded or written by this probe.

The optional trace is disabled by default, capped at 32 primary-player joins per
enable, and hex-encodes at most 128 ID bytes to avoid log control characters.
World IDs and personal storage paths are deliberately omitted from this record.
Diagnostic build and package/license checks passed. Other local worlds,
profile/storage-root separation, remote sessions and actual shape restoration
remain unverified; this observation does not prove global uniqueness.

### Local shape persistence wiring (2026-09-23; runtime pending)

World join/exit now binds and clears a `ShapeWorkspace`. Local joins resolve an
existing world below the game's current `FilePathManager::mWorlds`; saves use a
`lamium/shapes.json` sidecar inside that world. Remote/unresolved joins remain
explicitly session-only. UI descriptions reflect storage state and distinguish
write failures from invalid names/geometry. A load failure blocks creation until
reentry and cannot silently replace the unreadable file.

The full unit suite passed, including separate roots containing the same Level
ID, traversal/separator/relative-root rejection, missing-world rejection and
the previous transaction/reentry tests. The client build passed with
`shape_trace` enabled. This new binary has not yet been installed: real SDK
storage-root resolution, successful save, restoration and UI save-error recovery
remain pending. The running instance still uses the earlier identity probe.

### Shape name input persistence scheduling (2026-09-23)

Native name text and Backspace events now mark the edit dirty instead of writing
the entire shape workspace inside each input callback. The settings render pass
coalesces pending characters into one rename/save; finishing an edit (including
Enter, Escape, navigation and focus-loss cancellation) also applies a pending
name before clearing its target. Validation and storage failures still preserve
the last committed shape and use the existing visible error messages.

Client build, the full existing unit suite and package/license checks passed.
These checks do not exercise native keyboard timing. This is an input-path
latency improvement, not proof that the observed extra trailing character in
automated native name entry is fixed. Repeated native text entry and IME testing
remain required. This change has not yet been installed in Minecraft.

### Local shape persistence smoke (2026-09-23)

The 7536e98 diagnostic build was installed (DLL SHA-256
`D64328C864293EE225814DD82CD3DBCF8100F9A32F6A49E792185ECFF9E2BE3F`).
In a local creative world, Shape Manager reported automatic local-world saving.
A sphere was created, named and resized to radius 4.5 through native numeric
input. Its sidecar contained the changed definition before closing settings.
After Save & Quit and reentering that world, the guide appeared again. The
Manager and Editor then showed the saved name, radius 4.5, original coordinates,
visibility On and Block Center snap, with a fresh session shape ID.

The automated name entry produced an extra trailing character, which was also
persisted/restored; successful persistence is not evidence of correct native
name input. Multiple-world isolation, dimension transitions, unreadable-file UI
and write-failure recovery still require runtime validation.

The ecb784e input-scheduling build was subsequently installed after normal
Minecraft shutdown, preserving configuration, and launched with LeviLauncher.
Installed DLL SHA-256:
`A8E847D51840E7EC2A2A92E7BA49702926451CD02F383E0073366C37A8956138`.
It retains the bounded `shape_trace` diagnostics. After this process restart,
the local sphere was again restored with radius 4.5 and the same stored fields.
Native automated name input still failed: replacing the selected name with
`Saved sphere` produced `Saved sphere spheree`; Ctrl+A followed by `abc` produced
`abc spheree`. Search input `shape` was correct in the same run. This points to
stale native text/selection synchronization as another hypothesis to investigate;
the callback/save scheduling change alone does not resolve the defect. The
test shape currently retains the latter name. IME remains untested.

### Native text buffer initialization fix (2026-09-23)

The native keyboard now starts with an empty insertion buffer; Lamium continues
to own the displayed text and selection. Finishing an edit releases keyboard
ownership so another field starts a fresh native session. Passing the existing
Lamium text into the independent native buffer had reproduced stale suffixes
when replacing names through Lamium's select-all handling.

Client build, existing unit tests and package/license checks passed. The new
DLL was installed after normal shutdown and launched through LeviLauncher:
`79F54C3E9D963B0738F7DBF07DF355B5C0A7A9E3E474187E1DAE642C5065DB47`.
In the same local test world, search `shape` worked. Selecting the existing
`abc spheree` name and typing `Saved sphere` produced exactly `Saved sphere`;
Ctrl+A and typing `abc` in that same edit session produced exactly `abc`.
Both the input row and committed editor title agreed, without the former stale
suffix. The test shape is now named `abc`. This verifies the reproduced ASCII
replacement cases, not IME composition, long/repeated input, numeric-field
regressions or all focus transitions; those remain outstanding.

### Unicode name and numeric input follow-up (2026-09-23)

On the same ff25950 binary, native automated insertion of `建築の球` replaced
the selected ASCII name exactly. Backspace removed only the final `球`, leaving
`建築の`; inserting `球` restored the name without corruption or duplication.
This exercises committed Unicode text, not IME preedit/candidate selection.

Clicking Radius directly while name editing switched the text owner correctly.
Replacing radius 4.5 with 3.25 updated the value and visible grid geometry.
Ctrl+A then `-1` displayed the range error while retaining committed radius 3.25
and its guide. Replacing the invalid text with 4.5 cleared the error and restored
the larger guide. A sidecar read while settings remained open confirmed the
exact Japanese name, radius 4.5 and visibility true.

The invalid numeric state currently repeats the range message in both footer
lines; this is a presentation issue, not a failed rejection. Long input, IME
composition, integer-only field errors and storage-failure recovery are still
unverified in Minecraft. The test sphere retains the Japanese name above.

### Shape save failure and retry (2026-09-23)

On ff25950, the existing local-world sidecar was opened with a read-only Windows
handle sharing reads but denying replacement for 60 seconds. While that handle
was confirmed live, changing radius 4.5 to 3.25 displayed the dedicated save
failure message. The input retained the attempted 3.25, while the committed row
value and visible guide retained 4.5. A file read also retained 4.5 and the
Japanese name, and no `shapes.json.*.tmp` files remained. The lock holder verified
that the complete sidecar SHA-256 was unchanged before releasing its handle.

After confirmed release, selecting and retyping 3.25 in the same editor cleared
the error, changed the guide, and persisted radius 3.25 in the sidecar. No world
reload or process restart was required. This verifies replacement-denied write
failure and explicit edit retry for a numeric field; it does not cover every
filesystem failure or failed-load recovery. The test sphere now has radius 3.25.

### Experimental Freelook startup and settings smoke (2026-09-23)

Build `0e22e25`, including the detached-look interaction hooks, was installed
with matching source/destination DLL SHA-256:
`117FD3DC75282810D5518B8954F3863A602D07A230EE33F2484E7FE3127B2CB6`.
Camera trace and fixed-angle probe were disabled. The existing launcher instance
opened a local creative world in rear third-person view; world and Shape overlay
rendering remained visible. F8 displayed the experimental Freelook row, initially
Off and Unbound. Enabling it and assigning Mouse 1 worked in the settings UI.

An automated mouse drag returned to the same visible view without an observed
crash. Only the post-release frame was captured: this is not evidence that the
hold activated, native turn input reached Freelook, or camera rotation occurred.
Body isolation, interaction suppression, input scale/sign, and lifecycle recovery
remain unverified at runtime. Do not classify this smoke as working Freelook.

The toggle was restored to Off and Reset to Minecraft mapping restored Unbound;
both were verified together in the settings screen. Minecraft was closed normally
and its window disappeared. The flushed session log confirms Lamium enabled and
later reached mod disabling, with no ERR entry in that session. This establishes
startup and settings integration only, not the detached-camera validation gate.

### Freelook active-path trace (2026-09-23)

Diagnostic build `caf0d9b` was installed with matching DLL SHA-256
`024052C6AF8AC9644A4E432417418FC0D48B6898C0FAE5A82347636E58407A30`.
Camera trace was enabled and the fixed-angle probe disabled. In the same local
creative world and rear third-person view, Freelook was enabled and temporarily
bound to Mouse 1. An automated left-button drag was issued in gameplay.

After normal shutdown, the flushed log contained one successful session begin,
one native turn sample with pitch/yaw both zero, and two render application
samples with relative pitch/yaw both zero. Thus the binding reaches session
activation and the render override; this attempt did not supply nonzero turn
input while active. The unchanged post-release screenshot cannot validate
rotation, sensitivity or body isolation. Do not adjust the rotation matrix or
input scale based on this zero-input experiment. CustomInput's mouse listener
explicitly passes both absolute and relative move events through; the next check
must distinguish mouse capture/input delivery from detached-view math.

Freelook was restored to Off and its binding reset to Unbound, verified together
in the settings screen before exit. Minecraft's window disappeared normally and
the session log reached Lamium disabling. The installed DLL remains diagnostic;
restore a non-trace build before ordinary use.

### Light overlay display and settings smoke (2026-09-23)

Normal build `a4213a0` replaced the preceding diagnostic DLL. Camera trace,
fixed-angle camera probe, and Shape trace were disabled. Source/destination DLL
SHA-256 matched:
`016258EC81297F2D4458053012FE2E02C8520942830B52FB84C84D5E07DC89D7`.

In a local creative world in rear third-person view, F8 search found Light Level
Overlay, initially Off and Unbound. Enabling it drew white floor digits behind
the translucent settings panel. Selecting stored sky light changed visible
digits from 0 to 15; they remained visible in gameplay after closing settings.
Reopening settings and restoring sky light to Off returned block-light display.
Disabling the overlay removed its floor digits while Shape rendering remained.
Both options were verified Off and the binding Unbound before normal shutdown.

Minecraft's window disappeared, and the flushed session log records Lamium
enabled at 09:23:00.778 and disabling at 09:29:59.590, with no ERR entry in that
session. The installed DLL is now the normal build above, superseding the
diagnostic-install state in the preceding historical entry.

This is display/settings smoke evidence, not independent verification of native
light values or all eligible surfaces. Light-source changes, dimension changes,
depth/readability across perspectives, and frame cost remain unverified.

### Target-coordinate and Debug View smoke (2026-09-23)

Normal build `037d7ea` was installed with matching source/destination DLL SHA-256:
`329ABFF1A80BEFC64B6EAA0A9A27360472F306AFB0C61CD65308225D0AF4A01E`.
In a local creative world with Deesse UI, Target Info and its new block-coordinate
option were enabled through F8 search. A local relative teleport command changed
the view to look straight down without requesting a position change. After
switching perspective, Target Info displayed Sulfur, `minecraft:sulfur`, and
integer block coordinates. The vanilla `testforblock` command succeeded for that
displayed position and type. First-person rendering showed all three rows.

Turning the coordinate option Off immediately removed only that line. Target
Info was then restored to Off. Enabling Debug View displayed the same block
coordinates in the right column despite the normal options being Off, alongside
player information in the left column. Disabling Debug View removed both columns.
No bindings were assigned. The original rear third-person perspective was
restored; the view remains pointed downward. Minecraft closed normally; the log
records Lamium enabled at 09:37:04.921 and disabling at 09:43:19.398.

This verifies one positive-coordinate tile target and the settings/profile
transitions, not negative-coordinate targeting, entity transitions, multiplayer,
all GUI scales, or every information provider's accuracy.

The smoke also exposed an existing player-position discrepancy: the HUD Y value
was approximately 1.62 above the vanilla teleport result. PlayerInfo currently
uses `getPosition()` for XYZ and the cell labeled "Light at feet". Its coordinate
reference must be investigated and corrected or explicitly labeled before
claiming feet-based sampling. This finding does not invalidate the independently
checked target-block position, which comes from the tile hit.

### Feet-position correction (2026-09-23)

PlayerInfo now uses the SDK's `getFeetPos()` for displayed XYZ and biome/light
cell sampling, instead of the actor state-vector position. The Light Level
Overlay scan center uses the same feet reference. No fixed eye-height subtraction
is used, since the SDK owns the pose-dependent offset.

The native build and package/license check passed. The normal DLL was installed
with matching source/destination SHA-256:
`665C19C1266BBEA9E56DE3D02533E59A533638EA59177FC164CF02E6D00DFE9A`.
In the same local creative-world standing scene, Debug View's Y changed from
72.6 to 71.0, matching the preceding vanilla teleport result at that location.
Stored sky/block light remained displayed as 15/0. This is a standing-coordinate
baseline, not independent verification of the light samples. Crouching, swimming,
riding and other poses remain untested, and the overlay scan-center change was
not visually rechecked with Light Level Overlay enabled.

Debug View was restored to Off with its binding Unbound; both information columns
disappeared immediately. Minecraft closed normally and its window disappeared.
The flushed log records Lamium enabled at 09:46:25.057 and disabling at
09:52:13.481. This supersedes the unresolved player-height finding above for the
tested standing case only.

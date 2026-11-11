# Chat position (L-174)

## Scale (L-321, 2026-11-11)

HUD layout also scales the chat, 50-200 %, like Lamium's own elements. The
game's layout stays as it is: for each draw the part's place inside the
panel, the control's size and the text's font scale and line height are
multiplied and restored, which keeps the line breaks and spacing (the
`chat_scale_trace` experiment, VALIDATION-LOG). The text shadow's offset
grows with the scale. A chat scaled past the screen is left as the game
draws it. Below, "only offers position controls" and "no ... appearance
settings" predate this.

## Current contract (decided 2026-10-14)

Information has a saved, default-off **Chat position** switch and an unbound
toggle key. Enabling it places ordinary HUD chat at the bottom left, with
its left edge flush against the screen and its bottom 44 GUI units above
the screen edge (chosen 2026-10-14). Two standard Durability HUD
rows occupy 36 units with their 4-unit bottom inset, leaving a 4-unit gap.
This is a fixed default; it does not track later Durability HUD changes.
Existing saved positions remain unchanged; reset uses the new default.
HUD layout provides dragging, keyboard nudging, nine snap positions and
reset; the chat toolbar only offers position controls. Placement follows
the shared edge-anchor rules as the measured message area grows/shrinks.

The game's message contents, font, size, shadow, background, order and fade
are retained. The input/history screen is outside this step. No width,
line-count or appearance settings. The layout editor uses sample text, not
saved chat contents. The initial scope is verified and no longer carries
Experimental. BACKLOG-DONE owns completion; VALIDATION owns coverage.

## Chat-return regression (2026-10-15, L-203)

The local smoke on `d5978e2` found a brief vanilla-position draw after
returning from chat input before the chosen HUD position returns. Basic
placement still works; this transition is reopened, with server coverage
still pending. The render scope already identifies the actual HUD visual
tree. It now uses that identity without also consulting the global screen
stack name, which may still identify the closing chat/menu while the HUD
is drawing again. Input/history visual trees remain untouched. Existing
foreground/player, capability and hierarchy checks and per-draw restoration
remain in place. The maintainer passed the supplied chat-return checklist
locally on `f8c714b`: no vanilla-position jump reported, with ordinary OFF
restoration retained. This closes L-203 locally; server and broader layout/
pack coverage remain open. Evidence and test scope: VALIDATION-LOG.

## Current implementation and validation (2026-10-14)

The maintainer passed the initial three-step checklist on `5b5750c`, then
reported a brief flicker of remaining messages when an older one expires.
This establishes basic placement, editing/reset and OFF restoration, not
the broader checks below. Evidence: VALIDATION-LOG.

The follow-up replaces the subtree-wide pre-render translation with a
render scope containing only owned screen/layout values. Native text and
background render hooks find the panel through a bounded ancestor walk,
read its current measured size after layout, translate only the component's
cached position for its draw and restore it immediately. Parent-relative
positions and dirty flags are left alone; no game pointers cross frames.
Renamed native text/background controls fail open. The maintainer passed
the expiry/long-message checklist on `b28a40d`: no flicker and long messages
remain correct. This confirms the fix in the tested context; the broader
checks below remain unitemized. The saved placement was left 0, bottom
offset -45; the maintainer then chose a 4-unit upper gap for the default
(left 0, bottom offset -44). Existing saved placement is retained.
The default-reset check passed on `dc3ac45`; the maintainer accepted closing
L-174. Fresh settings and the broader checks remain unitemized.

## Research and first implementation (2026-10-14, superseded by the fix above)

- SDK 26.51.5 exposes `UIControl::mParentRelativePosition`,
  `mCachedPosition`, `mCachedPositionDirty`, `mSize`, `mChildren` and
  `_setCachedPosition()`, and `ScreenView::render(UIRenderContext&)`.
- Mojang's [HUD UI sample](https://github.com/Mojang/bedrock-samples/blob/main/resource_pack/ui/hud_screen.json)
  separates `chat_panel` from `player_position`, the paper-doll padding
  and other children of `chat_stack`. The panel contains a message factory;
  each message has its own text/background and native fade animation.
  This supports a translation-only experiment; it does not establish
  the runtime render/cache order on the installed game.
- `ChatPosition.cpp` gathers at most 512 controls in that panel's subtree,
  resolves dirty cached positions, and validates measured geometry before
  any translation. The relative root and absolute cached positions are
  translated for one render call. A scope guard restores them and marks
  them dirty so vanilla can re-resolve them afterwards. No game pointers
  survive the call; only owned screen/size values inform the editor sample.
- Only the gameplay HUD view is eligible while a player exists and Minecraft
  has foreground focus. Disabled features, menus, world exit and unknown
  game executables retain vanilla placement. Unexpected pack hierarchies,
  clipping and invalid/oversized geometry fail open. The version gate is
  `Chat position (HUD control positions)`.
- Build/tests do not prove that native batching consumes the translated
  caches. First checkpoint: see both text and background move together;
  confirm fading, coordinates remaining in place and disabling restoration.

Build checkpoint: release DLL, LamiumTests and LamiumNativeTests pass; the hook inventory
matches. No runtime result is claimed at this checkpoint.

## Runtime checks (basic scope and expiry regression passed; broader checks pending)

First repeat: send three short messages with time between them and watch
the oldest fade/expire. The other messages and backgrounds should stay
steady. Repeat with a wrapped message, then with Chat position OFF. Passed
on `b28a40d`; the final default after reset passed on `dc3ac45`.

1. Enable Chat position; send several messages and a wrapped long message.
   Text and backgrounds should appear together at the lower left and fade
   as vanilla does. Coordinates/paper doll must stay where vanilla put them.
2. In HUD layout, drag the chat sample, use arrows and snap positions; close,
   send messages and restart. Placement should persist. Reset restores the
   lower-left default. No font/background/scale controls for chat.
3. Disable; send another message. Open chat input/history and close it, use
   F1, alt-tab, leave/rejoin and change dimensions. The input/history screen
   stays vanilla and no chat offset is left on another screen.
4. Resize/change GUI scale; repeat with Pocket UI and a resource pack. On
   unsupported panel layouts retain vanilla. Verify local world and server
   chat separately. Check the forced `unverified_game` gate separately.

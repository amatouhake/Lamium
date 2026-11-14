# Chat position

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/CHAT-POSITION.md`.

HUD layout places and scales the game's own HUD chat messages. The chat is
one part of **Game HUD placement** (default off), beside the paper doll, the
scoreboard and the boss bars ([HUD.md](HUD.md), "Game HUD placement" and
"Showing, hiding and names"). Its row there is Standard / Placed /
Hidden, default Placed, with one unbound key that steps the three. Older
settings with Chat position on load with Game HUD placement on and only the
chat selected.

## Behavior

- Placed: the default is the lower left, its left edge flush with the screen
  and its bottom 44 GUI units above the screen edge (chosen 2026-10-14). Two
  default Durability HUD rows take 36 units with their 4-unit inset, leaving
  a 4-unit gap. It is a fixed default, not tracking later Durability HUD
  changes or avoiding anything. Saved placements are kept; reset uses this
  default.
- HUD layout drags, nudges, snaps and resets the chat like any element, and
  scales it 50-200 %. It has no look controls (background, shadow). The
  shared edge-anchor rules apply as the measured message area grows and
  shrinks. The editor shows sample text, not saved chat contents.
- Scaling keeps the game's layout: the part's place inside the panel, the
  control's size and the text's font scale and line height are multiplied
  for each draw and restored, so line breaks and spacing stay. The text
  shadow's offset grows with the scale. A chat scaled past the screen is
  left as the game draws it.
- Hidden: the HUD messages are not drawn; the game's layout and state stay.
- The game's message contents, font, shadow, background, order and fading
  stay vanilla. The chat input/history screen is never moved. No width,
  line-count or appearance settings.
- Only the gameplay HUD is affected, while a player exists and Minecraft has
  foreground focus. Switched off, in menus, on world exit and on an
  unverified game executable the chat stays where the game puts it.

## Code

- `src/features/information/ChatPosition.cpp/.h`. Pure: `chatPosition::placed`
  and `translated` (translation plus a uniform scale; shared with the
  scoreboard). `preview` gives the editor sample from owned geometry only.
- Hooks: `ScreenView::render` opens a render scope only for a visual tree
  whose root control name ends in `.hud_screen` (that identity, not the
  global screen stack name, which may still name a closing chat or menu).
  `TextComponent` and `SpriteComponent` render hooks handle the `chat_text`
  and `chat_background` controls: a bounded ancestor walk (16 levels) finds
  `chat_panel` under `chat_stack`, reads the panel's measured size after
  layout, translates (and scales) only the component's cached position and
  size for its draw, and restores them right after. Parent-relative positions
  and dirty flags are left alone. No game pointers cross frames.
- Capability `ChatPosition` ("Chat position (HUD control positions)").
- Default placement: `HudElementId::Chat` in `src/ui/HudElement.h`.

## Known

- `UIControl` exposes `mParentRelativePosition`, `mCachedPosition`,
  `mCachedPositionDirty`, `mSize`, `mChildren`, `mClipsChildren` and
  `_setCachedPosition()`.
- The vanilla HUD (`hud_screen.json` in Mojang's bedrock-samples) keeps
  `chat_panel` apart from `player_position`, the paper-doll padding and the
  other children of `chat_stack`. The panel holds a message factory; each
  message has its own text, background and fade animation.
- The game has no fixed chat place: the chat is the last item of a stack
  under the paper doll's room, the position line, the day count and the game
  tips, so its height changes with what shows (why the default is a fixed
  lower left, HUD.md).

## Tried

- Translating the whole panel subtree before render (relative root and
  cached positions, restored and marked dirty afterwards): remaining
  messages flickered when an older one expired.
- Gating on the global screen stack name: the chat was drawn at the vanilla
  place for a moment after closing chat input.

## Limits

- A clipping panel on the way, a renamed text/background control, an
  unexpected pack hierarchy or invalid/oversized geometry leaves the chat
  where the game draws it (hiding still works).
- Coverage: [VALIDATION.md "Information and overlays"](VALIDATION.md#information-and-overlays)
  (Chat position, Chat and scoreboard scale). Not yet checked: fresh
  settings, GUI scales, Pocket UI, resource packs, F1, alt-tab,
  leave/rejoin, dimension change, servers, and the forced `unverified_game`
  gate.

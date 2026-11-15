# In-world language selection and server resource packs

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/LANGUAGE.md`.

Two General settings rows for things vanilla only offers outside a world:

- **Game language** (`gameLanguage`, opens a page, no key): change the game's
  display language while staying in the same world or server. The selected
  language is Minecraft's own setting, not a second Lamium setting.
- **Skip server resource packs** (`visuals.skipServerPacks`, off by default,
  toggle key `skipserverpacks`, unbound): join remote servers
  without applying their resource packs.

Task status and open choices:
[L-167 Change the language and resource packs from inside a world](backlog/L-167.md).

## Behavior

### Game language

- Chosen by the maintainer 2026-10-13. The page (`ui/LanguageView.cpp`)
  lists every language I18n reports as supported, with their own names; the
  current one is highlighted and also shown on the settings row. Click or
  Enter applies; Esc, Backspace or "< General" returns to the settings list.
- Inside a world, one change per 5 s; a sooner choice shows how many
  seconds remain. Outside a world (settings opened from the title screen,
  L-175) there is no interval.
- Apply through vanilla's selection and settings APIs, with their observers
  and persistence; never write an option field or keep a controller pointer.
  Success is read back from I18n and the option, not from a return value.
- `ui/GameLanguage.cpp` queues the apply to the client thread and checks
  again there: same language as when asked, same dimension, the settings
  screen still open, the capability `Game language (native apply)` allowed,
  the code supported, and no schematic save running (resource changes
  invalidate its live readers). Then, in the order vanilla OreUI uses:
  `I18n::chooseLanguage` (it runs the game, font and pack-language observers
  itself), confirm the current language changed, then
  `IOptionRegistry::setLanguage` and `saveIfNeeded`.
- Before applying, English Search's names, schematic item names and
  schematic ghost resources are dropped so they rebuild in the new language.
  Lamium's ordinary UI reads the current locale when drawing.

### Skip server resource packs

- Decided by the maintainer 2026-10-14. `features/visuals/ServerPacks.cpp`
  records from `onStartJoinGame` whether the join is a local world; on a
  remote join it empties the resource packs of the server's
  `ResourcePackStackPacket` before the client handles it. The
  download/acceptance handshake is never changed; only local application is.
  Local worlds and joins the game did not announce keep their packs.
- The look is vanilla, as on any required-pack server: the player's global
  packs are not applied either and Global Resources shows a gray page.
- When packs were left out, the HUD shows a message toast with their count
  once the world draws. Turning the switch on or off applies from the next
  join. Capability: `Skip server resource packs (pack stack packet)`.
- World and server packs are never edited; per-server and per-pack choice
  are not wanted now.

## Code

- `ui/LanguageView.cpp` (page), `ui/LanguageSelection.h` (pure: the
  interval), `ui/GameLanguage.cpp` (queued apply and checks), row in
  `ui/SettingsRows.h`, current name drawn in `ui/SettingsTableView.cpp`.
- `features/visuals/ServerPacks.cpp` (join and stack-packet hooks; toast
  from `features/information/InfoHud.cpp`).
- `xmake f --language_trace=y`: read-only research hooks in
  `features/research/LanguageTrace.cpp`, off in normal builds. They keep all
  arguments, original calls and return values; Lamium initiates no
  selection, pack change, packet change or preference write; no assets,
  packet contents, server addresses or pack identifiers are logged. At most
  300 entries per enable; settings snapshots keep only strings; list
  enumeration is capped at 96 entries and runs only when the snapshot
  changes. It logs General settings' language entries and flags with the
  current and saved language, the selection and I18n calls with game, font
  and pack observers, pack stack operations as type/count/result (Level=0,
  Global=1, Treatment=2, BaseGame=3) and received server pack information
  as flags and counts.

## Known

- Vanilla's in-world settings have no Language tab (static `not
  $is_pregame` condition in the settings screen JSON; the maintainer confirmed
  it on 1.26.51.01); the controller still holds the list. In-world Global
  Resources lists active packs with no control to change them.
- Title-screen selection is done by OreUI, not the JSON controller. Order
  seen: I18n selection, game observer, font update, then the pack-language
  callback. The saved language is still the old one inside the I18n call.
- `I18n::chooseLanguage` already calls `ResourcePackManager::onLanguageChanged`;
  calling it again ran it twice. Do not invoke observers independently or
  during UI rendering.
- SDK: `I18n::getSupportedLanguageCodes`, `getLanguageName`, `getLocaleFor`,
  `chooseLanguage`; `IOptionRegistry::getLanguage`, `setLanguage`,
  `saveIfNeeded`; `GeneralSettingsScreenController::_chooseLanguage(code,
  index)` with its owned `mLanguages`. 29 supported languages on
  1.26.51.01.
- In-world switching froze the game after 12 changes in 23 s; the title
  screen does not. Cause unknown; hence the interval.
- English Search keeps a Localization pointer until world exit; schematic
  entity/material display names can also be retained.
- Server packs: the required flag
  (`ResourcePacksInfoPacketPayload::mResourcePackRequired`) means packs must
  be applied to join; an optional decline is not the same as disabling a
  required pack while staying connected. After the client completes the
  normal download/acceptance, the server sends the stack to apply
  (`ResourcePackStackPacket`); emptying its packs before the client handles
  it joins and stays connected with the packs unapplied.
- With a required server pack, vanilla itself applies no global packs and
  shows the gray Global Resources page, with or without Lamium.
- The SDK separates Level and Global stacks and exposes
  `ResourcePackManager::setStack`; that does not establish that changing a
  Level stack during play safely reloads meshes, atlases, UI, languages and
  entity/block resources. A server pack can supply custom content and UI,
  so removing it can leave received gameplay state unrenderable.

## Tried

- Listing only already-loaded languages (two in practice): use I18n's
  supported list.
- Calling `ResourcePackManager::onLanguageChanged` after `chooseLanguage`:
  it ran twice.
- Declining a required pack: expected to disconnect, not tried.

## Limits

- The player's own global packs are not applied while server packs are
  skipped; reapplying the Global stack in a server session needs research
  (L-167 Open).
- Whole-pack versus selected-resource overrides is an open product choice.
- Unchecked in game: English Search and schematic ghosts after a change,
  BDS for language. [VALIDATION.md](VALIDATION.md) row "In-world language
  selection (L-167)".

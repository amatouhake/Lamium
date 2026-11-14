# Item details

Implementation notes for the lines Lamium adds to an item's tooltip (L-319):
durability, work penalty, a suspicious stew's effect, food values, property
values, the ID, tags and NBT. Work status and open choices are in
[BACKLOG.md](BACKLOG.md) under L-319; runtime coverage is in
[VALIDATION.md](VALIDATION.md), with evidence in
[VALIDATION-LOG.md](VALIDATION-LOG.md). The player-facing description is in
[FEATURES.md](FEATURES.md) ("Container previews and Item details").

Read "Current behavior" for what the code does now. "Record" is the dated
build history and contains superseded trials (armour lines, an indented NBT
tree, a held key with a long form, a Latin-raise setting).

## Current behavior

Checked against the code at `0abdb291` (2026-11-14). Built after 0.6.0
(first commit `8a3f8cca`, 2026-11-13); not in a release yet.

### Where it runs

- One hook, `DurabilityHovertext` on `ItemStackBase::getFormattedHovertext`
  (`src/features/inspection/Inspection.cpp`), appends the lines to the
  game's own text. The game lays out and places whatever text it is given;
  Lamium does not resize or move the tooltip.
- Pure part: `src/features/inspection/ItemDetails.h` (`details::lines`,
  `wrapped`, `clip`, `plain`, `effectTint`, `modifierLine`, the limits),
  tested in `tests/ItemDetailsTests.cpp`; `enchantPlan::workPenalty` and
  `priorUses` in `src/features/inventory/EnchantPlan.h`.
- The game asks for a slot's hover text once, when the pointer reaches the
  slot, and keeps it. A setting changed meanwhile shows at the next hover.
  `LockedTrades.cpp` calls `getFormattedHovertext` itself for a hovered
  locked trade, so the hook also runs from there.
- The hook body is wrapped in `try`/`catch (...)`: on a failure the text is
  returned with whatever was appended so far.

### Settings

One feature row, "Item details" (`inspection.itemTooltip`, on by default),
is the main switch. Off: the hook returns the game's text untouched and the
child switches keep their values; their rows are dimmed and the footer says
why (DESIGN, Components). Children, in the tooltip's order:

| Row | Settings key | Default | Toggle key |
|---|---|---|---|
| Item details (main switch) | `inspection.itemTooltip` | on | `itemtooltip` |
| Durability numbers | `inspection.durability` | on | `durability` |
| Work penalty | `inspection.workPenalty` | on | `workpenalty` |
| Suspicious stew effect | `inspection.itemDetailsStew` | on | `itemstew` |
| Food values | `inspection.foodValues` | on | `foodvalues` |
| Property values | `inspection.itemDetails` | on | `itemdetails` |
| Show ID | `inspection.itemDetailsId` | on | `itemid` |
| Show tags | `inspection.itemDetailsTags` | off | `itemtags` |
| Show NBT | `inspection.itemDetailsNbt` | off | `itemnbt` |

- All toggle keys are unbound by default. They work outside screens only,
  so a key is pressed before the inventory is opened (known and left).
- Each child works alone: tags and NBT do not need Property values on.
- A settings file without `itemDetailsStew` or `itemDetailsId` takes the
  file's `inspection.itemDetails` for them (`SettingsStore.cpp`), so a file
  from before the split keeps what it showed.
- There is no key that changes the tooltip while held, and no count-only
  form of the NBT.

### Lines, top to bottom

After the game's own text:

1. **Durability** (grey): remaining / maximum, for a damageable item.
2. **Work penalty** (grey): "Work penalty: +3 (2 anvil uses)" from
   `getBaseRepairCost()`. Nothing when the stored cost is 0 or less. A cost
   that is not 2^n - 1 is shown alone ("Work penalty: +5"), without a
   count. It does not predict an operation's cost.
3. **Suspicious stew effect**: only for `minecraft:suspicious_stew`, and
   only when the tooltip does not already hold it (compared without colour
   codes, so Creative, where the game prints it, shows it once). The text
   is `Item::buildEffectDescriptionName(stack, true)`, followed by one line
   per attribute the effect changes (`MobEffect::mAttributeModifiers`,
   `getAttributeModifierValue(0, ...)`, named by the game's
   `attribute.name.<attribute>` text and left out where that text is
   missing or the value is 0). The first effect level is assumed. Colour
   as the game's: red for a harmful effect, blue otherwise, white when the
   effect cannot be told. The effect is found by the longest translated
   effect name the line starts with (`MobEffect::getNameById` 1..38); the
   last answer is remembered and asked again only when the line changes.
4. **Food values**: a line of placeholder glyphs that `TooltipPainter`
   covers with hunger-bar icons (L-92/L-64 machinery; `findGlyphRun` takes
   the last line made of glyphs alone, so lines may follow it).
5. **Property values** (grey), each only where the item has one:
   enchantability above 0; a stack limit from 2 to 63; cooldown in
   seconds; use time in seconds, up to 60 s; fuel as items smelted.
   - Fuel: `FurnaceBlockActor::getItemBurnDuration(item, 200)` / 200;
     where that answers 0, the `minecraft:fuel` component's
     `mFuelDuration` (seconds). What the function's second argument means
     was not checked in the headers; coal reading 8 was seen in game.
   - Use time: `Item::mMaxUseDuration`; where that is 0, the
     `minecraft:use_modifiers` component's `mUseDuration` (seconds).
   - Numbers have up to two decimals, without trailing zeros.
6. **ID** (dark grey): the type name, with " #N" for a data value other
   than 0.
7. **Tags** (dark grey): `Item::mTags`, joined by ", " into rows of about
   44 bytes, four rows at most, then an ellipsis. One row sits beside the
   "Tags:" label; more go under it, every row indented two spaces.
8. **NBT** (dark grey): one line per top-level entry of `mUserData`,
   "key: value", the key cut at 40 characters and the value at 60; a
   compound or list is its whole SNBT text on that line. At most 24
   entries, then "… N more". A section sign in the data is replaced by
   "&" so it cannot recolour the tooltip.

Not shown, on purpose: armour, armour toughness, knockback resistance and
attack damage (the game prints them); attack speed (the game has no attack
cooldown); a bow's or shield's use time (above the 60 s limit).

### Version gate (`versionSensitiveAllowed`)

- The work penalty line asks `versionSensitiveAllowed(EnchantOrder)`, the
  capability of the enchantment order: on a game version that is not
  verified the line is left out. That capability belongs to the
  `enchantOrder` feature, so nothing marks Item details as paused when the
  line goes missing.
- The suspicious stew's effect, the property values, the ID, tags and NBT
  ask `versionSensitiveAllowed(ItemDetails)`, a capability of this feature
  (decided 2026-11-14, L-338; before that they were read without a gate).
  On a game version that is not verified those lines are left out and the
  feature is marked partly paused; the game's tooltip, the durability
  numbers and the food values stay. What they read: `Item::mTags`,
  `ItemStackBase::mUserData` and `CompoundTag::mTags`, `Item::mFullName`,
  `Item::mMaxUseDuration`, `FurnaceBlockActor::getItemBurnDuration`,
  `FuelItemComponent::mFuelDuration` and
  `UseModifiersItemComponent::mUseDuration` (component pointers cast by
  component name), `Item::getEnchantValue`, `getCooldownDuration`,
  `getMaxStackSize`, `Item::buildEffectDescriptionName` and the
  `MobEffect` members `mDescriptionId`, `mIsHarmful`,
  `mAttributeModifiers`. Reads only; no game memory is written.

### Known differences from what the settings say

- "Main switch off leaves the game's own tooltip" holds for the lines
  above only. Hiding a shulker box's contents text belongs to Container
  previews (`ShulkerContentsText` checks `containerPreviews`,
  `shulkerPreviews` and `hideShulkerContents`, not `itemTooltip`) and stays
  in force with Item details off.
- With "Show NBT" on, each of the first 24 entries is serialised whole
  (`toSnbt`) and cut to 60 characters afterwards. The first trial's rule
  "compounds and lists by size only, so a full shulker box is not
  serialised each frame" is no longer in the code. The game's slots ask
  once per hover; the locked-trades tip asks every frame.

### Not checked in game

From VALIDATION.md (both rows, 2026-11-13, local world only):

- Item details: a settings file from before the split; languages other
  than those seen (English, Japanese, Chinese); servers; an unverified
  game version.
- Work penalty: a cost that is not 2^n - 1 (shown without a count); the
  toggle key; an unverified game version; servers.
- Everything confirmed was confirmed on builds of 2026-11-13 (`8a3f8cca`
  to `fab713d8`). The inspection folder changed afterwards for the
  container preview only; the item-detail code is as confirmed on
  `a3a8dcca`, `e6562d2e` and `988806af`.

## Decisions

Maintainer decisions still in force. Where a later one replaced an earlier
one only the later is listed; the record below has both.

- 2026-11-07: show the anvil work penalty and its prior-use count; do not
  predict a whole operation's cost.
- 2026-11-07: derived values use deterministic formulas only.
- 2026-11-13: the work penalty is one grey line under durability; nothing
  for a stored cost of 0; a cost that is not 2^n - 1 is shown without a
  count; its own setting, on by default.
- 2026-11-13: the work penalty is read as the enchantment order reads it
  and asks the same capability (`EnchantOrder`).
- 2026-11-13: the armour lines are not shown; the game prints the same
  values.
- 2026-11-13: attack speed is not shown; the game has no attack cooldown.
- 2026-11-13: a suspicious stew's effect is shown outside Creative, above
  the food icons, in the game's words and colours with its attribute lines.
- 2026-11-13: the food icons sit above Lamium's detail lines.
- 2026-11-13: NBT is the raw content, one line per top-level entry, cut at
  60 characters, at most 24 entries then a count; no indented tree, no
  scrolling.
- 2026-11-13: the line says "NBT", the only name the game's language file
  has for it.
- 2026-11-13: enchantability keeps its line; what the number means is told
  in the setting's help, not in the tooltip.
- 2026-11-13: no key that switches the tooltip's form inside the
  inventory; "Show tags" and "Show NBT" are toggles, off by default, with
  no count-only form.
- 2026-11-13: one heading "Item details" with a child switch per kind of
  line, in the tooltip's order; Durability and Work penalty stay apart;
  the five property values stay together; fuel stays inside Property
  values.
- 2026-11-13: nothing saved changes meaning: no settings key or action id
  is renamed, and the two new keys default to the file's
  `inspection.itemDetails`.
- 2026-11-13: Item details has a main switch (`inspection.itemTooltip`,
  on by default) whose use is comparing with the game's own tooltip.
- 2026-11-13: every row can have a key, all unbound; F3+H is not a
  default.
- 2026-11-13: the ID stays on by default.
- 2026-11-13: "Hide shulker box contents text" is on by default (a saved
  choice is kept); the rows say "shulker box", not "Shulker".
- 2026-11-13, shared rules recorded in [DESIGN.md](DESIGN.md): a
  feature's children are dimmed while its switch is off and their names
  are not rewritten (Components); a child's toast reads "Feature > Child"
  and the toggles of one key press share a toast (Toast); the Waypoints
  switch is the whole feature's switch; the Latin raise is fixed,
  Japanese-only, 1 unit, with no setting (Text and languages).

## Record

Moved verbatim from BACKLOG.md's L-319 section on 2026-11-14, in its
order (oldest first). It is a build diary: later rounds supersede earlier
ones, "not yet checked in game" is as of each round, and the first line's
"Kind: Design" is the item's kind when it was opened. Do not rewrite it.

Kind: Design. Maintainer's note 2026-11-07 (multiplayer-session feedback).
Target Info gives blocks a rich card, while items only get container
contents, durability and food values in the tooltip. Main ask: show the
anvil Work Penalty — the item's RepairCost as the level surcharge it adds
to any anvil operation (a rename alone costs penalty + 1) and the
prior-use count behind it (penalty = 2^n - 1). Check both against the cost
the anvil screen shows before trusting them, as L-170 says (the 2^n - 1 rule
was read from the screen 2026-11-12, VALIDATION-LOG). Do not predict a whole
operation's cost; that depends on the other item and the operation.
Then investigate what else the client knows about an item but Lamium does
not show, and show that too. Candidates to verify in code before listing
them as displayable: per-instance data (suspicious stew effect; firework
detail vanilla does not print) and item-definition data (attack damage
and speed, armour value, fuel burn time, anvil repair material,
enchantability, max stack). A derived value (grindstone experience
return) needs its own call — deterministic formulas only. Home: the item
tooltip, beside durability and food values.
Header read 2026-11-12 (SDK 26.60.4; reading only, nothing run):
- Linkable or virtual, so readable without a symbol request:
  `ItemStackBase::getBaseRepairCost()` (the Work Penalty) and
  `getMaxStackSize()` (`MCAPI`); `Item::getAttackDamage()`,
  `getArmorValue()`, `getEnchantValue()`, `getEnchantSlot()`,
  `getMaxDamage()` and `isValidRepairItem(...)` (virtual).
- Not found as a linkable accessor: fuel burn time (only
  `FuelItemComponent::mFuelDuration` for component items, and the brewing
  stand's `PotionBrewing::getFuelValue`, `MCNAPI`), attack speed, and a
  reader for the suspicious stew's effect (`SuspiciousStewItem` has only the
  type enum). These stay off the list until a source is found.
- Pure part done: `enchantPlan::priorUses(penalty)` in
  `src/features/inventory/EnchantPlan.h` gives the prior-use count for a
  stored surcharge, or nothing when the value is not 2^n - 1. Not shown
  anywhere yet.
Decided by the maintainer 2026-11-13, built the same day and confirmed by
the maintainer in game (`8a3f8cca`, VALIDATION-LOG: one, two and three
anvil uses agreeing with the anvil's cost, nothing on a fresh item, a line
on a merged book, the setting off):
- First only the work penalty; the other values wait for a look at what
  is worth showing and what the game already prints.
- One line under the durability line, in the same grey: "Work penalty: +3
  (2 anvil uses)". Nothing for an item whose stored cost is 0. A cost
  that is not 2^n - 1 is shown alone, without a count (pure
  `enchantPlan::workPenalty`).
- A setting of its own, "Work penalty", on by default, a feature row
  beside Durability numbers and Food values with its toggle key (unbound).
- It is read as the enchantment order reads it and asks the same
  capability (`EnchantOrder`): on a game version that is not verified the
  line is left out.
Still open, not started:
- Which definition values to show. Recommended: only values that differ
  from "nothing" (attack damage above 0, armour above 0), after checking
  what the game's own tooltip already prints; enchantability and max
  stack are rarely useful and can wait.
The maintainer's direction 2026-11-13 (the work stops here for now; not
decisions):
- The rest is meant wide: from attack damage and armour to what the game
  folds into "+NBT", as much about an item as can be shown.
- Durability numbers, Food values and Work penalty all add a line to an
  item in an inventory, so they may belong together in the settings. The
  doubt was whether Food values also covers the HUD: it does not. Food
  values (`inspection.foodValues`) is the tooltip line only; the hunger
  bar's outline and its preview of held food are the separate feature
  Saturation (`information.saturation`, `information.saturationPreview`).
  The agent's view: group them when the wider values arrive, as one
  feature with a switch per line; the settings file's keys and the three
  toggle keys can stay as they are, since only the rows' grouping changes.
Decided by the maintainer 2026-11-13 (later the same day) and built as a
trial, not yet checked in game:
- All four groups are shown: armour values, fuel, the ID with tags and
  stored data, and the other definition values. Always shown for now; what
  to do if a tooltip gets too long is decided after seeing it.
- One setting, "Item details" (`inspection.itemDetails`, on by default,
  toggle key unbound), a feature row after Work penalty. The agent's
  choice for the trial; a switch per line waits for the grouping above.
- The lines, after the work penalty and before the food icons (pure
  `inspection/ItemDetails.h`): grey for armour, armour toughness, knockback
  resistance (%), enchantability, a stack limit between 2 and 63, cooldown
  (s), fuel as items smelted; dark grey for the ID ("#N" for a data value
  other than 0), the tags (wrapped, four lines at most) and the top level
  of the stored data (eight entries, then a count; compounds and lists by
  size only, so a full shulker box is not serialised each frame).
- Fuel is `FurnaceBlockActor::getItemBurnDuration(item, 200)` divided by
  200. What the second argument means was not checked: coal must read 8.
- Left out: the time an item takes to use (`Item::getMaxUseDuration` wants
  an `ItemStack` the tooltip does not always have), attack speed and the
  suspicious stew's effect (no reader found), and attack damage (the game
  prints it; to confirm in game).
To see in game: the values against known ones (netherite chestplate 8 / 3 /
10%, coal 8, ender pearl stack 16 and cooldown 1 s), how long the tooltip
gets on an enchanted tool and a shulker box, and whether any line repeats
what the game already prints.
Seen by the maintainer 2026-11-13 on `63c672ca` (VALIDATION-LOG) and
changed the same day, not yet checked in game:
- The armour lines are gone: the game prints the same three values.
- Coal and charcoal showed no fuel while logs (1.5) and a lava bucket (100)
  did. Where the furnace's function answers 0 the burn time is now read
  from the item's `minecraft:fuel` component (seconds).
- Stored data is told whole (a picked piston hid one entry behind the
  count): compounds and lists are walked, each level indented, up to 48
  lines and 4096 entries walked. The maintainer: first show all that can
  be read, calm the look afterwards.
- The food icons sit above the detail lines (`findGlyphRun` takes the last
  line made of glyphs alone, no longer only the text's last line).
- A suspicious stew's effect, which the game prints only in Creative, is
  added from `Item::buildEffectDescriptionName(stack, true)` when the
  tooltip does not already hold it. Asked for that one item only.
- Use time is read from the `minecraft:use_modifiers` component (seconds),
  so only component items have it; values above 60 s are not told.
- Attack speed is dropped from the list: the game has no attack cooldown.
Open: "Enchantability: 15" tells that an item can be enchanted but not
what the number means to a player; the maintainer keeps the line and may
want a better wording. Tooltip length waits until the repeats are gone.
Seen by the maintainer 2026-11-13 on `6f70bdb2` (VALIDATION-LOG): coal and
charcoal read 8, the picked piston's data is whole, the stew's effect shows
in Survival, the food icons sit right above the details. Changed the same
day, not yet checked in game:
- The stew's effect came without the game's colour and was repeated in
  Creative, where the game prints it itself. The line is now compared and
  shown without colour codes, and coloured as the game does: red for a
  harmful effect, blue otherwise, white when the effect cannot be told.
  Harmful is `MobEffect::mIsHarmful` of the effect whose translated name
  the line starts with.
- No use time showed on a bow, bread or a milk bucket: they have no
  `minecraft:use_modifiers` component. `Item::mMaxUseDuration` is read
  first now, the component only where that is 0.
Seen by the maintainer 2026-11-13 on `70de5f54` (VALIDATION-LOG): the
stew's effect red or blue in Survival and not repeated in Creative; use
time on bread and a milk bucket (1.6 s), dried kelp (0.8 s) and a crossbow
(1.25 s), none on a bow. Changed the same day, not yet checked in game:
- In Survival the stew's effect sat under the food icons and lacked the
  game's second line ("-0.5 Attack Damage" under Weakness). The effect
  now goes above the food icons, where the game puts it, followed by one
  line per attribute the effect changes: `MobEffect::mAttributeModifiers`
  with `getAttributeModifierValue(0, ...)`, named by the game's
  `attribute.name.<attribute>` text and left out where that text is
  missing. The first level is assumed, which is what a stew gives.
  Confirmed by the maintainer the same day on `becfac62` (VALIDATION-LOG):
  Weakness in Survival reads as in Creative, an effect without an
  attribute adds no line, Creative shows nothing twice.
Decided with the maintainer 2026-11-13 (the design talk), built the same
day, not yet checked in game:
- Two forms. Short, by default: the value lines, the ID and the tags as
  before, and the stored data as one line with its count ("NBT: 12
  entries", first worded "Stored data"). Long, while a key is held: the count gives way to the data.
- The long form is the raw content, nothing left out, but not a tree:
  each top-level entry is one line, a compound or list as its whole text
  on that line, cut at 60 characters; at most 24 entries, then a count. A
  shulker box's items are one cut line: seeing them is the container
  preview's job. The indented tree is not taken for now.
- The key is an action of its own ("Show NBT in tooltips while held",
  `itemdetailsmore`), held, Alt by default. The agent's choice among
  keys that all mean something in an inventory: Shift and Ctrl are held
  for nearly every transfer, Alt only for "every item of a kind" clicks,
  so the tooltip changes least often by accident. The maintainer is not
  against Alt but has not settled it.
- Enchantability keeps its line ("Enchantability: 15"); what the number
  means is told in the setting's help, not in the tooltip.
- No scrolling: with one line per entry it is not needed, and the wheel
  stays Inventory transfer's.
Seen by the maintainer 2026-11-13 on `83bddd4c` (VALIDATION-LOG): the
count line shows and Alt + click still moves every item of a kind, but the
long form never came, with Alt or another key. Changed the same day, not
yet checked in game:
- The key was only allowed during gameplay, like most actions, and
  tooltips are in container screens. It is now a container-only key, as
  Sort is (`containerOnly` in `Binding.h`), so it also only meets
  container keys in the Hotkeys conflict list. If the long form still does
  not come, the game keeps a slot's tooltip text instead of asking each
  frame, and the switch has to move to where the text is drawn.
- Tags that wrap were ragged: the first row after the label, the others
  indented two spaces. One row stays beside the label; more go under it,
  every row indented alike.
- "Stored data" was Lamium's own word. The game has no player-facing name
  for it; its language file only says "NBT" (a command message), so the
  line and the key say NBT.
Seen by the maintainer 2026-11-13 on `28d12745`: wrapped tags line up;
the long form still never comes, held before or after the pointer reaches
the item. A second gameplay-only check sat in `executeAction`
(`Actions.cpp`), ahead of the line that sets the held state; the press is
now handled before it, beside Sort. Not yet checked in game.
Seen by the maintainer 2026-11-13 on `f3b0e62f` (VALIDATION-LOG): the key
works, but the form only changes when the pointer moves to another item,
on pressing and on releasing; the long form of a shulker box fits the
screen and is enough to look into a structure or a picked piston. Changed
the same day, not yet checked in game:
- The game asks for a slot's hover text once, when the pointer reaches it.
  Both forms are now built then and remembered by their text, and the
  tooltip's painter (`TooltipPainter`) puts the other one in for the draw
  when the key's state differs: `HoverRenderer::mFormattedContent` and
  `mBoxDimensions`, written for one draw and put back. The new box keeps
  the room the game left around the text and takes the other text's lines
  and widest line (pure `details::resized`). Its place is not recomputed:
  a long form opened near the screen's edge may run off it until the
  pointer moves. Gated by a capability of its own (`TooltipForms`); on a
  game that is not verified the form follows the key at the next hover.
Seen by the maintainer 2026-11-13 on `bde4219b` (VALIDATION-LOG): the
form changes at once on pressing and releasing, the food icons stay in
place in the long form, but the box keeps the size of the text the game
had: `mBoxDimensions` is not what the draw uses. Changed the same day, not
yet checked in game: the hover control's own `mSize` is set to the new box
and its `mCachedPosition` moved back so the box ends inside the screen
(the outermost parent's size; pure `details::keptInside`), both put back
after the draw, as Scoreboard position does with its control.
Seen by the maintainer 2026-11-13 on `d9db34bb`: the box still does not
change with Alt. Neither the renderer's `mBoxDimensions` nor the control's
`mSize` draws the frame. Third try, not yet checked in game: the frame is
the renderer's `NinePatchLayer` (`mLayer`), which holds its size (`w`,
`h`) and nine cached quads; for the draw the quads' far coordinates are
moved by the growth (pure `details::stretched`) and put back. The first
six swaps log what the game holds ("Tooltip forms: ..." in lamium.log:
control, box, cursor, offset, frame size and quads) so the next step can
be read from the log if this fails too; take the line out once the frame
follows.
Seen by the maintainer 2026-11-13 on `ff043a26` (VALIDATION-LOG): the
frame now takes the long form's size on Alt, but not its place: the game
places a tooltip for the text it has when the pointer arrives, so a form
changed afterwards runs off the screen, leaves its text, or sits away from
the item. Held or not held from the start, both forms look right.
Decided by the maintainer the same day, built, not yet checked in game:
- No key. Switching inside the inventory is hard to understand, and with
  one line per entry the long form is rarely too tall. The held action
  (never pushed, so no saved id is left behind), the swap while drawing,
  its capability and its log lines are removed; the game lays out
  whatever text it is given.
- Two detail toggles under Item details, both off by default: "Show tags"
  (`inspection.itemDetailsTags`) and "Show NBT"
  (`inspection.itemDetailsNbt`). No count-only form for NBT: the count
  alone is seldom wanted, and two states make it a toggle.
- The value lines and the ID stay under the one Item details switch.
Confirmed by the maintainer the same day on `a3a8dcca` (VALIDATION-LOG).
For a later session, the maintainer's wish: look at Durability numbers,
Food values, Work penalty and Item details (with its two toggles) with
fresh eyes and decide how these tooltip settings should be grouped and
split. Not to be decided as a by-product of other work.
Asked by the maintainer the same day: hiding the game's list of contained
item names in a shulker box's tooltip, since the preview shows the same.
That exists: "Hide Shulker contents text" under Container previews, off by
default. Whether it should be on by default is the maintainer's call.
Decided by the maintainer 2026-11-13: on by default, since the shulker box
preview is on by default and shows the same contents (a saved choice is
kept; only installs without one change). The same day the rows were renamed
from "Shulker" to "shulker box" in English and Japanese ("Shulker box
previews", "Show empty shulker boxes", "Hide shulker box contents text" and
their help), because "Shulker" alone reads as the mob; the Chinese rows
already said "shulker box". Built and confirmed 2026-11-13 (`26ebab3a`).
Decided by the maintainer 2026-11-13 in that session, looking only at what
each line shows and for whom, built the same day and confirmed by the
maintainer on `e6562d2e` (VALIDATION-LOG):
- One heading, "Item details", without a switch or key of its own (as Map
  text is), and a child switch per kind of line in the tooltip's order:
  Durability numbers, Work penalty, Suspicious stew effect, Food values,
  Property values, Show ID, Show tags, Show NBT. The four feature rows are
  gone; their toggle keys sit on the child rows.
- The old Item details switch held three unlike things and is split. The
  stew's effect tells what the game hides in Survival, so a player may not
  want it (`inspection.itemDetailsStew`). The ID is the one line on every
  item and is for commands and packs (`inspection.itemDetailsId`). The five
  values of an item's kind stay together as Property values, which keeps
  `inspection.itemDetails` and the `itemdetails` key. Durability and Work
  penalty both tell the one item's state but are read by different players,
  so they stay apart.
- Nothing saved changes meaning: no settings key or action id is renamed.
  The two new keys, when a file lacks them, take the file's
  `inspection.itemDetails`, so nothing appears that was switched off. Tags
  and NBT no longer need Item details on.
- Not asked, left as they were: the ID stays on by default, fuel stays
  inside Property values.
Decided by the maintainer 2026-11-13 (later), built the same day and seen
by the maintainer on `988806af` (VALIDATION-LOG):
- Item details gets a main switch (`inspection.itemTooltip`, on by
  default), as Hide effects has: off leaves the game's own tooltip, the
  child switches keep their values and are marked "Main switch off". Its
  use is comparing with what the game itself shows.
- Every row can have a key: the main switch (`itemtooltip`) and the four
  children that had none (`itemstew`, `itemid`, `itemtags`, `itemnbt`).
  All unbound. F3+H is not a default: a player who knows that key from
  Java Edition expects the ID and technical lines, not this switch.
- The ID stays on by default: it is how a player finds an item's English
  name for a search or a command.
- The help shown on a child while a main switch is off named Hide effects
  for every feature; it now says "this feature".
Known and left: the toggle keys only work outside screens, so a key is
pressed before the inventory is opened.
Seen by the maintainer on `988806af`, three doubts, answered the same day:
- The toast of a child's key drew the main switch's state. Fixed
  (`cd71814f`) for every key that sits on a child switch's row.
- "(Main switch off)" was added to the child's name. Decided: names are not
  rewritten; the row is dimmed and the footer says why (DESIGN, Components).
- Only Hide effects, Game HUD placement and Item details dimmed their
  children. Decided: every feature with a switch does. Checked in code for
  the children that might act with the feature off (the stew preview of
  Saturation, the death point of Waypoints, the mark layers of Map marks,
  the hide options of Info HUD, Debug View and Status effects); only the
  death point does, so Waypoints is left undimmed. The rest of the child
  settings were not read one by one. Confirmed by the maintainer on
  `d80c18ca` (VALIDATION-LOG).
Three more doubts from that check, decided by the maintainer the same day
(DESIGN: Components, Toast), built, not yet checked in game:
- A child's toast did not say which feature it belongs to. It reads
  "Feature > Child" now, from the settings screen's labels; the toast-only
  names are removed.
- Keys bound alike all fired but each toast replaced the one before. The
  toggles of one key press share the toast, a row each (four, then a count).
- Waypoints' switch only hid the markers while a death point was still
  recorded. It is the whole feature's switch now, so Waypoints dims its
  children like every other feature.
Confirmed by the maintainer on `b8ddb003` (VALIDATION-LOG). The sign
between the names was "›": it drew, but in another font in English and
low in Japanese, so it is ">" now (confirmed on `5220a3db`). The toast's
text sat at its row's top, about two units above the switch's middle; it
is centered in the row now, as a HUD line with a background is. Not yet
checked in game.
Decided by the maintainer the same day, built, not yet checked in game:
the raise of Latin runs, a hidden 1.5 units in Japanese only, becomes the
setting "Latin text raise" (DESIGN, Text and languages). In the
screenshots Japanese looked raised too far and Chinese, with no raise, had
Latin runs low. Open: the Automatic values per language, to be read off
the setting by the maintainer (Japanese now 1.5, Chinese 0).
Seen by the maintainer on `dba113dd` (VALIDATION-LOG): Japanese is right at
1; in Chinese a raised Latin run changes font and width, because a run
drawn apart from the hanzi gets the pixel font, so the setting cannot do
there what it says. The toast's centered text was right in English and a
unit low in Japanese and Chinese; it starts a unit higher in those now
(`rowTextLift`), not yet checked in game.
Decided by the maintainer the same day: the setting is removed (it was
never released, so `interface.latinRaise` needs no migration) and the raise
stays Japanese-only, now 1 unit instead of 1.5 (DESIGN, Text and
languages). Confirmed by the maintainer on `fab713d8` (VALIDATION-LOG),
with the toast's text beside its switch in Japanese and Chinese.
Still open: the enchantability wording if the help text is not enough.

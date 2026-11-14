# Item details

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/ITEM-DETAILS.md`.

Lines Lamium adds to an item's tooltip: what the client knows about an item
and the game does not print. Direction (maintainer 2026-11-13): as much
about an item as can be shown. Open work:
[L-319 Item details: Work Penalty first, then the unshown rest](backlog/L-319.md).
Player-facing description: [FEATURES.md](FEATURES.md) ("Container previews
and Item details").

One heading, "Item details", with a main switch and a child switch per kind
of line, in the tooltip's order. Every row can have a key; all are unbound
(F3+H is not a default: a player who knows it from Java Edition expects the
ID and technical lines, not this switch).

| Row | Settings key | Default | Toggle action |
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

## Behavior

- Main switch off: the hook returns the game's text untouched and the
  children keep their values; their rows are dimmed as
  [DESIGN.md](DESIGN.md) "Screens" says. Its use is comparing with the game's own tooltip.
- Each child works alone: tags and NBT do not need Property values on.
- A settings file without `itemDetailsStew` or `itemDetailsId` takes the
  file's `inspection.itemDetails` for them, so a file from before the split
  shows nothing that was switched off. No settings key or action id is
  renamed.
- The ID stays on by default: it is how a player finds an item's English
  name for a search or a command.
- Grouping (maintainer 2026-11-13): Durability and Work penalty stay apart
  (they tell the one item's state but are read by different players); the
  five property values stay together; fuel stays inside Property values;
  the stew's effect has its own switch because it tells what the game hides
  in Survival; the ID has its own because it is for commands and packs.
- Derived values use deterministic formulas only. No whole anvil operation
  cost is predicted.
- No key changes the tooltip while held, no count-only NBT form, no
  scrolling (the wheel stays Inventory transfer's).

### Lines, top to bottom

After the game's own text:

1. **Durability** (grey): remaining / maximum, for a damageable item.
2. **Work penalty** (grey): "Work penalty: +3 (2 anvil uses)" from
   `getBaseRepairCost()`, one line under durability. Nothing when the
   stored cost is 0 or less. A cost that is not 2^n - 1 is shown alone
   ("Work penalty: +5"), without a count. Read as the enchantment order
   reads it (the 2^n - 1 rule was read from the anvil screen, L-170).
3. **Suspicious stew effect**: only for `minecraft:suspicious_stew`, and
   only when the tooltip does not already hold it (compared without colour
   codes, so Creative, where the game prints it, shows it once). Above the
   food icons, where the game puts it. The text is
   `Item::buildEffectDescriptionName(stack, true)`, followed by one line per
   attribute the effect changes (`MobEffect::mAttributeModifiers`,
   `getAttributeModifierValue(0, ...)`, named by the game's
   `attribute.name.<attribute>` text and left out where that text is
   missing or the value is 0), e.g. "-0.5 Attack Damage" under Weakness.
   The first effect level is assumed, which is what a stew gives. Colour as
   the game's: red for a harmful effect, blue otherwise, white when the
   effect cannot be told. The effect is found by the longest translated
   effect name the line starts with (`MobEffect::getNameById` 1..38); the
   last answer is remembered and asked again only when the line changes.
4. **Food values**: a line of placeholder glyphs that `TooltipPainter`
   covers with hunger-bar icons ([L-92](backlog/done/L-92.md),
   [L-64](backlog/done/L-64.md)); `findGlyphRun` takes the last line made of
   glyphs alone, so Lamium's detail lines may follow it.
5. **Property values** (grey), each only where the item has one:
   enchantability above 0; a stack limit from 2 to 63; cooldown in seconds;
   use time in seconds, up to 60 s; fuel as items smelted. Numbers have up
   to two decimals, without trailing zeros.
   - Enchantability keeps its line; what the number means is told in the
     setting's help, not in the tooltip.
   - Fuel: `FurnaceBlockActor::getItemBurnDuration(item, 200)` / 200; where
     that answers 0, the `minecraft:fuel` component's `mFuelDuration`
     (seconds).
   - Use time: `Item::mMaxUseDuration`; where that is 0, the
     `minecraft:use_modifiers` component's `mUseDuration` (seconds).
6. **ID** (dark grey): the type name, with " #N" for a data value other
   than 0.
7. **Tags** (dark grey): `Item::mTags`, joined by ", " into rows of about 44
   bytes, four rows at most, then an ellipsis. One row sits beside the
   "Tags:" label; more go under it, every row indented two spaces.
8. **NBT** (dark grey): the raw content, one line per top-level entry of
   `mUserData`, "key: value", the key cut at 40 characters and the value at
   60; a compound or list is its whole SNBT text on that line. At most 24
   entries, then "… N more". A section sign in the data is replaced by "&"
   so it cannot recolour the tooltip. The line says "NBT", the only name the
   game's language file has for it. A shulker box's items are one cut line:
   seeing them is the container preview's job.

Not shown, on purpose (maintainer 2026-11-13): armour, armour toughness,
knockback resistance and attack damage (the game prints them); attack speed
(the game has no attack cooldown); a bow's or shield's use time (held for as
long as the button is).

### Version gate

- The work penalty line asks `versionSensitiveAllowed(EnchantOrder)`, the
  enchantment order's capability: on a game version that is not verified
  the line is left out, and nothing marks Item details as paused.
- The stew's effect, property values, ID, tags and NBT ask
  `versionSensitiveAllowed(ItemDetails)` ("Item details (item data reads)",
  partial impact). On an unverified game those lines are left out and the
  feature is marked partly paused; the game's tooltip, the durability
  numbers and the food values stay. Reads only; no game memory is written.
  What it reads: `Item::mTags`, `ItemStackBase::mUserData` and
  `CompoundTag::mTags`, `Item::mFullName`, `Item::mMaxUseDuration`,
  `FurnaceBlockActor::getItemBurnDuration`,
  `FuelItemComponent::mFuelDuration` and
  `UseModifiersItemComponent::mUseDuration` (component pointers cast by
  component name), `Item::getEnchantValue`, `getCooldownDuration`,
  `getMaxStackSize`, `Item::buildEffectDescriptionName` and the `MobEffect`
  members `mDescriptionId`, `mIsHarmful`, `mAttributeModifiers`.

## Code

- One hook, `DurabilityHovertext` on `ItemStackBase::getFormattedHovertext`
  (`src/features/inspection/Inspection.cpp`), appends the lines to the
  game's own text. The game lays out and places whatever text it is given;
  Lamium does not resize or move the tooltip. The hook body is wrapped in
  `try`/`catch (...)`: on a failure the text is returned with whatever was
  appended so far.
- `LockedTrades.cpp` calls `getFormattedHovertext` itself for a hovered
  locked trade, so the hook also runs from there, every frame.
- Pure: `src/features/inspection/ItemDetails.h` (`details::lines`,
  `wrapped`, `clip`, `plain`, `effectTint`, `modifierLine`, the limits),
  tested in `tests/ItemDetailsTests.cpp`; `enchantPlan::workPenalty` and
  `priorUses` in `src/features/inventory/EnchantPlan.h`, tested in
  `tests/EnchantPlanTests.cpp`.
- Hiding a shulker box's contents text is Container previews'
  (`ShulkerContentsText` checks `containerPreviews`, `shulkerPreviews` and
  `hideShulkerContents`, not `itemTooltip`); "Hide shulker box contents
  text" is on by default (maintainer 2026-11-13).

## Known

- The game asks for a slot's hover text once, when the pointer reaches the
  slot, and keeps it. A setting changed meanwhile shows at the next hover.
- The game places a tooltip for the text it has when the pointer arrives;
  text changed afterwards is not re-placed.
- The tooltip frame is drawn from `HoverTextRenderer`'s `NinePatchLayer`
  (`mLayer`: size and nine cached quads); neither the renderer's
  `mBoxDimensions` nor the hover control's `mSize` sizes it.
- `getItemBurnDuration` answers 0 for coal and charcoal; their burn time is
  only in the `minecraft:fuel` component. What its second argument means was
  not checked in the headers; coal reading 8 was seen in game.
- A bow, bread and a milk bucket have no `minecraft:use_modifiers`
  component; their use time is `Item::mMaxUseDuration`.
- No linkable accessor was found for attack speed (SDK 26.60.4 headers).
  `Item::isValidRepairItem(...)` (virtual) only answers for a given item; the
  repair material is not shown (L-319 Open). `Item::getMaxUseDuration` wants an `ItemStack` the
  tooltip does not always have.

## Tried

- A held key switching between a short and a long NBT form inside the
  inventory: the hover text is built once per hover, and swapping the text,
  `mBoxDimensions`, the control's `mSize` and the frame's quads at draw time
  resized the frame but could not re-place it, so a changed form ran off the
  screen or away from the item. Removed (maintainer: hard to understand, and
  one line per entry is rarely too tall).
- NBT compounds and lists shown by size only: hid entries (a picked piston).
  An indented NBT tree: not taken.
- Armour lines: repeated what the game prints.
- Fuel from the furnace function alone: coal and charcoal showed none.
- Use time from `minecraft:use_modifiers` alone: bread and a milk bucket
  showed none.

## Limits

- "Main switch off leaves the game's own tooltip" holds for the lines above
  only; "Hide shulker box contents text" stays in force (L-319 Open).
- With "Show NBT" on, each of the first 24 entries is serialised whole
  (`toSnbt`) and cut to 60 characters afterwards; the locked-trades tip asks
  for the text every frame (L-319 Next).
- The stew's effect line is left out when the tooltip already contains the
  effect's text anywhere, e.g. a renamed stew
  ([L-339 Smaller bugs found by the 2026-11-14 code review](backlog/L-339.md)).
- The toggle keys work outside screens only, so a key is pressed before the
  inventory is opened (known and left).
- Unchecked in game: the "Item details in item tooltips (L-319)" and "Work
  penalty in item tooltips (L-319)" rows in [VALIDATION.md](VALIDATION.md).

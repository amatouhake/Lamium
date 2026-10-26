# Validation log

Append-only record of what was tested, on which build, and what happened,
newest first. Entries are not rewritten. The current state per feature is in
[VALIDATION.md](VALIDATION.md); read that first and search this log (for
example `git grep -n "L-66" docs/VALIDATION-LOG.md`) instead of reading it
whole. This file was split off from VALIDATION.md on 2026-09-30 without
changing its entries.

Baseline: Minecraft 1.26.51.01, LeviLamina Client 26.51.5, Windows x64.
Entries below that name 26.51.3 were verified on that release. After the
26.51.5 update (commit 4a5b975) a brief in-game check found no regressions;
it was not a full re-run of every entry.

## Rebase note for the 2026-10-20 entries

The local commits named in the 2026-10-20 entries below were rebased onto
`origin/main` (the merged PR #31, the slime item) before they were pushed.
Those commit ids are the pre-rebase local ones and no longer resolve in the
public history; the DLL SHA-256 in each entry identifies the build that was
actually run. The binding-display item was renumbered from L-232 to L-235 at
the same time, because origin took L-232 for the slime chunk indicator.

## L-260 Connected Textures across pack changes checked in game (2026-10-26)

Local `1.26.51.01` LIP deployment, LeviLamina Client 26.51.7, Windows x64,
resource pack "Mr.Atlaz's COMPLETELY NORMAL RTX" (16 px colors, 32 px
normal maps), maintainer.
Before the fix (`9372daf4`, DLL SHA-256
`6053BF77DB8C8AC40F2D16D9FA38583673D01066704E4935AD75E31FF58E0CB8`):
switching the graphics mode between Fancy, Vibrant Visuals and Ray Traced,
rejoining and Connected Textures off/on did not reproduce the report. With
the pack disabled on the title screen and the world joined, joined faces
showed their sprites at twice the size; with it enabled again, sprites
were drawn at half size next to missing-texture squares. Connected
Textures off cleared both. The log showed a new atlas build each time,
"833 of 833 sprites in place".
After the fix (`bb49043d`, DLL SHA-256
`FE7A6DCB0A1B8E93E980B460C62C559379B0643917DCF540ABE3C46B37DE6190`):
checklist passed: pack disabled then enabled on the title screen, joining
each time and repeated, joined glass, bookshelves and sandstone drawn
normally. Not seen: the reporter's own setup (Amethyst launcher,
LeviLamina 26.51.8, world add-ons); other packs; servers.

## SDK 26.51.8 build checked in game (2026-10-26)

`main` (`9372daf4`, built against LeviLamina Client SDK 26.51.8), deployed
DLL SHA-256
`6053BF77DB8C8AC40F2D16D9FA38583673D01066704E4935AD75E31FF58E0CB8`, default
release build, local `1.26.51.01` LIP deployment with LeviLamina Client
26.51.7, Windows x64. Maintainer report: checklist passed: (1) Lamium loads
and a world can be joined; (2) Freelook, the effect-time HUD and edge guard
work as before (these read entity components through entt, whose sparse
page size the new SDK corrects). Not seen: loader 26.51.5, 26.51.6 or
26.51.8 (26.51.5 is covered only by the import check in GAME-UPDATES.md);
other features beyond this smoke check.

## PR #38 and #39 checked in game (2026-10-26)

`main` (`ffd6409b`), deployed DLL SHA-256
`6B2844486A05CCBDE1BDB7766DBD9CCE6AC9F71D4FC5FE2C0697066C0C145CFD`, default
release build, local `1.26.51.01` LIP deployment, Windows x64. Maintainer
report: checklist passed: (1) holding Freelook (Hold), pressing Escape,
closing the menu, releasing and pressing again starts Freelook each time
(PR #38); (2) with the game language set to Traditional Chinese the settings
screen shows Traditional Chinese, with 界伏蚌盒 and 組合包 for the Shulker
Box and Bundle (PR #39). Not seen: Freelook after other cancels (item use,
focus loss); zh_TW in views other than the settings screen.

## Traditional Chinese UI reported by its translator (PR #39, 2026-10-24)

Contributor report, not a maintainer check: Minecraft 1.26.51.01, LeviLamina
26.51.7, Lamium 0.3.0 with the `zh_TW` table from PR #39 (branch `zh-tw`,
`76cebe2d`; no DLL hash given). The settings and schematics screens showed
Traditional Chinese, and the wording was reviewed in game by the translator,
a native speaker. Not seen: other views, narrow windows. After the merge the
Shulker Box and Bundle names were switched back to the game's own `zh_TW`
text (界伏蚌盒, 組合包); that build was not run in game.

## L-242 follow-ups checked in game (2026-10-22)

`main` (`2900d1dc`, version 0.3.0), deployed DLL SHA-256
`E4847CC53FAB40347AB49FC79BF1D1490C041B4847ED5AEF2362B2736810A9E5`, default
release build, local `1.26.51.01` LIP deployment, Windows x64. Maintainer
report: checklist 1-3 passed: (1) the default Alt+Wheel bindings draw as
separate key caps in the key settings; (2) the world map opens, pans, zooms,
closes and reopens with no image problem; (3) the Target block line matches
the Target card, including a ghost in front and the camera tag in FreeCamera.
The normal-view screenshot showed white stained glass at 1369, 91, 115 on
both the card and the line. Not checked: a legacy placement without its
structure file (the new unavailable display), an unknown restriction mode,
and map clearing/session-only storage; the settings decode and map range
helpers are covered by `LamiumTests`, the other cases were not in the
shortened checklist. A following comment-only record (L-245, `4b3dd528`) rebuilt
the same code as DLL SHA-256
`79843CC2EA06B14847DE4B187E6A325913E7C57825DF232F7A19B224746EBBF3`;
it differs from the checked E484 build only in a source comment and line tables.

## L-241 pre-0.3.0 review fixes checked in game (2026-10-22)

`main` (`fca2ef9`, version 0.3.0), deployed DLL SHA-256
`B4030C7020AA53C402AC1AB5E02B89E990CB57606E85723E75F161DB98973B0C`, default
release build, local `1.26.51.01 LIP` deployment, Windows x64. Maintainer
report: checks 1-6 passed: `"camera": true` in settings.json kept every
other setting and the bindings; FreeCamera above the Nether created no
out-of-range `nether/y*` folders and recorded caves under the camera; "Move
area →" with corners in another dimension said there is no target; a
schematic file held open by another process loaded once released; the
Schematic HUD showed counting right after a cut change; FreeCamera's help
has no "Experimental" prefix. Not checked: a server without map storage
(temporary session folder). During these checks the maintainer saw large
terrain gaps that do not reload when looked at (L-243).

## Edge Guard help wording checked in game (2026-10-22)

`main` (`610e603`), deployed DLL SHA-256
`22BF09C0B7FA9A825E4A22A7B95586CE935DC95EA040A951E1A9FF3AA19B278E`, default
release build, local `1.26.51.01 LIP` deployment, Windows x64. Maintainer
report: Edge Guard's help ends with the softened sentence (normally no fall;
rare drops under heavy lag or a very small margin).

## 0.3.0 badge review checked in game (2026-10-22)

`main` (`c710e77`), deployed DLL SHA-256
`22185F3C460CDA10F33390E244E3696B25645A818314CB534218D82403227FBE`, default
release build, local `1.26.51.01 LIP` deployment, Windows x64. Maintainer
report: the settings show no Experimental badge on FreeCamera, Hand Restock,
Tool Protection, Auto Elytra and Edge Guard, and still show it on Inventory
transfer; Edge Guard's help shows the added sentence. Correction to the entry
below: the maintainer's point is that Edge Guard, like sneaking, normally
keeps you from falling and drops only rarely (heavy lag, a very small
margin); "no fall guarantee" overstated it. The help, README, FEATURES and
DESIGN wording was softened accordingly after this check.

## Maintainer use reports for the 0.3.0 badge review (2026-10-22)

Reported in conversation, from everyday use; builds not recorded, not a
checklist run. FreeCamera: other players still see the body, and it works on
servers. Edge Guard and Auto Elytra: work on servers without problems; Edge
Guard cannot guarantee no fall (like vanilla sneaking, lag or a small margin
can still drop you). Fake Offhand: works on servers to a degree; many items
and situations stay unverified by design. Death layout restore: sometimes
restores where the player does not recall dying, or in a layout that looks
wrong; hard to pin down (L-240). These reports back the decision to drop the
Experimental badge from FreeCamera, Hand Restock, Tool Protection, Auto
Elytra and Edge Guard for 0.3.0 (DESIGN.md).

## L-239 layer mode memory checked in game (2026-10-22)

`main` (`fce4b99`, implemented by Codex, reviewed and built before commit),
deployed DLL SHA-256
`743C0B2CA833150DC844FED17B37407F24458769A3EE573392289B376BA39DE3`, default
release build, local `1.26.51.01 LIP` deployment, Windows x64. Maintainer
report: all four checks passed: an Only cut edit survives stepping the mode
through Up to to All and Cut up returns to Only with the cut kept; an Up to
Keep or cut edit, then All and Cut here, gives Up to at the view's layer; the
Schematics screen's Cut + and typed cut follow the same rule; after
rejoining, leaving All gives Only. The first version (`5a6f8ef`, which
remembered the mode left for All) was not checked in game: the maintainer
found its rule unpredictable from the spec and it was revised first.

## L-237 presentation follow-up checked in game (2026-10-22)

`main` (`1bf531e`, implemented by Codex, reviewed before commit), deployed
DLL SHA-256 `E091F246F080526E4CD53B7ECF6BA814482FE860BF92972C5C05F6EFD28F3FE5`,
default release build, local `1.26.51.01 LIP` deployment, Windows x64.
Maintainer report: checks (e)-(h) passed: the join toast is yellow, starts
with the feature name, stays about six seconds and shows with toggle toasts
off; the unreadable Placed row is yellow including its name; the load error
sits at the same height on Placed, Check and Materials; with Schematics off,
Check and Materials say it is off instead of "Counting...".

## L-237 unreadable schematic notice checked in game (2026-10-22)

`main` (`1fd201b`, implemented by Codex, reviewed before commit), deployed
DLL SHA-256 `99498ECCD0661A5D8CF3512ACADC2BA220E8295BC768BB07AE2E77A2F4E25AEF`,
default release build, local `1.26.51.01 LIP` deployment, Windows x64.
Maintainer report: checks (a)-(d) passed (one toast on join with a renamed
file, "!" in the Placed list and the HUD message; recovery after renaming
back; no toast with Schematics off until turned on; one log line per
failure). Presentation concerns raised in the same session became the L-237
follow-up: the toast is short, uncolored, indistinguishable from the paused
notice and hidden when toggle toasts are off; only the "!" is colored; the
Check tab places the load error 2 units higher than the other tabs; with
Schematics off, Check and Materials stay at "Counting..." forever.

## L-238 schematic menu and key parity checked in game (2026-10-22)

`main` (`41d3ea7`), deployed DLL SHA-256
`CC1A542260C958CF29419C0BCF8E8DFCE45EB90A940A0DF8DA639B576879C159`, default
release build, local `1.26.51.01 LIP` deployment, Windows x64. Maintainer
report: the menu has six categories (Move, Placement, Layers, Show and
check, Area, Screen) of five or six items and stays on screen, English
included; hovering Forward/back shows its Alt+Wheel keys above the ring; the
key settings follow the menu's groups and order with the eight new keys, and
a newly bound one works. The long-name fit is otherwise covered by
`LamiumTests` only; not every new key was bound and tried.

## Docked tool titles checked in game (2026-10-22)

`main` (`18a1102`, fix in `147008f`), deployed DLL SHA-256
`C792A060D2A7B23FF4506138C7ECF16EECA55394A09060C0872ECD9951AB4A5B`, default
release build, local `1.26.51.01 LIP` deployment, Windows x64. Maintainer
report: at UI Profile 100%, docked Schematics shortens its title before the
"Key settings" link instead of running under it; docked Shapes and Waypoints
titles no longer overlap the link either.

## PR #35 docked tool panels checked in game (2026-10-22)

Local merge of PR #35 (`cd2a25d`, issue #33) onto `main` after PR #37
(`1569603`), deployed DLL SHA-256
`BE6252D707A1F54DE472843C8B1BCBE5E5F6C3B3E80795F72E87C37202794CA3`, default
release build, local `1.26.51.01 LIP` deployment, Windows x64. Maintainer
report: docked Schematics keeps "Show all" on its own row below the tabs, and
the tabs, Show all, Expand and Close respond; on a shorter window the list
gives up rows and the editor stays reachable (judged likely, not measured);
every placement field, including the new Keep row, scrolls into view. Seen in
passing at UI Profile 100%: the docked title "Schematics" ran under the "Key
settings" link. That bound predates PR #35 and is shared by Shapes and
Waypoints; it was fixed separately on `main` (`147008f`, not yet checked in
game). The too-short case (only Close, Expand and the hint) is covered by
`LamiumTests` only.

## L-234 second review fixes checked in game (2026-10-22)

Branch `schematic-layers` (`27311d8`), deployed DLL SHA-256
`5DE1B13A1525830C18CD0F0DE6D361524024AFE9DACF2FD76B360DA501C9E1BF`, default
release build, local `1.26.51.01 LIP` deployment, Windows x64. Maintainer
report: typing a cut in the Schematics screen for a placement other than the
HUD-selected one changed that placement within its own layer range; with the
schematic file renamed, a converted placement back on All could be rotated
and mirrored while Only refused with the message; Only and Up to (low and
high side) showed the same ghosts as before under rotation and mirror (the
section build now rejects hidden cells with a placed slab first). HUD and
list lookup savings and the translation cleanup were not measured or checked
separately.

## L-234 review fixes checked in game (2026-10-21)

Branch `schematic-layers` (`ec8a76e`), deployed DLL SHA-256
`7693CA222849ECBA691EA8BD0192C829B581CE0B33F8A690822A5F15F2CD4A4F`, default
release build, local `1.26.51.01 LIP` deployment, Windows x64. Maintainer
report: a placement still on the old six-direction layer view, set to Only
and then rotated and mirrored, kept the same build cells (the view is
converted on the turn); the three layer keys now sit after Layer up/down/here
in the key settings; ghosts and layer changes behaved as before. The
mistyped-`layerView` load fallback is covered by `LamiumTests` only, not
checked in game.

## L-234 cut-plane layers checked in game (2026-10-21)

Branch `schematic-layers` (`3d68fd3`), deployed DLL SHA-256
`109C3201AB961F9231F4C66B8FDA8927D5C3FA97E05A3E971F58AB00A2AB585C`, default
release build (no trace/probe options), local `1.26.51.01 LIP` deployment,
Windows x64. Maintainer report: the structure-local cut plane, axis changes
keeping the cut, attached rotation/mirror, the five-item Layers menu
(Axis/Cut/Mode/Keep/Cut here), the three layer keys, operation-naming toasts,
and the no-rebuild behavior for layer fields that do not affect visibility
all matched the intended behavior. Missing structure: with
`broken_village_house.mcstructure` renamed away, keys, menu and the settings
screen refused to change the placement and the saved layer view stayed put;
restoring the file and reloading worked normally. After the restore the
placement was edited normally, so the placement document differs from the
pre-test backup; that is not a guard failure. The missing file is only
reported after selecting the placement, not on world join or in the Placed
list (L-237). Per-section ghost reuse (L-236) and Check reuse after a cut move
are not checked.

## L-234 migration cell check on real worlds (2026-10-21)

Branch `schematic-layers`, `LamiumTests` with the migration harness
(`LAMIUM_MIGRATE_PLACEMENTS` / `LAMIUM_MIGRATE_SCHEMATICS`), default release
build, Windows x64. Three local worlds (8 placements: 6 old six-direction,
2 structure-space). For every cell the old-form view and the new
structure-local view were compared, and each new view was dual-written and
read back. Result: 0 mismatched cells, 0 bad dual writes, 0 missing
structures. This is a data/cell comparison, not in-game rendering, Check or
Materials. The one-placement and three-placement worlds were old-form only;
the four-placement world mixed two old and two new placements.

## L-231 move-target controls checked (2026-10-21)

Build `89c7925` (branch `schematic-controls`), deployed DLL SHA-256
`20580b8179fce9e24984a4c4969f24872196075f732bd36067154ea562a49073`,
default release build (no trace/probe options), local `1.26.51.01 LIP`
deployment, Windows x64. Maintainer report: the L-231 checklist passed.
Rounds: `25cd13fd68c4e2a79c8cbbf7641573d3420fdfa9907742bbee5a8a3febc5ed3e`
passed the first 1-10; `b3626aa2a27bfa79d4e7b5efc9d0dbe961ee42fe5befe3335ad3a387225af834`
passed the overlap, camera-feet, repeat-option and small-menu checks;
`20580b8179fce9e24984a4c4969f24872196075f732bd36067154ea562a49073`
passed the settings parent/child layout and the regression items. Seen:
valid-target cycling with a no-target state and fallback; direction keys,
Alt+Wheel and move-to-feet on the shared target; looked-at selection of
corner cells in the air, behind glass/plants and overlapping an opaque block,
then placements and the save area; FreeCamera picking, corner setting and
virtual feet; menu-only repeat with the "Repeat every menu step" option; the
small menu default; labels and settings hierarchy. Not seen/not implemented:
the target HUD line or world frame emphasis; repeat clearing on a
same-dimension world change; L-234.

## Open World map world check passed (2026-10-20)

Build `de8335f`, deployed DLL SHA-256
`62c1876881782e7de848b73e81af005fe0c80890ea738e6319c104e8e7e5ffac`,
default release build (no trace/probe options), local `1.26.51.01 LIP`
deployment, Windows x64. Maintainer report: from the title-screen settings,
the World map row's Open button shows `worldOnly` and does not open the map;
in a world it opens the settings map view, matching the sidebar. This is a
maintainer report, not an agent observation. The branch is committed locally
before this entry; the push follows it.

## L-229 second fix checked (2026-10-20)

Build `fee90cf`, deployed DLL SHA-256
`e09ef07ed07208180dec1c2e725769ce3ff4fd84cb857cd69810a3c7749bd8bc`,
default release build (no trace/probe options), local `1.26.51.01 LIP`
deployment, Windows x64. Maintainer report: holding the schematic menu
opener (R) leaves the radial menu open, and release then a fresh press closes
it. Esc, right-click/back and the menu actions are unchanged.

The directional key help text added after this build (DLL SHA-256
`e4b2c506c4fc3501dcdaea2dba33540c80ff55c5df4db3d0dfd1b2345cf8e609`) has not
been looked at in game. This is a maintainer report, not an agent observation.
The branch is committed locally and not pushed.

## L-229 correction and all-six movement check (2026-10-20)

Correction to the entries below: the schematic menu opener check was reported
as passed but was not. Holding R still closed the radial menu on `7f47f9a`
and on `dcab6ce`. Cause: the first fix dropped key repeats only after the
settings screen had seen the key, but the opening press happens before the
screen listens, so the first repeat still looked fresh and closed the menu.
A second fix (DLL SHA-256
`e09ef07ed07208180dec1c2e725769ce3ff4fd84cb857cd69810a3c7749bd8bc`) seeds
the opener's held inputs when the menu opens; it is not yet checked in game.

On `dcab6ce` (DLL SHA-256
`03aa48491a5d53cde4aca536a525985c791444b9bf5b1e04e148c6312f98dd00`) the
maintainer confirmed the all-six movement checklist passed with no problems:
pitch just under and over 60 degrees up and down, up/down taking over the
horizontal direction, 45-degree yaw, the four move targets, rotated and
mirrored placements, Freelook and FreeCamera, layer keys and bound wheel
chords.

This is a maintainer report, not an agent observation. The branch is committed
locally and not pushed.

## Key binding tooltip checked (2026-10-20)

Build `dcab6ce`, deployed DLL SHA-256
`03aa48491a5d53cde4aca536a525985c791444b9bf5b1e04e148c6312f98dd00`,
default release build (no trace/probe options), local `1.26.51.01 LIP`
deployment, Windows x64. Maintainer report:

- a clipped key cell shows the whole chord on hover; short bindings show no
  tip;
- a conflict tip with four caps and "Overlaps: 3" stays readable, with the
  count beside the caps instead of squeezing them;
- the padding above and below is equal;
- a deliberately long chord and a small window did not make the caps wrap,
  so the wrap path remains unobserved in game (it stays as a tested
  fallback).

The all-six view frame (L-230) is not covered by this check and remains
unchecked in game. This is a maintainer report, not an agent observation. The
branch is committed locally and not pushed.

## Schematic movement, menu opener and key display checked (2026-10-20)

Build `7f47f9a`, deployed DLL SHA-256
`b4d761a644c01f5404fa762df1a0ac359ce949951d57fcc04d7f030a5fe3571f`,
default release build (no trace/probe options), local `1.26.51.01 LIP`
deployment, Windows x64. Maintainer report from the checklist:

- holding the schematic menu opener (R) leaves the radial menu open; a fresh
  press after release closes it;
- the horizontal forward/back and left/right wheel steppers move one block per
  notch;
- looking up or down past 60 degrees moves forward/back on Y;
- at 45-degree yaw the next notch switches the horizontal axis;
- looking straight down, the forward key still follows the yaw;
- the repeat key says "Use an adjustment in the schematic menu first" before
  a menu stepper was used;
- the key column shows ALT/CTRL and hover reveals the full binding, but the
  tip also appeared for short bindings and had more padding below than above.

After this report the movement rule was revised so up/down also follow the
view (all six directions from one camera frame), and the binding tip now
appears only for clipped rows with equal padding. Neither revision has been
checked in game. Corners/area, rotated or mirrored placements, Freelook,
FreeCamera, servers and other dimensions were not tested.

This is a maintainer report, not an agent observation. The branch is committed
locally and not pushed.

## L-228 height follow fix verified (2026-10-20)

Build `07f0dc1`, docs `15c8453`, deployed DLL SHA-256
`e1ec445a248e5b6314525feb8c5044f37b1d572ebfdd71588501372be5fbd44c`,
default release build (no trace/probe options). Local `1.26.51.01 LIP`
deployment, Windows x64; the maintainer played the world and reported no
problems with the height-follow fix or the earlier band/body fixes:

- repeated `+` at Y64/Y80 no longer returns to the player's height;
- the Y63 to Y64 opening band and body/camera cave reference checks pass;
- the green camera arrow, matched-height resume and panel layout still work.

This is a maintainer report, not an agent observation. Measured frame times,
long sessions, servers and the remaining per-band/terrain coverage are still
unchecked. The branch is merged locally and not pushed.

## L-228 height follow snap report (2026-10-20)

Build `a5c8db3` (DLL SHA-256
`4ce71d89d38f16c0d7f9a9a5268ff89c8542e128839671952a7ea4719241ba1d`),
default release build. The maintainer reported the band/body checklist items
1-3 passed, then found that while the map was open at Y64 or Y80, pressing
`+` sometimes did not climb and returned to the player's height. Observed
patterns: `64 -> 64 -> 64` and `78 -> 79 -> 80 -> 78`. The cause was the
render loop following the raw eye height while the height controls used the
stable cave band: a `+` that landed on the stable band enabled following and
the next frame snapped back.

Fix `07f0dc1` makes the render loop follow the same stable reference as the
height controls; the pure `caveReferenceHeight` boundary cases are covered by
`LamiumTests`. Built as DLL SHA-256
`e1ec445a248e5b6314525feb8c5044f37b1d572ebfdd71588501372be5fbd44c`, not
checked in game. No trace/probe options.

## L-228 per-band footprint explanation (2026-10-20)

No new build or runtime measurement. The maintainer explained the earlier
Y32/Y67 footprint difference: the higher band had been recorded with a
larger render distance and the lower trip with a smaller one. Each 16-block
band records only the chunks received at that time, so the same X/Z can have
a different recorded footprint at another Y. The 500-block scale is zoom,
not coverage. This is expected under per-band recording, not a code defect.
Measured frame times and broader server coverage remain unchecked.

## L-228 final checklist (2026-10-20)

Build `7e0e2f9`, docs `3a1c855`, deployed DLL SHA-256
`0cd4eed1852f17ce937c192bfd13a4db1b4d0151eeba3dd2eef4b40ccc2b413e`,
default release build (no trace/probe options). Local `1.26.51.01 LIP`
deployment, Windows x64; the maintainer played the world and reported the
final checklist passed:

- the FreeCamera marker is the same arrow shape in green, readable against
  the white player arrow, with its heading visible;
- the cave panel has no `Return to camera` button or click action; closing
  and reopening the map returns to the camera;
- after changing the height and using `+/-` back to the camera height, the
  panel returns to `Matching camera Y62` and the match action is disabled;
- the panel and top bar keep their layout at UI Profile 50/75/100.

This is a maintainer report, not an agent observation. Measured frame times,
long sessions, servers, the smaller-footprint cause and the remaining
per-band/terrain coverage are still unchecked. The branch is not merged or
pushed.

## L-216 maintainer smoke check; cave/Nether design feedback (2026-10-19)

Build `6ff0610`, deployed DLL SHA-256
`1fc5f25bd63af06526e0eba97c9405412368e71dcb3abf5b8799fd9119102f7a`,
no trace/probe options. Same local deployment as the filesystem check below:
Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64, LamiumTest.
The maintainer reported no abnormality in appearance, opening/zoom speed,
Nether/cave display or saved-map retention after rejoining, and no
conspicuous problem with the L-216 PR overall. This is a local smoke
result, not measured frame times, a server regression or injected failure
coverage. Older-build merges, future/damaged files and forced save failures
remain separately unchecked in game.

The same report questioned the cave/Nether terrain representation. Around
Y67 (screenshot band Y64-79), gray terrain and dark areas dominate and
expected lava lakes below are hard to see; around Y32 (band Y32-47), the
visible footprint is smaller and mostly lava. Both screenshots show a
500-block scale and "Loading 4". No cause for the footprint difference
was established. This does not identify a new LMR2 regression; L-228
records the open Design question. The usefulness and continuity of the
cave/Nether view are not declared fully validated by this smoke result.
Screenshot evidence is retained locally with the validation material;
no replacement drawing behavior was selected or implemented.

## L-216 LMR2 live migration: filesystem and log checks (2026-10-19)

Build `6ff0610`, DLL SHA-256
`1fc5f25bd63af06526e0eba97c9405412368e71dcb3abf5b8799fd9119102f7a`
(build and deployed copies matched; no trace/probe options). Local
LamiumTest world, Windows x64, Minecraft 1.26.51.01 (log: 1.26.51+0559ac5),
LeviLamina Client 26.51.7. View distance was 50 chunks at join and changed
to 96 during the run. The maintainer loaded and played the world; the agent
checked files and logs outside the game without changing the live map.

Before joining, a read-only snapshot captured all 1,563 Overworld `.lmr`
files: 209,462,740 bytes (199.8 MiB), no `.lm2`, no skipped files. The store
attached at 11:47:16 JST. By the 11:50:47 poll (about 3.5 minutes later),
no `.lmr` remained and 1,601 `.lm2` files occupied 35,038,581 bytes
(33.4 MiB, 83.3% smaller), including 38 newly recorded regions. The final
11:52:41 inventory had 1,611 `.lm2` files, 35,247,577 bytes (33.6 MiB),
and still no `.lmr`.

An independent Python reader checked 1,603 `.lm2` files enumerated at the
start of the final audit: container/table sizes, coordinates, stored CRCs,
single dictionary-free zstd frames, palette indices/alpha, height planes,
CHNK and OLDF. All 1,563 baseline regions were compared to the snapshot:
their absorbed legacy size/time/CRC matched; 1,492 retained exactly the
recorded colors/heights, and 71 had differences only in chunks carrying a
new recording time (773,370 differing columns). No recorded column was
lost, no baseline region was missing, and there were no unexpected
differences, decode errors or skipped files. Regions unchanged with zero
chunk times establish conversion beyond rescanned terrain. Log snapshots
contained no warning/error lines or map refusal, damage, save/conversion
failure or recording-stop messages. Private comparison archives, reports
and log snapshots are retained locally under gitignored `build/validation`.

This verifies file-side live migration, preservation of existing recorded
columns and disk reduction in this world. It does not independently verify
the colors produced by rescanning. The maintainer has not yet reported
appearance, opening/zoom speed, frame times or persistence after rejoin.
Nether/cave layers, other worlds/servers, an older release writing again,
future/damaged files and forced save failures were not exercised in this
runtime check; their automated coverage is separate.

## L-225 and L-226 map rescans (2026-10-19)

Same environment as L-222. `9a565f0` (DLL
`13ec74f778dbdb17cf0849046190421cf89866cbd3212a6fbc7dc2841c56b591`): with the
world map recording and FreeCamera flown more than 60 chunks from the
player (checklist item 1), the minimap around the camera no longer lost pieces; slow chunks at
the edge filled in instead of staying holes (item 2). Maintainer: both pass. Seen in
the same session: past the render distance FreeCamera shows only the sky
(L-227, separate).

## L-224 target card at ghost edges (2026-10-19)

Same environment as L-222. `507c605` (DLL
`6f31895e105bee9094bbc875688860fda55f406bc91536e6f5a100c2a725fe32`): stacked
ghost and block fixed; with ghosts set diagonally (touching at an edge) to
a real block or another ghost, slowly turning showed what was behind for a
moment, flipped between the two ghosts several times, and ghost edges
flickered (screenshots). `30b66ce` (DLL
`050a0dd5094de4cdb6c0d7c4e0c0b475c7bc86661fdebf6e148c97b5671e9ec9`): one
change between diagonal ghosts, nothing from behind, no flicker at edges.
Maintainer: OK. Not checked: a ghost in front of an entity.

## L-223 position labels (2026-10-19)

`cdc02c5` (DLL `b6aea5601ae2df9363d83602e69d541ab04007be6eda788eb980ca2b4572a9a1`),
same environment as L-222, English: Info HUD "Exact position", "Position",
"Target block"; card row "Position"; Debug View still "XYZ" and "Block".
Maintainer: OK. Same session: dragging across a ghost's edge sometimes
showed what was behind the ghost (L-224).

## L-222 target card ghosts at a block's edge (2026-10-19)

Builds from `l222-ghost-target-seam`, LIP instance, Minecraft 1.26.51.01,
LeviLamina Client 26.51.7, Windows x64, local world, maintainer.
`b66feb0` (DLL `73a2dd39a3fb90ef7962fee1e2b0226c67f62fe35925454d9efec9471cfe752d`):
a black wool ghost at 527, 63, -460 under a real block and in front of
real stone; aimed just under the seam the card named the stone at 528
(screenshots), elsewhere the ghost. `41ee214` (DLL
`90b74e1710b64ded067a49ced408703855ebc87f42f26c938a44750ed8f9fed9`): OK.
Same session: the block card in Freelook and FreeCamera OK; the HUD in
English and Simplified Chinese read without problems apart from the mixed
English position names (L-223).

## L-219 Info HUD coordinate lines and order, L-221 HUD layout lines list (2026-10-18)

Builds from `l217-info-hud-fixes`, LIP instance, Minecraft 1.26.51.01,
LeviLamina Client 26.51.7, Windows x64, local world, maintainer.
`cdd2bcd` (DLL `6e1e811a123994b2128fe1c9ef98550a1065ea72be2c86197c0de639e065ad93`):
"Block coordinates" and "Target block" lines show the expected values
(screenshot: target block 547, 48, -463 matching the card); the new line sat
at the end of the saved order. `7856e1e` (DLL
`8608677db8592fdb2e4ebd55d4835b8238fdb172f1b8eea9d3759804c21c0ccb`): the
settings rows list the grouped order; the chat sample no longer shows
through the editor's lines list. `5a99b86` (DLL
`b5152eb469ecff33698966b596cd8c090883bc1b1a3346cebdb15e679028fee8`): the
lines list stays clear of Reset/Done, scrolls by wheel, and the Info HUD's
"Reset this element" restores the line order; dragging the bar did not
move it. `b8b065a` (DLL
`253d438704b74ac0152ded855bd18edcbf07b6fb8d261330a0a70d6551611f99`): the bar
follows a drag. Not checked: Simplified Chinese text, Target block in
FreeCamera.

## L-217 target card block, L-218 Info HUD help, L-220 calculator row (2026-10-18)

Builds from `l217-info-hud-fixes`, LIP instance, Minecraft 1.26.51.01,
LeviLamina Client 26.51.7, Windows x64, local world, maintainer.
`1b578e4` (DLL `e7861a9bfdf6a244757195f6c891e228366d21d3ba3bd35d15fe22975c5d81b6`):
Info HUD rows show their own descriptions (OK); the calculator is gone from
the sidebar and opens from its row (OK, but wanted under General); the
target card still named the upper of two stacked stone blocks when aiming
just under the seam while the game outlined the lower one (screenshots:
528, 64, -460 against 528, 63, -460 a pitch degree apart).
`c4884e5` (DLL `9769c5339217837e29067c0b301bb2b4ceb15cacd491b9acf2d7477b7291f503`):
the card follows the outlined block at the seam, and the calculator row
is under General: both OK. Not checked: the card in Freelook and
FreeCamera, Simplified Chinese text.

## L-215 world map images at every scale while moving (2026-10-18)

Builds from `l215-map-coarse-refresh`, the last `f281fa1` (main plus the
branch; DLL `d3d76e69e2212833ae843f4c5c13a8f31a0eb9b767b88064e3dd840a7f69e974`),
LIP instance, Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64,
local world, maintainer. Earlier builds of the branch still showed recorded
areas black at coarse scales (never-built images waiting, quarters cleared
or kept stale). With `f281fa1`, flying with the map open and zooming in and
out showed no unnatural gaps; only places the client never loaded stayed
dark. The "World map image" trace logged no line in that session.

## L-214 FreeCamera underground view hold and its setting (2026-10-18)

Builds from `l214-spectator-hold`: `d703378` (3 s hold, DLL
`7a7af228671c12f61bcf770059b876e51694a46c355accbf7eb740dbdcdd94cc`) and
`f8bcd8d` (the "Keep underground view" setting, DLL
`f08a1f4e39dfa37f4d933ce2641f01b94db7c9680e894336d5d2cd463ddcb62d`), LIP
instance, Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64, local
world, maintainer. The 3 s hold stopped the rebuild at every crossing of a
small cave or the ground; the setting appears under FreeCamera with its help
and its choices behave as described (0 s, timed, Always). Not measured:
frame rates per choice.

## L-213 FreeCamera culler check tolerates switching (2026-10-18)

Combined test build `6c2fde2` (main with PR #20 and #21, plus
`l213-culler-check`; DLL
`296cdca7f9a270c03ebdb96fcebb0ac8a7a6118b186ee5b41277bc499d167dc3`), LIP
instance, Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64, local
world, maintainer. Flying the FreeCamera in and out of terrain for a while:
the underground view kept working and the setting was not paused (before:
paused 19 s after binding). Noted: entering even a small cave rebuilds the
view when the culler switches (L-207 behavior), which may not suit everyone
(L-214).

## L-211 finer map images stand in after zooming out (2026-10-18)

Combined test builds with `l211-map-zoom-out-stand-in` (`6c720f3`) merged
(`d524a54`, DLL `6ba62deccf5b5bf6f7fd0c3f4703a7a07002a38a1d5903533aa5caa6d11bea50`,
and `38d7d5a`), LIP instance, Minecraft 1.26.51.01, LeviLamina Client
26.51.7, Windows x64, local world, maintainer. Zooming the world map out over
explored areas showed the drawn map instead of dark areas, now and then
with a short delay. Not checked: memory or upload counts against a build
without the change.
## L-210 world-map recording scan grid (2026-10-18)

Builds from `l210-map-scan-grid` merged into local test branches (the last
one `38d7d5a`, main plus PR #20 and #21; DLL
`32daef21a8789cac3fa1fb2f19f2678aa8e1995066aa5e928cd9bd9f365d82ae`), LIP
instance, Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64,
local world, render distance 96 chunks, maintainer. Runs, each after a
teleport to new terrain: (1) no arcs or wedges, but the map stopped about
33-40 chunks out while standing (edge retries starved the scan); (2) near
partial chunks starved it the same way; (3) range reached in about a minute
after picking was bounded, filled slowly; (4) with L-212 the area around the
view fills, but as scattered specks; (5) with probed retries the log shows
about 90,000 checks in the first 30 s (0.015 ms each), 0 unseen, the rest
waiting for the client; the map fills from where the player looks. Left
dark: one-chunk lines of chunks the client never receives, also holes in
the world itself with Lamium disabled (not in the latest store release).
Carpets, snow layers and rails draw on the map (also checked in this
build). Not measured: frame time against a build without the recording.

## L-212 distance fog next to the render distance (2026-10-18)

Combined test build `9afe8d4` (main plus `l212-distance-fog-range` and PR
#20; DLL `b9867739437a27a02893563f87d668826c323f3e46ca5b50d03f77a1fab57426`),
LIP instance, Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64,
local world, render distance 96 chunks. Before the fix, re-entering the same
place three ways: A (Lamium, distance fog hidden) built only exactly where
the player looked; B (Lamium, distance fog shown) built the area around the
view; C (Lamium disabled) the same as B. With the fix and distance fog
hidden, the area around the view fills as in B; the fog stays gone on land,
at night and in rain; the same with the minimap and world map on. Small holes
that stay even when looked at appeared in A, B and C at the same places, and
not in the latest store release without LeviLamina (maintainer).

## L-205 carpets on the world map (2026-10-18)

Build from `l205-carpet-recording` (`82a90ae`, DLL
`f0fe542f517095b79510982389336ad12cd34d8c2e146b1aa98e57a272f82339`), local
single player (LamiumTest: a 16-color carpet row and wool stripes on a stone
platform at y=100), Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows
x64. Run by an agent driving the game, not the maintainer. A temporary
diagnostic logged for a carpet: graphics shape (0.40,0.40)-(0.60,0.60),
type shape (0.00,0.00)-(1.00,1.00), cover true. After the fix the saved
regions hold the carpet columns at y=101 with their colors where they held
the stone (y=100) before. The same change is in the combined test builds the
maintainer ran on 2026-10-18 (`84052d8`); carpets there were not separately
reported. Not checked: other thin blocks (rails, redstone dust, trapdoors).

## L-208 Connected Textures corner edges (2026-10-18)

Combined test build `84052d8` (main plus PR #19, #20 and #18; DLL
`6d71574e2ed6fb9a0afd67049a773d6f34f1b68c2756aba0703abf3be6a96173`; the
glass code is the same since `43407ec8`), local single player, Minecraft
1.26.51.01, LeviLamina Client 26.51.7, Windows x64. The maintainer checked:
the flat-L notch shows the one corner texel without sticking out; with
"Edges at inside corners" on, glass-block walls keep a vertical line on both
sides of an inside corner, and off they join as before; with "Edges at glass
pane corners" on, an L-shaped pane gets an outside edge without a stretched
post. The two options show their own help text. Not checked: 64 px resource
packs, three- and four-side notches in game, frame cost of the extra sprites.

## 0.2.3 release build check (2026-10-15)

Maintainer checked the release build from `b80fa83` (version 0.2.3), DLL
SHA-256 `DD3A7B861D8FF5566524B37AE3497B7A841D461A400718A3597BD5760392EAFD`,
the same DLL as in `Lamium-0.2.3-client-windows-x64.zip`. Minecraft
1.26.51.01, LeviLamina Client 26.51.7, Windows x64, local world. The title
screen showed Lamium 0.2.3, `L` opened the settings, and the world map's
layer switches and the new Item Transfer gestures in a chest worked. No
issue reported. Not a full regression.

## World map: compact layer switches and L-204 side panel lists (2026-10-15)

Build `34d71ca`, normal build (DLL
`1FACED1BF8C5D7BF481E115DF1D313F7760A83451E59110F1F3202166A9A674E`), local
single player, Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64.
The maintainer passed the checklist: at UI Profile 100% in the Nether the
top bar stays one row with symbol-only switches named in the bottom bar on
hover; the panel header menu lists the three layers with counts;
placement and shape rows select, center and show/hide; the chosen list
survives reopening; a layer switched off on the map keeps its list with the
"Hidden on the map" line. That line touched the first row's highlight; it
got its own line in the next commit. Servers were not checked.
Follow-up `5ddab02` (DLL
`6D231C13E05035425C2E24B91EE02176FC1D2CED2F731C10FCFBCC5169DC4362`): the
note on its own line looked right to the maintainer.

## L-169 held stack and drop gestures (2026-10-15)

Build `9da5d9d`, normal build, traces off (DLL
`D8D799B0B2E650F85A39DA44E8E98DA909F6D5DA564CE6965A53C8A1AFE2D627`), local
single player, Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64.
The maintainer passed the checklist in a chest: Shift + click on an empty
slot with items held placed them and pulled the matching items from the
inventory; drop key + Ctrl + left / Shift + right / Shift + left drags
dropped one, all but one and the stack; Shift + click outside the window
dropped the held stack and the matching items, hotbar included; each of the
five new switches turned its gesture back to vanilla. Not checked: other
screens, the inventory screen, servers, failure cases.

## L-146 layer switches and the L-169 drop trace (2026-10-15)

Build `0ba6e1d` with `--transfer_trace=y` (DLL
`D153830793A4F479FFDA1E19AE2A72DA90006F79F6F54C1D3B058C97FA155D5C`), local
single player, Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64.
- L-146: the maintainer passed the checklist: three switches with symbols
  left of "Center on me", the side panel button named "List"; a switch off
  removes its marks from the world map (death point with waypoints); the
  choices stay after reopening the map. Servers and narrow two-row layouts
  were not checked.
- L-169 vanilla observations (Item Transfer off), from the log: the drop key
  over a slot calls `_handleDropItem` with ItemTransferAmount bytes
  `01000000 02000000 00ffffff` and drops one item; Ctrl + drop key
  `01000000 00000000 00ffffff` drops the stack. A left click outside the
  window calls `_handleDropSelectedItem` with the stack bytes, a right click
  with the one-item bytes (the cursor's items). Both reach
  `ContainerManagerController::handleDrop`, one request each, accepted.
  Shift + left click on an empty slot with items held called auto-place
  with amount 2147483647 and moved nothing; the cursor stayed held.

## Settings screen follow-ups after the L-197 review (2026-10-15)

Maintainer passed each supplied check on these builds, same local
environment (Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64,
ordinary release build, traces off); deployed DLLs matched the builds.
- `59e7252` (DLL `435E4F2A921C2E21AB3FB7986FA6A212D1F927357214CCFBD2ABCDBD24E099F0`):
  a lone left/right click bound to the world map no longer closes it; a side
  button still does. At a tiny window the warning stayed white only before
  expanding, and the expanded footer did not scroll with the wheel (both
  fixed next).
- `d0924e5` (DLL `BAB997A344DA56CED0C20DD73EFCF4969CF8459D7177B2158F607AC80EDB71D9`):
  warning color in the one-line footer, wheel scrolling over the footer,
  numeric rows with help and range in white and controls in the faint hint
  line, and F11 toggling fullscreen while the settings screen is open.
- `21220bd` (DLL `2B8E29E285F1E8C60ABBADE22BA8D18E6C659F5E22F89C676FAF6FD22E6AAC71`):
  a maximized 1080p window and a very small one both draw the sidebar
  layout shrunk instead of tabs; clicks, wheel and search typing follow the
  drawn positions; list and footer scrollbars drag; Schematics preview and
  item icons stay inside their frames when shrunk.
- `bc078fa` (DLL `6D6D002F753CA94F507987D254A10B69136BAD88C58571E621F9D87FB5C2A9BB`):
  no Read full description button; two description lines; at a tiny window
  an overflowing description shows a scrollbar and the wheel hint, scrolls
  without moving the list, and restarts at the top on another row.
Not seen: English and Simplified Chinese layouts, docked tool views while
shrunk, HUD editor/world map unaffected by the shrink, IME caret position
in shrunk text fields.

## In-place description expansion (2026-10-15, L-197)

Maintainer checked the in-place footer expansion on `b2885fb` and reported
it mostly fine, with two issues: the operation hint, the button and their
bottom margin shifted when expanding, and the expanded state persisted after
the selection moved to another row. `c4b8f01` anchors the hint and button at
the same bottom position and collapses/rewinds the description when the
selected row changes; the wheel-scroll hint returns only when the expanded
text overflows. The maintainer passed the supplied three-item check on
`c4b8f01`, DLL SHA-256
`4A49927A467C94C08AEBD15155718B6FA0599D296F72E19D016BE26B24F575EC`.
Environment: Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64,
local world, ordinary release build with traces off. Deployed DLL matched.
- Expanding keeps the operation hint and the Collapse button at the same height.
- Selecting another row while expanded returns to the collapsed footer and
  "Read full description".
- Overflowing expanded text shows the wheel hint and scrolls with the wheel.
Not seen in this check: every window size/GUI scale and language combination,
numeric-row help, and editing/capture/error states while expanded.

## Help presentation and chat-return follow-up (2026-10-15, L-197/L-203)

Maintainer passed both supplied checks on `f8c714b`, DLL SHA-256
`9AB0A06CA4EDA61477092A03F339C9884B7E2928C5CC06B7CDDEA419ADE00636`.
Same configured local environment: Minecraft 1.26.51.01, LeviLamina Client
26.51.7, Windows x64, ordinary release build with traces/probes and forced-
unverified mode off. Deployed DLL/PDB/manifest matched the build.
- The compact translucent full-description panel's appearance, size,
  scrolling and return interaction passed.
- The chat-return check passed: repeated send/close and Esc-close, short/
  wrapped messages and OFF restoration were supplied in that check. No
  vanilla-position jump was reported. This closes L-203 in the local scope.

The maintainer then replaced L-197's interaction contract: keep the existing
settings screen visible, grow its own description footer to the measured
text, and open/collapse it using the button only. Remove the separate panel
and F1 shortcut. That subsequent implementation has no runtime result yet.
Server and previously pending forced-unverified/failure checks remain open.

## Post-0.2.2 normal-build local smoke (2026-10-15, L-197 through L-203)

Maintainer checked items 1-14 from the deployment checklist on `d5978e2`,
DLL SHA-256 `5ABB2DCC6B3EA3EB6AB8AE2D689E323D22BFECC4D96BE5DCF7B3EA88CBF71B2E`.
Environment: Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64,
local world, ordinary release build with traces/probes and the forced-
unverified option off. The deployed DLL/PDB/manifest matched the build.

The maintainer performed the checklist except server checks and reported
it was mostly fine, with two exceptions:
- L-197: full descriptions open/read/scroll/return, but the help panel is
  opaque instead of matching Lamium's translucent surfaces and takes up
  nearly the whole settings area. Presentation needs a follow-up.
- L-203: returning from chat input can briefly draw HUD messages at the
  vanilla upper-left position before the chosen chat placement returns.

The checklist covered the three Fake Offhand regressions (L-198),
FreeCamera waypoint/death-point distance and range/restoration (L-199),
underground terrain and restoration (L-200), M/Mouse 5/chord map closing
and editing (L-201), full-description interaction, load/settings and quiet
waiting state, FreeCamera hand/hotbar/HUD hiding, visual adapters and
resource reload, Edge Guard/trading/transfer, schematic world/3D preview,
in-world language, feature toggles and world/dimension re-entry. This is a
local smoke report, not an itemized exhaustive regression of every adapter.
The ordinary log identifies loader 26.51.7 and records the terrain culler
binding and native `3 -> 5` retention during this run.

Server behavior/server-pack skipping, forced-unverified fallback and
injected hook/cleanup failures were not established by this run. No result
for the subsequent help presentation/chat-return fixes is claimed here.

## Maintainer regression reports (2026-10-15, L-197 through L-201)

Reported build/commit and DLL hash were not supplied. The local ordinary
log identifies the configured Minecraft 1.26.51.01 instance and LeviLamina
26.51.7; only the loader rejection below is tied to that log, not every
reported reproduction. The maintainer reported:
- Fake Offhand: primary blocks prevent fireworks while gliding and aiming
  into air; with blocks in both slots secondary placement takes priority;
  secondary blocks prevent eating primary food (L-198).
- FreeCamera movement does not change waypoint distance (L-199), and
  underground visibility has regressed (L-200). The log says
  `FreeCamera terrain: vanilla visibility retained (unsupported loader version)`
  after detecting 26.51.7; the code admitted only .5/.6.
- Settings descriptions are cut off (L-197).
- A world-map M binding opens/closes; Mouse 5 only opens (L-201).

These reports reopen coverage for those exact cases. Fixes and pure tests
are not a successful runtime recheck; the new build still awaits Minecraft
validation. Server environment and other cases were not reported.

## Placed list headers and 0.2.2 release build (2026-10-15)

Maintainer report, local: the release build `6ae5977` (DLL `8d3d2fad...a6ac4`)
starts and shows "Lamium 0.2.2"; on `bd28452` (DLL `71490ffc...5b7d`) the
Schematics > Placed headers "Done" and "Shown" are apart and above their
values and switches, also docked right and in Japanese (L-195).

## Hotbar section and BDS pass for Item Transfer (2026-10-15, L-169)

Maintainer report on `37c3745` (DLL `5786921f...ebfac`), traces off:
- Local: "everything" over a hotbar slot moves the hotbar alone (chest and
  Alt + Shift click), over the main inventory leaves the hotbar, and in the
  inventory screen moves the hotbar into the main inventory.
- BDS: the `7586995` checklist (everything both ways, matching coal by wheel
  and Alt click, hotbar kept, Shift + right drag leaving one, furnace and
  brewing stand routing) and the hotbar checklist above: no problems seen.
Not itemized: rejections/timeouts, latency, other screen families.

## Batched Item Transfer pass (2026-10-15, L-169)

Maintainer report on `7586995` (DLL `d93cfd79...a376`), local, Quick
Container off: everything both ways in a large chest, matching coal moves
(wheel and Alt click), hotbar kept, Shift + right drag leaving one, furnace
and brewing stand routing all behaved as expected. Not checked: a server.
The maintainer then asked for the same "everything" move on the hotbar.

## Bulk moves of a resource pack traced (2026-10-15, L-169)

Trace build `917496b` (`transfer_trace=y`), DLL SHA-256
`10e9df62f95d1df72201d223bf79b8f3c3af8b49ac8daebb37accbdadbda31f6`, local
world, Item Transfer off, the Quick Container resource pack on (observed only,
PROVENANCE). Maintainer's sequence in a large chest: one Shift + click, chest
to inventory everything, inventory to chest everything, hotbar to chest
everything, coal: chest to inventory and back, then the held coal put down.
- Every pack operation was vanilla `_handleAutoPlace(INT_MAX, collection,
  slot)`, called once per source slot (high slots first) within one click,
  2-35 ms in total. No take-all, place-all or cursor handler ran for them.
- No call waited for an answer: the unsent batch grew to 44 requests during
  one click, went out together, and one response packet answered all of them
  about 30-60 ms later (counts 44, 43, 9, 31, 13, 18). Every response was a
  success. The matching moves called only the slots holding coal.
- The ordinary Shift + click was the same handler for one slot (1 request).
So Bedrock accepts many auto-place requests in one frame; Lamium's slowness
comes from its own one-request-then-confirm pacing (DESIGN "Inventory
transfer"), not from the game. Not checked: a server, rejections, furnaces.

## L-169 normal-build smoke checklist passes locally (2026-10-15)

Maintainer report: the requested checks are confirmed, all behaved as
expected, on implementation commit
`c8c1f1bc0ccfcc4a57340c16d0c3e8dbb797269c`. Built and installed DLL SHA-256
both `2A6B0A52B397511108815C2BE4315EFBA84403343AAA242B632D2B9FEF6BE515`.
Windows x64 release client, all traces/probes off; manifest still 0.2.1,
no release tag. The log shows a local world and LeviLamina 26.51.7 against
the verified Minecraft 1.26.51.01 target.

The requested smoke checklist covered Ctrl + wheel matching moves,
Ctrl + Shift + wheel everything with the player's hotbar retained,
Alt + left / Alt + Shift + left moves from the clicked side, Shift + right
drag leaving one, and individual switches passing through to vanilla.
Chest and furnace/brewing/beacon checks were requested. The maintainer's
blanket confirmation does not itemize each gesture/slot/screen combination,
game mode or packs; it does not establish the full screen matrix, servers,
controlled rejection/timeouts or unverified-game fallback.

The captured normal log contains source-count-change stops in storage,
player, furnace and brewing collections. Triggers were not specified;
no controlled interruption result is inferred. No route-hook failure,
request rejection or slot confirmation mismatch appears in that log.
The implementation build, LamiumTests and game dependency inventory
(60 files / 286 hooks) passed. L-169's 0.2.2 development part is complete;
the remaining coverage is in ITEM-TRANSFER, VALIDATION and BACKLOG's
Pre-release checks. Later Design scope remains in L-169.

## L-169 native-handler checkpoint passes locally (2026-10-15)

Maintainer report on e3f9276, `transfer_trace=y`, deployed DLL SHA-256
`850CC7B7A8A053F24BE8D6C04A8B0BAB3497623765727A14C322EF54FEF3F103`:
the requested chest and furnace/brewing/beacon checks are finished and
behavior remains as before. The diagnostic log shows a local-world load
and LeviLamina 26.51.7; game target is the verified 1.26.51.01 executable.
Game mode and pack coverage were not stated. Item Transfer was to be off
for observing vanilla; the logged tested clicks are not cancelled.

Alt key code 18 reaches the loader, and Alt + left clicks reach take-all.
Take-all changes cursor false to true with 64 oak stairs; place-all returns
them and clears the cursor. These are cursor operations, not side sweeps.
Shift transfer reaches auto-place with the whole-stack sentinel. Inventory,
large chest, furnace, brewing stand and beacon report is-container true.
The native orders identify player/storage collections, furnace fuel then
ingredient, brewing fuel/ingredient/bottles, and beacon payment. Outputs
route to the player inventory; player routes also contain hotbar/main
fallbacks. An elytra was accepted into the furnace ingredient slot while
charcoal occupied fuel. Beacon payment takes one emerald; coal instead
follows a player-side fallback. See ITEM-TRANSFER.md for the selected path.

Snapshots immediately after handlers are prediction. The user confirms
unchanged visible vanilla behavior, not exact authoritative packet counts.
No new Lamium gesture, native hover-field mapping or scoped manager route
filter was tested in this build. Other screen families and servers remain
unchecked. The 0.2.2 implementation's changed-feature smoke test is next.

## L-169 vanilla slot-role observations (2026-10-15)

Maintainer follow-up: chest Alt + left click, Shift + right drag and
ordinary take/place remain as before. Vanilla Shift + left click routes
items by role in furnace, brewing stand and beacon screens. A furnace's
fuel slot also accepts an item that cannot burn; exact item/count was not
given. Brewing/beacon slots appeared to accept suitable items only; this
was not an exhaustive acceptance check. Local/server context, packs and
the exact tested build were not stated.

At follow-up, the installed DLL SHA-256 was
`3664E3EF9B025885C9F56F899CBAA8E0BFFFC6E89DB73EA54CB34CBD49C33951`,
different from e3f9276's prepared diagnostic DLL
`850CC7B7A8A053F24BE8D6C04A8B0BAB3497623765727A14C322EF54FEF3F103`.
The current log names LeviLamina 26.51.7 and has no L-169 diagnostic lines.
The observations are not attributed to the diagnostic build and do not
verify the new gestures, handler names, collection names or registered
routes. ITEM-TRANSFER.md retains the pending diagnostic checkpoint.

## L-169 vanilla input observations (2026-10-15)

Maintainer report during the 0.2.2 implementation investigation: in a
vanilla storage screen, Shift + right drag transfers the whole hovered
stack; Alt + left click selects an item like an ordinary click. Exact
screen, local/server context, game/loader versions, Lamium build and DLL
hash were not recorded for this observation. No new L-169 gesture, native
handler effect, additional screen or role-specific route was verified.
The read-only diagnostic checkpoint is in ITEM-TRANSFER.md.

## Ghost tint slider and shared outline edges pass (2026-10-15)

Maintainer report on `ee196c6` (DLL `870e781a...1c45`), local: the checklist
passed: the ghost tint is a percent slider, 40% looks like the former weak,
0% and 100% work; wrong-state stairs over a missing dirt ghost no longer
fight at the shared edge. The maintainer then chose 70% tint and Normal
outlines as the new defaults.

## Ranked outline bars, first check (2026-10-15)

Maintainer report on `976a628` (DLL `0874f6d4...053b`), local: a wrong block
beside a missing ghost no longer flickers. Wrong-state stairs above a missing
dirt ghost: the yellow and light-blue lines still fought slightly at the
shared edge. Fixed in the next build (strongest copy only), not checked yet.

## Mistake outlines missing (2026-10-15)

Maintainer report on `cdf288d` (DLL `6fab1a59...1c3d`), local: the outline
around red and yellow mistake faces is gone; when it went is not known. Cause
found in code: since `2515d62` the sections' rebuild key (graphics mode, tint,
outline width) was also compared as the graphics mode, so every mode was
treated as Vibrant Visuals and mistake marks used that path. Fixed in the next
build, not checked yet. The target card should also name the mismatch
("Schematic: Wrong block / Wrong state"); added in the same build.

## Schematic sections on the target card rejected (2026-10-15)

Maintainer feedback on the `771ba02` presentation (no new build or hash
supplied): the Stacked and Side by side sections are hard to read. Keep the
Target Info schematic setting and the ghost as a target; return to the
ordinary single-card layout and polish its details and wording instead. No
in-game result is recorded here; the replacement build awaits checks.

## Corrected schematic target presentation accepted (2026-10-15)

Maintainer accepted the corrected stacked presentation after the supplied
`0b89cd2` build, DLL SHA-256
`41c58dde3b3e237709dd57a93f89ee593a8164442bf0340734586dd14c248434`,
then requested a None / Stacked / Side by side display option for Target Info.
No individual test cases or new environment details were supplied. This is
acceptance of that presentation, not a claim of complete regression coverage.
The new display choices and their persistence/fallback still need runtime checks.

## Target-card schematic feedback and Placed headers (2026-10-15)

Maintainer feedback after the supplied `334889a` build, DLL SHA-256
`584c3dec9090401a0e406fe5654df5776e8f61d56ab0cb3146011e85e0884b6f`:
- L-184: a ghost hit before a real block paired the ghost's Missing status
  with the rear real block at another coordinate. Both could be grass blocks,
  making the display seem contradictory. The maintainer requested hiding the
  rear block or showing air at the ghost cell; the correction chooses hiding
  the rear block. Placement names are also to be removed from Target View.
- L-195: the supplied English Schematics > Placed screenshot shows Done and
  Shown headers touching/overlapping. Two placements show 0% and 1%; Lamium
  0.2.1 is visible. The maintainer explicitly leaves this outside the current
  Target View change.
World/server and graphics mode were not supplied for this report. No overall
target-card pass is claimed. The corrections and broader cases need another
in-game check; the Placed layout is only recorded for later.

## Ghost outline bars and whole-HUD hiding pass (2026-10-15)

Maintainer report on `7d79bef` (DLL `c206411d...294c`), local: normal and
thick ghost outlines look like one solid line up close; FreeCamera "Whole
HUD" also hides Lamium's HUD and it returns when FreeCamera ends.
Not checked: servers, controllers, Vibrant Visuals for the outline bars.

## Ghost target, ghost look settings, FreeCamera hiding (2026-10-15)

Maintainer report on `b5daa75` and earlier same-day builds, local:
- Ghost target card (L-184 part): aiming at a ghost cell shows "Ghost of"
  with the block; aiming at a real block shows the usual card.
- Ghost tint and outline width (L-186 part): changing either applies at
  once. Normal and thick outlines, then four offset copies of a 1 px line,
  looked like parallel thin lines up close; replaced by bars afterwards
  (not checked yet).
- FreeCamera hiding (L-191): the hand hides while flying; "Hand and hotbar"
  also hides the hotbar; "Whole HUD" left Lamium's own HUD drawn (fixed in
  `a86fa68`, not checked yet); everything returns when FreeCamera ends.
Not checked: servers, controllers, Vibrant Visuals for the new outlines.

## L-189 passes (2026-10-15)

Maintainer report on `a1a758e` (DLL `58377e90...8089`), local survival:
wheat seeds on stone and dirt use the fake offhand and plant on farmland;
a carrot at full hunger uses the fake offhand on dirt and plants on
farmland. With the earlier rounds, every listed case is as expected.
Not checked: hoes, shears, honeycomb, crossbows, Riptide tridents, primary
fireworks, servers.

## L-189 first check (2026-10-15)

Maintainer report on `9c487e5` (DLL `47142c2e...82c1`), local, survival:
passes as expected: bread at full hunger on wood and dirt; a shovel on wood
(fake offhand) and on grass (path); wheat seeds in air (fake offhand) and on
farmland (planted); bone meal on wood (fake offhand) and on grass (grass);
an axe on a log (stripped), on oxidized copper (scraped) and on plain copper
(fake offhand); a minecart on grass (fake offhand) and on a rail (placed);
a bow without arrows aimed at the sky (fake offhand); an empty bucket aimed
at the sky or grass (fake offhand).
Not as expected: bread, a shovel and bone meal aimed at leaves did
nothing (leaves are not solid, so not a plain target); wheat seeds on stone
or dirt did nothing (no planter component on wheat seeds); a bow without
arrows aimed at copper or grass did nothing (the no-arrows check was air
only); an empty bucket aimed at water used the fake offhand instead of
filling (the hit's liquid flag was unset). Fixes in the next build: plain
also covers blocks whose collision fills the cell; planters ask the planted
block's mayPlace; bows, crossbows and Riptide tridents are idle on blocks
too; a liquid is looked for along the aim.
- On `1b0cd47` (DLL `9adb2e08...40a9fc`): all as expected except wheat
  seeds on stone or dirt, which still did nothing. Next build: the planted
  block must also `canSurvive` there, a farmland / soul sand fallback when
  the planted block is unknown, and one log line per planter item
  ("Fake Offhand: planter ...") saying how it was judged.
- On `572e7e4` (DLL `dda3f55e...e785`): wheat seeds on stone or dirt still
  did nothing; seeds and carrots on farmland planted; a carrot at full
  hunger on dirt used the fake offhand. No planter log line at all: neither
  wheat seeds nor carrots report the planter property or component, so the
  planter check was never reached (seeds hit the old `_seeds` rule; a carrot
  at full hunger on farmland would have used the fake offhand). Next
  build: vanilla planters also count by name.

## L-182 reporter's doubt and the log timeline (2026-10-15)

The reporter doubted the cause: the crashes were in the world where the
schematic was first imported, the only one where schematics were used.
Their `2026-10-14.4.log` (14:49:27-15:51:56, after the 14:49 crash) shows
world joins ("World map: saving to ...") alternating between two world
folders: JDce+B0LUjQ= 14:50:05, fBx9HspzF+c= 15:08:55, JDce 15:09:31, fBx9
15:48:59, JDce 15:50:04, fBx9 15:50:51, JDce 15:51:46, then the crash at
15:53:07. Rejoining any world (the same one included) is expected to be
enough. Asked: the second world, and whether the earlier crashes followed
a rejoin. Maintainer 2026-10-15: schematic drawing work pauses (L-192,
L-193 far cases and L-194 remain open).

## Blended members fix passes; corner over glass isolated (2026-10-15)

Maintainer report on `621d879` (DLL `7a5722fa...d7a`):
- Walking around terrain_water_lamium: no water sections blink, about
  55 fps as before.
- Corner marks: over air no change with position; over water and over
  glass the look changes, across a block plane through the corner (left or
  right of it, screenshot with a sandstone strip at the boundary), also for
  a corner one block above glass wherever a glass face lies against the mark.
  Reading: the seen-sides change at that plane; over air it shows nothing,
  so the material's reaction to the real block's depth just behind is
  suspected (the Fancy face material is the hologram pointer). Asked:
  the same in Simple graphics?
- Answer: it changes in Simple graphics too, so not the material. Next
  build: the seen faces 0.02 out of the cell, and corners beside a
  see-through block drawn after the world's translucent blocks.
- On `8570986` (DLL `e58097ee...13e1`), four screenshots (Fancy and
  Simple, each side of the line): still changes over glass and water. From
  one side the real water behind the mark is hidden, from the other it shows
  through; the line lies on Chunk Borders lines. Reading: some of the game's
  translucent terrain is drawn after the block entity alpha pass depending
  on the camera's chunk, and the mark's depth hides it. Next build: these
  corners draw in the name tag pass (`GhostLatePass`).
- On `8b7c76d` (DLL `b970c0b3...e62b`): unchanged over glass and water; the
  0.02 outset made the faces look lifted over stone (screenshot). The late
  pass, the alpha-pass move and the outset are reverted (`8d3cdae`,
  `754a1d6`, and the following commit); parked as L-194.

## Re-sort interval: faster, but water blinked by section (2026-10-15)

Maintainer report on `dc38abd` (DLL `61a4a7d5...d6d5`): walking around
terrain_water_lamium now runs at 55+ fps (about 40 before), but the water
surface blinks out in 8-block sections, easily seen. Cause: a section that
entered the sorted budget between sorts was in neither mesh. Fix in the
next build. Real glass alone does not change with position or turning;
only a corner mark over see-through glass does.

## Perf trace of `2965ec8`: schematic frame cost (2026-10-14)

Maintainer runs on the `schematic_perf_trace` build of `2965ec8` (DLL
`48c0576e...d083`), Windows x64:
- A nearly empty world: terrain_water_lamium about 60 fps still and about
  40 fps moving; desert village 60 fps still, 55+ moving. Trace with the
  water schematic (720 sections): ghost pass about 5 ms/frame still;
  moving, the blended pass 6-7 ms/frame with a re-sort in 203 of 207
  frames (the section set within the 30000-quad budget changed nearly every
  frame and each change re-sorted at once; held to the interval in the next
  build).
- The busy test world (3086 sections): ghost pass 4-6 ms/frame even
  standing still with no builds (prepare 0.5, faces 0.8, lines 0.3, block
  actors 0.4, entities 0.7 ms): per-section draw calls are a standing cost
  (L-105 material).
- The far flicker and darkening remain there (likely past the sorted
  budget with many placements shown). Corners over glass still change
  colour with the camera position; adjacent corner lines are now stable.

## Near-only sort, corner marks, schematic frame rate (2026-10-14)

Maintainer report on `5723ac2` (DLL `67bee317...17e5`), one screenshot:
- Frame rate about the same: about 30 fps with terrain_water_lamium, about
  40 fps with desert village, as before today's changes; heavy schematic
  drawing in general, not a regression of the sort (to be measured).
- The flicker is gone near the camera but remains far away (beyond the
  24-block sorted radius).
- Corner 1/2 marks still change with the camera position (their faces more
  than their lines). Two adjacent corners trade which edge line shows on top,
  over glass and over stone alike.
Changes in `2965ec8`: see-through corners draw after the world's
translucent blocks; corner 1 pulled nearer than corner 2; the sorted mesh
takes the nearest sections up to 30000 quads. Deployed as a
`schematic_perf_trace` build of `2965ec8` (DLL `48c0576e...d083`) to
measure the frame time with terrain_water_lamium and desert village.

## L-192/L-193 pass with the global sort; its cost; corner mark (2026-10-14)

Maintainer report on `995b704` (DLL `5006e9bc...bd80`), local world:
- The flicker and the darkening are both gone.
- `terrain_water_lamium.mcstructure` drops from 60 to about 30 fps;
  `desert village.mcstructure` (with water) feels a little heavier than
  before but far less. Fix in the next build: only sections within 24 blocks
  are camera-sorted together.
- New: the red mark of save corner 1 over a see-through block looks
  different from different positions (two screenshots), not with turning,
  not over time, and not over an opaque block such as stone. Reading: the
  mark's far side is pulled toward the eye by a distance-dependent amount,
  in front of or behind the real block's near face. Fix `82d8298`: only the
  seen sides and their edges. Not checked.

## L-192/L-193 after the real see-through fix (2026-10-14)

Maintainer report on `171ed42` (DLL `9a2756f7...25ab8`), two screenshots:
- The flicker over real glass is gone, and an area that used to darken no
  longer does; glass ghosts still darken by camera position in another area
  (beside piston ghosts and red mistake marks).
- The flicker remains where see-through quads of different placements lie
  behind one another on screen; one placement shaped for several layers
  (`white_glass_L.mcstructure`) showed none from any angle.
Reading: blended quads were sorted per section at build time and sections
by their centers, so overlapping quads of two placements kept a fixed
order, and a section's order stayed the build-time one after the camera
moved. Fix in the next build: all blended quads sorted together from the
current camera (re-sorted after a quarter-block move). Not checked.

## L-192/L-193 isolated: real see-through blocks below (2026-10-14)

Maintainer report on `59ac118` (DLL `93738313...7cfc`), three screenshots:
- With one placement shown the border flicker does not occur. Glass ghosts
  darken by camera position (L-192) only where a real see-through block
  (glass) is below; jumping changes it only there. With two placements over
  real glass the flicker is limited to some areas.
- Simple graphics: the same. Vibrant Visuals: white glass ghosts do not
  draw correctly, so no flicker there.
- The flicker is irregular, every few frames at 60 fps, like an old
  fluorescent lamp.
Reading: real glass writes depth in the plane of the ghost's bottom face
(both sides drawn, no depth writes, unpulled), so the two fight by float
precision, which differs per section offset and camera position. Fix
`cddccc2`: such ghost faces are left out like faces against opaque
blocks. Not checked in game. Vibrant Visuals' wrong white glass ghost is a
separate observation.

## L-193 perf traces: Lamium's draw is constant while it flickers (2026-10-14)

Maintainer runs of two `schematic_perf_trace` builds (DLL `aa18be08...265d`
on `08118df`, then `82929ec8...343e` on `59ac118` with order and camera
counters), standing still where the glass ghosts flicker; the flicker is
seen from every angle and position nearby.
- No section rebuilds in the still periods (builds 0 for 5-7 periods of
  about 5 s); rebuilds seen only while looking around (looked-at).
- Second build, seven still periods: blended order changes 0, camera moves
  0, a blended draw every frame. Lamium draws the same thing each frame, so
  the alternation comes from what the game draws around it.
Next: isolate in game (one placement hidden, no water below, graphics
setting, how fast it alternates). Normal build `59ac118` redeployed.

## L-193 flicker remains on `08118df` (2026-10-14)

Maintainer report on `08118df` (DLL `a83ddf23...121e`), two screenshots: the
flicker remains. It also shows where faces do not meet directly (an air
cell lies between the glass in the red circle and the near glass), and the
view alternates between two looks. So it is not two faces in one plane
alone. Next: the `schematic_perf_trace` build (DLL `aa18be08...265d`, from
`08118df` with only that option) to see whether sections keep being
rebuilt while the view alternates.

## L-193 border log and piston state log (2026-10-14)

Maintainer run on `59ee8da` (1.26.51.01 without LIP), log read here:
- Piston ghosts: `file 1/1 state 2/2, loaded 0/0 state 2/2` for extended
  ones: the game's load leaves progress at 0, so the restore in `46ff471`
  is what draws them extended.
- L-193: four lines, all "border face kept at -87 84 -60 side 0: neighbor
  white_stained_glass sides 63 reaches true blended true/true near camera
  true". The faces were kept by the near-camera pairing, not missed as
  neighbours. Fix in the next build: same-kind see-through pairs drop both
  faces even next to the camera.

## L-182 and L-178 preview fixes pass; border flicker placements (2026-10-14)

Maintainer report on `7ba2d82`, DLL SHA-256
`539314b2ecf8b682fcf2f48c87b2bbea4fcf789edbabd82a195d60c17f4a6e01`,
local world: leaving and rejoining with a checked placement and opening the
Check tab preview repeatedly did not crash (L-182, the reporter not yet
asked); preview piston heads draw, extended and retracted (L-178).
The remaining flicker is between `pistons` (8x3x7 at -95 84 -60) and
`shulkers` (5x5x5 at -87 84 -63), both unturned, all layers: their white
glass ghosts face each other across x = -87 at y 84, z -60 and -59. A
bounded log line ("border face kept at ...") is in the next build.

## L-182 crash traces resolved; L-178/L-192 follow-up (2026-10-14)

L-182: the reporter sent three CrashLogger traces and minidumps
(13:59:00, 14:49:03, 15:53:07) from Lamium 0.2.1 on Minecraft 1.26.51 and
LeviLamina 26.51.5. Resolved here with the 0.2.1 symbol asset
(`Lamium-0.2.1-client-windows-x64.pdb.zip` from the GitHub release). All
three: SettingsSceneRender detour -> settings render -> table_view
renderTable -> schematics_view renderContent / drawSchematicsBody ->
preview::draw -> preview drawActors -> makeBlockActor (14:49, 15:53;
access at [rcx+0x30], rcx null or garbage) or renderBlockActor (13:59).
Cause: the preview kept its blocks and block actors across worlds;
preview::reset() was never called. Fixed in `95a09e0` (reset on world exit
and stop); not reproduced or checked in game.

Maintainer report on `5711cc3`, Windows x64, local world:
- Piston ghosts in the world are right, extended included, and under
  rotation and mirror. In the Schematics 3D preview the heads are still
  missing (fix `27150e5`, unchecked).
- The remaining z-fighting is between two adjacent placements (screenshot:
  a vertical boundary between their glass ghosts).
- L-192: the dark glass sections depend only on the current camera
  position, not on where the camera was before; with the camera below the
  floor both sides turn dark, back above only the right one; the 8-block
  size was checked against Chunk Borders.

## L-178 follow-up: piston heads, border faces, glass brightness (2026-10-14)

Maintainer report on `c364a64`, DLL SHA-256
`1c4bf8962539758de6881e8351b80cb81c8718f3a4e057d66a69745c7619b38a`,
Windows x64, local world (screenshot).

- Piston ghosts now draw their head, but extended ones draw it retracted.
  Candidate fix and diagnostic in the next build: the file's progress and
  state are put back into the ghost actor (gated) and logged once per kind
  ("piston ghost: file .../... loaded ..."), and the renderer sees the arm
  block's ghost in the head cell.
- Z-fighting between white glass and piston ghosts of the same placement is
  gone. Between white glass ghosts of two different placements it remains.
- New: white glass ghosts change brightness with the camera position, in
  world-aligned groups that look like the 8-block ghost sections, not per
  block; moving the placement keeps the dark area at the same world
  coordinates. Recorded as L-192.

## L-180 Riptide launch and L-178 piston ghosts (2026-10-14)

Maintainer report on `e8c5e3f`, DLL SHA-256
`77d901375df4cf780f5910a45f7525fe1749f7a407df87c4668c5eabe4a57e0b`,
1.26.51.01 LIP instance, Windows x64, local world.

- L-180: a Riptide launch in water and in rain puts the elytra on; a jump in
  the air glides; the chestplate returns a few seconds after landing; a
  charge without a launch and the child off change nothing. All passed.
- L-178 (on the earlier `253ff81` build, 1.26.51.01 without LIP): with
  `pistons.mcstructure` (pistons and sticky pistons in six directions,
  retracted and extended with their arms), the base draws correctly but the
  pushing face and arm never draw, in any state. The earlier wording of the
  report was the maintainer's reading of the reporter's screenshot. Cause:
  ghosts drew a block actor only for blocks without a mesh. Fix `b9c9d69`,
  not yet checked.
- Z-fighting seen where a glass ghost meets a piston ghost of the same
  placement, and where it meets another placement's glass ghost. Fix
  `17d7201` (one face per plane for see-through ghosts, touching placements
  counted), not yet checked.

## L-187 and L-179 follow-up fixes pass (2026-10-14)

Maintainer report on `7548f0f`, DLL SHA-256
`3618cc102f78a6b9be17cfa56ed8552290d420c28058e8d571d2fbfc52ca3e4f`,
both 1.26.51.01 instances deployed, Windows x64, local world: both checks
passed (the whole world map turns sharp after zooming in from a far view;
a shulker box opening another way gets the yellow mark, which goes away
when it is placed the right way).

## L-187 slot fix, L-179 opening check, Schematics off (2026-10-14)

Maintainer report on `d9a016c`, DLL SHA-256
`1388c732d6713ffa27f8d1c34ae3fbd903db1642c721d227a50253756b41d023`,
both 1.26.51.01 instances deployed, Windows x64, local world.

- The three requested checks were "mostly fine".
- L-187: after zooming in, only part of the view became sharp; the rest
  stayed coarse, enlarged far images (screenshot). Cause found in code:
  `00be8f0` kept a waiting slot's last-use time old, so it was reassigned
  every frame and the finer tiles never showed. Fixed in `f25188f`, not yet
  checked.
- L-179: the Check detects a shulker box opening another way (target card
  `facing up -> east`, screenshot), but no yellow mark appeared on it in the
  world while red marks showed elsewhere. Cause: the section marks judged the
  block alone. Fixed in `c92d93a` (shared comparison, section rebuilt when
  the opening arrives), not yet checked.
- Schematics off guidance: no problem reported.

## L-187, L-176, L-188, L-179 batch check (2026-10-14)

Maintainer report on `9b3b8a9`, DLL SHA-256
`b1ed4d6a708cbe9b91cad22c717d1435928fa51b3fae29ca46ba643efc151327`,
deployed to both the 1.26.51.01 and 1.26.51.01 LIP instances (which one ran
was not stated), Windows x64, local world. All diagnostics off.

- L-187 world map zoom-out: a small world stops at 1/8 as before; a large
  world zooms out until all explored area is on screen. Defect: right after
  viewing a large area, a quick wheel zoom-in briefly drew the map wrongly,
  apparently showing other places than the one zoomed into. Candidate fix
  `00be8f0` (a reused texture slot is drawn only a few frames after its new
  image is uploaded; any coarser image stands in), not yet checked.
- L-176: in Vibrant Visuals Hitboxes and Chunk Borders show their colors.
  Ordinary modes, graphics switching and depth were not itemized.
- L-188: L opens and closes settings; holding L opens it once; L typed in
  search does not close; binding L as a key works.
- L-179: rotated and mirrored placements with shulker boxes match a
  structure block load of the same file. Found: the Check counted a placed
  shulker box with another opening as correct (fixed in `27fff48`, not yet
  checked).
- Separately: with the Schematics feature off, R's radial menu items (area
  selection, save) did nothing, without a clear message. Guidance built in
  `6a8b37c`, not yet checked.

## L-176 Chunk Borders and Hitboxes lose colors in Vibrant Visuals (2026-10-14)

Maintainer report: Chunk Borders loses its line colors in Vibrant Visuals,
and an in-game check confirms Hitboxes also has no distinct line colors
there. The Hitboxes symptom is confirmed, rather than inferred only from
the shared rendering path. No fix has been implemented or tested.

The build/DLL SHA-256, loader, UI profile, packs and local/server context
for this observation were not separately reported. The latest deployed
build in this session is `dc3ac45`, DLL SHA-256
`f0f51371a77025a533a73730630cf05f8c1bc66d650c7dce53c9b8716a10a0fb`,
in the 1.26.51.01 LIP instance on Windows x64; that is context, not confirmed
identity for this check. Ordinary modes, switching graphics settings and
depth/occlusion regressions were not itemized in this report.

## L-174 final default reset accepted (2026-10-14)

Maintainer report on `dc3ac45`, DLL SHA-256
`f0f51371a77025a533a73730630cf05f8c1bc66d650c7dce53c9b8716a10a0fb`,
deployed to the 1.26.51.01 LIP instance, Windows x64. Loader revision,
UI profile, packs and local/server context were not reported. All diagnostics off.

- The supplied default-reset check passed. The flush-left placement with
  the chat bottom 44 GUI units above the screen edge is accepted, including
  the 4-unit gap above two default Durability HUD rows.
- The maintainer accepted closing L-174. No remaining defect was reported.
- A fresh settings file and the broader UI/pack/server checks were not
  separately reported; they remain coverage gaps, not completion blockers.

## L-174 expiry flicker fixed and default placement chosen (2026-10-14)

Maintainer report on `b28a40d`, DLL SHA-256
`91b932172b336bf50f9f3415ac82fce882a88d0ffa018d79b3768da39ebcc0ae`,
Minecraft 1.26.51.01, Windows x64; loader revision, UI profile, packs and
local/server context were not reported. All diagnostics off.

- The supplied expiry/long-message checklist passed. Remaining messages do
  not flicker when an older one disappears; wrapped long messages are fine.
- The maintainer chose their current placement as the default: flush left,
  vertically above two Durability HUD rows with a little space. Read-only
  confirmation of the saved HUD placement gives BottomLeft, dx 0, dy -45;
  Durability is at 100%, BottomLeft, dx 4, dy -4. Each native Lamium
  durability row is 18 GUI units, leaving a 5-unit gap at that placement.
- Follow-up decision in the same session: use 4 GUI units above the two
  durability rows, matching their bottom inset. The new default is therefore
  BottomLeft, dx 0, dy -44; the existing saved -45 placement is kept.
- This verifies the chosen position in the tested layout, not the new
  binary's reset/fresh-settings default. Broader coverage remains as before.

## L-174 native chat placement: initial checklist and expiry flicker (2026-10-14)

Maintainer report on `5b5750c`, DLL SHA-256
`b204f68fc239f41ff7545ea5eff561686a556b9dd2c274361105e3893dc83235`,
Minecraft 1.26.51.01, Windows x64; loader revision, UI profile, packs and
local/server context were not reported. All diagnostics off.

- The supplied three-step checklist passed: enabling puts native text and
  backgrounds at the bottom left and fades normally; HUD layout dragging,
  saving and reset work; OFF restores placement and coordinates and the
  chat input screen are unaffected.
- Exception: as messages disappear, the remaining messages briefly flicker
  (the maintainer described it as possibly being redrawn). Cause is not
  established. The first implementation forced position caches dirty after
  every render and took the panel height before native render-time layout.
- GUI scales, Pocket UI, packs, focus/world/dimension changes and server
  coverage were not separately itemized. The follow-up fix is unverified.

## L-175 title screen entry and language outside a world (2026-10-14)

Maintainer checks with screenshots, all diagnostics off.
- `6fc969a`: "Lamium 0.2.0" shows bottom right, hover underlines it, a
  click opens the settings; stays after returning from other screens and
  across UI profiles. Without a band it looked unlike LeviLamina's line.
- `f560b37`/`11665ae`: band added; measured from screenshots the band was
  about 93% opaque (drawn on several title-screen views per frame) and
  taller than LeviLamina's, with an 8 px gap.
- `bbbeb7c` (DLL SHA-256
  `520656d191aa944e799b70f22e0560b4d32c6b8918bf8c96378f8a71e4eedadf`):
  drawn once per frame at the measured opacity, height and text place;
  matches LeviLamina's line. Choosing a language from the title screen
  stayed at "Applying..." (executor not running outside a world).
- `a4810bc`: the language applies from the title screen; L works at the
  title screen and in worlds through the shared queue. The thin underline
  looked too thin in English.
- `5c35918` (DLL SHA-256
  `2e1e4831ca639e1621af27f4f4a6c37a32942b30039bf9fb91f695eb26f5fc08`):
  repeated switching from the title screen applies without a notice or
  freeze; in a world the 10 s notice still appears; the underline is right
  in English, Chinese and Korean (thin only in Japanese).

## L-175 Lamium settings outside a world (2026-10-14)

Maintainer checks, Minecraft 1.26.51.01, LeviLamina Client 26.51.7.
- `67a6cf7` (DLL SHA-256 `6b20815f...`): L opens nothing at the title
  screen or in OreUI menus; in a world it works as before.
- Uncommitted diagnostic builds: the title screen's name is `start_screen`;
  OreUI pages are `ui://./hbui/index.html`. Keys reach Lamium there and the
  settings action is queued, but LeviLamina's client thread executor never
  runs the queue before a world is joined.
- Diagnostic build with the fix (DLL SHA-256 `590e627b...`; the code of
  `1632237` plus temporary logs): L opens the settings at
  the title screen and in the OreUI settings and most other menus (not the
  dressing room or Marketplace; the maintainer is fine with that). General
  switches change; world-only views are dimmed and show "Available inside
  a world."; Esc returns to the title screen; in a world L works as before.

## Pack changes kept old block textures: bisect (2026-10-14)

Isolated BDS with the required magenta-dirt pack; the global test pack
(yellow dirt, cyan stone) active. Maintainer checks; "case" = join the BDS,
leave, enter a local world in the same session.
- Unmodded Minecraft (no LeviLauncher): global packs return after the BDS
  and after re-enabling them in a session.
- LeviLauncher, Lamium moved out (ChiyanMap, LHolo, LeviSchematic loaded):
  case shows yellow dirt (correct).
- `35b2692` + `feature_skip` (DLL SHA-256
  `4dcb8701004ea00badf1d4e51269e4ed3643d43d87c0877a04729e5010523dcd`):
  skipping Connected Textures: still wrong. Skipping the map, waypoints,
  schematic ghosts and target icons: the local world keeps magenta dirt.
  Skipping 20 more features: local yellow/cyan then the BDS shows yellow
  dirt and vanilla stone; after a restart the BDS's magenta dirt stays in
  the local world. Skipping all 50: correct. Skipping only "Settings
  screen" (which also draws the HUD, minimap included): correct.
- Only textures both packs replace (dirt) stuck at the first one loaded;
  stone changed correctly.
- Cause found in code: the minimap's block colors, radar faces, the
  saturation outline and Connected Textures read images by path through
  `TextureGroup::getCachedImageOrLoadSync`; that cache is reused when the
  terrain atlas is rebuilt for a new pack stack. Fix `241f635` reads them
  from the packs directly.
- Fix checked on `241f635` (DLL SHA-256
  `411ecd564b80f93d6fdaf5cc75bb3eba9b01ad05bacb39ee2185161bef21c8c7`, all
  diagnostics off): local world yellow/cyan, then the BDS magenta dirt and
  vanilla stone, then the local world yellow/cyan again. Minimap colors,
  the saturation display and Connected Textures look as usual.
- Same build, Skip server resource packs on: the BDS shows normal dirt and
  the toast "Server resource packs not applied (1)"; leaving for a local
  world brings back the global pack (yellow dirt, cyan stone). The earlier
  vanilla look after skipping is not seen with the fix.

## L-167 Skip server resource packs, product build (2026-10-14)

Maintainer check on `b1e03d1`, all trace options off. Deployed DLL SHA-256
`ccf8700298365689c01a3c3c87582b3a4f5c2f3400c637d42b7c449de91a8542`.
Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64, isolated BDS
1.26.51.1 with the required magenta-dirt test pack; the global test pack
(yellow dirt) active.
- Switch off: joining the BDS shows magenta dirt, no notice.
- Switch on, rejoin: joins normally, dirt normal, the toast "Server
  resource packs not applied (1)" appears.
- Then a local world in the same session: vanilla look, the global test
  pack not applied although Global Resources lists it; no notice. Same
  symptom as the earlier trace session, where the pack had been activated
  at the title screen in that session. Whether vanilla does the same after
  leaving a required-pack server, without Lamium, is unchecked.

## L-167 global packs with a required server pack; a lost global pack (2026-10-14)

Agent-made global test pack (yellow dirt, cyan stone) in the instance's
shared resource_packs; BDS as in the required-pack entry below (magenta
dirt). Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64.
- `2084bc9` + `language_trace=y` (DLL SHA-256
  `e24cf90959c5f8deaeacfa4bca4db588e9f82e767e06ff226f372e6dcec167dc`):
  calculator Close button closes. The maintainer activates the test pack at
  the title screen (log: Global stack set with 1 entry), enters a local
  world: dirt and stone are vanilla. Log: at world start the Global stack is
  set again with 0 entries; `global_resource_packs.json` still lists it.
  This session followed earlier forced exits (the language hang).
- Same game with Lamium moved out of mods (ChiyanMap, LHolo, LeviSchematic
  still loaded): the local world shows yellow dirt and cyan stone; Global
  Resources lists the packs. Joining the BDS with the required pack: dirt
  magenta, stone vanilla (the global pack is not applied), and in-world
  Global Resources shows the gray page. So the gray page and the dropped
  global pack on a required-pack server are vanilla, not Lamium's skip.
- Unmodded Minecraft (started normally, not through LeviLauncher; no
  LeviLamina or mods) joining the same BDS: the maintainer's own global
  pack is not applied, the server's pack is, and in-world Global Resources
  shows the gray page. This confirms the behavior is vanilla.
- Normal build `fb16849` code (DLL SHA-256
  `b890835bf52538df349ffa1ab84a260917e7e27a757483010bb1a09dcafd55c0`),
  Lamium back in mods, after clean exits: the local world shows yellow dirt
  and cyan stone; Lamium settings open with L. The earlier 0-entry Global
  stack did not recur; likely vanilla recovery after the forced exits or
  that trace session, not reproduced with Lamium.


Maintainer check on `1109e35`, all trace options off. Deployed DLL SHA-256
`0f42327a6d0d2141a680e0ab5facc5426a712940d5234139a6d3c7852ca1f9d6`.
Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64, local world.
- A second choice within 10 s shows the footer notice with the seconds
  left and does not switch; after waiting 10 s the switch works.
- Not a test of whether the interval prevents the earlier hang.
- Later report (2026-10-14, builds up to `241f635`): no freeze while
  switching in worlds since the interval was added; the maintainer keeps it
  as is for now. Not a stress test.

## L-167 required server pack: accepted, not applied locally (2026-10-14)

Agent-prepared isolated BDS 1.26.51.1 (`texturepack-required=true`, world
`lamium-packs` with one Lamium-made test pack that only turns dirt magenta).
Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64.
- Baseline on `2c77a5c` with `language_trace=y` (DLL SHA-256
  `16abdd0477cb4f60a39985051cca14ec46d8f03d9dd43976059fc844edd9abf0`): the
  maintainer accepts the download prompt; dirt is magenta. The trace line
  cap was used up earlier in that session, so this join has no trace.
- The same game at the title screen: rapid vanilla language switching does
  not hang and feels faster than in-world switching.
- Research build `2c77a5c` + `language_trace=y server_pack_skip=y` (DLL
  SHA-256 `fe9ab8a1c5c1561c4db2fad7470fb34ac2b986d9b998f49948e7cbcdf84a8ea3`):
  no download prompt (pack cached from the baseline). Log: pack info
  required=true entries=1; server stack resources=1 required=true; the
  research build empties it; Level stack set with 0 entries. BDS logs a
  normal connect/spawn and a disconnect only when the maintainer leaves
  about two minutes later.
- The maintainer joins normally, dirt has its normal texture, walking for
  1-2 minutes shows no missing textures or UI problems. Exception: in-world
  Settings > Global Resources shows only the top menu; its content area is
  plain gray.
- Not checked: a first join without a cached pack, packs with custom
  blocks/items/entities or UI, servers other than BDS.

## L-167 framed language list and a hang after repeated switching (2026-10-14)

Maintainer check on `2c77a5c` with `language_trace=y` (server pack skip off).
Deployed DLL SHA-256 `16abdd0477cb4f60a39985051cca14ec46d8f03d9dd43976059fc844edd9abf0`.
Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64, local world.
- The Game language list inside the settings frame works; switching
  repeatedly works at first.
- Schematic ghosts are rebuilt after each switch (Lamium drops ghost
  resources on purpose); the world's chunks are not redrawn.
- After 12 switches in 23 s (00:22:44 to 00:23:07) the game froze about
  9 s after the last one (ja_JP -> es_MX). The log shows that apply
  completed (`applied=true`) and ghosts rebuilt; nothing is logged after.
- The hung process used almost no CPU and did not respond. A minidump shows
  every thread waiting, the main thread in a kernel wait, and no Lamium
  frame on any thread's stack. Cause not established: not shown to be
  Lamium code, not shown to be vanilla. Whether vanilla title-screen
  switching hangs the same way is unchecked.
- Server pack checks (BDS required pack) not started.

## L-167 all game languages and in-world Global Resources (2026-10-13)

Maintainer check on `6d536e4` with `language_trace=y`. Source/deployed DLL
SHA-256 `7d9f888a25de561bb7cf4a51502c20125caae2bf2461a5e7efc45db37ce0f84a`.
Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64, local world.
- Game language lists 29 languages; switching to languages other than
  Japanese/English works without freezing. Log: en_US, ja_JP, fi_FI, nb_NO,
  el_GR, tr_TR and back, each `applied=true`.
- Each in-world apply logged the pack-language callback twice: once from
  `I18n::chooseLanguage`, once from Lamium's explicit call (removed after
  this check).
- Pause > Settings is the JSON screen
  `screen_world_controls_and_settings_fullscreen - global_texture_pack_tab`
  (global=false, resourceTab=true; the controller also holds all 29
  languages). The maintainer sees Global Resources listing active packs but
  no control to activate, deactivate or reorder them. No pack stack change
  is logged while in the world.

## L-167 in-world Game language list (2026-10-13)

Maintainer check on `9651b4e`, all trace options off. Source/deployed DLL
SHA-256 `82e276dfd570f157a8028cbf1ad63d220cf4fd23e92f0c754e0567985c1aa88e`.
Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64, local world.
- On `7355548` the list opened with no text (buttons still worked): the view
  did not flush its text batch. Fixed in `9651b4e`.
- General settings > Game language shows language names, codes on the right,
  the current language highlighted and "Current: ..." in the footer.
- Choosing English switches the display to English without leaving the
  world; the maintainer reports other language-dependent game text follows.
  The language is kept after restarting Minecraft.
- Log: ja_JP -> en_US -> ja_JP -> en_US, each `applied=true` with the saved
  preference equal to the current language.
- Not separately checked: English Search afterwards, schematic ghosts after
  a change, BDS.

## L-167 vanilla language selection baseline (2026-10-13)

Maintainer check on `e93d154`, with `language_trace=y` only. Source/deployed
DLL SHA-256 `8081322c3246a2603d3324a9d5d60e681d3f23f9347ff4381b631a0eb7655d70`.
Minecraft 1.26.51.01, LeviLamina Client 26.51.7, Windows x64, local world.
- Maintainer confirms title-screen Settings Japanese -> English -> Japanese
  switches display and fonts normally; after entering a local world,
  Lamium settings opens normally. Language is absent from pause settings,
  as reported before this build.
- Log at 23:11:59 / 23:12:06 shows `I18nImpl::chooseLanguage`, then the
  MinecraftGame observer and FontRepository update, followed by
  ResourcePackManager's language callback. I18n changes from ja_JP to en_US
  and back. During each choose call, the saved option still holds the old
  language; by the next selection it holds the previous selection.
- The actual settings screen is OreUI (`ui://./hbui/index.html`); the JSON
  GeneralSettings controller's selection hook is not seen. I18n reports 29
  supported languages. No crash or font regression reported.
- World join information contains required=false and zero resource packs.
  This is not a required-server-pack test. Pack removal, Global Resources
  editing, restart persistence, in-world language changes, cached feature
  names and BDS language switching remain unverified.

## L-166 controlled BDS delays, interruption and re-entry (2026-10-13)

Agent Computer Use with the maintainer performing modified row clicks and
the requested Alt+Tab/close gestures. Normal `481dc7d`, all trace options off;
source/deployed DLL SHA-256
`88deafb6e351ea72afb637ca5373163835023c53d09f6489c4901849aefd8352`.
Minecraft 1.26.51.01, LeviLamina Client 26.51.7, isolated BDS 1.26.51.1.
- Delay only the BDS thread named MC_SERVER; leave NetherNet communication
  threads running. Helpers verify thread ownership/name, have a fixed
  deadline, and resume their own suspension in finally. Connection remains
  alive. This creates delayed server processing, not measured network RTT.
- A 45-second hold with four ordinary books causes selection/refill but no
  new enchanted books. After responses return and focus returns, holdings
  stay at 200 emeralds, four ordinary books, eight enchanted books. The
  maintainer kept Minecraft foreground for six seconds after the trigger;
  normal logs do not isolate the timeout from subsequent focus loss.
- With two-second holds separated by 0.2-second run windows, a fresh
  modified click trades the remaining four books: 12 enchanted books total,
  172 emeralds, no ordinary books; the book row is sold out. This finishes
  before the attempted dimension interruption and counts only as delayed
  trading/sold-out coverage.
- A fresh smith starts with 240 coal at price 15. Two-second holds separated
  by 0.01-second run windows produce three emeralds and 195 coal. A console
  teleport then moves the player to a prepared Nether platform and back;
  the trade screen closes, totals remain three/195, and no run restarts.
  These snapshots do not prove the exact request phase at dimension change.
- Hold server processing again, trigger the coal row, immediately Alt+Tab.
  The selection changes from axe ingredients to 64 coal, but output remains
  three emeralds. Restore server responses and focus: still three/195.
- Repeat with the maintainer clicking the screen's × about one second after
  the trigger: closed screen, three emeralds and 195 coal after responses
  return. Physical Esc in the earlier attempt stopped Computer Use, so the
  × gesture replaces that attempt.
- Save/exit, join the same BDS again, wait and reopen the trader: still three
  emeralds and 195 coal, no automatic restart. The final item-entity query
  finds only a Lead, no coal or emerald drop. Remove the temporary Nether
  ticking area; leave the isolated test world saved.
- Coverage limits: real network latency/loss, exact runtime timeout cause,
  forced world exit with a pending response, other servers and price changes.
  Part 1 completion covers the implemented repeated-trade behavior and the
  observed local/normal-BDS checks; these limits stay in pre-release checks.
- Finish on Minecraft's main menu; stop only the agent's test BDS cleanly,
  restore server.properties byte-for-byte from its pre-test backup, and
  confirm the restored hash matches. All delay helpers have finished.
  The normal deployed DLL remains unchanged with the hash above.

## L-166 normal BDS settings-off and enchanted books (2026-10-13)

Agent Computer Use with the maintainer performing Shift + right clicks.
Same normal `481dc7d` build and source/deployed DLL SHA-256
`88deafb6e351ea72afb637ca5373163835023c53d09f6489c4901849aefd8352`;
Minecraft 1.26.51.01, LeviLamina Client 26.51.7, BDS 1.26.51.1 test world.
- Turn Repeated trading off in Lamium's settings. Several modified clicks
  on the librarian's 7 emeralds + 1 book offer do nothing: 256 emeralds,
  8 ordinary books, no enchanted-book output. Closing returns the inputs
  unchanged. Restore the setting to on.
- One modified click then produces eight enchanted books in separate
  inventory slots. Emeralds decrease 256 -> 200, ordinary books 8 -> 0;
  the run stops for missing second input. Closing preserves these totals.
  BDS finds no dropped-item entity.
- A latency fixture that pauses all BDS threads for 20 seconds disconnects
  the client before the maintainer can trade. The helper resumes every
  acquired thread; this attempt establishes no delayed-trade behavior.
  Client and test BDS are restarted for a more selective fixture.
- Interrupted runs and delayed trade responses remain unchecked here.

## L-166 normal BDS sold-out and safe input return (2026-10-13)

Agent Computer Use with the maintainer performing the right-click gestures.
Minecraft 1.26.51.01, LeviLamina Client 26.51.7, isolated BDS 1.26.51.1
test world. Normal build `481dc7d`, all trace options off; source/deployed
DLL SHA-256 `88deafb6e351ea72afb637ca5373163835023c53d09f6489c4901849aefd8352`.
- Repeat the full-inventory fixture from the preceding overflow report:
  64 emeralds in the ingredient slot, 64 in inventory, all 36 inventory
  slots occupied. Shift + right click produces no axe, both emerald stacks
  remain 64, and BDS finds no dropped-item entity. Remove the test stone
  before closing, because ordinary vanilla close can return overflow.
- With space restored and 128 emeralds, plain right click buys one axe and
  consumes three emeralds, then remains at one output while waiting.
- One Shift + right click buys 11 more axes: 12 total, 92 emeralds remain.
  The row displays sold out. Closing returns inputs to inventory as 64+28;
  reopening still displays sold out. BDS finds no dropped-item entity.
- Build, LamiumTests, LamiumNativeTests and the game-hook inventory check
  pass. Two-input books, settings-off, delayed responses and interruption
  checks remain pending; this entry does not complete L-166.

## L-166 full-inventory input overflow found (2026-10-13, BDS)

Agent Computer Use with the maintainer performing the Shift + right-click
start gesture. Minecraft 1.26.51.01, LeviLamina Client 26.51.7, isolated
BDS 1.26.51.1 test world. Normal build `0193c02`, source/deployed DLL SHA-256
`5a3dd09c714bb716221d1b735db0dc5bbaca6ccf18227bb646a570c63142c97d`.
- Prepare 128 emeralds and 34 full stone stacks. Left-click the axe row
  to place 64 emeralds in its ingredient slot; add one stone stack to fill
  the slot that selection freed. The output destination is now full.
- No dropped-item entity is found before starting. Shift + right click
  produces no axe, but an emerald item entity is present afterwards.
  Closing the screen also leaves emerald items beside the villager.
- The selection/refill route can return ingredient overflow by dropping it.
  Exact dropped counts and the response sequence are not established by this
  no-trace run. Do not treat zero output as a successful capacity check.
- Follow-up adds a preflight for returning both existing ingredient stacks
  before selection/refill. Build and pure tests pass; runtime recheck pending.

## L-166 Shift + right-click row trigger (2026-10-13, local)

By the maintainer on Minecraft 1.26.51.01 (LeviLauncher instance
`1.26.51.01`), existing local test world. Uncommitted L-166 work on
`a1773c6`, `--repeat_trade_trace=y`; source and deployed DLL hashes matched.
- Diagnostic build `b1eef22ade01181a3e30b0436536f34e066c12537c99a35e6fa8e83142c397d6`:
  Shift + right click reaches `ScreenView::handleButtonEvent` as global
  button 1485818800 (plain right click: 710676172) and reaches the trade
  rows' `InputComponent`s. Their mappings are 3024615655->2196830738
  (pressed), 710676172->4096534505 (pressed) and 3745745897->2196830738
  (focused); nothing maps 1485818800, so no screen event follows.
- Row remap build `af7be411ff29211ffa4994327d219f9cdad85f6cda63dcb50c2725dab83289b5`:
  Shift + right click started a run and sent the refill, then stopped
  ("context changed"): Shift's key auto-repeat counted as new input. Seen in
  game as selection only. Locked row: nothing. Plain right click: one vanilla
  trade. Inventory Shift + right click: vanilla stack move.
- Build `5196a9fa90a7e9d729a1663e08c9044feac24a5ef501d77108d0c39969f2485c`
  (key auto-repeat no longer cancels): Shift + right click on a usable row
  trades until sold out or inputs run out; a row without inputs trades
  nothing; a key or click during a run stops it.
- Not checked: BDS, full inventory, two-input/book offers, settings-off,
  screen close/focus loss/dimension change mid-run, the normal build.

## L-166 auto-place spike result (2026-10-13, local)

By the maintainer on `1.26.51.01 LIP`, existing local test world. Trace build
with the `RepeatAutoPlace` diagnostic; source and deployed DLL SHA-256
`28893a80cedf6e86db6b3a0f11b84f811380244cf50fcb921a1146e3b5156f7a`.
- Shift-clicking a trade row does nothing: no request batch follows.
- Shift-clicking the result slot calls
  `_handleAutoPlace(2147483647, trade2_result_item, 0)` once. Client-side
  `notifyTrade` predicts uses 5 -> 9, then one request batch with
  CraftRecipe crafts=4 succeeds and server-side `notifyTrade` confirms
  uses 5 -> 9 with 4 emeralds out.
- The 4 crafts match the 64 coal in the ingredient slot at price 15
  (64 -> 4 left). The maintainer held much more coal in inventory and only
  the slot contents were traded: no restock from inventory in this path.
- Memo verdict: single-request multi-craft is real, but slot-limited. It
  cannot replace the refill loop for trading up to holdings; it could only
  serve as bigger per-round steps after a refill.
- Not checked: full-inventory refusal on this path.

## L-166 first repeated-trade runtime pass (2026-10-13, local)

Agent Computer Use, Minecraft 1.26.51.01, LeviLamina Client 26.51.7,
existing local test world. Implementation based on `a1773c6` with uncommitted
L-166 changes, only `repeat_trade_trace` on. Source and deployed DLL SHA-256:
`5127f9181731ad7c4038ddd873b060b0125b0423a1f9c886c13be46343c9dd5b`.
- One right-click on coal runs seven separate vanilla automatic-recipe
  requests, uses 9 -> 16/16, emeralds 9 -> 16, total coal 514 -> 409.
  Every next request follows the previous owned success response and exact
  inventory delta; the run stops at the offer limit. Cursor stays empty.
- Left-clicking the axe row refills/selects its emeralds without buying.
  Right click buys five unstackable axes, emeralds 16 -> 1, uses 0 -> 5/12,
  then stops for insufficient inputs. Each output occupies an inventory slot.
- Right click on the visible locked level-3 row does nothing. Right click
  on an unlocked iron row without iron stops before any trade request.
- Earlier candidates preserved vanilla one-trade behavior because they
  looked only in the pressed callback map. The verified row handlers are
  single callbacks in the interacted map (IDs 2196830738 and 4096534505).
- This pass does not establish BDS, full-inventory refusal, two-input/book,
  settings-off, interrupted-run or trace-disabled behavior. Later source
  adds explicit cancellation on physical key/mouse input and dimension ID.

## L-166 client offer prediction research (2026-10-13)

Agent Computer Use in the same local test world, Minecraft 1.26.51.01,
LeviLamina Client 26.51.7. Read-only trace based on `a1773c6` with additional
offer/notify diagnostics; source and deployed DLL SHA-256
`8081f24def058d4f22ad20a12cbd89529d836fa929e0c82837a80f4911b4e033`.
- The client actor's offer list begins at 6/16 for the coal recipe. A manual
  right-click calls client-side `notifyTrade(0,1)` before sending the request,
  changing uses to 7/16. The local server independently changes 6 -> 7.
- Request -7 succeeds with coal 49 in the ingredient slot and emeralds 7.
  No trade-notification ActorEvent or UpdateTradePacket was observed for it.
- Client uses alone are prediction. An automatic round must wait for its
  request IDs and exact ingredient/output deltas; reopening on BDS still
  needs to verify the offer counter agrees with the server. No automatic
  behavior or BDS/full-inventory result is established by this trace.

## L-166 vanilla trade request research (2026-10-13)

Observed by the agent through Computer Use, authorized by the maintainer,
in the existing local test world on Minecraft 1.26.51.01, LeviLamina Client
26.51.7, Windows x64. Read-only trace from `a1773c6`, only
`--repeat_trade_trace=y`; source and deployed DLL SHA-256 both
`f07983c72b979e5abe09d2d3377df16f76259154c496a38ddbd69f2bde6fc876`.
- The weapon smith's coal offer costs 15, with uses 3/16 on opening.
  Trade button: one CraftRecipe (type 12); right-click row: one
  CraftRecipeAuto (type 13). Left-click row refills ingredients without
  executing another trade. Each ordinary request remains vanilla-owned.
- Three manual trades: emeralds 3 -> 4 -> 5 -> 6, requests -7/-9/-17
  accepted with inventory slot amounts after vanilla applied the response.
- No UpdateTradePacket was received after those trades. The screen's
  mVisualOffer stays unset (show=false, recipe=0), so it is not usable as
  current-offer evidence. Follow-up diagnostics inspect client actor offers,
  notifyTrade and ActorEventPacket for the authoritative-use update.
- Not checked: automatic repeated trading (not implemented), Shift trading,
  full inventory, sold-out/locked cases, two-input books, BDS or latency.
  The world was saved and the client closed normally after this round.

## L-162 normal-build BDS smoke and margin 0.02 sneak release (2026-10-13)

By the maintainer on test BDS 1.26.51.1, Minecraft 1.26.51.01,
LeviLamina Client 26.51.7, Windows x64. Normal build from `e8b0a52`,
`--edge_trace=n`, deployed DLL SHA-256 rechecked:
`c698ec122e28399d5bf82a539cb9b2ea449ecf7e00af98641f11979ae1794699`.
- Edge stopping and short slab bridging pass on the restored normal build.
- Separately, at margin 0.02, approaching an edge while holding Shift and
  then releasing Shift does not fall. The maintainer suggests the improvement
  may follow L-162; this is an observed result, not an established cause or
  evidence that 0.02 covers all server corrections.
- Maintainer decision: retain the safer existing setting policy/default.
  Default margin remains 0.10 and the selectable range remains 0.02–0.30.
  No code or user configuration is changed for this result.
The normal-build next-launch check is complete. Longer play, other servers,
slippery blocks and corrections already moving the feet off support remain
the stated limits; no additional diagnostic run is requested.

## L-162 packet consistency and guarded replay verified on BDS (2026-10-13)

By the maintainer on test BDS 1.26.51.1, Minecraft 1.26.51.01,
LeviLamina Client 26.51.7, Windows x64. Build `e8b0a52`, `--edge_trace=y`,
deployed DLL SHA-256
`406861193453a1f8e8256ce540c38dd6ea803785da2609b5157de5f93a938553`.
- WASD, stopping on key release and edge stopping are normal. Edge Guard
  on, margin 0.10: repeating the previously failing slab-bridging operation
  for two to three minutes shows no problem. Jumping, sneak release and
  disabled movement remain normal.
- The packet raw-vector change and owned-copy paths execute. Outgoing raw
  and analog agree on guarded sends while live physical input retains its
  direction keys. For example tick 4100 replaces raw (0.707,-0.707) with
  guarded (0.017,-0.027); the send dump has matching move/raw/analog.
- Twelve correction windows are recorded. Nine replay guarded analog input
  without direction keys, retaining matching processed/raw values through
  preApply, both vector calculations and postApply. The other three replay
  ordinary direction-key input, as expected. No fall dump or hook failure.
- Correction 11 shifts x about +0.045; guarded (-0.038,0.017) survives replay.
  Correction 12 shifts z about +0.044; guarded (0.017,-0.027) survives replay.
  These approach the earlier small-correction case without losing input or
  falling; they are not an exact reproduction of the original fall chain.
The log is saved locally (SHA-256
`5739904c25ce628d9914684beb04a78845b8d1ab6a29e97ecb133cac985e61b4`).
Close L-162 for the verified replay/input-consistency fix. The earlier
unsupported-position correction was not repeated at the identical location,
and the server's internal use of the raw field is not proven by this run.
Longer play, other servers, slippery blocks and corrections that already put
the feet outside support remain coverage/behavior limits in Pre-release checks.
The instance is restored to the saved no-trace normal build from the same
`e8b0a52` source; source and deployed DLL SHA-256
`c698ec122e28399d5bf82a539cb9b2ea449ecf7e00af98641f11979ae1794699`.
Configuration has `--edge_trace=n`; LamiumTests passes. This normal artifact
has not yet been checked in game; its next-launch smoke remains listed.

## L-162 fall reproduced while slab bridging despite guarded replay (2026-10-13)

By the maintainer during the additional BDS run, explicitly while placing
slabs to bridge. Minecraft 1.26.51.01, test BDS 1.26.51.1,
LeviLamina Client 26.51.7, Windows x64. Same build `4fe7b29`,
`--edge_trace=y`, deployed DLL SHA-256
`2fd64e3bbbd9706a8ef4e5b5d66a5aadd275d6ace9950c9e1abfad66d4ec05ed`.
- Three correction windows and one fall dump are recorded. Hook attachment
  and owned-copy execution succeed; no hook failure is logged.
- Correction 1, server tick 963: input `0x1fec66be4d0` captured guarded
  analog (0.042,0.024) with live physical Down/Right. Its replay preApply
  replaces raw Down/Right with that guarded analog; both replay vector
  calculations and postApply retain it. This verifies actual ownership of
  the supplied copy and fixes the observed raw-key rebuilding at this boundary.
- Correction 2, server tick 979: the local position x=0.161 is corrected
  to x=0.339 (about +0.178). The corrected foot box starts at x=0.039;
  the last logged support slab ends at x=0.000. It is already outside that
  support before replay moves it. Input `0x1feb5bdd6a0` retains guarded
  analog (-0.084,-0.099) in both processed and raw replay states, with no
  direction keys. Replay moves about +0.075 in x and -0.078 in y, reaching
  x=0.414 and leaving the ground. The next live frames fall; one fall dump
  appears at 13:53:46.121. Correction 3 occurs after landing lower down.
- Slab placement/refusal calls occur shortly before correction 2. These
  client-side calls and shapes do not establish which server placement or
  collision produced the larger correction; no jump flag is logged in the
  recorded onset. Do not equate this with the exact earlier +0.047 case.
The log is saved locally (SHA-256
`ad57a08dfb7cc449545d346ecba997b1e8adc2b129cd1499b4bf1c23c19ff312`).
The copy path preserves guarded input but is insufficient to prevent this
fall. The existing replay input calculation skips copied component states,
and the collision guard leaves an already unsupported box vanilla. Next
investigate slab-placement/correction ordering and guarding against the
corrected replay position; replaying a brake planned at the old position
alone cannot recover support already lost to the correction. Keep L-162
open. Additional repetition on this unchanged build is not requested.

## L-162 actual-capture copy passes BDS smoke checks (2026-10-13)

By the maintainer on test BDS 1.26.51.1, Minecraft 1.26.51.01,
LeviLamina Client 26.51.7, Windows x64. Build `4fe7b29`, `--edge_trace=y`,
DLL SHA-256 `2fd64e3bbbd9706a8ef4e5b5d66a5aadd275d6ace9950c9e1abfad66d4ec05ed`
(deployed DLL still matches the saved diagnostic build).
- WASD and stopping on key release are normal. Edge Guard on, margin 0.10:
  diagonal edge approaches and slab bridging have no pullback or fall.
  Jumping, sneak release, disabling, and enabling/rejoining remain normal.
- The capture hook attaches and logs use of an owned guarded input copy.
  Read-only capture/preApply/postApply diagnostics also attach. The first
  approach records guarded processed analog alongside untouched physical
  raw direction keys in the live component, as intended.
- Only one correction window is recorded, at server tick 802. Its selected
  input `0x28db4af9f00` was captured with unguarded Down/Right, processed
  and raw analog (0,0), movement (-0.707,-0.707). Its replay calculates the
  same full vector. That is expected for this unguarded capture and provides
  no evidence for or against the guarded-input substitution during replay.
  No fall dump or capture-hook failure is recorded.
The log is saved locally (SHA-256
`879678f209da9fe4dba0583feb74f0c9e88798c10aeb3eedd5e204f6990f1f0f`).
Normal controls and execution of the copy path are verified; replay of a
guarded capture and the original rare fall remain unchecked. Keep L-162
open. Next collect another bounded BDS run on this same diagnostic build,
alternating diagonal edge approaches and slab bridging at margin 0.10;
no forced fall or repeated full smoke checklist is needed.

## L-162 actual input capture and replay application observed (2026-10-13)

By the maintainer on test BDS 1.26.51.1, Minecraft 1.26.51.01,
LeviLamina Client 26.51.7, Windows x64. Build `a32e9bb`, `--edge_trace=y`,
DLL SHA-256 `f7b23cf81bcb228209497a24ab220d17c8f53147aab7c136fca11ff16f34bef1`
(deployed copy matches the saved diagnostic build).
- WASD, stopping on key release and margin 0.10 edge stopping work. Several
  minutes of diagonal approaches and slab bridging found no problem.
- All exported diagnostics attached; actual input capture/preApply/postApply
  diagnostics attached at compiler-resolved slots 22/1/2. Three correction
  windows were recorded with no fall dump.
- At correction tick 1251, capture on input object `0x29dc76cbcb0` receives
  guarded processed analog (0.170,-0.145), but raw Down/Right and analog
  (0,0). The same object is selected at tick 1252 and applies to the replay
  entity. Both replay calculations rebuild (-0.707,-0.707) from Down/Right;
  the replay entity's processed input has become that full vector by
  postApply. At tick 1587 the same chain turns guarded (-0.519,-0.428)
  into full Down (0,-1), using another traced input object.
This directly connects local input capture to actual replay application,
not only the prepared entity snapshot. It verifies input loss at two guarded
replay cases; it does not reproduce the original rare fall or establish a fix.
The log is saved locally. The next candidate will substitute guarded
horizontal fields in an owned component copy passed to actual input capture,
leaving the live physical input untouched. That candidate is not built yet.

## L-162 rollback recovery confirmed (2026-10-13)

By the maintainer after restoring diagnostic build `8b8823d`, Minecraft
1.26.51.01, LeviLamina Client 26.51.7, test BDS 1.26.51.1, Windows x64.
DLL SHA-256 `7f10147c19e878b0ced7d4da94c70c2e8d6ca0e15c35e2ca6b760b67c4e238d0`
(deployed copy matches the saved build).
- WASD forward/back/left/right operation and stopping after key release
  are restored. Edge Guard on, margin 0.10 stops at the edge again.
- The recovery run records two correction windows with no fall dump.
  Full Down movement still appears during the first replay; the second
  contains Up/Jump/Sprint while airborne. L-162 remains unresolved.
The recovery log is saved separately. Next diagnostics observe actual
selected replay input capture/application, without either removed input
mutation. No additional movement fix is claimed.

## L-162 persistent raw-input candidate breaks WASD; reverted (2026-10-13)

By the maintainer on test BDS 1.26.51.1, Minecraft 1.26.51.01,
LeviLamina Client 26.51.7, Windows x64. Build `981f630`, `--edge_trace=y`,
DLL SHA-256 `49dab77189696704dd485d5206eebab2a86b2c243af75ce3c8467a30d78cda89`
(deployed copy matched).
- Maintainer: WASD no longer controls movement; only slow sliding relative
  to the view direction remains. The four smoke checks could not be run.
- Trace: after synchronization at send counter 55, raw input has no
  direction flags and analog (0.136,-0.033). Frame captures and normal raw
  move calculations at counters 56 and 57 retain that same analog input.
  This supports feedback of the guarded value into later physical-input
  processing; the precise device-update dependency is not established.
- Both L-162 movement candidates are removed from source. The instance
  was restored, while Minecraft was closed, to previously tested diagnostic
  build `8b8823d`, DLL SHA-256
  `7f10147c19e878b0ced7d4da94c70c2e8d6ca0e15c35e2ca6b760b67c4e238d0`
  (saved source and deployed copy matched). This has diagnostic history
  reads, with neither candidate's extra history-input mutation.
The failed run's log is saved. Recovery after this deployment still needs
the maintainer's confirmation; earlier checks on `8b8823d` passed. L-162
remains open. Do not use the saved persistent-input normal or trace builds
for restoration or release. Next establish the actual replay input capture
and application boundary independently of entity snapshot preparation.

## L-162 history-capture candidate, BDS correction replay still bypasses input (2026-10-13)

By the maintainer on test BDS 1.26.51.1, Minecraft 1.26.51.01,
LeviLamina Client 26.51.7, Windows x64. Build `d59f16b`, `--edge_trace=y`,
DLL SHA-256 `476dc27c76b6943d19fac13f13df06da4e0dcefe77cf8f5314f6f30acc780077`
(deployed copy still matches the tested build).
- Maintainer confirmed all four checks on BDS: ordinary edges stop, slab
  bridging has no pullback or fall, jumping leaves, sneak release does not
  fall, disabling restores ordinary movement, and enabling then rejoining
  restores the stop.
- Nine correction windows were recorded, without a fall dump. Prepared
  rewind entities retain guarded raw snapshot input, but eight windows
  calculate full Down movement (0,-1) from another input state during
  replay. At correction tick 418, prepared raw analog (-0.635,-0.395)
  has no direction flags, yet both replay calculations use Down and (0,-1).
  The live raw input at this point still contains Down and analog (0,0).
- Thus the scoped frame-capture substitution does not fix actual replay
  input loss. Smoke checks alone did not establish a fix. The original
  rare fall remains unreproduced.
The BDS log is saved separately. The next candidate removes the scoped
history hook and keeps processed horizontal input in the live history raw
fields after normal input calculation, preserving other physical fields.
That candidate is not yet runtime-verified.

## L-162 history-capture fix, solo smoke checks (2026-10-13)

By the maintainer in solo play, Minecraft 1.26.51.01, LeviLamina Client
26.51.7, Windows x64. Same diagnostic build `d59f16b`, `--edge_trace=y`,
DLL SHA-256 `476dc27c76b6943d19fac13f13df06da4e0dcefe77cf8f5314f6f30acc780077`
(deployed copy still matches the tested build).
- Maintainer confirmed all four checks again in solo play: ordinary edges
  stop, slab bridging has no pullback or fall, jumping leaves, sneak release
  does not fall, disabling restores ordinary movement, and enabling then
  rejoining restores the stop.
- The new session log identifies a local world and reports both the local
  server-copy guard and the raw history-capture guard running. The first
  guarded approach records raw history input without horizontal direction
  flags and guarded analog values (for example (0.098,-0.086)).
- No correction window or fall dump was recorded in this solo session.
  This expands local-world smoke coverage; server correction replay and
  the original rare fall remain unverified in the fix build.
The solo log is saved separately from the preceding BDS run. The authorized
additional BDS run remains pending; solo play does not replace that check.

## L-162 history-capture fix, smoke checks and capture verified (2026-10-13)

By the maintainer on the local test BDS 1.26.51.1, Minecraft 1.26.51.01,
LeviLamina Client 26.51.7, Windows x64. Build `d59f16b`, `--edge_trace=y`,
DLL SHA-256 `476dc27c76b6943d19fac13f13df06da4e0dcefe77cf8f5314f6f30acc780077`
(source and deployed copy matched).
- Maintainer: at margin 0.10, ordinary edges stop and slab bridging has no
  pullback or fall. Jumping leaves the edge; releasing sneak at the edge
  does not fall. Disabling restores ordinary movement; enabling and
  rejoining restores the edge stop. All four checks passed.
- All exported history diagnostics attached. The history-capture guard
  ran. At frame tick 181, the raw history input had no horizontal direction
  flags and guarded analog (-0.217,0.171), while the prior processed input
  still held Right and move (-1,0). Subsequent frames likewise captured
  freshly guarded raw analog values. Live packet raw movement remained
  (-1,0), supporting restoration outside synchronous history capture.
- No correction window was recorded in this run. Capturing guarded raw
  input is verified; replay after a correction and the original rare fall
  remain unverified. A fall dump contains full unguarded movement near the
  disable check; its relation to that check is inferred from the reported
  sequence, not an explicit setting-state trace.
The log is saved locally. The maintainer agreed to a further few minutes
of diagonal edge approaches and slab bridging with the same diagnostic
build to collect a natural correction. Other servers, slippery blocks and
large/late corrections remain unchecked.

## L-162 second diagnostic round, guarded input lost in replay (2026-10-13)

By the maintainer on the local test BDS 1.26.51.1, Minecraft 1.26.51.01,
LeviLamina Client 26.51.7, Windows x64. Build `8b8823d`, `--edge_trace=y`,
DLL SHA-256 `7f10147c19e878b0ced7d4da94c70c2e8d6ca0e15c35e2ca6b760b67c4e238d0`
(source and deployed copy matched).
- Maintainer: margin 0.10 stops at ordinary edges, slab bridging has no
  pullback or fall, jumping may leave. All three checks passed again.
- All three exported history diagnostics attached. Five correction windows
  were recorded without a fall. History frame inputs retained the guarded
  processed input but physical direction keys in `mRawInputState`.
- Example, correction tick 410: captured processed move (0.030,0.036), no
  direction flags; captured raw input Down/Right, analog (0,0). Preparation
  tied live entity 2/11 to rewind entity 2/2621454. During replay the copied
  state calculated (-0.707,-0.707) twice, ignoring the guarded analog move.
  Later windows show the same loss with other directions and corrections.
- Another case (tick 1557) retained processed (-0.141,0.168) in the prepared
  rewind entity, but raw Down/Left rebuilt (0.707,-0.707); replay then moved
  live z from 104.805 to 104.706. No fall was observed.
This establishes loss of the guarded horizontal input during history replay,
not reproduction of the original rare fall. History is captured before
that tick's movement calculation. The candidate fix guards an owned local
raw-input copy from the current frame's position and velocity, substitutes
its horizontal fields only during capture, then restores live raw input.
Its runtime check remains pending.
Other servers, slippery blocks and large/late corrections remain unchecked.
The round's log is saved locally before the next run.

## L-162 first diagnostic round, no fall (2026-10-13)

By the maintainer on the local test BDS 1.26.51.1, Minecraft 1.26.51.01,
Windows x64. Actual loader reported by the runtime: LeviLamina 26.51.7
(compiled SDK 26.51.5). Build `d476ae5`, `--edge_trace=y`, DLL SHA-256
`b612379a7034312c2999cc3d08c0382dd06216dba81cdd0f4c8907d0fee92f5f`
(source and deployed copy matched).
- Maintainer: Edge Guard on, margin 0.10; approaching an ordinary edge stops,
  slab bridging has no pullback or fall, and jumping may leave the edge.
- Log: the first guarded approach was saved. One correction at tick 2400
  moved z from 236.809 to 236.765 (ground true); after send tick 2401,
  `tickMovementCorrectionReplay` ran on registry 2. Two move calculations
  used `other` input states with Down/Right and move -0.71,-0.71 unchanged;
  replay collision used box min (-27.791,66.500,236.465) and speed
  (0.008,-0.078,-0.113), also unchanged. Replay then changed live z from
  236.695 to 236.652. There was new slab support in this window and no fall.
- The input-history diagnostic reported "unsupported SDK virtual-call
  thunk" and did not attach. Capture/preApply/postApply, exact replay
  ownership and replay-versus-live motion are therefore still unobserved.
  The trace supports copied replay input bypassing the live input guard,
  but does not establish the original rare fall's cause or a fix.
No other-server, slippery-block or rare-fall coverage. Round 2 replaces
the unavailable history diagnostic with exported SDK history boundaries;
it changes no movement. The original round's log is saved locally.

## L-171 Calculator (2026-10-13)

By the maintainer, local, screenshots.
- `e664562` (DLL `1ffc66f7...c9c7f35bb`): the panel opened but showed no
  text (no text flush). `bef8059` (`f4e523f4...efbc98736`): text, the
  sidebar item, `20 lc - 1234` = 67,886 with "39 chests + 7 stacks + 46",
  Enter and `ans / 2`, Up/Down, `x`, copying, Esc, no gameplay input
  while typing. OK; review led to calculator-v2/v3.
- `a2fd612` (`5b11a983...ca6ae870`): the two-column panel, keys, results,
  history cards, copy button and menu, copy notes. OK. Help showed the
  history through it; "v" did not read as a menu; the history did not
  scroll; "×" sat left.
- `4eb9978` (`3a2fbff0...ba16de9f6`): help covers the history, wheel and
  scrollbar scroll, the menu triangle, shaped × − backspace, "nether"
  fits. OK; Japanese checked too, no squashed text. The scrollbar moved
  in card steps and backspace left the operator spaces.
- `de9792d` (`3b0b6718...e3013a3`): smooth scrolling with cards clipped at
  the column's edges; backspace removes "2 × sqrt(" piece by piece. OK.
Not seen: narrow windows, Simplified Chinese, a resource reload while open.

## L-172 Text fields: caret, paste and click (2026-10-13)

By the maintainer, local.
- `94e48b5` (DLL `e8c43c16...5d3976d`): settings search with Left/Right,
  Home/End and typing in the middle, Delete; a Shapes or Waypoints name
  typed and converted with the Japanese IME in the middle of the text;
  Ctrl+V into a name field; a click inside a name field moves the caret;
  a number field (zoom magnification) edited at a moved caret. OK. The
  caret bar sat on the next letter (a "t" hid it).
- `ed4a4e7` (DLL `6c40d457...b4d955b6`): the caret bar no longer covers
  letters; world map waypoint X typed into its new number field moves the
  waypoint with Enter, Esc keeps the old value. OK.
Not seen: Ctrl+C, word jumps, very long text scrolling, the save prompt.

## L-164 Connected Textures with generated sprites (2026-10-13)

By the maintainer, local, screenshots.
- `028b1ed` (DLL `2e3947c2...5a95210`): glass (clear, stained, tinted) joins
  without inner lines and keeps its outer frame in L shapes and single
  rows; panes joined across and up show no dark half (L-133) and no
  stretch; bookshelves with a neighbor on one side only show no dark bands
  under Vibrant Visuals (L-156); stacked sandstone loses its band; the
  switch changes the look at once. OK. Under Vibrant Visuals large joined
  areas caught light flatly (bookshelves, sandstone, glass). Log: 275
  sprites for 21 textures, all in place, base places match vanilla.
- `6274b53` (DLL `ca4b0d23...4fa6c15`): sprites get the base's texture set
  with their maps baked the same way; joined faces now catch light like
  single blocks under Vibrant Visuals, and nothing changed with it off.
  Log: bookshelf's texture set has a `metalness_emissive_roughness_subsurface`
  map; 275 texture sets with 275 baked maps. OK.
Not seen: resource packs (other textures or sizes), a resource reload while
in a world, servers, the unverified-version fallback.

## L-164 spike round 4: bookshelf sides, Vibrant Visuals (2026-10-12)

By the maintainer, local, `d126608` (`ctsprite_trace`, DLL
`bb470c6d...9b81f4d0c`), screenshots: three bookshelves in a row, the
middle one's sides (joined left and right) drawn as one quad with a
generated sprite; one-sided joins still split.
- The middle shelf joins without the divider, no missing texture. OK.
- Vibrant Visuals: no dark bands on the sprite face. With only a left
  neighbor (split cells) the dark showed; with neighbors on both sides
  (sprite) it did not. The L-156 darkening follows the split, not the
  bookshelf or the glass below.
- Vibrant Visuals surface (relief, gloss): no visible difference from the
  neighbors, although the log shows the base textures have texture sets in
  pack 13 and the sprites have none (`.texture_set.json` lookups -1).
Not seen: other blocks with stronger PBR maps, resource packs.

## L-164 spike: a generated glass sprite in the terrain atlas (2026-10-12)

By the maintainer, local, trace builds (`ctsprite_trace`), three rounds. A
3x3 plain glass wall with Connected Textures on; the middle face (joined on
all four sides) drawn as vanilla's single quad with a sprite Lamium
composed from the loaded glass texture.
- Round 1 (`e94426b`, DLL `c246c2ec...02e1d367`): the middle face showed the
  missing texture, also under Vibrant Visuals. Log: the terrain atlas took
  the added `texture_data` entry and gave the sprite its own place, but
  never asked the texture group cache for the image.
- Round 2 (`4cb9ffe`, DLL `4e1fbdf0...dfaef44639`): still the missing
  texture. Log: the atlas first asks `getPackStackIndexOfResource` and gave
  up on -1; no load followed.
- Round 3 (`67e9f26`, DLL `786d5601...5b77a4e9`): the middle face looks like
  the rest of the joined wall, also under Vibrant Visuals. Log: the lookup
  answered with plain glass's pack for image extensions, the atlas then
  loaded the PNG through `load(ResourceLocationPair, ..., extensions)`;
  `.texture_set.json` lookups stay -1 (no PBR data for the sprite). OK.
Seen in the log: the string half of the borrowed lookup answer printed as
garbage; a real build must return a view of the matching entry of the
caller's extension list. Not seen: blocks with PBR texture sets, the L-156
bookshelf case, resource packs, a resource reload while in a world,
mipmapped distance views beyond a short step back.

## L-147 Status effects HUD and L-150 ammo in the Durability HUD (2026-10-12)

By the maintainer, local, over `95a34d5`, `c670147`, `bc3e830`, `cd738e5`
and `ee84063` (last DLL `6c72ab47...2e2c27`):
- Status effects HUD: names in Japanese, levels, shortest-first order,
  times counting down, the warning color under 10 s and the vanilla icons
  hidden while on (`95a34d5`). Icons by effect id and the infinite word
  "無期限" (`c670147`; the effect's own icon name drew nothing and the
  infinity sign was missing from the Japanese font). Level column in
  Arabic numerals, right-aligned with the times, with names on and off
  (`cd738e5`); level 1 shown as "1" (`ee84063`). OK.
- Durability HUD: arrows beside a bow, arrows and fireworks beside a
  crossbow, fireworks beside an elytra, the loaded crossbow mark (`95a34d5`);
  zero in the warning color (`c670147`); ammo icons the same size as the
  tools after clearing the pickup pop, the 4-unit gap before the count, and
  separate Armor and Elytra switches (`bc3e830`); 6 units between a
  crossbow's two kinds (`ee84063`). OK.
Not seen: beacon effects, servers, Pocket UI.

## L-148 Nether portal screen hidden (2026-10-12)

By the maintainer, local, `cd47a29` (DLL `78c1f9c4...c3ff37ad`): with Hide
effects on the purple portal screen does not show and the transfer works as
usual; with the child off it shows; the child's key toast reads "Hide the
Nether portal screen". OK. Earlier trace runs the same day: the effect time
zeroed after each tick removed the screen without changing the transfer
time.

## Durability HUD per-line background (2026-10-12)

By the maintainer, local, trace build of `2466a02` (DLL `bc8847c6...df404a`):
"Per line" can be chosen for the Durability HUD; each row gets its own band;
moved to the right side the bands line up on the right edge. OK.

## Overlays over the vanilla selection outline (2026-10-12)

By the maintainer, local, `da218bd` (DLL `bcdc66b2...31e650e0a`): Chunk Border
lines on the targeted block stay over the black selection outline; the
schematic save area's outline and corners are not hidden by it; chunk lines
behind walls do not show through noticeably; on Shapes the black outline
still shows. All OK.

## L-157 Chunk Borders pulled toward the eye (2026-10-12)

By the maintainer, local, `6cd2a68` (DLL `7f9ddee8...85011914`): lines on a
wall lying on the chunk wall's plane stay whole at different distances and
angles; lines behind walls do not show through noticeably; Hitboxes still
draw. All OK. Noticed: where a chunk line runs along the vanilla block
selection outline (black), the black outline covers it; a schematic corner
mark's outline stays over the black outline (screenshots, not in the repo).

## L-161 edge margin setting (2026-10-12)

By the maintainer, normal build `bd65aae` (DLL `1ce35b49...debb12e0`): "Stop
inside the edge by" appears under Edge Guard showing 0.10 blocks; at 0.02 the
player gets closer to the edge than at 0.10, at 0.30 stops further inside;
the value survives rejoining; English and Simplified Chinese labels display.
All reported OK.

## L-161 margin 0.1 and build-start refusal (2026-10-12)

By the maintainer, trace build of `2dc7e45` (DLL `5f17de3b...2802c818`), BDS
and a local world: slab bridging no longer fell except once on BDS. Trace of
that fall: a server correction (+0.047 block) arrived with 0.139 block of
overlap left, then the client's next position was 0.17 block further out
(as if its re-run of recent ticks used unguarded input), with no overlap,
and it fell. The maintainer finds stopping 0.1 block inside slightly less
convenient for placing blocks and asked for the margin as a setting.

## L-161 slab merge refusal on BDS (2026-10-12)

By the maintainer, normal build `b36cf7b` (DLL `f7c89965...51084cc6`): on
BDS a slab still went into the player's own cell and they fell; in a local
world it did not. Trace build `3d021db` (DLL `3fb28139...b1533210b`): the
`useItemOn` check ran for the local player with a slab and refused the
pushing merges it saw (both `buildBlock` calls then returned false), and
allowed merges under the player's center. Two BDS falls: (1) the player
stood 0.026 block over a slab and a server correction placed them 0.04
further out, off the block; (2) the slab under the player became a full
block while their center was outside it; the placement lines had hit the
trace's 300-line cap, so whether that merge passed the check is unknown.
Not seen locally.

## L-160 slab merge push-out; vanilla sneak check (2026-10-12)

By the maintainer, trace build of `3c5d202` (DLL `fa261000...b0ea917d`), BDS,
four slab-bridging falls: each time the player stood on a bottom slab with a
tiny overlap (e.g. 0.03 block in x, feet 67.5), the cell's shape changed from
y 67.0..67.5 to 67.0..68.0 (a slab merged into a full block in the player's
own cell), and the next tick the player moved outward and fell, with no
server correction. With Edge Guard off and sneaking at the edge, the
maintainer could not place a slab into their own cell at all (vanilla
sneaking refuses it), so vanilla sneaking does not hit this. Movement fixes
of L-160 held in this session; the merge case went to L-161.

## L-160 slab bridging falls (2026-10-12)

By the maintainer, trace build of `29bf8ad` (DLL `d86012bb...50d47c972`), BDS:
the 45-degree wall case and sneak release no longer fall. Slab bridging fell
again; the maintainer noticed a slab sometimes goes into the cell the player
stands in, lifting the body by half a block, around the fall. Trace (four
falls): no server correction; the guard and the movement clamp both let a
full input through for a tick right before the client walked off, so both
judged ground ahead that was not there. Cause not yet found; the next trace
build logs the collision shapes around the feet.

## L-160 corner falls beside a wall (2026-10-12)

By the maintainer, trace build of `c73fc16` (DLL `0ef93b72...cbc7f796`), BDS and a local world: letting go of
sneak at an edge and slab bridging no longer fall. Building a bridge toward a
wall and pressing a key about 45 degrees into the wall a few times still fell.
Trace (five falls, same pattern): the box sat exactly on a block corner
(-140.700, -4.700), horizontal collision set, the step-back input pointed
diagonally into the wall and did not move the player; when the keys were let
go the client itself lost ground and fell, with no server correction.

## L-160 falls on BDS after sneaking (2026-10-12)

By the maintainer, trace build of `410353a` (DLL `ddfed996...0fa8f1ef`):
bridging no longer pulled back (`1870ca1`), but the player sometimes fell on
BDS only (not locally). Reproduced as: sneak to an edge, then let go of
sneak. Trace: vanilla sneaking stops with the box exactly touching the block
edge (feet at -107.700, -0.700); one to two ticks after sneak was released
the server's correction had the player falling (ground false), although the
client's guard braked. Slab bridging was not reproduced (no materials;
`/give` had been refused earlier; the reason was not confirmed, and the
player is now listed as operator in permissions.json).

## L-160 input state rewrite (2026-10-12)

By the maintainer, trace build of `68d2211` (DLL `f7580692...15edea76`), test
BDS and a local world: walking and sprinting into an edge stop, walking
diagonally along an edge works, jumping off works. Bridging (placing blocks
while walking backward in one direction) sometimes reached the true edge and
sometimes was pulled slightly back, on BDS and locally. Trace: a full reverse
brake (move 0,1 -> -0.04,-1.00 at 0.046 blocks/tick) pushed the player back
about 0.1 block; and all-or-nothing input left slow approaches short of the
edge.

## L-160 Edge Guard on BDS, trace (2026-10-12)

By the maintainer on the local test BDS 1.26.51.1 (official, at
`C:/Users/kenke/Documents/Lamina/bds-1.26.51.1`); in a local world the
build stopped at edges. `1153107` (DLL `20c644ab...41406eca5`): on BDS the
player still stopped for a moment and fell. Trace build of `2391912` (DLL
`331e1c5c...ece720e`, `--edge_trace=y`): at the edge the guard changed the
move vector from 0,1 to 0.03,0 for both the raw and the input state, and the
sent `PlayerAuthInputPacket` carried move 0.03,0 but raw move 0,1 and the Up
key; the server's correction then put the player 0.7 blocks further along
(-79.078 against the client's -79.790), and the player fell. So the server
rebuilds the vector from the sent input state, not from the packet's move.

## L-159 follow-up and Edge Guard on BDS (2026-10-12)

By the maintainer, local, `e93606e` (DLL SHA-256 `7f5d6086...a8cbc18b`):
binding editor buttons on the edited row replace its switch and give it back
after Clear, Reset and Cancel; no overlap on a row with a stepper; Find by key
sits next to the key column, leaves "Key: C_" focused, Backspace shows all,
typing switches to a name search. All OK.
Edge Guard (L-160): works in a local world; on BDS the player stops at the
edge for a moment and falls within a second. Build and server version not
recorded for this observation.

## L-159 Find by key in Hotkeys (2026-10-12)

By the maintainer, local, `c4ac083` (DLL SHA-256 `99f5dce8...b71734c`).
Find by key with C lists Zoom, F3+G lists Chunk Borders, an unused key shows
the no-match line and Esc shows all again; the button does not overlap the
Action heading in a narrow window. All reported OK.

## L-158 shared label resolution (2026-10-12)

By the maintainer, local, `be0b1f1` (DLL SHA-256 `5b285364...0eec2eee`).
Settings search for the child label "速度を上げる" finds FreeCamera's speed
row; Hotkeys search still finds the speed actions; toggle toasts show the
feature name as before. All reported OK.

## L-155 action labels (2026-10-12)

By the maintainer, local, `3767542` (DLL SHA-256 `12ef4e1a...a01194`).
Settings search for Connected Textures, Inventory HUD and Used slots shows
translated action labels instead of `key.Lamium.*` in English, Japanese and
Simplified Chinese. Hotkeys screen not reported separately.

## 0.2.0 actual-play bug reports: L-155, L-156, L-157 (2026-10-12)

Reported by the maintainer with three screenshot attachments, all on the
latest Lamium 0.2.0. The repository's `v0.2.0` tag is
`b8fa42994e9c95b9684d8675c0e1b4f951731577`; the release checkpoint names
build source `6f77159` and release-ZIP DLL digest `71db2f0f...ae49c`.
The installed DLL's SHA-256, exact Minecraft/loader versions, trace options,
resource packs and local/server context were not supplied for these reports.
Do not substitute the release checkpoint digest for the reported DLL's hash.

- L-155: searching settings for `connected` displayed
  `key.Lamium.connectedtextures` literally. Attachment:
  `スクリーンショット 2026-10-12 013700.png`. Static source inspection
  confirms the action ID exists and its action translation key is absent
  from the English/Japanese and Simplified Chinese tables.
- L-156: with Vibrant Visuals enabled and glass below, lower bookshelves
  became dark. Attachment: `スクリーンショット 2026-10-12 014000.png`.
  The maintainer compared the look with the existing darkening issue
  (L-133); a common cause is not established. No same-scene Connected
  Textures on/off or other-graphics comparison was reported.
- L-157: Chunk Borders were sometimes hard to see. Attachment:
  `スクリーンショット 2026-10-12 015458.png`. Lines are visible, with some
  much less distinct over terrain than sky. Trigger and graphics mode were
  not specified; no agent runtime reproduction or cause is established.

Follow-up from the maintainer the same day:
- L-156: compared Connected Textures on and off; the dark vertical bands on
  bookshelf walls appear only with Vibrant Visuals and Connected Textures
  both on. Not seen with Connected Textures off under Vibrant Visuals, nor
  with Connected Textures on in the other graphics mode checked.
- L-157: the screenshot was taken under Fancy graphics, at night; other
  video settings were not checked.

These are observed failures, not successful regression checks. No fixes,
new DLL, post-fix validation or additional server coverage in this entry.

## Paused features shown to players (2026-10-11)

By the maintainer, local, `--unverified_game=y` builds. `1da4316` (DLL
`2cbcb1ad...a405bc`): FreeCamera's toggle toast says "partly paused", Night
Vision's "paused"; the settings header warning and its tooltip show; the
join toast was one line too long for the screen. `ddac26c` (DLL
`8efcdf7e27283218b857567f83f0ee91a2d4341d00475aa0a2f49f0b029c0bd5`): the join
toast wraps and reads whole; only "Some features paused" is in the warning
color. Reported as passing. Normal build restored afterwards (DLL
`5c70e133...602c9`).

## 0.2.0 regression checks, fail-open build (2026-10-11)

By the maintainer, local, `891b798` built with `--unverified_game=y` (DLL
`52ce90228783ae58c7c868de59f5b1ed0b87b636ac465cf1152f8ac5c747a17e`). No
crash. Night Vision had no effect and a real potion looked vanilla; Edge
Guard let the player walk off an edge; water fog, rain and the nausea swirl
stayed while particles and boss bars still hid; Connected Textures left
glass borders; FreeCamera kept vanilla underground culling and otherwise
worked; schematic ghost blocks were absent and entities were dashed frames,
while frames, the Check list, Materials, area saving and map marks worked.
The log has one "stays vanilla" line for each of the nine capabilities in
GAME-UPDATES.md (forced by the build option, version 1.26.51+0559ac5), no
errors. Normal build restored afterwards (DLL `625e186e...ac5403a`).

## Info HUD over the position line (2026-10-11)

By the maintainer, local, `f822b70` (DLL
`4ad9cc074e1fd375667f6b9895d3fd6e0dd6bf97e114930f230bb6c0e25af720`): with
Show Coordinates on and the Info HUD at its default place the game's
position line is not drawn; moved away, the paper doll and the position
line return (L-145). Reported as no problems.

## Glint in previews and the Info HUD over the paper doll (2026-10-11)

By the maintainer, local. `77ceea4` (DLL `4e6a6887...dbca654`): still no
glint in the shulker preview for a pickaxe, an enchanted golden apple and
a shield. `df79b8d` (DLL
`a5fa642172c1a3bed37b0e9406348c9fd4d3b50c2aef555b436222ae89ccfc9e`): the
enchanted pickaxe, golden apple and book show their glint in the shulker
preview (L-142); fence gates, other blocks and tool durability bars in the
same box correct. Enchanted leather armor shows see-through undyeable parts
there (unenchanted leather does not): known cost of the direct path. The
paper doll is not drawn under the Info HUD at its default place (L-145);
the game's position line (Show Coordinates) still overlapped it, so L-145
also hides that where covered (next build).

## L-141 to L-144 on `9c173b7` (2026-10-11)

By the maintainer, local, `9c173b7` (DLL
`2d59d5d2131510f8d8d7b26e32307430dce9d6e488d304811f46771c047b19fb`). Passed:
Debug View hides the game's position line and paper doll and they return
when it closes (L-143); the HUD layout editor shows the hotbar (L-144);
overlapping placements no longer flicker and hiding the earlier one shows
the later one's ghosts there (L-141). Failed: enchanted items still show no
glint in the shulker preview (L-142, round 2 follows).

## Zoom Wide-angle view, world map names, Files names (2026-10-11)

By the maintainer, local. On `376cfae` (DLL `1ff38c03...4cbbef4`): long
schematic file names end before the Size column; the dark plate makes world
map names readable on sand. On `9e675cc` (DLL
`6f5d801c72ffa069d3b6d52e6d66e052c17f835a6fc9b18498a0a713624dc841`): world
map names show for the hovered and selected mark only, "Always show names"
shows them all; Zoom without Wide-angle view stops at 1x, with it goes to
0.5x, turning it off after a wide level reopens at 1x (L-140). Reported as
no problems, not itemized further. Found meanwhile: ghosts of two
placements on the same cells flicker (L-141).

## 0.2.0 regression checks, normal build (2026-10-11)

By the maintainer, local, on the 0.2.0 release ZIP's DLL (`2aea267`, DLL
`1854bc418b38c9172b11564a44328360bc769e9de7d66f26fd6dd180fe02f125`, trace
off). Check 1 of the 0.2.0 regression checks (schematic ghosts and saving,
Shapes and the breaking region, map layers and waypoint actions, the
settings screen, the gated paths on the verified game) reported as passing
("almost": no problem with the listed items). Not itemized per step. Two
display problems found while looking again: long names in the Schematics
Files tab ran over the Size column; waypoint and placement names on the
world map were hard to read on bright terrain. Fixed in `ca83014` (names end
before the Size column) and `376cfae` (dark plate behind the small names),
not yet checked in game. The fail-open build (check 2) was not run yet.

## L-96 shading on split faces before 0.1.9 (2026-10-11)

By the maintainer, during the 0.1.9 smoke test. Split faces showed dark
smudges at night (gone with Night Vision): first on stacked sandstone and
bookshelves near glass, later only on glass panes. Bilinear corner shading
was replaced by the drawn triangles' blend (`21cff69`) and copies took the
first draw's normals, material data and facing (`7cc3d50`); sandstone and
bookshelves stopped showing it. Panes kept a darker half of each block,
depending on the view direction (facing south at east-west panes: west half;
facing east at north-south panes: south half; facing north or west: none),
also in daylight; appending copies instead of redrawing (`f530e1a`) and one
quad info per face (`20b9059`) did not help. `4ef3d51` (DLL
`8ddf70b6e122ad2b448a3f527509da1dc15c1221d5e67b2629be5fd88d8741da`) draws
panes with the inward texture again: no dark halves; stained glass blocks
show none from any side; joins correct. Stained pane crosses show seams to
their arms with the feature off too (vanilla). Follow-up: L-133.

## Vibrant Visuals regression: distance fog (2026-10-11)

By the maintainer. With Vibrant Visuals the world flickered and turned magenta
on 0.1.8 (not on 0.1.7, not in vanilla; other instance 1.26.51.01 LIP), with
Connected Textures off. Feature-skip diagnostics (`b1777e9`,
`--feature_skip=y`) narrowed it to Effect visibility, and turning off only its
distance fog child fixed it live. `c9f579b` (DLL
`9951ca5d9eaddafe4736960fd318cb883a223c715ac2a7e3ebaaebbaedc58fe0`) keeps
vanilla distance fog at graphics mode Advanced and up: no magenta or flicker
under Vibrant Visuals with the child on; in normal graphics distance fog still
hides.

## L-96 glass panes drawn split (2026-10-11)

By the maintainer, local. `17cedc6` (DLL
`f18bfc6feaa192754da56be3f24aa7139a2d5d455a15212a01cc7c3731918125`): rows,
stacked walls, mixed shapes stacked, crosses and T shapes and panes against
blocks look right in detail; no stretch while placing; joints unnoticeable
from afar; about ten crossing 61x61 pane walls in several directions caused
no felt slowdown and almost no FPS change.

## L-96 bookshelves, sandstone and the split method (2026-10-11)

By the maintainer, local. Crop trial `ac3198c`: bookshelf dividers and the
sandstone band went away, but sandstone looked stretched and placing glass or
bookshelves showed the stretch. Split method: `ec5aa69` joined sandstone
without stretching but darkened the first block's band area (corner shading
not interpolated), fixed in `b430199`, which also split glass and
bookshelves (no stretch while placing, no gaps or steps, no slowdown near a
50x50 wall). Far glass still showed its joints (back when moving away);
`4789cb1` rebuilt every dimension's chunks and changed nothing there;
`a339a6d` (DLL `390ab3f08faa059059693bd9aac2ae26cb2ff2cf131eeb55bb3c9e56eb49f4ba`)
filled joined glass edges from the middle and the far joints faded. Sandstone
copying middle rock (`4789cb1`) was judged unnoticeable; bookshelves stay
with the adjacent column (aligned books up close; fine at a distance).

## L-96 Connected Textures for glass panes (2026-10-11)

By the maintainer, local. Trial rounds `52fb6eb`..`dbc16e0` (`--ctm_trace=y`)
found the method: whole-texture trimming joined east-west rows only, cut
borders on arms with nothing above, and folding the thin faces by collision
boxes left gaps and squares; per-part adjustment on the built mesh
(`dbc16e0`) passed. Feature build `3e76e7a` (DLL
`21b8e049b28eb94bd2a0751d1b14af8ab8087bda7701035e913caa01d756618f`): rows in
both directions, stacked walls, an L under a single pane and the reverse,
crosses and T shapes, and panes against blocks look as in the trial; off
restores vanilla panes and glass; stained panes join only the same color.
Not seen: Vibrant Visuals, resource packs, servers.

## L-96 Connected Textures for glass (2026-10-11)

By the maintainer, local. `8c3d21a` (DLL
`c9ee183914f0293401c3d45e38fa509eeb08335b8d2d86d3e82bc4f6df7a1058`): off at
first with vanilla glass; turning it on from the settings or the hotkey joined
glass at once without a rejoin, and off brought the borders back; rejoining
with it on, the Nether and placing and breaking glass kept the joins right;
switching caused no hitch and no terrain flicker. Not seen: panes (not
covered), Vibrant Visuals, resource packs, servers, an unverified game
version (the gate itself).

## L-96 connected glass spike (2026-10-11)

By the maintainer, local. Trace build `b3d93c7` (`--ctm_trace=y`, DLL
`24414651ebab05ad2318505e159a9dd7e7865746197f6f5d2c1fbcecc69397b7`), after
rejoining the world: the inner lines between glass blocks disappeared and the
outer frame stayed, in walls, floors and other arrangements; same-color stained
glass joined and different colors kept their line; about 50x50 glass caused no
noticeable slowdown. The log showed the face position equal to the world
position and 16x16 source images. Not seen: panes, toggling without a rejoin,
Vibrant Visuals, resource packs, servers.

## L-120 Debug View entity counts by kind (2026-10-11)

By the maintainer, local and a server. `75620ed` (DLL
`9b5c42ab33a131dbbfeea8a3b91b4c8b4f3f81948e78a67ee595d543ebb51c67`): the
breakdown line under the entity count adds up to the total; zombies count as
hostile, cows and villagers as passive, dropped items as items, arrows and
experience orbs as other, and the server's entities looked right too. The
type list closes the left column with the top 5; Top 10 and Off switch it;
the Java F3 label style shows identifiers.

## L-127 inventory HUD and used slots (2026-10-11)

By the maintainer, local. First build `1789bee` (DLL
`48afd205db4d55a05515df14f330e723236638389eafab26de243388efbff563`): grid
contents, counts, durability bars, the hotbar row and the counter matched the
inventory; the warning color at 0 free; layout editor moves, scale and
backgrounds; the offhand slot unchanged after sharing its glint, bar and
count drawing. Review changes followed (bottom-right defaults, frames on
every slot, card / none, even card padding: `603ca1b`, DLL
`7affe07e892cfb9b56d5be9354bd7bd3e91ca3237d9da5527d5b9f350ac53ce5`, checked;
one hotbar switch per element: `6a58268`, DLL
`fbd0a68fde281a8ceea8fcca2a0a601f027a7ae00049252dba0ba21d807c0e8e`). Final
build `7906581` (DLL
`2c8c3f949dbebe51453aad1415fdf7fd9aa23a12a600374536e4b82b906a3b38`): "Used
slots" shows used/total such as 13/36 at the bottom right and turns the
warning color when full; slot frames around the whole cell leave room around
the icons; the new names read naturally in Japanese and English. Not seen:
servers, Simplified Chinese text, 75 % and 150 % after the frame change.

## L-131 player list dimensions and permission marks (2026-10-10)

By the maintainer, `4ad4e18` (DLL
`bc4c9cf0eb309d0bca6843b7b5b32f703aac84d2e79a042c58eadff70a65a30c`). BDS,
the maintainer moving between dimensions: players in the same dimension show
the dimension icon and distance; a never-seen player stays blank; after
moving, players last seen elsewhere show that dimension faded without a
distance (Overworld, Nether, End). Everyone is a member there, so no marks;
with "Member marks" on, every row has the star. Singleplayer: your crown.
PC-hosted LAN world with a phone: changing the phone player to visitor,
operator, member and custom drew the matching marks.

## L-128 player list (2026-10-10)

By the maintainer, local and BDS. `d8c16d2` (DLL
`624ba34f416bf4c9ab5d6aa7f2dc185f81cae539419a5d3a886d6904c092cb03`): Tab shows
the list and hides it on release; singleplayer lists you with face, green
name, platform and a host crown; the dimension icon was missing and the crown
sat slightly low; on BDS other players listed by name with distances for
nearby ones and no crown (the server is the host); hidden while chat is open;
movable in the layout editor; overlaps the target card and Debug View.
`f7e6f87` (DLL `4d8b92f592ee57c37b14c31344317e49d22cc2637fbd4f79c7fe9361ec9e253a`):
dimension icon shown, icons on the text line, the list drawn over Debug View
and the target card hidden while Tab is held; the per-line background was not
offered. `ea0ade4` (DLL
`cddf9ab04b647bcdd879829c359bbfe0f89a15f036e3cd10246d5579734655ef`): per-line
background offered, one band per player and on the header; no problems found.

## L-130 English name search (2026-10-10)

By the maintainer, local. `8488e3b` (DLL
`b099a9e319e9fc7e4ec402114c590459b0ba79df8e8b889b09d6d09ec2d06a32`): with
the game in Japanese, `plank` in the recipe book, `diamond sword` and
`oak_log` in creative search found their items; Japanese search unchanged;
Off removed the English matches. The help text was cut off in the settings
screen, and the log showed every English name empty (the description id
already ends in ".name"), so only identifiers had matched. `3646a11` (DLL
`23a30e587fd59e8c8fffcc5c6b9e83fa2698284518a8311a08bec84c1b650b0a`): short
help fits; English display names found (`granite wall`, `bamboo mosaic`,
`smooth stone`); log "tile.planks.oak.name -> 'Oak Planks'". Worked with the
game in Japanese and in Simplified Chinese.

## L-123 body culling rounds (2026-10-10)

By the maintainer, local. Round 1 `8946d65` (DLL
`a730d876582eadf6bce4f728729e25fcd8369cf1e3b8da7c9e2b6324fcc4aea5`): elytra
still stretched with the body off screen near the camera; no change seen
elsewhere; the override log line never appeared. Round 2 `bae4bc9` (DLL
`1903410bcc703d62a6a53882d899ec10d5d6ea39b2a729e652c5660ecacb1462`): log
"isAABBVisible 1751 calls (10 near the body), cullerIsVisible 249 calls (22
near the body)" and "kept visible ... (point)"; elytra still stretched.
Both reverted.

## L-129 tooltip alignment (2026-10-10)

By the maintainer, local, normal build `3cffc44` (DLL
`515f9d356d23ca6ff4bf6c8d472c929749414c37d605780c501e49e2ff326b6b`): the
locked trade tooltip's frame and text no longer shift apart while the
pointer jitters.

## L-129 locked trade tooltips (2026-10-10)

By the maintainer, local, normal build `c7beed9` (DLL
`ffa939332776622f9c5cf44ef604b90b23e9929018fb4b8f36e75b1182f998d4`). Hovering
a locked level's sword, axe or enchanted book shows its description
(enchantments), payment items too; it disappears when the pointer leaves;
unlocked trades keep vanilla's hover text with no Lamium frame on top.
Found: while the pointer jitters over a locked item the frame and the text
shift apart; positions rounded to whole units after this check (`3cffc44`).

## L-129 all trader levels (2026-10-10)

By the maintainer, local, normal build `0a41937` (DLL
`75ada29991e5914e7de833b5df034b1586639de070fe282a3e8d652cc4166fc4`). A level 1
villager's trade screen lists levels 1 to 5; levels above the trader's show
vanilla's locked look and cannot be selected; Off restores vanilla's list.
Also checked on the same day: the offhand icon on the hotbar's half unit
(`4f4bb20`) lines up with the hotbar. Requested: hovering a locked trade's
items should show their description (enchantments) as unlocked ones do;
vanilla shows none there.

## Shield glint variants and offhand offset (2026-10-10)

By the maintainer, local, trace build `75433f6` (DLL
`953bf3e4f07429554e3a5ff0100d42ed5fd28d27771767b905702bfc054b9396`). Three
glint variants for an enchanted shield in the shulker preview (chunk 4/5
directly; chunk 4 in a material-5 batch with the glint texture in the second
or the first texture slot) all drew the shield without glint. Screenshot:
a totem in the offhand slot sits about 1 px lower than in the hotbar.

## Shared-mesh item icons everywhere (2026-10-10)

By the maintainer, local, normal build `cbaacfc` (DLL
`e71d9e3cf3ad34ebbdcec9cbbd54106431cb98c84a1dcebdf6199deb31cefb2d`). Fence
gates and leather armor's undyeable layer show in the shulker preview;
leather in the durability HUD looks like the vanilla slot; offhand slot,
target card and other icons unchanged in place and size; no flicker, no
crash. Noted: the offhand slot's item may sit about 1 px lower than the
hotbar's (not known whether this build caused it). Shield glint not
addressed.

## L-119 shared-mesh batch in the shulker preview (2026-10-10)

By the maintainer, local, normal build `6a32266` (DLL
`9c532aa3baf4ebc3b3ab740194088354922c9d2a65dbf910d84c856603dde39d`). With
block items drawn through a shared-mesh batch built like a slot's, the oak
and bamboo fence gates show in the shulker preview, next to the fences,
snow block and sandstone. No flicker in the inventory and no crash while
opening the preview repeatedly. Shield glint and leather layers not changed
by this build.

## L-91/L-119 icon route trace (2026-10-10)

By the maintainer, local, trace build `e329b55` crashed when hovering a
shulker box: `InventoryItemRenderer::getItemRenderInfo` called from the
`renderGuiItemNew` trace hook on a preview stack (access violation, crash
trace `trace_2026-10-10_16-54-18.log`). Trace build `7dac7d5` (DLL
`c2c2d434a46c66279802bc0d1750d1c6a2fdcbf1087f22b9f1979ece00bd3ad8`) without
that call ran: the inventory slots showed gates, the shield glint and
leather layers; the shulker preview still lacked all three. The log showed
no geometry atlas calls, block items in shared-mesh batches (type 1) and
shields in default batches with up to three passes.

## L-119 depth and winding experiment (2026-10-10)

By the maintainer, local, trace build `1e46a3e` (DLL
`64178dd7ca464aad8126e8470a039df465311b9eeb6dfd9468cf0d20530f27b0`). In the
shulker preview, the oak and bamboo fence gates were missing in all three
rows: normal; without a slot background at z 40; at scale -1. Fences, snow
block, sandstone and door showed in every row; at scale -1 the blocks showed
their inside faces. Reverted.

## L-119 block chunk with alpha 1 (2026-10-10)

By the maintainer, local, trace build `696a11c` (DLL
`128d2591b1502a07959440c1dce8d43b0c44b9ab1cc1f5e4bc31415059811df4`). The
shulker preview drawing block items as chunk type 0 with alpha 1 showed the
fences, snow block, sandstone and door, but not the oak and bamboo fence
gates: the same as `renderGuiItemNew`. Both preview experiments reverted.

## 2026-10-10 follow-up: camera angles, block chunk icons, trade dump

By the maintainer, local, trace build `6e29917` (DLL
`6f706dd728b423b072887745a705909f7ba58a666a057ef322266dbf735f55ea`).
- L-124: camera angles follow the camera in the Info HUD and Debug View;
  the label reads "Camera".
- L-119: the shulker preview drawing block items as chunk type 0 with the
  slot's alpha 0 showed no block icons at all (gates, fences, stairs,
  blocks); the door (item route) still showed. Next try: alpha 1.
- L-129: the SNBT dump of a level 1 weaponsmith held 9 recipes, tiers 0-4,
  and `TierExpRequirements`.

## 2026-10-10 batch: FreeCamera readouts, hit exit, Night Vision, icon and trade traces

By the maintainer, local, the 1.26.51.01 instance without LIP, on the trace
build `d2f288c` (`icon_trace`, `trade_trace`; DLL
`4647a9688a41c8113a8d6ba527f7ea06cde94cb2f2da80020f91426ff188f632`).
- L-125: during FreeCamera the Add waypoint key filled the camera's position.
- L-124: Camera and Player lines showed with correct positions; the camera's
  angles still showed the body's (the session angles stay at the start
  pose). The English "Cam" label read oddly. Both changed after this check.
- L-126: hits ended FreeCamera ("FreeCamera left: the body was hit (event 2)"
  in the log, four times).
- L-121: at Brightness 0 % with Night Vision, the option removed the dark
  area around light sources and Off gave the vanilla look. The log showed
  the game's light gamma as 1 at that time, so whether `mGamma` is the
  Brightness value is not confirmed by the log.
- L-119: in the shulker preview the oak and bamboo fence gates showed no
  icon while the bamboo and cherry fences did. Vanilla slots draw fences,
  gates, stairs, walls and trapdoors as chunk type 0 (alpha argument 0) into
  `_renderGuiBlockTypeItem`; Lamium's `renderGuiItemNew` uses chunk type 12
  (alpha 1) into the same function for gates and fences alike.
- L-129: one villager trade screen; `UpdateTradePacket` tier 0, size 0, new
  screen, economy trade. The dump used `toString`, which only printed
  "2 entries"; changed to SNBT after this check.
- L-123: elytra on the inventory model drawn wrong with the camera near the
  body but the body out of view; drawn right with the camera far and the
  body in view. The body being culled, not the distance, matters.

## Inventory and hand features on a server (reported 2026-10-10)

By the maintainer, reported in chat; build, hash and server not recorded.
Weapon Switch (L-67), offhand swap (L-94), Fake Offhand item use (L-95),
Hand Restock threshold/order and inventory-screen transfer (L-102, L-103)
and fixed-slot fetch (L-97) worked correctly in everyday use on a server.
Every feature was used, but not along the detailed check steps; the
maintainer accepts that some cases may be missed and will report them.

## L-89 distant players and L-104 map follow-ups on a server (reported 2026-10-10)

By the maintainer, reported in chat; build, hash and server not recorded.
Distant players from the Locator Bar state (L-89) and the L-104 follow-ups
(black areas within render distance filled by the client's missing-section
requests; teleport from the map) were already checked and fine. No details
beyond that were given.

## L-57 counters and L-60 waypoints on a server (reported 2026-10-10)

By the maintainer, reported in chat while tidying the backlog; build, hash
and server not recorded. Debug View's entity, chunk and particle counts
looked fine on a server (L-57). The map's remaining checks, including
waypoint storage per server address and port, were fine (L-60). No details
beyond "no problem" were given.

## 0.1.8 release smoke test (2026-10-10)

By the maintainer, local, the 1.26.51.01 instance without LIP, on the DLL
from `Lamium-0.1.8-client-windows-x64.zip` (`e175eed`, DLL SHA-256
`1e2f4e6fe6f4657a8a1f7f18a3d067a32f9a100e3875deaa45886b537ae55a2c`, ZIP
`b99f9f2798da94745ab8e6c863ebf397856617f1f722febb13b4c810e300da4b`): the
mod loads, settings open with 0.1.8, and distance fog, Fake Offhand
fireworks and the schematic ghost fixes work. Not a full regression.

## L-93 Waterlogged ghost stairs, crops and farmland beside water (2026-10-10)

By the maintainer, local, the 1.26.51.01 instance without LIP. `62bff77`:
water in waterlogged ghost stairs no longer shows through the stairs'
covered sides; ghost beetroots no longer flicker; distance fog starts
selected without saved settings. Left there: the pool inside the stairs
still showed the flowing texture, and farmland beside schematic water
flickered on its side. `b40c543` (DLL SHA-256
`77a9f13dfb74295bf4ee6f32abd603b6ad77ef79dc44200db051cd6daca1d4d8`): the
pool shows the still texture (its edge slope kept), a source at a drop
still shows the flowing one, and the farmland side no longer flickers.

## L-122 Fake Offhand fireworks and L-118 distance fog (2026-10-09)

By the maintainer, local, `34c3593` (DLL SHA-256
`39d27e14a3ae4273d9a846cba9fe193d609ff3702e03f9abf7d034cd418aac2b`, the
1.26.51.01 instance without LIP). L-122: with a firework in the Fake Offhand
slot and the right-click binding, opening and closing doors and placing
blocks (the last one of a stack too) launched no firework; fireworks while
gliding and aimed at a block still launched. L-118: with Hide effects on and
Distance fog selected, the fog went away on land (day, night, rain), in the
Nether and in the End, and came back when switched off, in the normal
graphics modes. Under Vibrant Visuals it had no effect (as Night Vision
there); left for later. No problems found. Seen in the same session, in the
LIP instance on `c48e59c`: water in waterlogged schematic stairs showed
through the ghost stairs' sides and looked flowing; ghost beetroots
flickered (both addressed in `62bff77`, not yet checked).

## L-93 Schematic lines and mistake faces under Vibrant Visuals (2026-10-09)

By the maintainer, local, switching between Fancy, Simple and Vibrant
Visuals. `fce6057`: placement frames on `selection_box` with the current
shader color stay light blue under Vibrant Visuals (same width). `3995e2a`:
every schematic line keeps its color there (shapes, red, yellow, orange,
selection, entity frames); mistake faces colored; normal modes unchanged;
mode switches safe; but faces flickered, and after closing the inventory
faces and lines glow until the player moves (shapes too). `c363475`
fixed the mistake faces' flicker, but read the overlay material's
`twoSided` the wrong way round: selection corners kept flickering under
Vibrant Visuals and showed only lines on opaque blocks in Fancy and
Simple. `b7e24c2` (DLL SHA-256
`da26fa54ad58d658944b89bd2dca9074067a4d42143330af097bbeaa88648bcd`): corner
faces visible in Fancy and Simple on any block, flickering under Vibrant
Visuals no more than the mistake faces (about like shapes); waiting-column
floors and the shown point's faces visible in both. Still: ghost faces
untinted and the honey ghost black under Vibrant Visuals.

## L-93 Schematic ghosts looking straight down and under Vibrant Visuals, B5 (2026-10-09)

By the maintainer, local, `b55aa03` (DLL SHA-256
`eb26e5995c06a6c7d2ec26492a2537cfe87cf22a1d9cda569f146a181394bce6`).
Looking straight down onto and from inside ghosts: no change in brightness
or color, nothing missing or flickering. Vibrant Visuals: no crash; ghost
blocks, entities and block entities draw with their shapes, but without
the light-blue tint (they read almost like real blocks), Lamium's outlines
are thin black lines, and the honey ghost is black. The structure block's
own outline keeps its color and width there. Also seen: Lamium's Night
Vision changes nothing under Vibrant Visuals.

## L-93 Schematic ghost performance, C (2026-10-09)

By the maintainer, local, several large placements (`desert village`,
`terrain big`...), standing, placing blocks and flying inside them, with
the `schematic_perf_trace` builds (F3 for fps; 60 fps cap). Schematics off:
a steady 60. Start (`3b247fb`): 30-50 fps; the ghost pass took 5-6 ms a
frame with nothing changing and 7-12 ms while building or flying, single
16-block sections up to 21 ms. `8835802` (faces in one pass with one light
setup, outlines within 48 blocks, rebuilds limited to 3 ms): 40-55 fps.
`2345212`/`8efd52f`: the outline distance is a setting; changing it
crashed once (a static block pointer, fixed), then 0 and 192 drew far
outlines without crashing. `2521ec2` (8-block sections, kept wanted
list): 35-55 fps, fewer dips, rebuilds at most 2-5 ms. `0cffb83` (DLL
SHA-256 `54073603ec98fdd8db8b125a222ec97271552965633e36174363ef421adff348`,
frames kept as one mesh): 45-60 fps, dips to about 40; the ghost pass about
8 ms a frame while working inside placements (faces 1.0-1.8, prepare 1,
verification scan 1, blended 0.6, outlines 0.5, hash checks 0.35); frames
follow moves, selection and hiding correctly. No visual change seen from
the 8-block sections. Not measured: servers, a single huge placement only.

## L-93 Schematic previews with block entities and entities (2026-10-09)

By the maintainer, local, Files tab. `d07d158`: in `mixture` and
`entities` the block entities (chests, ender chest, spawner contents
aside) and the entities drew in place, facing right, moving with turn,
zoom and layer peeling (peeled ones gone); entities carried the world
ghosts' light-blue outlines; `block-like` (block entities only) drew
nothing. `4b98908`: `block-like` draws its chests, beds, banners, signs,
heads and its two entities; previews draw no entity outlines; world
ghost entities keep theirs. A campfire's flames hid the block entities
and entities behind them (empty texels write depth); `3889c2a` (DLL
SHA-256 `a086c5ade2779faf6d38e759d7d9d69c93513c45e56f7a3468d5637aea0d5899`)
draws them before the mesh: visible behind the flames, stable (no draw
order flicker), still hidden by blocks in front, block brightness
unchanged.

## L-93 Schematic foliage tints (2026-10-09)

By the maintainer, local. A real mangrove leaf placed beside the preview
was green while the preview's mangrove leaves were gray; world ghosts of
mangrove leaves and vines were gray too. `b02aea7` (tint only the seasons
layers): no change. `dac35cb` (DLL SHA-256
`5131c4c4fee63e8e682cdea398b2bc9036f52dc752b5801a2372dffb39b341cf`,
foliage tint methods on still-gray vertices): mangrove leaves and vines
green in the world ghosts and the previews; grass sides not turned green,
grass tops unchanged. Log: vines tint method 1 in layer 11 sampled
0.44/0.65/0.30; mangrove leaves tint method 1 in layer 10 (a seasons
layer) sampled 0.69/0/0. Not understood: why the mangrove leaves now look
green (likely their seasons vertex colors are not gray, so this pass
leaves them, and the earlier seasons-only pass did not color them
usefully).

## L-93 Schematic preview biome tints (2026-10-09)

By the maintainer, local. `9b4699d` (cells tessellated above the build
limit around the player): no change, grass tops still gray. `d536c30`
(DLL SHA-256
`a5ff1d107922e61025a6b911efbebe27a6bd595507c4ace316fc139396cc64b9`,
cells at the top of the world inside its height, around the player):
grass tops and grass plants take the player's biome color in
`broken_village_house`, `terrain_water_lamium` (66x53x65) and `desert
village`; other previews unchanged. Unclear: mangrove leaves in `desert
village` look gray, not yet compared with a real mangrove leaf in the
same biome. Tints follow where the player stands, not where the file was
saved.

## L-93 Schematic block-entity ghost outlines (2026-10-09)

By the maintainer, local, `417ccd1` (DLL SHA-256
`b25c527133c839e7c2714f48b34a8928b4a6ca2be1b434dd4e552e9b84814514`),
`block-like`: the light-blue outlines of chest, bed, banner, sign and head
ghosts follow each block's own selection outline instead of the whole
cell, also turned and mirrored.

## L-93 Schematic block-entity ghosts at full brightness, B5 (2026-10-09)

By the maintainer, local, `block-like` at night and underground. `b245aae`
and `e6b6872` crashed the game at start inside the hook on
`BlockSource::getLightColor` (reading its `Brightness` argument: passed by
value in the game, by reference in this SDK's type). `c0cc930`, hooking
only `getBrightnessPair`, started but changed nothing. `ae1716d` (hook on
the position overload of `ActorShaderManager::setupShaderParameters`):
banners, signs and heads lit as by day, chests and beds still dark.
`9e9178b` (DLL SHA-256
`044f3bf2b23d88955590b87b50b20fa0d4cb1a0631fce96d28be68c5f29f9359`, also
the `BrightnessPair` overload): chests and beds lit as by day too; real
chests and beds still follow the world's light; start and world entry
fine. Not checked: looking straight down, Vibrant Visuals.

## L-93 Schematic waterlogging check, D (2026-10-09)

By the maintainer, local, `56c9921` then `edbd7f5` (DLL SHA-256
`31d0042ccb13ca51a5e75bd0994b6eb795ce0a6cdcfa9e45456989288b07b4ed`),
`submerged_lamium` on land. On `56c9921`: a fence or stair placed without
its water is listed as a state mistake and its target card reads
`waterlogged: false -> true` (now -> should be); a waterlogged block where
the file has none gets a yellow mark; placing it right clears both. But a
placed block missing its water got no mark and kept a ghost water shell,
which looked like real water. On `edbd7f5` it gets a yellow mark and no
shell; ghosts not yet placed still show their water; the other cases are
unchanged.

## L-93 Schematic block entity data, B4 (2026-10-09)

By the maintainer, local, `79198e1` then `438e24a` (DLL SHA-256
`4879826ce38a8baf1ea7bbf57c862df66f4b21dbfbddeb01f321ab3119a30e22`), the
vanilla export `block-like` (banners standing and on a wall, six head
kinds at angles, colored beds, standing, wall and hanging signs with text,
chests). On `79198e1` beds and signs showed color, text and angle; banners
the pattern but the wrong mount and facing; heads nothing. On `438e24a`:
banners mounted and facing right, every head kind drawn and turned right,
also rotated, mirrored and both; beds and signs unchanged. A copy of the
same things saved by Lamium and placed as ghosts looks the same as that
save loaded by a structure block (colors, text, patterns). The structure
block itself breaks signs and banners with nothing to stand on, and
handles beds with one half missing on its own way (top half only: drawn
whole without the lower half's collision; lower half only: not drawn).
Not seen: a container's items. The "cushion" mentioned on `79198e1` is
an entity (entity saving was off), not a block entity. Also seen that
day: block-entity ghosts (chest, bed, banner, sign, head) follow the
world's light at night and underground (B5; their brightness is read
through non-virtual light queries).

## L-93 Schematic preview translucency, closed (2026-10-09)

By the maintainer, local, `fe82509` (DLL SHA-256
`fa7f0af30193cf9ad6e39dd75605bcf8ca9dd655af1516345968de49c194b1c0`),
`submerged_lamium` compared with the world placement. With the
depth-writing block blend material back (and a liquid cell's quads still
ordered around its surface), stair tops, fences and the spawner's top
stand above the water as in the world. Accepted limits, all from empty
texels writing depth when the far-to-near order is off: seagrass shows
two of its four quad sides depending on the angle, the water behind it
can be missing, and the stone behind a spawner is not drawn while the
spawner looks dry. The maintainer chose to stop here and move on.

## L-93 Schematic preview translucency (2026-10-09)

By the maintainer, local, Files tab with `water_and_lava`,
`submerged_lamium`, `stone_patch`. `d583e71` (fixed 0.5 UI units of depth)
drew nothing; the log then showed the UI pass maps UI z 0 to normalized
depth -1 and z -1000 to +1, so `fd22671` reads the matrices and places the
build between 0.25 (nearest) and 0.75: the preview drew again with
translucent water over the floor and waterlogged blocks. Two meshes
(solids, then liquids) made a rail under water flicker (draw order);
one blended mesh far to near (`b5a6db5`) fixed it, lava made opaque.
The block blend material wrote depth for seagrass's empty texels (its
other quad and the water behind went missing); the beacon beam material
without depth writes (`7ae2b4e`) fixed that. Still wrong: in
`submerged_lamium` parts above the water (stair top, fence, spawner top)
look submerged in the preview, unlike the world. Not the cause: ordering
a waterlogged cell's quads around the surface (`485d829`, no change) and
vertex alpha (logged 255 everywhere; `91036fc` reverted). Stopped after
these two rounds to report.

## L-93 Schematic blended ghosts after the world's translucent blocks (2026-10-09)

By the maintainer, local, `a926f09` then `eb0a99b` (DLL SHA-256
`5fb26a8c726d4eaf974c96a0f5110c7f2692f12a1e5e7613cbd29f91d59519aa`). The
blended mesh (stained glass, honey, slime, liquid shells, every mistake
mark) is drawn in the block entities' alpha pass with the beacon beam's
material, without depth writes. Log: the pass runs once a frame (599 of
600 frames), `beacon_beam_transparent` found. Seen: yellow marks over real
water no longer flicker and the real water inside shows; real honey,
slime or glass placed by mistake beside a honey ghost no longer flickers;
blended ghosts keep their strength; ghost liquids inside and beyond real
water still show. `a926f09` made ghost water above a ghost floor flicker
near the camera (the shell's faces against opaque ghosts were kept there);
fixed and checked on `eb0a99b`. Reported then, older: from inside a
non-full ghost (door, spawner), its faces and a neighboring non-full
ghost's in the same plane fought; `361a0f6` keeps one face of any such
pair near the camera: no flicker, but a door or spawner lost some of its
own faces seen from inside, and a stair still flickered from inside (its
two half-height faces). `d20d0aa` draws only front faces in the camera's
cell: both checked fine. With `361a0f6`'s pairs, a spawner's face beside
a half block was missing from just outside but shown from afar; with only
opaque pairs (`064eef2`) it showed but flickered there, as real blocks
also do (a little, less often). The maintainer chose the pairs back on,
behind a switch.

## L-93 Schematic liquids, B3 (2026-10-09)

By the maintainer, local, rounds on `0f164ee`, `e1a74a2` and `6fbaa3a`
with `water_and_lava`, `submerged` and `submerged_lamium`, on land and in
real water. Seen: water and lava shells whole and textured from the
atlas, no gaps; waterlogged ghosts show their water; no shell where the
real water already is; flowing water and lava slope like the real ones
(`e1a74a2`); sloped tops, including sources beside a flow, switch to the
flowing texture over exactly the right cells, still pools keep the still
texture (`6fbaa3a`). That build sampled only half the flowing texture (it
looked 8x8 and fast); the fix, `96fe71f`, was checked the same day:
16x16, the real speed, the real direction. Also seen then: real water or
lava placed beside a ghost flow bent its slope and flow direction (fixed
in `be48f75`, checked the same day: real liquids beside a ghost flow no
longer change it, and no shell appears in real water), and a
yellow mark over real water (a different flow level than the file's)
flickered against a ghost liquid shell, swapping draw order each frame
like the marks over real honey or glass. Previews: liquids
and waterlogged blocks draw, but the preview's single pass keeps the
first fragment at a spot, so near water hides parts of waterlogged blocks
and the floor behind it (deferred, with the blended-material depth
question).

## L-93 Schematic ghosts by render layer, B2 (2026-10-09)

By the maintainer, local, rounds on `ba046d0` to `59fc02c` (last DLL
SHA-256 `cfdfcaeee522f26127ef6f9f6133fa420b30ee2c2ab20faab8fdf60163817c69`),
`mixture` and its mirrored copy. Honey blocks now draw their real look
(opaque inner cube, blended outer) instead of an outline, brightness the
same at night; nothing else changed look (ordinary blocks, glass, panes,
doors, fences, inside view, mistake colors, previews). Found and fixed
along the way: a smooth stone slab ghost drew full height after a honey
block was tessellated (the tessellator's current shape carried over,
`99b04f6`); the honey ghost's outer face fought with a neighbor's mistake
mark (mark faces against a ghost that fills that side are left out,
`e9a0285`, `59fc02c`) and swapped draw order with it from frame to frame
(marks moved into the sorted blended mesh, `a90251d`); that move hid a
real honey block placed by mistake inside its mark, and ghost faces
against a real honey block were dropped (`59fc02c`: marks over real
blended blocks keep the old material, honey and slime no longer count as
hiding faces). Seen on `59fc02c`: marks show in full beside trapdoor
ghosts; a real honey block placed by mistake shows with its mark and its
neighbors' faces stay. Still flickering: a real honey, slime or glass
block placed by mistake next to a honey ghost, whose mark (old material,
separate draw) swaps order with the blended ghost. Ordinary and partial
blocks next to honey ghosts are stable.

## L-93 Schematic door halves and mirrored entity facing (2026-10-09)

By the maintainer, local, deployed `a32a49f`, DLL SHA-256
`3092a9d7ee32eca948e385d5f75f8ab62d57e4fe4df07ea1dab93f82fff454bb`.
Every item of the checklist passed: in `submerged_lamium` the waterlogged
jungle door whose upper half is outside the file now draws its half, also
turned and mirrored, matching a structure block; doors with both halves
draw as before, and placing the real lower half leaves no displaced ghost;
the `mixture` armor stand matches the structure block under the X mirror,
the Z mirror and mirror plus 90 degrees; unmirrored entity facing is
unchanged; the Files/Check preview draws the door at the area's edge.
Not checked: a bed with one half outside the file, redstone.

## L-93 Schematic mirror axes swapped for block states (2026-10-09)

By the maintainer, local, deployed `54c6a8a`, DLL SHA-256
`45192484611150f3771328bcd5ea62267291f588e79ba71d17e53057a799fe4d`.
`mixture` placed by Lamium and by a vanilla structure block side by side
with the same mirror. Before the fix (`9993c78`, Z mirror): stairs high on
the wrong side, the glass pane joined toward the outside instead of the
glass, chest and ender chest faced the other way, the fence joined the
air instead of the spawner. After: Z mirror matches the structure block;
X mirror and mirror plus 90 degrees match for every block. Placing blocks
on mirrored and turned placements removes the right ghosts, and wrong
blocks or states are marked as such. Unmirrored ghosts look as before.
Not matching: under the X mirror the armor stand (stored yaw 0, facing
south) faces the opposite way from the structure block's. Lamium keeps it
facing south (a true east-west reflection); the structure block turns it
north. Which one Lamium should follow is open.

## L-93 Schematic neighbors spike, B1 (2026-10-09)

By the maintainer, local, deployed `9993c78`, DLL SHA-256
`90652047eb491bf7e45ab11b9f424930b73ca11d01fc270d2c105340b2a8d659`
(`SchematicRegion`, a `BlockSource` subclass the tessellator reads
through). Samples `submerged`, `submerged_lamium`, `mixture`, `no_mesh`.
Seen: the world loads and placing ghosts does not crash. Doors draw both
halves with the right hinge side and open state; beds draw head and foot
(color still not the file's, B4); fences connect to ghost fences and
blocks and grow no arm toward empty cells; glass panes and iron bars
connect to each other and a lone one is not a cross. No regressions:
plain block faces, the filled look from inside, one-layer view (top faces
kept), red/yellow mistake marks, Files/Check previews draw the same shapes.
Not right: in `submerged_lamium` the waterlogged jungle door's cell is
empty. The file holds only the lower half (2,1,5 in the top layer); the
upper half is outside the area, so the tessellator finds no partner (not a
water problem). With a mirror (Z in the screenshot), several blocks seemed
to face the wrong way, stair corners included; whether this is new or the
game's state transform (only stairs and chests were checked before,
2026-10-03) is not known yet. Not checked: redstone (no sample), stair
corners without a mirror in detail. Water and waterlogged blocks are drawn
as before (B3).

## L-93 Version 1 and 2 large saves side by side (2026-10-09)

By the maintainer, local, same files as the entry below. LHolo failed on
both version 1 Lamium saves, `terrain big` (122x39x203) and `terrain big 2`
(276x14x232), with its message that the vanilla StructureTemplate could not
load the structure (screenshot in the conversation). The version 2 copy
`terrain big v2` loaded there (entry below) and also loaded through the
game's structure block. So the version 1 header was the cause, and the
game's loader takes a 122x39x203 file. Not checked: 276x14x232 as version
2, servers.

## L-93 Large schematic as version 2 in another tool (2026-10-09)

By the maintainer, local. The agent copied the Lamium save `terrain big`
(122x39x203, version 1, two layers with an all-void second layer) as
`terrain big v2`: format_version 2 and the empty second layer removed,
every other tag unchanged (3,863,500 bytes shorter, exactly that layer;
both files pass the `LAMIUM_SAMPLE_STRUCTURES` round trip). LHolo read
the version 2 copy; earlier the original failed there at the vanilla
structure loader. So the version, not the size, was the likely cause.
Not checked in this round: the original again, and the game's own loader
on an area this large (it only runs right after a save).

## L-93 Schematic saves as version 2 with a vanilla-loader check (2026-10-09)

By the maintainer, deployed `d37bd6d`, DLL
`b697e13e49e45305c3f87794a6786d824d0a3bdd24dd1d6523a6be2b5d21d798`, Minecraft
1.26.51.01, local world. Three area saves; the log line and the file
headers agree:
- `stone_patch` 3x2x3, no liquid: version 2, one layer; "accepted".
- `submerged_lamium` 7x2x10 with waterlogged blocks: version 2, two
  layers (17 cells in the second); "accepted". Loaded in Lamium, the ghosts
  draw as before.
- `terrain_water_lamium` 66x53x65 (just over 64 in X and Z, 227,370
  cells, 720 waterlogged): version 2, two layers; "accepted". No pause
  noticed at the end of the save.
The game's structure block (load mode) placed the first two correctly,
waterlogged blocks with water and sign facing included; a door whose upper
half was outside the area broke, as expected. LHolo read all three; it
drew doors and water in a simplified way. Not checked: a save of the size
that failed in LHolo before (122x39x203 and larger), so whether that
failure was the version or the size is still open, and whether the game's
loader has a size limit; servers.

## L-121 Night Vision shading with Smooth Lighting (2026-10-09)

By the maintainer, Minecraft 1.26.51.01, local world, Smooth Lighting on,
looking down at a floor with a one-block hole (screenshots in the
conversation). The Lamium build was the deployed one of that day (hash not
recorded). Seen: with Lamium's Night Vision on, the blocks around the hole
turn dark blue in a wide ring; strong at Brightness 0%, weaker at 50%,
almost gone at 100%. The vanilla Night Vision effect (Lamium's off) looks
the same at 0%, and so does vanilla Minecraft without LeviLamina. So this
is the game's own behavior, not a Lamium bug. Not checked: other Night
Vision settings, light sources nearby, the Nether.

## L-114 shortened Check note and preview hint (2026-10-09)

By the maintainer, deployed `511cb35`, DLL
`911cbf65ce73288ebe9de38aa834b510cd210abd106157faa18f887e7e569db9`, Minecraft
1.26.51.01. Passed: the note under the Check counts and the preview hint
are readable at UI Profile 75% in Japanese, English and Chinese. At UI
Profile 100% they are cut off; the maintainer accepts that.

## L-114 preview inspect and the Check list on large builds (2026-10-09)

By the maintainer, deployed `ccaf63b` to `7b490be` (DLL
`cb84422196519deb670803fd2cdb30158fa5fb114bb225ecb35427b0638342c4`), Minecraft
1.26.51.01, local world, mixture and terrain big. Passed on `7b490be`: a
click in the Files preview lights the block and names it with its place; a
drag only turns; a click never turns the view, also after earlier drags;
the Check list keeps the nearest 2000 of each kind (wrong rows show on
terrain big), a picked mistake in the list is selected and scrolled to, one
outside the list is checked on the spot and "show in world" marks it; a
correct block says so; peeled cuts can be picked. Found on the way: the
list kept the first 2000 found, so missing blocks filled it and wrong ones
never listed (fixed); the note under the counts was cut off in Japanese at
UI Profile 75% (shortened after this check in `511cb35`).

## L-114 preview zoom limit and layer peeling (2026-10-08)

By the maintainer, deployed `771a84b` then `07937ed` (DLL
`ee773a037554f20bccfc41d3aa203b59ad8a9eaad1f8ea458c77fed32f47ae4f`), Minecraft
1.26.51.01, local world. Passed: zoom stops at the box from every side (the
UI scissor never clipped the mesh: GUI units, pixels and a committing fill
all failed); Shift+wheel peels from the side the view looks down on, the
side stays fixed after turning (a height cut seen from the side), the top
strip resets it without starting a drag; the Check tab keeps its colors;
waiting after a peel on large builds is acceptable. On `771a84b` the side
followed the view and the label click did nothing (both fixed).

## L-114 Check tab layout A and wheel zoom (2026-10-08)

By the maintainer, deployed `cdebe56`, DLL
`5f8b9b2c0af130a1aae045dbcba4a0a5831f98f11a5e56a542933f598ca6b625`, Minecraft
1.26.51.01, local world. Passed: the Check preview keeps its size while
rows and chips change; counts beside the selected mistake, readable, no
overlap with the buttons; chips change which kinds are colored; the wheel
zooms both previews with the order intact and still scrolls the list
outside them. Found: a zoomed preview spills over the screen (scissor not
applied to it). The maintainer finds every layer-control variant of
schematic-preview-layers.html awkward and wants the preview's scope
settled first (camera moves, layers, possibly a simple 3D editor).

## L-114 Check tab colored preview (2026-10-08)

By the maintainer, deployed `1c0b8bc`, DLL
`ace332d7fc5aceaae13254c0b4e0ca8ed1ef0eb9aa2a1769f05be9efc86c5ff2`, Minecraft
1.26.51.01, local world, mixture and desert village placements. Passed:
missing light blue, wrong red, wrong state yellow; the selected row's block
stays lit and the rest dims, following the selection; drag turns it without
other clicks; no flicker or hitch when colors change, also on the large
placement. Found: a trapdoor drew behind the structure block beyond it
(also in the Files preview; fixed after this check by sorting blocks
first); the preview resizes whenever the text above it changes height;
mistakes are hard to find in large or complex builds. The maintainer wants
another mockup round for the Check tab and how mistakes are shown.

## L-114 Files tab 3D preview (2026-10-08)

By the maintainer over seven rounds (`bd7f7f6` to `f2fbf49`, last DLL
`f7ee2e5512972ac1d858d849a5bb14fbc20ec110725bb2909f8755a7dfacdb9e`), Minecraft
1.26.51.01, local world, schematics from 3x3x3 to 122x69x112 (screenshots in
the conversation). On `f2fbf49`: real block textures, near and far right
from every side and from above and below, slabs, trapdoors and grindstones
where the world ghosts put them, fences as in the schematic, campfire flame
bright, brightness the same whether the world behind is dark or light,
shading by face direction, a slab next to a honey block not missing a face
(also fixed in the world ghosts), large builds shown within a second and
turning without the back showing through, drag turns the way the pointer
moves, "Drag to turn" hint at the top until the first drag. Not drawn:
entities, water, chest/ender chest/shulker box, honey block.

## Schematic entity follow-ups (2026-10-08)

By the maintainer, deployed `553f339`..`47534d7` (last DLL
`792fae30c673a3123c7eeb83ace993344f8a056a257c5897caea358be4ca6c00`, all
trace options off), Minecraft 1.26.51.01, local world. Passed: a real entity
of the same type on the next block no longer hides the ghost, one on the
same block does; dropped items get a frame about their own size; name tags
only over frames; Japanese name tags without colored fringes and with the
plate matching the text; English tags unchanged. Left: Japanese tag text is
dark gray (accepted for now, L-117). Not seen: Chinese after the last
change.

## L-115 entity ghost models (2026-10-08)

By the maintainer over about twenty probe and trace builds (`da315e7` to
`ef9bc54`, last DLL
`49d29a5afbce08e0ef40f8892ac269ac67457f7af9e327e7f161f6a5e8201648`, trace
option on), Minecraft 1.26.51.01, local world, schematics with many mobs
(screenshots in the conversation). On `ef9bc54`: models at the right size,
place and saved facing for armor stand, chicken, cow, creeper, witch (robe
outlined), wolf (mane at the shoulders), pig, polar bear, turtle, camel,
frog, wandering trader, stray, zombie, drowned; armor stand default pose with
the arms on the same sides as a real one, and no longer following a posed
live armor stand; horse, donkey, mule, llama, trader llama, villager in
light-blue faces in Fancy and Simple graphics (Simple flickers); outlines
on the model parts; no slowdown with many entities. Not right: sheep wool on
head and legs missing; wolf tail straight down into the body; drowned
sleeves flicker against the jacket. Not seen: mobs in water or flying,
projectiles, the trace-off build (same code without logging).

## L-93 screen review refinements (2026-10-08)

By the maintainer, deployed `4b0f531`, DLL
`aaf161572fbd2c9453f8438d72fbabdd58846d5a1ad8b5c56d3e94be60844f1a`,
Minecraft 1.26.51.01, local world. Passed: selected frame solid and others
dashed; Files headings as paths; Check filters as outlined pills apart from
the tabs; the Check pane split into the whole placement and the selected
position; the calculator button readable; per-column amount tips without
overlap; no entity tag under the Entities heading and no heading overlap.
Found: in Japanese the filter text overlapped the pill outline; with a
placement whose file is missing, Check and Materials showed "counting" for
ever while Placed showed "file not found". Both fixed after this check.

## L-93 screen review step 1 (2026-10-08)

By the maintainer, deployed `fb0500f`, DLL
`eab620753bb9b050a6df6970177f574bbfb495df5959d02e92a59f5c86a163be`,
Minecraft 1.26.51.01, local world, all trace options off (screenshots in the
conversation).
Passed: Placed list accent bar, progress column filling in after a moment,
progress in the detail pane, coordinates dropping when narrow; Files folder
headings, size/block columns, PageUp/PageDown skipping headings; Check
filter counts and filtering, wrong-state rows with the differing states on
the right; Materials sections, chest amounts on hover and selection, HUD
switch, missing slots, the ResourceCalculator link with the right items.
Not good enough:
- World frames show, but the selected one cannot be told from the others
  (the line material likely ignores alpha).
- Entity boxes with faces looked worse than the dashed frames, and names at
  any distance were too many; reverted (`5fa8bcd`). Models (L-115) next.
- Files: the "(top level)" heading looks odd; headings should read as paths.
- Check: the filter buttons line up with the tabs above and read as more
  tabs, not as part of the Check list; the right pane does not say whether a
  line is about the placement or the selected position.
- Materials: one very large material fills every slot (one slot per stack);
  the hover tip was drawn under the list text (fixed in `d368ffb`).
- ResourceCalculator computes with Java Edition recipes, which can differ
  from Bedrock; the hint says so now (`d368ffb`), and L-116 (the game's own
  recipes) matters for that reason too.

## Change arrow height in Japanese (2026-10-08)

By the maintainer, deployed `c32f276`, DLL
`a45a000467aea6449f29836535b7386e47495e0a3c587433212a40fff4c0c414`,
Minecraft 1.26.51.01, local world, Japanese locale. The arrow in a wrong
stair's facing row sits level with the kanji.

## Drawn change arrow in the target card (2026-10-08)

By the maintainer, deployed `3b70d73`, DLL
`6b89c75435b4d55f63c968310e711020a5ed0574d77a09b57db7ffea4108a2a2`,
Minecraft 1.26.51.01, local world. The arrow now matches the text size and
color. In Japanese it sat slightly above the kanji (about 0.8 units in the
screenshot); lowered by `shapeTextDrop()` (1 unit, Japanese only) after this
check. The maintainer also noted that numbers sit higher than Japanese text
in the settings screen; that is the Latin raise amount, recorded separately.

## L-93 target-card redesign, L-112 state names, L-109 switch (2026-10-08)

By the maintainer, deployed `f325932`, DLL
`3e571290aa07fd204bb8304527d9d5de7ca1ad4fe2021eabb9e386e4c5309030`,
Minecraft 1.26.51.01, local world, all trace options off. Every checklist
item passed: "Should be [icon] <block>" in red for wrong blocks and
"Should be Air" for extra ones; a wrong stair's facing row in yellow with
the block's own facing row left out; facings matching the stairs and
trapdoors seen in game; upside down, slab half, log axis and translated
door directions; the "Also restore the inventory" switch fits, restores only
the hotbar, armor and offhand when off and everything when on.
Reported: the U+2192 arrow in "North → East" is not in the game fonts. In
English it fell back to a smaller glyph from another font; in Japanese it
showed as a box (screenshots in the conversation). Replaced by an arrow
drawn from rectangles (`ui::changeArrow`) after this check.

## L-109 two-choice restore scope (2026-10-08)

By the maintainer, deployed `89950fc`, DLL
`787f0d899f2df24fa4823f969ff8b4600478b693ec2078e130cdb03e9736bfc3`,
Minecraft 1.26.51.01, local world, all trace options off. The setting offers
the two choices and, with the default, a death and pickup restored the
hotbar, armor and offhand. The English value "Hotbar & equipment" was cut
off in the settings row and read awkwardly; replaced by a switch (below).
Screenshots also showed the schematic target-card rows listing raw state
names ("weirdo_direction 0 (now 3)") twice; redesigned (L-93, L-112).

## L-109 recheck: rejoin, death point removal, restore scope (2026-10-08)

By the maintainer, deployed `1bd1102`, DLL
`97a3a84a4a03157f8f15884240ea70a386bcb88ff68f8f05966e7b9b650a5331`,
Minecraft 1.26.51.01, local world, all trace options off. Confirmed: the
Hotbar scope restored only the hotbar; Hotbar and equipment also put armor
back on and the offhand back; with Everything, picking up part, rejoining
and picking up the rest restored everything; removing the death point on
the map before any pickup, or after a partial restore, stopped further
rearranging. The maintainer then asked for Hotbar & equipment as the
default and no hotbar-only choice (changed after this check).

## L-110, L-93 target-card line, L-57 counts, L-109 death layout (2026-10-08)

By the maintainer, deployed `8180fe2`, DLL
`7ec1281c26be16297094cf26f89a3cb2b4050e3815bc1c4493557ac9a84fbf26`,
Minecraft 1.26.51.01, local world, all trace options off.
- L-110: restriction faces near and far, from above to grazing, no flicker;
  Shapes faces on ground and walls steady in Fancy and Simple; ghosts and
  light overlay unchanged.
- L-93 target card: the expected block's icon on the schematic row; wrong
  stairs/doors list the differing states; the row shows with details off.
- L-57: the counts line under fps looks plausible (particles rise with
  rain/explosions, chunks with render distance); no frame-rate drop.
- L-109 passed: armor, offhand, hotbar and inventory back about a second
  after pickup with the armor worn again; split stacks (30 + 20) split back;
  unrelated items gained since death kept; keepInventory does nothing;
  rearranging by hand after a restore is not undone by a later pickup.
- L-109 failed: picking up part, rejoining, then picking up the rest
  restored only the part picked up before rejoining; hotbar, offhand and
  armor stayed where vanilla put them (screenshots in the conversation). The
  log shows "inventory kept at respawn": joining read the player as not
  alive for a moment, which was taken for a death and dropped the saved
  layout. Removing the death point on the map did not stop restoring: the
  point was only looked for during a run, so a removal before the first
  pickup was never noticed.
Fixed after this check (`LifeWatch`, death point watched every tick); the
maintainer also asked for a hotbar-only restore as the default.

## L-108, L-15 step 1 with L-73 step 13, L-93 map placements (2026-10-08)

By the maintainer, deployed `178dc00`, DLL
`cac728be730a4a0260aa1b8590ff5cb0aa05af42fcd5dc0ff50c15f5f2e1d751`,
Minecraft 1.26.51.01, local world, all trace options off. Confirmed every
checklist item:
- L-108: moving from a short target card to one with many block states and
  back, the text never leaves the card; animations on and off.
- L-15 breaking (survival): Layer keeps the first block's height, rejected
  blocks are silent and breaking resumes on allowed ones; releasing and
  pressing again re-anchors; Height band 2 digs a walking tunnel and the row
  count applies; Plane, Line and Column behave as specified; the faint faces
  show only while held and skip the targeted block; Status shows mode and
  anchor; creative is restricted the same way. World exit and dimension
  change leave no stale region; the capture/reset rows are gone.
- L-73 step 13: with the restriction on, Tool Switch picks the right tool
  inside the region and mining continues after an inventory fetch; Tool
  Protection stops or swaps a nearly broken tool.
- L-93 map placements: outlines match the ghosts on the minimap, follow a
  rotating map and stay inside a round one; the world map shows outlines and
  names, the selected one white and hidden ones faint with "(hidden)"; far
  zoom still shows a small square.
Reported issues: the restriction faces z-fight with the blocks under them,
as Shapes faces do (L-110); the restriction modes and keys need a rethink
(L-15 reopened for Design); feature integration should be strengthened
(L-111). Not checked: servers, other dimensions for the map outlines.

## L-107 Lamium HUD hides with F1 (2026-10-07)

By the maintainer, deployed `18e2cc8`, DLL
`934028ffaf42b4dfadff261fc288ebbca6f52d55abe5ad05ef89c16513f9fcbb`,
Minecraft 1.26.51.01 / LeviLamina Client 26.51.6, all trace options off.
Confirmed all eight checklist items: F1 hides the minimap, Info HUD and
Target Info; Debug View, Status, durability HUD, Schematic HUD,
magnification and toggle toasts; the offhand slot and saturation marks.
Restoring F1 returns the configured display. The same holds during
FreeCamera. Moving while hidden is still recorded on the world map, a death
while hidden is still recorded as a death point, and the HUD layout editor
preview is unchanged. World-space overlays were not part of this check.

## L-106 waypoint F1 hide/restore passes; other HUD remains (2026-10-07)

By the maintainer, deployed `775c8c0`, DLL
`a719272b3055cc029eb207b6961b8c90374d86d1159690a24da5c8561b67a97e`,
Minecraft 1.26.51.01 / LeviLamina Client 26.51.6, all trace options off.
The local-world/BDS distinction was not supplied for these checks.
Confirmed all three checklist items: F1 hides normal waypoint markers,
death points, names and distances; restoring F1 returns the configured
display; the markers also disappear with F1 during FreeCamera.
Individual While held key combinations were not reported separately.

Reported that the same issue remains broadly in other HUD elements,
including the minimap, Target Info and Info HUD. These elements remain
visible while vanilla HUD is hidden. Recorded as open bug L-107; the rest
of the affected-element inventory is not yet established. This result closes
L-106 only. No implementation of L-107 was requested with this report.

## L-37 Hold/Toggle, menus, focus and dimension follow-up (2026-10-07)

By the maintainer, follow-up on deployed `d56b81e`, DLL
`822285b2f0ecceb4a9b6bf1b0310c0f15fa1e2e96a4edd1af9f095835f8bfe7c`,
Minecraft 1.26.51.01 / LeviLamina Client 26.51.6, all trace options off.
Reported that both Hold and Toggle behave as intended, as before the cave
rendering change. Menus and focus loss retain the detached camera rather
than returning to the player; dimension travel turns FreeCamera OFF.
The maintainer considers this correct behavior. The environment for each
individual lifecycle case was not specified separately; the preceding entry
records cave drawing in both a local world and on BDS.

This covers the reported activation/menu/focus/dimension behavior, not every
input sequence or lifecycle event. Other players' view of the body remains
unchecked, as do controllers, world-exit/death regression coverage for the
terrain adapter, other graphics/resource configurations and loader 26.51.5.
No code or camera lifecycle policy was changed for this result.

## L-37 underground terrain visible locally and on BDS (2026-10-07)

By the maintainer, local world and BDS, `d56b81e`, DLL
`822285b2f0ecceb4a9b6bf1b0310c0f15fa1e2e96a4edd1af9f095835f8bfe7c`,
Minecraft 1.26.51.01 / LeviLamina Client 26.51.6, all trace options off.
Reported that underground FreeCamera drew the surrounding terrain correctly
in both environments. The supplied screenshot shows extensive cave terrain
below the surface from a camera targeting solid stone.

The runtime log records loader 26.51.6 and adapter arming at 22:37:01,
binding at SDK-derived virtual slot 26 at 22:37:23.479, and
`FreeCamera terrain: native request 3 -> 5 retained` at 22:37:23.480.
Unlike the first build, the replacement actually ran. This validates the
native-request approach for the reported scenes; it does not measure native
call frequency or prove every terrain/rendering case.
The maintainer also confirmed normal-view and player-control restoration
after turning FreeCamera off. Other players' view of the body is explicitly
unverified. Detailed Hold/menu/focus/world/dimension cleanup, controllers,
other graphics/resource configurations and other game/loader versions were
not individually reported. Current coverage remains in VALIDATION.md.

## L-37 first candidate disabled by loader version gate (2026-10-07)

By the maintainer, local world, `aa5efa9`, DLL
`f08a49689a8d6096eed9dbf967678fc8d01136f549379f579765a725669e2ef8`,
all trace options off. Two screenshots near (-2, -9, 215) and (0, -8, 215)
showed the surrounding cave almost completely absent in one view and visible
in the other. The maintainer reported no improvement over previous partial
cave rendering. No separate restoration or lifecycle result was supplied.

The runtime log at 22:26:15 says
`FreeCamera terrain: vanilla visibility retained (unverified game/loader version)`;
there are no binding/substitution messages. The installed LeviLamina manifest
is 26.51.6, while this build required 26.51.5. The game executable file version
is 1.26.51.1 (launcher 1.26.51.01). The candidate never ran; the screenshots
confirm the existing limitation, not the effect of replacing native requests.
Next build permits the installed 26.51.6 patch, keeps the exact game version
and runtime call/field checks, and logs arming plus the detected loader.
The next check must establish a native 3 -> 5 substitution before interpreting
the cave image as a result of this approach. L-37 remains open.

## 0.1.7 release smoke test (2026-10-07)

By the maintainer, local world, release build `b3c6555`, DLL
`2c3e7546...fd903806b` taken from the release ZIP
`Lamium-0.1.7-client-windows-x64.zip` (`2e0926e0...93b013b5`), all trace
options off: reported OK (the mod loads at 0.1.7, settings open, schematics
and the other changed features work, no crash). Not covered: servers and
the server checks listed under BACKLOG's Pre-release checks.

## L-93 list scrolling and pane fit fine; follow-ups (2026-10-07)

By the maintainer, local world, `f4e402d` (DLL `314a043e...098f3a`), with
screenshots: list scrolling under the pointer and scrollbar dragging,
narrow-list columns, the warning and button fit and the HUD rule spacing
are fine. Seen but not blockers (after 0.1.7): the target card could say
more (the expected block's icon, which states differ); a bed drew only one
half, and another bed cell showed only the light-blue outline with its
name. The Verify tab's colored preview also waits until after 0.1.7.

## L-93 toasts, parentheses, dense mistakes fine; list scrolling (2026-10-07)

By the maintainer, local world, `5103ee5` (DLL `d28d352b...0c0818`), with
screenshots in Japanese and Chinese: the two-line adjust toast, the
full-width parentheses and the lighter dense mistakes are fine. New: the
Schematic HUD's title rule sat too close to the counts; the large-file
warning ran over its button and the two buttons overlapped with the green
one underneath; at a larger UI Profile the Materials and Check lists could
not be scrolled with the wheel or the scrollbar, and Materials lost its
name column. Cause of the scrolling: wheel events carry no position, so
the Shapes, Waypoints and Schematics screens always scrolled the detail
pane. Changed in the next build (not yet checked).

## L-93 menu notes, adjust toast, dense mistakes (2026-10-07)

By the maintainer, local world, `7535a0d` (DLL `f234a873...e2b67`), with
screenshots. The note under the menu and the combined adjust toast work,
but the combined toast shifted left and right as its tail changed; wanted
as a second line. In Japanese, digits and parentheses in the toast and the
menu sat a little higher than the kana. Drawing is much lighter, but an
area dense with wrong or extra blocks is still fairly heavy. Changed in the
next build (not yet checked): two-line toasts, full-width parentheses from
a translation, touching mistake marks without the faces and outlines
between them.

## L-93 remaining 0.1.7 items checked; external review (2026-10-07)

By the maintainer, local world, `e09334d`: the items listed as unseen in
Pre-release checks were looked at and are broadly fine, except two: a Move
from the menu with no area set did nothing visible (toasts are not drawn
over Lamium's screens), and the adjust key's "Wheel: left/right (corner 2)"
toast was replaced at once by the step's own toast. An external code
review of `c004858` also found: a placed file replaced on disk left the
renderer on the old palette (possible out-of-range lookup); border ghosts
not following changes in the neighboring section; the block-entity cache
keyed by position only; per-frame section listing and stale-section search
growing with schematic size; entities of a large save collected only at
the end; moving an area from another dimension changed it while saying
there was none. All fixed in `5faf83b` and `7535a0d` (not yet checked).

## L-93 drawing inside schematics confirmed (2026-10-07)

By the maintainer, local world, `e09334d` (DLL `9a5f4fb6...a52c36`):
inside a schematic neither z-fighting nor hollows show any more.

## L-93 sixth look at drawing; sidebar fixed (2026-10-07)

By the maintainer, local world, `60b5fab` (DLL `e989a476...46263f4`), with
a screenshot. The settings sidebar at UI Profile 100% is fine now. Inside a
schematic no hollows showed, but z-fighting came back, most visible on
east-west faces (the 0.4% inset was not enough). Changed in the next build
(not yet checked): one face per touching pair again, the camera-facing
one, for every pair where either cell is within one of the camera's cells
(the earlier hollows were pairs between a near and a farther cell, where
both faces were dropped); ghosts within two cells are never skipped.

## L-93 fifth look; settings sidebar at UI Profile 100% (2026-10-07)

By the maintainer, local world, `931e0c4` (DLL `ff4e9a9f...08c59a0`), with
screenshots. The menu animation follows the Animations setting (done). No
z-fighting inside a schematic, but some hollows still showed (likely where
the near clip plane cut the one face kept per pair). New: at UI Profile
100% (larger UI) the settings sidebar items overlapped ("General" ran into
"Hotkeys"), since Schematics added a section and a pinned item. Both
changed in `60b5fab` (not yet checked): ghosts near the camera keep every
face, inset a hair; the sidebar narrows its items to fit and falls back to
tabs.

## L-93 fourth look at drawing (2026-10-07)

By the maintainer, local world, `efcbda7` (DLL `91167736...4d4d20`), with a
screenshot. Inside a schematic the blocks around the camera now look
right, and the menu animation is good, but z-fighting came back near the
camera (both faces of touching ghosts were kept), and the menu animation
ignored Lamium's Animations setting. Both changed in the next build (not
yet checked).

## L-93 third look at drawing and the menu (2026-10-07)

By the maintainer, local world, `f6f5386` (DLL `68a0d497...85864`), with a
screenshot. The menu is fine now. Drawing still felt off: with the camera
at a cell border a hollow showed, and blocks over the feet and legs were
not drawn (the open camera cells). The menu animation is wanted before
0.1.7 after all (for approachability), and the "Shortcuts:" prefix on the
key groups reads odd. Changed in `efcbda7` (not yet checked): ghosts within
one cell of the camera's cells drawn whole, the opening animation, the
groups' old names back.

## L-93 menu tidy-up and drawing, second look (2026-10-07)

By the maintainer, local world, `4e413ed` (DLL `3604d22b...19c87b`), with
screenshots. The menu now matches Lamium's look, but the small lower-right
menu let items touch each other and the center, and the stepper hint was
cut off. Adjacent ghost blocks z-fought where their faces met (seen from
inside a block). The drawing problem with skipping enclosed ghosts was
being inside a schematic: around the camera blocks were missing, which is
far from the truth for filled terrain. Map links and menu animation can
wait until after 0.1.7. Changed in `f6f5386` (not yet checked): faces
toward opaque ghosts dropped (no z-fighting), the camera's cells kept
open, skipping on for everyone with the option removed, the ring sized so
items never meet, hints one per line.

## L-93 menu, adjust key and drawing, first look (2026-10-07)

By the maintainer, local world, `b80f4dc` (DLL `ff622d33...db200be`), with
screenshots, including saving and placing `terrain big` (122 x 39 x 203).
Drawing is much lighter, but skipping enclosed ghosts does not depend on
the view and can show what is not there; not acceptable as the default.
The menu works overall, but: Move had no way to change its target; the
small lower-right menu moved its center between levels; Japanese text
touched item borders; the look did not match Lamium's other screens. The
adjust key works, but its hint under the crosshair duplicated the toasts.
The key guidance felt too pushy. Wanted: turning the layer direction to
the opposite side should keep the same layer; a waiting save should make
the chunks it waits for easier to find; placements visible on the map.
Changed in `4e413ed` (not yet checked): the menu redrawn with Lamium's
parts and a fixed center, a Move target item, the adjust key reporting
through toasts, the layer rule, skipping as an option (off) suggested for
large files, a compass direction and yellow frames for waiting columns,
quieter key guidance.

## L-93 third save and entity check (2026-10-07)

By the maintainer, local world, `1c5a676` (DLL `d4e9c333...846e9f`), with
screenshots. Fine: red/blue corner outlines; the save prompt no longer cuts
anything off; entity name tags in the world (steady, shrink with distance,
seen through walls); saving an area beyond the render distance by walking
along it, with the reminder, progress and Stop. Problems: the last two
lines of the prompt's hint and key text sit closer than the others; the
caret bar and the selection highlight start at the top of the field with
space only below the text (worse with Japanese); the honey block fallback
looks like its inner part stretched; the door still draws nothing. Not
seen: the large-file warning for `terrain big.mcstructure` (7.7 MB,
122 x 39 x 203; how it was opened is to be asked). Other findings: a large
schematic made the game slow; moving one placement makes the others blink
(every placement change drops all built sections); name tags through walls
and from far away may be too many with many entities; wanted: unloaded
parts of a save shown in the world, selection and corners driven by the
placement keys, a key to clear the selection, and a rethink of the many
schematic keys. Implementation paused for a design conversation.

## L-93 corners, save prompt, entity names, blocks without a mesh (2026-10-07)

By the maintainer, local world, `cd21ee4` (DLL `13fc81e5...c74fc8`), with
screenshots. Corner 2 no longer opening the prompt and the kept area work.
Problems: the corner marks sat inside the block, so on a full block neither
corner could be seen; the save prompt cut off "Corner 1/2", the file path,
the hint and the key line; entity names drawn on the HUD jumped as their
size was recomputed each frame and should look like name tags in the world;
a larger area could not be saved ("chunk not loaded" beyond the render
distance); the trailing "_" in text fields could not be told from a typed
"_". Blocks without a mesh: torch and bed draw like other ghosts, skull and
door show only the light-blue outline, and so does a honey block. The
large-file warning was not tried. All addressed in `1c5a676` (not yet
checked); skull stays an outline (it needs its block entity data).

## L-93 schematic entities, area save, open folder (2026-10-07)

By the maintainer, local world, `14c2265` (DLL `a997b66c...4357e5`). Entity
frames and checks, saving an area and "Open folder" work in broad terms; no
problem found in items 1-3 beyond these: the two corners looked the same;
setting corner 2 opened the save prompt at once, which made adjusting the
corners awkward; saving cleared the corners, so a later re-save meant
choosing again; entity names were drawn at a fixed screen size like
waypoint names and came out far too large for one entity. All four are
changed in `cd21ee4` (not yet checked). The outline for blocks without a
mesh (item 4) was not reported.

## L-93 schematic fixes after 035350c, reported late (2026-10-07)

The maintainer said on 2026-10-07 that they had checked the schematic builds
after `035350c` in game but had not reported it at the time; the items are
closed on that statement. Covered: the icon cache dropped on world exit
(`3092a4c`, the crash after a language change) and the instant update at the
crosshair (`67cd37e`, `ef31e17`, `1e60f3f`). No build hash, observations or
environment beyond that were recorded.

## Settings snapshot keeps changes immediate (2026-10-07)

By the maintainer on `667031e`, ordinary DLL SHA-256
`cd3abafef667fc1bd2d268b9e6cded89190254d0a19cf024ba0aad1fc13abe9d`: Edge
Guard on/off, chunk border and hitbox overlays, HUD background opacity and
Tool Switch on/off all took effect right after closing the settings screen.
Frame rate was not measured.

## Worn-item drag passes (2026-10-07)

By the maintainer on `037a151`, ordinary DLL SHA-256
`b9fb0a6cfc900159807d5fbefc5ed8e485f7773790b92a04a6bb1f87b50ab9ee`, local:
Shift + left click on armor equips it; holding on from armor equips it and
transfers the slots passed afterwards; a drag begun elsewhere transfers armor
without equipping it; no double moves or stalls; chest screens unchanged.

## Follow-ups on `0d5950c` pass; worn-item drag refined (2026-10-07)

By the maintainer on `0d5950c`, ordinary DLL SHA-256
`68a2c21bae2afba942ffbf49dc350d5242d1b437a63f10fe060f0c2f9c5593ec`: the
checklist passed (worn items equip with Shift + left in the inventory screen,
chest transfer unchanged, Fake Offhand materials pass on stone but not on a
decorated pot, lectern or sign, the map without section requests). The
maintainer refined the worn-item rule: a press on a worn item equips it and
a held drag continues transferring from there; a drag begun elsewhere moves
worn items like any other item.

## L-104 saved colors inside the render distance pass (2026-10-07)

By the maintainer on `ea70c4d`, ordinary DLL SHA-256
`befc7a5f8e0d615bb8a0a2a8fb1d62b24e76121bb16fd61b5794250b5da21d6b`: after a
rejoin, unviewed land inside the render distance showed the saved colors;
looking at it kept or set the correct colors; land saved black earlier
recovered once rescanned; new land drew as before.

## L-104 black tint fix passes (2026-10-07)

By the maintainer on `8472bbf`, ordinary DLL SHA-256
`fd3a408f2435ce1c0b7692644dcfb7ea4616a91ba155baf1370eafb3adb866b3`: correct
land no longer turns dark when it enters the render distance; land saved
dark earlier recovers only when rescanned. After a rejoin, unviewed land
inside the render distance looked slightly off (the stand-in map tint) until
looked at, then correct (screenshots). The maintainer suggested preferring
the saved colors there, as outside the render distance.

## L-104 darkening traced to a black renderer tint (2026-10-07)

By the maintainer on `20c1880` (DLL SHA-256
`7f63fb92cbbb5afa301762672a5f8071c4af9cb770970f69bd458a15c99e98c1`): after a
rejoin and more than 30 s without turning, the world map still showed large
dark areas inside the render distance and some outside it (screenshot).
The "Map dark:" log: all 40 dark-block samples were
`minecraft:grass_block color ff000000 texture ff939393 tint 5`, i.e. the
block renderer's grass tint returned (0, 0, 0). Whole chunks scanned without
stand-ins came out dark (`pending false ... dark 256`); partial chunks were
filled from the saved map correctly (`unknown 256 -> 0 ... saved true`),
but some saved data was already dark (`savedDark 256`), recorded by earlier
builds since `6c7414c`. The missing-section path was not the cause.

## L-104 teleport passes; darkening still inside the render distance (2026-10-07)

By the maintainer on `5d8dcfc`, ordinary DLL SHA-256
`96b96ec17316ce7d36f8b74d0fbfa36abf2ba4f926258650d028918bdf543d0f`.
- Teleport: offered and working where /tp works (including commands forced
  by LeviLamina), absent where /tp is not available. Teleport is done.
- Darkening: still present after a rejoin inside the render distance. Land
  saved correctly outside it turned dark when it came within the render
  distance and stayed dark until fully received; corners not looked at
  stayed dark; another world showed a ragged map (screenshots). Unseen sides
  may fill slowly; no burst of loading without turning. The next build logs
  "Map dark:" lines (dark blocks found by the scan, and per partial chunk:
  unknown columns before/after the saved-map fill, whether the saved map was
  available, dark saved columns).

## L-104 rejoin darkening persists inside the render distance (2026-10-07)

By the maintainer on `9039411`, ordinary DLL SHA-256
`8a0f04edbe078b2bc5cd7cb9a63b0b6945799ea0bb568cebf6003c2993133d31`.
- Teleport followed the cheat setting correctly. The maintainer clarified the
  intent: when /tp works through LeviLamina's `forceEnableCheatCommands`,
  teleport should be offered too (usability over the cheat setting).
- After exploring a wide area and rejoining (render distance about 16
  chunks), land outside the render distance kept its saved colors, but
  inside it, where the client had the chunk without all its sections, land
  turned black again (screenshots). Static cause: sections not received at
  all read as air, the 16-block scan found nothing and returned the known
  "nothing to stand on" dark column, which was saved over explored land.

## L-104 teleport flags and rejoin darkening (2026-10-07)

By the maintainer on `506e3fc`, ordinary DLL SHA-256
`bdbb0233330d3debf7c5c762a971f0ce00c7331904d3a983451970e898a00726`.
- Teleport was hidden in both a cheats-off and a cheats-on world. The log:
  cheats-off world `cheats false commands false permission 3`; cheats-on
  world `cheats false commands true permission 3`. The client's LevelData
  cheats flag stays false; the commands flag follows the world's cheat
  setting even with LeviLamina's `forceEnableCheatCommands`.
- After rejoining, land explored on `6c7414c` showed black on the world map
  until looked at again (screenshot). `mergeChunk` never records unknown
  columns, so the saved data was intact; the live tile of a partly received
  chunk replaced it on screen.

## L-104 map tint, relief and teleport pass; commands in a cheats-off world explained (2026-10-07)

By the maintainer on `6c7414c`, ordinary DLL SHA-256
`8e1f5cd30c3a483e60a71d55fafbdc4ba669b608ea0fa73f8298668d1ce7c23a`. The
checklist passed: swamp foliage tinted differently, stronger relief, ground
and waypoint teleport, no teleport item from another dimension's map, the
existing menu items. The log shows tint values from the renderer's policy
(no crash with a null tint cache).
- In a world with cheats off, chat commands (/tp, /give) worked and
  achievements still unlocked; vanilla without mods does not allow this.
  Cause: the instance's LeviLamina config has `"forceEnableCheatCommands":
  true` (Config.json, unchanged since 2026-09-23). Lamium only reads the
  commands flag and sends /tp on a click. The teleport item had appeared
  there because it trusted that flag; it now also requires the world's
  cheats flag (not yet checked).
- Map: areas not yet looked toward (screenshots: between trees) stay black
  until viewed. The log shows `client_request_placeholder_block` stand-ins.

## L-103 drag re-entry fix passes (2026-10-07)

By the maintainer on trace build `163bb96` (`transfer_trace`, DLL SHA-256
`3e1ebe7719de28e38cfaef0a5d4d93c3d41f5d1ac4acc3d3abd8d01119df25b7`): repeated
Shift + left drag round trips between hotbar and inventory, and between
player inventory and a chest, moved items on every pass; holding still on a
slot moved it once; Ctrl + left drag round trips moved one item per entry;
jittering on slot borders caused no unintended repeats. The trace has 200
accepted sends and no response failure, source change or missing
destination. The trace-disabled build `163bb96` (DLL SHA-256
`37d4108bfa0b87b305fc34a42e90b3971e630745b88a3cfbe73de698f2cd96da`) was then
deployed without a separate check.

## L-103 transfer in every game mode; drags skipped revisited slots (2026-10-07)

By the maintainer on `97c44c6`, ordinary DLL SHA-256
`734f7c38fbc70cb3153aa29d4edddadf8f7848057c6044a1f380988867f1ffd1`:
inventory-screen transfer worked in creative and adventure as in survival;
the creative catalog stayed vanilla; creative also showed `inventory_items`
27 and `hotbar_items` 9. While holding Shift + left drag across both sides,
an item moved out and back could not be moved a third time, in every mode
and screen.

Trace build `ef6ac93` (`transfer_trace`, DLL SHA-256
`4b4576bde8aee777f335cff111f677f1018667b4a194a810a69b5ccec97e5312`), one
held Shift + left drag over the inventory screen: every send was accepted;
no response failure or source change. Slots that had already been dragged
over once in the stroke (for example `hotbar_items:0`) received items later
but were never enqueued again: the per-stroke visited set skipped them.
"No destination" appeared only while the hotbar was full.

## L-102 Hand Restock threshold/order and L-103 inventory-screen transfer pass in survival (2026-10-07)

By the maintainer on `bd30648`, ordinary DLL SHA-256
`afb3c765a169c3711d36dfda8febcb49603ddb186b2ded8a7a094ee1f1914523`,
same configured baseline instance, local survival. All checklist items were
reported without problems:
- Hand Restock: both rows present with defaults 6 and smallest stack;
  held 7 cobblestone with reserves 12/32/64 refilled from the 12 (slot freed);
  largest stack took the 64; threshold 0 waited for depletion; threshold 20
  refilled at 20 or fewer.
- Inventory screen: wheel down/up moved one item to the hotbar/main
  inventory (into a matching stack with room); Shift+wheel moved stacks;
  Shift+left drag moved passed stacks, leaving what did not fit; Ctrl+left
  drag moved one each; Shift+left click on armor moved it to the hotbar
  instead of equipping; chest transfer unchanged; the creative inventory
  stayed vanilla (survival-only gate at that build).
- Log: the survival inventory screen has `inventory_items` 27 and
  `hotbar_items` 9.

The maintainer then asked for inventory-screen transfer in every game mode,
as in storage screens.

## L-94/L-95 fireworks swap switch passes (2026-10-07)

By the maintainer on `d93f04d`, ordinary DLL SHA-256
`9a8a5ae7265dad5256d171a360d493153b88672612a3f122a67e2254f83d893c`,
same configured baseline instance. All seven checklist items passed: the
"Fireworks to Fake Offhand" row and help; F sends a held firework to the
Fake Offhand slot (real offhand unchanged); an empty hand takes it back (real
offhand first); switch off, or Fake Offhand off, sends it to the real
offhand; shields/totems unchanged; Fake Offhand fireworks while gliding and
on blocks restore the prior selection on the ordinary build.

## L-95 vanilla held firework fires once while gliding (2026-10-07)

By the maintainer on the same session (trace build `15cedc2`), Fake Offhand
off: holding use with a main-hand firework while gliding launched one
firework. The Fake Offhand result (one firework per hold) matches vanilla and
is kept.

## L-95 firework selection echo undone; held firework does not repeat (2026-10-07)

By the maintainer on trace build `15cedc2` with `offhand_trace`, DLL SHA-256
`c4b9286426a514579c9260d4776951694407c6f36e4f4c7985e4dfb10b925dac`.

- Fireworks on a block and single uses while gliding kept the prior
  selection; a manual selection right after a use was kept; snowballs
  unchanged. The maintainer reported the checklist mostly without problems.
- Trace: every correction came from `PlayerHotbarPacket` (`source=hotbar`,
  one or two per use), none from MobEquipment; each restored the prior slot.
- Holding use while gliding launched only one firework. The trace shows the
  hold staying owned and later build ticks borrowing the firework slot, but no
  further use callbacks. Whether vanilla repeats a held main-hand firework
  while gliding was not checked.

## L-95 firework trace: the server reselects the borrowed slot after restore (2026-10-07)

By the maintainer on trace build `0be2f0d` with `offhand_trace`, DLL SHA-256
`19893e2cd90757c2e2703528af6bc19612fc0d366f122971fbc07d28b38c7379`. Empty
primary slot 1, firework or snowball in slot 9: one click on a block, one in
air and one while gliding.

- Snowballs returned to slot 1; fireworks always stayed on slot 9.
- Firework trace: the borrowed use reports slot 8 (zero-based), runs the
  air use, and the restore succeeds (`now=0`, `sent=0`). About 50 ms later,
  when the firework count drops 53 to 52, a build tick already sees slot 8
  selected; the hold then cancels because the selection no longer matches.
  No build or use hook selected it, so the update arrives from the server
  side. The exact packet was not logged.

## L-95 empty-hand sneak placement on containers fixed; fireworks never restore (2026-10-07)

By the maintainer on `d2a4370`, ordinary DLL SHA-256
`7b9fe128243edffba26f0db03a339d9fa3155174ee65326f216ad9a21e954491`,
same configured baseline instance, all trace options off.

- The checklist was run: an empty main hand with dirt in the Fake Offhand
  slot now places dirt on a sneak right click on a chest instead of opening
  it. The remaining items (ordinary chest opening, single/held placement,
  gunpowder control, sword/snowball) were reported without problems.
- Firework selection (on `f693adc` and earlier sessions): after every firework
  use from the Fake Offhand slot, selection stayed on that slot. This held on
  the ground and while gliding, and with an empty hand or gunpowder in the
  primary slot; selection never returned. A trace build follows.

## L-95 property-based eligibility mostly passes; two faults (2026-10-07)

By the maintainer on `f693adc`, ordinary DLL SHA-256
`f3ba0a3327208ea1ec5f462cc0591fef32fe04d081980ac88bd4a8271ca2edcb`,
same configured baseline instance, all trace options off. The property-based
checklist (block items/materials/tools/food primaries in air and on blocks,
projectiles, buckets and fireworks as the secondary item, placement and chest
regressions) was run; the maintainer reported it as mostly without problems.
Individual checklist items were not reported one by one.

- Sneak + right click on a chest with dirt in the Fake Offhand slot placed
  dirt with gunpowder in the main hand (correct), but opened the chest with
  an empty main hand. Static explanation: vanilla acts on the first press
  inside the native use-button handler, before the build tick that borrows
  the placement slot, and an empty hand interacts even while sneaking.
- After using a firework from the Fake Offhand slot, selection stayed on
  that slot instead of returning to the prior slot. Whether the player was
  gliding and which primary item was held were not recorded; no trace exists.

## L-95 trace-disabled instant smoke passes; passive primary gate is narrow (2026-10-07)

By the maintainer on `58d121d`, ordinary DLL SHA-256
`ce2604e8e8966dc33bbd46f1d240e56a6ff4487eda8fb10c9716c304add16a89`,
same configured baseline instance, all trace options off.

- Sword with secondary snowball: single click, held repetition and release
  worked. Water bucket placement/collection also worked.
- With other primary items that have a right-click use, and with a totem
  expected to have no use action, the secondary snowball did not throw.
  The other item identities and target contexts were not specified.
- Static evidence: the primary eligibility gate only accepts empty hands
  and vanilla sword/pickaxe tags. It therefore excludes totems and materials
  regardless of whether they have an applicable use action.

The revision admits known passive totems and selected basic materials by
identity, still rejecting block items and preserving the existing target
interaction/primary-use guards. Runtime checks for these additions are pending.
General target-sensitive primary pass/failure classification remains open.

## L-95 native first-input borrowing passes instant-use checks (2026-10-07)

By the maintainer on `6e41014`, diagnostic DLL SHA-256
`7807d4c8ea445cbf871a2b7632cefddf036bbb6398383414d9a68ee9ee8643c4`,
same configured baseline instance. Fake Offhand on, Auto Use and Hand Restock
off. The installed DLL hash matches the tested build.

- Empty hand, sword and pickaxe with secondary snowballs: long holds repeat
  in air and on ordinary block faces, and release stops throwing.
- Short clicks throw one snowball.
- Sword with secondary water bucket: placement and collection work.
- Existing block placement and chest interaction remain usable.
- The maintainer reported no problems in those checklist items. Logs show
  native first-input borrowing and a suppressed queued duplicate, followed
  by selection restoration. The earlier `e7ce3f1` manual-selection check
  remains evidence for that build, not a fresh check of this revision.

This validates the tested instant subset and primary hands only. Eggs,
non-mouse activation, overlap, broader cancellation, rejoin and server
synchronization remain unchecked. Timed use, entity use and passive effects
are still open. A trace-disabled build needs a brief smoke check.

## L-95 tool eligibility passes; primary use precedes replay (2026-10-07)

By the maintainer on `2f7878c`, diagnostic DLL SHA-256
`9451ad565dc084ac2622528d794e1c8898614cb72711e224c0581f549e39a019`,
same configured baseline instance. The requested empty/sword/pickaxe long
holds in air and on ordinary blocks, then sword/bucket placement/collection,
were exercised. Short clicks were omitted; no explicit success/failure report
was supplied in this turn.

- Adapter trace: both diamond sword and diamond pickaxe pass the tag/idle
  predicates, choose slot 8, select it and deliver the raw down-handler list.
  No ownership cancellation is recorded for these holds.
- Native trace: each tool's initial physical primary use precedes the queued
  borrowed down. Neither tool produces borrowed snowball use, and the target
  count stays unchanged. Empty-primary samples reach borrowed air use and
  decrease the target count while held.
- Sword/bucket controls replace water with empty and empty with water after
  the borrowed down. Callback/stack evidence does not prove server persistence.

The revision borrows before the native use-button handler's first attempt,
replays its captured handlers once, and prevents a later queued/physical
duplicate. The hypothesis is a prior native primary attempt gating subsequent
air use; the internal gate is unknown. This revision awaits runtime checks.

## L-95 native hold works with an empty primary; tool snowballs fail (2026-10-06)

By the maintainer on `e7ce3f1`, diagnostic DLL SHA-256
`5830155266b2baf0858de91f3fd796f3e32b8b0a03115a462d8353f47aaa4d8c`,
same configured baseline instance, Fake Offhand on, Auto Use and Hand Restock
off. No separate server/environment report was supplied.

- Empty primary: snowballs repeated in air and on block faces and stopped
  on release; short bucket clicks acted once and held bucket use repeated.
- Manual slot changes stopped repetition and preserved the chosen slot.
- Block placement and chest interaction remained usable.
- Extended primary-hand checks: buckets worked with a sword and a pickaxe,
  but snowballs could only be thrown with an empty primary.
- The installed DLL hash matches the named build. Failed pickaxe/sword
  snowball samples show primary-item use callbacks and an unchanged target
  snowball count. The trace does not include adapter eligibility or replay
  outcomes, so it cannot establish the rejecting stage. Those observations
  do not justify treating the callback bool as a safe fallback instruction.

The next diagnostic records adapter choice, primary block/tag/idle predicates,
target identity, hit type, held ownership and replay outcomes. It makes no
item-use behavior change. Non-mouse activation, eggs, overlap, focus/menu/
dimension cancellation, rejoin and server synchronization remain unchecked.

## L-95 ordinary held snowballs repeat in air and on blocks (2026-10-06)

By the maintainer on the deployed `c522b22` diagnostic DLL, SHA-256
`6c9c0d4c038f561307e0eb2678e3a9be3ec6aea1918bda9cf9a095b8a009f156`,
same configured baseline instance. Minecraft was restarted; Fake Offhand,
Auto Use and Hand Restock were off. Snowballs were selected in the main hand.

- Holding ordinary use repeatedly threw snowballs at a steady interval,
  both in air and while targeting a block face.
- Trace: the first air-use callback is inside handleBuildAction; later
  air-use callbacks occur inside build processing roughly 200-250 ms apart,
  with the selected source count decreasing between calls. On a block face,
  initial block use precedes the first air use. Subsequent uses share the
  native held air-use path.
- This establishes native repeat reachability, not Fake Offhand repetition.
  The revised adapter retains one ordinary down edge until release and
  borrows selection only inside each build call. Runtime checks are pending.

## L-95 queued instant click works; held repetition remains open (2026-10-06)

By the maintainer on `c522b22`, DLL SHA-256
`6c9c0d4c038f561307e0eb2678e3a9be3ec6aea1918bda9cf9a095b8a009f156`,
only `offhand_trace` enabled, same configured baseline instance. No separate
server/environment report was supplied.

- Snowball throwing, water placement and water collection worked.
- Block placement and chest interaction also worked.
- The maintainer rejected once-per-hold as the target behavior. The adapter
  must repeat while held with ordinary item-use cadence and cooldowns.
- Trace: a borrowed snowball reached both air-use boundaries, with target
  count later observed changing from 12 to 11. Borrowed instant clicks
  restored the primary selection. This does not establish server persistence.
- When slot 8 was actually selected with a bucket, held build traces show
  water/empty replacement transitions roughly 200-250 ms apart. This is a
  native comparison sample, not evidence of a universal repeat constant.

Eggs, all primary-hand combinations, native held snowball routing, repeat
support, cancellation, overlap, rejoin and server synchronization remain open.

## L-95 known-item gate: buckets act repeatedly, snowballs do not (2026-10-06)

By the maintainer on `c4d6258`, DLL SHA-256
`9449ca57ae7d6835dfe3d059b4350bd6ca175adad90f317e008290ee1b33ca2a`,
only `offhand_trace` enabled, same configured baseline instance.

- Snowballs did nothing.
- Water placement and collection appeared to work, but could happen in such
  quick succession that only sound was noticeable and no water remained.
- Trace: target snowballs were selected during build calls, but only block
  use callbacks occurred, not air use. Water became an empty bucket and
  changed back to water about 50 ms later during the same held input.
- Water/empty buckets report maximum duration 32 and Drink animation; a
  maximum-duration-zero gate would exclude them.
- The new intention trace's budget was consumed by repeated idle/continued
  build actions before the instant-item tests; it did not establish the
  use-press flags. The revision logs intention changes instead of every call.

This is partial reachability, not confirmation of usable instant-item support.
A queued ordinary click pair replaces the instant held-build route; single
effects, selection restoration, cancellation, servers and overlap remain open.

## L-95 first instant-use candidate failed (2026-10-06)

By the maintainer on `621a7b8`, DLL SHA-256
`566d683ef87b856ba78167f98cfa2ba38d5b3325072c82e3fc8950a3e3d39f4e`.
Only `offhand_trace` enabled, same configured baseline instance. No separate
server/environment report was supplied.

- Fake Offhand on: tried empty hand, sword and pickaxe with water buckets,
  empty buckets and snowballs in the target. Every combination failed:
  no water placement or collection, and no snowball throwing.
- Block placement still worked, confirming that the feature was enabled.
- Sampled logs show slot 8 holding a water bucket while slot 0 still held
  the sword at build-enter, useItemOn and useItem. No borrow is visible in
  that case. Eligibility values were not recorded, so the exact rejecting
  condition is not established.

This disproves instant-use support on the first candidate. Revised gate and
additional intention/duration/animation diagnostics are pending validation.

## L-95 vanilla use baseline and placement regression (2026-10-06)

By the maintainer on `c673fad`, DLL SHA-256
`49059b210fc94b7c0b831f360680eabe8dd5c9de3911d1294028507fd09bf554`.
Only `offhand_trace` enabled. Baseline platform: Windows x64, Minecraft
1.26.51.01, LeviLamina Client 26.51.5. No server-specific result was supplied.

- Fake Offhand off: food completion and early release, bow firing, water
  placement/collection and animal feeding were performed.
- Food and bow use stopped on a manual slot change; left click had no effect
  on either ongoing use, according to the maintainer.
- Fake Offhand on: existing block placement and chest interaction were
  confirmed unchanged.
- Logs show food use recording inventory slot 2 and bow use slot 1. A bow
  use callback returns false even though startUsingItem populated the use
  item. Slot change calls stopUsingItem with the original use slot still
  recorded. Release also reaches stop. Water use may call both useItemOn
  and useItem within an action, including air use after the bucket empties.

This is the ordinary-use baseline, not validation of additional Fake Offhand
items. Timed borrowing, instant target use, entity target use, passive effects,
servers and non-mouse activation remain unconfirmed. See FAKE-OFFHAND.md.

## L-97 follow-ups, weapon fetch and offhand swap modes (2026-10-06)

By the maintainer, local world, `c7bb827` (DLL `b14b6905...0eff`): a diamond
sword in the inventory is fetched past a diamond shovel in the hotbar; an
equal hotbar weapon is used instead of fetching; a fetch slot equal to Fake
Offhand's slot turns both rows' values orange with the reason in the footer,
and changing either clears it; F swaps with the offhand and the Fake Offhand
slot in creative and adventure without duplicating, losing or reverting
items; nothing happens in spectator.

## L-97 fixed-slot fetch (2026-10-06)

By the maintainer, local survival world, `a15930f` (DLL `c1d5a471...3f03`):
Tool Switch fetches a pickaxe into slot 1 and selects it, the slot's old item
goes to the pickaxe's inventory slot, the rest of the hotbar stays; the slot
stays selected afterwards; Weapon Switch fetches into its slot and the first
hit counts; with Fake Offhand on slot 9 and a fetch slot of 9 the selected
slot is used and slot 9 stays; "Selected slot" restores the old behavior.
Reported meanwhile: Weapon Switch kept a diamond sword in the inventory while
the hotbar held a diamond shovel (any hotbar weapon blocked the fetch); a
warning was wanted when a fetch slot equals Fake Offhand's slot; the offhand
swap (F) should work in every game mode, not only survival. All three are
changed in the next build (not yet checked).

## L-94 switch + command settings (2026-10-06)

By the maintainer on `ed288b6` (DLL `42175f2c...e8ae`): the heading has a
saved switch (on) with the F command row under it; the switch off stops F,
on restores it; a bound toggle key flips it with a toast. The heading and
the command had the same name; renamed to "Offhand swap" / "Swap now" in
the next build (strings only, not rechecked).

## L-94 Swap with offhand (2026-10-06)

By the maintainer, local survival world, `856d79c` (DLL `857b5b98...b875`):
a shield goes to the real offhand and back with F, and swaps with what the
offhand held; arrows, maps, fireworks and totems also went to the real
offhand (so the item flag `mAllowOffhand == Yes` covered them); a stone
block swaps with hotbar slot 9 while Fake Offhand is on and does nothing
when it is off; an empty hand takes the offhand item back, then slot 9's;
holding slot 9 with a block does nothing; F has no vanilla binding to clash
with. The log shows the items sent to the Fake Offhand slot: an undyed
shulker box, snow and a spyglass, all with the flag 2 (No), so none was an
offhand item missed. Not checked: a server. The maintainer asked for the
settings to follow
the switch + command rule (Sort); rebuilt in the next build.

## L-98 follow-ups checked (2026-10-06)

By the maintainer, local world, `f63be4e` (DLL `0fa4431c...902e`): Japanese
and English bands match the text in both Debug View columns, Info HUD and
Status (markers included); the Info HUD re-dropped near the top-left keeps
its top edge when the line height changes or lines appear; General shows
the three headings; background opacity changes every card and band (0 %
hides them); right-anchored per-line bands align right. The maintainer then
asked for new defaults: Info HUD per line, Status card, Debug View per line
(next build, not checked on a fresh settings file).

## L-98 follow-up: Japanese per-line bands (2026-10-06)

By the maintainer on `70d271b` (DLL `8f9fe249...afb`), Japanese, line height
12, Debug View with per-line backgrounds: the bands did not match the text;
left-column bands ended before the text, right-column text started left of
its band. Cause: in Japanese, labels draw Latin runs separately (raised), and
their summed width differs from measuring the whole line, which the bands
used. The next build measures with the same runs (`ui::labelWidth`). The
maintainer chose 60 % as the default background opacity.

## L-98 HUD line height and per-line backgrounds (2026-10-06)

By the maintainer, local world, `4cf9acd` (DLL `6ad093bf...0b4`): the line
height 9-16 changes the Info HUD, Status and Debug View at once; Japanese
and English fit at 9-11; per-line bands on Info and Status (markers inside),
on both Debug View columns (right column right-aligned, no band on the blank
line); the layout editor boxes follow. 12 was chosen as the default.
Seen meanwhile: the Info HUD placed near the top-left moved when the line
height changed, and rose when lines such as Sprinting appeared (its saved
anchor was the middle); General felt overloaded for one heading; wanted a
background setting and right alignment for right-side bands.

## Zoom follow-ups: toggle-mode wheel, dimension level, no ease (2026-10-06)

By the maintainer, local world. On `0b06c8f` (DLL `2008debb...0283`) the
press/release ease-in/out (0.1 s, following the Animations setting) felt
wrong; it was removed. On `32390cc` (DLL `7e2810db...bd17`): press and
release switch at once with Animations on; Hold mode unchanged; Toggle mode
switches on at the press with a toast, the wheel scrolls the hotbar while the
key is up, holding the key and using the wheel adjusts and stays on, and a
tap while on switches off at the release with a toast; a level below 1x
survives a Nether portal and resets on leaving the world.

## L-99 Zoom below 2x (2026-10-06)

By the maintainer, local world, `692dfe4` (DLL `c5351ce8...5baf`): the wheel
stops once on ×1.0 and goes down to ×0.5 with a wider view and normal
turning; with vanilla FOV at its maximum it stops near ×0.7 at the 160 degree
limit; a level below 1x is kept on the next press; a zoom left at ×1.0
reopens at the setting; leaving the world restores the setting; the HUD
readout matches; no drawing problems seen in the wide view.

## L-91 two-pass leather result; parked (2026-10-06)

By the maintainer on `cb0cfaa` (DLL `554cf9c7...6ddc18`): the two-pass leather
icons still show the undyeable parts as transparent, in the shulker box
preview and the durability HUD; the plain material drops those pixels too.
A screen recording of vanilla's Shift-click equip animation (which draws
with `renderGuiItemNew`) shows the same transparent parts, so the icon call
itself cannot draw the layer. The two-pass change was reverted; L-91 is
parked with its findings in BACKLOG.md.

## L-91 leather experiment round 2 result and the fix build (2026-10-06)

By the maintainer on `718ea09` (DLL `1df9fb4d...b469a3`): with the
multi-color material the secondary color tints the dyeable pixels (dye/dye,
white/dye and 0/dye all show the right color; dye/black is black), and the
undyeable pixels are not drawn at all (transparent; round 1's dark parts were
the slot background). The fix draws leather twice: the plain material with a
white color, then the multi-color material with the dye as the secondary
color (`IconTint.h`, `ItemIcon.cpp`). Deployed as a normal build; not yet
checked (see the next entry once reported).

## L-91 leather experiment, round 1 result and round 2 (2026-10-06)

By the maintainer on `030d965` (DLL `940a6c78...f40d`), shulker preview and
durability HUD alike: the multi-color flag (helmet) and the
`mUIIconBlitMaterialMultiColorTint` swap (chestplate) look the same: the
undyeable layer is back, but the dyeable part is white instead of the dye.
The entity change-color material (leggings) fills the transparent pixels as
a tinted square. The UI `Item` material swap (boots) changed nothing. The
hook also reached vanilla's equip animation (Shift-click), which briefly
showed the experiment look; vanilla uses `renderGuiItemNew` there too.
Round 2 deployed: `718ea09` (trace, DLL `1df9fb4d...b469a3`), all leather
with the multi-color flag; helmet color=dye secondary=dye, chestplate
white/dye, leggings dye/black, boots 0/dye. Not yet checked.

## L-91 icon trace result and leather material experiment (2026-10-06)

By the maintainer on `2b3ffe5` (trace, DLL `269a6688...7a7368`): dyed and
undyed leather armor, an enchanted shield and an enchanted golden apple in
the inventory, a shulker box preview, the durability HUD and the offhand
slot. Findings from the log:
- Leather: vanilla slots draw one pass, UI material `Item` (13), chunk type
  2, and one icon blit; Lamium's `renderGuiItemNew` makes the same blit (same
  UV, dye color, no glint, no multi-color). Only the material differs, so the
  undyeable layer is lost to the material, not to missing draw calls.
- Glint on flat icons (golden apple): three slot passes, `ItemGlintStencil`
  (6, chunk 2, blit glint 1), `InventoryItemGlint` (5, chunk 4, glint 1),
  `ItemUnglintStencil` (7, chunk 5, glint 2).
- Shield: three slot passes, `Shield` (9, chunk 7), then the same glint pair
  (chunks 4 and 5); no icon blit (a model). Lamium's foil call for the shield
  makes no blit at all, which is why its glint is missing.

Experiment deployed: `030d965` (trace, DLL `940a6c78...f40d`), Lamium's own
leather calls only: helmet with the multi-color flag, chestplate with
`mUIIconBlitMaterialMultiColorTint`, leggings with
`mEntityAlphatestChangeColorMaterial`, boots with the UI `Item` material
from `RenderMaterialGroup::common()`. Not yet checked.

## L-91 icon trace deployment (2026-10-06, unchecked)

Trace build from `2b3ffe5` with `icon_trace` on (saved in
`bin/Lamium-icon-trace`, DLL `269a6688...7a7368`). Minecraft was closed
before deployment; the local configuration was reset to `icon_trace=n`.

## World map dimension frame, Debug View versions and right column (2026-10-06)

By the maintainer, local world, `38373e9` (DLL `d3036b00...415781`). The
selected world map dimension button keeps its whole accent frame for every
dimension. The Debug View's first line shows Minecraft (as
`1.26.51+0559ac5`, kept as is by the maintainer's choice), LeviLamina and
Lamium. With UI Profile 100% the right column, which had run off screen on
`fc727a2`, now stays on screen; 50% and 75% are unchanged.

## L-101 settings header version (2026-10-06)

By the maintainer, local world, `ff55b9f` (DLL `eb27255c...35794`). The
version after the title, its tooltip, the copy with its toast, and the
removed breadcrumb in Shapes, Waypoints and Schematics all worked as
specified; the docked panels, HUD layout and world map show no version.
Two unrelated findings in the same session: the world map's selected
dimension button lost the right edge of its accent frame under the next
button (every button but the last), and the Debug View did not show the
LeviLamina version. Both are fixed in the next build (not yet checked).

## L-93 crash on world load after a language change (2026-10-03)

By the maintainer, local world, `035350c` (DLL `7146d554...e747eb`). Layer
keys and the Placed tab key work. Changing the game language (Japanese to
English, later English to Chinese) and then loading the world crashed twice
(06:15:13, 06:17:10), both in `renderGuiItemNew` called from
`drawSchematicHud` (InfoHud.cpp:339). A restart in the new language loaded
fine. Cause: the icon stacks cached in `SchematicItems` outlived the world
and pointed at item objects the game had rebuilt. The next build drops the
cache on world exit (not yet checked).

## L-93 schematic layer-here key, key order, tab keys (2026-10-03)

By the maintainer, local world, `72dad66` (DLL `95a74972...07ef`). Key
order, the Files/Check/Materials keys and the smaller HUD are fine; a key
for the Placed tab was missing. "Show the layer you stand in" did nothing
with any binding, while the screen button worked. The log showed every
press arriving ("Schematic key: layerhere"): the key looked the structure
up inside the session's change callback, which re-entered the session lock
and failed. Layer up/down shared the problem in their toast. Fixed in the
next build by looking the structure up before the change (not yet checked).

## L-93 schematic settings regroup, keys and HUD look (2026-10-03)

By the maintainer, local world, `6be984d` (DLL `19d8bbf7...aa0b`,
trace-disabled). The regrouped settings, per-key descriptions, HUD switches
and layout link, select-looked-at, the six move keys and the nearest-mistake
toast work. Problems: the move keys were not listed together (catalog
order); "show the layer you stand in" bound to Mouse 4 + 6 did nothing;
wanted keys that open the Check and Materials tabs directly; the HUD card
was wider than needed. Wording: 最も reads better than いちばん. Addressed
in the next build except the layer-here key, which now logs each schematic
key press to find out whether the press arrives (not yet checked).

## L-93 schematic HUD and keys, first look (2026-10-03)

By the maintainer, local world, `58ee64a` (DLL `b5f42df1...f9c320`,
trace-disabled). Mostly fine. Problems: the HUD could not be set up in
detail and its child switches did not read as "show this part"; the HUD
should look better; all schematic settings sat under one feature row with
thin descriptions; "nearest mistake, again: the next" was confusing and its
toast showed a stray dot; "select the placement you look at" did nothing
(ghosts are not blocks, so the crosshair hit misses them); moving needed
away/closer/left/right/up/down keys; "match where I stand" had no key.
Addressed in the next build (not yet checked).

## L-93 schematic Check and Materials tabs (2026-10-03)

By the maintainer, local world, `2f3a127` (DLL `435be892...53ce59`,
trace-disabled), `broken_village_house` placed. Check: counts and the list
match the world and follow placing and breaking within seconds; the filter
and "Show in world" work, but the marker was hard to see. Materials: names,
icons and counts are right (stairs, slabs, chests, beds), carried counts
include shulker boxes; shown-layers-only works. Problems: tab order differs
from the mockup, long names in Check cut off and the world block had no
icon, the Materials detail pane used little space, and item icons kept the
pickup squash. Wish: a button to set the layer to where the player stands.
All addressed in `f796ce0` (not yet checked).

## L-93 schematic mistake faces, refresh, chunk arrival (2026-10-03)

By the maintainer, local world, `80b1eaf` (DLL `519d5873...7658c5`,
trace-disabled): wrong and turned blocks are easy to tell apart with the
tinted faces; ghosts and outlines change quickly (about 0.25 s) when blocks
are placed or broken; rejoining shows no red flash before the chunks load.
The maintainer asked whether the looked-at block could update even faster
(done in the next build: the crosshair cell and the cell against its face
rebuild their section as soon as they change).

## L-93 schematics, first playable build (2026-10-03)

By the maintainer, local world, `a84a6d7` (DLL `72b533af...df77b`,
trace-disabled), with `mixture.mcstructure` and
`broken_village_house.mcstructure` in mods/Lamium/schematics. The switch in
the new Schematics category and the Schematics screen work: the files list,
"Place at feet" and the ghosts at the feet. Placing the right block removes
its ghost; a different block gets a red outline, a turned one a yellow
outline (both correct, but thin and hard to see beside the vanilla selection
outline). Every change took about two seconds to show. Rotation and mirror
turn the whole build and the stairs/chest facing correctly (the game's
transform matches Placement.h). Layer modes and directions behave as
described. Ghost brightness stays the same at night and in caves. Deleting a
placement removes it; after leaving and rejoining the world the placement is
kept, but everything showed red outlines for about two seconds while the
chunks arrived. Not covered: servers, other dimensions, large schematics,
block entities and entities from files, performance.

## Radar face size at 512 blocks (2026-10-03)

By the maintainer, local world, `2651ed5` (DLL `bd727aca...a4017de7d`,
trace-disabled): with mob faces on, the sheep face is no longer larger than
the other faces at 512, 256 and 128 blocks.

## L-89 distant players: height, Nether, disconnect, rejoin (2026-10-03)

By the maintainer, trace build `a86b076` (DLL `5e5563e4...027eafe`), a world
hosted on the PC joined from the phone, then a phone-hosted world for the PC
rejoin. A loaded player 10+ blocks above stays opaque; a distant one stays at
70 %. The Nether removed the distant marker, which came back after returning
and moving. Disconnecting removed it; rejoining and moving showed it faded
again. After the PC left and rejoined, with the other player moving, no old
marker was left. Also seen: at 512 blocks a sheep face looked much larger
than other faces (fixed in `368f81a`, not yet checked).

## L-89 distant players, sneak and coming into range (2026-10-03)

By the maintainer, phone-hosted world joined from the PC, trace build
`8c9522c` (DLL `b304ea15...cfec3488573`). Sneaking far away removed the
marker (two HIDEs at 00:04:49 and 00:05:32, each followed by an update when
the player moved again); the earlier "sneak did not hide" was the phone's
toggle sneak, not a fault. Coming into range: the Actor entered the actor
list at about 67 blocks (00:06:08) and the marker switched to it then, but it
stood about 14 blocks above the PC player, so the radar's height rule (8+
blocks above or below: 40 %) kept it faint until it came down at 00:06:18 -
seen as "still faded for several seconds". Rejoining from the PC showed no
leftover marker, but the other player did not move, so it was inconclusive.

## L-89 distant players on the map (2026-10-02)

By the maintainer, a world hosted on a phone and joined from the PC, `d08e96c`
(DLL `1f60eac4...63295b842db`, trace-disabled). Near: normal head and name,
smooth. Beyond range: a faded head with a grey name on the minimap and world
map, jumping every few seconds while the player moves, kept while still.
Not as expected: sneaking far away did not remove the marker (waited a
while); coming into range, the faded marker stayed for some time before the
normal one took over. Not tried: the Nether, the other player disconnecting
(the host was the phone), rejoining from the PC.

## L-60 map on an external server (reported 2026-10-02)

By the maintainer, an external BDS server with other players, an explored
area of roughly several thousand blocks square; build not recorded (a
release from 0.1.5 on). Minimap, radar and world map behaved as in a local
world; slowness came from the server's load, not the map. Other players'
heads showed on the minimap and the world map but disappeared a short
distance away (outside entity tracking; this led to L-89). Not reported
separately: waypoint storage per server, mob faces (L-85), process exit.

## L-67 Weapon Switch enchantments (2026-10-02)

By the maintainer, local world, `7b702da` (DLL `60d38197...c179699b`). A
Smite V sword wins over an equal plain sword against a zombie; against a pig
the plain left sword wins the tie; a held Smite sword stays against a zombie;
a Sharpness V iron sword wins over a plain one against a pig; a Bane sword is
chosen against a spider.

## L-67 Weapon Switch (2026-10-02)

By the maintainer, local world, trace-disabled builds. `7290912`
(DLL `7a953659...7edc7c8f20`): the slot switched, but the switching hit only
played the swing - no damage, no knockback; off and in Creative hits were
normal. `20e8cb5` (DLL `3e475963...d7ce13137`, equipment packet sent before
the attack): the switching hit lands with the weapon's damage and knockback,
from a block and from a bare hand; the shown and used item stay in step after
manual slot changes; a sword wins over an equal axe from a bare hand; a held
axe stays against an equal sword; armor stands, item frames, boats and
minecarts do not switch; Creative does not switch; off does nothing; the
inventory fetch moves a sword in and that same hit deals sword damage. Not as
designed: a Smite V sword and a plain sword tied for zombies and pigs (the
left one won). Not checked: servers, Sharpness, Bane, trident/mace.

## L-92 readouts inside the vanilla tooltip (2026-10-02)

By the maintainer, local world. Probe `b61f5c9`: appended lines show in the
tooltip; color codes do not tint glyphs. `8e7f8a9`: durability as the last
tooltip line - tools, non-damageable items, ordering, switch, containers,
creative and language all fine. Probes `055369a`/`d0b925b`: no capture
through `drawText`; `c24a307`: `Font::drawCached` gives the text corner and
frames matched the glyphs. `91d1254`: icons painted under the glyphs and
missing in English (values taken from another item's hover text).
`d246710`: lookup fixed; the drumstick glyph still showed (it faces the
other way). `478c53a`: blank placeholders, but 4 units wide, so icons ran
past short names. `49665ca`: bread, golden carrot, enchanted golden apple,
melon slice, Japanese and English all fine.

## L-64 food values in the inventory (2026-10-02)

Build `3ffd304`, local world, by the maintainer: bread (two and a half
drumsticks, three outlined), golden carrot (three drumsticks and outlined
empty icons, seven in all), melon slice (one drumstick, right half
outlined), rotten flesh, cookie and steak show; non-food shows nothing; the
icons sit right in the box; the switch hides it; container screens too. The
maintainer suggested moving such readouts into the vanilla tooltip (L-92).

## L-63 saturation on the hunger bar (2026-10-02)

By the maintainer, local world (research also on a server). Trace `8a1214b`:
hunger and saturation reach the client in both; bread 14/0 -> 19/6, a golden
carrot capped saturation at 20; `hunger_rend` placement matched classic 75 %
and 100 % and Pocket UI, gone in creative, fine while riding. `8cce406`:
outline cut from `hunger_full` marked the bone and meat edges instead of the
outline. `8c23b96`: outline from `hunger_background`, but one unit left.
`91733b1`: aligned and readable; held-food preview hard to read with odd
saturation. Opacity compared at 30-70 % on `23e1d88`; `70c440c` with opaque
pale gold gain outlines and 50 % hunger icons was judged readable. Half
marks match the half drumstick; off switches, UI size, Pocket UI and
creative behaved as intended.

## L-75 offhand slot (2026-10-02)

By the maintainer, local world. `d99bdb1`: no slot (the hotbar was looked up
by its type name). `118ddb0`: the slot shows left of the hotbar, follows
video and UI-size changes; the count lacked vanilla's shadow. `6018a41`:
count matches the hotbar. `3360b37`/`bb9cdb5`: log shows the enchanted shield
with `isGlint` true, yet no glint is drawn (also missing in container
previews; an enchanted golden apple does shine) - split out as L-91. On
`bb9cdb5`: hidden while empty, the empty-frame option, UI size, Pocket UI,
F1 and the inventory screen all behave as intended. Offhand compasses are
not possible in Bedrock, so the animated-frame path was not exercised.

## L-88 target hearts in absolute health units (2026-10-02)

Build `c6378e8` (DLL `d0de4fbb...5c619a72`), local world, by the maintainer:
zombie one line of ten hearts with half hearts on odd HP, enderman two
lines, iron golem five lines, a boss (Wither or Warden) shown as the bar
with its number, chicken two hearts with the number pulled in, and the
card resizing between targets: all as expected, no problems seen.

## L-90 Simplified Chinese locale (2026-10-02)

Build `ca25c2c` (DLL `857d96c9...0361d31e`), local world, by the maintainer:
with the game set to 简体中文, the settings screen and the rest of Lamium's
text were checked; no misaligned or shifted text and no wrong behavior was
seen, so the Japanese Latin raise is not needed for Chinese. Returning to
Japanese and English was part of the run. The wording has not had a native
review yet.

## L-87 radar player heads, outer layer (2026-10-02)

Research build `e073fdc`, by the maintainer: the skin's outer layer (hair,
hats) is drawn over the face as expected. Skins with custom head models
were not seen.

## L-87 radar player heads, character-creator skins (2026-10-02)

Research build `e073fdc` (DLL `4e05e895...a7ee419604`), with several other
players, by the maintainer: the character-creator skin (bii8634), wrong on
`723738e` and `8e51d53`, shows its face; the other players' heads are
right; minimap and world map agree. The log shows three character-creator
skins read from their animated face and one 128x128 classic skin from its
geometry. sei07626 (the classic skin right on `fa1caed`) was not online.
The outer layer was not judged separately.

## L-87 radar player heads (2026-10-02)

Build `723738e` (DLL `7b90fd0b...3383aa58bc`), with another player, by the
maintainer: the "Players as heads" switch is there; on the minimap the
player was drawn with an outlined square, but it did not read as a head
(another part of the skin texture, most likely a character-creator skin
whose face is not at the classic place; the log had no "no head" line, so
the classic cut was taken). The outer layer could not be judged. The
world map drew a head with the name under it; switching off gave dots on
both maps. The maintainer asked to fold the switch into "Mobs as faces".

## L-85 radar mob faces (2026-10-02)

Builds `8bfc845` to `f225a72`, local world, by the maintainer, mobs from
spawn eggs. Final state on `f225a72`: faces (opt-in, default dots) are
right for the mobs common so far: zombies, skeletons, creepers, spiders,
endermen, villagers, farm animals, cats and others; the outline is steady
while moving; the hold key flips faces and dots. Known gaps, left for
L-86: silverfish and tadpoles stay dots (no head part), camel and hoglin
faces doubtful (rotated head bones). The snow golem and shulker, made dots
in `f225a72`, go back to faces at the maintainer's request (next build).

## L-84 IME composition in text fields (2026-10-01)

Build `384c751` (DLL `78121a9a...b24c291d`), local world, by the
maintainer: Japanese typed and converted into shape and waypoint names, the
world map panel, the add prompt and the settings search leaves only the
converted text; Latin typing and Backspace unchanged.

## L-83 swatch color choosers (2026-10-01)

Build `dae926d` (DLL `3e45fef9...be34e03e4`), local world, by the
maintainer: the 4-point checklist passed (12 and 4 swatches with the
chosen one framed, click and arrow keys, drafts, docked layout). Found
while testing: IME composition text left in name fields (L-84).

## L-83 settings order and keymap rules (2026-10-01)

Build `964167c` (DLL `f69a88f0...63eb0091`), local world, by the
maintainer: the 7-point checklist passed (section order, automation
status under Info & overlays, the nine toggle keys bindable with toasts,
Add a waypoint and Open the world map as child rows with bindings kept,
Open the Shapes screen under Shape drawing with the screen's key link,
"Durability numbers", the Hotkeys list).

## L-82 seed link zoom (2026-10-01)

Build `ab837d8` (DLL `382e1985...45dd3d4d`), local world, by the
maintainer: ChunkBase opens at about the world map's scale zoomed in and
out, and at its closest zoom when the map is closer than 8 pixels per
block. No problem found.

## L-82 seed link (2026-10-01)

Build `46d947b` (DLL `f00d6ff3...7b1cf3a`, trace options off), by the
maintainer. Local world: the copied seed matched the world settings; "Open
this place in ChunkBase" opened Bedrock 26.50 - 26.52 with the seed, place
and dimension, and its terrain matched the recorded map; the setting hides
both items. A friend's server: the seed arrived, matching the one its
owner had shared and the scenery; the minimap and world map worked there
as far as looked at (`server.properties` not compared). Asked: carry the
map's scale into ChunkBase's zoom (added after this build).

## L-60 world map side panel and sidebar entry (2026-10-01)

Build `e6781e5` (DLL `f2dcd9cb...617610b5d`, trace options off), local
world, by the maintainer: the 9-point checklist passed (one-row top bar in
the Nether, "Center on me" returning the layer, the sidebar entry and
returning to the settings, viewing with the feature off, the side panel's
list, selection, editing and adding, the death point, "Open in screen" and
"< Map", zoom about the pointer). Remark for L-83: the side panel nearly
duplicates the Waypoints screen.

## L-60 world map bars and delete button (2026-10-01)

Build `3a8e246` (DLL `d6b7ff7b...0be597fa`), local world, by the
maintainer: the 4-point checklist passed (buttons hold their text, the
Nether layer on a second row, hints in the bottom bar, only the delete
button arms). Reported: the wheel zoomed about the top-left corner, not the
pointer (wheel events carry no position; fixed in `b4a61eb`, not checked
yet). Raised for a design discussion: see BACKLOG L-60 world map, "Open
after the maintainer's first use".

## L-60 world map first build (2026-10-01)

Build `b3fb28d` (DLL `ee4a0c78...fd0b31b`, trace options off), local world,
by the maintainer: mostly fine (map screen, recording, Nether layers seen
in a screenshot). Found: text overflowing the top bar's buttons; the
Nether's top bar too full to fit; no control hints in the bottom bar; the
saved map's delete armed from anywhere on its row. Fixed in the next build
(buttons as tall as settings key caps with the locale text inset; the
Nether layer moves to a second row when the bar is full; smaller hints
right-aligned before the scale bar; only the delete button arms).

## L-60 Waypoints screen (2026-10-01)

Build `8714c20` (DLL `c0cbeeea...fa09525`, trace options off), local world,
by the maintainer: the 14-point checklist passed (sidebar item and key,
list order and distances, shown switches and "Show all", renaming,
stepped and typed coordinates with the range warning, Move here, color,
two-press delete, keeping and deleting the death point, "+ Add here",
docking, the key settings link, keyboard use, cursor returned on close).
Remarks for a later settings review (L-83): the Waypoints screen key sits
under Map while the other sidebar screens' keys are under General, and its
color chooser differs from the add prompt (and the demo) and from Shapes.

## L-60 waypoint world markers smooth (2026-10-01)

Build `189c554` (DLL `8288e78a...acf645`, trace options off), local world,
by the maintainer: world markers move smoothly while walking, strafing and
turning, near and far; no problem found.

## L-60 waypoint world marker position (2026-10-01)

Build `d71d35e` (DLL `eb0a6c7b...a9f9f5`, trace options off), local world,
by the maintainer: markers now sit at their places in first and third
person, Zoom, Freelook and FreeCamera, and while jumping, running and
flying; the three-way "Show in the world" with its key works. Remaining:
markers visibly jitter while the player moves (positions were rounded to
whole GUI units, several screen pixels each).

## L-60 waypoints 5b (2026-10-01)

Build `d519dd9` (DLL `a50d00de...93bb02e2`, trace options off), local world,
by the maintainer: red death cross, distance limit, hold key, "Show in the
world", names near the crosshair and the cross-dimension option behaved
as described. Problem: world markers sat in the wrong places (bunched
toward the screen center) while the minimap was right. The log showed the
camera copied in setupCamera at 0, 0, 0: that pass is camera-relative, so
waypoints were projected from the world origin. Also: "Show in the world"
and the hold key were far apart in the list, and a hide-while-held key does
not allow showing markers only while a key is held.

## L-60 waypoints 5a (2026-10-01)

Build `3658749` (DLL `819f883c...c88c8aa3`, trace options off), local world,
by the maintainer: the whole 5a checklist passed (settings rows, the add
prompt with Japanese input, Backspace, Ctrl+A, Tab/Shift+Tab and clicked
colors, Enter adds with a toast and returns the cursor, Esc cancels,
minimap diamonds and edge clamping on square and round maps, persistence
across re-entry and per world, the death cross moving on each death and
staying when recording is off, the 1/8 option in the Nether, hiding).
Not checked: a server (no BDS run). Request: the death cross was white and
read as a passive mob's dot; a red one was asked for.

## Dappled Forest biome name (2026-10-01)

Build `022fd6a` (DLL `6d4f6f07...dd5873`, trace options off), local world,
by the maintainer: the minimap line and the Info HUD show the name for the
Dappled Forest; no other biome was seen with its raw id.

## L-60 minimap wobble fixed (2026-10-01)

Build `adf0d35` (DLL `b7a4e34e...a708cd`, trace options off), local world,
by the maintainer: radar dots no longer wobble while running, at the
checked ranges and with turning on; the arrow stays centered; FreeCamera
fine. (`11fc6a1`, interpolation alone, still wobbled.) Found on the way:
the biome line showed the raw id `minecraft:dappled_forest`, and the Info
HUD's "name (id)" style showed only the id there; other biomes had names.
Cause: Lamium's built-in biome names lacked this newer biome (the game's
language files carry no biome names).

## L-60 map settings split, enlarged markers (2026-10-01)

Build `82f24fa` (DLL `8a178595...cf6bcc0`, trace options off), local world,
by the maintainer: markers keep their size while enlarged; the Map category
shows Minimap, Map text, Cave view and Radar as separate features; the cave
view key works from its row. Problem: while running, radar dots wobble left
and right against the map (the center used the player's smoothly updated
position, mobs their ticked positions).

## L-60 radar rows, dot size, invisible option (2026-10-01)

Build `8d9a552` (DLL `f89c45de...cdcc6f8`, trace options off), local world,
by the maintainer: distinct row names, smaller dots on wide ranges and the
invisible option work. Problem: while enlarged, the arrow and dots grew
with the map (twice their normal size; measured against a square dirt
patch), because marker sizes followed the texture size. The settings list
held one "Minimap" feature with 19 children, the radar and its kinds on
the same level; the maintainer asked for a split.

## L-60 minimap radar (2026-10-01)

Build `bc4a7ae` (DLL `e4cd3598...e3d52b4d`, trace options off), local world,
by the maintainer: hostile dots red and passive dots white with black
rings; dots 8+ blocks above or below fainter; invisible mobs hidden;
positions right with rotation, round map, enlarged map and FreeCamera; no
stutter near a village. Not checked: other players on a server (no BDS
run). Problems: the per-kind rows all read "レーダー" (the settings list
cuts the name at the first ": ", and the labels were "Radar: players: {}");
at wide ranges the dots crowd the map because they keep their size; no way
to show invisible mobs (wanted as an option).

## L-60 minimap, cave holes, finer steps, shimmer (2026-10-01)

Build `fe94370` (DLL `45256bae...fe0c7c`, trace options off), local world,
by the maintainer: the five-point checklist passed with no problem (Nether
cave view without holes, eleven range steps with the old range kept, 1 %
size steps with a readable value, no shimmer at 512 or enlarged in the End,
enlarged map without edge flicker).

## L-60 minimap, size setting and enlarge key (2026-10-01)

Build `42c8e10` (DLL `7a2e727d...6941c`, trace options off), local world,
by the maintainer (screenshots at 40 % size, enlarged, layout 100 %): the
size setting and the hold-to-enlarge key work, key assignment fine, no
other checklist problem. Problems: in the Nether cave view many areas stay
transparent (unloaded) even after waiting, chunk-shaped; the range and
size rows are hard to set finely and the size value was cut off ("画面の
高..."); in the End at 512 blocks small features (chorus plants, the purple
end city) shimmer and shift by a pixel as the player walks.

## L-60 minimap, calmer cave view (2026-10-01)

Build `6685cb2` (DLL `3a5e185c...e883d503`, trace options off), local world,
by the maintainer: the cave view is easier to read and the checklist found
no other problem. The log confirmed the earthy unloaded ground was the
client's `minecraft:client_request_placeholder_block`. Remaining: in the
Nether cave view some areas looked transparent (chunks whose window held
only open space were still counted as not received), so "not loaded" and
"hollow" could not be told apart. Requests: a permanent map-only size, and
a key that enlarges the map while held. Log: 300-1100 composes per 30 s at
0.7-1.3 ms, chunk scans 0.02-0.12 ms on average.

## L-60 minimap, cave view and FreeCamera (2026-10-01)

Build `0bca475` (DLL `5ea52295...c0d3a`, trace options off), local world,
by the maintainer: FreeCamera following works; Nether colors are right in
the cave view. Problems: ground not loaded yet still shows an earthy color
(End screenshot: a brown speckled area beside the main island), so the
dark fill does not reach it; the cave view follows the player's Y exactly
and changes on every small height change. Log: in cave view about 1700
composes and uploads per 30 s (every frame, 1.0-1.2 ms each), chunk scans
0.02-0.15 ms on average; surface view 0.9-1.6 ms composes.

## L-60 minimap, texture colors (2026-10-01)

Build `73729d3` (DLL `cda2ba68...577b17a`, trace options off), local world,
by the maintainer: in the Overworld (flower forest, savanna, plains, ice
biomes, birch and others) the colors match the world; the lines and compass
letters read well; no other checklist problem. Problems: the Nether map
shows the bedrock roof (gray and brown), not the ground where the player
walks (expected: no cave view yet); in the End and elsewhere the not-loaded
area is transparent and looks wrong; with FreeCamera the map follows the
player, not the camera. Turning smoothness not reported as a problem.

## L-60 minimap, first build (2026-10-01)

Build `84195f0` (DLL `7404771e...ad78d58`, trace options off), local world,
by the maintainer, following the 12-point checklist: the texture path works
(map shown, scrolls and fills nearest first, zoom keys, rotate/round/lines/
compass, layout editor, dimension changes and re-entry, Debug View hiding,
resize and pack changes, off, clean process exit) with no blocking problem.
Problems: (1) the colors differ strongly from the world in the Overworld and
the Nether: the blocks' own map colors are the vanilla map palette (grass
plains came out tan, ice spikes purple-blue, ocean noisy dark blue), not the
"representative color per block with biome tints" the spec asks for;
(2) details of the look and the text positions feel off (not itemized yet);
(3) with "Turn with view" on the map turns in visible steps. Log over 36 s:
texture upload 0.06 ms, 241 composes averaging 1.99 ms (terrain recomposes
at most 30 times a second, so turning steps), 10018 chunk scans averaging
0.063 ms, 2450 tiles kept. FPS cost not reported as a problem.

## Detached camera turn sensitivity with Zoom (2026-09-30)

Build `30f0c4a` (DLL `29005cc0...8a3cdb`, trace options off), local world, by
the maintainer: FreeCamera + Zoom and Freelook + Zoom were checked against
ordinary Zoom and the turn sensitivity now matches; the unzoomed detached
look and head/body/pitch isolation were unchanged; releasing Zoom restores
vanilla sensitivity. No problem found. Individual mode/menu/focus
combinations and controllers were not reported separately.

## L-80 Zoom 2x floor and L-81 range warning (2026-09-30)

Build `1cdb481` (DLL `03de0853...9478`, trace options off), local world, by
the maintainer: the Magnification setting stops at 2x with the arrows and a
typed 1 is rejected with the warning; the held-Zoom wheel stops at 2x; an
out-of-range number shows the warning, which goes on another row, another
tab or another edit, and the same in Shapes fields and shapes; correcting the
value in place clears it and saves. No problem found.

## L-73 step 10, settings edits finish with the frame (2026-09-30)

Build `084b424` (DLL `ac7db25e...ac4b`, trace options off), local world, by
the maintainer: a typed number is kept with Enter, Esc and Tab (Esc does not
close the screen) and survives reopening; Ctrl+F while editing keeps the value
and moves to search; Shapes name and number fields finish the same way; Enter
in search selects the first row. No problem with the change. Found in
passing: L-80 (Zoom setting and wheel lower limits differ) and L-81 (the
out-of-range warning stays after its edit ends).

## L-73 CameraSessions rename (2026-09-30)

Build `e408fbb` (DLL `d9b18518...1480`, trace options off), local world, by
the maintainer: no start errors in the log; Zoom with wheel, Freelook and
FreeCamera start and end as before. No problem found.

## L-73 periodic input fix recheck (2026-09-30)

Build `221edcb` (DLL `e8b025e6...05fa`, trace options off), local world, by
the maintainer: Auto Attack and Auto Use click again at their interval and a
physical press takes priority; Fake Offhand uses its slot on right click; no
"unavailable" or "could not start" in the log. No problem found.

## L-73 refactor steps 1-8 (2026-09-30)

Build `1e06d70` (DLL `d133d4b0...6e7`, trace options off), local world, by
the maintainer. Seen working: Tool Switch fetch from inventory pausing and
resuming while held; Tool Protection swap and stop toast; Zoom, Freelook
(including F5 before ending), FreeCamera (flight, body, F5 locked, start from
third person, dimension change and leaving the world); no "could not start"
in the log. Breaking Restriction resumes on an allowed block after a
forbidden one in most cases, but a held attack occasionally does not break
the allowed block; whether this predates the build is unknown. Auto
Attack/Use switched and changed mode but never clicked: 5e877e5 started
periodic input in enable(), after the client had registered its button
handlers. Fixed in `221edcb`; not yet rechecked. Fake Offhand, which uses the
same handlers, was not checked on this build.

## 0.1.4 released (2026-09-30)

Tag `v0.1.4` at `bb69057` (annotated), pushed after the smoke test below;
its CI run 36709787394 passed. GitHub release "Lamium 0.1.4" (not a
pre-release, marked latest) carries `Lamium-0.1.4-client-windows-x64.zip`;
GitHub's asset digest equals the local archive,
`6DE6DB2820837CACEFEE2E8B0722AFFB899271A72366BA485453A085A2CDF5E5`
(DLL `7315B7C915167D9D93879F5699F95F54107E3CEC8AF6A7F13E5E8C1A1A8E7E66`).
The development instance still runs the `4d25424` release candidate; the
label-only change in `fc090fa` was not rechecked in game. LeviLauncher and
Bedrinth availability waits for the registry PR.

## 0.1.4 release build smoke test (2026-09-30)

On the release build `4d25424` (DLL `5954FAF0...0AE3`) the maintainer checked
and found no problem: Debug View shows 0.1.4; Hand Restock with blocks, food,
eggs, a remainder and held use; an offhand totem refill; Tool Protection swap
and stop; Tool Switch inventory fetch; Auto Elytra by key and firework jump
with the chestplate return; the dedicated Hotkeys/Shapes/HUD layout openers.
They reported the Japanese hotkey labels "FreeCamera の速度を上げる/下げる"
(the feature is フリーカメラ). `fc090fa` aligns those labels and other label
spellings (切替/切り替え, Night Vision, Tool Switch) with the feature names;
text only, not rechecked in game. The rebuilt release archive has SHA-256
`6DE6DB2820837CACEFEE2E8B0722AFFB899271A72366BA485453A085A2CDF5E5`, DLL
`7315B7C915167D9D93879F5699F95F54107E3CEC8AF6A7F13E5E8C1A1A8E7E66`.

## 0.1.4 release build deployment (2026-09-30)

Source `4d25424` (version 0.1.4), all trace/probe options off. DLL build,
LamiumTests, LamiumNativeTests, `Check-Package.ps1` and
`New-ReleaseArchive.ps1` passed. The DLL and manifest were copied from
`bin/release/Lamium-0.1.4-client-windows-x64.zip` (SHA-256
`0A245E3EAF01FB700D579A0F24F30D2AD0C5C51198FDB3AA5A364E79BBEF7F1B`); zip and
installed copies match. Minecraft was closed. Awaiting the pre-release smoke
test; no tag has been pushed.

- DLL: `5954FAF01EF67B0D7796E315452DE07D32840952852347D8D56828F6C9520AE3`
- PDB: `E81513B4204EF598AF7A44ACE14CF413E6391934867FF5361B6A0877CB9A7206`
- Manifest: `A9CC0CD1D19FF864793EEAA28FE7442D8B5CC678A1775BDB42CC0C9DE413DAE8`

## Pre-release default revision deployment (2026-09-30, unchecked)

Source build `464bae8`, all trace/probe options off: Hide effects starts with
the master off and all seven effects selected; the durability HUD's offhand
and armor rows default on (the HUD stays off). Existing settings files keep
their saved values, so the new defaults show only after a category reset or
with a fresh file. Minecraft was closed; DLL build and LamiumTests passed.

- DLL: `62B9FF6BCA6D4936AC05191FD80F288C85C30DA3453398C1AF2852B72D76D297`
- PDB: `98E2E6DFA204FA84A9C7D25121D5CC79F0E64C8E9D9E244FC83BE7042B6FE5B0`
- Manifest: `229599F5D5651C87E2856028C3403FB3E4CB14034C1FE1FAADDA946C30A6D192`

## Normal build after parking the frame switches (2026-09-30)

On `449c5c2` the maintainer found no problem: Hide effects shows seven
children without the pumpkin/spyglass rows, water/lava/powder snow hiding
still works, and non-leather icons in the durability HUD and shulker box
preview are correct (leather's missing layer is the parked known issue).

## Normal build after parking the frame switches: deployment (2026-09-30)

Source build `449c5c2`, all trace/probe options off, replacing the frame
count trace build. Minecraft was closed; DLL build and LamiumTests passed.
No runtime result yet.

- DLL: `461EA9C557F692DDF0EDB425069FB41A0E091B5495784EA8D850104B4AFE2633`
- PDB: `21FC1C71D2B5F3BDE33F53CDF05ADEA8614246858E8349029FD20E202B2387EA`
- Manifest: `229599F5D5651C87E2856028C3403FB3E4CB14034C1FE1FAADDA946C30A6D192`

## Frame count trace result (2026-09-30)

On the `f7d9b88` trace build: pumpkin gate 20:03:25-32 (341 frames), scoping
gate 20:03:35-40 (284 frames). Only the XP bar custom renderer's per-frame
count changed; the vignette renderer drew no image. The maintainer parked the
leather icon issue.

## Frame count trace deployment (2026-09-30, unchecked)

Trace build from `f7d9b88` with `effects_trace` on (saved in
`bin/Lamium-effects-trace`); icons are back on `renderGuiItemNew`. Minecraft
was closed; the local configuration is back to all trace options off.

- DLL: `DBE93D183204BAF7EAFA7D83074DCFB6DEABCBC4B3C8944407BB2666AC03411E`
- PDB: `EAB3ABDFC13CD54832528BB42907D5E0443FB8798F64AC7FB456FB86979EB6E5`
- Manifest: `229599F5D5651C87E2856028C3403FB3E4CB14034C1FE1FAADDA946C30A6D192`

Requested: the same frame procedure; share `research L-42 count` and
`research L-42 vignette` lines.

## UI context frame trace result (2026-09-30)

On the `33d0bef` trace build: the pumpkin gate opened 19:57:42-50 and the
scoping gate 19:57:52-58; no gated key of any kind was logged. The chunked
item icon pass (`e987861`) filled leather armor slots in the shulker box
preview and the durability HUD with a flat tint square; reverted in
`a38eeaf`, so leather icons are back to missing their undyeable layer.

## UI context frame trace deployment (2026-09-30, unchecked)

Trace build from `33d0bef` with `effects_trace` on (saved in
`bin/Lamium-effects-trace`); it also carries the chunked item icon pass
(`e987861`). A first copy failed while Minecraft was running (only the PDB
was replaced); after the maintainer closed it, all three files were copied.
The local configuration is back to all trace options off.

- DLL: `A34CF7414A85D706CA8B79699CC9542A499DCDACCD2CE0A2892086D15807F67C`
- PDB: `44E592A86FEC9FAB1E64D5538D652AD626B3D8DF73D919C8C6AE15B7B156CC8C`
- Manifest: `229599F5D5651C87E2856028C3403FB3E4CB14034C1FE1FAADDA946C30A6D192`

Requested: the same frame procedure; leather armor in a shulker box preview
and the durability HUD.

## Gated frame and leather chunk trace result (2026-09-30)

On the `f7d49cf` trace build (packs removed) the maintainer confirmed the
revised shape glyphs, the list glyphs and the durability HUD without the
elytra row, then restarted and followed the frame procedure (equipping went
through the inventory).

- L-61: vanilla slots drew leather armor, diamond tools and flat items with
  `renderGuiItemInChunk` type 2 (damaged diamond tools also 4 and 5), and
  diamond blocks/ores with type 0. Lamium now draws non-block icons with type
  2 in the durability HUD and container previews (`e987861`), unchecked.
- L-42: see VISUAL-EFFECTS.md; no new mesh/blit/tessellator key while the
  pumpkin was worn, and no scoping gate line.

## Gated frame and leather chunk trace deployment (2026-09-30, unchecked)

Trace build from `f7d49cf` with `effects_trace` on (saved in
`bin/Lamium-effects-trace`); it also carries the L-74 glyph/list changes, the
durability HUD without the elytra row, and icon position rounding. The
maintainer added that leather armor's undyeable layer is missing, dyed or
not, in both the durability HUD and the shulker box preview. Minecraft was
closed before deployment; the local configuration was reset to all trace
options off afterwards.

- DLL: `67EF0AF23AA925161EE0DA48DE6E5C2F62CC40D66B60A26328920283F64C207A`
- PDB: `085093CA934A5B9F7A1D7F58BD00863811D5A5E5B03C590D37583E247FE671CF`
- Manifest: `229599F5D5651C87E2856028C3403FB3E4CB14034C1FE1FAADDA946C30A6D192`

Requested: ten seconds without frames, a carved pumpkin worn for five, then a
spyglass scoped for five; open the inventory with leather and diamond armor
in slots; share `research L-42 gated` and `research L-61 chunk` lines.

## L-74, L-61 and L-42 frame/immersion playtest (2026-09-30)

Build `87f11cd`, DLL `D0DCC431993409CC6BBC6D98DC39CB7299C3153AFFB24E01706820820A717E87`,
custom resource packs removed by the maintainer.

- L-74: the ten types show distinct glyphs in the new-shape list. The
  maintainer questioned the cylinder, sphere and plane glyphs and asked for
  the list's color square to be the shape's own glyph in its color
  (both done in `38623de`, unchecked).
- L-61: held-only display, all three looks, offhand/armor row order, the
  gliding elytra row with its outline, and layout editor placement passed.
  Leather armor icons showed see-through stripes (helmet: a vertical one in the
  middle; leggings: a horizontal one). The maintainer finds the elytra row
  too prominent and may park it together with the flight time.
- L-42: underwater, lava and powder snow (fog and frost frame) hiding passed;
  lava with and without Fire Resistance was not checked separately. The
  carved pumpkin and spyglass frames stayed visible. (The spyglass was first
  reported as hidden and corrected by the maintainer.) The log has route lines
  for fog media 256, 128 and 64 only, no mesh route for either frame.

## L-74, L-61 and L-42 frame/immersion deployment (2026-09-30, unchecked)

Source build `87f11cd`: ten shape type glyphs (L-74); the durability HUD
without flight time (L-61); carved pumpkin, spyglass, underwater, lava and
powder snow children under Hide effects (L-42). Minecraft was closed before
deployment. All trace/probe options are off. The DLL build, LamiumTests and
LamiumNativeTests passed. The maintainer will remove the custom resource packs
before checking, because the HUD element and frame routes may depend on them.
No runtime result yet; nothing was pushed.

Source and installed copies match:

- DLL: `D0DCC431993409CC6BBC6D98DC39CB7299C3153AFFB24E01706820820A717E87`
- PDB: `3326021141E017F86EF0A7E9FA53A7A04A71164365C9E460025210D1B26496BA`
- Manifest: `229599F5D5651C87E2856028C3403FB3E4CB14034C1FE1FAADDA946C30A6D192`

## Zoom retention and nausea help playtest (2026-09-30)

Build `e5ee44a`, DLL `3E106847C1FAA5619A9B63B5E2E562B50ADB31C0A2DA3BAE12968433A059A0A8`.
The maintainer confirmed: a wheel-adjusted Zoom level held during FreeCamera
stays after a FreeCamera speed key and the speed toast appears; a magnification
changed in settings applies to the next Zoom; camera and Zoom otherwise showed
no problem. The Japanese nausea help overflowed its description area (the
final sentence about the effect and status icon was cut off); the English text
barely fit. Both final sentences were removed in the next commit. Nausea color
hiding itself was not reported separately on this build.

## Zoom retention and nausea help deployment (2026-09-30, unchecked)

Source build `e5ee44a`: Zoom keeps its held state and wheel level when an
unrelated camera setting (FreeCamera speed key) is saved; the nausea mesh
filter checks its screen scope first; the nausea help names Settings >
Accessibility > Screen distortion at 0. Minecraft was closed before
deployment. All trace/probe options are off. The DLL build and LamiumTests
passed. No runtime result yet; nothing was pushed.

Source and installed copies match:

- DLL: `3E106847C1FAA5619A9B63B5E2E562B50ADB31C0A2DA3BAE12968433A059A0A8`
- PDB: `8ED78A7EF6588F03997977FE703C223A069753B0AD5C05597C0BFDC37901ECC0`
- Manifest: `229599F5D5651C87E2856028C3403FB3E4CB14034C1FE1FAADDA946C30A6D192`

Requested checks: help text for the nausea child in Japanese and English;
wheel-adjusted Zoom level kept across a FreeCamera speed key; a changed Zoom
magnification applies to the next Zoom; nausea color hiding still works.

## L-42 nausea child normal-build playtest (2026-09-30)

Tested build `b239eb9`, all trace/probe options off, DLL SHA-256
`218F05A547F765E27AF78114D5E04A09EF0F10A2C9B6FAB7F1F6F2F7457E3FBA`.
Minecraft 1.26.51.01 / LeviLamina Client 26.51.5 / Windows x64. The last
reported configuration is Fancy with the previously recorded global HUD pack;
no updated graphics/pack stack was supplied.

The maintainer confirms the supplied three checks: with vanilla Screen
Distortion zero, enabling the child hides the green nausea effect; individual/
master off and the assigned key restore it; effect state, icon and vanilla
preference remain unchanged. Restart persistence, other modes/packs and
lifecycle/owner cases were not reported. Remaining five L-42 effects are
unimplemented. The maintainer requested a handoff to another agent because of
subscription usage; no further game changes or deployment are part of this
handoff preparation. See HANDOFF-CAMERA-VISUALS.md for the continuation brief.

## L-42 nausea child normal-build deployment (2026-09-30, unchecked)

Source build `b239eb9`. Minecraft was closed before deployment. All trace/probe
options are off. The ordinary DLL build, LamiumTests and LamiumNativeTests
passed. No runtime hiding/restoration result for this new child has been
supplied. The previous camera playtest is recorded below; nothing was pushed.

Source and installed copies match:

- DLL: `218F05A547F765E27AF78114D5E04A09EF0F10A2C9B6FAB7F1F6F2F7457E3FBA`
- PDB: `EF801D8F55F9BEFD216BF886A4EB1EBA16A3EC7923E3E4C1A0C13925206A51F0`
- Manifest: `229599F5D5651C87E2856028C3403FB3E4CB14034C1FE1FAADDA946C30A6D192`

Requested checks: with vanilla Screen Distortion zero and a visible green nausea
effect, enable the new child and verify only the color overlay disappears.
Child/master off and the optional key should restore it immediately, while the
effect/icon and vanilla preference remain unchanged. Remaining pumpkin/spyglass
and immersion switches are unimplemented; no repeated passive frame/medium
observation is requested this step. Additional static inspection found SDK
26.51.5's FancyFrameRenderer/Resources, FullscreenEffectDescription,
FullScreenOverlayObject and InsideBlockEffect declarations empty. They provide
no new callable per-effect contract or safe fields for those remaining effects.

## L-78 playtest and L-42 green-overlay observation (2026-09-30)

Tested build `d3f0293`, effects trace enabled, DLL SHA-256
`675AB9C6A76CDAEDD95D15A50DDE2BFED9D2DEB39457DDFB70673FC048960E3F`.
Minecraft 1.26.51.01 / LeviLamina Client 26.51.5 / Windows x64; last reported
graphics/global-pack configuration is Fancy with the previously recorded HUD
pack. No new mode/pack stack was supplied.

The maintainer confirms the supplied Toggle Player/World Lamium-view checklist:
position/orientation retained and flight paused in the panel. Inventory,
window movement and explicit release also behave normally. L-78 is closed;
Hold, detailed Escape/Close and lifecycle/release-build cases remain unchecked.

They displayed pumpkin/spyglass frames and, with vanilla Screen Distortion
set to zero, confirmed the visible green nausea effect for the requested
intervals. The log records stage 1, material ui_texture_and_color_blur_additive,
texture textures/misc/nausea. Its preceding textures/ui/nausea_effect draw
uses a generic UI material at stage 0 and remains classified as a status icon.
The new production nausea filter is based on the exact green draw pair;
this playtest did not test hiding it.

All sixteen trace hooks installed. Entry records confirm inGameRender,
spanMesh, rectBlit and variantBlit callback reach. Sampled span draws include
debug/span[0] and holo_hand_pointer/span[0]. There is no pumpkin/spyglass
candidate, screen candidate or entry record for textureBlit, postLevelRender,
vignetteRender or tessellatorIntercept. Absence is limited to observed paths
and cannot establish that an effect is not drawn. No immersion repeat was
requested or reported in this test.

## L-78 fix and L-42 mesh-span trace deployment (2026-09-30, unchecked)

Source build `d3f0293`, including the shared Lamium view-opener fix `1d28754`.
Minecraft was closed before deployment. The ongoing authorized investigation
build has only effects_trace enabled; local configuration was then reset to
all trace/probe options off. Both trace and ordinary DLL builds passed, as did
LamiumTests and LamiumNativeTests. No new runtime result has been supplied.
The repository has no uncommitted changes after recording this deployment;
nothing was pushed.

Source and installed copies match:

- DLL: `675AB9C6A76CDAEDD95D15A50DDE2BFED9D2DEB39457DDFB70673FC048960E3F`
- PDB: `15B77B0BD00303C7CDC0960ED7A0AC2205D44D9CBBA411F21EFD53EE8175129C`
- Manifest: `229599F5D5651C87E2856028C3403FB3E4CB14034C1FE1FAADDA946C30A6D192`

Trace-disabled DLL retained in the normal build output, SHA-256
`20781CF771EE3F769E521076E9EDFD82C511AE0D934F3F7091E4E501B0786B9F`.

Requested checks: Toggle FreeCamera retains position/orientation through
Settings, Hotkeys, Shapes and HUD layout, with flight paused while the view
owns input, in Player and World reference. Closing resumes fresh input;
inventory/focus behavior and explicit off should remain normal. Hold input
ownership-loss cleanup also needs a regression check. For L-42, observe
pumpkin/spyglass frames and nausea's green effect for at least five seconds
each, setting vanilla Screen Distortion to zero for the latter. No repeat
immersion observation is requested this step. The additional mesh-span,
tessellator and entry-reach hooks are read-only; six hide switches remain
unimplemented pending their render contracts.

## L-42 follow-up overlay clarification (2026-09-30)

For the `d20fdf8` observations, the maintainer confirms that pumpkin and
spyglass frames were actually visible. Nausea produced the warp: vanilla
Screen Distortion had not been set to zero, so the green color effect was
not observed. The recorded generic nausea texture cannot establish its color
overlay path. The next color-effect check needs that vanilla preference;
Lamium will not change the preference as part of the hide switch.

## L-77/L-42 follow-up playtest and Lamium view-entry defect (2026-09-30)

Tested build `d20fdf8`, effects trace enabled, DLL SHA-256
`79066B8D5ABDC38F6B119A94D2ED1D45783F78F3D3A0FB2195739BE8F2689BBB`.
Minecraft 1.26.51.01 / LeviLamina Client 26.51.5 / Windows x64; reported
configuration is Fancy with the global HUD pack recorded in the prior entry.
No updated graphics/pack stack was supplied for this follow-up.

The maintainer confirmed improved elytra motion in World reference, normal
reference switching and release. The log records the native interpolation
writer being reached. L-77 is closed, with broader release checks retained.
They confirmed boss bars/names hidden and switches behaving as expected;
optional key and unrelated HUD elements were not reported separately.
They also reported displaying the remaining requested effects for five seconds.

All fourteen trace hooks reported installed. Settled distance-fog endpoints
are about 29.5375 in water, 0.64 in lava and 2 in powder snow; recorded density
is zero. Water's one-second endpoint is 14.6841. Lava/fire resistance selects
types 3/4, but the resolved phase budget is shared for mediumBits 2, so the
fire-resistant resolved values are not separately established. Frozen drawing
uses stage 1/on_screen_effect/textures/ui/frozen_effect. There is no screen-blit
candidate or vignette route, and no pumpkin/spyglass draw candidate. The nausea
texture still uses a generic UI material at stage 0, without proving a color
overlay. Absence from the sampled paths does not prove absence of drawing.

New defect L-78: opening Lamium settings with L returns the FreeCamera position.
The maintainer wants it preserved, consistent with earlier inventory/focus
behavior. Static inspection found the shared Lamium view opener calling the
full camera reset; the input suspension path already preserves FreeCamera.
No result for the new opener fix has been supplied.

## L-77 candidate fix and L-42 boss/follow-up trace deployment (2026-09-30, unchecked)

Source build `d20fdf8`, including the camera interpolation fix `1e18b64`.
The maintainer's existing request to apply the investigation build covers
this follow-up. Minecraft was closed before deployment. The saved trace build
has only `effects_trace` enabled. All trace/probe options were then reset off;
the normal DLL, LamiumTests and LamiumNativeTests build/run checks passed.

Trace copy source and installed destination match for all three files:

- DLL: `79066B8D5ABDC38F6B119A94D2ED1D45783F78F3D3A0FB2195739BE8F2689BBB`
- PDB: `61F4CA1BBD352F33F54D80711E34FB5A83A1A28CB90E1F51E0312468E8F5F299`
- Manifest: `229599F5D5651C87E2856028C3403FB3E4CB14034C1FE1FAADDA946C30A6D192`

The trace-disabled normal DLL remains in the build output, SHA-256
`751370BD6DD2E238270872E8DB68FE8D7293E05948D3DB32CFB3194220B7EF90`.
No runtime result for the new code has been supplied and nothing was pushed.

Requested checks: rapid elytra/body motion in World reference, live reference
switching and release; boss bar/name hiding with its child, master and optional
key, keeping other HUD elements visible and restoring immediately. For the
remaining six effects, observe pumpkin, spyglass and nausea, then water, lava
with/without fire resistance and powder snow/freezing for at least five
seconds each. Start with the reported Fancy graphics/current pack selection.
The expanded screen-stage/blit and settled-fog logs are research evidence;
the remaining six hide switches are still unimplemented.

## L-42 trace environment clarification (2026-09-30)

The maintainer reports Fancy graphics for the `43c4211` observations.
The instance's active global-resource-pack metadata selects Déesse UI Pack
1.3.9; its UUID matches the installed manifest. This agrees with the custom
HUD routes in the collected log. Metadata establishes the global selection,
not the complete pack stack of each tested world. No pack implementation or
assets were used to derive Lamium code.

## L-76 world reference playtest and L-42 observations (2026-09-30)

Tested build `43c4211`, `effects_trace` enabled, deployed DLL SHA-256
`9016870FD47B8670BCCA8C30D2BB21CD15BE263441D199BE79E256720096A542`.
Configured Minecraft 1.26.51.01 / LeviLamina Client 26.51.5 / Windows x64.
Graphics mode and resource-pack details have not yet been supplied. Custom
HUD routes are present in the log.

The maintainer confirmed world position retention during body movement, live
Player/World switching without a jump and restoration on release. They also
reported frequent visible position corrections during rapid body movement,
including elytra flight. This is an observed defect, tracked as L-77; the
previous after-UI compensation path is not smooth enough at high speed.

They completed the requested boss bar, pumpkin, spyglass, nausea, water, lava/
fire-resistance and powder-snow/freezing observations. All eight trace hooks
reported installed. The log records boss text/sprites under hud_screen /
boss_health_panel / boss_hud_panel / boss_health_grid with empty/filled progress
bar textures. Water selects distance/density type 2, lava type 3 and fire-
resistant lava type 4; powder snow selects distance type 5 and density type 1.
The frozen view mesh uses material on_screen_effect and textures/ui/frozen_effect.
textures/ui/nausea_effect used with ui_textured_and_glcolor may be the status
icon; it does not establish the nausea vignette path. No pumpkin or spyglass
draw candidate was recorded. Absence from the sampled hooks is not evidence
that an effect is not drawn. None of these seven hide switches was tested or
implemented in the tested build.

## L-76 implementation and L-42 trace deployment (2026-09-30, game check pending)

Source build `43c4211` (includes L-76 implementation `c3c5bdb`). The maintainer
explicitly selected the expanded read-only trace build for the remaining seven
L-42 effects. `effects_trace` is enabled in the deployed DLL; all other trace
and probe options were off. Minecraft was not running at deployment.
Configured environment: Minecraft 1.26.51.01, LeviLamina Client 26.51.5,
Windows x64; graphics mode has not been reported for this check.

The DLL, PDB and manifest were copied from the separate trace output; each
source/deployed SHA-256 pair matched:

- DLL: `9016870FD47B8670BCCA8C30D2BB21CD15BE263441D199BE79E256720096A542`
- PDB: `A30179F8A25F00A5285E0F7ABF07F65D8E94D823179377FB18E97C8041C5DB17`
- Manifest: `229599F5D5651C87E2856028C3403FB3E4CB14034C1FE1FAADDA946C30A6D192`

Both trace-enabled and trace-disabled release DLL builds passed. LamiumTests
and LamiumNativeTests passed. The current build configuration was reset to
all traces/probes off, and the ordinary output was rebuilt separately (DLL
SHA-256 `45B167E4CCC3D5984F00C1B164516EAEEE79F60D4796DC24818E4410B2D23409`).
No game result or runtime trace has been collected from this deployment yet.
No hide switches for the seven remaining L-42 effects were implemented.

L-76 checks supplied: select World, test body falling/knockback without camera
input and while flying, switch Player/World in settings without a jump,
check target readouts, then disable/re-enable and leave the world. L-42
observation checks: keep a boss bar, carved-pumpkin overlay, spyglass frame
and nausea visible for several seconds; enter water, lava (also with fire
resistance) and powder snow (including the freezing effect). Report graphics
mode. Hook availability and candidate draw/fog paths remain unverified until
the log is read; the trace does not suppress these effects.

## L-26 and L-42 follow-up playtest (2026-09-30, game confirmed positive)

Build `41b1ff6`, trace-disabled release DLL SHA-256
`4DBE00B81610D3EB800A703E9BE7831E1C3BAE636DFCBC0BB247EB9CC4094ED8`.
Configured environment: Minecraft 1.26.51.01, LeviLamina Client 26.51.5,
Windows x64; graphics mode and server type were not specified.

After deployment the maintainer reported checking in game and finding no
problems. The supplied checklist covered concise FreeCamera speed-action
labels; sprint sustained after key release during forward movement and ended
by strafe/backward/stop/menu/focus changes, with vertical speed unchanged;
master Hide effects off/on restoring drawing and selections; and Rain and snow
alone hiding rain splashes while ordinary water splashes and rain sound remain.
No individual case results were reported, so this is a positive follow-up
playtest rather than confirmation of every input/lifecycle combination.

Not separately reported: restart persistence, child keys while the master is
off, controllers, resource packs, graphics-mode coverage, Nether ambient
particles, world/dimension transitions and split-screen. No renderer trace
was collected; native callback coverage cannot be inferred from the result.
The seven remaining L-42 effects are still unimplemented.

## L-26 and L-42 first-step playtest (2026-09-30, game confirmed)

Build `7e72244`, trace-disabled release DLL SHA-256
`5FED25BE905503B76AB1C36E1153E3E19D64BA2D16BF50118CEC9B9107D430FC`.
Configured environment: Minecraft 1.26.51.01, LeviLamina Client 26.51.5,
Windows x64; the maintainer did not specify graphics mode or server type.

The maintainer confirmed all three supplied checks: FreeCamera speed changes
in steps of five with the bound increase/decrease keys; holding sprint doubles
horizontal motion with unchanged vertical speed; rain/snow and particles hide
independently, restore when switched off, and leave rain sounds intact.

They also observed that rain splashes follow Particles rather than Rain and
snow, and requested a revision: concise speed action names, sprint sustained
after key release only while moving forward, a master Hide effects switch
that preserves child selections, and rain splashes included in Rain and snow.
Those revisions are not covered by this result. Restart persistence, menu/
focus transitions, remapped sprint, resource packs, graphics modes, Nether
ambient particles and split-screen were not reported separately.

## L-53 Info HUD follow-up (2026-09-30, game confirmed)

The maintainer checked the L-53 follow-up (embedded English and Japanese biome
names, the Japanese angle labels and the Biome / Real time display-format
rows) in game on a current main build and found no problem. No individual
results per row were reported.
## L-68, L-62, L-69, L-70 follow-up playtests (2026-09-30, game confirmed positive)

Trace builds (`--restock_trace=y --research_trace=y`), local world, then a
light pass on dedicated BDS 1.26.51.1. Not every child option was exercised on
BDS.

- `5728561` (`2E4773E0...7632`): Auto Elytra no longer crashed; armor moves
  traced `recorded-armor value=0`, `recorded-inventory value=1`, so the armor
  setter records no action and the added one was accepted. The swap triggered
  on ordinary ground jumps and `tryStartGliding` never started a glide. Hand
  Restock had faulted for the session on a transient HUD/inventory mismatch in
  the new tick-by-tick totem watch ("Hand Restock stopped after an inventory
  error"), so totems were not refilled.
- `05648bf` (`06EA96A4...C6A`): block and totem restock worked again; the strict
  Tool Protection child kept a tool at 1 from mining on a new press and let it
  when off. The firework trigger fired on the ground jump; the key only swapped;
  gliding never started from the mod (62 of 62 tries false in the earlier run).
- `5c0597d` and `1192f57` (`FE36A5F4...3D3E`): key and firework-jump triggers,
  the firework child off (key only), the chestplate after the delay including
  sprint jumping, a hand-worn elytra followed by the best chestplate, and an
  empty chest keeping the elytra all behaved as expected. The maintainer then
  checked offhand totems, Tool Protection, tool fetching and the elytra swap
  on BDS and found no problem. Re-join after each round: no item gained or lost.

Not verified: the trace-disabled normal build, latency beyond a same-machine
server, every child option on BDS, and elytra durability replacement in flight.

## L-68, L-62, L-69, L-70 first playtests (2026-09-30, mixed)

Trace builds (`--restock_trace=y --research_trace=y`), local world.

- `04b594d` (`782CA952...1E6D`): totems refilled in the offhand and in the
  selected hand after `/damage` (`totem-event`, `totem-moved`); the offhand
  move traced `recorded-inventory value=1`, `recorded-offhand value=0`, so the
  offhand setter records no action and the added one was accepted. Tool
  Protection swapped a worn pickaxe from the inventory and kept mining, and
  stopped with a toast without a spare; a new press mined on. Inventory tool
  fetch worked on a new press only. Firework rockets cannot be used from the
  offhand in vanilla. Auto Elytra never triggered: no `tryStartGliding` call
  arrives without a worn elytra. Re-join: no item gained or lost.
- `0ca7af5` (`A36D5743...B3DE`): held mining fetched the right tool at every
  block change (also with Haste II), Tool Protection took a hotbar spare, the
  stop toast showed text only. Offhand arrows were not refilled after a bow
  shot (not investigated; the feature was dropped). Auto Elytra crashed the
  game on the mid-air jump when an elytra was in the inventory: access
  violation in `movePair` while walking the transaction's action map after
  the armor setter (crash trace 2026-09-30 02:16-02:18). Re-join: no change.

## L-66 continuous use, throwables and remainders (2026-09-30, game confirmed positive)

Trace builds (`--restock_trace=y`), local world unless noted.

- `0f52892` (`DEA9BCE8...493C`): blocks and food refilled while holding use;
  eggs never did. No server inventory update followed any egg throw, single
  or held, so every observation expired.
- `b179dae` (`0495F1AB...BC85`): with the 150 ms quiet period, single and held
  egg throws refilled (`settle-quiet`, `move-predicted value=16`). Held stew
  did not refill even once: holding use retried the leftover bowl, whose
  failed use replaced the waiting operation right after `server-confirmed`
  (confirmed with `607aa04`, trace cap raised).
- `01583e5` (`D161FCDA6B095132EF2E1CF483692E582932802826535DCA6092270610868221`):
  the maintainer reported held stew, water bucket, eggs, block placement and
  golden apples refilling as expected in the local world, then on dedicated
  BDS 1.26.51.1 with no problem found. Remainder exchange (bowl/bucket into the
  reserve's slot) was part of these checks.

Not verified: the trace-disabled normal build (the maintainer chose to defer
it to the pre-release check), latency above a same-machine BDS, screens,
focus, dimension changes and manual drops during observation.

## L-66 reserve order and hotbar sources (2026-09-30, game confirmed positive)

Builds `37E161DFF6EBDD5AED2DD146711F9D0E92ABCD1CFE575904DA1E7014FB646E27`
(commit `6d84bed`) and `6E1959D1E2D17F8A6C1C4D60B300CC245F7270D6BDBC7D79889894A614364E70`
(commit `db3c31f`), both `--restock_trace=y`, local world (and BDS for
6d84bed). The maintainer reported the checklist behavior as expected: the
largest main-inventory stack supplied the refill (a lone item was left
alone), equal stacks came from the lower row, main inventory won over the
hotbar, and in db3c31f opt-in hotbar reserves topped up the selected slot
without changing the selection and stayed put with the option off. The
6d84bed help text overflowed the two-line footer; db3c31f shortened it and
gave the child row its own help. The trace showed no correction or error.

## L-66 completion-based food restock (2026-09-30, game confirmed positive)

Build `6183AC74B1DB58627CCB5565D5A4698A5AB532998C76A3F1E3D545859886DB93`
(commit `c383bb8`, `--restock_trace=y`), dedicated BDS 1.26.51.1 and a local
world. Maintainer checks, all as expected: enchanted golden apple eaten once
and released (hand refilled, source reduced); eating held continuously
(refilled right after the first completion, eating continued); eating
interrupted by release (no consumption, no refill); stone placement from 7
with one source stack (refilled, no regression); BDS re-join agreement and
normal GUI movement. Beyond the checklist the maintainer tried 1 to 0
depletion and repeated runs with varied counts and found no problem.

The trace logged 10 predicted moves. In two held-eating runs a server
inventory update arrived about 20 ms after the move
(`server-update-after-move`); it only closed observation and the maintainer
saw the refilled count stay. Not covered: 16-stack throwables, remainder
exchange, hotbar sources, latency, screens/focus/dimension changes, and the
trace-disabled normal build.

## L-66 food and local-world restock (2026-09-29, blocks positive, food negative)

Build `CAC8F24792648A90CEBC9DA7B4377F1CF49DB78F412FA9979EAC92481E532C6F`
(commit `6745b4b`, `--restock_trace=y`).

- BDS, enchanted golden apple 2 with 53: `complete-timed` marked the use
  complete and a server update showed the consumption, but no release
  (`ItemReleaseTransaction`) was sent while use was held; the next eating
  start replaced the observation, and `use-evidence value=13` (use, completed,
  timed, no release) ended as `use-not-correlated`. `completeUsingItem` also
  ran again 57 ms after the next start and was attributed to the new use. No
  refill; unchanged after re-join.
- Local world, apple 7 with 57: the same pattern (three completion calls within
  40 ms, `server-confirmed value=6`, then replacement by the next start). No
  refill.
- Local world, stone 7 with sources 1 (slot 24) and 64 (slot 33): the local
  world also sent a server update (`server-confirmed` 41 ms and 10 ms after
  the use). First placement moved the 1 from slot 24 (hand 7), the second
  moved the 64 (`move-predicted value=64`, 6 left at source), as the
  first-source rule specifies. The server-side player's callbacks appear as
  `begin-other-player` and were ignored. The maintainer found the one-item
  refill unnatural (product question, open).
- The 250 ms fallback was not used in any run.

## L-66 server-ordered restock (2026-09-29, blocks positive, food negative)

Build `8E0FED70DF04193FDF1CA638B393281D856B12FA64D8F9B9E66E9ED1AFA13278`
(commit `ff3b5da`, `--restock_trace=y`), dedicated BDS 1.26.51.1.

- Stone 7 with 47 in main inventory, one placement: `settle-wait value=6`, a
  server slot update 20 ms after the use (`server-confirmed value=6`), the
  move on the next tick (`settle-server value=49`, `move-predicted value=53`),
  then `prediction-stable`. The maintainer saw 53 in hand; it stayed. The
  250 ms fallback was not used. Re-join and GUI checks were not reported.
- Enchanted golden apple 2 with 54: holding use re-sent a Use transaction for
  the same slot 4 ticks into eating (`send-not-correlated`), cancelling the
  observation; completion found nothing pending. No refill, inventory unchanged.

## L-66 production restock diagnosis (2026-09-29, game confirmed negative)

Build `96894BA7E8D9515D61FF11FA3EF5ABB0B033AC6973A4881CFEE5469C5B3F919A`
(commit `a41f0f2`, `--restock_trace=y`), dedicated BDS 1.26.51.1. Setup:
stone 7 in the selected slot with 48 in main inventory; enchanted golden apple
2 with 55 in main inventory.

- Stone, one placement: the Place send and the secondary Use send correlated
  in the same tick, `plan-source value=33`, `move-predicted value=54` 33 ms
  after the use; 21 ms later a server inventory update arrived
  (`server-update-after-move`). The maintainer saw about 50 briefly, then 6;
  the main-inventory stack was unchanged. No duplication or loss.
- Apple, one eat: `begin-use`, `start-timed`, `finish-success value=0`,
  `finish-invalid`. The starting `GameMode::useItem` returned false, so the
  observation was dropped; `complete-timed value=-1` found nothing pending. The
  hand went 2 to 1 as in vanilla; nothing moved.

Interpretation: the move reached the server before the placement it followed
(spike B's 250 ms wait had been dropped), and timed uses were filtered by the
callback result. Both are addressed in the next commit, which is not yet
runtime verified.

## L-66 spike B threshold partial refill (2026-09-29, game confirmed positive)

Build `4694D6E4A1E1B4569F26D0E05484B842D1067B306756B92101F6A00929F8E48B`
(commit `619f871`, `--restock_trace=y --partial_restock_trace=y`). Fixed
trigger: a tracked use (here block placement) that leaves the same item at or
below `threshold = 8`; refill amount `min(sourceCount, maxStackSize -
leftCount)`. Environment: dedicated BDS 1.26.51.1. One source, one restock, no
retry.

Trace evidence, case 1 (selected 7 stone, main slot 32): after placing one,
`partial-ready value=32`, `spike-armed value=17`, one
`complex-transaction-send-type value=0` (the setter-driven send),
`spike-predict value=1`, `spike-moved value=38`. Case 2 (main slot 64):
`partial-ready value=58`, `spike-moved value=64`. No correction or slot update
followed either refill; a later placement from the refilled stack in case 2
applied (`legacy-content-applied-held-count value=63`).

The first attempt with the previous build (`3c1160f`) did not refill at the
threshold: block placement emits two ItemUse transactions per action (the
`useItemOn` one and a secondary `useItem` one), and the observation hook
cancelled the pending operation on the second, matching send, so only the
placement that emptied the stack reached the whole-stack path. The fix
(partial-flag only; normal builds unchanged) ignores a second matching use
send for the same hand and slot.

Maintainer in-game check per case: expected immediate state (38 with an empty
source; 64 with 6 left in the source), GUI movement of related and unrelated
items, matching state after re-join, no duplication, loss, ghost item,
rollback, correction or inventory lock, and a single restock per use. The
refilled stack was used again by a later placement; a use at the refill moment
itself was not exercised because the runner left the probe window alone.

Not established: items whose max stack differs from the tested 64 stack (the
probe reads the held item's max stack), replacement-item and non-placement
triggers for the partial path, and multi-source or chaining behavior
(explicitly out of scope).

## L-66 trigger spike A (2026-09-29, callbacks observed on BDS)

Observation-only build
`602735EF6240ED81507B17C57CCD8266B65E162F6CB52B598DF9F452C83C16E2` (commit
`4df1fa7`, `--consumption_trace=y --research_trace=y --restock_trace=n`), so
no restock behavior ran during these tests. Environment: dedicated BDS
1.26.51.1 (same setup as the dedicated-server entry above). The maintainer
performed one bounded action per category and reported vanilla-normal
behavior; supplies came from commands. Trace facts per category:

- Block placement (stone 8 to 7, then 1 to 0): `Player::useItem`
  `method=Place consumeArg=true` ran first, followed by `GameMode::useItemOn`
  `success=true`, both showing the item count falling inside the call; a
  later `GameMode::useItem` returned false. Transport was a legacy complex
  use transaction (`sendComplex type=2`, carrying a legacy request group on
  the first placement).
- Throwable (egg 2 to 1, then 1 to 0): `Player::useItem`
  `method=Throw consumeArg=true` then `GameMode::useItem result=true`; the
  selected-slot count read inside both callbacks still showed the old value
  (2 to 2, 1 to 1). Transport `sendComplex type=2`.
- Food (bread 3 to 2, then 1 to consumed): `Player::startUsingItem`
  (`duration=32`) then `stopUsingItem` and `completeUsingItem`; no
  `Player::useItem`; the count read at completion still showed the old value.
  Transport was `sendComplex type=2` on start and `type=4` on stop, followed
  by a full inventory content update after completion.
- Replacement items (mushroom stew, potion, milk bucket, count 1): the same
  start/stop/complete pattern; the selected slot kept count 1 because a
  replacement item remains. The item identity change itself is not
  represented in the trace (counts only) and was established by the test
  setup. Transport matched the food case.
- Durability break (golden pickaxe, four break events):
  `ItemStackBase::hurtAndBreak(delta=1)` returned true at each break; the
  stack count inside the call was not reliably zero (1 to 1 in two events, 1
  to 0 in the others), and no use callback preceded it. Transport differed
  from the other categories: a targeted `legacySlotUpdate container=0 slot=4`
  and an `itemStackResponses count=1` followed, i.e. the modern
  request/response path.

Not established: whether `hurtAndBreak` returning true is the reliable break
signal in every case (two events returned true while the stack count was
still 1 at call time); callback and transport behavior for other members of
each category (buckets, bottles, other tools); and per-category server-side
state after re-join (the maintainer checked overall vanilla behavior, not a
rejoin per category). No restock behavior or transfer was exercised in this
build, and no fix was attempted.

## L-66 dedicated-server validation (2026-09-29, behavior confirmed; one metadata item not met)

Environment: official BDS 1.26.51.1 executable (bundled with a local tool; its
plugins and pre-existing worlds were excluded), same machine, client and
server in separate processes. Server: world `L66`, `allow-cheats=true`,
`default-player-permission-level=operator`, `online-mode=true`. Client: probe
build `0168ECF55B34A7CB0095D230B63109650CB733B225C6C86FA4E4B412ED15F8F8`
(commit `67b7f77`, `--research_trace=y --restock_trace=y`).

Trace evidence, probe 1 (`spike-armed value=17`): one `addAction` pair
(`slot=17 from=16 to=0`, `slot=4 from=0 to=16`), one
`sendInventoryTransaction` (`sendComplex type=0`), `send-state legacyId=-6`,
`populateLegacy id=-6 groups=1` with `group container=29 count=1 slots=[4]`.
Probe 2, run immediately after with the same setup: the same single-send
sequence with `legacyId=-12` and the same one-slot group. No
`legacySlotUpdate` and no immediate `legacyContentUpdate` correction followed
either probe. During the post-probe usability check the maintainer threw an
extra egg; that use arrived while probe 2's observation window was still open
and cancelled it, so probe 2 has no `spike-moved` line even though its
transaction and client state were correct.

Maintainer in-game check on the dedicated server: after each probe the
selected slot held the moved stack (16), it was immediately usable, source,
destination and unrelated slots stayed movable in the inventory screen, the
state matched after leaving and re-joining the server, and no duplication,
loss, ghost item, rollback, correction or inventory lock appeared.

Checklist result: single send, single action pair, non-zero legacy request id,
client immediate state, server/re-join agreement, immediate usability, GUI
operability, two consecutive probes, and absence of rollback, correction,
ghost, duplication, loss and inventory lock all held.

The `LegacySetItemSlots` group contained only the destination slot (container
29, `InventoryContainer`) in both the local and dedicated runs; the emptied
source slot was absent. The transaction actions themselves carried both slots
(17: 16 to 0, 4: 0 to 16) and the server applied them, so the omission is not
evidence of a functional failure, and the expectation that both slots belong
in this group was an assumption rather than a requirement. The field's exact
semantics are not established: the SDK shows the server consuming the request
id and slot list in `ItemStackNetManagerServer::_handleLegacyTransactionRequest`,
no response packet header exposes a legacy request id, and other-client
observation was not tested separately. The SDK-generated packet shape was left
unchanged; no slots are added by hand. No further probe or fix was attempted.

## L-66 predicted-move refinement (2026-09-29, game confirmed positive)

Environment: local single-player / integrated-server world; the inventory
authority model was not directly confirmed. Two bounded follow-ups on the same
branch.

Trace evidence, build
`CED4EB7C536D8F02C7F20041EF58F9DE99FED7C4801F48FA2B2E439F02921147` (commit
`27c75f6`): the manual `InventoryTransactionManager::addAction` calls were
removed; the two `Inventory::$setItem` calls alone record and send the
transaction. One run logged exactly one action pair
(`addAction slot=33 from=15 to=0`, `addAction slot=6 from=0 to=15`), one
`sendInventoryTransaction`, and no second send.

Trace evidence, build
`7F04CC8ED0F4FE69D3CB861FEC330873EF5B867432B70673BE4359F6CFE67B96` (commit
`00a4fa1`): the exported static
`ItemStackNetManagerBase::_tryBeginClientLegacyTransactionRequest(Player*)`
opens a legacy request scope around the setters. The same single-send trace
shows the scope active (`addAction-state legacyId=-6`, `send-state
legacyId=-6`) and `populateLegacy id=-6 slots=1`. That establishes one
`LegacySetSlot` group only; its container enum and inner slot indices were not
recorded, so no same-shape claim against the vanilla drop reference
(`id=-4 slots=1`) is made here.

Maintainer in-game check (one run per build): correct immediate client state,
replenished stack immediately usable, source and destination movable in the
inventory screen, unrelated slots operable, state unchanged after world
re-entry, no duplication, loss or ghost item. No inventory lockup, rollback
or extra correction was observed in the logs. Multiplayer is the next
validation step and was not started.

## L-66 predicted-move probe (2026-09-29, game confirmed positive)

Branch `spike/l66-client-inventory-transaction`, build
`57CB061257FE0E40B4A0F44F1A28A18C94911DC3F6312788C671866BF9F9BDEF` (commit
`f54eadc`). Environment: local single-player / integrated-server world; the
inventory authority model was not directly confirmed. The retired packet-only
path was not retried. One bounded operation after an egg depletion with an
inventory-only reserve: local prediction through `Inventory::$setItem`
(source emptied, selected slot filled), the same change recorded through
`InventoryTransactionManager::addAction`, and the client's own flush
(`Player::updateInventoryTransactions`) attempted only when recording had not
already sent.

Trace evidence: `setItem slot=33 count=0`, `addAction slot=33 from=16 to=0`,
`setItem slot=6 count=16`, `addAction slot=6 from=0 to=16`, then
`sendInventoryTransaction` with both actions (`sendComplex type=0`), followed
by a second identical addAction pair and send. The duplicate send was
redundant (the second transaction's source state no longer matched) and no
correction arrived. `populateLegacy` reports `id=0 slots=0`, while the vanilla
drop reference in the same session sent with an active legacy request
(`id=-4 slots=1`), so this revision did not fill the legacy set-item slots.

Maintainer in-game check after the one attempt: correct immediate client
state, replenished stack immediately usable, source and destination movable in
the inventory screen, unrelated slots operable, state unchanged after world
re-entry, no duplication, loss or ghost item. Other containers and the
offhand remain unverified. The probe is trace-build-only and not integrated
into the feature.

## L-66 vanilla flow observation (2026-09-29, SDK-unavailable confirmed)

Research trace build `D76433901951EA901BAAC16E7CB85CCD4F82A8D027833D8578B58631C07A6394`
(commit `e67be2d`, `--research_trace=y`, `--restock_trace=n`). Setup: local
survival world, inventory screen open, manual whole-stack moves main
inventory <-> hotbar (64 and 32 zombie spawn eggs). Hooks on
`InventoryTransactionManager::addAction`, `Inventory::$setItem` /
`$setItemWithForceBalance`, `LocalPlayer::$sendInventoryTransaction`, plus a
virtual call to `allowInventoryTransactionManager()` and per-frame sampling of
`mLegacyTransactionRequestId`. Only slots, counts, call order and boolean
state were logged.

Observed order for one move (source slot first, then destination):
`Inventory::$setItem(slot, new)` -> nested
`Inventory::$setItemWithForceBalance(slot, new, false)` ->
`InventoryTransactionManager::addAction(source=ContainerInventory,
container=Inventory, slot, from, to, balanced=false)`. Every observed
`addAction` reported `allowInventoryTransactionManager()=false`,
`mLegacyTransactionRequestId=0`, net manager `enabled=true`, and
`LocalPlayer::$sendInventoryTransaction` was never called. The request id
never changed. The move still reached the server.

Observation: the screen-move flow is "apply locally and record the action,
then submit through the item-stack request path". The submission half of that
specific flow is the SDK-unavailable `ItemStackNetManagerClient` /
`ItemStackRequestScope` surface (all `MCNAPI`), and the legacy
`InventoryTransactionManager` transport is not allowed while the screen is
open (`allow=false`). Following that specific flow with SDK-exported APIs
alone was not possible; a different combination later produced a working
no-screen move (see the predicted-move entries above). This is a statement
about the observed path, not proof that no exported path exists.

Follow-up on the middle-click block pick, vanilla's no-screen inventory ->
hand case (builds `3F33FECC9B...`, `05AC3E0F...`, commits `fc9cbbd`,
`ee3220b`). Setup: one matching block stack only in the main inventory, empty
selected hotbar slot. Trace on `LocalPlayer::pickBlock`,
`FillingContainer::$swapSlots`, `PlayerInventory::selectSlot`,
`$sendComplexInventoryTransaction`, `$sendInventoryTransaction` and the reply
paths. Every screen move produced `itemStackResponses count=1` (the modern
request/response path). The pick produced only:
`pickBlock withData=false`, `pickBlock-state legacyId=0 allow=1`, then
`legacyContentUpdate container=0 slots=36` and `selectSlot slot=4`, with no
`setItem`, `addAction`, `swapSlots` or transaction send on the client. The
item moved and persisted in the world. This is consistent with a
server-mediated behavior: the outgoing pick request was not traced directly
(the only SDK-declared client->server pick packet is `BlockPickRequestPacket`),
and the client only selected and applied the server's content update.
`pickBlock` is exported, but its input is the looked-at block, not a chosen
stack. Within the paths investigated here (screen move, packet-only send,
pick block), no exported generic no-screen move was found; that is not proof
that none exists.

## L-66 client-built transaction spike (2026-09-29, game confirmed negative)

Branch `spike/l66-client-inventory-transaction`, three trace builds. The
transaction is buildable from SDK headers only: two `InventoryAction`s on
`ContainerID::Inventory` (source slot to empty, selected slot empty to source
stack), balanced with `forceBalanceTransaction` and handed to
`LocalPlayer::sendInventoryTransaction`. No other mod's source or a signature
scan was used.

- Build `2013352B2652CC6513F21CEF16F4E5E9E79193683922DB933A4337664A29F98A`
  (pre-commit): sent ~16 ms after the egg throw. The server executed the move
  before the queued legacy use, so the use consumed from the moved stack
  (`legacy-content-applied-held-count` 9 from 10) and the live client desynced
  into a stale 1-egg hand that could not be used.
- Build `FCB5B6FAC431740CBC7AE0453A8A6343471C036D0BEA01FAF8421F6A088EE334`
  (commit `a3137ed`): waited for a server slot update showing the depleted
  hand. The local integrated-server world sent no such update for the
  consumption itself, so only `spike-no-server-depletion` was logged and
  nothing was sent.
- Build `C060CD4E0717B5B3B995265EDE77A1825DE9DD4EC55EEC80E9DDCE6A4C18DA94`
  (commit `9c748c3`): waited 250 ms after arming, then sent. `spike-send
  value=3` and `value=2` both ended in `spike-timeout`: the live client never
  reflected the move. The inventory screen then refused all item moves with
  no exception in the log; re-entering the world showed the moved stacks in
  the hotbar. The server had executed both transactions while the client
  stayed unchanged.

Conclusion so far: the packet reaches the server and the move is executed,
but a packet-only client-built NormalTransaction was not usable on the tested
local single-player / integrated-server world. The client never applied the
change and inventory gestures stopped working until world re-entry. The
authority model was not directly confirmed, and the dedicated-server case was
not tested at this point.

## L-66 restock spike (2026-09-29, game confirmed negative)

Trace build from commit `ec231d8`, DLL SHA-256
`2337CEE87ACE9E0FDFEE38126A8C79E3288489AA66091958191B8746438908F5`
(source and instance copies matched). Setup: Hand Restock on, one egg
selected, one compatible stack in the main inventory, no hotbar reserve.
Throwing the egg logged `spike-armed 10`, then `spike-A-return 0` with an
empty request batch (`capture-end-batch count=0`), then `spike-B-return 0`,
`spike-B-refused` and `Hand Restock spike moved nothing`. Both vanilla HUD
verbs refuse synchronously and create no inventory request. A later throw
with a hotbar reserve logged `selected hotbar reserve`, confirming the
feature was on. The client-built scope path was already ruled unbuildable
(no linkable SDK export). Server behavior was not tested.

This file is chronological evidence, not the current product specification.
Older sections intentionally preserve what was true at that checkpoint and may
describe behavior that has since been replaced. For current behavior use
[DESIGN.md](DESIGN.md); for current work use [BACKLOG.md](BACKLOG.md).

## L-02 dedicated settings openers (2026-09-28, source validated)

Hotkeys, Shapes and HUD layout now use a temporary navigation destination when
opened through their dedicated actions. Closing one without navigating no
longer replaces the page reopened by the ordinary Settings action; manually
choosing another sidebar page still makes that page the ordinary destination.
The pure navigation-state regression checks and `LamiumTests` passed. The
release DLL built with SHA-256
`BA925999912F543FBF804A65D143442AA35DDFF08521D4C95BC604F30E242546`; the
behavior still needs an in-game check.

## L-53 More Info HUD lines, wave 1 (2026-09-28, partially game confirmed)

The Info HUD gained default-off rows for local time, Overworld/Nether 1:8
coordinates, separate yaw and pitch, horizontal and signed vertical speed,
sprinting (visible only while sprinting), and world difficulty. The existing
biome row was intended to use the game's localized biome name; its new
default-off ID switch appended the registry id beside that name. Settings
persistence, line ordering, formatting, coordinate conversion and speed
splitting have pure test coverage. `LamiumTests` and `LamiumNativeTests`
passed, and the first release DLL built with SHA-256
`955C2E73C39B72C9FCE98EAAB280AA82FC4A3111FE88B50366A474C740F40D2B`.
The maintainer confirmed the scaled-coordinate behavior in the Overworld,
Nether and End, the movement-dependent values and difficulty. Biome ID also
looked correct. The screenshot showed `minecraft:beach` instead of a localized
name because those game translations are loaded only by the Editor pack in
normal gameplay. The follow-up embeds the current vanilla English and Japanese
names, changes the Japanese angle labels to `視点角度`, `水平角` and `上下角`,
and adds sibling display-format rows for Biome and Real time. Those refinements
passed `LamiumTests`; the follow-up release DLL has SHA-256
`FF777241EFB6EC4EE6C06695CA2203F3B6D1A633CCD4B8993AF4C28C9399AF39` and still
needs an in-game check.

## Review fixes for transfer and Debug View (2026-09-27, game confirmed)

Commits c2a09e4, 0eebe86 and fb02d69: a cancelled inventory transfer releases
the shared request barrier at once (closing a chest mid-response no longer
blocks Hand Restock or the next Sort), a cancelled drag press also cancels
its release, the Debug View draws nothing without a player instead of sample
values, and the GPU and display lines follow the monitor showing the game.

Verified in game 2026-09-27 on fb02d69, DLL SHA-256
`5AC1443CB424145C04868527A06380B459276708C72322AB368F3B2A251D9E6B` (source and
instance copies matched): Hand Restock right after closing a chest mid-wheel
transfer, releasing Shift before the mouse during a Shift+left drag, no
sample values during world load, and the GPU/display lines including a
resolution change or monitor move. Multiplayer latency remains unverified.

## L-54 Debug View and L-55 armor display (2026-09-27, game confirmed)

The Debug View is a fixed panel instead of the old profile that force-enabled
every Info HUD line and the Target card (L-54). The left column hangs from the
top-left screen inset and the right column from the top-right; it is not a HUD
element, and the layout editor neither shows nor edits it. The row-name-style
option switches between game-standard names and Java-F3 abbreviations. Client
settings come from the SDK; the PC lines (memory, CPU, GPU, display, OS) from
local Windows reads, and a line disappears when the machine cannot provide it.
Child options hide the Info HUD and Target while it is open (both on by
default) and turn its text shadow off. The Target card's armor row gained
`targetArmor` (icons default, bar, number) with the vanilla armor sprites and
the bar color sampled from the armor icon (L-55).

Verified in game 2026-09-27 on DLL SHA-256
`944E886D2733043505F9DA1B87DBFD675895A7AAB99B40F1517AB139A5603503` (source and
instance copies matched): the fixed panel and both label styles, right-edge
alignment in Japanese and Java-F3 labels, the client and PC lines, the hide
and shadow switches, armor icons/bar/number and no row at zero armor. Lamium
now draws its own text shadow half a GUI unit away; the maintainer confirmed
the closer shadow. Not covered: the L-57 counter lines (entity/chunk/particle
counts) are not implemented, and armor toughness is not exposed by the client.
The Info HUD default lines were also aligned with DESIGN (L-56, no separate
in-game check).

## L-52 Java-style default keys (2026-09-28, game confirmed)

The maintainer chose F3 for Debug View, F3+B for Hitboxes and F3+G for Chunk
Borders, with NightVision unbound by default. Existing saved custom chords and
explicit unbinds continue to override defaults. LamiumTests passed, including
default-chord dispatch checks for F3 release and both longer chords. The
initial release DLL built with SHA-256
`A068B8E49FA26E9DD4C5EF0DFDA10D86152ACB544B74698150998DFF373AA556`.
The maintainer later confirmed that F3, F3+B and F3+G work without conflict on
the installed normal build from commit `51a2ad0`, DLL SHA-256
`CABB272FD84BA955356016CEEE9CF6370D154AFCFC8115C467628BE1263093FE`.

## L-52 settings/keymap review (2026-09-27, game confirmed)

Layout B was selected by the maintainer. The sorting parent now has a keyless
switch with the existing Sort (`R`) command as its first child. The Breaking
Restriction key moved from the Block Restrictions group heading to its own
switch row. Other bindings, saved action IDs and settings values are unchanged.
LamiumTests and the release DLL build passed. The DLL SHA-256 is
`6B44039B51F71893A904DA0CAE71ADB9C05CC489FEE4BE524F1D497743DC3163`.
Commit `2e3dbab` was deployed to the LeviLauncher 1.26.51.01 instance with
Minecraft closed. Source and installed SHA-256 hashes matched for Lamium.dll,
Lamium.pdb and manifest.json.
The maintainer reported that the new settings layout behaves correctly in game.
Individual binding-editing and existing-custom-binding cases were not
separately confirmed.

## L-41 inventory transfer (2026-09-27, initial and revised builds)

The first source build used `ContainerScreenController::_handleAutoPlace` for
the four initial gestures and serialized requests through the existing response
tracker. The release DLL built with
SHA-256 `D4E1463F67DD68882887706287DBC3E8578E3BDC1A88E18AE8042418F4DD5269`;
LamiumTests and LamiumNativeTests passed. In-game checks must cover all four
gestures, both directions, partial
stacks, full destination, rapid drag, repeated wheels, cursor item, closing a
screen mid-transfer, text focus, and another player changing a source slot.
The build from commit `d74d2aa` was deployed to the LeviLauncher 1.26.51.01
instance on 2026-09-27 with Minecraft closed. Source and installed hashes
matched for Lamium.dll, Lamium.pdb and manifest.json. The maintainer reported
that the first build was broadly satisfactory in game, then clarified the
wheel semantics, corrected Ctrl+right drag to vanilla behavior, and requested
an unbound toggle action. Individual initial gestures were not confirmed.
The revised behavior has not yet been tested in game.

The revised release DLL built with SHA-256
`21B9AD2D517831DE4EF4AE95C9FA5D0C5F834528DD9C5B8495DC65C0E358560A`.
LamiumTests passed. The SDK declares `handlePlaceAmount` for the explicit
hovered destination, but its revised in-game behavior remains unverified.
Commit `14fd12c` was deployed to the LeviLauncher 1.26.51.01 instance on
2026-09-27 after confirming Minecraft was closed. Source and installed
SHA-256 hashes matched for Lamium.dll, Lamium.pdb and manifest.json.
The maintainer subsequently reported that the revised build behaves correctly
in game. This is an overall confirmation; the individual edge cases listed
above and multiplayer behavior have not been separately confirmed.

## L-41 gesture switches (2026-09-27, game confirmed)

The master Inventory Transfer switch now defaults on when absent from saved
settings. Four independent gesture switches default on; disabled gestures
leave mouse input to Minecraft. Previously saved master switch values are
preserved. The release DLL built with SHA-256
`DA76A37365F7C3477B57D05E8D4CF0522234891B5643B9F82E273B0BC1B53EEE`;
LamiumTests passed. This revision has not yet been checked in Minecraft.
Commit `f038d76` was deployed to the LeviLauncher 1.26.51.01 instance on
2026-09-27 with Minecraft closed. Source and installed SHA-256 hashes matched
for Lamium.dll, Lamium.pdb and manifest.json.
The maintainer then confirmed that this build works correctly in game and
considered L-41 complete. This is an overall confirmation; no separate result
was reported for each edge case or for multiplayer.

## Fake Offhand review fixes (2026-09-27)

The maintainer tested commit 6a94106 (DLL SHA-256
14A1321ECF93E4E5D08D3829F9CA6BEB801CB2A2B4330A308AC34E61A94B096B), which
includes the f483fff eye-offset interpolation, and reported no problems.
No individual checklist results were reported; multiplayer slot sync
remains unverified.

## Eye marker correction follow-up (2026-09-27)

The maintainer tested commit d31e2ef (DLL SHA-256
594E6A0303354EB7567736DF44765EF7432D01B53C1989DA61D7F44848A5A589)
in game. The red marker moved with the white bounds but stayed at a fixed
relative position instead of following the mob's eye. The next revision
restores the live eye offset and interpolates its tick samples; it is not yet
verified in game.

## Moving Hitboxes follow-up (2026-09-27)

The maintainer tested commit 3eca83e (DLL SHA-256
8AA53813E0B7DC73066D1717DF6E8DCFE96CE02CF4433AA24FB5C4E9D5F31C1E)
in game. The white bounds move smoothly, but the red eye marker still appears
to stay at an earlier position, as the bounds had before the fix. The next
change derives the eye position from the interpolated actor position and its
eye offset. That change has not been checked in game.

## Settings search and moving hitboxes (2026-09-27)

The maintainer confirmed that searching for `zoom` and clicking Zoom reveals
its child settings in the deployed 96d5feb build (DLL SHA-256
125A3B0E670BD3136EECD01706453E3187C6FBAC66225256367C31AD135BA6EF).
The `magnification` collapse case was not reported separately.

The maintainer also reported that outlines lag and jitter behind moving mobs.
This is a report, not a verified fix; L-51 tracks the suspected difference
between simulation AABBs and interpolated model positions.

## Fake Offhand initial check (2026-09-27)

The maintainer reported that the initial build (commit f82f21a, DLL E007DD9D)
appeared to work correctly. A concern about held right-click block stacking
was reproduced in unmodded Bedrock and withdrawn. The attempted fix in
15cba27 kept the target slot selected for the entire hold; the maintainer
reported that this prevented use of a sword held in the main hand. That change
was reverted. The original per-action slot restoration remains; multiplayer
selection timing and the full interaction matrix have not been verified.

## Camera activation options (2026-09-27)

The maintainer confirmed the L-48 Freelook starting-view and FreeCamera
Activation follow-up in game and considered it complete (commit a7ce3a6,
DLL SHA-256 D262D42E0D113BD1B84249C25CBB9D7071D9BF60B1F2E11ECBE37A9F0D832668).
No individual edge-case results were reported.

## Current verified status (2026-09-25)

Main is beyond the original settings/HUD prototype. In-game checks through the
HUD/Target integration verified translucent single-pass HUD drawing, settings
ownership, the live HUD layout editor, element toolbar/snapping/reset/popovers,
Target icons and vanilla hearts, target-card morphing and global Animations,
camera-following Target picks in Freelook/FreeCamera, the unified Range setting,
and Bedrock-style sliders including stepping while numeric entry is active.

Freelook and experimental FreeCamera have local runtime evidence. FreeCamera was
checked for first-person flight, movement freeze, blocked attack/use, overlay
movement with the camera, perspective locking and cleanup on settings, death,
focus loss and world re-entry. Multiplayer, controllers and some dimension/menu
edges remain incomplete.

Shapes have local-world persistence evidence and the current renderer/editor is
integrated. Chunk Borders and ordinary entity Hitboxes have moved beyond the
compile-only prototype; Java-style border colors were compared against a Java
reference and the hitbox overlay now includes mob eye/look markers. Broad
graphics-mode/resource-pack/performance coverage is still incomplete.

The current main build/test workflow is exercised by hosted GitHub Actions.
Hand Restock remains experimental and has **not** successfully replenished an
item. Hide Offhand still needs a shield-specific render-path investigation.
Continuous Tool Switch (L-31) through `continueDestroyBlock` passed its
in-game dirt/wood/stone hold check on 2026-09-25 (DLL c81c6cb1). Breaking/placement handoff, dragon multipart
hitboxes and mob growth/breeding timers remain research. Hotkey overlap semantics L-32, release-triggered leading keys and the
conflict display are verified in game; L-34 Auto Attack/Auto Use (switch plus mode,
Fast click held-only switch, keyed option rows) passed its in-game checklists
on 2026-09-25 (DLL 66d534be). Multi-click-per-update landing on servers is
not separately verified.

## Historical checkpoint — Info HUD prototype

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

## Historical checkpoint — Tool Switch prototype

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

## Historical checkpoint — Offhand visibility prototype

Hide Offhand Item is an opt-in setting with an initially unbound Toggle action
in Features/Hotkeys. Its hook skips `ItemInHandRenderer::renderOffhandItem` only
when the SDK FirstPersonPass flag is present and WorldPass/UIPass are absent.
It writes no equipment, item stacks, use state, or network messages. Existing
settings keep the offhand visible. Catalog/persistence/localization tests pass.
Runtime behavior is unverified: check shields while blocking, totems, maps,
main-hand rendering, third-person/paper-doll views, toggling, and world changes.
Special item render paths may need additional coverage after observation.

## Historical checkpoint — Overlay geometry foundation

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

## Historical checkpoint — Settings foundation in progress

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
Hotkeys marks effective bindings (defaults included) as Shared (same chord)
or Overlap (one chord's inputs include the other's); the footer names the
other actions. Native Minecraft and other-mod conflicts are not detected.
L-32 overlap semantics (order-sensitive ordinary chords, most specific chord
wins, latching, modifier-like Zoom/Freelook, shared chords firing together,
legacy sorted-order migration) are covered by event-sequence tests in
BindingTests and SettingsStoreTests and passed the in-game checklist on
2026-09-25 (DLL 188a1c3a). Release-triggered leading keys (F3 alone fires on
release, silent after F3+B) passed in game on 2026-09-25 (DLL 3e7045f8).
Warning key caps and the key-cell conflict tooltip (hover and keyboard
selection) are covered by BindingTests and passed in game on 2026-09-25
(DLL 8cbf8977).
Pure capture tests cover arbitrary chords, opener suppression, mouse buttons,
and modified wheel input. Layout, hit targets, input routing, focus loss, and
the full capture/save/dispatch cycle still require Minecraft verification.

Custom input resets on screen/assignment changes, world exit, and app focus loss.
Held inputs are blocked until release after invalidation, preventing key repeats
from reactivating an action. Consumed Zoom wheel events preserve the custom hold,
and key-up is observed even for cancelled events. Pure regression tests cover
these state transitions. Native text focus and the settings scene suppress
custom actions; Sort retains the container/text-input checks. Since L-23
every action goes through this path. Running actions inside the key event
crashed Sort (Minecraft's assertion writes 0xDEADC0DE while reading the first
inventory slot), because window-procedure input arrives outside the client
tick. Actions are now queued and executed through ClientThreadExecutor; on
2026-09-23 Sort worked in the inventory, a chest and an ender chest, and
Settings (L), Zoom (C), NightVision (J) and chat suppression still worked. Build and unit
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

## Release readiness

Hosted Windows CI is now exercised on main and runs the build, pure tests,
native SDK-type tests and package validation. Earlier clean-checkout and
isolated dependency-restore evidence below remains useful historical evidence;
hosted CI itself is no longer an open gate.

The 0.1.1 pre-release package (DLL db4af5cd) was installed over the existing
instance's Lamium folder, keeping its config. A brief in-game check on
2026-09-26 (settings screen, preserved settings, everyday features) found no
problems. It was not the full regression or a fresh install listed below.

Batched in-game check on 2026-09-26 (research-trace build, DLL 766d6fd6, commit
1a0d568): container previews no longer play the pickup animation and animated
items still animate (L-35); Zoom reaches 50x smoothly in about 20 notches, stays
controllable at 50x, returns to 1x, and the settings row goes to 50 with the
wheel-step row gone (L-38); Permanent Sprint starts, shows its status line,
stops on its key and was cancelled by opening the inventory, which has since
been changed (L-43, re-check pending). An earlier build of the same batch
crashed on world load because of a research-only culler hook; it was removed
before this check.

Second check the same day (DLL 1b6ba9eb, commit 7c4e5fa): Permanent Sprint now
resumes after closing the inventory (L-43). Research results are recorded in
BACKLOG L-36, L-37, L-40 and L-44.

Later checks the same day (research-trace builds up to DLL 1db48ae3): Breaking
Restriction resumes on allowed blocks after a forbidden one and stops cracking
the allowed block while on a forbidden one (L-36); Zoom magnification readout
as a HUD element and the raised toggle toast default (L-45); Reset all,
category and key resets (L-46); Zoom/Freelook/FreeCamera as switch-driven
sessions, FreeCamera keeping its position through inventory and settings, and
Freelook Hold/Toggle (L-47, L-27); Edge Guard holding at block edges in a local
world while stairs and slabs stay walkable and jumping still leaves the edge
(L-40). Multiplayer servers were not tested.

Current pre-release priorities are:

- design and build the L-15 breaking/placement restriction redesign (L-32,
  L-34, L-31 and the L-16 light overlay are done and verified; L-20 was
  closed as not reproducible);
- keep experimental/research features honest: Hand Restock is not working,
  Hide Offhand has a shield path gap, and L-30/L-33/L-16 remain bounded
  research/design work rather than completed features;
- run one full release-build regression across settings/hotkeys, previews,
  sorting, camera tools, HUD/Target, Shapes and world overlays, including
  world/focus/dimension transitions;
- verify the packaged mod from a fresh install and finish the remaining
  dependency/distribution review before publishing a stable release.

Multiplayer, controllers/touch, alternate UI resource packs and several
feature-specific edge cases do not have comprehensive coverage. Their status
must be described accurately; compilation or CI is not runtime evidence.

### Earlier build and distribution evidence

A separate clone with no project build output also completed configuration,
DLL compilation/packaging, test compilation/execution, and the package check.
Its dependency lock remained unchanged. This reused the machine's downloaded
dependency cache, so it is evidence for a clean checkout build, not a clean
dependency restore.

A subsequent isolated dependency-cache restore exposed an upstream runtime
build failure: the defaulted `MinecraftCommands` destructor uses an incomplete
`CommandRegistry` with MSVC 14.44 headers. Lamium now provides a client SDK
recipe using checksum-pinned official source headers and release exports. In
the isolated environment, this recipe installed successfully and Lamium's DLL
and test suite built and passed. Dependencies were downloaded during this
validation, including upstream precompiled packages where available. The normal
development build also passes all tests and the package/notice check with the
new SDK. An untouched clone of `a6e58dc`, with separate initially empty xmake
configuration, package install, download-cache and temporary directories, also
completed dependency restore, DLL build, all tests and package checks. Its
working tree remained clean and its dependency lock hash matched the source
checkout. This used the existing system compiler/Windows SDK; it was not a fresh
operating-system installation.

The development DLL built against the new SDK loaded through LeviLauncher with
Client 26.51.3. A local world displayed the then-current settings UI, showed
`1561 / 1561` for a full-durability diamond pickaxe, and logged an already-sorted
27-slot inventory with 12 occupied slots and one locked slot. No inventory
transfer was issued in this smoke test. This is historical evidence; later UI
behavior is covered by the dated entries below.

`dumpbin /dependents` confirms a normal import of `LeviLamina.dll` and a delayed
import of `bedrock_runtime.dll`. Xmake's earlier LGPL warning came from
classifying the DLL-less SDK import library as static. The package fetch metadata
now reports shared linkage, and configure/build/package validation succeeds
without that warning or disabling license checks. The dependency lock remains
unchanged. A deliberate extra `LeviLamina.dll` in the package is rejected by
the package checker; removing the probe restores a passing result. Additional
DLLs and linker inputs are not allowed in the package. The distribution review
remains open; these are technical linkage and packaging checks, not a legal
conclusion.

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

### Light overlay redesign (L-16, 2026-09-26)

Build `da601ba`, DLL SHA-256
`9e7fd5942655dde0598df25eb39cec3dddf06d9633c830c3089b77edf5db5bb2`, passed the
maintainer's checklists: filled digits turned toward the view (including
Freelook and FreeCamera), red/yellow spawn tints matching real night
spawning, torch refresh within a second, number modes, fixed directions,
range up to 64 while walking and flying without stutter, dimension changes,
and steady (non-flickering) rendering in Fancy, Simple and Vibrant Visuals.
Frame cost was judged by feel only. This supersedes the open items of the
2026-09-23 smoke below.

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

### Inventory request ownership regression (2026-09-23)

Normal build `97800a2` was installed with matching source/destination DLL SHA-256:
`88A2CD77F06139E768FA96C51BD06B6E3037BC7FF12CE7D3F377FBA99640A609`.
In the local creative test world with Deesse UI, R sorted the existing main
inventory. The log records 27 slots, 13 occupied kinds and one locked slot,
followed by 11 acknowledged operations completed at 10:06:24.482. The visible
hotbar and locked helmet stayed in place.

A stack of 64 sticks was manually split into two stacks of 32, with the second
placed in an empty main-inventory slot. R consolidated them back to 64 and
cleared that second slot. The log records a separate one-operation sort
acknowledged at 10:06:55.788. This exercises acquiring and releasing the new
response ownership token across multiple operations and across separate jobs.

Minecraft exited normally, its window disappeared, and the log reached Lamium
disabling at 10:07:26.872. The updated startup message directs users to Lamium
Settings / Features / Hotkeys. No feature settings were changed during this
check. The installed DLL includes the light-digit batching change, but its
rendering/performance was not rechecked in this session.

This is local inventory regression evidence, not validation of multiplayer
rejection/delay, concurrent feature execution, or Hand Restock. Restock's native
connection is still pending.

### Read-only HUD inventory mapping (2026-09-23)

An opt-in `restock_trace` build installed with matching DLL SHA-256
`5A6441D99B5DEDCDA2BAB74620674795220664134E20293CEFA909346307326F`
observed vanilla HUD controller creation in a local creative world. At
10:13:27.672 the controller reported one collection, `hotbar_items`, size 36,
and `closed=false`. Each occupied slot 0–21 uniquely matched the corresponding
player-inventory index. The remaining slots were empty, so their mapping was
not established by item comparison. No transfer or gameplay setting change was
performed. Minecraft subsequently closed normally and its window disappeared.

This establishes an available HUD-owned inventory view for a future Restock
adapter, not successful gameplay transfers or consumption detection. Survival,
multiplayer, delayed initialization and recreated HUDs remain to be checked.
The session log reached Lamium disabling at 10:14:13.170. Afterward,
`restock_trace` was disabled, the normal build/package check passed, and the
instance DLL was restored with matching source/destination SHA-256
`CB33F555198F70446B03A8353A7DF9E64A9782514F882989A55139518C4085B2`.

### Hand Restock first depletion check: not working yet (2026-09-23)

The normal native adapter build at `0d083a3` was tested with installed DLL
SHA-256 `E79ED64E1878CF801B919B9CE105A1555C96024EB52439FC9A42BD4206EF0730`.
In a local world with Deesse UI, one egg was placed in the selected hotbar slot
and 15 matching eggs remained in a main-inventory slot. Hand Restock was enabled
through F8, then the player was changed from creative to survival. A single
right-click consumed the held egg. The selected slot remained empty after
waiting, and reopening inventory showed all 15 reserve eggs still in their
original slot. No Hand Restock acknowledgement, response failure or inventory
error appeared in the session log. This is a failed replenishment check, not a
successful safety or networking test; the silent early-exit cause is unresolved.

Hand Restock was restored to Off, creative mode was restored, and the temporarily
stored shield was returned to the original hotbar slot with durability 336/336.
The one test egg was consumed normally; the reserve stack remained 15. Minecraft
closed, its window disappeared, and the global log recorded Lamium disabling at
10:35:39.555. No multiplayer, food, firework or block-placement path was tested.

The next diagnostic build adds bounded fixed stage labels to the existing
`restock_trace` option to distinguish hook entry, failed eligibility/HUD checks,
unavailable capture and absent depletion plans. These observations are needed
before attributing this failure to any particular guard or changing request
ownership behavior.

### Hand Restock observation timing diagnosis (2026-09-23)

The bounded trace at `8e8f456` entered `use-item`, acquired capture, and
reported `after-use-count=1` followed by `no-depletion-plan` for a survival
egg use. The HUD subsequently showed an empty selected slot. This locates
the early exit before response handling: synchronous return is too early to
observe this depletion.

A second diagnostic build temporarily wrapped `SurvivalMode::useItem` and
`useItemOn` as well (DLL SHA-256
`0B2FCC69D03A2F9AB95B0F76F35648C09CD9E0BC637BF3FA08803DBB82BCA0A3`).
At 10:54:36.349, the outer survival hook acquired capture, the nested base
hook correctly skipped capture, and the outer return still reported count 1
and no plan. The held egg then disappeared without replenishment. Expanding
the synchronous hook boundary therefore did not solve the observed failure;
these extra hooks were removed from source after this experiment.

The next investigation must observe inventory updates after use returns and
establish the actual acknowledgement path. Neither a successful use return
nor elapsed time alone proves server acceptance. Food, blocks, fireworks,
multiplayer and actual replenishment remain unverified.

The adapter now closes capture at successful use return and defers snapshot /
depletion planning until a subsequent tick observes Accepted for the owned
request token. It continues to cancel Untracked, Rejected and TimedOut results.
This avoids rejecting depletion solely because the callback returned too early;
it does not establish that egg use emits the required request. The diagnostic
build and existing LamiumTests passed after this change. Native validation of
the revised sequencing is pending; the installed experimental DLL from the
previous experiment has not yet been replaced by this build.

### Deferred restock planning: egg use is Untracked (2026-09-23)

The `b6380bb` trace build was installed with matching source/instance SHA-256
`B85EF749A97B303DDBF8ED4521197643B86C14A34C9E537535401251A9E9CC5A`.
The previous world session was saved through Save & Quit before replacing the
DLL. In the same local survival test, a single egg in the selected slot was
used. At 11:02:13.554 the trace recorded `use-item`, `capture-started` and
`use-finished`. At 11:02:13.587 the adapter logged inventory response 3
(`Untracked`); the held slot became empty and no replenishment occurred.

This proves the revised sequence reaches response evaluation, but the current
batch-difference capture does not obtain a use request for this egg path. It
does not prove that vanilla emits no request anywhere, nor that increasing a
timeout would fix it. Investigate the consumption transaction / authoritative
inventory update path before permitting replenishment for this case. The
diagnostic DLL remains installed; no successful restock is claimed.

### Legacy inventory-update diagnostic smoke (2026-09-23)

The `4f75321` diagnostic DLL was installed with matching SHA-256
`B49851B1FFEB37BF676C9524FFB5F185AAA0B64CB79ECF6EF28B2D98B973BE3B`.
In the local survival world, replacing the empty selected slot with a test egg
produced four `legacy-content-applied-held-count=1` observations at
11:08:41.076. This demonstrates that the content hook is active and that its
post-handler inventory read sees the newly supplied stack.

Using that egg at 11:09:24.398 reached capture and use completion, then stopped
as Untracked at 11:09:24.417. The HUD became empty. No legacy slot/content
observation followed this use, including a subsequent log read after the
initial smoke. The diagnostic sample cap was not exhausted. This does not
establish that the slot hook works, nor exclude other synchronization paths;
it rules out relying on the observed legacy-content path alone for this case.
Next investigate item-stack responses without captured IDs and local inventory
mutation/synchronization rather than extending an uncorrelated wait. The trace
build remains installed and replenishment is still not validated.

### Request capture boundaries for egg consumption (2026-09-23)

The `324e411` diagnostic DLL was installed with matching SHA-256
`FFFFB4E5B875ECF2DF809F69EAF0A5735C84CA338746BE4A192908D0BA9B8756`.
At 11:16:41.841, a local survival egg use logged both capture-start and
capture-end-batch with count 0 and active=false. It stopped as Untracked at
11:16:41.857. The selected slot became empty without replenishment.
No responses-applied or legacy inventory-update observation followed the use
in the log, including a later read. Supplying the egg beforehand did produce
four legacy-content observations with held count 1.

This narrows the failure to a use path not captured by the current batch
observer; it does not prove absence of all network traffic or validate the
response hook. Extending its timeout is not supported by this evidence.
The next opt-in diagnostic observes LocalPlayer's complex-transaction send
boundary, recording only transaction type and whether use capture is active.
A send is not acknowledgement and cannot authorize replenishment. Native
validation of that diagnostic is pending; Hand Restock remains experimental.
The diagnostic build and existing LamiumTests passed. These checks cover
compilation and existing inventory-planning invariants, not hook execution
or successful replenishment in Minecraft.

### Egg use takes the complex-transaction send path (2026-09-23)

The `83814f7` diagnostic DLL was installed with matching SHA-256
`AEB1694C5B53341DCDE4A19BB546F670E1922833297F572361AFE05C21EBEE69`.
In the local survival world, using one egg at 11:26:38.774 produced the
following order: use-item, capture-start (0/false), capture-end-batch
(0/false), use-finished, complex-transaction-send-type=2, and
complex-transaction-during-use=0. The SDK defines type 2 as
ItemUseTransaction. The adapter stopped as Untracked at 11:26:38.807 and
the selected slot was empty. A subsequent log read showed no later response
or inventory-update observation.

This positively validates the complex-send diagnostic hook and identifies a
send after the current synchronous capture boundary. It does not establish
server acceptance, and no replenishment occurred. Repeating batch capture or
extending its timeout is not the next implementation step.

The adapter needs separate consumption observation and replenishment-response
tracking. Investigate matching a successful local use to the outbound use
transaction's slot/hand/action and subsequent inventory depletion; cancel on
selection/context changes, unrelated slot changes, or manual inventory actions.
Any resulting replenishment must still use vanilla controller operations and
the owned response barrier. Do not label client prediction or transaction
submission as server acknowledgement. Block, food, firework, rejection and
multiplayer behavior require independent validation. Keep this uncertainty
bounded rather than blocking the remaining feature waves.

### Correlated legacy consumption adapter (2026-09-23, runtime pending)

The adapter now observes complex use sends in normal builds. A locally captured
successful use may take the legacy path only when a main-hand Use/Place
transaction matches its selected slot. After vanilla submits that transaction,
the adapter waits at most one second for the selected stack to disappear, with
all other inventory slots unchanged. This is local consumption observation,
not a claim of server acceptance; the timeout only cancels, never succeeds.
Manual drop, another unmatched transaction, changed selection/context, and
unrelated inventory mutations cancel the operation. Replenishment still uses
the vanilla HUD swap and requires its own captured request response.

The planner no longer falls back to another reserve when a source changed.
Regression cases cover changed reserve, other hotbar mutation, and pickup into
an unrelated slot. The diagnostic native build passed. Native success,
correction/rejection handling, and the full block/food/firework matrix remain
unverified; the installed DLL is still the preceding diagnostic build.

### Consumption planning reached; HUD swap did not replenish (2026-09-23)

Installed `77d4ad2` with matching DLL SHA-256
`A1127758015E8A0363BD886DD3992073CE6EE65CABC8052D3AD4730976CCDD3F`.
Before the test, the selected slot was empty and the main inventory contained
14 reserve eggs. After supplying one test egg and using it, the trace at
11:35:03.758 recorded observed-use-count=0 and plan-ready. A new capture for
the refill opened and closed with an empty batch. No refill acknowledgement
was logged. Opening inventory afterward showed the reserve still at 14 and
the selected slot empty. Consumption correlation now reaches planning, but
the inventory transfer is not working.

The next build uses handlePlaceAmount with the reserve's exact count for the
known-empty destination and records its boolean return. This is a targeted
controller-operation experiment, not a proven fix. It builds successfully;
native validation is pending. The previous implementation did not log the
swap return, so its precise rejection reason remains unknown.

### HUD count transfer returns false (2026-09-23)

Installed the `df664b3` code build with matching DLL SHA-256
`C611E2A56D803E75C7C2DDC71D7BF45871A67A84AB3F1C96BC29C3D0CAD86FF9`.
The local survival egg test reached observed-use-count=0 and plan-ready at
11:40:52.469. The count-transfer call then logged replenishment-submitted=0
and capture-end-batch count=0/active=false. The selected slot remained empty.
Thus changing swap to count transfer did not establish a working HUD transfer
path. Investigate controller permissions/context and simulation mapping before
further transfer attempts; repeated waits or method substitutions are not a
supported fix. This feature remains experimental and must not block other waves.

### Freelook camera detachment (2026-09-23)

User testing of the earlier Freelook showed the view and player both fixed while
terrain in the turned direction was culled. Two replacement attempts were traced
and discarded (see CAMERA.md). The current implementation withholds
`UpdatePlayerFromCameraComponent` from the active camera while held, restores the
saved direct-look/orbit angles on release, and keeps the head yaw captured at
activation.

In the user's local world on Minecraft 1.26.51.01 / LeviLamina Client 26.51.3,
first and third person: the camera turned freely and returned on release; body,
head pitch and head yaw stayed fixed without visible jitter or a snap on release.
During elytra flight the original flight direction was retained while looking
around. Normal build installed afterwards, DLL SHA-256:
`4463C1F9B684FD1D870DB4442E1777CE761117C185C375E606AA139332DE253A`.
Multiplayer, riding, dimension changes during a hold and controllers remain
unverified.

### FreeCamera experiment (2026-09-24)

Verified by the maintainer on 31323b8 (camera trace build): first-person
flight with WASD/Space/Shift, the player frozen in survival and creative
(no movement, attack or use), terrain, shapes and chunk borders following the
camera, F5 ignored with the body shown and the perspective restored, and
clean exits on toggle, settings, death, Alt-Tab and world re-entry. Not
verified: dimension change while flying, controllers, multiplayer. The
trace-free cleanup build (cf8d5e9) passed the same checks the same day. Left
clicks still swing the arm while detached (no attack or break); tracked in
L-25.

### HUD editor, target card and sliders (2026-09-24/25)

Verified by the maintainer in game on builds up to 1c6ff4e: HUD fills are
translucent once the HUD draws only on the hud_screen view (it drew on four
views per frame before); single text shadow; HUD hidden under the settings
list; layout editor with element toolbar, snapping, reset and popovers that
avoid the toolbar; settings dialog no longer blocks hotkeys after a
dimension change; target card with icons (villagers included), vanilla
heart sprites, 0.1 s morph and the Animations setting; camera-following
picks during Freelook/FreeCamera without liquids; Range slider (default 6);
Bedrock-style sliders with working -/+ while typing. Not verified:
multiplayer servers, controllers.

### Target icon resolution (2026-09-28)

Verified by the maintainer in a local survival world and in the End on
Minecraft 1.26.51.01 / LeviLamina Client 26.51.5 / Deesse UI 1.3.9 across
the builds of 971cfa3, 9a4051a, a941826 and 786effc. Confirmed: ordinary block
items, the wheat pick item, villager and zombie villager spawn eggs, snowball,
arrow, ender pearl, painting and dropped stacks, the thrown trident, Bedrock's
renamed ids (end crystal, eye of ender, experience bottle), the nether portal
and the end portal as a single texture frame, and the end crystal item icon.
The first two builds still drew the portal wrong - a file-system prefixed
texture path, then atlas uv coordinates applied to the source file - and both
were fixed and re-checked. The falling block first showed the wrong icon;
see the review follow-up below. Experience orbs, players, lightning and every other target
with neither a spawn egg nor an item show no icon, by design. Not verified:
multiplayer, other resource packs than vanilla.

### Left-click placement overlap and the L-49 trace crash (2026-09-28)

The research-trace build at 60b1d2f (DLL
F0B154379D5BBF30DD42540017AF1F3D090D47E8D7FD4665AD3F213A75257CCC) crashed six
times on 2026-09-28, every time inside the L-49 diagnostics:
`ClientInstance::getInProgressBAI()` answers with a null reference when no
build action is in progress and the trace read `mAction` unconditionally. The
hooks are installed at load time, so the crash did not depend on the feature
switch. Commit 9e23946 guards the read; the maintainer then saw no crash with
Fake Offhand on, with it off, and when clicking the settings search field
(DLL 59CCB0EDDF17D3EEA6D17DB3F0DE8E8F10737276A3A02A6FA71C493E3BC12391).

With the trace running, the reported overlap reproduced: one left click
inserted while holding right stops placement, keeping right held does not
resume it, and releasing and pressing right again does - with Fake Offhand
both off and on. Unmodded Bedrock behaves identically, so L-49 closed as
vanilla parity without a Lamium change and the diagnostics stay in place.
Keeping placement alive across the left click is now L-59 (Design). The
closing normal build of 2026-09-28 is commit 786effc, DLL SHA-256
2BC647BE238E521C97D3A876E8CC5C70E4C18D3FC713A0140A65B19EF543F589, with
`research_trace` and `automation_trace` disabled again.

### Moving hitbox eye marker (2026-09-28)

The maintainer checked the red eye marker revision (f483fff) in game on the
closing normal build (commit 786effc, DLL
2BC647BE238E521C97D3A876E8CC5C70E4C18D3FC713A0140A65B19EF543F589) and saw no
visual problem while mobs walk and turn. L-51 is closed. The marker still
interpolates simulated body samples with a sampled eye offset rather than
reading the position the model renders from; the maintainer left that as an
open question about a more fundamental source, not as a defect.

### Target icon review follow-up (2026-09-28)

The research-trace build at 6a30903 (DLL
A79DD9A78A89A0175E909FEAED9D8A3319D184984059EF7BE07897943B0E9BEA) logged,
for every falling block, a variant naming the carried block (gravel, sand,
anvil, white concrete powder) and legacy id/data 0:0, which names
`minecraft:info_update` - the source of the earlier wrong icon. The maintainer
saw the carried block's icon for sand, gravel, concrete powder and the anvil;
spawn eggs and the nether portal unchanged; and spawn-egg icons still present
after leaving and entering another world (the egg index is now rebuilt per
world). Commit 51a2ad0 then dropped the unused legacy fallback; its normal
build is DLL CABB272FD84BA955356016CEEE9CF6370D154AFCFC8115C467628BE1263093FE,
not re-checked in game separately since the resolution path it keeps is the
one verified.

### Managed update through LeviLauncher / LIP (2026-09-28, L-65 step 1)

In a new LeviLauncher instance (1.26.51.01 + LeviLamina Client, separate from
the development instance, where Lamium is deployed by copy and is not in the
LIP lock file), the maintainer installed Lamium 0.1.2 from Bedrinth, bound
FreeCamera to X and enabled the Info HUD, closed Minecraft, updated to 0.1.3
in LeviLauncher and started again. Both settings were kept, and the Fake
Offhand row that 0.1.2 does not have appeared in Settings, so 0.1.3 was
running. Afterwards the instance's `tooth_lock.json` listed
`github.com/amatouhake/Lamium` 0.1.3 (client) with 28 placed files, none
under `config/` or `logs/`; `mods/Lamium/config/settings.json` was present;
the installed DLL SHA-256
351A1B82C2BB6F14B71700B563C7F48604BAFDD72ECB2B4A7EC25572161951ED matched
the one in the v0.1.3 release ZIP. Not checked here: LIP CLI and an
update across a settings schema change.

The maintainer then uninstalled Lamium from the same instance in LeviLauncher.
Every file LIP had placed was removed (DLL, PDB, manifest, notices, license
texts) and the Lamium entry left `tooth_lock.json`. `config/settings.json`
and `logs/` stayed, as did an empty `licenses/` directory: LIP deletes the
files it recorded but not the directories that held them.

# Feature lifecycle: world exit, dimension change, focus loss

AGENTS.md rule 3: every feature restores vanilla when disabled, on world
exit and dimension change, and one that holds input or changes game state
also on focus loss. This table says, for each row of the feature table in
`src/app/Runtime.cpp`, who handles each event. `scripts/Check-Lifecycle.ps1`
(CI) fails when a feature row has no line here or a line names no row.

The check proves that a feature's answer is written down, not that the code
does it; a line changes with the code it describes. Rows whose name ends in
"diagnostics" or "probe" only start and stop with their build option and
are not listed.

Words: **own** (the feature's listener or hook), **CustomInput** (handled
by name in `input/CustomInput.cpp`: its exit listener, `invalidate()` on
focus loss, held keys released through `releaseStates()`), **polled** (the
feature notices the change itself each tick), **n/a** (nothing to restore,
with why), **open** (a finding in L-402, not settled).

| Feature | Exit | Dimension | Focus |
|---|---|---|---|
| Client tasks | n/a: a queue | n/a: a queue | n/a: a queue |
| Camera | own | own | own; keys also CustomInput |
| Lighting | n/a: render only | n/a: render only | n/a: render only |
| Inspection | own | n/a: per frame, screen-bound | n/a: draw-only |
| Locked trades | own | n/a: the trade screen closes | n/a: draw-only |
| English search | own | n/a: filters per call | n/a: filters per call |
| Inventory | own (Sort, Transfer, Hand restock) | polled (Transfer, Hand restock); Sort ends with its screen | own (Hand restock), CustomInput (Transfer); Sort: open |
| Repeated trading | own | polled | CustomInput |
| Enchantment order | own | n/a: the anvil closes | CustomInput |
| Settings screen | own | n/a: UI only | CustomInput |
| Custom input | own | n/a: keys stay physically held | own |
| World overlay | own | polled | n/a: draw-only |
| Offhand visibility | n/a: draw-only | n/a: draw-only | n/a: draw-only |
| FreeCamera hiding | n/a: draw-only | n/a: draw-only | n/a: draw-only |
| Connected Textures | n/a: chunks rebuild | n/a: chunks rebuild | n/a: render only |
| Block highlight | n/a: marks re-checked | n/a: marks kept per dimension | n/a: draw-only |
| Server resource packs | n/a: decided per join | n/a: join only | n/a: join only |
| Tool Switch | own | polled | n/a: per mining call |
| Weapon Switch | n/a: per attack, no state | n/a: per attack | n/a: per attack |
| Death layout | polled (world change) | polled | open: keeps moving items unfocused |
| Fake offhand | CustomInput | own | CustomInput |
| Frame timing | n/a: measures only | n/a: measures only | n/a: measures only |
| Target icons | own | polled | n/a: draw-only |
| Minimap | own | n/a: per frame | CustomInput (held keys) |
| Waypoints | own | n/a: arrival per frame | n/a: holds no input |
| World map store | own | n/a: regions per dimension | n/a: holds no input |
| Waypoint markers | n/a: draw-only | n/a: per frame | CustomInput (held key) |
| Schematic ghosts | own | polled | n/a: draw-only |
| Schematic pick block | n/a: one pick | n/a: one pick | n/a: one pick |
| Schematic adjust key | polled | polled | CustomInput (held key) |
| Schematic plan targets | n/a: no state | n/a: no state | n/a: no state |
| Breaking Restriction | own | own | CustomInput (the attack press it follows) |
| Edge guard | n/a: decided per call | n/a: decided per call | n/a: decided per call |
| Sneak placement | open: the old world's sneak state is kept | n/a: the pulse ends itself | n/a: the pulse ends itself |
| Tool Protection | own | n/a: per call | open: keeps moving items unfocused |
| Mining session | n/a: reset on the next start | n/a: reset on the next start | n/a: reset on the next start |
| Auto Elytra | own | polled | open: keeps moving items unfocused |
| Recipe table | own | n/a: per world | n/a: data only |
| Recipe notes | own | n/a: per world | n/a: data only |
| Item list | own | n/a: closes with its screen | CustomInput |
| Mass Craft | own; the key's release | n/a: ends with the key | CustomInput (held key) |
| Carried bundles | n/a: read per frame | polled | n/a: read only |
| Sneak | CustomInput | own | n/a: a lock the player set (rule 3) |
| Effect visibility | n/a: draw-only | n/a: draw-only | n/a: draw-only |
| Debug View vanilla HUD | n/a: draw-only | n/a: draw-only | n/a: draw-only |
| Chat position | n/a: draw-only | n/a: draw-only | n/a: draw-only |
| Scoreboard position | n/a: draw-only | n/a: draw-only | n/a: draw-only |
| Boss bar position | n/a: draw-only | n/a: draw-only | n/a: draw-only |
| Status effects vanilla icons | own | n/a: draw-only | n/a: draw-only |
| Book paste | n/a: ends with the book screen | n/a: ends with the book screen | n/a: one paste |
| Periodic input | CustomInput | own | CustomInput; open: clicking goes on unfocused |

Periodic input is started in `Runtime::load()`, outside the table.

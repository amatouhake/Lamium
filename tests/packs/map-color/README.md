# map_color test packs

A minimal add-on for checking how Lamium's minimap and world map color
add-on blocks (L-324 Map: custom blocks are drawn in a dirt-like brown).

- `BP/` behavior pack, `RP/` resource pack (the BP depends on the RP).
- `lamiumtest:map_magenta`: full cube, gray-green texture, block component
  `"minecraft:map_color": "#FF00FF"`.
- `lamiumtest:map_none`: full cube, orange texture, no `minecraft:map_color`.

Both are in the creative inventory under Construction.

## Install

Copy `BP` into `games/com.mojang/development_behavior_packs/` and `RP` into
`development_resource_packs/` (any folder names). In world settings, enable
"Lamium map_color test BP" under Behavior Packs (the RP is added with it).

## What to expect

Place a filled area of each block in a part of the world the map has not
recorded yet.

- `map_magenta`: magenta on Lamium's map and on the game's map item, not the
  gray-green of its texture.
- `map_none`: Lamium's map shows the orange-ish average of its texture; the
  map item shows whatever default the game gives it.

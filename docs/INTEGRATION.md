# Integration between features (proposal)

Draft for [BACKLOG L-111](BACKLOG.md), written by an agent on 2026-10-08
for the maintainer to decide on. Nothing here is decided until it moves into
BACKLOG.md or DESIGN.md.

Decided 2026-10-11: the boundary, order and 0.2.0 scope are in L-111 in
BACKLOG.md; A is L-138 and B is L-139 (required), C to E are optional.
Built 2026-10-11: A and B (in-game checks pending, BACKLOG 0.2.0 regression
checks).
Shapes on the minimap are a setting; the menu key question stays open until
C.

## Why

Most features are mature on their own, but the links between them have
weakened. Schematic placements now show on the maps, but the map cannot
switch them on or off or edit them the way it edits waypoints; Shapes do not
appear on the maps at all. The radial menu built for Schematics is used by
Schematics only. Overlays that draw faces each solved z-fighting their own
way until L-110.

## Principle: shared parts, not features calling each other

The module split is worth keeping: each feature starts and stops on its own,
fails open to vanilla on its own, and a bug stays inside it. So integration
should come from **shared parts that no single feature owns**, which features
register into, rather than features reaching into each other's internals.

- A shared part is pure where it can be (a header with tests) and has one
  small glue file.
- A feature that is off registers nothing; disabling it leaves the shared
  part working for the others.
- Adopting a shared part is one feature per commit, and the feature's current
  behavior stays the default until its in-game check passes.

## Candidates

### A. World face drawing (started by L-110)

`overlay/Depth.h` now holds the depth rules every face overlay follows. The
next step is one mesh builder for "faces and outline of a set of cells" that
Shapes, the breaking restriction and the schematic area frame all call, so a
new overlay cannot reintroduce the inset or its own color/alpha handling.
Size: small. Risk: low (Shapes and restriction already share `drawShape`).

### B. Map layers

One list of things drawn on the minimap and world map, filled by the
features that have them:

| Layer | Mark | World-map actions |
|---|---|---|
| Waypoints | diamond, name | already: select, edit, hide, teleport |
| Death point | cross | already |
| Schematic placements | footprint | show/hide, select, move here, open in the Placed tab |
| Shapes | footprint of the shape | show/hide, open in the Shapes view |
| Breaking region (L-15) | while held, nothing on the map | none |

A layer gives marks (position or footprint, label, visible, selected) and an
optional list of actions; the maps draw and hit-test them the same way, and
the world map's side panel gets one section per layer instead of waypoints
only. Waypoint storage and schematic placements keep their own files.
Size: medium (`WorldMap.cpp` is about 1,200 lines). Risk: medium, mostly in
the world map's hit testing and side panel; the minimap side is small.

### C. A Lamium radial menu

`ui/RadialLayout.h` and the schematic `MenuModel.h` already form a tested
radial menu with categories, steppers and commands. Generalize it into a
shared component that features register categories into, opened by one
"Lamium menu" key:

- Camera: Zoom, Freelook, FreeCamera speed
- Shapes: show/hide, new shape at your feet, open the view
- Map: add waypoint here, open the world map, radar faces
- Schematic: its current eight categories, unchanged, as a submenu
- Block restrictions: breaking mode and band (would answer part of L-15's
  "too many modes, no key for placement mode")

The schematic menu key keeps opening the schematic categories directly.
Size: medium to large. Risk: medium (input ownership while open, key
conflicts, controller/touch). Needs a demo in `docs/demos/` before building.

### D. Looked-at selection

Schematics can select the looked-at placement. The same "what am I looking
at" could select a shape or a waypoint marker for the radial menu's actions.
Depends on C. Size: small once C exists.

### E. Settings cross-links

Feature help could link related features (Tool Switch with Tool Protection,
Map with Waypoints and Schematics). Size: small. Risk: low. Useful but not
integration in the sense above.

## Not proposed

- Merging data files (waypoints, placements, shapes, death layout stay
  separate; a bad file affects one feature).
- A general event bus between features.
- Making any feature depend on another being on.

## Suggested order

1. A (finish the face builder; small, follows L-110).
2. B for schematic placements and Shapes on the world map, the most visible
   gap; the minimap keeps drawing only what it draws now plus Shapes.
3. A demo for C, then C with the schematic submenu and one or two other
   features; L-15's rethink can use it for mode choice.
4. D, then E when convenient.

A release can come between any two steps; 0.2.0 could be the point where B
and C are in.

## Questions for the maintainer

- Is "shared parts features register into" the right boundary, or should
  some features merge outright (for example Shapes and Schematics' area
  frame)?
- Map layers: should Shapes appear on the minimap too, or only on the world
  map?
- Radial menu: one key for everything, or per-feature keys that open the
  same component at their own category?
- Which of A-E belong in 0.2.0, and which can wait?

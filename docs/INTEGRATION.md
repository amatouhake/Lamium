# Integration between features (proposal)

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/INTEGRATION.md`.

Proposal for
[L-111 Integration between features: remaining shared controls](backlog/L-111.md),
written by an agent for the maintainer to decide on. Nothing here is decided
until it moves into the L-item or [DESIGN.md](DESIGN.md); the L-item's
Decided and Open lists win over this file.

The required part shipped in 0.2.0: one cell-overlay builder for Shapes, the
breaking region and the schematic save area
([OVERLAYS.md](OVERLAYS.md)), and map layers that put schematic placements
and Shapes on the world map and minimap ([MAP.md](MAP.md)). What remains,
C to E below, is optional.

## Principle: shared parts, not features calling each other

Each feature starts and stops on its own, fails open to vanilla on its own,
and a bug stays inside it. Integration comes from **shared parts that no
single feature owns**, which features register into, rather than features
reaching into each other's internals (kept as decided in L-111).

- A shared part is pure where it can be (a header with tests) and has one
  small glue file.
- A feature that is off registers nothing; disabling it leaves the shared
  part working for the others.
- Adopting a shared part is one feature per commit, and the feature's
  current behavior stays the default until its in-game check passes.

## Candidates

### C. A Lamium radial menu

`ui/RadialLayout.h` and the schematic `MenuModel.h` already form a tested
radial menu with categories, steppers and commands. Generalize it into a
shared component that features register categories into, opened by one
"Lamium menu" key:

- Camera: Zoom, Freelook, FreeCamera speed
- Shapes: show/hide, new shape at your feet, open the view
- Map: add waypoint here, open the world map, radar faces
- Schematic: its current categories, unchanged, as a submenu
- Block restrictions: breaking mode and band (would answer part of
  [L-15 Breaking and placement restrictions](backlog/L-15.md): too many
  modes, no key for placement mode)

The schematic menu key keeps opening the schematic categories directly.
Size: medium to large. Risk: medium (input ownership while open, key
conflicts, controller/touch). Needs an agreed demo in `docs/demos/` before
building.

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
- Merging features outright (L-111 Decided).

## Suggested order

1. A demo for C, then C with the schematic submenu and one or two other
   features; L-15's redesign can use it for mode choice.
2. D, then E when convenient.

A release can come between any two steps.

## Questions for the maintainer

- Radial menu: one key for everything, or per-feature keys that open the
  same component at their own category?
- Which of C-E, if any, to schedule. The rest of the open questions
  (spatial actions usable while working, input contexts, binding
  conflicts, UI weight) are in L-111's Open list.

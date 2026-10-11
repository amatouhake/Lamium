# Documentation guide

Start here to find a document; start with the
[Current execution order](BACKLOG.md#current-execution-order) to pick work.
The [agent guide](../AGENTS.md) is the working manual and the
[project README](../README.md) is the user-facing overview.

## Document roles

| Question | Source |
|---|---|
| What should Lamium look like and how should it behave? | [DESIGN.md](DESIGN.md): accepted product/UI rules; proposed rules are labeled |
| What should be worked on next, and what is still open? | [BACKLOG.md](BACKLOG.md): execution order, L-items, release policy and pending checks |
| What was actually checked in Minecraft? | [VALIDATION.md](VALIDATION.md): current coverage per feature |
| Which build and observation support a result? | [VALIDATION-LOG.md](VALIDATION-LOG.md): append-only evidence, newest first |
| What happened to a finished or closed L-item? | [BACKLOG-DONE.md](BACKLOG-DONE.md): retained task history |
| How is Lamium packaged, updated and released? | [DISTRIBUTION.md](DISTRIBUTION.md): package contract and release checklist |
| What does Lamium rely on in the game, and what to do on a game update? | [GAME-UPDATES.md](GAME-UPDATES.md): gated capabilities, update playbook and the hook inventory (script-checked) |
| How are contributions and translations handled? | [CONTRIBUTING.md](../CONTRIBUTING.md), [TRANSLATING.md](TRANSLATING.md) |
| Which outside sources may be used? | [PROVENANCE.md](PROVENANCE.md): dependencies and source-use boundaries |
| Which mockup was adopted? | [demos/README.md](demos/README.md): demo status and links |

## Feature and implementation notes

L-items own work status and open choices; DESIGN owns shared product/UI rules;
VALIDATION owns runtime coverage. Dated implementation records describe the
build named there and may include superseded experiments.

| Area | Document |
|---|---|
| Settings, HUD and input foundation | [UX-FOLLOWUP.md](UX-FOLLOWUP.md) |
| Parent switches, command rows and keys | [SETTINGS-KEYMAP.md](SETTINGS-KEYMAP.md) |
| Freelook and FreeCamera | [CAMERA.md](CAMERA.md) |
| Shapes, chunk borders, hitboxes and persistence | [OVERLAYS.md](OVERLAYS.md) |
| Light-level overlay | [LIGHT-OVERLAY.md](LIGHT-OVERLAY.md) |
| Visual effect hiding | [VISUAL-EFFECTS.md](VISUAL-EFFECTS.md) |
| Auto Attack/Use and permanent movement | [AUTOMATION.md](AUTOMATION.md) |
| Breaking/placement restrictions and research | [RESTRICTIONS.md](RESTRICTIONS.md) |
| Hand Restock | [HAND-RESTOCK.md](HAND-RESTOCK.md) |
| Tool/Weapon Switch, Tool Protection and Auto Elytra | [EQUIPMENT.md](EQUIPMENT.md) |
| Fake Offhand | [FAKE-OFFHAND.md](FAKE-OFFHAND.md) |
| Minimap, radar, waypoints and world map | [MAP.md](MAP.md) |
| Schematics, checking, materials and area save | [SCHEMATIC.md](SCHEMATIC.md) |
| Integration between features (proposal, L-111) | [INTEGRATION.md](INTEGRATION.md) |

## Keeping the docs consistent

- Put shared decisions in DESIGN and task choices/dependencies in the L-item.
  Link to feature contracts rather than copying them into execution order or
  UI summaries. The L-item wins if its status disagrees with a summary.
- Put current notes ahead of dated records. Label superseded checkpoints so
  an old experiment cannot be mistaken for the next task.
- Record an in-game result at the top of VALIDATION-LOG, then update VALIDATION.
  Build/tests alone do not establish runtime behavior.
- Keep BACKLOG-DONE and VALIDATION-LOG intact. Search for the L-number or
  feature instead of reading either whole.
- Update the demo index when its decision or implementation status changes.
  Mockups show intent; current text and implementation can differ from them.
- Machine-specific paths stay in the gitignored `AGENTS.local.md`.

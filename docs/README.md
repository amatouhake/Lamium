# Documentation guide

Start here to find a document; start with the
[Current execution order](BACKLOG.md#current-execution-order) to pick work.
The [agent guide](../AGENTS.md) is the working manual and the
[project README](../README.md) is the user-facing overview.

## Document roles

| Question | Source |
|---|---|
| What does each feature do, for a player? | [FEATURES.md](FEATURES.md): the user guide behind the README's feature tables |
| What should Lamium look like and how should it behave? | [DESIGN.md](DESIGN.md): shared product/UI rules; one feature's decisions are in its document below |
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

L-items own work status and open choices; DESIGN owns shared product/UI rules and
each feature document its own decisions;
VALIDATION owns runtime coverage. Dated implementation records describe the
build named there and may include superseded experiments.

| Area | Document |
|---|---|
| HUD elements, layout editor, target card, debug view | [HUD.md](HUD.md) |
| Settings, HUD and input foundation | [UX-FOLLOWUP.md](UX-FOLLOWUP.md) |
| Native HUD chat placement | [CHAT-POSITION.md](CHAT-POSITION.md) |
| Parent switches, command rows and keys | [SETTINGS-KEYMAP.md](SETTINGS-KEYMAP.md) |
| Freelook and FreeCamera | [CAMERA.md](CAMERA.md) |
| Shapes, chunk borders, hitboxes and persistence | [OVERLAYS.md](OVERLAYS.md) |
| Light-level overlay | [LIGHT-OVERLAY.md](LIGHT-OVERLAY.md) |
| Visual effect hiding | [VISUAL-EFFECTS.md](VISUAL-EFFECTS.md) |
| In-world language selection and pack research | [LANGUAGE.md](LANGUAGE.md) |
| Auto Attack/Use and permanent movement | [AUTOMATION.md](AUTOMATION.md) |
| Breaking/placement restrictions and research | [RESTRICTIONS.md](RESTRICTIONS.md) |
| Hand Restock | [HAND-RESTOCK.md](HAND-RESTOCK.md) |
| Tool/Weapon Switch, Tool Protection and Auto Elytra | [EQUIPMENT.md](EQUIPMENT.md) |
| Fake Offhand | [FAKE-OFFHAND.md](FAKE-OFFHAND.md) |
| Villager trade helpers and L-166 diagnostics | [TRADE-HELPERS.md](TRADE-HELPERS.md) |
| Item Transfer and L-169 native-handler investigation | [ITEM-TRANSFER.md](ITEM-TRANSFER.md) |
| Enchantment order planner and the panels beside the anvil (L-170) | [ENCHANT-ORDER.md](ENCHANT-ORDER.md) |
| Item tooltip lines: work penalty, stew effect, property values, ID, tags and NBT | [ITEM-DETAILS.md](ITEM-DETAILS.md) |
| Minimap, radar, waypoints and world map | [MAP.md](MAP.md) |
| Schematics, checking, materials and area save | [SCHEMATIC.md](SCHEMATIC.md) |
| Schematic controls and cut-plane layers (implemented contract) | [SCHEMATIC-CONTROLS.md](SCHEMATIC-CONTROLS.md) |
| Integration between features (proposal, L-111) | [INTEGRATION.md](INTEGRATION.md) |

## Keeping the docs consistent

- Put shared decisions in DESIGN, one feature's decisions in its document
  (a "Decided" section) and task choices/dependencies in the L-item. DESIGN
  stays short: a rule that only one feature follows does not go there.
  Link to feature contracts rather than copying them into execution order or
  UI summaries. The L-item wins if its status disagrees with a summary.
- Put current notes ahead of dated records. Label superseded checkpoints so
  an old experiment cannot be mistaken for the next task.
- Record an in-game result at the top of VALIDATION-LOG, then update VALIDATION.
  Build/tests alone do not establish runtime behavior.
- A feature added, renamed, made default on/off or out of Experimental is
  changed in both the README feature tables and FEATURES.md, using the
  English names of the settings screen.
- Keep BACKLOG-DONE and VALIDATION-LOG intact. Search for the L-number or
  feature instead of reading either whole.
- Notion's idea inbox holds unchosen proposals. When importing selected
  notes, assign or reuse an L-item, preserve unresolved choices as Design /
  Research, and record the mapping in the inbox. Current task status stays
  in the repository; a dated Notion checkpoint does not reopen shipped work.
- Keep only current work and remaining coverage in BACKLOG. Completed
  planning and old release/regression checkpoints are retained in
  BACKLOG-DONE, with their pending-status wording labeled as historical.
- Update the demo index when its decision or implementation status changes.
  Mockups show intent; current text and implementation can differ from them.
- Machine-specific paths stay in the gitignored `AGENTS.local.md`.

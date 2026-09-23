# Design demos

Standalone HTML mockups used to agree on UI before implementation. Open them
in a browser (they need network access only for the Google Fonts stylesheet).
Text in the demos is Japanese because they were reviewed in Japanese.

| Demo | Status | Notes |
|---|---|---|
| [settings.html](settings.html) | Implemented (layout A) | Sidebar + table, switches, key caps. The native screen is the source of truth where they differ. |
| [shapes.html](shapes.html) | Implemented | Draft → create flow, dock button. "Lines + faces" was dropped after review. |
| [hud.html](hud.html) | Under review | HUD elements, layout editing, target card, toggle toast. Do not implement until docs/DESIGN.md marks the HUD section Decided. |

Rules for agents:

- A demo shows intent and structure, not exact pixels. Build with the tokens
  and widgets in `src/ui/Widgets.h` and the sizes in docs/DESIGN.md.
- The "確認したいこと" (open questions) box at the bottom of a demo lists what
  was still undecided. Check docs/DESIGN.md and docs/BACKLOG.md for the answers.
- When a new demo is made, add it here with its status.

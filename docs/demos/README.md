# Design demos

Standalone HTML mockups used to agree on UI before implementation. Open them
in a browser (they need network access only for the Google Fonts stylesheet).
Text in the demos is Japanese because they were reviewed in Japanese.

| Demo | Status | Notes |
|---|---|---|
| [settings.html](settings.html) | Implemented (layout A) | Sidebar + table, switches, key caps. The native screen is the source of truth where they differ. |
| [shapes.html](shapes.html) | Implemented | Draft → create flow, dock button. "Lines + faces" was dropped after review. |
| [hud.html](hud.html) | Implemented, being reworked | HUD elements, layout editing, target card, toggle toast. BACKLOG L-02 to L-04, L-08. |
| [hud-editor.html](hud-editor.html) | Decided, being implemented | Rework after the first editor build: three editing models, three card styles, target card with icons and bars. |
| [hotkey-conflicts.html](hotkey-conflicts.html) | Decided, being implemented | Warning style for shared/overlapping bindings in every key cell and a hover tooltip listing every related binding (L-32 follow-up). |

Rules for agents:

- A demo shows intent and structure, not exact pixels. Build with the tokens
  and widgets in `src/ui/Widgets.h` and the sizes in docs/DESIGN.md.
- The box at the bottom of a demo lists either open questions ("確認したいこと")
  or, once agreed, the decisions ("決定事項"). docs/DESIGN.md is authoritative.
- When a new demo is made, add it here with its status.

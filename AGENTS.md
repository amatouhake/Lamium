# Lamium agent guide

Lamium is a client-side quality-of-life mod for Minecraft Bedrock on
LeviLamina Client (Windows x64, C++20, clang-cl, xmake). This file is the
working manual for any coding agent. Read it fully before changing code.

## Where things are

- Pick work: BACKLOG.md (the index: policy, execution order, one line per
  item), then only that item's file `docs/backlog/L-<n>.md` and the feature
  document it links. The item file wins if a summary drifts.
- Document roles and the feature documents: [docs/README.md](docs/README.md).
- Shared product and UI rules: [docs/DESIGN.md](docs/DESIGN.md) (short; one
  feature's decisions are in that feature's document).
- What was checked in game: [docs/VALIDATION.md](docs/VALIDATION.md).
- Machine-specific paths: `AGENTS.local.md` if present (gitignored; never
  copy it into tracked files).

A decision made in chat that affects implementation goes into the repository:
a shared rule into DESIGN.md, one feature's into its document, task choices
into the item file, rewritten as the current state (BACKLOG.md "How items
are kept"). In long feature documents, newer decisions win over older dated
checkpoints. Finished items (`docs/backlog/done/`) and VALIDATION-LOG.md are
records: search them (`git grep -n "L-66" docs/backlog`), never read them all.

## Talking to the user

- Reply in the user's language. Code, comments, commit messages and repo
  docs are English.
- The user tests in Minecraft; you cannot. A change to game behavior ends with
  a short in-game checklist and the build tested (commit, DLL SHA-256).
- "Builds and tests pass" is not "works in game". Do not claim runtime
  behavior nobody has seen.
- Name a backlog item with its title, never the number alone: "L-183
  Disregard selected schematic check mismatches", not "L-183". In a list
  the title may be shortened, but keep enough to tell what it is.

## Build, test, deploy

```bash
xmake f -a x64 -m release -p windows --target_type=client -y   # once
xmake build Lamium            # DLL -> bin/Lamium/Lamium.dll (+ .pdb, manifest.json)
xmake build LamiumTests && xmake run LamiumTests               # pure tests
xmake build LamiumNativeTests && xmake run LamiumNativeTests   # SDK-type tests
```

- Run `LamiumTests` after every change; it must print no failures. UI changes
  also run `scripts/Check-UiTokens.ps1` (CI does).
- Deploy only when the user wants to test, with Minecraft closed: copy
  `bin/Lamium/Lamium.dll`, `Lamium.pdb`, `manifest.json` into
  `<instance>/mods/Lamium/` and report the SHA-256 of both copies. Log:
  `<instance>/mods/Lamium/logs/lamium.log`.
- Trace builds are xmake options (`--camera_trace=y`, see `xmake.lua`); never
  deploy or commit with one left on unless asked.
- Search with `git grep` or within `src tests docs`; `build/` holds SDK
  copies and a recursive grep over it takes minutes.
- Game types: SDK headers in the xmake package cache
  (`%LOCALAPPDATA%/.xmake/packages/l/levilamina-client-sdk/*/*/include/mc/`).
  Search there before guessing a name.
- Windows: use `python`, not `python3`. Files are UTF-8 (Japanese and Chinese
  in `src/ui/Translations*.h`); prefer the edit tool to shell rewrites.

## Architecture and rules

```
src/app        Runtime (feature lifecycle, logging, settings access)
src/settings   Settings struct, option catalog (Options.h), JSON store
src/input      Actions and default keys (Binding.h), dispatch (Actions.cpp)
src/ui         Settings screen, views, shared widgets, layout math, translations
src/overlay    World-space geometry (pure) and rendering (WorldOverlay.cpp)
src/features   One folder per area; hooks live here
tests          Pure unit tests (no game types). tests-native: SDK-type tests
```

1. **Pure logic in headers, game glue in .cpp.** Layout, geometry, planning
   and state machines live in `*.h` without Minecraft types, tested in
   `tests/`. First ask "which part can be a pure function with a test?".
2. **No game pointers kept across frames.** Key and mouse events arrive
   outside the client tick: never touch game state there; queue the action
   (`input/CustomInput.cpp`).
3. **Every feature restores vanilla behavior** when disabled, on world exit,
   dimension change and focus loss.
4. **Settings**: field in `Settings.h`, entry in `Options.h`, load/save in
   `SettingsStore.cpp` (tolerate missing keys), row in `SettingsRows.h`,
   strings in all four languages. The tests catch most omissions.
5. **Actions**: append to `enum Action` and `actions` in `Binding.h` (never
   reorder; ids are saved), toggles in `ToggleAction.h`, presses in
   `Actions.cpp`. Press/Hold/Toggle is decided by the action.
6. **Translations**: English, Japanese, `zh_CN` and `zh_TW` for every key;
   the Chinese files keep `Translations.h`'s order (the build checks it).
   Keep `Name: {}` labels with an ASCII ": ". See docs/TRANSLATING.md.
7. **UI** draws only rectangles and text through `src/ui/Widgets.h` (read
   its header comment). A control the shared files (`Widgets.h`,
   `ListViewWidgets.h`) lack goes into them, never into the screen's file or
   copied from another screen; no color literals (`Check-UiTokens.ps1`). A
   control that must look or behave unlike the shared one (another button
   look, color meaning, Esc or focus rule) is a Design question: stop and
   ask. Older screens still hold local copies (L-341); they are not the
   pattern.
8. **Game dependencies**: a new hook gets an entry in
   [docs/GAME-UPDATES.md](docs/GAME-UPDATES.md) (CI checks it). A path that
   writes game memory or relies on an unverifiable meaning asks
   `versionSensitiveAllowed()` (`app/Versions.h`). Version moves follow the
   playbook there; the verified game version changes last.
9. **Engineering behavior** (DESIGN.md has the wording): bound expensive work
   per frame/tick, discard stale async results by world/dimension
   generation, confirm gameplay changes from authoritative state, and fail
   open to vanilla when a version-sensitive path cannot be verified.

## Code style

- Terse, few comments, comments explain *why*. Match the surrounding style;
  UI controls still come from the shared widgets (rule 7).
- C++20, no exceptions across hook boundaries (`noexcept` handlers catch).
- Tests use `check(bool, "what must hold")` in `tests/*Tests.cpp`, declared
  and called from `main` in `tests/CameraTests.cpp`.
- Never copy code, strings, assets or distinctive constants from other mods.
  [docs/PROVENANCE.md](docs/PROVENANCE.md) lists reference-only projects
  (do not open their source) and where another mod may be named at all.
  Specs describe Lamium in its own terms, not "like mod X".

## Git

- Follow the user's branch and push preferences (`AGENTS.local.md`);
  otherwise work on a branch and do not push unasked.
- One logical change per commit. Subject: English, imperative, sentence case,
  no prefix, ~70 chars. Body only when the why is not obvious.
- Commit only when build + LamiumTests pass. Check `git diff --stat` first.
- Update the relevant docs in the same or next commit. A game result goes on
  top of VALIDATION-LOG.md (build, hash, what was and was not seen) and into
  the feature's VALIDATION.md row. A finished item's file moves to
  `docs/backlog/done/` (and its line to BACKLOG-DONE.md); what is left for
  the release goes to `docs/backlog/pre-release-checks.md`.

## When to stop and escalate

Stop, write down what you found, and hand back to the user when:

- The task is **Design** or **Research** in BACKLOG.md, or leaves a
  user-visible choice open. Do not decide product/UX questions.
- A game function is not in the SDK headers, or one hook approach failed
  twice at runtime.
- A fix would change the settings format, action ids, or delete user data.
- Two build/test attempts in a row fail for reasons you do not understand.
- The change touches more than ~5 files outside the task's listed files.

Hand-back note: goal, commits, what you observed, the open question, your
best next guess.

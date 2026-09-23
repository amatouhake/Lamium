# Lamium agent guide

Lamium is a client-side quality-of-life mod for Minecraft Bedrock on
LeviLamina Client (Windows x64, C++20, clang-cl, xmake). This file is the
working manual for any coding agent (Claude Code, Pi, OpenCode, ...).
Read it fully before changing code.

- Product direction, UI rules and colors: [docs/DESIGN.md](docs/DESIGN.md)
- What to work on, and who should do it: [docs/BACKLOG.md](docs/BACKLOG.md)
- Per-feature technical notes: `docs/*.md` (CAMERA, OVERLAYS, RESTRICTIONS, ...)
- UI mockups agreed with the maintainer: `docs/demos/` (see its README)
- Machine-specific paths (instance folder etc.): `AGENTS.local.md` if present.
  It is gitignored; never copy its contents into tracked files.

The repository is the source of truth for work: everything needed to pick up
a task is in this file and `docs/`. If a decision made in chat affects
implementation, write it into DESIGN.md or BACKLOG.md.

## Talking to the user

- Reply in the user's language. Code, comments, commit messages and repo
  docs are English.
- The user tests in Minecraft. You cannot. Every change that touches game
  behavior ends with a short checklist: what to do in game and what they
  should see. Say which build (commit and DLL SHA-256) they are testing.
- Do not claim runtime behavior you have not seen confirmed. "Builds and
  tests pass" is not "works in game".

## Build, test, deploy

```bash
xmake f -a x64 -m release -p windows --target_type=client -y   # once
xmake build Lamium            # DLL -> bin/Lamium/Lamium.dll (+ .pdb, manifest.json)
xmake build LamiumTests && xmake run LamiumTests               # pure tests
xmake build LamiumNativeTests && xmake run LamiumNativeTests   # SDK-type tests
```

- Run `LamiumTests` after every change. It must print no failures.
- Deploy only when the user wants to test: Minecraft must be closed
  (`tasklist | grep -i Minecraft.Windows` prints nothing), otherwise the DLL
  copy fails. Copy `bin/Lamium/Lamium.dll`, `Lamium.pdb`, `manifest.json` into
  `<instance>/mods/Lamium/` and report the SHA-256 of both copies.
- Debug traces are xmake options (`xmake f --camera_trace=y` etc., see
  `xmake.lua`). Never deploy or commit with a trace option left on unless the
  user asked for a trace build. Reset with `xmake f --camera_trace=n ...`.
- Runtime log: `<instance>/mods/Lamium/logs/lamium.log`.
- SDK headers (read-only reference for game types): the xmake package cache,
  `%LOCALAPPDATA%/.xmake/packages/l/levilamina-client-sdk/26.51.5/*/include/mc/...`.
  Search there before guessing a member or function name.

### Windows shell pitfalls

- `python3` is a Store stub; use `python`.
- Write multi-line files with the file-writing tool, not bash heredocs with
  quotes/backticks inside.
- Files are UTF-8 (Japanese strings in `src/ui/Translations.h`). Tools like
  perl need `-Mutf8`/`-CSD` or they corrupt text; prefer the edit tool.

## Architecture in one page

```
src/app        Runtime (feature lifecycle, logging, settings access)
src/settings   Settings struct, option catalog (Options.h), JSON store
src/input      Actions and default keys (Binding.h), dispatch (Actions.cpp)
src/ui         Settings screen, Shapes view, widgets, layout math, translations
src/overlay    World-space geometry (pure) and rendering (WorldOverlay.cpp)
src/features   camera / information / inspection / interaction / inventory /
               lighting / visuals — one folder per area, hooks live here
tests          Pure unit tests (no game types). tests-native: SDK-type tests
```

Rules the code already follows; keep them:

1. **Pure logic in headers, game glue in .cpp.** Layout, geometry, planning
   and state machines live in `*.h` with no Minecraft types and are covered
   in `tests/`. Hooks and rendering call into them. When adding behavior,
   first ask "which part can be a pure function with a test?".
2. **No game pointers kept across frames.** Collect owned values each frame.
3. **Every feature restores vanilla behavior** when disabled, on world exit,
   dimension change and focus loss.
4. **Settings**: add a field in `Settings.h`, an entry in `Options.h`, load/save
   in `SettingsStore.cpp` (tolerate missing keys), a row in `SettingsRows.h`,
   and English + Japanese strings in `Translations.h`. `SettingsStoreTests`
   and `TranslationsTest` catch most omissions.
5. **Actions**: append to `enum Action` and `actions` in `Binding.h` (never
   reorder; ids are saved), map toggles in `ToggleAction.h`, handle presses in
   `Actions.cpp`. Behavior (Press/Hold/Toggle) is decided by the action.
6. **Translations**: English is the default, Japanese is required for every
   key. No Chinese unless requested.
7. **UI** draws only rectangles and text through `src/ui/Widgets.h`. Use the
   palette and widgets there; do not invent colors or sizes. See DESIGN.md.

## Code style

- Match surrounding code: terse, few comments, comments explain *why*.
- C++20, no exceptions across hook boundaries (`noexcept` handlers catch).
- Tests use `check(bool, "what must hold")` in `tests/*Tests.cpp`, declared and
  called from `main` in `tests/CameraTests.cpp`.
- Do not copy code, strings or assets from other mods (Litematica, MiniHUD,
  Tweakeroo, Flarial, LeviSchematic...). Behavior may be referenced; code may not.

## Git

- Follow the user's branch and push preferences (see `AGENTS.local.md`);
  otherwise work on a branch and do not push without being asked.
- One logical change per commit. Subject: English, imperative, sentence case,
  no prefix, ~70 chars (e.g. "Pull shapes slightly toward the eye to stop
  flicker inside blocks"). Body only when the why is not obvious.
- Commit only when build + LamiumTests pass. Push when the user asks or after
  they confirm a runtime check.
- Update the relevant `docs/*.md` in the same or next commit when behavior or
  validation status changes. Keep VALIDATION.md factual: verified vs not.

## When to stop and escalate

Cheap models: stop, write down what you found, and hand back to the user
(who may bring in a stronger model) when any of these happens:

- The task is marked **Design** or **Research** in BACKLOG.md, or the spec
  leaves a user-visible choice open. Do not decide product/UX questions.
- You need a game function you cannot find in the SDK headers, or the same
  hook approach failed twice at runtime.
- A fix would change settings file format, action ids, or delete user data.
- Two build/test attempts in a row fail for reasons you do not understand.
- The change touches more than ~5 files outside the task's listed files.

Hand-back note format: goal, what you changed (commits), what you observed
(test output, logs), the open question, and your best next guess.

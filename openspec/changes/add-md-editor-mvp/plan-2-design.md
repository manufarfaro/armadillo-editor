# Plan 2 Design — Toolbox Wiring + Ship

**Date:** 2026-04-30
**Status:** Approved, ready for implementation plan
**Scope:** All non-host-testable modules of the MVP plus on-device verification and release infrastructure. Splits into milestones `v0.1.0` (Plan 2a) and `v0.2.0` (Plan 2b).

This document captures the architectural decisions for Plan 2. The bite-sized TDD-style tasks live in separate plan files:

- `plan-2a-bare-shell.md` — bare-boot `.APPL` with empty editor pane → tag `v0.1.0`
- `plan-2b-mvp-features.md` — file I/O, view toggle, parse cycle, syntax coloring, smoke test → tag `v0.2.0`

The global architecture doc (`design.md`) remains the source of truth for the overall system; this file is the Plan-2-specific addendum.

## Goal

Deliver the MVP of Armadillo Editor as a shippable `.APPL`: opens a `.md` file, edits in a TextEdit-backed source pane with debounced syntax coloring, toggles to a QuickDraw-rendered Read pane, and saves. Tier 0 + 1 of `PRD.md`. Verified manually on QEMU; built and released via the existing `release.yml` workflow on every semver tag push.

## Why split into 2a and 2b

Plan 1's safety net was host TDD — every module had Unity tests run before any commit landed. Plan 2 has zero host tests for its modules (TextEdit, windows, menus, real QuickDraw, the Standard File package — all require live Toolbox). The equivalent risk-mitigation is a smaller intermediate ship.

`v0.1.0` (Plan 2a) gets a `.APPL` that boots on Quadra, opens an editor window with a typeable empty source pane, closes cleanly, and quits. If that boots, the foundational Toolbox-init / event-loop / window-creation / menu-dispatch / `src_pane` plumbing is verified before any of the runtime complexity (file I/O, parse cycles, view toggle) stacks on top. `v0.2.0` (Plan 2b) layers the real features onto the now-known-good shell.

A single Plan 2 covering everything would mean every Toolbox bug surfaces at once, mixed with feature bugs, against the same `.APPL` build. The split contains blast radius.

## Module breakdown

| Module | Where | Purpose | Plan |
|---|---|---|---|
| `armadillo.r` | repo root (expand existing) | `MBAR` 128 + `MENU` 128/129/130/131 (Apple/File/Edit/View), `WIND` 128 (editor window), `ALRT` + `DLOG` 256/257 (about, errors, unsaved-changes), `STR#` 128 (error strings per `design.md` §5.2), `vers` (1) for Finder Get Info | 2a + 2b |
| `src/app.c` | replaces `stub_main.c` | Toolbox init (`InitGraf` / `InitFonts` / `InitWindows` / `InitMenus` / `TEInit` / `InitDialogs` / `FlushEvents`), production `MacSyscalls` struct (file-scope static, per-module-static MacSyscalls long-lived ownership rule), main `WaitNextEvent` loop, dispatch to `menus_handle_command` and `win_editor_handle_event`, idle-time `debounce_poll` and parse-cycle drive (2b) | 2a + 2b |
| `src/menus.c/.h` | new | Menu setup from `MBAR` 128, `menus_handle_command(menu, item, win, sys)` dispatch table, About box (`Alert` 256), ⌘-shortcut routing via `MenuKey` | 2a |
| `src/win_editor.c/.h` | new | Opens the `WIND` 128 window, owns one `Doc*` + one `SrcPane*` + one `RenderModel*` + one `Arena*`, lays out source-pane bounds inside the content area, handles `mouseDown` / `keyDown` / `updateEvt` / `activateEvt`, drives the parse cycle (2b), implements view toggle (2b) | 2a (skeleton) + 2b (parse cycle, view toggle) |
| `src_pane/src_pane.c` | new (header `src_pane.h` already exists) | `TENew`-backed editable pane behind the vendor-free `src_pane.h` API; `src_pane_handle_event` forwards to `TEKey` / `TEClick`; `src_pane_apply_runs` walks the `MdStyleRun[]` and calls `TESetStyle`; `src_pane_draw` calls `TEUpdate` | 2a (no styling) + 2b (apply runs) |
| `render/draw_qd.c/.h` | new | Real `DrawOps` vtable: thin forwarders to `SetFont` / `MoveTo` / `DrawText` / `LineTo` / `FrameRect` / `GetFontInfo`, plus `RGBForeColor` for `set_fg` | 2a (struct exists, exercised indirectly via TextEdit) + 2b (used directly by `render_layout_and_draw`) |
| `src/file_io.c` | extend existing | Replace the Plan 1 `kFileIoErrCancel` stubs in `file_io_open_interactive` and `file_io_save_as` with real `StandardGetFile` / `StandardPutFile` flows that delegate to existing `file_io_open` / `file_io_save` data paths | 2b |
| `test/smoke_test.c` | new test app | Separate `.APPL` (second CMake target). Hardcoded sequence: open a bundled fixture `.md`, validate `doc_text()` length, run a parse cycle through the real arena + render, save to `Smoketest.md` next to the app, exit. Manual-eyeball verification on QEMU before tagging `v0.2.0` | 2b |

Two architectural rules from earlier work are preserved:

**Vendor-free public headers (CLAUDE.md HARD rule).** No `WindowPtr`, `TEHandle`, `FSSpec`, `RGBColor`, or other Toolbox types cross any `.h` boundary. `src_pane.h`, `win_editor.h`, `menus.h` use opaque pointers (`SrcPane*`, `WinEditor*`) and project-owned types (`MdStyleRun`, `BlockKind`). `.c` files internally cast to/from Toolbox types as needed.

**Per-call vs. long-lived `MacSyscalls`** (from `arena-owns-macsyscalls`):
- `app.c`'s production `MacSyscalls` is file-scope static — lives for program lifetime, no copy needed.
- `WinEditor` and `SrcPane` retain `sys` past their constructors → take `const MacSyscalls* sys` as a parameter, copy `*sys` into a `MacSyscalls` field by value at init.
- `menus_handle_command` is per-call → takes `const MacSyscalls*` and does not retain it.

## Runtime data flow

### Boot (2a)

```
main()                                        [src/app.c]
  ├─ InitGraf, InitFonts, InitWindows, ...    [Toolbox init]
  ├─ build production MacSyscalls struct      [file-scope static]
  ├─ menus_install(MBAR=128)                  [menus_install hooks the MBAR]
  ├─ win = win_editor_new(&sys)               [empty Doc, empty SrcPane]
  └─ event_loop(win, &sys)                    [WaitNextEvent → dispatch]
```

### Event loop (2a)

Standard Mac event-loop shape — `WaitNextEvent` with sleep ticks driven by `debounce_state.dirty`:

| Event | Dispatch |
|---|---|
| `mouseDown` in menubar | `MenuSelect` → `menus_handle_command(menu, item, win, &sys)` |
| `mouseDown` in window content | `win_editor_handle_event(win, &event)` → `src_pane_handle_event` (calls `TEClick`) |
| `mouseDown` in go-away box | `win_editor_close(win)` (with unsaved-changes guard in 2b) |
| `keyDown` with ⌘ | `MenuKey` → `menus_handle_command` |
| `keyDown` (other) | `win_editor_handle_event` → `src_pane_handle_event` (calls `TEKey`, sets dirty, `debounce_on_edit`) |
| `updateEvt` | `win_editor_handle_event` → `src_pane_draw` (Source mode) or `render_layout_and_draw` (Read mode, 2b) |
| `activateEvt` | `win_editor_handle_event` → `src_pane_set_focus` |
| `null event` (idle) | `debounce_poll(&debounce_state, &sys)` → on fire, `win_editor_run_parse(win)` (2b) |

### Parse cycle (2b)

Triggered when `debounce_poll` returns 1 (≥ 250 ms since last keystroke). `win_editor_run_parse(win)` runs:

1. `arena_reset(arena)` (clears high_water; backing Handle preserved per `design.md` §4.3)
2. Snapshot `doc_text(doc, &len)`
3. `arena_ensure(arena, len * 4 + 16384)` — grow before parse, never grow mid-parse (`design.md` §4 footgun)
4. Build `MdParseSink[]` array of two: render sink + scanner sink
5. `mdparse_run(text, len, sinks, 2)`
6. **Success path:** commit new `RenderModel` + push `MdStyleRun[]` to `src_pane_apply_runs(pane, runs, run_count)`
7. **Failure path:** clear `current_model = NULL`, leave src pane styled as last good state (no error alert — would spam during long edits)

### View toggle (2b)

`View → Source` and `View → Read` menu items dispatch to `win_editor_set_mode(win, kModeSource | kModeRead)`. Source mode shows the `SrcPane` and routes events to it. Read mode hides the `SrcPane` and on every `updateEvt` calls `render_layout_and_draw(model, params, real_draw_ctx)` against the window's `GrafPort`. One mode visible at a time (per PRD); the unselected mode is invisible and inactive.

### File I/O (2b)

| Menu | Calls | Behavior |
|---|---|---|
| `File → New` | `win_editor_new_doc(win)` | Frees old doc/model; new empty doc + parse cycle |
| `File → Open…` | `file_io_open_interactive(&new_doc, &sys)` | `StandardGetFile` then `file_io_open` data path; on success swap doc into win |
| `File → Save` | `file_io_save(doc, &sys)` | Uses cached filename; if doc has none, falls through to Save As |
| `File → Save As…` | `file_io_save_as(doc, &sys)` | `StandardPutFile` then `file_io_save` data path; updates doc filename |
| `File → Close` / window go-away | `win_editor_close_with_guard(win)` | If `doc_is_dirty(doc)`: `StopAlert` 257 (Save / Discard / Cancel); else close immediately |
| `File → Quit` / ⌘Q | `app_quit(win)` | Same guard as Close, then exits the event loop |

## Error handling

`design.md` §5 already defines the per-module error enums and `STR#` 128 string indices. Plan 2 is a consumer of that machinery — nothing new to design — but the new failure surfaces are:

- **Toolbox init failure** (rare on 68030+/Sys 7) — `app.c` falls back to `SysBeep(30)` and exits; alerting requires init to have completed, which it has not.
- **Required resource missing** (`'WIND' 128`, `'MBAR' 128`) — `StopAlert` 256 with `STR# 128` index 1 ("Required resource missing"), then exit.
- **`TENew` returns NULL in `src_pane_new`** — propagates as `kSrcPaneErrOOM`; `app.c` surfaces with the standard out-of-memory alert and aborts the failing operation.
- **Parse failure mid-cycle** — Read pane goes blank until next successful parse (per `design.md` §4.4); no alert (spam-prone).
- **File I/O errors** (`kFileIoErrOpen`, `kFileIoErrTooBig`, `kFileIoErrRead`, `kFileIoErrOOM`) — surface via existing alert paths from Plan 1; Plan 2 just wires `kFileIoErrCancel` from `Standard File` cancellation as a silent return.
- **Unsaved-changes on close/quit** — `StopAlert` 257 with three buttons (Save / Discard / Cancel) using a `MissingItems` filter. Save delegates to `file_io_save` (or `file_io_save_as` if no filename); Discard proceeds; Cancel aborts the close/quit.

## Testing strategy

Plan 1 modules retain their host TDD coverage (`make -f Makefile.hosttests test` continues to pass on every push). Plan 2 modules are not host-buildable. Verification flow:

1. **CI builds the `.APPL` and `.dsk`** on every `push` to `main` (existing `release.yml` cross-build job, runs in `ghcr.io/autc04/retro68:latest`).
2. **Manual checklist run on QEMU** before each tag push. Two checklists, written as part of the corresponding plan files:
   - `qa-checklist-2a.md` — boot, menu bar, window open/close, type into editor, ⌘Q
   - `qa-checklist-2b.md` — open `.md`, edit + see syntax coloring, view toggle, save, unsaved-changes alert, smoke test
3. **`smoke_test.c` (2b)** is a separate `.APPL` target. Hardcoded sequence with no menu interaction; intended for QEMU eyeball-verification of the parse → render → save pipeline against a known-good fixture. Runs from launch to exit unattended.

Unscripted-but-deterministic verification was deliberately preferred over scripted QEMU automation because (a) driving Toolbox UI events deterministically through QEMU's monitor is itself a multi-week project, (b) the manual checklist scales to all of MVP scope in one sitting, (c) the `smoke_test.c` covers regressions in the data-path code that would matter most.

## Release process

Existing `release.yml` already triggers on `v*` tags and uploads `.APPL` + `.dsk` to a GitHub Release with auto-generated notes. Plan 2 reuses it without changes.

- **Versioning:** semver, staying in the 0.x range while the editor evolves toward stability. `v0.1.0` = Plan 2a complete. `v0.2.0` = Plan 2b complete (MVP shipped). Subsequent post-MVP feature drops bump the minor (`v0.3.0`, `v0.4.0`, …); patch bumps (`v0.2.1`) are reserved for hotfixes against a tagged release. `v1.0.0` is deliberately deferred — it signals an API/format stability commitment the project isn't ready to make yet.
- **Tagging:** `git tag v0.1.0 && git push --tags` after the corresponding QA checklist passes. Tags are SSH-signed via 1Password (per repo convention).
- **Branches:** no release branches. Single `main` line, semver tags only. Hotfix tags land on `main`.
- **`'vers' (1)` resource:** `armadillo.r` includes a `vers` resource hand-edited per release so Finder "Get Info" shows the human-readable version. Bumped in the same commit as the tag.
- **Release notes:** `generate_release_notes: true` in `release.yml` produces them from commit messages. Existing imperative-subject discipline gives clean output. A `CHANGELOG.md` is deferred unless the auto-output proves noisy.

## Acceptance Criteria

### Plan 2a (`v0.1.0`)

1. `.APPL` boots on QEMU/Quadra without alerts.
2. Menu bar shows Apple / File / Edit / View with at minimum: About, New, Close, Quit, Cut/Copy/Paste/Undo, Source/Read.
3. `File → New` opens an editor window with an empty TextEdit-backed source pane.
4. Typing into the source pane works (TextEdit accepts keystrokes; cursor moves; visible glyphs).
5. Window close-box and `File → Close` close the window without a dirty-check (no doc state yet).
6. ⌘Q quits cleanly without crashing.
7. Tag `v0.1.0` pushed; GitHub Release auto-published with `.APPL` and `.dsk` attached.

### Plan 2b (`v0.2.0`)

8. `File → Open…` opens any `.md` ≤ 32 KB; contents appear in source pane.
9. Editing a loaded doc triggers debounced parse (≥ 250 ms idle); syntax coloring appears on heading / bold / italic / link / code-span tokens.
10. `View → Read` switches to QuickDraw-rendered Read pane; `View → Source` returns to source pane.
11. `File → Save` writes to the open file; `File → Save As…` writes to a new location and updates the doc's filename.
12. Closing a dirty window prompts the unsaved-changes alert (Save / Discard / Cancel).
13. `smoke_test.APPL` runs unattended on QEMU: opens fixture, parses, renders, saves to `Smoketest.md`, exits cleanly.
14. Tag `v0.2.0` pushed; GitHub Release auto-published.

## Out of scope (deferred to later tiers)

Per `PRD.md`:

- Inline HTML whitelist render (Tier 2)
- Floating tool palette / status bar (Tier 3)
- Multi-document support (Tier 4)
- Finder-style documents window (Tier 5)
- Custom folder icon stamps (Tier 6)
- Custom text engine replacing TextEdit (post-Tier 6)
- `<img />` rendering inline (post-Tier 6)
- Scripted QEMU automation
- Spell check / grammar check / collaborative editing

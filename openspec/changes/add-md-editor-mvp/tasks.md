# Tasks: Markdown Editor MVP

Implementation checklist for the MVP. Tasks ship in three milestones:

- **Plan 1 (Groups 0–3):** host-testable pipeline. Shipped on `feat/plan-1-host-pipeline`; tagged `plan-1-complete`.
- **Plan 2a (Groups 4–5):** bare-shell `.APPL` boots and accepts typing. Shipped on `feat/plan-2a`; tagged `v0.1.0`.
- **Plan 2b (Groups 6–8):** feature-complete MVP — file I/O, view toggle, parse cycle, syntax coloring, smoke test. Targets `v0.2.0`.

## 1. Plan 1 — Host-testable pipeline (shipped, `plan-1-complete`)

- [x] 1.1 Repo foundation — `.gitignore`, `README.md` stub, `CLAUDE.md`, `CMakeLists.txt`, `armadillo.r` SIZE-only stub, `Makefile.hosttests`, vendored Unity, vendored md4c, `src/mac_syscalls.h`, `test/fake_syscalls.{h,c}`, build verifies
- [x] 1.2 Headers & opaque types — `render/blocks.h`, `render/inlines.h`, `render/arena.h`, `render/draw_qd.h`, `render/render.h`, `mdparse/mdparse.h`, `scanner/scanner.h`, `src_pane/src_pane.h`, `src/doc.h`, `src/file_io.h`, `src/debounce.h`
- [x] 1.3 Tests-first (TDD) — Unity test files for arena, mdparse, scanner, render (with `test/recorder.c`), doc, debounce, file_io's data path
- [x] 1.4 Implementations in dependency order — `arena`, `doc`, `debounce`, `mdparse` (md4c adapter + sink fan-out), `scanner` (MdParseSink → MdStyleRun[]), `render` (Block array + layout pass with style runs and word-wrap), `file_io` data-path
- [x] 1.5 Host CI workflows — `host-tests`, `lint` (cppcheck), `release` cross-build via `ghcr.io/autc04/retro68:latest`
- [x] 1.6 CodeQL Default Setup enabled (replaces the original custom workflow)
- [x] 1.7 `make -f Makefile.hosttests test` green; full `clean → test` runs in <2 s; baseline recorded

## 2. Plan 2a — Bare-shell `.APPL` (shipped, `v0.1.0`)

- [x] 2.1 Expand `armadillo.r` — `MBAR` 128 + `MENU` 128/129/130/131 (Apple/File/Edit/View), `WIND` 128, `ALRT`+`DLOG`+`DITL` 256/257, `STR#` 128 (8 error strings), `vers` (1)
- [x] 2.2 Wire `CMakeLists.txt` — Plan 1 sources + Plan 2a sources, drop `stub_main.c`
- [x] 2.3 Module skeletons — `render/draw_qd.{h,c}`, `src/menus.{h,c}`, `src/win_editor.{h,c}`, `src_pane/src_pane.c` (matching existing header), `src/app.c` (replaces `stub_main.c`)
- [x] 2.4 Production `MacSyscalls` struct in `src/app.c` (~20 thin wrappers, file-scope static)
- [x] 2.5 Main `WaitNextEvent` event loop with quit flag and 60-tick sleep (Plan 2b drops to 15)
- [x] 2.6 Menu install (`GetNewMBar`/`SetMenuBar`/`AppendResMenu`/`DrawMenuBar`); `MenuSelect` dispatch routing File→Quit / File→Close
- [x] 2.7 ⌘-shortcut routing via `MenuKey`
- [x] 2.8 About box on Apple → About Armadillo (`Alert(256, NULL)`)
- [x] 2.9 `win_editor_new` opens `WIND` 128 with empty `Doc` + TextEdit-backed `SrcPane`; `File → New` callback wired
- [x] 2.10 TextEdit-backed `src_pane.c` — `TENew`/`TEKey`/`TEClick`/`TEActivate`/`TEDeactivate`/`TEUpdate`/`TEIdle`/`TEDispose`; selection accessors
- [x] 2.11 Full mouseDown / keyDown / update / activate dispatch — in-menu / in-content / in-go-away / in-drag / desk-accessory routing
- [x] 2.12 QA checklist (`qa-checklist-2a.md`) walked through on QEMU Quadra 800 / System 7.5.5 — pass logged
- [x] 2.13 Tag `v0.1.0`; release.yml publishes GitHub Release with `.bin` + `.dsk`

## 3. Plan 2b — Feature-complete MVP (in progress, target `v0.2.0`)

### 3a. File I/O interactive flows

- [x] 3.1 Replace `file_io_open_interactive` stub with real `StandardGetFile` flow (filtered for `'TEXT'`); delegate to existing `file_io_open` data path
- [x] 3.2 Replace `file_io_save_as` stub with real `StandardPutFile` flow; update `Doc` filename on success
- [x] 3.3 Wire `File → Open…` and `File → Save As…` menu items in `src/menus.c` to call into `file_io_*` and swap doc on the front window
- [x] 3.4 Wire `File → Save` to `file_io_save` (uses cached filename); fall through to Save As if no filename
- [ ] 3.5 Verify on QEMU: open an `.md`, edit, save, reopen, byte-for-byte match

### 3b. Parse cycle drive

- [x] 3.6 Add `Arena*` field + `RenderModel*` field + `Scanner*` field + `DebounceState` to `WinEditor` struct
- [x] 3.7 `win_editor_run_parse(win)`: `arena_reset` → snapshot doc text → `arena_ensure(len*4 + 16384)` → build `MdParseSink[2]` (render sink + scanner sink) → `mdparse_run` → on success commit new `RenderModel` + push runs to `src_pane_apply_runs`; on failure clear `current_model = NULL` (no alert)
- [x] 3.8 Drop event-loop sleep from 60 ticks to 15 in `src/app.c` so `debounce_poll` fires within ~250 ms
- [x] 3.9 Idle-time `debounce_poll(&debounce_state, &g_syscalls)` in the event loop; on fire, call `win_editor_run_parse(g_front_window)`
- [x] 3.10 `src_pane`'s edit callback fires after `TEKey` mutates the doc, calling `debounce_on_edit` to mark dirty + capture the tick

### 3c. Syntax coloring

- [x] 3.11 Implement `src_pane_apply_runs` — walk the `MdStyleRun[]` and call `TESetStyle` for each run's range with the style's font + color (from a small style-table helper in `src_pane.c`)
- [ ] 3.12 Verify on QEMU: typing `# heading`, `**bold**`, `_italic_`, `[link](url)`, `` `code` `` shows distinct visual styles

### 3d. View toggle + Read pane render

- [x] 3.13 `win_editor_set_mode(win, kModeSource | kModeRead)` — Source mode shows `SrcPane`; Read mode hides it and routes update events to `render_layout_and_draw`
- [x] 3.14 Wire `View → Source` and `View → Read` menu items in `src/menus.c`; check the active mode item in the menu
- [x] 3.15 Fill in `render/draw_qd.c` callbacks with real QuickDraw calls — `SetFont`, `MoveTo`, `DrawText`, `LineTo`, `FrameRect`, `GetFontInfo`, `RGBForeColor`
- [ ] 3.16 Verify on QEMU: Read pane renders headings (Chicago 17 bold for H1, Chicago 14 bold for H2), paragraphs (Geneva 12), code (Monaco 10), bullets, blockquote bars, horizontal rules

### 3e. Unsaved-changes guard

- [x] 3.17 `win_editor_close_with_guard` — when `doc_is_dirty(doc)` is true, show `StopAlert(257)` (Save / Don't Save / Cancel)
- [x] 3.18 Wire `File → Close` and the window's go-away box to use the guard
- [x] 3.19 Wire `File → Quit` (and ⌘Q) to use the guard before exiting

### 3f. Smoke test `.APPL`

- [ ] 3.20 Add second CMake target `ArmadilloSmokeTest` with `test/smoke_test.c` source
- [ ] 3.21 Smoke test: open a hardcoded fixture `.md` from the resource fork, validate `doc_text()` length, run a full parse cycle (arena → mdparse → scanner + render), apply runs, render once, save to `Smoketest.md`, exit
- [ ] 3.22 Display PASS/FAIL via `Alert` before exit so an eyeball check on QEMU is unambiguous
- [ ] 3.23 Bundle the fixture markdown as a `'STR '` resource in `armadillo.r`
- [ ] 3.24 Run smoke test on QEMU; record pass

### 3g. QA + release

- [ ] 3.25 Write `qa-checklist-2b.md` covering 2b scenarios (Open/Save/Save As, syntax coloring, view toggle, unsaved-changes alert, smoke test pass)
- [ ] 3.26 Walk QA checklist on QEMU Quadra 800 / System 7.5.5; record pass entry with merge SHA
- [ ] 3.27 Open release branch `release/0.2.0`, merge to main, tag `v0.2.0`
- [ ] 3.28 Verify `release.yml` publishes the `v0.2.0` GitHub Release with `.bin` + `.dsk` attached

## 4. Documentation (rolling, not gated on milestone)

- [x] 4.1 Project conventions in `CLAUDE.md` (architecture, build, conventions, commit discipline)
- [x] 4.2 README updates: build instructions, CI badges, workflow descriptions, CodeQL note
- [ ] 4.3 Update README with v0.2.0 release notes after Plan 2b ships
- [ ] 4.4 Add `docs/acceptance.md` with the manual acceptance checklist from `design.md` §6.1

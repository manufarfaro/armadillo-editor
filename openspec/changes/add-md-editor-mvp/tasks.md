# Tasks: Markdown Editor MVP

Ordered TDD task list. Groups run in sequence; tasks within a group can be parallelized where noted. Each task maps to one or more scenarios in the capability specs under `specs/<cap>/spec.md`.

## Group 0 — Repo foundation

- [ ] 0.1 Create `.gitignore` (build/, build_test/, *.dsk, *.APPL, .DS_Store, xcode* artifacts)
- [ ] 0.2 Create top-level `README.md` with build instructions (placeholder; filled fully in Group 7)
- [ ] 0.3 Create `CLAUDE.md` with project conventions (mirrors hello-world's shape)
- [ ] 0.4 Create `CMakeLists.txt` stub that builds an empty `ArmadilloEditor.APPL` (opens a window, handles quit)
- [ ] 0.5 Create `armadillo.r` stub with minimal `'MBAR'` + File/Edit menus (enough to Quit)
- [ ] 0.6 Create `Makefile.hosttests` stub with `make clean` working
- [ ] 0.7 Vendor Unity test framework under `third_party/unity/` (pin to version; see reference project)
- [ ] 0.8 Vendor md4c under `third_party/md4c/` at a specific commit; record the commit SHA in `third_party/md4c/COMMIT.txt`
- [ ] 0.9 Create `src/mac_syscalls.h` with the `MacSyscalls` struct declaration (function-pointer fields; no implementations yet)
- [ ] 0.10 Create `test/fake_syscalls.c/h` with `FakeSyscalls` struct and `fake_syscalls_init()` returning sensible defaults
- [ ] 0.11 Verify `cmake ..` + `make` builds the stub .APPL; verify `make -f Makefile.hosttests clean` works

## Group 1 — Headers and interfaces (types-only, no implementations)

Producing these headers unblocks Group 2 (tests) to be written in parallel. No `.c` files in this group.

- [ ] 1.1 `render/blocks.h` — `BlockKind` enum, `Block` struct, exactly as in `design.md` §1.1
- [ ] 1.2 `render/inlines.h` — `StyleKind` enum, `MdStyleRun` struct
- [ ] 1.3 `render/arena.h` — opaque `Arena*` + public API (init/destroy/ensure/alloc/reset/high_water/capacity)
- [ ] 1.4 `render/draw_qd.h` — `DrawOps` vtable struct + `DrawContext`
- [ ] 1.5 `render/render.h` — opaque `RenderModel*` + public API + `LayoutParams` + `render_layout_and_draw`
- [ ] 1.6 `mdparse/mdparse.h` — `MdParseSink` callback struct + `mdparse_run` signature + `MdParseError` enum
- [ ] 1.7 `scanner/scanner.h` — opaque `Scanner*` + `scanner_sink`, `scanner_runs`, `scanner_reset`
- [ ] 1.8 `src_pane/src_pane.h` — opaque `SrcPane*` + public API (vendor-free; no TE types)
- [ ] 1.9 `src/doc.h` — opaque `Doc*` + data accessors + dirty flag API
- [ ] 1.10 `src/file_io.h` — `FileIoError` enum + `file_io_open`, `file_io_save` signatures
- [ ] 1.11 `src/debounce.h` — `DebounceState` struct + `debounce_on_edit`, `debounce_poll` signatures
- [ ] 1.12 Verify headers compile cleanly under `-std=c89 -Wall -Werror` with host `cc` (empty `int main(){}` fixture)

## Group 2 — Tests first (TDD; all tests fail at group completion)

Written against the Group 1 headers; unimplemented modules link-fail or return dummy values. This is expected and correct at this stage.

### 2a Arena tests

- [ ] 2.1 `render/arena_test.c` — test harness skeleton, FakeSyscalls wired
- [ ] 2.2 Test: `arena_init` allocates and HLocks a Handle of the requested size
- [ ] 2.3 Test: `arena_alloc` returns 4-byte-aligned pointers
- [ ] 2.4 Test: `arena_alloc` bumps `high_water` by the requested size rounded up to 4
- [ ] 2.5 Test: `arena_alloc` beyond capacity returns NULL without corrupting state
- [ ] 2.6 Test: `arena_ensure` grows the Handle successfully when capacity is insufficient
- [ ] 2.7 Test: `arena_ensure` preserves previous high_water on grow failure
- [ ] 2.8 Test: `arena_reset` resets high_water to 0 but does not shrink capacity
- [ ] 2.9 Test: `arena_destroy` disposes the Handle (verify dispose_handle_calls == 1)
- [ ] 2.10 Test: repeated reset+alloc cycles reuse the same backing memory without re-NewHandle

### 2b mdparse tests

- [ ] 2.11 `mdparse/mdparse_test.c` — harness, synthetic markdown fixtures
- [ ] 2.12 Test: empty source produces no sink events
- [ ] 2.13 Test: `# Hello` produces open(kBlockHeading,h_level=1) + text("Hello") + close(kBlockHeading)
- [ ] 2.14 Test: `- item1\n- item2` produces two open/close(kBlockListItem) with list_depth=1
- [ ] 2.15 Test: nested `- a\n  - b` produces list_depth=1 then list_depth=2
- [ ] 2.16 Test: `**bold**` produces on_span(kStyleStrong, ...)
- [ ] 2.17 Test: `_italic_` produces on_span(kStyleEmph, ...)
- [ ] 2.18 Test: `[text](url)` produces on_span(kStyleLink, ..., "url", 3)
- [ ] 2.19 Test: fenced code block produces kBlockCodeBlock with raw text
- [ ] 2.20 Test: `<aside>...</aside>` produces kBlockHtml with raw contents
- [ ] 2.21 Test: sink abort return code propagates as kMdParseErrSinkAbort
- [ ] 2.22 Test: multiple sinks receive events in array order

### 2c Scanner tests

- [ ] 2.23 `scanner/scanner_test.c` — harness
- [ ] 2.24 Test: empty event stream produces 0 runs
- [ ] 2.25 Test: on_span(kStyleStrong, start=5, length=8) produces one MdStyleRun with matching fields
- [ ] 2.26 Test: multiple non-overlapping spans produce multiple runs in input order
- [ ] 2.27 Test: nested spans (e.g., bold containing italic) produce runs for each with correct ranges
- [ ] 2.28 Test: heading open produces a full-line kStylePlain run covering the heading text (for "heading" coloring)
- [ ] 2.29 Test: kBlockHtml opens produce runs tagged kStyleHtmlSpan across the block's text
- [ ] 2.30 Test: `scanner_reset` clears accumulated runs; next parse starts fresh

### 2d Render tests

- [ ] 2.31 `render/render_test.c` — harness; `test/recorder.c/h` for the mock emitter
- [ ] 2.32 Test: empty model draws nothing (recorder has 0 calls)
- [ ] 2.33 Test: single paragraph draws set_font(Geneva 12 plain) + move_to + draw_text("..." )
- [ ] 2.34 Test: H1 draws set_font(Chicago 17 bold) + correct text
- [ ] 2.35 Test: H2 draws set_font(Chicago 14 bold) + correct text
- [ ] 2.36 Test: bulleted list item draws bullet glyph at indent_list*list_depth offset + text
- [ ] 2.37 Test: nested list (list_depth=2) draws deeper indent than list_depth=1
- [ ] 2.38 Test: blockquote draws a left bar (line) + text indented by indent_quote
- [ ] 2.39 Test: horizontal rule draws a line across content_width
- [ ] 2.40 Test: fenced code block draws set_font(Monaco 10) + text
- [ ] 2.41 Test: kBlockHtml draws set_fg(purple) + set_font(Monaco 10) + raw text (MVP behavior)
- [ ] 2.42 Test: inline bold span draws intermediate set_font(bold) + set_font(plain) around the range
- [ ] 2.43 Test: inline link span draws set_fg(link color) + set_font(underline) around the range
- [ ] 2.44 Test: layout wraps text that exceeds content_width at word boundaries
- [ ] 2.45 Test: layout advances the draw cursor vertically by line_height after each visual line

### 2e Doc tests

- [ ] 2.46 `src/doc_test.c`
- [ ] 2.47 Test: `doc_new` returns a non-NULL Doc with empty text and dirty=false
- [ ] 2.48 Test: `doc_set_text` stores the bytes and updates text_length; sets dirty=true
- [ ] 2.49 Test: `doc_mark_clean` clears the dirty flag
- [ ] 2.50 Test: `doc_free` does not leak (run with valgrind / leak detector if available)

### 2f Debounce tests

- [ ] 2.51 `src/debounce_test.c` — FakeClock harness
- [ ] 2.52 Test: fresh state: poll returns "no parse needed"
- [ ] 2.53 Test: after on_edit + clock advance < kParseDebounceTicks → poll returns "no parse"
- [ ] 2.54 Test: after on_edit + clock advance ≥ kParseDebounceTicks → poll returns "parse now"
- [ ] 2.55 Test: two consecutive on_edits reset the debounce window
- [ ] 2.56 Test: poll after firing clears dirty (next poll returns "no parse")

### 2g File I/O tests (host-buildable parts)

- [ ] 2.57 `src/file_io_test.c`
- [ ] 2.58 Test: file_io_open on a Doc of 0 bytes succeeds with empty text
- [ ] 2.59 Test: file_io_open on a file > kMaxDocBytes returns kFileIoErrTooBig
- [ ] 2.60 Test: file_io_open with open_df_result = fnfErr returns kFileIoErrOpen
- [ ] 2.61 Test: file_io_open with fs_read returning ioErr returns kFileIoErrRead
- [ ] 2.62 Test: file_io_save writes Doc's text bytes verbatim
- [ ] 2.63 Test: file_io_open cleanup path: on failure mid-sequence, all opened resources are closed

### 2h Host-test build wiring

- [ ] 2.64 Update `Makefile.hosttests` with target rules for every test above
- [ ] 2.65 Verify `make -f Makefile.hosttests test` builds all tests (they will fail; that's expected)
- [ ] 2.66 Record baseline failure count for comparison as Group 3 lands

## Group 3 — Implementations in dependency order

Each task ends when its module's tests pass. Module order matches the dependency DAG.

### 3a Arena

- [ ] 3.1 Implement `arena_init` (NewHandle, HLock, populate struct)
- [ ] 3.2 Implement `arena_alloc` (4-byte align, bump, NULL-on-overflow)
- [ ] 3.3 Implement `arena_ensure` (doubling-to-cap-then-linear growth; HUnlock/SetHandleSize/HLock; update base)
- [ ] 3.4 Implement `arena_reset` (zero high_water; keep backing)
- [ ] 3.5 Implement `arena_destroy` (DisposeHandle, free struct)
- [ ] 3.6 Verify all §2a tests pass

### 3b Doc

- [ ] 3.7 Implement `doc_new` / `doc_free` / `doc_text` / `doc_set_text` / dirty flag helpers
- [ ] 3.8 Verify all §2e tests pass

### 3c Debounce

- [ ] 3.9 Implement `debounce_on_edit` and `debounce_poll` against the FakeClock interface
- [ ] 3.10 Verify all §2f tests pass

### 3d mdparse

- [ ] 3.11 Implement `mdparse_run`: md4c callback set, depth counter, translation to MdParseSink events, multi-sink fan-out
- [ ] 3.12 Implement error remapping (md4c return code → MdParseError)
- [ ] 3.13 Verify all §2b tests pass

### 3e Scanner

- [ ] 3.14 Implement `scanner_new`, `scanner_reset`, the MdParseSink callbacks, `scanner_runs`
- [ ] 3.15 Verify all §2c tests pass

### 3f Render

- [ ] 3.16 Implement `render_model_new` + MdParseSink callbacks → Block array construction
- [ ] 3.17 Implement `LayoutParams` defaults helper
- [ ] 3.18 Implement layout pass: block-by-block, line-by-line, advancing cursor; word-wrap
- [ ] 3.19 Implement style-run application during inline drawing (set_font / set_fg per run)
- [ ] 3.20 Implement each block kind's layout branch: paragraph, heading (per level), list item, blockquote, code block, hr, html (raw)
- [ ] 3.21 Verify all §2d tests pass

### 3g File I/O (host-buildable parts)

- [ ] 3.22 Implement `file_io_open` data-path (open_df, get_eof, size check, new_handle, hlock, fs_read, build Doc)
- [ ] 3.23 Implement `file_io_save` data-path (new_file or get_eof, SetFPos, FSWrite; cleanup on failure)
- [ ] 3.24 Verify all §2g tests pass

## Group 4 — Toolbox-coupled targets (no host tests; on-device verification)

- [ ] 4.1 `src/draw_qd_real.c` — implement each `DrawOps` field using real QuickDraw primitives
- [ ] 4.2 `src_pane/src_pane.c` — TE wrapper behind vendor-free API (TEHandle, TEInit, TEActivate, TEIdle, TEKey, TESetStyle, etc.)
- [ ] 4.3 `src/file_io.c` — Standard File parts (SFGetFile, SFPutFile; wire to real open_df / fs_read / new_file; check-against-fnfErr etc.)
- [ ] 4.4 `src/menus.c` — MBAR init, menu command dispatch table, keyboard shortcut mapping
- [ ] 4.5 `src/win_editor.c` — editor window creation, layout (source pane fills window in Source mode; Read pane fills window in Read mode), update event redraw, activate/deactivate, mode toggle on View menu command
- [ ] 4.6 `src/app.c` — main(), Toolbox init (InitGraf, InitFonts, InitWindows, InitMenus, TEInit, InitDialogs, InitCursor), menu bar setup, main WaitNextEvent loop with debounce poll, MacSyscalls real wiring, quit handling
- [ ] 4.7 Implement real `MacSyscalls` wrappers in `src/app.c` (real_open_driver, real_tick_count, real_new_handle, real_hlock, real_hunlock, real_dispose_handle, real_open_df, real_fs_read, real_fs_write, real_fs_close, real_get_eof, real_set_handle_size, real_set_fpos, real_note_alert, real_stop_alert, real_standard_get_file, real_standard_put_file, real_gestalt, real_mem_error)
- [ ] 4.8 Verify cross-compiled `ArmadilloEditor.APPL` launches in Quadra 800 VM, can open and save a small .md file

## Group 5 — Resources

- [ ] 5.1 Fill `armadillo.r` with full 'MBAR' + 'MENU' 128..131 entries per design §7.5
- [ ] 5.2 Add 'ALRT' 256..265 + matching 'DITL' resources for every error code in design §5.2
- [ ] 5.3 Add 'STR#' 128 with all error messages in the order specified in design §5.2
- [ ] 5.4 Add 'ICN#' 128 and 'ICON' 128 with placeholder 32×32 1-bit armadillo icon
- [ ] 5.5 Verify all resources load in ResEdit and the menu bar + alerts render correctly

## Group 6 — Smoke test

- [ ] 6.1 Write `src/smoke_test.c`: runs inside the VM as a separate `.APPL`
- [ ] 6.2 Smoke test step 1: programmatically create a Doc with `# Hello\n\ntext` content
- [ ] 6.3 Smoke test step 2: run mdparse_run through scanner + render sinks
- [ ] 6.4 Smoke test step 3: apply scanner runs to a real SrcPane and verify via its API that styles are present
- [ ] 6.5 Smoke test step 4: render_layout_and_draw into a real window, show for 2 seconds
- [ ] 6.6 Smoke test step 5: save the Doc to a test file; re-open; verify byte-for-byte match
- [ ] 6.7 Smoke test step 6: display PASS/FAIL in a dialog box (DrawString or Alert)
- [ ] 6.8 Add `ArmadilloSmokeTest` target to CMakeLists.txt per design §7.2
- [ ] 6.9 Build smoke test .APPL; run manually in Quadra 800 VM; verify PASS

## Group 7 — Documentation

- [ ] 7.1 Write full `README.md` with architecture diagram, build instructions for both target and host, how to run in QEMU, configuration constants, troubleshooting notes
- [ ] 7.2 Write `CLAUDE.md` conventions doc (project overview, architecture, build system, testing, conventions) modelled on hello-world's
- [ ] 7.3 Write `docs/acceptance.md` with the manual acceptance checklist from design §6.1 Tier 3
- [ ] 7.4 Update `third_party/md4c/README.md` noting the pinned commit, license, and upgrade procedure
- [ ] 7.5 Update `src_pane/README.md` noting "TE-backed for MVP; replace internals with custom piece-table engine in a future change — keep the public header vendor-free"
- [ ] 7.6 Verify all docs render correctly in a markdown viewer; no broken internal links

## Group 8 — Ship prep

- [ ] 8.1 Run full host test suite; confirm all tests pass (`make -f Makefile.hosttests test`)
- [ ] 8.2 Run smoke test .APPL on Quadra 800 VM; confirm PASS displayed
- [ ] 8.3 Run manual acceptance checklist; confirm all items pass
- [ ] 8.4 Build release `.dsk` with `ArmadilloEditor.APPL`; verify it boots and runs on a fresh Quadra 800 VM image
- [ ] 8.5 Tag release commit `mvp-1.0.0`
- [ ] 8.6 Write a brief release notes entry describing what MVP includes and what's deferred to later tiers

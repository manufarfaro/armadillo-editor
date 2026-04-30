# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Armadillo Editor — a native System 7 markdown editor for **68030-and-up classic Macintosh**, cross-compiled with Retro68. Single `.md` file at a time, Source ↔ Read toggle, QuickDraw-rendered preview, TextEdit-backed source pane with syntax coloring. Period-correct UI. No network, no cloud.

## Source of truth — read these before touching code

The spec comes first; it drives implementation. Before proposing code changes, check:

1. `PRD.md` — product scope, MVP boundary (Tier 0+1), tier roadmap, non-goals.
2. `openspec/changes/add-md-editor-mvp/design.md` — the 7-section architecture doc. Section 1 (data model), Section 4 (arena), Section 5 (error model) are the most load-bearing.
3. `openspec/specs/<capability>/spec.md` — authoritative Given/When/Then scenarios for the 7 capabilities (`app-shell`, `doc-store`, `src-pane`, `mdparse`, `scanner`, `render`, `file-io`). These drive TDD.
4. `openspec/changes/add-md-editor-mvp/plan-1-host-pipeline.md` — ordered TDD step list for the host-testable pipeline (Groups 0–3). Plan 2 (Toolbox wiring + ship) is not written yet.

If implementation-time reality conflicts with the design, update the design inline with a dated log entry — do not silently diverge.

## Current state

Pre-implementation. The repo holds PRD, OpenSpec change proposal + design + tasks + specs, and Plan 1. No source yet. Plan 1 execution creates `src/`, `src_pane/`, `render/`, `mdparse/`, `scanner/`, `test/`, `third_party/md4c/`, `CMakeLists.txt`, `Makefile.hosttests`, and `armadillo.r`.

## Architecture (the non-obvious, cross-file parts)

- **Flat block model, not a tree.** Nesting lives in `list_depth` / `quote_depth` scalars on each `Block`. md4c's SAX callbacks fan into a `Block[]` directly; we never build a parent/child tree. The layout pass walks the array linearly emitting QuickDraw calls down the page. See `design.md` §1.
- **Single parse, two consumers.** One debounced md4c parse per edit cycle feeds an `MdParseSink` fan-out. `scanner/` consumes spans and produces `StyleRun[]` for the source pane's coloring. `render/` consumes the full event stream and builds a `RenderModel` for the Read pane. Never parse twice — it's expensive at 25 MHz.
- **Handle-backed arena with grow-before-parse policy.** All variable-length render-pipeline data lives in one `Handle` that's `HLock`ed for its lifetime. `arena_ensure()` grows up-front; `arena_alloc()` never grows — returns NULL on overflow. Grow-mid-parse is a footgun because `SetHandleSize` can relocate the block. See `design.md` §4.
- **Three test seams.** `MacSyscalls` (Toolbox vtable), `DrawOps` (QuickDraw vtable), `MdParseSink` (parser events). The renderer calls through `DrawOps`; scanner/render plug into `MdParseSink`; host tests inject fakes at each seam. Two storage shapes for `MacSyscalls`:
  - **Per-call APIs** (functions that use `sys` only during the call and don't retain it) take `const MacSyscalls* sys` as a parameter. Cheap (4-byte arg), no lifetime issue. Examples: `debounce_on_edit`, `debounce_poll`, `file_io_open`, `file_io_save`.
  - **Long-lived owners** (heap-allocated structs that retain `sys` past their constructor's stack frame) take `const MacSyscalls* sys` as a parameter, then copy `*sys` into a `MacSyscalls` field by value at init. The struct owns its private 80-byte snapshot and is independent of the caller's storage lifetime. Examples: `Arena`, `SrcPane` (Plan 2).
- **Vendor-free public headers — HARD rule.** No `TEHandle`, `WindowPtr`, `FSSpec`, `RGBColor`, or md4c types (`MD_BLOCK_TYPE`, etc.) cross any module boundary. Opaque pointers + project-owned types only. This is what lets us swap implementations (TE → custom piece-table, md4c → something else) without touching callers.
- **Top-level module folders, not deep `src/`.** `src_pane/`, `render/`, `mdparse/`, `scanner/` are peers of `src/` — not under it. `src/` holds only the app shell (event loop, menus, file I/O, doc container, debounce state machine, production `DrawOps` wiring). Reason: modules whose implementation is vendor-dependent or exceeds ~300 LoC get isolated with their own boundary.

## Build & test

### Host unit tests (fast, no Toolbox / QEMU)

```bash
make -f Makefile.hosttests test       # full suite (< 2 s)
make -f Makefile.hosttests clean
```

Run a single test binary directly:

```bash
make -f Makefile.hosttests build_hosttests/render_test
./build_hosttests/render_test                               # all tests in that binary
./build_hosttests/render_test -n test_render_layout_h1...   # Unity's -n selects by name prefix
```

Tests live **colocated** as `<module>_test.c`. Host-buildable modules: `render/arena`, `render/render`, `mdparse`, `scanner`, `src/doc`, `src/debounce`, `src/file_io`. NOT host-buildable (require Toolbox): `src_pane/`, `src/win_editor`, `src/menus`, `src/app`, `src/draw_qd_real`. Those are covered by the on-device smoke test `.APPL` (Plan 2).

### Cross-compile (Retro68 → Quadra 800 VM)

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/Documents/Projects/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
make
```

Produces `ArmadilloEditor.APPL` + `ArmadilloEditor.dsk`.

## Reference project

`~/Documents/Projects/QemuMac/retro68-projects/hello-world` — established patterns for the Retro68 toolchain file, `mac_syscalls.h` mockable-OS-calls layer, host-side `_test.c` colocation, OpenSpec directory layout, `goto cleanup` resource-release pattern. When in doubt about "how is this normally done in this toolchain?" look there first.

## Conventions (load-bearing ones)

- **C89 compatible.** Retro68's GCC emits C89-compatible code; host tests use `-std=c89 -Wall -Werror`. No C99 designated initializers, no `//` line comments in `.c`/`.h`, no variable declarations mid-block in library code. (Test code can be looser when it aids clarity.)
- **`goto cleanup` for any function acquiring ≥ 2 resources.** Label at bottom, reverse-order release, NULL out locals after ownership transfer so cleanup skips them.
- **No `printf` / `DebugStr` / `SysBeep` in library code.** Errors propagate as return codes. Alerts (`NoteAlert` / `StopAlert`) are the app shell's job only.
- **Per-module error enums, remapped at boundaries.** `MdParseError`, `RenderError`, `FileIoError`, etc. When an internal arena failure surfaces through `mdparse_run`, translate to `kMdParseErrArenaOOM` — don't propagate the raw arena code. Callers never see more than one layer of error enum.
- **Error messages live in `'STR#'` 128** in `armadillo.r`, indexed per `design.md` §5.2. Alert dialogs (`'ALRT'` 256..265) look up text by index — strings are editable in ResEdit without recompiling.
- **`kMaxDocBytes = 32767`** for MVP (TE's hard ceiling). A custom piece-table engine lifts this in a post-Tier-6 change — internals-only, behind the stable `src_pane.h` API.
- **Target floor: 68030, System 7.x.** 32-bit clean, no 68040-only instructions, FPU not assumed. Memory budget assumes 4–8 MB RAM.

## Commit discipline

- **Signing is required.** The user's private key lives in 1Password. Prefix every `git commit` with:

  ```bash
  SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
    git commit -m "..."
  ```

  `ssh-add -l` in a default shell returns "no identities" — the default `SSH_AUTH_SOCK` points at Apple's launchd socket, not at 1Password's agent. Overriding the socket per-command is the cleanest fix. Use `$HOME` rather than a hard-coded `/Users/<name>/` path so the example doesn't leak the local username into commits.

- **Do NOT add `Co-Authored-By: Claude` trailers.** The user explicitly does not want them. Commit messages end at the last line of the body.

- **Do NOT `--no-verify` signing or hooks.** If signing fails, re-check `SSH_AUTH_SOCK`. If a hook fails, investigate and fix the underlying cause.

- **Imperative commit subjects**, ≤ 72 chars. Explain *why* in the body only when non-obvious; `git log` for `hello-world` is a good style reference.

## Architecture gotchas — things to get right the first time

- **`SetHandleSize` can relocate the block.** After any grow, `*backing` is a different address. Never hold an arena pointer across an `arena_ensure` call — the pointer is dead. The whole point of "grow-before-parse" is to confine growth to moments where no live arena pointers exist.
- **`unsigned short` offsets in the block model.** Capped at 64 KB per field — fine because TE caps the doc at 32 KB. When the custom text engine lands and raises the cap, these need to become `size_t` across the model. Flagged in `design.md` §1.5.
- **md4c does NOT give source offsets on `enter_block` / `enter_span`.** Only on text events. Scanner tracks HTML block ranges via the min/max offsets of enclosed text events — see `scanner/scanner.c` and the scanner capability spec. Don't re-introduce a "block start offset" assumption.
- **Debounce uses `MacSyscalls.tick_count`, never `TickCount()` directly.** The indirection is what makes `FakeClock` tests deterministic.
- **No `malloc` / `NewHandle` / `NewPtr` in `render/`, `mdparse/`, `scanner/`.** Arena only. If you need scratch space in one of those modules, allocate from the arena, not the system heap.

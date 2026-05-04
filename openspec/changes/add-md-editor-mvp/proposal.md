# Proposal: Add Markdown Editor MVP

## Why

Armadillo Editor doesn't exist yet. We want a native System 7 markdown editor for 68030-and-up classic Macintosh that opens a `.md` file, edits it with syntax coloring, renders a QuickDraw read view, and saves — period-correct UI, no network, no cloud. The MVP corresponds to Tier 0 + Tier 1 in the tier roadmap (see `PRD.md`); subsequent tiers (inline HTML whitelist, floating tool palette, multi-document support, Finder-style documents window, custom folder icons, custom text engine) become their own OpenSpec changes layered on top of this one.

The architectural choices that drive everything else in the codebase:

- **Single md4c parse, flat block model, two consumers.** md4c's SAX stream naturally flattens into blocks with scalar `list_depth` / `quote_depth` attributes. A tree buys us nothing for rendering to a linear stream of QuickDraw calls and costs recursive allocation and traversal on a 25 MHz CPU. The same parse feeds both the source pane's syntax coloring and the read pane's render — never parse twice.
- **Arena-backed allocation, grow-before-parse only.** Mac OS Memory Manager uses movable `Handle` blocks; libraries that assume stable-pointer `malloc` don't compose with it cleanly. Our arena `HLock`s once, grows in controlled moments (never mid-parse, because `SetHandleSize` can relocate the block), and resets instead of freeing between cycles. This is the concrete answer to "how do we use a C library on classic Mac without fighting the Memory Manager."
- **Vendor-free public headers across every module boundary.** No `TEHandle`, `WindowPtr`, `FSSpec`, `RGBColor`, or md4c types leak across module boundaries. Every module's internals can swap without touching callers. This is the single most important architectural rule because the text engine (TextEdit today → custom piece-table later) and the HTML-rendering strategy (literal text → whitelist in Tier 2) are both scheduled to change.
- **Three test seams** — `MacSyscalls` vtable (any module that touches the OS), `DrawOps` vtable (renderer), `MdParseSink` (anything downstream of md4c). Each seam admits a hand-written fake. Host tests cover arena, render, mdparse, scanner, doc, debounce, and the host-buildable part of file I/O.
- **TextEdit for MVP with `kMaxDocBytes = 32767`** ships fastest and carries zero vendor-license risk. The custom text engine that lifts the cap is a future change scheduled after the more impactful UX tiers (HTML whitelist, tool palette) land.

## What Changes

This change adds the entire MVP. Concrete deliverables:

- Application shell — Toolbox initialization, event loop, menu bar (Apple / File / Edit / View), About box, Quit command, standard keyboard shortcuts (⌘N, ⌘O, ⌘W, ⌘S, ⌘Q)
- Single-document data model (one `.md` open at a time) with filename + dirty flag
- Standard File Open / Save / Save As flows for `.md` files up to 32,767 bytes
- Source pane as a vendor-free opaque module, TextEdit-backed in MVP, exposing `SrcPane*` API plus style-run application
- Markdown parsing via vendored md4c, called through a project-owned sink-dispatching adapter (`mdparse`)
- Syntax scanner producing `MdStyleRun` arrays from md4c events, applied to the source pane on each debounced edit cycle
- Block / inline layout engine in a top-level `render/` module: flat block array, arena-backed allocation, mockable `DrawOps` emitter, real-QuickDraw emitter for production
- View menu toggle between Source and Read modes; one mode visible at a time
- Inline HTML rendered as literal text in MVP (md4c-detected HTML blocks/spans shown as-is, colored distinctly); whitelist-rendered HTML deferred to Tier 2
- Error handling via period-correct Mac OS alerts (`NoteAlert` / `StopAlert`) driven by per-module error enums and `STR#` 128 string resources
- Host-side unit tests for every host-buildable module (Unity, colocated `<module>_test.c`)
- On-device smoke test `.APPL` covering edit → parse → render → save
- CMake build wiring for Retro68 cross-compilation; `Makefile.hosttests` for native host tests
- Vendored md4c at a pinned commit; resource file `armadillo.r` with menus / alerts / strings / version / app icon

**Out of scope** (deferred to later tiers — see `PRD.md`):

- Inline HTML whitelist renderer (Tier 2)
- Floating tool palette, status bar (Tier 3)
- Multi-document support, window switcher (Tier 4)
- Finder-style documents window with folder tree (Tier 5)
- Custom folder icon stamps (Tier 6)
- Custom text engine replacing TextEdit (post-Tier-6)
- Split view, fenced-code-block syntax highlighting, spell/grammar check
- Inline image rendering (markdown `![alt](url)` and HTML `<img>`) — post-Tier-6
- Link click-through — links render styled but inert in MVP
- Preferences dialog, network I/O, PPC / CFM-68K / Carbon / macOS targets, Unicode beyond MacRoman

## Capabilities

### New Capabilities

- `app-shell` — Toolbox init, event loop, menu bar, command dispatch, About box, debounce poll, graceful quit, production `MacSyscalls` wiring
- `doc-store` — opaque `Doc*` data container: text bytes, filename, dirty flag, `kMaxDocBytes` enforcement
- `src-pane` — vendor-free editable text pane (TextEdit-backed in MVP), style-run application via the public API
- `mdparse` — md4c adapter that hides md4c types behind a project-owned `MdParseSink` event interface
- `scanner` — `MdStyleRun[]` producer from `MdParseSink` events for the source pane's coloring
- `render` — arena allocator, flat block model, layout + draw via the swappable `DrawOps` vtable; production wires real QuickDraw, tests wire a recording sink
- `file-io` — File Manager + Standard File wrappers; data-path open/save are host-testable, interactive flows are Toolbox-only

### Modified Capabilities

None — this change introduces the whole codebase from zero.

## Impact

- **Code:** creates the entire repository — `src/`, `src_pane/`, `render/`, `mdparse/`, `scanner/`, `test/`, `third_party/md4c/`, `third_party/unity/`, `CMakeLists.txt`, `Makefile.hosttests`, `armadillo.r`.
- **Build / test:** introduces `make -f Makefile.hosttests test` (host TDD) and the Retro68 cross-build (`ghcr.io/autc04/retro68:latest`).
- **CI:** `host-tests`, `lint` (cppcheck), and `release` workflows. CodeQL Default Setup enabled at the repo level.
- **External dependencies:** md4c (vendored, pinned commit), Unity test framework (vendored).
- **Memory budget:** worst case ~170 KB arena for a 32 KB document; capped at 512 KB hard limit. Targets 4–8 MB Quadra-class machines.
- **Conventions:** establishes vendor-free-headers HARD rule, per-call vs. long-lived `MacSyscalls` storage shape, `goto cleanup` resource pattern, `STR#` 128 + `ALRT` 256+ error string indirection.

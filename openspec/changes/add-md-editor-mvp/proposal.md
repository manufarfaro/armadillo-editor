# Proposal: Add Markdown Editor MVP

## Intent

Build the first shippable version of Armadillo Editor — a native System 7 markdown editor for 68030-and-up classic Macintosh. This change introduces the complete MVP: a single-document editor that opens a `.md` file, shows it in a source pane with syntax coloring, renders a read view via QuickDraw using a custom block/inline layout engine, and toggles between the two views from the View menu.

The MVP corresponds to Tier 0 + Tier 1 in the tier roadmap (see `PRD.md`). Subsequent tiers (inline HTML whitelist, floating tool palette, multi-document, Finder-style documents window, custom folder icons, custom text engine) become their own OpenSpec changes layered on top of this one.

## Scope

### In scope

- Application shell: Toolbox initialization, event loop, menu bar (Apple / File / Edit / View), About box, Quit command, standard keyboard shortcuts (⌘N, ⌘O, ⌘W, ⌘S, ⌘Q)
- Single-document data model (one `.md` open at a time) with filename + dirty flag
- Standard File Open / Save / Save As flows for `.md` files up to 32,767 bytes
- Source pane as an opaque vendor-free module, implemented with Mac Toolbox TextEdit under the hood, exposing `SrcPane*` API plus style-run application
- Markdown parsing via vendored md4c, called through a project-owned sink-dispatching adapter (`mdparse`)
- Syntax scanner producing `MdStyleRun` arrays from md4c events, applied to the source pane on each debounced edit cycle
- Block/inline layout engine in a top-level `render/` module: flat block array, arena-backed allocation, mockable `DrawOps` emitter, real-QuickDraw emitter for production
- View menu toggle between Source and Read modes; one mode visible at a time
- Inline HTML rendered as literal text in MVP (md4c-detected HTML blocks/spans are shown as-is, colored distinctly); whitelist-rendered HTML is deferred to Tier 2
- Error handling via period-correct Mac OS alerts (`NoteAlert`/`StopAlert`) driven by per-module error enums and STR# string resources
- Host-side unit tests for every host-buildable module, Unity-based, colocated `_test.c` files
- On-device smoke test `.APPL` covering the core edit → parse → render → save path
- CMake build wiring for Retro68 cross-compilation
- `Makefile.hosttests` for running host unit tests with native `cc`
- Vendored md4c in `third_party/md4c/` pinned at a specific commit
- Resource file `armadillo.r` with menus, alerts, string list, app icon

### Out of scope

- Inline HTML whitelist renderer (Tier 2)
- Floating tool palette, status bar, status-bar mode toggle (Tier 3)
- Multi-document support, window switcher, multi-window UI (Tier 4)
- Finder-style documents window with folder tree (Tier 5)
- Custom folder icon stamps via Get Info paste-icon (Tier 6)
- Custom text engine replacing TextEdit (post-Tier-6)
- Split view mode
- Syntax highlighting for fenced code blocks
- Spell check, grammar check, collaborative editing
- PDF / HTML / Word export
- Markdown extensions beyond CommonMark
- Inline image rendering (markdown `![alt](url)` and HTML `<img>`) — deferred to a post-Tier-6 tier
- Relative path resolution for link `href` and image `src` — deferred to the same post-Tier-6 tier
- Link click-through (external URLs, sibling `.md` files, anchor fragments) — deferred; links render styled but inert
- Preferences dialog (MVP uses compile-time constants)
- Any network I/O
- PPC / CFM-68K / Carbon / macOS targets
- Mac OS 8.5+ MLTE integration
- Unicode beyond MacRoman

## Approach

Single pipeline, two consumers, flat intermediate model:

1. **User edits** the source pane. The edit increments `dirty` and records `last_keystroke_tick`.
2. **Debounce poll** in the main event loop's `WaitNextEvent` tick: if 250 ms have elapsed since the last keystroke and `dirty` is set, run a parse cycle.
3. **Single md4c parse** of the source buffer. md4c's SAX-style callbacks are dispatched by our `mdparse` adapter to an array of sinks.
4. **Two sinks consume the same event stream:**
   - The **scanner** accumulates `MdStyleRun` tuples (start, length, kind) for the source pane's syntax coloring.
   - The **render** module builds a `RenderModel`: a flat array of `Block` structs (no tree) with nesting expressed via `list_depth` / `quote_depth` scalar fields, arena-allocated inside a single `HLock`ed `Handle`.
5. **After parse:** scanner's style runs are applied to the source pane via its vendor-free API; render pane is invalidated and redraws on the next update event via a `DrawOps` emitter (real QuickDraw in production, a recorder in tests).

### Module decomposition

Top-level module folders, each with a vendor-free public header:

- `src/` — app shell, menus, event loop, window, file I/O, doc data container, debounce state machine, production `DrawOps` implementation
- `src_pane/` — opaque source-pane API, internally TE-backed in MVP (swappable to a custom text engine later without touching any other module)
- `render/` — arena allocator, flat block model, layout + emit-via-DrawOps
- `mdparse/` — md4c adapter, hides md4c from the rest of the codebase
- `scanner/` — style-run producer from `MdParseSink` events
- `third_party/md4c/` — vendored source at a pinned commit

Host tests are colocated per module as `<module>_test.c`. Target-only modules (`src_pane/`, parts of `src/`) are covered by an on-device smoke test `.APPL`.

## Why this approach

- **Single parse, flat block model, two consumers.** md4c's SAX stream naturally flattens into blocks with scalar depth attributes. A tree buys us nothing for rendering to a linear stream of QuickDraw calls and costs recursive allocation and traversal on a 25 MHz CPU.
- **Arena-backed allocation, grow-before-parse only.** Mac OS Memory Manager uses movable `Handle` blocks; libraries that assume stable-pointer `malloc` don't compose with it cleanly. Our arena `HLock`s once, grows in controlled moments (never mid-parse), and resets instead of freeing between cycles. This is the concrete answer to "how do we use a C library on classic Mac without fighting the Memory Manager."
- **Vendor-free public headers across every module boundary.** No `TEHandle`, `WEReference`, `RGBColor`, `WindowPtr`, or md4c types leak across module boundaries. Every module's internals can swap without touching callers. This is the single most important architectural rule in the project, because the text engine (TE today → custom later) and the HTML-rendering strategy (literal text → whitelist in Tier 2) are both scheduled to change.
- **Three test seams:** `MacSyscalls` vtable (any module that touches the OS), `DrawOps` vtable (renderer), `MdParseSink` (anything downstream of md4c). Each seam admits a hand-written fake with explicit behavior. Host tests cover arena, render, mdparse, scanner, doc, debounce, and the host-buildable part of file I/O.
- **Synchronous parse with debounce** is the simplest correct choice on cooperative classic Mac OS. The user isn't typing while we're parsing, by construction. If profiling shows an acceptable upper bound is exceeded, we revisit (incremental re-parse by block, or Thread Manager for 7.5+).
- **TE for MVP with `kMaxDocBytes = 32767`** ships fastest and carries zero vendor-license risk. The custom text engine that lifts the cap is a future change scheduled after more impactful UX tiers (HTML whitelist, tool palette) land. The source-pane module's vendor-free API makes the eventual swap an internal-only change.
- **OpenSpec capability decomposition mirrors module decomposition.** Each top-level module has its own capability spec: `app-shell`, `doc-store`, `src-pane`, `mdparse`, `scanner`, `render`, `file-io`. Seven small specs are easier to maintain and evolve than one monolithic document.

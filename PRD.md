# Armadillo Editor — Product Requirements

## One-liner

A native System 7 markdown editor for 68030-and-up classic Macintosh. Opens a `.md` file, shows a source view with syntax coloring, renders a read view via QuickDraw, and toggles between them from the View menu. Period-correct UI. No network, no cloud, no new APIs — just a clean editor that feels like it came out of 1993.

## Target audience

Retro-computing enthusiasts, vintage-hardware writers, System 7 hobbyists, and the intersection of "people who still run real Quadras" + "people who write in markdown today." Secondary: emulator users (QEMU, SheepShaver, Mini vMac in 68k mode) who want a richer editor than TeachText / SimpleText for their markdown workflow.

## Target hardware

- **Floor:** Motorola 68030 @ 16 MHz, 4 MB RAM, System 7.x. Covers the Mac II family, IIci, IIcx, IIsi, IIfx, LC III, SE/30, Classic II, PowerBook Duo, LC 475, etc.
- **Primary:** Quadra 68k (605, 610, 650, 700, 800, 840AV, 900, 950) @ 25–40 MHz, 8 MB+ RAM, System 7.1–7.6.
- **Not supported:** 68000 / 68020 hardware, System 6 or earlier, Mac OS 8+ (we may run on 8.x by accident; we won't test for it).
- **Development platform:** Retro68 cross-compiler + QEMU with Quadra 800 image.

## Core experience

The user launches Armadillo Editor, opens a `.md` file via File → Open, sees the markdown source in a window with syntax coloring, toggles to Read view via the View menu, reads a QuickDraw-rendered preview, saves, quits. No surprises, no modal anything, no network activity, no background processes. The app does markdown editing the way Mac apps did word processing in 1993.

## MVP scope — what ships first

MVP corresponds to **Tier 0 + Tier 1** of the tier roadmap below. Concretely:

- App shell: Toolbox init, event loop, menu bar (Apple / File / Edit / View), About, Quit, standard keyboard shortcuts
- Single-document model: one `.md` open at a time
- File I/O: Standard File Open / Save / Save As, reading and writing `.md` files from disk
- Source pane: editable text view with syntax coloring (headings, bold, italic, code, links, inline HTML colored distinctly); based on Mac Toolbox TextEdit internally, with `kMaxDocBytes = 32767`
- Markdown parsing: md4c (vendored), debounced at 250 ms after last keystroke
- Read pane: custom QuickDraw-driven renderer that lays out blocks and inline runs, drawing headings / paragraphs / lists / blockquotes / horizontal rules / code blocks / links
- Mode switch: View menu toggles between Source and Read. Not split — one mode at a time in MVP.
- Inline HTML: rendered as literal text in MVP (the md4c-detected HTML block/span is shown as-is, colored distinctly). The whitelist renderer comes in Tier 2.
- Error handling: period-correct Mac OS alerts for out-of-memory, file-too-big, parse failure.

## Tier roadmap — post-MVP work, in order

Each tier becomes its own OpenSpec change with its own proposal + design + tasks + specs, layered on top of the MVP's capabilities.

- **Tier 2 — Inline HTML whitelist renderer.** Walk md4c-detected HTML spans/blocks with a ~300-line purpose-built tokenizer limited to `<aside>`, `<h1–h4>`, `<p>`, `<ul>/<ol>/<li>`, `<b>/<strong>`, `<i>/<em>`, `<u>`, `<a href>`, `<code>`, `<br>`, `<hr>`, `<blockquote>`. Feeds the same block/inline layout engine built in MVP.
- **Tier 3 — Floating tool palette + status-bar mode toggle.** The 16-tool Photoshop-style floating palette from the wireframe (Text, Para, Bold, Italic, Under, Strike, H1, H2, H3, Quote, List, Num, Link, Code, HTML, Rule). Status bar at the bottom of the editor window with the icon-only Source/Read toggle replacing the View-menu toggle.
- **Tier 4 — Multi-document.** Support multiple open `.md` files. One of V4 (multi-window + top-right pull-down switcher) or V5 (View menu with Source/Read toggle + modal Open) from the wireframe, chosen at that tier's brainstorm.
- **Tier 5 — Finder-style documents window (V3).** A floating Armadillo documents window with list view, disclosure triangles, folder tree, icon view toggle. Built on a lightweight document index.
- **Tier 6 — Custom folder icon stamps.** Get Info dialog with paste-icon flow, attaching custom 1-bit folder stamps via the HFS icon resource mechanism. Wireframe shows `arma`, `star`, `clock`, `gear` stamps.
- **Post-Tier-6 — Inline images and relative path resolution.** Extend the HTML whitelist renderer (Tier 2) to support `<img src="..." alt="...">`. Extend the markdown pipeline to render native `![alt](url)` image syntax (md4c already detects this as `MD_SPAN_IMG` — it just needs a render path). Load local images from disk using **PICT** as the primary format (QuickDraw's native `DrawPicture` handles it directly); add **GIF / JPEG** support conditionally if the target has QuickTime (Gestalt-probed at startup). Resolve `src` paths on `<img>` and `href` paths on `<a>` / markdown links **relative to the open document's parent folder** using HFS path semantics (FSSpec vRefNum + parID). Absolute HFS paths (`HD:Docs:pic.pict`) also supported; `file://` / `http://` / other URL schemes remain styled but non-clickable (click-through is a separate future consideration, not in this tier). Images larger than the arena allow (worst case: a 640×480 8-bit PICT is ~300 KB) live in their own per-image `Handle`s alongside the render model, not inside the arena — the renderer's block model stores image references, not pixel data.
- **Post-Tier-6 — Custom text engine.** Replace Toolbox TextEdit with an in-house piece-table or gap-buffer engine to lift the 32,767-byte document cap and improve editor responsiveness on larger files. Internal-only change behind the stable `src_pane.h` API; every other module is untouched.

## Non-goals — explicitly out of scope, permanently or for a long time

- Network access, cloud sync, any outbound connection
- Syntax highlighting for anything other than markdown (no code-block language-aware highlighting; fenced code blocks are styled monospace only)
- Spell-check, grammar-check, collaborative editing
- PDF / HTML / Word export (Save produces bytes-for-bytes identical to what the user typed)
- Markdown extensions beyond CommonMark + the HTML whitelist (no tables, no footnotes, no task lists in MVP — may revisit in a later tier if users need them)
- Inline image rendering — neither markdown's `![alt](url)` nor HTML `<img>` are rendered in MVP or Tier 2; planned for a post-Tier-6 tier (see tier roadmap)
- Relative-path resolution for link `href` and image `src` — planned alongside image support in post-Tier-6
- Link click-through (opening a clicked link in the browser, a sibling document, or scrolling to an anchor) — links render styled in MVP and Tier 2, but clicks are inert; click-through is post-Tier-6 at earliest
- Plugins / extensibility
- Preferences dialog (MVP uses compile-time constants; preferences come if and when they're needed)
- OpenTransport / MacTCP / anything network-related
- PPC / CFM-68K / Carbon / macOS — 68k Retro68 only
- Mac OS 8.5+ MLTE — doesn't exist on our target
- Unicode beyond MacRoman (System 7's native encoding)

## Success criteria

MVP is done when, on a stock Quadra 800 running System 7.1 under QEMU:

1. The user can File → Open a `.md` file up to 32 KB and see it in the Source pane with markdown syntax coloring applied.
2. Typing in the Source pane updates the colors within ~500 ms of idle (250 ms debounce + parse time).
3. View → Read switches to the Read pane, which shows a QuickDraw-rendered preview of the markdown with headings, paragraphs, bulleted/numbered lists, blockquotes, code blocks, horizontal rules, and styled inline runs.
4. File → Save writes the Source pane's current text back to the original file byte-for-byte unchanged (modulo the user's edits).
5. Opening a file > 32 KB displays the alert "That document is larger than 32 KB." with a single OK button; the app does not crash or enter an inconsistent state.
6. Out-of-memory during parse does not crash; an alert is shown and the previously rendered content remains visible.
7. All host unit tests (arena, render, mdparse, scanner, doc, debounce, file-io) pass with `make -f Makefile.hosttests test`.
8. The on-device smoke-test `.APPL` reports PASS for the core edit/parse/render/save path when run in the Quadra 800 VM.

## Open questions

_(None at design-doc time. If additional decisions arise during implementation they are captured in the design doc update log rather than here.)_

## References

- Figma wireframes: `Armadillo Editor Wireframes.html` in the Mulita Editor handoff bundle — visual source of truth for System 7.1 UI chrome, tool palette, folder widget variations, element inventory.
- Reference project: the Retro68 hello-world project at `~/Documents/Projects/QemuMac/retro68-projects/hello-world` — pattern source for CMake setup, `mac_syscalls.h` mockable-OS-calls layer, host-side `_test.c` convention, OpenSpec directory layout.
- md4c: vendored in-tree at `third_party/md4c/`; CommonMark parser with SAX-style callbacks, caller-controlled allocation, C89, zero external dependencies.
- Retro68: cross-compiler at `~/Documents/Projects/Retro68-build/toolchain/`.

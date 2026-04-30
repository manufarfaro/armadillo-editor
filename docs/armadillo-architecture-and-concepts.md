# Armadillo Editor — Architecture and Concepts

*A guide to what we're building, why it looks the way it does, and the computing concepts that drive every decision. Written to be read end-to-end and turned into a conversation.*

---

## Prologue: A markdown editor for 1993

Armadillo Editor is a markdown editor for classic Macintosh computers. The kind of machine with a black-and-white screen, a beige box, a single-button mouse, and a memory footprint so small that you measured it in the high hundreds of kilobytes. The target floor is a Motorola 68030 processor running System 7.x — so anywhere from a 16 MHz Mac IIsi from 1990 to a 40 MHz Quadra 840AV from 1993. The upper bound is the Quadra 68k family: the last generation of 68k machines Apple shipped before the PowerPC transition.

You open the editor, it presents a source pane with your markdown, you toggle to a Read view and it lays out the document — headings, bold and italic runs, bulleted lists, blockquotes, horizontal rules, fenced code blocks — using QuickDraw, the Mac's native 1984-era drawing API. The look is pinstripe title bars, beveled buttons, 1-bit icons, no anti-aliasing. The toolbar font is Chicago; the body is Geneva; code is Monaco. If you lived through it, it looks exactly right. If you didn't, it looks like a video-game's "retro terminal" mode, except this one really does run on a 25 MHz computer with a floppy drive.

The app ships as a classic `.APPL` bundle inside a `.dsk` floppy image that you mount in QEMU, or copy to a real Quadra if you have one. It doesn't talk to the network. It doesn't sync to the cloud. It doesn't have preferences. It opens one markdown file, lets you edit it, shows you a preview, and saves it back byte-for-byte the way you typed it.

**Why build this in 2026?** Because the constraints teach you things. A modern markdown editor gets to lean on browser text layout, unbounded RAM, preemptive threads, unlimited storage, and a standard library that handles every weird Unicode corner case. Pull those away and you get to re-learn what a markdown editor actually *is* — a parser feeding a layout engine feeding a drawing API — as separate concerns that fit together a certain way because the platform forced that shape. Every decision this project makes is a tradeoff that big machines hide from you.

This document walks through those decisions. By the end you should be able to explain, without looking at the source code: what a Mac OS `Handle` is and why it mattered; what an arena allocator is and why we built our own; why SAX-style parsing is the right shape for markdown on slow hardware; what TextEdit's 32-kilobyte ceiling is and why Marco Piovanelli wrote WASTE; why `MdParseSink` fan-outs one parse to two consumers instead of parsing twice; why the Read pane never calls QuickDraw directly; and why all of this discipline ends up making the final `.APPL` easier to write, not harder.

---

## Part 1: The target platform

### 1.1 The Mac OS Memory Manager — Handles, not pointers

If you've written C on Unix or Windows, your mental model of a pointer is a number that stays put. `char* p = malloc(128)` gives you `p`, and `p` identifies the same 128 bytes for as long as the allocation lives. You pass `p` around, store it in a struct, compute `p + 5` — all of this works because the operating system does not secretly move your memory while you're looking away.

Classic Mac OS, designed for a machine with 128 KB of RAM, did not have that luxury. Apple's engineers invented a memory manager that actively moves allocations around to compact the heap. When you ask for memory you don't get a pointer — you get a `Handle`, which is a pointer to a master pointer. Diagrammatically:

```
Handle h   ─────────▶  [master pointer]  ─────────▶  [your 128 bytes]
                          (stable in the MPB)            (movable!)
```

The master pointer lives in a stable region called the master pointer block. The actual bytes it points at can move, and the Memory Manager will silently update the master pointer when it does. So `*h` always works — you dereference the handle to get the current location of your memory. But if you grab a raw pointer `char* p = *h` and then make any Toolbox call that might trigger heap compaction, `p` becomes garbage. Dereference it and you corrupt some other object's storage.

To stop the Memory Manager from moving a block, you call `HLock(h)`. The block is pinned until you call `HUnlock(h)`. While locked, `*h` is stable and you can hold raw pointers derived from it. The cost is that locked blocks fragment the heap — so the discipline in classic Mac programming was: lock as late as possible, unlock as early as possible, and never hold a raw pointer across a Toolbox call unless you know it's locked.

There's another operation that can move a block: `SetHandleSize(h, newSize)`. If the existing bytes can't be grown in place, the Memory Manager allocates a new block elsewhere, copies the contents over, updates the master pointer, and returns. From your perspective, `h` is the same handle, but `*h` is now a different address. Every raw pointer you derived from the old address is now dangling.

This is not theoretical. Every classic Mac C library that tries to pretend `malloc`/`free` works the same way eventually breaks, because the Memory Manager's movability is a feature of the platform, not an accident. Armadillo Editor takes this seriously: it never pretends the Memory Manager is `malloc`. It embraces the Handle, locks it once for the lifetime of a useful region of memory, and builds a custom allocator on top. That allocator is the arena, and we'll come back to it.

### 1.2 The Toolbox — a 1984 API that still runs

The Mac Toolbox is the name for the collection of APIs that make up the user-land side of classic Mac OS. QuickDraw handles drawing. TextEdit handles editable text fields. The Window Manager handles windows. The Menu Manager handles menus. Standard File handles open/save dialogs. The File Manager handles HFS I/O. The Gestalt manager handles "does this system have feature X?" queries. Each is a set of procedural functions you call, grouped by prefix.

Toolbox APIs are strongly procedural and handle-based. You don't construct a `Window` object with inheritance; you call `NewWindow(...)` and get back a `WindowPtr` — which is really a pointer to a `WindowRecord` that you treat as opaque. You don't configure it by setting properties; you call `SetWTitle(window, ...)`, `MoveWindow(...)`, `SizeWindow(...)`. The Toolbox was designed when "object-oriented" was a debate, not a consensus, and it shows.

This shape turns out to be extremely mockable. A QuickDraw call like `DrawString("Hello", 5)` is a function call with specific arguments — it doesn't invoke virtual dispatch on a `Window` instance. If you want to test code that calls QuickDraw without actually having QuickDraw, you wrap each QuickDraw function behind your own vtable of function pointers, and inject a fake. That's exactly what Armadillo does, and we'll return to it when we talk about the three test seams.

### 1.3 Retro68 — cross-compiling C to 68k in 2026

Apple stopped shipping 68k Macs in 1996. GCC has long since dropped 68k support from its default distribution. So how do you build a classic Mac app today?

Retro68 is a project by Wolfgang Thaller (GitHub user `autc04`) that packages a modern GCC cross-toolchain targeting `m68k-apple-macos`. You install it once, and now `cmake` with the right toolchain file builds a `.APPL` that runs on a real Quadra or under QEMU. The toolchain includes a Rez-compatible resource compiler, linker scripts that know how to split your binary into classic Mac code/data/resource sections, and the `add_application()` CMake function that packages everything into a double-clickable `.APPL`.

It's a striking piece of work. You write standard C89, compile it with a modern GCC, and get a binary that runs on a 25 MHz processor from 1992. Even better, Retro68 now ships an official Docker image at `ghcr.io/autc04/retro68` that's rebuilt on every commit. We use that image in our GitHub Actions CI — the cross-build runs inside the container, produces `.APPL` and `.dsk` artifacts, uploads them to the workflow, and on tagged commits attaches them to a GitHub Release. No Retro68 install needed in CI; the container has it all.

---

## Part 2: Core architectural decisions

### 2.1 The arena allocator

An **arena allocator** (also called a region or pool allocator) is a memory-management pattern where you claim a chunk of memory up front and sub-allocate out of it with a bump-pointer. Allocations are O(1) and tiny — you just advance a cursor. Deallocation is O(1) for the whole pool — you reset the cursor. Individual sub-allocations cannot be freed independently; they all live until the pool is reset or destroyed.

This pattern has two powerful properties. First, it's faster than general-purpose `malloc`/`free` by orders of magnitude, because there's no free-list management, no fragmentation, no coalescing. Second, it completely eliminates a whole class of memory bugs: use-after-free, double-free, leak-on-error-path. If everything in a logical unit of work (a parse cycle, a request handler, a frame) lives in the arena, you can't leak it — the whole pool goes back to zero between units.

Armadillo's render pipeline is a textbook arena use case. Each time the user stops typing, we re-parse the markdown, which produces a flat list of blocks, each with its text and style runs. When the user types again and the debounce fires, we do it all over. We *never* need to free an individual block or style run — the whole model is replaced atomically. So: one arena, everything in it, reset between parses, freed once on doc close. The implementation is about 100 lines of C.

But this is where the Mac OS Memory Manager story comes back. If we used standard `malloc` for the arena's backing memory, we'd be fine — `malloc`-ed memory doesn't move. But the design for classic Mac says to use the Memory Manager, which means our arena is backed by a `Handle`. And we established in Part 1 that the Memory Manager can move Handle-backed memory during `SetHandleSize`.

So the arena does something careful: at init, it allocates a Handle and immediately calls `HLock` on it, pinning the backing memory. `HLock` is kept active for the arena's entire lifetime. As long as we don't shrink or grow the Handle, the bytes don't move, and we can hand out raw pointers to sub-allocations that stay valid.

When the arena needs to grow — we're about to parse a larger document than the current capacity allows — we do it deliberately, at a precise moment, in a specific sequence:

1. `HUnlock` the backing Handle.
2. `SetHandleSize` to the new capacity.
3. `HLock` again.
4. Re-read `*backing` — this might be a different address now.
5. Update the arena's internal `base` pointer.

Any raw pointer we held before step 2 is potentially invalid after step 4. So the arena enforces a rule: **we only grow before a parse, never during one**. Callers are required to call `arena_ensure(a, estimate)` to pre-size the arena before starting to allocate. `arena_alloc` itself never triggers a grow; if a bump would exceed capacity, it returns `NULL` and the whole parse cycle fails cleanly. The previous (successful) model stays live on screen until the next parse succeeds.

This is the concrete answer to "how do you use a C allocator pattern on a platform whose Memory Manager moves things around?" You accept the movability as a first-class concern, confine it to one tightly-controlled moment, and make the policy load-bearing in your API. The test suite has a case that mocks `SetHandleSize` to fail on the first call, then verifies that the arena's state is untouched — no corrupted high-water mark, no half-updated base pointer. That test exists because this is exactly the kind of bug that eats classic-Mac programmers.

A few other details that matter:

- **Four-byte alignment.** The 68020 and later processors (which includes 68030, our floor) take a bus error if you try a 32-bit longword load or store on an odd address. So `arena_alloc` always rounds the requested size up to the next multiple of 4. A 5-byte allocation consumes 8 bytes of the arena. The cost is tiny (at most 3 bytes per allocation); the alternative is bus errors that are miserable to debug.

- **Growth policy.** The arena doubles its capacity up to 64 KB, then grows in 32-KB linear steps, with a hard cap of 512 KB. The doubling keeps amortized allocation cheap for small documents; the linear step keeps the memory cost predictable for large ones. The hard cap prevents a pathological document from eating the entire system heap.

- **Reset, not free.** Between parses, we call `arena_reset` which sets the high-water mark back to zero. The Handle is NOT released — its backing memory stays with us, and the next parse reuses it. This means ten consecutive parse cycles trigger exactly one `NewHandle` (the initial one) and zero additional ones. On a 25 MHz processor where every Memory Manager call has measurable cost, this matters.

### 2.2 Vendor-free public interfaces

Here's a rule that runs through every module boundary in the codebase: **no vendor or Toolbox types cross a public header**. No `TEHandle`, no `WindowPtr`, no `FSSpec`, no `RGBColor`, no `Handle`, no md4c types. Opaque pointers and project-owned types only.

The reason is swappability. Every major implementation choice in this project is going to change at some point, and we want the change to be local. Today, the source pane is backed by TextEdit, which has a 32-kilobyte document ceiling. Tomorrow, we'll replace TextEdit with a custom piece-table engine that lifts that ceiling. The day that happens, we want the other eleven modules to not notice. The way you achieve that is by hiding TextEdit entirely behind an opaque `SrcPane*` handle, with a vendor-free API: `src_pane_set_text`, `src_pane_apply_runs`, `src_pane_get_selection`. No caller ever sees a `TEHandle`.

The same principle applies to md4c. It's the markdown parser we vendored, and it's a good parser, but we might replace it. So the entire md4c API is hidden behind `mdparse/mdparse.h`, which exposes a project-owned `MdParseSink` callback struct and never mentions `MD_BLOCKTYPE`, `MD_SPANTYPE`, or any other `MD_`-prefixed name. Callers see our types; md4c's existence is an implementation detail.

This rule looks pedantic until you watch what happens when it's violated. A year later, someone tries to replace the markdown parser, and finds that fifteen files across the codebase include `md4c.h` directly. Each one has to change. Some of them have to change in subtle ways (they were relying on an md4c enum ordering). Suddenly "swap out the parser" becomes a month of careful refactoring instead of a week of writing the new adapter.

The mechanical enforcement is simple: you can include vendor headers from inside a module's `.c` file, but never from its public `.h`. Where a vendor type needs to cross the boundary — for example, Standard File returns an `FSSpec` that the `file_io` module needs to hand up to the app layer — we pass it as a `const void*` with a documented "cast to `FSSpec*` inside the implementation" contract. Ugly, but confined.

### 2.3 TextEdit vs WASTE — the 32-kilobyte problem

TextEdit, the Toolbox's built-in text component, was designed in 1984 for the original 128-kilobyte Mac. Its internal structures — selection range, character count, line-starts array — all use `signed short`, a 16-bit integer. The maximum value of a signed short is 32767. So a TextEdit record can hold at most 32,767 bytes of text. Not 32 megabytes, 32 kilobytes.

For what TextEdit was designed to be — the text field in a dialog box, the About box, the text inside a small form — 32K was plenty. But it means TextEdit cannot, by construction, hold a long document. Apple never retrofitted TextEdit to use 32-bit offsets because, in parallel, every Mac app that cared about long-form text (MacWrite, WriteNow, Word, Nisus) shipped its own text engine and TextEdit stayed in its dialog-box lane.

The most famous third-party text engine for classic Mac was WASTE, written by Marco Piovanelli starting in 1993. The name is a backronym-ish joke — WASTE officially stands for "WorldScript-Aware Styled Text Engine" but it was originally pitched as "Worry About Stuff Edit." It was designed as a near-drop-in replacement for TextEdit: similar API shape, similar event-handling model, but with 32-bit offsets and thus unlimited document size, plus free undo, better style-run handling, drag-and-drop text, and WorldScript (bidirectional and multi-byte character) support. It was used by BBEdit, Nisus Writer, SimpleText, and a dozen other shipping apps.

Piovanelli's site (`merzwaren.com`) went dark in 2006. The last released version is WASTE 3.0.1 SDK from December that year. The code is archived on Macintosh Garden. The license is not OSI-conventional open source — source is available, and use is free for freeware applications and shareware priced up to $10 per seat, but commercial distribution beyond that requires a license from Piovanelli, who may not be reachable anymore.

So for Armadillo, we had a three-way choice for the MVP:

1. **Use TextEdit and accept the 32-KB ceiling.** Simple, zero-dependency. But most serious markdown documents will bump against it within a few sessions. A single README for a non-trivial project often exceeds 32K; a book chapter certainly does.

2. **Use WASTE.** Solves the size problem. But introduces a vendored dependency with a non-standard license, a dead upstream, and source we'd own fully ourselves if anything broke.

3. **Write our own text engine from day one.** Most work up front, but we own everything and can license the whole project cleanly. The common implementation for a serious text engine is either a *gap buffer* (the approach used by Emacs and most IDE editors) or a *piece table* (the approach used by Microsoft Word and VS Code). Both provide 32-bit offsets by default and efficient edit operations.

We chose option 1 for MVP. The reasoning: ship something working fast, accept that the first public version is positioned as "for notes and short documents," and then replace TextEdit with our own piece-table engine in a post-MVP release. The key move that makes this work is the vendor-free interface rule — `src_pane/src_pane.h` exposes an opaque `SrcPane*` that says nothing about TextEdit, so when we swap in the custom engine, no other module changes. We also track the decision cleanly in the PRD's tier roadmap: WASTE was evaluated and specifically rejected because of its license ambiguity and dead upstream.

If you want the canonical summary: **TextEdit is a 40-year-old text widget designed for dialog boxes; WASTE is a 30-year-old replacement designed for serious text apps with a license that's fine for hobbies but messy for commercial use; custom engine is what serious markdown apps in 2026 write**. Armadillo uses TextEdit today and will write its own piece table tomorrow.

### 2.4 Markdown parsing with md4c

A markdown parser has two broad shapes. A **DOM-style parser** reads the entire document and builds a tree of block and inline nodes in memory; consumers walk the tree. A **SAX-style parser** (named after the Simple API for XML) reads the document and fires a callback for each syntactic event — "block open heading level 1," "text 'Hello'," "block close heading" — as it goes; consumers respond to events.

DOM parsers are convenient for small documents on modern hardware. You get a full tree, you can traverse it multiple times, you can query and transform. But the memory cost is O(document size), and every node is typically a separate heap allocation, and on a 68030 with 4–8 MB of RAM the allocation churn alone is painful.

SAX parsers have the opposite properties. The parser itself holds almost no state; the caller decides what to remember. Memory usage is whatever the caller chooses. Multiple consumers can each tap the same event stream and build completely different internal representations. On a slow machine, the ability to make one parse pass drive multiple consumers is a direct performance win.

Martin "mity" Mitáš's md4c is a SAX-style CommonMark parser written in pure C89. The whole thing is about 6,500 lines in a single `md4c.c` file plus an `md4c.h` header. It has zero external dependencies — not even libc functions for allocation. It does not allocate anything itself except tiny stack-local state; every byte of memory it touches is in your caller-provided buffer. This was designed to run on microcontrollers and retro systems, and it does.

We vendored md4c at release 0.5.2, pinned by commit SHA in `third_party/md4c/COMMIT.txt`. Upgrading is a deliberate act: you fetch the new source, drop it in, record the new SHA, run all the tests, commit with a message explaining why. No auto-updates, no submodule indirection.

Our adapter module, `mdparse`, wraps `md_parse` with three responsibilities:

- **Type isolation.** Nothing outside `mdparse` ever sees an md4c type. The adapter translates md4c's enum values into our `BlockKind` and `StyleKind`.

- **Depth tracking.** md4c emits `MD_BLOCK_UL` and `MD_BLOCK_OL` events for list containers. Our adapter converts these into a depth counter rather than a user-facing event, and stamps the current depth onto contained list-item blocks. Same for `MD_BLOCK_QUOTE` and blockquote depth. This is what lets downstream modules treat nesting as a scalar field on each block instead of reconstructing it from a hierarchy of events.

- **Fan-out.** The adapter's `mdparse_run` function accepts an array of sinks and dispatches every event to every sink in array order. Scanner and render both plug in as separate sinks. One parse, two outputs.

### 2.5 Why we're writing our own HTML renderer

Markdown's spec says that inside a markdown document, raw HTML is legal. You can write `<aside>Important note</aside>` inline and it passes through to the rendered output. Armadillo's Read pane needs to render that HTML — at least a whitelisted subset of it — in period-appropriate QuickDraw.

The natural move is to grab an off-the-shelf HTML parser. The landscape isn't bad. Google's gumbo-parser is a full HTML5-spec-compliant C parser. Lexbor and Modest are modern alternatives. NetSurf's libhubbub was designed for small 32-bit ARM machines, so it's as close as you get to retro-friendly. All of them work; some are quite good.

We chose to write our own anyway, for three reasons. First, these parsers are big. Gumbo alone is around 500 KB of C code. Our *entire* MVP is targeting maybe 200 KB of code in the `.APPL`. Pulling in a parser that's twice the size of everything else is the tail wagging the dog.

Second, they assume `malloc` the way every modern C library assumes `malloc`. Freely, frequently, without consulting the caller. On classic Mac, where memory is Handle-managed and callable-from-interrupt is a real concern, every malloc becomes a small integration headache. You can work around it, but the ceremony adds up.

Third — and this is the insight that pushed us over the line — md4c already does the hard part. When md4c sees `<aside>Important note</aside>` inside a markdown document, it doesn't try to parse the HTML. It recognizes that it's HTML, emits an `MD_BLOCK_HTML` event (or `MD_SPAN_HTML` for an inline span), and hands us the raw bytes. The parser's authors explicitly decided not to take on HTML because the HTML5 spec is enormous and error-tolerance is a saga unto itself — so they delimit the HTML chunk and let the caller decide what to do.

For Armadillo's Read pane, the natural thing to do is walk the already-delimited HTML chunk with a small, purpose-built tokenizer that only understands about 15 tags: `<aside>`, `<h1>` through `<h4>`, `<p>`, `<ul>`/`<ol>`/`<li>`, `<b>` and `<strong>`, `<i>` and `<em>`, `<u>`, `<a href>`, `<code>`, `<br>`, `<hr>`, `<blockquote>`. Anything outside the whitelist renders as literal text. This is maybe 300 lines of C, fits entirely in our arena, and emits the same block and inline events as the markdown side — so the layout engine treats them uniformly.

The whitelist parser lands in Tier 2 (the second post-MVP release). The MVP itself just renders HTML blocks as raw monospace purple text — recognizable as HTML, but not structurally interpreted. Good enough to ship.

### 2.6 The flat block model

When you read the output of a markdown parser, it's tempting to imagine a tree: a Document node with children that are Block nodes, a Blockquote block with a ListItem child that contains a Paragraph, and so on. This is what DOM parsers produce and what most textbooks would have you build.

We don't. Armadillo's `RenderModel` is a flat array of `Block` structs, and nesting is expressed via scalar fields — `list_depth` and `quote_depth` — on each block. A nested list like this:

```markdown
- element
  - sub element
```

becomes:

```
blocks[0] = { kind=kBlockListItem, list_depth=1, text="element" }
blocks[1] = { kind=kBlockListItem, list_depth=2, text="sub element" }
```

The layout pass walks the array and indents each block by `list_depth * indent_per_level + quote_depth * quote_indent`. The bullet glyph can vary by depth — `•` for 1, `◦` for 2, `▪` for 3 — or stay the same. No tree traversal, no recursive allocation, no parent/child pointers to chase.

This isn't a novelty. Rich-text formats that render to flow text have been doing this for decades. Microsoft Word's DOCX stores paragraphs in a flat sequence with a "list level" attribute on each. Apple Pages does the same. HTML, when rendered as flow text in a browser, is essentially flattened into a linear stream of boxes during layout — the DOM tree is an editing convenience, not a rendering necessity.

The reason flat works is that for rendering-as-linear-flow, hierarchical containment and depth-stamping are *equivalent representations*. They describe the same visual output; the only difference is which algorithm you use to produce that output. A tree traversal recurses; a flat walk iterates. On a 25 MHz machine with a tiny stack, the flat walk wins by a lot.

We do give up something. Structural edits — "move this whole sublist from here to there" — are harder in the flat model. But we never perform structural edits on the render model anyway; the source-of-truth is the user's source text, and when it changes we regenerate the model from scratch. The tree's main benefit doesn't apply to us.

The flat model also plays nicely with the arena. Every block is a fixed-size struct (32 bytes); text slices and style-run arrays are arena-allocated alongside; the whole model is a chunk of contiguous memory that we replace atomically between parses. No pointer chasing, no indirection cost.

### 2.7 Single parse, two outputs — the debounce fan-out

Here's the render pipeline's overall shape.

The user types. Each keystroke fires an edit event, which records "dirty since last parse" and captures the current tick count. The main event loop polls every iteration: if the document is dirty and at least 250 milliseconds have elapsed since the last keystroke, run a parse cycle. If the user is still typing, the 250ms debounce window resets with each keystroke and parsing is deferred.

When the parse cycle fires, it does one md4c call. That call's callbacks dispatch through our `mdparse` adapter to every registered sink. We have two sinks:

- The **scanner** accumulates style runs — tuples of `(start, length, kind)` — for the source pane's syntax coloring. The source pane is what the user sees while typing; we want it to look styled (bold text in teal, italic in salmon, links in blue-underline) like the wireframe shows.

- The **render model builder** consumes the full event stream and produces the flat block array described above. This feeds the Read pane.

One parse, two outputs. If we called `mdparse_run` twice — once for the scanner, once for the render model — we'd pay the parsing cost twice, and on a 25 MHz processor parsing even a small document is measurable. The fan-out pattern lets us amortize the cost across every consumer that needs events.

The 250ms debounce matters for a different reason: classic Mac OS is **cooperative multitasking**, not preemptive. Your application is the only thread running until you voluntarily yield. A synchronous parse that takes 100–400 ms blocks the entire UI — the cursor stops blinking, click events queue up, the menu bar won't respond. Debouncing means the user is never typing while we're parsing, by construction. The parse only fires after the user has been idle for a quarter-second.

This works because markdown parsing is fast enough that a 400-ms worst case is acceptable when it happens while the user isn't typing. If it weren't — if parsing took seconds — we'd need to move it to a secondary thread via System 7.5's Thread Manager, and that changes the entire synchronization story. We might eventually need that for very large documents (which our custom text engine will enable). For MVP, synchronous-with-debounce is the simplest correct choice.

---

## Part 3: Testing, tooling, and discipline

### 3.1 Three test seams

The hardest question when testing C code that talks to an operating system is: how do you run the tests without the operating system? For us, the answer is to isolate every OS interaction behind an indirection layer, and inject fakes at the seam.

Armadillo has three such seams.

**`MacSyscalls`** is a struct of function pointers that covers every Toolbox call we use. `tick_count`, `new_handle`, `hlock`, `set_handle_size`, `fsp_open_df`, `fs_read`, `gestalt`, `note_alert`, and so on. Every module that touches the OS takes a `const MacSyscalls*` parameter and dispatches through it rather than calling the Toolbox directly. Production wires the vtable to real Toolbox functions (`tick_count = real_TickCount_wrapper`, etc.); host tests wire it to `FakeSyscalls`, which implements each function over `malloc`-backed fakes that record their call counts and respect their fail-after knobs.

**`DrawOps`** is the same idea, applied to the Read pane's drawing. The renderer never calls `DrawString` or `MoveTo` or `LineTo` directly. It calls through a vtable: `ops->draw_text(ctx, bytes, length)`, `ops->move_to(ctx, h, v)`, `ops->line(ctx, x0, y0, x1, y1)`. Production binds this vtable to a wrapper around real QuickDraw. Host tests bind it to a `Recorder` — a struct that records every call into a growable array — and the test then asserts on the recorded call sequence. "Did the renderer call `set_font(Chicago, 17, bold)` before drawing the H1?" Yes — the recorder has it logged.

**`MdParseSink`** is the fan-out interface we already discussed, which doubles as a test seam. Scanner and render plug into it in production, but tests can plug a recording sink directly in and feed the scanner or render synthetic event streams without running md4c at all. This isolates the renderer from md4c bugs and makes render tests fast.

The pattern is always the same: put a layer of function pointers between "caller" and "thing we can't easily test." In production the layer costs one indirection per call, which is negligible. In tests it's everything.

The resulting test suite runs in under two seconds for the full build. Every host-testable module has colocated `_test.c` files and we use Unity as the assertion framework. The CI pipeline runs them on every push. It's a tight feedback loop, and it exists only because the three seams are there.

### 3.2 Tests as documentation

We write tests that read like prose. Test function names are sentence-long — `test_arena_init_allocates_and_hlocks_backing_handle_and_reports_initial_capacity`, not `test_init` — and each test starts with a comment block that explains what concept is being tested, why it matters, and where the relevant spec lives.

```c
/* arena_init: allocation + HLock
 * ───────────────────────────────
 * arena_init SHALL allocate a Handle of the requested size via
 * MacSyscalls.new_handle and HLock it. Post-conditions: the arena
 * returned is non-NULL; new_handle was called exactly once; hlock
 * was called exactly once; the arena reports the initial capacity. */
void test_arena_init_allocates_and_hlocks_backing_handle(void) { ... }
```

This does two things. First, it makes the tests executable specifications — a new contributor can read the `_test.c` file top to bottom and learn both what the module does and how classic Mac OS concepts like Handles work. Second, it forces you to know *why* each test exists. If you can't explain the test's concept in a comment, you're probably duplicating coverage.

The convention came from the reference project `hello-world` at `~/Documents/Projects/QemuMac/retro68-projects/hello-world`, which uses the same approach to document MacTCP's quirks for readers who aren't familiar with them. We carry the idea forward.

### 3.3 C89 discipline

Retro68's GCC can compile modern C, but we stay in C89. The reason is portability and clarity, not limitation.

C89 is what the Mac Toolbox headers expect. The Toolbox was designed in the era when "C" meant K&R C, then ANSI C89. The type names, struct layouts, and `pragma pack` conventions are all predicated on C89 semantics. Straying into C99 or C11 features risks subtle compatibility problems that you might not catch until the app crashes on a specific Mac model.

The practical rules we follow:

- **Declare locals at the top of a block**, never after an executable statement. If you need a scoped variable mid-function, introduce a new brace-bounded block.

- **No `//` comments**. `/* ... */` everywhere. It reads fine once you're used to it.

- **No designated initializers** like `(struct Foo){.bar = 1}`. Write out the full struct assignment.

- **No `inline`, no `bool`, no `stdbool.h`**. Use `int` for booleans with 0 and 1.

- **No variable-length arrays**. If size varies, allocate from the arena.

The cost of this is mild. Modern eyes notice the absence of `//` for a day and then forget. The benefit is that your code is compilable with every C compiler ever shipped for the platform, and the data layouts are straightforward enough that you can reason about them byte-by-byte when you need to. On a 68030 where the debugger isn't as friendly as on Linux, byte-by-byte reasoning is sometimes all you have.

### 3.4 OpenSpec methodology

OpenSpec is a way of organizing a project's specification-and-change-proposal paperwork. It's not a tool so much as a directory structure and a convention:

```
openspec/
├── changes/
│   └── <change-id>/
│       ├── proposal.md   # what this change does and why
│       ├── design.md     # how we'll do it
│       ├── tasks.md      # ordered task list for TDD execution
│       └── specs/<capability>/spec.md   # delta spec for this change
└── specs/<capability>/spec.md   # authoritative spec (grows across changes)
```

Each "capability" is a logical unit of functionality — for us, things like `app-shell`, `render`, `mdparse`, `scanner`, `doc-store`, `file-io`, `src-pane`. Each has an authoritative spec at `openspec/specs/<capability>/spec.md` that states its purpose, requirements, and Given/When/Then scenarios. When a change wants to add or modify requirements in a capability, it writes a "delta spec" under its change directory and eventually merges those deltas into the authoritative spec.

The benefit is auditability. Six months from now, someone asks "why does the arena double up to 64 KB and then grow linearly?" The answer is in `openspec/specs/render/spec.md` under the "Arena allocator" requirement, with its scenario showing exactly what's being tested, plus `openspec/changes/add-md-editor-mvp/design.md` §4 for the rationale. You never have to reconstruct the reasoning from commit messages.

The overhead is real — you write significantly more prose than code for a project at this stage. But the prose is doing work: it's what the subagents are reading, what the tests are targeting, what the reviewers are checking against. The same words that explain the design to a human are the ones the implementer follows and the reviewer audits.

### 3.5 TDD with subagent-driven development

The whole project is being implemented by Claude subagents following a detailed plan, with a human auditing each phase. Each phase of work gets three subagent dispatches:

1. **Implementer**: reads the plan's task descriptions and writes the code, runs the tests, commits each task separately, self-reviews before reporting.
2. **Spec reviewer**: given the plan's requirements and the implementer's diff, verifies by reading the code that the implementation matches the spec line-by-line. Catches "we did less than asked" and "we did more than asked" equally.
3. **Code quality reviewer**: given the diff, looks for cleanliness, testability, defensive coding, consistency with project conventions. Flags issues by severity: Critical / Important / Minor.

The loop iterates until both reviewers approve. Only then does the phase count as done and the next phase start. The overhead is significant — three subagent invocations per phase instead of one — but the quality gate is structural. You cannot ship a phase where the spec reviewer found uncaught missing requirements or the code quality reviewer flagged a Critical issue.

Why does this work better than "write it and review once at the end"? Two reasons. First, context windows are finite. A human or AI reviewer asked to assess fifty phases of work simultaneously will skim. Asked to assess one phase that's a few hundred lines, they'll read carefully. Second, the spec/code-quality split is important: the two checks use different tools. Spec compliance is about completeness and correctness against the requirements document. Code quality is about long-term maintainability. Mixing them muddles both.

---

## Part 4: Deployment and distribution

### 4.1 GitHub Actions with the official Retro68 Docker image

Cross-compiling to 68k used to be a genuine operational chore. You had to install Retro68, which involved a 20-minute build from source, get the toolchain file path right, and keep the install current. In CI, you'd repeat all of that on every runner.

As of 2023 or so, Retro68's CI publishes an official Docker image at `ghcr.io/autc04/retro68` that's rebuilt whenever Retro68's main branch gets a commit. The image has the toolchain pre-installed at `/Retro68-build/toolchain/...`. Using it from a GitHub Actions workflow is three lines:

```yaml
jobs:
  cross-build:
    runs-on: ubuntu-latest
    container:
      image: ghcr.io/autc04/retro68:latest
    steps:
      - uses: actions/checkout@v4
      - run: |
          mkdir build && cd build
          cmake .. -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
          make
```

That's it. The runner pulls the image, clones the repo, builds. The full CI pipeline for Armadillo has four workflows:

- `host-tests.yml`: runs our Unity-based unit tests on Ubuntu. No Retro68 needed. Sub-minute.
- `lint.yml`: `cppcheck` and `clang-format` checks on the project sources. Sub-minute.
- `codeql.yml`: GitHub's CodeQL static analyzer for C, on the host build. About three minutes.
- `release.yml`: cross-builds inside the Retro68 container, uploads `.APPL` + `.dsk` as workflow artifacts. On tag pushes, additionally creates a GitHub Release with those files attached.

Every push to `main` runs all four. Tag pushes matching `v*`, `mvp-*`, or `tier-*` trigger the release flow. This is how we go from "I committed" to "there's a downloadable `.dsk` you can mount in QEMU" without any manual steps.

### 4.2 The .APPL and .dsk duo

A classic Mac application has a specific binary layout that's worth understanding if you come from Unix. The executable code and data live in something called the **resource fork** — a structured collection of typed records (code blocks, menus, strings, icons, alerts) accessed via `ResLoad` and friends. The data fork of the file is where traditional Unix-style content would live, but Mac apps typically have empty data forks because everything is in the resource fork.

When you copy a Mac app over a file system that doesn't support resource forks — like HFS+ → ext4 over a USB stick — the resource fork can get lost. To prevent that, classic Mac builds emit two sidecar files alongside the `.APPL`:

- `ArmadilloEditor.APPL` (data fork only, often zero bytes)
- `%ArmadilloEditor.ad` (the AppleDouble sidecar, which encodes the resource fork in a portable format)
- `ArmadilloEditor.bin` (MacBinary: a single binary blob that merges data fork + resource fork + Finder info into one file you can download)

The `.dsk` file is a separate thing: it's a raw disk image of an 800-KB floppy, which is what you mount in QEMU. The floppy contains the `.APPL` as a classic Mac file with its resource fork intact — no sidecars needed inside the floppy image.

So when our CI produces build artifacts, it uploads both the sidecar set (for downloaders who want to manipulate the app on a Unix host) and the `.dsk` (for anyone who wants to just drop it into QEMU and run). Both flows work; both are necessary.

---

## Part 5: The roadmap

### 5.1 Why the MVP is Tier 0 plus Tier 1

"MVP" is an overloaded term. For us, it means the smallest version of Armadillo that we'd be willing to release to a user who found the GitHub repo. Not a demo, not a skeleton — an editor you'd actually use.

We split the MVP into two tiers of work. **Tier 0** is the app shell: Toolbox initialization, event loop, menu bar, About box, Quit, error dialogs. Infrastructure that every Mac app needs and that doesn't depend on markdown at all. **Tier 1** is single-document editing: File→Open, File→Save, Source pane with TextEdit, the full markdown parsing pipeline, the Read pane with the flat-block layout engine, and a View menu item that toggles between Source and Read.

Everything else — the floating tool palette from the wireframe, the Finder-style documents window, multi-document support, custom folder icons, the custom text engine that lifts the 32-KB ceiling — is a post-MVP tier. We listed them explicitly in the PRD so scope was agreed up front, and each becomes its own OpenSpec change when we get there.

The split matters because it's a productivity tool. An MVP that only had Tier 0 (a running app that doesn't edit anything) isn't useful. An MVP that had Tier 0 + Tier 1 + Tier 2 (inline HTML whitelist) would be nicer to look at but would double the surface area of the first release and push the ship date out by weeks. Tier 0 + Tier 1 is the sweet spot where you have a working editor and nothing more. Every future tier adds a specific capability, builds on what's there, and can be released independently.

### 5.2 The tiers themselves

The post-MVP roadmap is, in rough order:

- **Tier 2: Inline HTML whitelist renderer.** The ~300-line hand-written tokenizer described earlier. Takes md4c's HTML handoff and renders it structurally — `<aside>` as a purple-bordered box, `<h1>` as a big Chicago heading, `<a>` as underlined sky-blue, etc.

- **Tier 3: Floating tool palette and status-bar mode toggle.** The 16-tool Photoshop-style floating palette from the wireframes (Text, Para, Bold, Italic, Under, Strike, H1, H2, H3, Quote, List, Num, Link, Code, HTML, Rule). Status bar at the bottom with the icon-only Source/Read toggle replacing the View-menu item.

- **Tier 4: Multi-document.** Open multiple `.md` files at once, with either a top-right pull-down switcher or a View-menu list of open documents. We'll pick one at the tier's brainstorming.

- **Tier 5: Finder-style documents window.** A floating Armadillo window that lists the documents in a user-chosen folder, with disclosure triangles to navigate into subfolders. List view and icon view toggles.

- **Tier 6: Custom folder icon stamps.** The Get Info dialog with a paste-icon affordance, letting users attach small 1-bit icon "stamps" to their folders. This uses classic Mac's HFS icon-resource mechanism.

- **Post-Tier-6: Inline images and relative paths.** Support `<img src="...">` in the HTML whitelist and markdown's native `![alt](url)` image syntax. Load PICT files from disk; conditionally support GIF/JPEG via QuickTime if it's installed. Resolve relative paths against the document's parent folder.

- **Post-Tier-6: Custom text engine.** Replace TextEdit internals with a piece-table engine that uses 32-bit offsets, lifting the 32-KB document limit. Internal-only change behind the stable `src_pane.h` API — every other module is untouched.

Each of these is its own OpenSpec change. The roadmap is a commitment to the progression, not a deadline.

---

## Part 6: Key concepts glossary

**Handle** — On classic Mac OS, a pointer to a master pointer. The master pointer lives in a stable region; the bytes it points at can be moved by the Memory Manager during heap compaction. To access the bytes safely you dereference `*handle` each time, or call `HLock` to pin the block.

**HLock / HUnlock** — Mac OS Memory Manager calls that pin and release a Handle's backing memory. While locked, `*handle` is stable; while unlocked, the Memory Manager is free to move it.

**SetHandleSize** — Resizes a Handle's backing memory. If the new size doesn't fit in the current location, the Memory Manager relocates the block, which means any raw pointer derived from `*handle` before the call is invalid afterward.

**QuickDraw** — The Mac Toolbox's drawing API. Procedural, coordinate-based, no state machine beyond a current pen and font. `MoveTo`, `LineTo`, `DrawString`, `FrameRect`. All our Read-pane output lands here.

**TextEdit** — The Mac Toolbox's built-in editable text widget, designed in 1984. 32,767-byte maximum per record due to 16-bit signed-short offsets. Fine for dialog boxes, insufficient for real documents.

**WASTE** — "WorldScript-Aware Styled Text Engine," by Marco Piovanelli. A TextEdit replacement with 32-bit offsets, free undo, better style runs, WorldScript support. Source-available freeware; dead upstream since ~2006; last hosted at `merzwaren.com`, now archived on Macintosh Garden.

**SAX parsing** — Event-driven parsing style where the parser fires callbacks for each syntactic event. Named after the Simple API for XML. Contrasts with DOM parsing, which builds a full tree in memory.

**Arena allocator** — Memory-management pattern where you allocate one chunk up front and sub-allocate by bumping a cursor. No per-object free; the whole pool is reset or destroyed together. Fast, simple, leak-proof by construction.

**md4c** — Martin "mity" Mitáš's CommonMark parser. SAX-style, C89, single file, zero dependencies, caller-controlled memory. The markdown parser we vendored.

**Retro68** — Wolfgang "autc04" Thaller's GCC cross-toolchain for classic Macintosh. Packages GCC, the Rez resource compiler, linker scripts, and CMake integration. Distributed as source and as an official `ghcr.io/autc04/retro68` Docker image.

**System 7** — Apple's operating system generation 1991–1997 for 68k Macs. System 7.1 and 7.5 are the most common versions on Quadra-class hardware. We target 7.x without depending on specific 7.5+ features for MVP.

**68030 vs 68040** — Motorola 68k-family processors. 68030 is the project's floor (16–40 MHz, 32-bit clean, no integrated FPU on most models). 68040 is Quadra's typical processor (25–40 MHz). No 68040-only instructions in our code.

**Quadra** — Apple's 68040-based Mac line 1991–1995. Primary performance target for this project. Quadra 800 is the canonical QEMU target.

**Multiversal Interfaces** — A community-maintained version of Apple's Universal Interfaces (the C headers for the Toolbox) that ships with Retro68. The official `ghcr.io/autc04/retro68` container uses these by default.

**Resource fork** — The Mac file-system extension where executable code and resources (menus, icons, strings, alerts) live for classic Mac apps. The data fork holds user content. On systems without resource-fork support, `%filename.ad` (AppleDouble) or `.bin` (MacBinary) sidecars carry the fork.

**MacBinary / AppleDouble** — Two sidecar formats for carrying classic Mac resource forks across resource-fork-less filesystems. MacBinary is a single file merging data + resource + Finder info. AppleDouble is two files: the data fork as-is, and a `%filename.ad` companion for the resource fork.

**FSSpec** — Classic Mac File Manager structure that identifies a file by volume + parent directory ID + name. Passed to `FSpOpenDF`, `FSpCreate`, etc.

**HFS / HFS+** — Hierarchical File System, Apple's classic filesystem (HFS) and its successor (HFS+). What a `.dsk` image contains.

**Gestalt** — Toolbox API for querying system capabilities ("does this system have QuickTime?", "which system version?"). `Gestalt(selector, &result)` fills `result` with a feature-specific encoding.

**MacTCP** — Apple's original TCP/IP stack for classic Mac. Accessed via Device Manager calls to the `.IPP` driver. Not used by Armadillo (we're offline) but relevant because the reference project, `hello-world`, uses it to demonstrate networking patterns we borrow from.

---

## Part 7: The people and projects behind this

**Marco Piovanelli** wrote WASTE. He maintained it actively from 1993 through the early 2000s under the Merzwaren brand, used by dozens of commercial Mac apps. His website went offline in the mid-2000s and contact became difficult. WASTE lives on as archived source on Macintosh Garden. If you're building anything serious that needs real text editing on classic Mac, you end up reading Piovanelli's code.

**Wolfgang "autc04" Thaller** maintains Retro68. The toolchain is a continuous engineering exercise — modern GCC is a moving target, Apple's old linker formats are esoteric, CMake integration for a cross-compiler is never fun. He also publishes the `ghcr.io/autc04/retro68` Docker image that makes our CI trivial. The project lives at https://github.com/autc04/Retro68.

**Martin "mity" Mitáš** wrote md4c. His design criteria — single-file C89, zero dependencies, caller-controlled memory — were not chosen by accident; they make md4c usable on hardware that a typical CommonMark parser would reject. The project lives at https://github.com/mity/md4c.

**The classic Mac retrocomputing community** on 68kmla, Macintosh Garden, and various other forums keeps the platform alive with VMs, emulators, documentation archives, and shared utilities. The fact that you can still run System 7 in 2026 and cross-compile C for it is the product of thousands of volunteer hours across decades.

---

## Closing

Armadillo Editor's design is ultimately a response to two questions.

The first is mechanical: how do you write a modern markdown editor for a 1993 machine with 4 megabytes of RAM, a 25 MHz processor, and a text widget that can't hold a novel chapter? The answers — a Handle-backed arena, SAX parsing with fan-out sinks, a flat block model, vendor-free interfaces, C89 discipline, host-testable modules behind three test seams — aren't individually surprising. Each is a known pattern. What makes the project interesting is the combination: each decision defends the ones around it, and together they produce code that's small, fast, testable, and maintainable on hardware that gives you none of those things for free.

The second question is philosophical: why bother? The honest answer is that constraints teach. A markdown editor on a modern laptop has infinite compute and memory, which means every tradeoff the big-machine editors quietly make is hidden from the user. On a 25 MHz Mac you have to see them. You have to decide. You parse once because parsing twice would be slow. You build a flat model because a tree would leak. You write your own arena because `malloc` fights the Memory Manager. You write your own HTML renderer because gumbo is bigger than your whole app. Every one of these choices is a lesson about what markdown editors *are*, underneath the frameworks that usually carry them.

If you listen to a podcast generated from this document, or read the code that implements it, or just stare at the hand-stitched pinstripe title bars in a QEMU window — the thing to take away isn't "here's how to target retro hardware." It's "here's what your editor is actually doing, with the abstraction layers peeled back one at a time." That's worth something regardless of whether you ever run it on a real Quadra.

The project itself is a single repository at `github.com/manufarfaro/armadillo-editor`. The PRD, OpenSpec proposal, design document, capability specs, and implementation plan are all in the `docs/` and `openspec/` directories. If you want the long version of any concept covered here, that's where to find it.

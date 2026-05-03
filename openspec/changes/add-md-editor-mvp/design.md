# Design: Armadillo Editor MVP

This document is the architecture-level design for the Markdown Editor MVP. It captures the decisions made during brainstorming on 2026-04-19 in full detail. Each section is organized to be audit-friendly: decisions are stated explicitly, rationale is included, and alternatives that were rejected are named so future contributors don't re-litigate settled questions.

**Target:** 68030 / System 7.x / Retro68 / Quadra 800 primary.
**MVP scope:** PRD Tier 0 + Tier 1.
**Primary constraint driving almost every decision:** Mac OS Memory Manager uses movable `Handle`-backed memory; classic C libraries assume stable-pointer `malloc`. Reconciling those is the single most important architectural concern.

---

## 1. Data types & the flat-block model

### 1.1 Core types

Every downstream module's API hangs off these types. They live in `render/blocks.h` and `render/inlines.h`.

```c
/* render/blocks.h */

typedef enum {
    kBlockParagraph,     /* plain text paragraph                         */
    kBlockHeading,       /* h_level = 1..6                               */
    kBlockListItem,      /* bulleted or numbered                         */
    kBlockBlockQuote,    /* > quoted block                               */
    kBlockCodeBlock,     /* ``` fenced or indented code                  */
    kBlockHr,            /* --- horizontal rule                          */
    kBlockHtml           /* md4c-detected HTML block; MVP renders raw    */
} BlockKind;

typedef struct {
    BlockKind       kind;
    unsigned char   h_level;      /* 1..6 for kBlockHeading, 0 otherwise   */
    unsigned char   list_depth;   /* 0 = not in list; 1+ = nesting depth   */
    unsigned char   quote_depth;  /* 0 = not in quote; 1+ = nesting depth  */
    unsigned char   list_ordered; /* 0 = bullet, 1 = numbered              */
    const char*     text;         /* pointer into arena; NOT NUL-terminated */
    unsigned short  text_length;  /* bytes                                  */
    unsigned short  run_count;
    const MdStyleRun* runs;         /* arena-allocated; NULL if run_count=0  */
} Block;
```

```c
/* render/inlines.h */

typedef enum {
    kStylePlain,
    kStyleEmph,         /* _italic_                                     */
    kStyleStrong,       /* **bold**                                     */
    kStyleCodeSpan,     /* `code`                                       */
    kStyleLink,         /* [text](url)                                  */
    kStyleHtmlSpan      /* inline <tag>...</tag>; MVP: raw              */
} StyleKind;

typedef struct {
    unsigned short start;     /* byte offset into Block.text            */
    unsigned short length;    /* bytes                                  */
    StyleKind      kind;
    short          link_index; /* index into per-model link table; -1 if N/A */
} MdStyleRun;
```

### 1.2 The flat-block model (no tree)

The `RenderModel` is a flat array of `Block` structs — not a tree. Nesting is expressed via scalar fields on each block: `list_depth` (0 = not in list, 1+ = nesting depth), `quote_depth` (same semantics for blockquotes).

**Example — nested list:**

```markdown
- element
  - sub element
```

becomes:

```
blocks[0] = { kind=kBlockListItem, list_depth=1, quote_depth=0, text="element"     }
blocks[1] = { kind=kBlockListItem, list_depth=2, quote_depth=0, text="sub element" }
```

The layout pass indents by `list_depth * kIndentListPx + quote_depth * kIndentQuotePx` and picks the bullet glyph by depth.

**Example — paragraph inside a blockquote inside a list:**

```markdown
> - item
>
>   second paragraph
```

becomes:

```
blocks[0] = { kind=kBlockListItem,  list_depth=1, quote_depth=1, text="item"             }
blocks[1] = { kind=kBlockParagraph, list_depth=1, quote_depth=1, text="second paragraph" }
```

### 1.3 Why flat — and why no tree

- md4c's SAX callbacks already flatten natively: open/close events for container blocks, stamped with their current depth by a depth counter in the `mdparse` adapter.
- The output is a linear stream of QuickDraw draw calls down the page. Hierarchical containment and depth-stamping are *equivalent representations* for rendering — only the layout algorithm differs.
- A tree would cost recursive allocation, recursive traversal, per-node free-list management, and harder arena policy (pointer graphs inside an arena are fiddly).
- The render model is *regenerated from scratch* on every debounced parse cycle. There are no structural edits on the model in flight. Trees pay off when you edit the structure; we don't.
- Rich-text formats that render to flow text (Word, Pages, HTML-when-rendered-as-flow) universally use depth-stamped flat paragraph sequences, not trees. We're in good company.

### 1.4 Invariants

Four invariants this shape commits us to; violating any breaks the architecture.

1. **No tree — flat list only.** Nesting expressed via `list_depth` / `quote_depth` scalars.
2. **All variable-length data lives in one arena.** `Block.text`, `MdStyleRun` arrays, link-URL strings — all out of a single `Handle`-backed arena owned by `RenderModel`. No per-object `malloc`, no per-object `NewHandle`.
3. **Text is not NUL-terminated.** Every string carries its length. Matches how md4c hands us text, and lets us reference slices of the source buffer directly without copying for un-escaped ranges.
4. **`unsigned short` offsets, not `size_t`.** Capped at 64 KB per field. TE's 32,767-byte ceiling bounds the source, so per-block text stays well below 32 KB. Saves ~4 bytes per block × thousands of blocks.

### 1.5 Alternatives considered and rejected

- **Tree model (parent/child pointers).** Rejected: recursive allocation, no benefit for linear rendering, harder arena policy. Revisit only if we ever need structural edits on the model (not planned).
- **32-bit offsets (`size_t`).** Rejected for MVP: TE caps the source buffer at 32,767 bytes, so 16-bit offsets are sufficient. Revisit when the custom text engine lands.
- **Styles stored per-character.** Rejected: O(N) memory per block vs. O(runs) memory with the run list; for typical markdown, runs are 10–50× smaller.

---

## 2. Module APIs & dependency graph

### 2.1 Dependency graph

```
                    ┌────────────┐
                    │    app     │  main event loop, Toolbox init
                    └──────┬─────┘
               ┌───────────┼───────────┐
               ▼           ▼           ▼
          ┌────────┐  ┌────────┐  ┌──────────┐
          │ menus  │  │win_edit│  │ file_io  │
          └────┬───┘  └───┬────┘  └────┬─────┘
               │          │            │
               │   ┌──────┼──────┬─────┘
               │   │      │      │
               │   ▼      ▼      ▼
               │ ┌────┐ ┌────────┐ ┌────────┐
               │ │doc │ │src_pane│ │mdparse │──► md4c (vendored)
               │ └────┘ └────────┘ └───┬────┘
               │                       │
               │           ┌───────────┼───────────┐
               │           ▼                       ▼
               │      ┌─────────┐             ┌─────────┐
               │      │ scanner │             │ render  │
               │      └─────────┘             └────┬────┘
               │                                   ▼
               │                              ┌─────────┐
               │                              │ draw_qd │──► QuickDraw or mock
               │                              └─────────┘
               ▼
       ┌────────────────┐
       │ mac_syscalls   │  wraps every Toolbox call used anywhere
       └────────────────┘
```

Acyclic, layered. No module calls a module that depends on it.

### 2.2 Module responsibilities (one sentence each)

- **`src/app`** — main event loop (`WaitNextEvent` loop), Toolbox init, quit, the `MacSyscalls` real wiring.
- **`src/menus`** — menu bar construction and command dispatch; maps menu selections to window or app commands.
- **`src/win_editor`** — the single editor window; owns the current mode (Source / Read), the source pane, the document, and the render model; coordinates the parse pipeline.
- **`src/doc`** — dumb data container: source text buffer, filename (as FSSpec or equivalent), dirty flag.
- **`src/file_io`** — Standard File Open / Save wrappers plus raw file-manager I/O for `.md` bytes.
- **`src/debounce`** — pure state machine: takes keystroke notifications and clock ticks, decides when to fire a parse.
- **`src/draw_qd_real`** — production `DrawOps` vtable: translates vtable calls into real QuickDraw.
- **`src_pane/`** — opaque editable-text-pane API; MVP internally TE-backed; future replacement is a custom piece-table engine.
- **`mdparse/`** — wraps md4c's `md_parse` behind a `MdParseSink` fan-out interface; hides md4c from the rest of the codebase.
- **`scanner/`** — `MdParseSink` implementation: accumulates `MdStyleRun` tuples for source-pane syntax coloring.
- **`render/`** — two things: (a) `MdParseSink` implementation that builds a `RenderModel` in an arena; (b) layout pass that emits through the `DrawOps` vtable.

### 2.3 Three test seams

Where mocks plug in and production code never knows the difference.

1. **`src/mac_syscalls.h`** — a struct of function pointers wrapping every Toolbox call used anywhere (`NewWindow`, `TESetStyle`, `StandardGetFile`, `NewHandle`, `HLock`, `TickCount`, `Gestalt`, `PBControlSync`, …). Any module that touches the OS takes `const MacSyscalls*` as a parameter and dispatches through it. Host tests inject a struct of fakes; production uses the real wrappers from `src/app.c`. This is the same pattern as the hello-world reference project.

2. **`render/draw_qd.h`** — the renderer never calls QuickDraw directly. It calls through a `DrawOps` vtable (`set_font`, `set_fg`, `move_to`, `draw_text`, `line`, `frame_rect`, `get_font_metrics`). Production wires this to `src/draw_qd_real.c`; tests wire it to a recording sink that captures every call into an array for assertion.

3. **`mdparse/mdparse.h` — `MdParseSink`** — scanner and render both plug in as `MdParseSink` instances. Tests can bypass md4c entirely and feed synthetic event streams straight into scanner or render. This decouples render tests from md4c's parse behavior.

### 2.4 Public header sketches

```c
/* src/doc.h */
typedef struct Doc Doc;
Doc*         doc_new(void);
void         doc_free(Doc*);
const char*  doc_text(const Doc*, unsigned short* out_len);
void         doc_set_text(Doc*, const char* bytes, unsigned short len);
int          doc_is_dirty(const Doc*);
void         doc_mark_clean(Doc*);
void         doc_mark_dirty(Doc*);
/* filename accessors, etc. */
```

```c
/* src_pane/src_pane.h — vendor-free; NO TEHandle, NO Toolbox types */
typedef struct SrcPane SrcPane;
SrcPane*     src_pane_new(const SrcPaneParams*, const MacSyscalls*);
void         src_pane_free(SrcPane*);
void         src_pane_set_text(SrcPane*, const char* bytes, unsigned short len);
const char*  src_pane_get_text(const SrcPane*, unsigned short* out_len);
void         src_pane_apply_runs(SrcPane*, const MdStyleRun* runs, size_t count);
void         src_pane_get_selection(const SrcPane*, unsigned short* out_start, unsigned short* out_end);
void         src_pane_handle_key(SrcPane*, short char_code, short key_code);
/* activate/deactivate, update, click, idle (cursor blink) */
```

```c
/* mdparse/mdparse.h — hides md4c */
typedef struct MdParseSink MdParseSink;   /* struct of callbacks, defined below */
int mdparse_run(const char* source, unsigned short source_len,
                const MdParseSink* sinks, size_t sink_count);
/* Returns 0 on success, negative kMdParseErr* on failure. */
```

```c
/* scanner/scanner.h */
typedef struct Scanner Scanner;
Scanner*             scanner_new(Arena*);
const MdParseSink*   scanner_sink(Scanner*);
const MdStyleRun*      scanner_runs(const Scanner*, size_t* out_count);
void                 scanner_reset(Scanner*);
```

```c
/* render/render.h */
typedef struct RenderModel RenderModel;
RenderModel*         render_model_new(Arena*);
const MdParseSink*   render_model_sink(RenderModel*);
size_t               render_model_block_count(const RenderModel*);
const Block*         render_model_block_at(const RenderModel*, size_t);

typedef struct LayoutParams {
    short content_width;     /* pixels */
    short indent_list;       /* px per list_depth step */
    short indent_quote;      /* px per quote_depth step */
    /* font sizes per block kind, spacing, etc. */
} LayoutParams;

void render_layout_and_draw(const RenderModel*, const LayoutParams*, DrawContext*);
```

### 2.5 Boundary rules

Locked in, applies to every module:

- **No Mac Toolbox types in public headers across module boundaries.** `render/`, `mdparse/`, `scanner/`, `src_pane/` expose only `short`, `unsigned short`, plain `char*`, project-owned structs. Never `TEHandle`, `WindowPtr`, `RGBColor`, `FSSpec`, or md4c types.
- **`Arena` is the only allocator used by `mdparse`, `scanner`, `render`.** Never `NewHandle`, `NewPtr`, or `malloc` directly in those modules. Arena lifetime equals the current parse cycle or up to doc close.
- **Each public function either returns `int` (0 = ok, negative = error) or returns a resource pointer.** Never both.
- **`_test.c` is colocated with `.c`** (except where the module isn't host-buildable).

---

## 3. Parse pipeline & debounce state machine

### 3.1 The shape of the loop

```
User keystroke in src_pane
       │
       ▼
win_editor.on_edit(): dirty = 1; last_keystroke_tick = TickCount()
       │
       ▼
App main loop: WaitNextEvent(mask, &ev, 6 /* ticks */, nil)
       │
       ├── every iteration (event or null) → check_debounce()
       │
       ▼
check_debounce():
   if (!dirty) return;
   if (TickCount() - last_keystroke_tick < kParseDebounceTicks) return;
   run_parse_cycle(); dirty = 0;
       │
       ▼
run_parse_cycle():                 (synchronous; blocks event loop ~100–400 ms)
   const RenderModel* prev = s->current_model;
   s->current_model = NULL;
   if (s->mode == kModeRead) InvalRect(&s->read_pane_rect);

   size_t estimate = s->doc_len * 4u + 16u * 1024u;
   if (arena_ensure(s->arena, estimate) != 0) { show_alert(kMemoryFullAlertID); return; }

   arena_reset(s->arena);
   RenderModel* nm = render_model_new(s->arena);
   const MdParseSink sinks[2] = {
       *scanner_sink(s->scanner),
       *render_model_sink(nm),
   };
   int rc = mdparse_run(s->doc_text, s->doc_len, sinks, 2);
   if (rc < 0) { show_alert(kParseFailedAlertID + (-rc)); return; }

   src_pane_apply_runs(s->src_pane, scanner_runs(s->scanner, &n), n);
   s->current_model = nm;
   if (s->mode == kModeRead) InvalRect(&s->read_pane_rect);
   (void)prev;   /* prev was already invalidated by arena_reset */
```

### 3.2 Why synchronous is safe and right

Classic Mac OS is cooperative. No preemptive threads, no async I/O in the middle of an event loop. A 100–400 ms parse on a 25 MHz 68030 blocks the UI, but only *after* the user has been idle 250 ms. The user isn't typing while we're parsing — the debounce window guarantees it.

Escape hatches we'd reach for only if profiling shows a problem (none of these ship in MVP):

- **Incremental parse by block.** md4c exposes no official incremental API, but we can chunk the buffer at blank lines (CommonMark block boundaries) and re-parse only the chunk containing the edit.
- **Thread Manager.** System 7.5+ has cooperative threads; move parse to a secondary thread with explicit yields. Raises our floor to 7.5+ and adds synchronization.

### 3.3 Debounce tuning knobs

All compile-time constants, overridable via `-D` at build time:

```c
#ifndef kParseDebounceTicks
#define kParseDebounceTicks 15    /* 60 ticks/sec × 0.25 s */
#endif
#ifndef kEventLoopSleepTicks
#define kEventLoopSleepTicks 6    /* ~100 ms null-event cadence */
#endif
```

### 3.4 Memory lifecycle per parse cycle

- **Before parse:** old `RenderModel` (pointers into old arena) is still valid and displayed.
- **`arena_reset(arena)`:** watermark back to 0. Backing `Handle` is NOT freed — reuse the already-allocated memory so we don't re-`NewHandle` every parse. Old model's pointers are now stale; nothing holds them (we zeroed `current_model` first).
- **During parse:** md4c callbacks allocate out of the arena via `arena_alloc(arena, n)`, which returns a pointer into the `HLock`ed backing memory. Block structs and text slices allocated similarly.
- **After parse:** arena watermark is at the new model's size. `Handle` is grown only if we overflowed during allocation (but see §4: we grow *before* the parse, never mid-parse).
- **Doc close:** `arena_destroy` calls `DisposeHandle` once. No per-block frees.

### 3.5 Error model for the pipeline

```c
typedef enum {
    kMdParseOk             =  0,
    kMdParseErrArenaOOM    = -1,   /* arena grow failed */
    kMdParseErrMd4c        = -2,   /* md4c returned non-zero */
    kMdParseErrSinkAbort   = -3,   /* a sink signaled abort */
} MdParseError;
```

On any error:
- `current_model` stays `NULL` (was zeroed before parse started).
- User sees a blank Read pane until the next successful parse — or they're in Source mode and see nothing broken.
- An alert surfaces the error cause via STR# strings.
- Scanner runs collected before the failure are discarded (never a partial style pass).

### 3.6 Sink dispatch order

`mdparse_run` calls sinks in array order for each event. Scanner first (cheaper), render second. Order doesn't affect correctness (sinks don't communicate) but determinism matters for tests.

### 3.7 Test fixture for the pipeline

Host tests exercise the whole pipeline without the event loop:

```c
void test_debounce_two_edits_within_window_parses_once(void) {
    FakeClock clock = {0};
    EditorState s = {0};

    editor_on_edit(&s, &clock);          /* clock=0 */
    clock.ticks = 10;
    editor_on_edit(&s, &clock);          /* still < 15 ticks → no parse */
    clock.ticks = 12;
    editor_poll_debounce(&s, &clock);    /* still within window */
    clock.ticks = 28;
    editor_poll_debounce(&s, &clock);    /* 28 - 12 = 16 ≥ 15 → parse */

    TEST_ASSERT_EQUAL_INT(1, s.parse_count);
}
```

`FakeClock` is a test-only `MacSyscalls.tick_count` that returns whatever we set. The debounce logic never calls `TickCount()` directly — always through `syscalls->tick_count()`.

### 3.8 Alternatives considered and rejected

- **Async / threaded parse from day 1.** Requires Thread Manager (System 7.5+) and synchronization. Rejected for MVP; blocks the main thread only after the user has stopped typing, which is fine.
- **Parse on every keystroke (no debounce).** Rejected: 100–400 ms per keystroke is brutally slow on 25 MHz. 250 ms debounce keeps interactive feel within budget.
- **Parse only on mode toggle (Approach C from brainstorm).** Rejected: ~200 ms stall on the toggle, and makes live-preview (Split mode, future tier) impossible without re-architecture.

---

## 4. Allocator policy & arena

The concrete answer to "Mac Memory Manager vs. library `malloc` assumptions." Everything in the render pipeline lives in one arena; the arena is the only thing that talks to `NewHandle` / `SetHandleSize`.

### 4.1 What a `Handle` is, briefly

A `Handle` on classic Mac is a *pointer to a master pointer*. The Memory Manager can *move* the block the master pointer points at, compacting the heap. To read the block's contents safely, you call `HLock(h)` which pins it in place until `HUnlock`. Modern C assumes pointers are stable — on classic Mac they are not, unless you lock them.

### 4.2 Our arena

One `Handle` of a few KB up to ~192 KB, `HLock`ed for the arena's entire lifetime (from `arena_init` to `arena_destroy`). While locked, `*backing` is a stable pointer into memory. Allocation is a bump pointer.

```c
/* render/arena.h */
typedef struct Arena Arena;
int    arena_init(Arena**, size_t initial_size, const MacSyscalls*);
void   arena_destroy(Arena*);
int    arena_ensure(Arena*, size_t bytes_needed);   /* grow up-front */
void*  arena_alloc(Arena*, size_t n_bytes);         /* 4-byte aligned */
void   arena_reset(Arena*);
size_t arena_high_water(const Arena*);
size_t arena_capacity(const Arena*);
```

```c
/* render/arena.c — internals */
struct Arena {
    Handle      backing;
    char*       base;      /* = *backing while HLocked */
    size_t      size;      /* current Handle size */
    size_t      high_water;
    size_t      max_ever;
    MacSyscalls sys;       /* by-value copy taken at arena_init */
};

#define kArenaInitialSize    ( 8u * 1024u)
#define kArenaDoublingCap    (64u * 1024u)
#define kArenaLinearStep     (32u * 1024u)
#define kArenaHardCap       (512u * 1024u)
```

The `MacSyscalls` field is stored by value, not as a pointer. `arena_init` takes a `const MacSyscalls*` parameter and copies `*sys` into the field at init. After that the `Arena` owns its private 80-byte snapshot and does not depend on the caller's storage lifetime. This eliminates the lifetime-coupling bug that an earlier draft (with a `const MacSyscalls*` field) introduced — see `openspec/changes/arena-owns-macsyscalls/` for the follow-up change that established this rule.

### 4.3 Four policy decisions

1. **Grow-before-parse, never grow mid-parse.** The pipeline calls `arena_ensure(arena, estimate)` *before* a single byte is allocated. The hot-path `arena_alloc` does a single bounds check and a bump. If allocation would overflow, it returns `NULL` → `mdparse_run` returns `kMdParseErrArenaOOM` → cycle fails cleanly with previous model displayed (or `NULL`, if this was the first parse).

   This rule matters because **`SetHandleSize` can physically move the block.** If grow happens mid-parse, every pointer issued before the grow is garbage. We dodge that by pre-sizing. Estimate: `source_len × 4 + 16 KB`. If the estimate is wrong and we OOM, bump +50% and retry once; if that OOMs too, fail the cycle.

2. **4-byte alignment on every allocation.** 68020+ (which 68030 is) traps on longword access to odd addresses. `arena_alloc` rounds `n_bytes` up to a multiple of 4 before advancing the watermark. < 3 B waste per allocation.

3. **Reset, don't free, between parses.** `arena_reset` sets `high_water = 0`. Backing `Handle` keeps its memory. Next parse reuses the same pool. `DisposeHandle` is called only by `arena_destroy`, which runs on doc close.

4. **Hard cap at 512 KB.** A runaway parse (pathological input) should fail rather than eat the system heap. 512 KB × 2 docs = 1 MB max, comfortable on a 4 MB machine.

### 4.4 Safety invariant: reset-and-swap for display integrity

On parse failure, we mustn't leave the Read pane pointing at half-written memory. The `run_parse_cycle` sequence (§3.1) enforces:

1. `current_model = NULL` *before* touching the arena.
2. `arena_ensure` — if this fails, give up with `current_model` still NULL.
3. `arena_reset` — old model's pointers become semantically stale (bytes still there until overwritten, but we never read them).
4. Parse. On success: commit `current_model = new_model`.
5. On failure: `current_model` stays NULL, user sees blank Read pane until next successful parse.

### 4.5 Worst-case sizing for a 32 KB doc

- Blocks: ~2000 worst case × 32 B = ~64 KB
- Style runs: ~8000 worst case × 8 B = ~64 KB
- Text slices: up to 32 KB (slices referenced, not copied, where possible)
- Link URLs: ~8 KB

**Worst case total: ~170 KB.** Typical for a 10 KB doc: ~25 KB. Fits comfortably under the 512 KB hard cap.

### 4.6 Test strategy for the arena

Host tests use `FakeSyscalls` with a `malloc`-backed fake `Handle`. The fake `SetHandleSize` can be instrumented to fail on the Nth call, letting us exercise grow-failure paths deterministically.

```c
void test_arena_grow_failure_preserves_previous_high_water(void) {
    FakeSyscalls sys = fake_syscalls_init();
    sys.set_handle_size_fail_after = 0;         /* fail the first grow */

    Arena* a = NULL;
    arena_init(&a, 128, (const MacSyscalls*)&sys);
    (void)arena_alloc(a, 64);                   /* fine */
    TEST_ASSERT_EQUAL_INT(-1, arena_ensure(a, 1024));
    TEST_ASSERT_EQUAL_INT(64, arena_high_water(a));
    TEST_ASSERT_EQUAL_INT(128, arena_capacity(a));
    arena_destroy(a);
}
```

### 4.7 Deliberately not doing (MVP)

- **Free list / per-object free.** Flat bump allocator only.
- **Two-arena double buffering.** Simpler to carry "parse failure = blank Read pane" for MVP. If we see parse failures with live documents in practice, upgrade later.
- **Compaction.** We never free individual allocations, so never need to compact.
- **`NewPtr` allocations.** Everything goes through `Handle`.

---

## 5. Error model & propagation conventions

### 5.1 Conventions

1. **Every function returns either a resource pointer OR an error code, never both.**
   - Resource acquisition: pointer return, `NULL` means failure.
   - Operation: `int` return, 0 = ok, negative = module-specific error code.
   - The two forms never mix.

2. **Each module defines its own error enum; errors don't flow through module boundaries un-translated.**

```c
/* render/render.h */
typedef enum {
    kRenderOk            =  0,
    kRenderErrArenaOOM   = -1,
    kRenderErrBadModel   = -2,
    kRenderErrEmitAborted = -3,
} RenderError;

/* mdparse/mdparse.h */
typedef enum {
    kMdParseOk           =  0,
    kMdParseErrArenaOOM  = -1,   /* maps internal arena OOM */
    kMdParseErrMd4c      = -2,
    kMdParseErrSinkAbort = -3,
} MdParseError;
```

   When `mdparse_run` internally triggers an `arena_ensure` that fails, it catches the arena's error and translates to `kMdParseErrArenaOOM`. Callers see `mdparse` errors only. No 12-layer enum-of-enums at the top.

3. **Goto-cleanup pattern, no exceptions:**

```c
int file_io_open(const FSSpec* spec, Doc** out_doc, const MacSyscalls* sys) {
    int rc = kFileIoOk;
    short ref = 0;
    Doc* doc = NULL;
    Handle text_handle = NULL;

    rc = sys->open_df(spec, fsRdPerm, &ref);
    if (rc != noErr) { rc = kFileIoErrOpen; goto cleanup; }

    long size = 0;
    if (sys->get_eof(ref, &size) != noErr) { rc = kFileIoErrStat; goto cleanup; }
    if (size > kMaxDocBytes)                { rc = kFileIoErrTooBig; goto cleanup; }

    text_handle = sys->new_handle((Size)size);
    if (!text_handle)                       { rc = kFileIoErrOOM; goto cleanup; }

    sys->hlock(text_handle);
    long actual = size;
    if (sys->fs_read(ref, &actual, *text_handle) != noErr) { rc = kFileIoErrRead; goto cleanup; }

    doc = doc_new();
    if (!doc)                               { rc = kFileIoErrOOM; goto cleanup; }
    doc_set_text(doc, *text_handle, (unsigned short)actual);
    doc_set_filename(doc, spec);

    *out_doc = doc;
    doc = NULL;                 /* ownership transferred */
    text_handle = NULL;         /* doc owns it now */

cleanup:
    if (ref)         sys->fs_close(ref);
    if (doc)         doc_free(doc);
    if (text_handle) { sys->hunlock(text_handle); sys->dispose_handle(text_handle); }
    return rc;
}
```

   Every function that acquires ≥ 2 resources uses this shape. `cleanup:` at bottom, resources released in reverse order, ownership transfers set the local to `NULL` so cleanup skips them.

4. **No `printf`, no logging, in library code.** `render/`, `mdparse/`, `scanner/`, `src_pane/`, `render/arena` never call `DebugStr`, `printf`, `fprintf`, or `SysBeep`. They return error codes. Only `src/` decides how to present errors — via ALRT resource + `NoteAlert` / `StopAlert`.

5. **`const MacSyscalls*` is threaded through every function that might touch the OS.** Never cached globally, never defaulted. This is what makes host-side tests possible — you always have a seam to inject mocks.

### 5.2 Error-to-user mapping

Alert IDs in `armadillo.r`:

```
enum                      ALRT id   STR# (128) index   message
──────────────────────────────────────────────────────────────────────────────
kRenderErrArenaOOM          256            1            "Not enough memory to display this document."
kRenderErrBadModel          257            2            "Internal error while laying out the document."
kMdParseErrArenaOOM         258            3            "Not enough memory to parse this document."
kMdParseErrMd4c             259            4            "Could not parse the Markdown source."
kFileIoErrOpen              260            5            "Could not open the file."
kFileIoErrTooBig            261            6            "That document is larger than 32 KB."
kFileIoErrRead              262            7            "Error reading the file."
kFileIoErrWrite             263            8            "Error writing the file."
kFileIoErrOOM               264            9            "Not enough memory to read this file."
kFileIoErrStat              265           10            "Could not get file information."
```

STR# resources make strings editable in ResEdit without recompiling — period-appropriate convention.

### 5.3 Test assertions on errors

Named error codes in assertions, never magic numbers:

```c
void test_file_io_open_missing_file_returns_kFileIoErrOpen(void) {
    FakeSyscalls sys = fake_syscalls_init();
    sys.open_df_result = fnfErr;
    Doc* doc = NULL;
    FSSpec spec = {0};
    int rc = file_io_open(&spec, &doc, (const MacSyscalls*)&sys);
    TEST_ASSERT_EQUAL_INT(kFileIoErrOpen, rc);
    TEST_ASSERT_NULL(doc);
}
```

---

## 6. Test strategy & tests-as-documentation

### 6.1 Three tiers

**Tier 1 — Host unit tests (the bulk of coverage).** Colocated `_test.c` files compiled with host `cc` + Unity. Linked only against the module under test + its deps. Run on every build via `make -f Makefile.hosttests test`.

| Module          | Host-testable? | What we test                                                                         |
|-----------------|----------------|--------------------------------------------------------------------------------------|
| `render/arena`  | Yes            | init/alloc/reset/grow/destroy; alignment; OOM paths; FakeSyscalls Handle fake        |
| `render/render` | Yes            | synthetic sink events → RenderModel; layout pass → recorded `DrawOps` calls          |
| `mdparse`       | Yes            | md4c fixtures → sink event sequence; error remapping; sink abort propagation         |
| `scanner`       | Yes            | synthetic sink events → `MdStyleRun` arrays; range math; overlap handling              |
| `doc`           | Yes            | setters, getters, dirty flag                                                          |
| `src/debounce`  | Yes            | state machine with FakeClock                                                          |
| `src/file_io`   | Partial        | non-Standard-File parts yes; Standard File dialog itself no                          |
| `src_pane`      | **No**         | depends on TE/Toolbox — target-only; verified via on-device smoke test               |
| `src/win_editor`| **No**         | depends on Window Manager / QuickDraw — target-only                                  |

**Tier 2 — On-device smoke tests.** A small test harness `.APPL` built with Retro68 that runs inside the Quadra VM, exercises Toolbox-heavy paths (source-pane edits → scanner runs → style application roundtrip; Standard File open → file_io → doc → display), and writes PASS/FAIL to the screen. Source at `src/smoke_test.c`. Run manually before tagging releases, not on every build.

**Tier 3 — Manual acceptance.** One-page checklist (goes in `docs/acceptance.md` when we create it):
- Open a ~30 KB markdown doc, toggle to Read mode, verify inline HTML renders as literal text (MVP behavior).
- Verify style coloring survives editing (add a bold, delete a heading, etc.).
- Verify Save preserves content byte-for-byte.
- Open a > 32 KB file → verify the alert appears and the app doesn't crash.
- Trigger OOM by opening many docs → verify graceful error, no crash.

Run before each tagged release.

### 6.2 The recording emitter (renderer's main test seam)

```c
/* render/draw_qd.h — the mockable interface */
typedef struct DrawOps {
    void (*set_font)(void* ctx, short font_id, short size, unsigned char face);
    void (*set_fg)(void* ctx, unsigned short red, unsigned short green, unsigned short blue);
    void (*move_to)(void* ctx, short h, short v);
    void (*draw_text)(void* ctx, const char* bytes, unsigned short length);
    void (*line)(void* ctx, short x0, short y0, short x1, short y1);
    void (*frame_rect)(void* ctx, short l, short t, short r, short b);
    void (*get_font_metrics)(void* ctx, short* ascent, short* descent, short* line_height);
} DrawOps;

typedef struct DrawContext { const DrawOps* ops; void* ctx; } DrawContext;
```

Production: `src/draw_qd_real.c` wraps real QuickDraw.
Tests: `test/recorder.c` wraps a `Recorder` — a `malloc`-backed array of `RecordedCall` unions, one entry per vtable call.

Render tests assert on the recorded call stream:

```c
void test_heading_level_1_draws_bold_chicago_17_with_text_content(void) {
    Arena* a; arena_init(&a, 4096, fake_syscalls_defaults());
    RenderModel* m = render_model_new(a);
    const MdParseSink sink = *render_model_sink(m);

    /* Feed synthetic md4c-shaped events; bypass md4c entirely */
    sink.on_block_open(sink.ctx, kBlockHeading, &(BlockAttrs){.h_level=1});
    sink.on_text(sink.ctx, "Hello", 5, 2);
    sink.on_block_close(sink.ctx, kBlockHeading);

    Recorder rec = {0};
    DrawContext ctx = recorder_draw_context_new(&rec);
    render_layout_and_draw(m, default_layout_params(), &ctx);

    TEST_ASSERT_EQUAL_INT(3, rec.count);
    TEST_ASSERT_EQUAL_INT(kRecSetFont,  rec.calls[0].op);
    TEST_ASSERT_EQUAL_INT(kChicago,     rec.calls[0].font.font_id);
    TEST_ASSERT_EQUAL_INT(17,           rec.calls[0].font.size);
    TEST_ASSERT_EQUAL_INT(bold,         rec.calls[0].font.face);
    TEST_ASSERT_EQUAL_INT(kRecMoveTo,   rec.calls[1].op);
    TEST_ASSERT_EQUAL_INT(kRecDrawText, rec.calls[2].op);
    TEST_ASSERT_EQUAL_INT(5,            rec.calls[2].draw_text.length);
    TEST_ASSERT_EQUAL_MEMORY("Hello",   rec.calls[2].draw_text.bytes, 5);

    arena_destroy(a);
    free(rec.calls);
}
```

### 6.3 Tests-as-documentation convention

Following the hello-world project's convention:

- **Function names are sentences:** `test_heading_level_1_draws_bold_chicago_17_with_text_content`, not `test_h1`.
- **Every test has a header block** explaining the concept under test:

```c
/* Heading level 1 rendering
 * ─────────────────────────
 * md4c emits MD_BLOCK_H for every heading with the level (1..6) in attrs.
 * Our mdparse adapter stamps h_level onto a kBlockHeading. The layout pass
 * picks the font from a lookup table (Chicago 17 bold for H1, 14 bold for
 * H2, etc., following System 7 convention). This test verifies that
 * mapping end-to-end by asserting on the recorded QuickDraw calls — font
 * selection, cursor positioning, and text content.
 */
```

- **Toolbox-adjacent tests include a "What is this Mac OS thing" comment:**

```c
/* Handle is a double pointer (Memory Manager master pointer block) that
 * the OS can move to compact the heap. HLock pins it so *handle is stable.
 * Our Arena HLocks its backing Handle for its whole lifetime — this test
 * verifies that arena_init calls HLock exactly once and that the resulting
 * base pointer equals *backing. */
void test_arena_init_locks_backing_handle_and_sets_base_to_dereferenced(void) { ... }
```

### 6.4 Fake syscalls

```c
/* test/fake_syscalls.h — used by every test file that needs Toolbox fakes */
typedef struct FakeSyscalls {
    MacSyscalls vtable;      /* must be first; cast (MacSyscalls*) to use */

    /* Controls for tests */
    unsigned long tick_count_now;
    int           set_handle_size_fail_after;   /* -1 = never */
    short         open_df_result;

    /* Records for assertions */
    int new_handle_calls;
    int hlock_calls;
    int hunlock_calls;
    int dispose_handle_calls;
} FakeSyscalls;

FakeSyscalls fake_syscalls_init(void);   /* sensible defaults */
```

Pattern: test calls `fake_syscalls_init` to get defaults, sets the one or two fields relevant to the test, casts `&fake.vtable` to `const MacSyscalls*` when passing to code under test.

### 6.5 Deliberately not doing (MVP)

- **No mocking framework** (CMock, FFF, etc.). Hand-written fakes.
- **No property testing / fuzzers.** Deterministic fixtures only.
- **No coverage enforcement.** Tests for things that matter, not for a number.
- **No QEMU-based automated on-device tests.** Smoke test is run manually.

---

## 7. Build, packaging & OpenSpec layout

### 7.1 Repo layout

```
armadillo-editor/
├── src/                          # app shell + small glue
│   ├── app.c/h
│   ├── menus.c/h
│   ├── win_editor.c/h
│   ├── file_io.c/h               # ~200 lines, stays in src/
│   ├── doc.c/h                   # ~120 lines, stays in src/
│   ├── draw_qd_real.c/h          # production DrawOps wiring
│   ├── debounce.c/h
│   ├── mac_syscalls.h
│   └── smoke_test.c
├── src_pane/                     # peer to render/, mdparse/, scanner/
│   ├── src_pane.h                # vendor-free; no TE types
│   └── src_pane.c                # TE-backed impl (target-only)
├── render/
│   ├── arena.c/h
│   ├── blocks.h                  # BlockKind + Block
│   ├── inlines.h                 # StyleKind + MdStyleRun
│   ├── render.c/h
│   ├── draw_qd.h                 # DrawOps vtable (pure, no QuickDraw include)
│   ├── arena_test.c
│   └── render_test.c
├── mdparse/
│   ├── mdparse.c/h
│   └── mdparse_test.c
├── scanner/
│   ├── scanner.c/h
│   └── scanner_test.c
├── test/
│   ├── fake_syscalls.c/h         # our own test internals
│   └── recorder.c/h              # our own test internals
├── third_party/                  # vendored deps (at pinned versions)
│   ├── md4c/
│   └── unity/
├── armadillo.r                   # menus, WIND, ALRT, DLOG, DITL, STR#, icons
├── CMakeLists.txt
├── Makefile.hosttests
├── PRD.md
├── README.md
├── CLAUDE.md
├── .gitignore
└── openspec/
    ├── changes/add-md-editor-mvp/{proposal,design,tasks}.md
    ├── changes/add-md-editor-mvp/specs/<cap>/spec.md   # delta specs
    └── specs/<cap>/spec.md                              # authoritative specs
```

### 7.2 CMakeLists.txt (cross-compile for 68k)

```cmake
cmake_minimum_required(VERSION 3.15)
project(ArmadilloEditor C)

set(MD4C_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/md4c/src)
set(MD4C_SOURCES ${MD4C_DIR}/md4c.c)

add_application(ArmadilloEditor
    src/app.c
    src/menus.c
    src/win_editor.c
    src/file_io.c
    src/doc.c
    src/draw_qd_real.c
    src/debounce.c
    src_pane/src_pane.c
    render/arena.c
    render/render.c
    mdparse/mdparse.c
    scanner/scanner.c
    ${MD4C_SOURCES}
    armadillo.r
)

target_include_directories(ArmadilloEditor PRIVATE
    src src_pane render mdparse scanner third_party/md4c/src
)

target_compile_definitions(ArmadilloEditor PRIVATE
    MD4C_USE_ASCII=1
)

target_compile_options(ArmadilloEditor PRIVATE
    -ffunction-sections -Os -Wall
)
set_source_files_properties(${MD4C_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-w"
)
target_link_options(ArmadilloEditor PRIVATE
    "LINKER:-gc-sections" "LINKER:--mac-single"
)

# Separate .APPL for on-device smoke test
add_application(ArmadilloSmokeTest
    src/smoke_test.c
    src/file_io.c
    src/doc.c
    src/draw_qd_real.c
    src_pane/src_pane.c
    render/arena.c
    render/render.c
    mdparse/mdparse.c
    scanner/scanner.c
    ${MD4C_SOURCES}
    armadillo.r
)
```

### 7.3 Makefile.hosttests (host unit tests)

```makefile
CC       ?= cc
CFLAGS   ?= -std=c89 -Wall -Werror -g \
            -Isrc -Irender -Imdparse -Iscanner -Isrc_pane \
            -Itest -Ithird_party/unity -Ithird_party/md4c/src
UNITY    := third_party/unity/unity.c
FAKES    := test/fake_syscalls.c
RECORDER := test/recorder.c

TESTS := build/arena_test build/render_test build/mdparse_test \
         build/scanner_test build/doc_test build/debounce_test \
         build/file_io_test

.PHONY: all test clean
all: $(TESTS)
test: $(TESTS)
	@for t in $(TESTS); do echo "=== $$t ==="; $$t || exit 1; done

build/arena_test:   render/arena.c render/arena_test.c   $(UNITY) $(FAKES)
	@mkdir -p build && $(CC) $(CFLAGS) -o $@ $^
build/render_test:  render/render.c render/render_test.c $(UNITY) $(FAKES) $(RECORDER)
	@mkdir -p build && $(CC) $(CFLAGS) -o $@ $^
build/mdparse_test: mdparse/mdparse.c mdparse/mdparse_test.c third_party/md4c/src/md4c.c $(UNITY) $(FAKES)
	@mkdir -p build && $(CC) $(CFLAGS) -o $@ $^
build/scanner_test: scanner/scanner.c scanner/scanner_test.c $(UNITY) $(FAKES)
	@mkdir -p build && $(CC) $(CFLAGS) -o $@ $^
build/doc_test:     src/doc.c src/doc_test.c             $(UNITY) $(FAKES)
	@mkdir -p build && $(CC) $(CFLAGS) -o $@ $^
build/debounce_test:src/debounce.c src/debounce_test.c   $(UNITY) $(FAKES)
	@mkdir -p build && $(CC) $(CFLAGS) -o $@ $^
build/file_io_test: src/file_io.c src/file_io_test.c src/doc.c $(UNITY) $(FAKES)
	@mkdir -p build && $(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf build/
```

### 7.4 md4c vendoring policy

Pin a specific commit (not a branch, not a mutable tag). Store as plain source in `third_party/md4c/` — no submodule, no package manager. Upgrade is a deliberate act: fetch new source, drop in, run tests, commit with a message naming the source commit SHA and the reason. No drive-by updates.

Use only `md4c.c` + `md4c.h`. Not `md4c-html.c` (we render our own, not to HTML), not the CLI. The parser is a single .c file, ~2500 lines, zero external deps, compiles clean with `-std=c89`.

### 7.5 Resource file (`armadillo.r`)

Following hello-world's Rez pattern:

- `'MBAR'` (128): menu bar wiring `'MENU'` IDs 128..131
- `'MENU'` 128 (Apple): About, separator
- `'MENU'` 129 (File): New, Open…, —, Close, Save, Save As…, —, Quit
- `'MENU'` 130 (Edit): Undo, —, Cut, Copy, Paste, Clear, Select All (all via Toolbox standard handling)
- `'MENU'` 131 (View): Source (checkmark), Read (checkmark) — mutually exclusive
- `'ALRT'` 256..265: one per error code (per §5.2)
- `'STR#'` 128: error message strings indexed per §5.2
- `'ICN#'` 128 + `'ICON'` 128: app icon (32×32 1-bit armadillo; stub for MVP, replaced with rasterized Figma icon later)
- No `'WIND'` resource for MVP — window created programmatically via `NewWindow` with code-provided bounds.

### 7.6 OpenSpec directory layout

```
openspec/
├── changes/
│   └── add-md-editor-mvp/
│       ├── proposal.md       # this change's scope, approach, why
│       ├── design.md         # THIS FILE
│       ├── tasks.md          # TDD-ordered task list
│       └── specs/            # delta specs (ADDED Requirements for this change)
│           ├── app-shell/spec.md
│           ├── doc-store/spec.md
│           ├── src-pane/spec.md
│           ├── mdparse/spec.md
│           ├── scanner/spec.md
│           ├── render/spec.md
│           └── file-io/spec.md
└── specs/                    # authoritative capability specs (grow across changes)
    ├── app-shell/spec.md
    ├── doc-store/spec.md
    ├── src-pane/spec.md
    ├── mdparse/spec.md
    ├── scanner/spec.md
    ├── render/spec.md
    └── file-io/spec.md
```

Seven capabilities, one per top-level module (plus `app-shell` for `src/`'s glue-level responsibilities). Each capability spec follows the hello-world shape: Purpose + Requirements + Scenarios (Given/When/Then).

### 7.7 Task ordering (TDD)

Mirrors hello-world's Group structure. Full detail in `tasks.md`; summary:

- **Group 0** — repo foundation: CMakeLists stub, toolchain wiring, Makefile.hosttests stub, Unity vendored, md4c vendored, armadillo.r stub, empty src/app.c that opens and closes a window.
- **Group 1** — types + interfaces: headers only (mac_syscalls.h, draw_qd.h, blocks.h, inlines.h, Arena public API, SrcPane public API, Doc public API, MdParseSink shape).
- **Group 2** — tests first: write every host unit test against the header-only interfaces. All tests fail at this point (link errors or empty implementations).
- **Group 3** — implementations in dependency order: arena → doc → debounce → mdparse → scanner → render → file_io. Each module's implementation ends when its unit tests pass.
- **Group 4** — Toolbox-coupled targets: src_pane (TE wrapper), draw_qd_real, win_editor, menus, app, file_io's Standard File parts.
- **Group 5** — resources: armadillo.r complete with all MBAR/MENU/ALRT/STR#/ICN# entries.
- **Group 6** — smoke test: src/smoke_test.c exercising edit → parse → render → save.
- **Group 7** — docs: README.md, CLAUDE.md, acceptance checklist.

---

## Appendix A — Key decisions log

Decisions made during brainstorming on 2026-04-19 that shape this design. Each entry names *what* was decided, *why*, and *what was rejected.*

1. **Target floor: 68030 + System 7.x.** Chosen over Quadra-only (narrower install base) and 68000/System 6 (too tight on mem + API drift). Practical middle ground. 32-bit-clean code, no 68040-only instructions, FPU not assumed.
2. **Markdown parser: md4c, vendored.** SAX-style callbacks, caller-controlled allocation, C89, single .c file, no external deps. Plays cleanly with Mac Memory Manager.
3. **HTML rendering: our own module, MVP ships it as literal-text passthrough.** Rejected: gumbo-parser (too heavy), NetSurf's libhubbub (too heavy, C99, own allocator conventions), litehtml (C++, doesn't cleanly link with Retro68), picohttpparser (wrong — it's an HTTP header parser, not HTML). Tier 2 adds a purpose-built ~300-line whitelist tokenizer limited to ~15 tags, feeding the same block/inline engine as markdown.
4. **MVP scope: Tier 0 + Tier 1.** Rejected: including Tier 2 HTML (bigger first spec, doubles surface area before first ship); including Tier 3 palette+status-bar (slow TDD feedback loop); V1-only demo with fixed content (throwaway).
5. **Source-pane text engine: Mac Toolbox TextEdit for MVP, with `kMaxDocBytes = 32767`.** Rejected: WASTE (source-available freeware, dead upstream since 2006, non-OSI license incompatible with MIT/BSD/GPL redistribution); custom piece-table from day 1 (+4–6 weeks to MVP); MLTE (requires OS 8.5+, outside our floor). Custom engine lands in a post-Tier-6 change behind the stable `src_pane.h` API.
6. **Renderer architecture: Approach A — single parse, two outputs, flat-block model, mockable QuickDraw emitter.** Rejected: Approach B (two parses — risks semantic drift); Approach C (lazy Read parse — mode-toggle hitch + blocks future Split mode).
7. **Flat-block model over a tree.** Nesting via `list_depth` / `quote_depth` scalars. Rejected tree: recursive allocation, no rendering benefit, harder arena policy.
8. **`unsigned short` offsets, not `size_t`.** Saves ~4 bytes per block; bounded by TE's 32,767-byte ceiling. Revisit when custom engine lifts the cap.
9. **Synchronous debounced parse, 250 ms idle debounce.** Rejected: per-keystroke parse (100–400 ms per keystroke on 25 MHz); lazy mode-toggle parse (UX hitch); Thread Manager async (7.5+ only, synchronization cost).
10. **Arena policy: single `HLock`ed `Handle`, 4-byte aligned bump allocator, grow-before-parse only, 512 KB hard cap, reset-don't-free between parses.** Rejected: free-list / per-object free (unneeded); two-arena double buffering (simpler to carry "parse failure = blank Read pane" in MVP); `NewPtr` allocations (Memory Manager wants movable).
11. **Per-module error enums, remapped at boundaries.** Rejected: one project-wide enum (grows unboundedly, couples modules).
12. **Goto-cleanup pattern for resource safety.** Matches hello-world reference.
13. **Three test seams: `MacSyscalls`, `DrawOps`, `MdParseSink`.** Hand-written fakes; no mocking framework.
14. **Tests-as-documentation convention.** Sentence-length test names, concept header blocks, Mac OS concept explanations.
15. **Module layout rule: big features live in their own top-level folder alongside `src/`.** `src_pane/`, `render/`, `mdparse/`, `scanner/` are peers. `src/` holds app shell and small glue only.
16. **Vendor-free public headers rule.** No Toolbox / 3rd-party types in any public `.h` across module boundaries. Opaque pointers + project-owned types only. Confirmed explicitly for `src_pane.h`: "let's keep src_pane.h vendor free then and we can pivot to our TE or WASTE-like implementation in next iterations."
17. **Naming: `src_pane/` for the source-pane module.** Over `pane/` (mild asymmetry risk if a future Read-pane module emerges) and `editor/` (clashes with app name). "src_pane" = "source pane" (wireframe's term), not "source-directory pane."
18. **CMake + Makefile.hosttests split.** One CMakeLists for 68k cross-compile .APPL; a plain Makefile for host unit tests. Matches hello-world's pattern of `cc -o build/..._test` invocations.

---

## Appendix B — Open items (none at design time)

All design-level decisions are made. Implementation will surface new choices (e.g., exact font sizes per block kind, exact error-alert wording, exact menu keyboard shortcuts) that fit inside the frame established here. Those are noted in `tasks.md` and resolved in the course of implementation.

If a new architectural concern arises during implementation, it belongs in this design doc — appended as a new section with a date stamp. Don't silently mutate existing sections; add to the log so auditability is preserved.

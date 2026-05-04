# Delta Spec: render

## ADDED Requirements


### Requirement: Arena allocator

The module SHALL provide a `Handle`-backed arena allocator in `render/arena.h`:

```c
typedef struct Arena Arena;
int    arena_init(Arena**, size_t initial_size, const MacSyscalls*);
void   arena_destroy(Arena*);
int    arena_ensure(Arena*, size_t bytes_needed);
void*  arena_alloc(Arena*, size_t n_bytes);
void   arena_reset(Arena*);
size_t arena_high_water(const Arena*);
size_t arena_capacity(const Arena*);
```

The arena SHALL:

1. Back itself with a single `Handle` obtained via `MacSyscalls.new_handle`.
2. `HLock` the handle once at init and keep it locked for the arena's entire lifetime.
3. Return 4-byte-aligned pointers from `arena_alloc`.
4. Use `arena_ensure` to grow the handle via `SetHandleSize` BEFORE any allocations in a cycle — growth SHALL NOT happen during `arena_alloc`.
5. Double its size up to 64 KB, then grow by 32 KB increments, capped at 512 KB (`kArenaHardCap`).
6. Preserve state on grow failure: high_water and existing allocations remain valid.
7. Zero its high_water but keep its backing memory on `arena_reset`.
8. Call `DisposeHandle` exactly once on `arena_destroy`.

#### Scenario: Allocation returns aligned pointer
- GIVEN an arena with capacity > 8 bytes and high_water == 0
- WHEN `arena_alloc(a, 5)` is called
- THEN the returned pointer's address is divisible by 4
- AND `arena_high_water(a)` returns 8 (5 rounded up to 4-byte alignment)

#### Scenario: Grow failure preserves state
- GIVEN `MacSyscalls.set_handle_size` returns failure
- WHEN `arena_ensure(a, 10000)` is called on an arena with capacity 1000 and high_water 500
- THEN `arena_ensure` returns non-zero
- AND `arena_capacity(a) == 1000` and `arena_high_water(a) == 500`

#### Scenario: Reset preserves capacity
- GIVEN an arena with capacity 4096 and high_water 2000
- WHEN `arena_reset(a)` is called
- THEN `arena_high_water(a) == 0` and `arena_capacity(a) == 4096`

### Requirement: Flat block model

The render model SHALL be a flat array of `Block` structs with nesting expressed via scalar `list_depth` and `quote_depth` fields — no tree structure.

```c
typedef struct RenderModel RenderModel;
RenderModel*         render_model_new(Arena*);
const MdParseSink*   render_model_sink(RenderModel*);
size_t               render_model_block_count(const RenderModel*);
const Block*         render_model_block_at(const RenderModel*, size_t);
```

`render_model_new` SHALL allocate the `RenderModel` inside the given arena; the model has no lifetime beyond the arena's current reset cycle. `render_model_sink` returns an `MdParseSink` whose callbacks accumulate blocks, style runs, text slices, and link URLs into the arena.

#### Scenario: Single heading produces one block
- GIVEN `on_block_open(kBlockHeading, h_level=1)`, `on_text("Hello", 5, 2)`, `on_block_close(kBlockHeading)` fed to the sink
- WHEN `render_model_block_count(m)` is called
- THEN it returns 1
- AND `render_model_block_at(m, 0)->kind == kBlockHeading`
- AND `render_model_block_at(m, 0)->h_level == 1`
- AND `render_model_block_at(m, 0)->text_length == 5`

#### Scenario: Nested list preserves depth on each block
- GIVEN sink events for `- a\n  - b\n` (list_depth 1 then 2)
- WHEN the model is built
- THEN `blocks[0].list_depth == 1` and `blocks[1].list_depth == 2`

### Requirement: Style runs per block

Each `Block` SHALL carry an array of `MdStyleRun` tuples describing inline styling within the block's text. The runs MUST be arena-allocated; each run's `start` and `length` MUST be relative to `Block.text`, not to the original source buffer.

#### Scenario: Paragraph with one bold span
- GIVEN source `Hello **world** end` producing one paragraph block
- WHEN the model is built
- THEN that block's `run_count == 1` and `runs[0].kind == kStyleStrong`
- AND `runs[0].start == 6` and `runs[0].length == 9` (offsets relative to the block's text)

### Requirement: Link table (deferred — Tier 2+)

**Status: not in MVP.** For MVP, `kStyleLink` runs SHALL carry `link_index = -1` and no link table SHALL be maintained. Link click-through and URL storage land in a post-Tier-6 change (see `PRD.md` non-goals). MVP renders link text with the link style (sky-blue + underlined) but MUST NOT resolve URLs.

The eventual model SHALL maintain a separate arena-allocated link table mapping `link_index` values to URL strings. `kStyleLink` runs' `link_index` field indexes into this table. URLs are stored with their lengths (not NUL-terminated).

#### Scenario: Link run resolves to URL (deferred)
- GIVEN a paragraph with one `[text](http://ex.com)` link
- WHEN the model is built
- THEN the paragraph's single `MdStyleRun` has `link_index == 0`
- AND the link table entry at index 0 contains "http://ex.com" with length 13

### Requirement: `DrawOps` vtable

The layout + draw pass SHALL emit all graphical output through a `DrawOps` vtable:

```c
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

The renderer SHALL NOT call QuickDraw directly. Production `DrawContext` wraps real QuickDraw (`src/draw_qd.c`); tests wrap a recording sink.

#### Scenario: Renderer calls only through DrawOps
- GIVEN a test run with a recording `DrawContext`
- WHEN `render_layout_and_draw` is invoked on any non-empty model
- THEN every graphical operation appears in the recorded call array (no lost operations)

### Requirement: Block layout and drawing

`render_layout_and_draw(model, params, ctx)` SHALL walk `model`'s blocks in order, positioning each block vertically based on its predecessor's bottom + block-kind-specific spacing, and emit draw calls for each block per this table:

| BlockKind         | Font        | Size | Face     | Color   | Prefix / Chrome                                   |
|-------------------|-------------|------|----------|---------|---------------------------------------------------|
| kBlockParagraph   | Geneva      | 12   | plain    | ink     | none                                              |
| kBlockHeading H1  | Chicago     | 17   | bold     | ink     | none                                              |
| kBlockHeading H2  | Chicago     | 14   | bold     | ink     | none                                              |
| kBlockHeading H3  | Chicago     | 12   | bold     | ink     | none                                              |
| kBlockHeading H4–H6 | Chicago   | 12   | plain    | ink     | none                                              |
| kBlockListItem    | Geneva      | 12   | plain    | ink     | ASCII `*` bullet at indent_list * list_depth (MVP); per-depth glyphs (•, ◦, ▪) land in Tier 2 |
| kBlockBlockQuote  | Geneva      | 12   | plain    | ink     | vertical bar at left; indent_quote * quote_depth   |
| kBlockCodeBlock   | Monaco      | 10   | plain    | ink     | background rectangle (lightgrey)                   |
| kBlockHr          | —           | —    | —        | ink     | horizontal line across content_width (MVP)         |
| kBlockHtml        | Monaco      | 10   | plain    | purple  | raw text (MVP); whitelist rendering in Tier 2      |

Per-block horizontal offset = `indent_list * list_depth + indent_quote * quote_depth + 8px (margin)`. Per-block vertical spacing = `line_height * 0.5` between blocks (rounded to an integer number of pixels).

#### Scenario: Paragraph draws Geneva 12 plain
- GIVEN a model with one kBlockParagraph of text "hello"
- WHEN `render_layout_and_draw` is called with a recording DrawContext
- THEN recorded calls include `set_font(Geneva, 12, plain)`, `move_to(x, y)`, `draw_text("hello", 5)` in order

#### Scenario: Horizontal rule draws a line
- GIVEN a model with one kBlockHr and `params.content_width == 400`
- WHEN rendered
- THEN recorded calls include `line(x, y, x + 400, y)` where x matches the content's left margin

### Requirement: Inline style run application

During block drawing, the renderer SHALL apply each `MdStyleRun` by emitting `set_font` / `set_fg` calls before the run's text and restoring the block's default style after. Inline run styles are the same mapping the source pane uses (see `src-pane` spec), with the exception that `kStyleLink` draws in the link color and underlined face.

#### Scenario: Bold run in a paragraph
- GIVEN a kBlockParagraph with text "plain bold rest" and run `{start=6, length=4, kind=kStyleStrong}`
- WHEN rendered
- THEN recorded calls include, in order: `draw_text("plain ", 6)`, `set_font(Geneva, 12, bold)`, `draw_text("bold", 4)`, `set_font(Geneva, 12, plain)`, `draw_text(" rest", 5)`

### Requirement: Word wrap within content_width (deferred — Tier 2+)

**Status: not in MVP.** For MVP, each block's text SHALL be emitted on a single line; text that exceeds `content_width` is allowed to overflow the window's right edge. Wireframe sample content and typical short documents don't trigger this; real wrap lands alongside the custom text engine that lifts TextEdit's 32-KB ceiling (see `PRD.md` tier roadmap).

The eventual requirement: text within a block SHALL wrap at word boundaries when a line exceeds `params.content_width - block's indent`. Lines advance vertically by `line_height` (retrieved via `DrawOps.get_font_metrics` for the current font). Word boundary detection uses ASCII space as the wrap point (CJK is out of scope for MVP).

#### Scenario: Long paragraph wraps to multiple lines (deferred)
- GIVEN a paragraph whose text exceeds `content_width` by 3 words and a font with `line_height == 16`
- WHEN rendered
- THEN two or more `draw_text` calls appear at different vertical y values
- AND y values differ by at least 16 pixels

### Requirement: Layout parameters

The render module SHALL accept layout parameters via the following struct:

```c
typedef struct LayoutParams {
    short content_width;       /* px */
    short indent_list;         /* px per list_depth step */
    short indent_quote;        /* px per quote_depth step */
    short left_margin;         /* px */
    short top_margin;          /* px */
    short block_spacing;       /* px between blocks */
} LayoutParams;
```

A `LayoutParams` defaults helper SHALL exist (`layout_params_defaults`) returning sensible values for a 480-px window: content_width=464, indent_list=14, indent_quote=8, left_margin=8, top_margin=8, block_spacing=6.

#### Scenario: Defaults helper returns stable values
- GIVEN no input other than `layout_params_defaults()`
- WHEN called
- THEN the returned struct has `content_width == 464` and `indent_list == 14`

### Requirement: Host testability via recording emitter

The render module SHALL be fully host-testable. Tests construct a `DrawContext` backed by a `Recorder` that appends every vtable call to a growable array, then assert on the recorded sequence. No Toolbox dependency.

#### Scenario: Test suite runs on host
- GIVEN `render/render_test.c` compiled with host `cc`
- WHEN the test suite is executed
- THEN all tests pass without requiring Retro68 or a running Toolbox

### Requirement: Error reporting

The render module SHALL surface failures via the following error enum:

```c
typedef enum {
    kRenderOk             =  0,
    kRenderErrArenaOOM    = -1,
    kRenderErrBadModel    = -2,
    kRenderErrEmitAborted = -3,
} RenderError;
```

Layout failures SHALL return these error codes. Arena OOM during model construction surfaces via the sink's return value (the sink returns non-zero, `mdparse_run` returns `kMdParseErrArenaOOM`).

#### Scenario: Model construction under arena OOM
- GIVEN an arena pre-sized too small for the input source
- WHEN `mdparse_run` drives the render model's sink and the arena exhausts
- THEN the sink returns non-zero and `mdparse_run` returns `kMdParseErrArenaOOM`
- AND `current_model` remains NULL in the caller

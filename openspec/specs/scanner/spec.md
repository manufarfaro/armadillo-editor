# scanner Specification

## Purpose

Produce a flat array of `MdStyleRun` tuples from an `MdParseSink` event stream, for application to the source pane's text as syntax coloring. The scanner is the simpler of the two downstream consumers of `mdparse` (the other being `render`). It does not build a model of the document — it only records which ranges of the source text should be drawn in which style, so `src_pane_apply_runs` can translate them into `TESetStyle` calls.

This capability gives us the wireframe's colored markdown source (headings bold, `**bold**` in teal, `_italic_` in salmon, `[links](url)` sky-blue underlined, HTML in purple, inline `` `code` `` in darker teal) without requiring the source pane to know anything about markdown.

## Requirements

### Requirement: Opaque handle

The `Scanner` type SHALL be declared as an opaque struct in `scanner/scanner.h`. The public header SHALL include only `<stddef.h>`, `render/inlines.h` (for `MdStyleRun`), `mdparse/mdparse.h` (for `MdParseSink`), and `render/arena.h` (for `Arena`).

#### Scenario: Opaque handle
- GIVEN `scanner/scanner.h`
- WHEN inspected
- THEN `struct Scanner` has no field definitions outside `scanner.c`

### Requirement: Arena-backed allocation

The scanner SHALL allocate all `MdStyleRun` storage out of an `Arena*` provided at construction time. The scanner SHALL NOT call `malloc`, `NewHandle`, `NewPtr`, or any other allocator directly.

```c
Scanner* scanner_new(Arena*);
void     scanner_free(Scanner*);
```

`scanner_free` SHALL NOT call `arena_destroy` — it merely releases the `Scanner*` struct itself (if heap-allocated) and NULLs the internal pointer. The caller owns the arena's lifetime.

#### Scenario: No direct allocation
- GIVEN the scanner receives 100 span events
- WHEN the resulting `MdStyleRun` array is inspected
- THEN every run's pointer lies within the arena's address range (not on the system heap)

### Requirement: Sink interface

The scanner SHALL expose a `MdParseSink` that can be plugged into `mdparse_run`:

```c
const MdParseSink* scanner_sink(Scanner*);
```

The returned pointer is valid for the lifetime of the `Scanner`. Its callbacks:

- `on_block_open`: if `kBlockHeading`, records a style-run covering the heading line's text in a heading-specific style (optional — MAY be implemented as per-heading-level styling); otherwise no-op.
- `on_block_close`: no-op for most kinds; for `kBlockHtml`, closes an active HTML-block style-run.
- `on_span`: records a `MdStyleRun` for the span's range (start, length, kind). For `kStyleLink`, the `link_url` is ignored (the source pane doesn't render URLs differently from the link text).
- `on_text`: typically no-op; the scanner cares about style spans, not text content.

#### Scenario: Scanner records a bold span
- GIVEN an empty scanner
- WHEN `on_span(ctx, kStyleStrong, 4, 6, NULL, 0)` fires on the scanner's sink
- THEN `scanner_runs(s, &count)` returns `count == 1` and `runs[0] == {start=4, length=6, kind=kStyleStrong, link_index=-1}`

### Requirement: Run accumulation and retrieval

After an `mdparse_run` call that used the scanner's sink, the caller SHALL retrieve the recorded runs:

```c
const MdStyleRun* scanner_runs(const Scanner*, size_t* out_count);
```

The returned pointer is valid until the next `scanner_reset` or arena reset. Runs are in the order recorded (which equals input-source order for non-nested spans).

#### Scenario: Empty input produces zero runs
- GIVEN a fresh scanner
- WHEN no sink callbacks have fired
- THEN `scanner_runs(s, &count)` returns any non-NULL pointer with `count == 0`

#### Scenario: Multiple spans record in order
- GIVEN a scanner receiving two `on_span` events: first at start=2, then at start=10
- WHEN `scanner_runs` is called
- THEN `runs[0].start == 2` and `runs[1].start == 10`

### Requirement: Reset

```c
void scanner_reset(Scanner*);
```

`scanner_reset` SHALL clear the recorded runs, making the scanner ready for a new parse cycle. It SHALL NOT free arena memory — the arena is reset separately by the caller (`arena_reset` in `run_parse_cycle`).

#### Scenario: Reset clears runs
- GIVEN a scanner with 5 recorded runs
- WHEN `scanner_reset(s)` is called
- THEN `scanner_runs(s, &count)` returns `count == 0`

### Requirement: Nested span handling

When a span nests inside another (e.g., `**bold _italic_ text**`), the scanner SHALL record one run per span, in the order their `on_span` events fire. No merging, no subtraction — the source pane's apply policy decides how overlapping runs compose.

#### Scenario: Bold containing italic
- GIVEN source `**bold _it_ end**` with md4c emitting outer strong first, then inner emph
- WHEN the scanner processes the sink stream
- THEN runs contain both: `{start=0, length=14, kind=kStyleStrong}` and `{start=7, length=3, kind=kStyleEmph}`

### Requirement: HTML-span handling

For `on_span(kStyleHtmlSpan, ...)` events, the scanner SHALL record a run with `kind=kStyleHtmlSpan`. This styles the raw HTML text in the source pane (purple in the default theme). The scanner SHALL NOT attempt to parse the HTML — it merely classifies the range.

For `on_block_open(kBlockHtml)`, the scanner SHALL enter "HTML-block mode": on the *next* `on_text` event received while in this mode, record the event's `source_offset` as `html_block_start`. On every subsequent `on_text` while in HTML-block mode, update `html_block_end = source_offset + length`. On `on_block_close(kBlockHtml)`, emit one `MdStyleRun` with `start = html_block_start`, `length = html_block_end - html_block_start`, `kind = kStyleHtmlSpan`, and exit HTML-block mode. If no `on_text` events fire between open and close (empty HTML block), no run is emitted.

The scanner SHALL track HTML-block mode with a single flag plus two offset fields. `BlockAttrs` does not carry source offsets (md4c's API does not supply block-start offsets directly), so the text-event-based tracking is the accurate mechanism.

#### Scenario: HTML block styles the whole block
- GIVEN source containing `<aside>\n  <p>x</p>\n</aside>` where md4c emits `on_block_open(kBlockHtml)`, then `on_text("<aside>\n  <p>x</p>\n</aside>", 25, source_offset=12)`, then `on_block_close(kBlockHtml)`
- WHEN the scanner processes this sequence
- THEN exactly one `MdStyleRun` is recorded with `start=12, length=25, kind=kStyleHtmlSpan`

#### Scenario: Multi-line HTML block spans all text events
- GIVEN an HTML block split across multiple `on_text` events: `on_text("<aside>", 7, 10)`, `on_text("  <p>x</p>", 10, 18)`, `on_text("</aside>", 8, 29)`
- WHEN the scanner processes open → these three text events → close
- THEN exactly one `MdStyleRun` is recorded with `start=10, length=27` (from offset 10 through offset 29+8=37)

#### Scenario: Empty HTML block emits no run
- GIVEN `on_block_open(kBlockHtml)` immediately followed by `on_block_close(kBlockHtml)` with no text events
- WHEN the scanner processes this sequence
- THEN no `MdStyleRun` is recorded for the block

### Requirement: Link index

Scanner-recorded `kStyleLink` runs SHALL use `link_index = -1` (URLs are not rendered differently in the source pane). The `render` module uses `link_index` to look up URLs in its own link table for the Read pane; scanner does not participate in that.

#### Scenario: Link run has -1 index
- GIVEN `on_span(kStyleLink, start=0, length=10, "http://x", 8)`
- WHEN the scanner records the run
- THEN the recorded run has `link_index == -1`

### Requirement: Host testability

The scanner SHALL be fully host-testable. Tests SHALL feed synthetic `MdParseSink` events directly (bypassing md4c) and assert on the accumulated run array.

#### Scenario: Synthetic events produce expected runs
- GIVEN a host test that constructs a `Scanner`, retrieves its sink, and invokes `on_span(kStyleEmph, 3, 4, NULL, 0)` directly
- WHEN the test calls `scanner_runs`
- THEN the returned array contains `{start=3, length=4, kind=kStyleEmph, link_index=-1}`

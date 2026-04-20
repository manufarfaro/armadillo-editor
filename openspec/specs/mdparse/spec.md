# mdparse Specification

## Purpose

Provide a project-owned adapter over the vendored md4c markdown parser. The adapter hides md4c's types and API from the rest of the codebase, presents a stable `MdParseSink` callback interface, and fans out a single parse to any number of consumers (scanner, render, future HTML whitelist renderer, etc.) in a single pass.

This capability exists so that (a) no other module imports md4c types, which protects us from md4c API changes, and (b) multiple downstream consumers can share a single parse cycle rather than each parsing independently — critical for performance on 25 MHz 68030.

## Requirements

### Requirement: Vendor-free public interface

The `mdparse/mdparse.h` header SHALL NOT include `md4c.h`, expose `MD_BLOCK_TYPE`, `MD_SPAN_TYPE`, `MD_TEXTTYPE`, `MD_CHAR`, or any other md4c type. Callers SHALL see only project-owned types (`BlockKind`, `StyleKind`, `BlockAttrs`, `MdParseSink`, `MdParseError`).

#### Scenario: No md4c symbols in the header
- GIVEN `mdparse/mdparse.h`
- WHEN inspected for `MD_` identifier prefixes
- THEN zero matches are found

### Requirement: `MdParseSink` callback interface

The module SHALL define a callback struct that consumers implement:

```c
typedef struct BlockAttrs {
    unsigned char  h_level;        /* 1..6 for kBlockHeading, 0 otherwise */
    unsigned char  list_depth;     /* 0 = not in list; 1+ = nesting depth */
    unsigned char  quote_depth;    /* 0 = not in quote; 1+ = nesting depth */
    unsigned char  list_ordered;   /* 0 = bullet, 1 = numbered */
} BlockAttrs;

typedef struct MdParseSink {
    int  (*on_block_open)(void* ctx, BlockKind kind, const BlockAttrs* attrs);
    int  (*on_block_close)(void* ctx, BlockKind kind);
    int  (*on_span)(void* ctx, StyleKind kind,
                    unsigned short start, unsigned short length,
                    const char* link_url, unsigned short link_url_len);
    int  (*on_text)(void* ctx, const char* bytes, unsigned short length,
                    unsigned short source_offset);
    void* ctx;
} MdParseSink;
```

Callbacks return 0 for continue, non-zero to abort. If any callback returns non-zero, `mdparse_run` SHALL stop processing further events, return `kMdParseErrSinkAbort`, and not invoke subsequent sinks for the event currently in flight.

#### Scenario: Sink abort propagates
- GIVEN a sink whose `on_block_open` returns non-zero on the first call
- WHEN `mdparse_run` is invoked with that sink
- THEN `mdparse_run` returns `kMdParseErrSinkAbort`
- AND no further `on_text` or `on_span` callbacks fire

### Requirement: Fan-out to multiple sinks

The module SHALL provide:

```c
int mdparse_run(const char* source, unsigned short source_len,
                const MdParseSink* sinks, size_t sink_count);
```

For each md4c event, `mdparse_run` SHALL dispatch to every sink in `sinks[0..sink_count-1]` in array order before advancing to the next event. Sinks do not communicate; the fan-out is strictly one-way.

#### Scenario: Two sinks both receive events
- GIVEN two sinks that each count `on_text` calls
- WHEN `mdparse_run` is invoked with source containing 3 text events
- THEN both sinks' counters equal 3

#### Scenario: Array order is preserved
- GIVEN two sinks that record the order they were called
- WHEN `mdparse_run` fires a single event
- THEN sink[0] is called before sink[1] for that event

### Requirement: Block kind mapping

The adapter SHALL map md4c's `MD_BLOCKTYPE` values to project `BlockKind` values:

| md4c block        | Project BlockKind    |
|-------------------|----------------------|
| MD_BLOCK_P        | kBlockParagraph      |
| MD_BLOCK_H        | kBlockHeading        |
| MD_BLOCK_LI       | kBlockListItem       |
| MD_BLOCK_QUOTE    | *not emitted as block; tracked for quote_depth only* |
| MD_BLOCK_CODE     | kBlockCodeBlock      |
| MD_BLOCK_HR       | kBlockHr             |
| MD_BLOCK_HTML     | kBlockHtml           |
| MD_BLOCK_UL/OL    | *not emitted as block; tracked for list_depth/ordered only* |
| MD_BLOCK_DOC      | *not emitted*        |

`MD_BLOCK_UL` and `MD_BLOCK_OL` increment the `list_depth` counter on open and decrement on close; they do NOT produce `on_block_open` callbacks themselves. The `list_depth` and `list_ordered` from the enclosing UL/OL are stamped onto each contained `MD_BLOCK_LI` when it opens.

Similarly, `MD_BLOCK_QUOTE` adjusts `quote_depth` for contained blocks but does not emit its own sink event — instead, contained blocks receive the updated `quote_depth` via their `BlockAttrs`.

#### Scenario: Nested list depth
- GIVEN source `- a\n  - b\n`
- WHEN mdparse_run is invoked
- THEN the first `on_block_open(kBlockListItem)` receives `list_depth=1`
- AND the second `on_block_open(kBlockListItem)` receives `list_depth=2`

#### Scenario: Blockquote depth on inner paragraph
- GIVEN source `> > text\n`
- WHEN mdparse_run is invoked
- THEN `on_block_open(kBlockParagraph)` receives `quote_depth=2`

### Requirement: Span kind mapping

The adapter SHALL map md4c's `MD_SPANTYPE` values to project `StyleKind` values:

| md4c span         | Project StyleKind    |
|-------------------|----------------------|
| MD_SPAN_EM        | kStyleEmph           |
| MD_SPAN_STRONG    | kStyleStrong         |
| MD_SPAN_CODE      | kStyleCodeSpan       |
| MD_SPAN_A         | kStyleLink (with link_url + link_url_len populated from MD_SPAN_A_DETAIL) |

`MD_SPAN_A_DETAIL.href` SHALL be passed as `link_url` with its length; for non-link spans, `link_url` is NULL and `link_url_len` is 0.

#### Scenario: Link span carries URL
- GIVEN source `[text](http://example.com)`
- WHEN mdparse_run invokes `on_span(kStyleLink, ...)`
- THEN `link_url` points to `"http://example.com"` and `link_url_len == 18`

### Requirement: Text offsets into source

The `on_text` callback's `source_offset` SHALL be the byte offset into the original source buffer where the text starts. This enables consumers to compute style-run ranges against the user's source text (not the post-escape-processing text that md4c produces).

For text that md4c unescapes (e.g., `&amp;` → `&`), the `bytes` pointer refers to md4c's internal buffer with the unescaped content, but `source_offset` still points to the ORIGINAL escaped position in the user's source.

#### Scenario: Offset matches source for plain text
- GIVEN source `"abc def"` (7 bytes) and md4c emits text event for "def" starting at byte 4
- WHEN `on_text` fires
- THEN `source_offset == 4` and `length == 3`

### Requirement: Error remapping

The adapter SHALL define `MdParseError`:

```c
typedef enum {
    kMdParseOk           =  0,
    kMdParseErrArenaOOM  = -1,   /* internal allocator OOM */
    kMdParseErrMd4c      = -2,   /* md4c's md_parse returned non-zero */
    kMdParseErrSinkAbort = -3,   /* a sink returned non-zero */
} MdParseError;
```

`mdparse_run` SHALL translate internal errors (md4c return codes, arena failures from sinks that use the arena) into these values. Callers never see md4c-specific error codes.

#### Scenario: md4c failure maps cleanly
- GIVEN md4c's `md_parse` returns -1
- WHEN `mdparse_run` is invoked
- THEN `mdparse_run` returns `kMdParseErrMd4c`

### Requirement: Zero parser state leak between calls

Consecutive calls to `mdparse_run` SHALL NOT leak state: the second call's events MUST NOT be influenced by the first call's input. The adapter SHALL not hold static parser state; any depth counters are reset at the start of each call.

#### Scenario: Repeated invocations are independent
- GIVEN two calls to mdparse_run with different source buffers and the same sink
- WHEN the second call runs
- THEN the event sequence depends only on the second source, not on the first

### Requirement: Host testability

`mdparse_run` SHALL compile cleanly on the host and exercise md4c fully. Tests SHALL use real md4c with synthetic markdown fixtures and assert on the recorded sink event sequences via a test-provided sink that appends each callback to an array.

#### Scenario: Host test runs md4c
- GIVEN `mdparse/mdparse_test.c` links against md4c.c
- WHEN the test calls `mdparse_run("# H", 3, sinks, 1)`
- THEN the recording sink captures `[on_block_open(kBlockHeading, h_level=1), on_text("H", 1, 2), on_block_close(kBlockHeading)]`

/*
 * scanner/scanner.c — style-run producer for the source pane.
 *
 * Consumes MdParseSink events and appends StyleRun tuples to an
 * arena-allocated array. Every run is (start, length, kind, link_index).
 *
 * Span events map 1:1 to runs (one event = one run, same byte range).
 * HTML blocks need special handling: on_block_open(kBlockHtml) doesn't
 * carry a source_offset, so we enter "HTML-block mode" and track the
 * min/max source_offset of enclosed on_text events. On on_block_close
 * we emit a single run covering (min, max - min, kStyleHtmlSpan).
 *
 * Run array grows in doubling chunks via arena_ensure + arena_alloc.
 * Old runs must be copied into the new buffer because arena_alloc hands
 * out fresh memory each call; we can't just grow in place.
 *
 * See openspec/specs/scanner/spec.md for the authoritative requirements
 * and Given/When/Then scenarios.
 */
#include "scanner.h"
#include <string.h>

struct Scanner {
    Arena*          arena;
    StyleRun*       runs;           /* arena-allocated, grown in chunks */
    size_t          run_count;
    size_t          run_capacity;

    int             in_html_block;
    int             html_seen_any_text;
    unsigned short  html_block_start;
    unsigned short  html_block_end;
};

static int scanner_grow(Scanner* s, size_t needed) {
    size_t next;
    size_t bytes;
    StyleRun* newbuf;

    if (needed <= s->run_capacity) return 0;
    next = s->run_capacity ? s->run_capacity : 16;
    while (next < needed) next *= 2;
    bytes = next * sizeof(StyleRun);
    if (arena_ensure(s->arena, bytes) != 0) return -1;
    newbuf = (StyleRun*)arena_alloc(s->arena, bytes);
    if (!newbuf) return -1;
    if (s->runs && s->run_count > 0) {
        memcpy(newbuf, s->runs, s->run_count * sizeof(StyleRun));
    }
    s->runs = newbuf;
    s->run_capacity = next;
    return 0;
}

static int scanner_push(Scanner* s, StyleRun r) {
    if (scanner_grow(s, s->run_count + 1) != 0) return -1;
    s->runs[s->run_count++] = r;
    return 0;
}

/* -------- MdParseSink callbacks -------- */

static int sc_block_open(void* ctx, BlockKind k, const BlockAttrs* a) {
    Scanner* s = (Scanner*)ctx;
    (void)a;
    if (k == kBlockHtml) {
        s->in_html_block = 1;
        s->html_seen_any_text = 0;
        s->html_block_start = 0;
        s->html_block_end = 0;
    }
    return 0;
}

static int sc_block_close(void* ctx, BlockKind k) {
    Scanner* s = (Scanner*)ctx;
    if (k == kBlockHtml && s->in_html_block) {
        if (s->html_seen_any_text) {
            StyleRun r;
            r.start = s->html_block_start;
            r.length = (unsigned short)(s->html_block_end - s->html_block_start);
            r.kind = kStyleHtmlSpan;
            r.link_index = -1;
            scanner_push(s, r);
        }
        s->in_html_block = 0;
    }
    return 0;
}

static int sc_span(void* ctx, StyleKind k, unsigned short start,
                   unsigned short length, const char* url,
                   unsigned short url_len) {
    Scanner* s = (Scanner*)ctx;
    StyleRun r;
    (void)url; (void)url_len;   /* scanner doesn't track URLs */
    r.start = start;
    r.length = length;
    r.kind = k;
    r.link_index = -1;
    return scanner_push(s, r);
}

static int sc_text(void* ctx, const char* bytes, unsigned short length,
                   unsigned short source_offset) {
    Scanner* s = (Scanner*)ctx;
    (void)bytes;
    if (s->in_html_block) {
        if (!s->html_seen_any_text) {
            s->html_block_start = source_offset;
            s->html_seen_any_text = 1;
        }
        s->html_block_end = (unsigned short)(source_offset + length);
    }
    return 0;
}

/* -------- Public API -------- */

Scanner* scanner_new(Arena* a) {
    Scanner* s;
    if (!a) return 0;
    if (arena_ensure(a, sizeof(Scanner)) != 0) return 0;
    s = (Scanner*)arena_alloc(a, sizeof(Scanner));
    if (!s) return 0;
    memset(s, 0, sizeof *s);
    s->arena = a;
    return s;
}

void scanner_free(Scanner* s) {
    /* Arena-backed: nothing to free here. Resources live until
     * arena_destroy. This is a no-op for now but kept in the API so
     * future internals (e.g., if we ever hold an OS resource) have a
     * place to release it. */
    (void)s;
}

const MdParseSink* scanner_sink(Scanner* s) {
    MdParseSink* sink;
    if (!s) return 0;
    if (arena_ensure(s->arena, sizeof(MdParseSink)) != 0) return 0;
    sink = (MdParseSink*)arena_alloc(s->arena, sizeof(MdParseSink));
    if (!sink) return 0;
    sink->on_block_open  = sc_block_open;
    sink->on_block_close = sc_block_close;
    sink->on_span        = sc_span;
    sink->on_text        = sc_text;
    sink->ctx            = s;
    return sink;
}

const StyleRun* scanner_runs(const Scanner* s, size_t* out_count) {
    if (out_count) *out_count = s ? s->run_count : 0;
    return s ? s->runs : 0;
}

void scanner_reset(Scanner* s) {
    if (!s) return;
    s->runs = 0;
    s->run_count = 0;
    s->run_capacity = 0;
    s->in_html_block = 0;
    s->html_seen_any_text = 0;
    s->html_block_start = 0;
    s->html_block_end = 0;
}

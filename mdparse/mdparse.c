/*
 * mdparse/mdparse.c — md4c adapter with MdParseSink fan-out.
 *
 * md4c's API:
 *   int md_parse(const MD_CHAR* text, MD_SIZE size,
 *                const MD_PARSER* parser, void* userdata);
 *
 * md4c invokes the parser's enter_block / leave_block / enter_span /
 * leave_span / text callbacks for events it detects. Our adapter
 * maintains a depth counter for UL/OL/BLOCKQUOTE (md4c emits open/close
 * events for these containers) and stamps the depth onto each contained
 * block's BlockAttrs.
 *
 * Sinks: we fan out to every sink in array order for every event. If
 * any sink returns non-zero, we stop the parse and return
 * kMdParseErrSinkAbort.
 *
 * Source offsets: md4c does not expose source offsets on enter_block /
 * enter_span. We track a running cursor (last_source_offset) that
 * advances by text-event length, giving consumers an approximate
 * offset useful for scanner-style range tracking of HTML blocks. It is
 * NOT a precise source column — that's a later-phase concern.
 */
#include "mdparse.h"
#include "md4c.h"
#include <string.h>

/* Per-parse state. Stack-allocated inside mdparse_run; fresh per call,
 * so state isolation between invocations is structural — no reset step
 * required and no global state to corrupt. */
typedef struct Dispatcher {
    const MdParseSink* sinks;                  /* fan-out target array */
    size_t             sink_count;             /* length of sinks */
    unsigned char      list_depth;             /* current list nesting (0 = not in list) */
    unsigned char      quote_depth;            /* current blockquote nesting */
    unsigned char      list_ordered_stack[16]; /* 0 = UL, 1 = OL; top = innermost list */
    unsigned char      list_stack_top;         /* number of open lists; saturates at 16 */
    unsigned short     last_source_offset;     /* running source-byte cursor (see file header) */
    int                aborted;                /* set when a sink returned non-zero */
} Dispatcher;

/* -------- Fan-out helpers ---------------------------------------------
 * One helper per MdParseSink callback. All four share the same shape:
 * iterate sinks in array order, invoke the relevant callback, and on
 * any non-zero return set d->aborted and bail. Subsequent md4c
 * callbacks that fire while md4c unwinds its own state will short-
 * circuit on the aborted flag.
 * --------------------------------------------------------------------- */

static int dispatch_block_open(Dispatcher* d, BlockKind k,
                               const BlockAttrs* a) {
    size_t i;
    for (i = 0; i < d->sink_count; i++) {
        int rc = d->sinks[i].on_block_open(d->sinks[i].ctx, k, a);
        if (rc != 0) { d->aborted = 1; return -1; }
    }
    return 0;
}

static int dispatch_block_close(Dispatcher* d, BlockKind k) {
    size_t i;
    for (i = 0; i < d->sink_count; i++) {
        int rc = d->sinks[i].on_block_close(d->sinks[i].ctx, k);
        if (rc != 0) { d->aborted = 1; return -1; }
    }
    return 0;
}

static int dispatch_span(Dispatcher* d, StyleKind k,
                         unsigned short start, unsigned short length,
                         const char* url, unsigned short url_len) {
    size_t i;
    for (i = 0; i < d->sink_count; i++) {
        int rc = d->sinks[i].on_span(d->sinks[i].ctx, k, start, length,
                                     url, url_len);
        if (rc != 0) { d->aborted = 1; return -1; }
    }
    return 0;
}

static int dispatch_text(Dispatcher* d, const char* bytes,
                         unsigned short length,
                         unsigned short source_offset) {
    size_t i;
    for (i = 0; i < d->sink_count; i++) {
        int rc = d->sinks[i].on_text(d->sinks[i].ctx, bytes, length,
                                     source_offset);
        if (rc != 0) { d->aborted = 1; return -1; }
    }
    return 0;
}

static int on_enter_block(MD_BLOCKTYPE type, void* detail, void* userdata) {
    Dispatcher* d = (Dispatcher*)userdata;
    BlockAttrs a;
    BlockKind k;

    if (d->aborted) return -1;

    /* Containers that adjust depth but emit no user-facing event.
     * list_ordered_stack saturates at 16 entries: list_depth keeps
     * counting past that, but list_ordered on contained blocks will
     * report the 16th-innermost level's flag for any deeper nesting.
     * Real-world markdown rarely exceeds 4–5 levels, so this is fine. */
    if (type == MD_BLOCK_UL) {
        if (d->list_stack_top < 16)
            d->list_ordered_stack[d->list_stack_top++] = 0;
        d->list_depth++;
        return 0;
    }
    if (type == MD_BLOCK_OL) {
        if (d->list_stack_top < 16)
            d->list_ordered_stack[d->list_stack_top++] = 1;
        d->list_depth++;
        return 0;
    }
    if (type == MD_BLOCK_QUOTE) {
        d->quote_depth++;
        return 0;
    }
    /* MD_BLOCK_DOC is md4c's root-document wrapper. Pure scaffolding —
     * no consumer needs it, no depth to track; drop it. */
    if (type == MD_BLOCK_DOC) return 0;

    /* Emitted blocks. */
    memset(&a, 0, sizeof a);
    a.list_depth  = d->list_depth;
    a.quote_depth = d->quote_depth;
    a.list_ordered = (d->list_stack_top > 0)
                     ? d->list_ordered_stack[d->list_stack_top - 1] : 0;

    switch (type) {
        case MD_BLOCK_P:    k = kBlockParagraph;  break;
        case MD_BLOCK_H: {
            MD_BLOCK_H_DETAIL* dh = (MD_BLOCK_H_DETAIL*)detail;
            k = kBlockHeading;
            a.h_level = (unsigned char)(dh ? dh->level : 1);
            break;
        }
        case MD_BLOCK_LI:   k = kBlockListItem;   break;
        case MD_BLOCK_CODE: k = kBlockCodeBlock;  break;
        case MD_BLOCK_HR:   k = kBlockHr;         break;
        case MD_BLOCK_HTML: k = kBlockHtml;       break;
        default:            return 0;
    }
    return dispatch_block_open(d, k, &a);
}

/* Mirror of on_enter_block: pop depth counters on container close,
 * translate leaf-block types, dispatch close to sinks. The `x > 0`
 * underflow guards are defensive — md4c should never close a container
 * that wasn't opened, but if it ever did (pathological input, bug in
 * a future md4c version), decrementing an unsigned counter past zero
 * would wrap to 255 and poison every subsequent block's attrs. */
static int on_leave_block(MD_BLOCKTYPE type, void* detail, void* userdata) {
    Dispatcher* d = (Dispatcher*)userdata;
    BlockKind k;
    (void)detail;

    if (d->aborted) return -1;

    if (type == MD_BLOCK_UL || type == MD_BLOCK_OL) {
        if (d->list_depth > 0) d->list_depth--;
        if (d->list_stack_top > 0) d->list_stack_top--;
        return 0;
    }
    if (type == MD_BLOCK_QUOTE) {
        if (d->quote_depth > 0) d->quote_depth--;
        return 0;
    }
    if (type == MD_BLOCK_DOC) return 0;

    switch (type) {
        case MD_BLOCK_P:    k = kBlockParagraph;  break;
        case MD_BLOCK_H:    k = kBlockHeading;    break;
        case MD_BLOCK_LI:   k = kBlockListItem;   break;
        case MD_BLOCK_CODE: k = kBlockCodeBlock;  break;
        case MD_BLOCK_HR:   k = kBlockHr;         break;
        case MD_BLOCK_HTML: k = kBlockHtml;       break;
        default:            return 0;
    }
    return dispatch_block_close(d, k);
}

static int on_enter_span(MD_SPANTYPE type, void* detail, void* userdata) {
    Dispatcher* d = (Dispatcher*)userdata;
    StyleKind k;
    const char* url = 0;
    unsigned short url_len = 0;

    if (d->aborted) return -1;

    switch (type) {
        case MD_SPAN_EM:     k = kStyleEmph;     break;
        case MD_SPAN_STRONG: k = kStyleStrong;   break;
        case MD_SPAN_CODE:   k = kStyleCodeSpan; break;
        case MD_SPAN_A: {
            MD_SPAN_A_DETAIL* da = (MD_SPAN_A_DETAIL*)detail;
            k = kStyleLink;
            if (da && da->href.text) {
                url = da->href.text;
                url_len = (unsigned short)da->href.size;
            }
            break;
        }
        default: return 0;   /* unsupported span; ignore */
    }
    /* md4c doesn't hand us ranges at enter_span. We emit start =
     * last_source_offset, length = 0. Consumers that need precise
     * ranges (scanner's HTML-block mode) track via text events. */
    return dispatch_span(d, k, d->last_source_offset, 0, url, url_len);
}

/* Deliberately a no-op. Span bounds are reconstructable from the
 * stream of on_text events between enter_span and leave_span, so
 * consumers that need precise ranges (scanner's HTML-block mode) track
 * via on_text rather than needing a leave-span event. md4c requires
 * a non-NULL function pointer for every callback field in MD_PARSER,
 * so this is the bound-but-empty stub. */
static int on_leave_span(MD_SPANTYPE type, void* detail, void* userdata) {
    (void)type; (void)detail; (void)userdata;
    return 0;
}

static int on_text(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size,
                   void* userdata) {
    Dispatcher* d = (Dispatcher*)userdata;
    unsigned short offset;
    int rc;
    (void)type;

    if (d->aborted) return -1;

    /* Snapshot the cursor as THIS text event's offset, dispatch with
     * it, then advance the cursor by the text length. This advance is
     * what gives last_source_offset its "approximately the source
     * position" behavior — see the file header for the drift caveats
     * introduced by MD_FLAG_COLLAPSEWHITESPACE and entity decoding. */
    offset = d->last_source_offset;
    rc = dispatch_text(d, text, (unsigned short)size, offset);
    d->last_source_offset = (unsigned short)(d->last_source_offset + size);
    return rc;
}

int mdparse_run(const char* source, unsigned short source_len,
                const MdParseSink* sinks, size_t sink_count) {
    MD_PARSER parser;
    Dispatcher d;
    int rc;

    /* Empty source is trivially successful — no work to do, no events
     * to fire. */
    if (source_len == 0) return kMdParseOk;

    /* Bad arguments (NULL source, NULL sinks, zero sink count) are
     * reported as kMdParseErrMd4c because the public MdParseError enum
     * has no dedicated invalid-args code. The enum is frozen from
     * Phase 2; when it gets its next change, adding kMdParseErrInvalidArg
     * and routing this case to it would be the cleaner shape. */
    if (!source || !sinks || sink_count == 0) return kMdParseErrMd4c;

    memset(&d, 0, sizeof d);
    d.sinks = sinks;
    d.sink_count = sink_count;

    memset(&parser, 0, sizeof parser);
    parser.abi_version  = 0;
    /* md4c parse flags:
     *   MD_FLAG_PERMISSIVEAUTOLINKS — recognize `http://...` URLs in
     *     body text without requiring [](...) wrapping. Matches how
     *     casual markdown is usually pasted.
     *   MD_FLAG_COLLAPSEWHITESPACE — collapse runs of whitespace in
     *     text events. Spares every consumer from doing it downstream;
     *     cost is that last_source_offset drifts behind the true source
     *     position by the elided bytes (see file header). */
    parser.flags        = MD_FLAG_PERMISSIVEAUTOLINKS |
                          MD_FLAG_COLLAPSEWHITESPACE;
    parser.enter_block  = on_enter_block;
    parser.leave_block  = on_leave_block;
    parser.enter_span   = on_enter_span;
    parser.leave_span   = on_leave_span;
    parser.text         = on_text;
    parser.debug_log    = 0;
    parser.syntax       = 0;

    rc = md_parse(source, source_len, &parser, &d);
    if (d.aborted) return kMdParseErrSinkAbort;
    if (rc != 0) return kMdParseErrMd4c;
    return kMdParseOk;
}

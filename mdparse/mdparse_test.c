/*
 * mdparse/mdparse_test.c — host unit tests for the md4c adapter.
 *
 * Each test feeds a markdown source string into mdparse_run with a
 * recording sink that captures every callback into an array. Tests
 * then assert on the captured event sequence.
 *
 * Why this shape: mdparse is a translator — md4c events in, our sink
 * events out. The cleanest way to spec it is "given this markdown,
 * the sink receives exactly this sequence of calls."
 */
#include "unity.h"
#include "mdparse.h"
#include <string.h>
#include <stdlib.h>

/* Recording sink — captures every callback into a growable array. */
typedef struct RecEvent {
    enum { kRecBlockOpen, kRecBlockClose, kRecSpan, kRecText } kind;
    BlockKind  block_kind;
    StyleKind  span_kind;
    BlockAttrs attrs;
    unsigned short start;
    unsigned short length;
    unsigned short source_offset;
    char        text[256];        /* truncated if larger */
    char        link_url[128];
    unsigned short link_url_len;
} RecEvent;

typedef struct Recorder {
    RecEvent  events[1024];
    size_t    count;
    int       abort_on_nth;      /* -1 = never abort */
    int       calls;             /* total callbacks seen */
} Recorder;

static int rec_block_open(void* ctx, BlockKind k, const BlockAttrs* a) {
    Recorder* r = (Recorder*)ctx;
    RecEvent* e;
    r->calls++;
    if (r->count >= 1024) return -1;
    e = &r->events[r->count++];
    e->kind = kRecBlockOpen;
    e->block_kind = k;
    if (a) e->attrs = *a;
    if (r->abort_on_nth >= 0 && r->calls > r->abort_on_nth) return -1;
    return 0;
}

static int rec_block_close(void* ctx, BlockKind k) {
    Recorder* r = (Recorder*)ctx;
    RecEvent* e;
    r->calls++;
    if (r->count >= 1024) return -1;
    e = &r->events[r->count++];
    e->kind = kRecBlockClose;
    e->block_kind = k;
    if (r->abort_on_nth >= 0 && r->calls > r->abort_on_nth) return -1;
    return 0;
}

static int rec_span(void* ctx, StyleKind k, unsigned short start,
                    unsigned short length, const char* url,
                    unsigned short url_len) {
    Recorder* r = (Recorder*)ctx;
    RecEvent* e;
    r->calls++;
    if (r->count >= 1024) return -1;
    e = &r->events[r->count++];
    e->kind = kRecSpan;
    e->span_kind = k;
    e->start = start;
    e->length = length;
    if (url && url_len > 0) {
        unsigned short n;
        n = url_len < sizeof(e->link_url) - 1 ? url_len :
            (unsigned short)(sizeof(e->link_url) - 1);
        memcpy(e->link_url, url, n);
        e->link_url[n] = '\0';
        e->link_url_len = url_len;
    } else {
        e->link_url[0] = '\0';
        e->link_url_len = 0;
    }
    if (r->abort_on_nth >= 0 && r->calls > r->abort_on_nth) return -1;
    return 0;
}

static int rec_text(void* ctx, const char* bytes, unsigned short length,
                    unsigned short source_offset) {
    Recorder* r = (Recorder*)ctx;
    RecEvent* e;
    unsigned short n;
    r->calls++;
    if (r->count >= 1024) return -1;
    e = &r->events[r->count++];
    e->kind = kRecText;
    e->length = length;
    e->source_offset = source_offset;
    n = length < sizeof(e->text) - 1 ? length :
        (unsigned short)(sizeof(e->text) - 1);
    if (n > 0 && bytes) memcpy(e->text, bytes, n);
    e->text[n] = '\0';
    if (r->abort_on_nth >= 0 && r->calls > r->abort_on_nth) return -1;
    return 0;
}

static MdParseSink recorder_sink(Recorder* r) {
    MdParseSink s;
    s.on_block_open  = rec_block_open;
    s.on_block_close = rec_block_close;
    s.on_span        = rec_span;
    s.on_text        = rec_text;
    s.ctx            = r;
    return s;
}

static Recorder recorder_new(void) {
    Recorder r;
    memset(&r, 0, sizeof r);
    r.abort_on_nth = -1;
    return r;
}

void setUp(void) {}
void tearDown(void) {}

void test_mdparse_empty_source_produces_no_events(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    int rc = mdparse_run("", 0, &s, 1);
    TEST_ASSERT_EQUAL_INT(kMdParseOk, rc);
    TEST_ASSERT_EQUAL_INT(0, r.count);
}

void test_mdparse_h1_emits_heading_with_level_1(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    int rc = mdparse_run("# Hello", 7, &s, 1);
    size_t i;
    int saw_open_h = 0;
    int saw_text = 0;

    TEST_ASSERT_EQUAL_INT(kMdParseOk, rc);
    TEST_ASSERT_TRUE(r.count >= 2);
    for (i = 0; i < r.count; i++) {
        if (r.events[i].kind == kRecBlockOpen &&
            r.events[i].block_kind == kBlockHeading) {
            saw_open_h = 1;
            TEST_ASSERT_EQUAL_INT(1, r.events[i].attrs.h_level);
        }
        if (r.events[i].kind == kRecText &&
            strstr(r.events[i].text, "Hello")) {
            saw_text = 1;
        }
    }
    TEST_ASSERT_TRUE(saw_open_h);
    TEST_ASSERT_TRUE(saw_text);
}

void test_mdparse_paragraph_emits_paragraph_block(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    size_t i;
    int saw = 0;

    mdparse_run("hi", 2, &s, 1);
    for (i = 0; i < r.count; i++) {
        if (r.events[i].kind == kRecBlockOpen &&
            r.events[i].block_kind == kBlockParagraph) saw = 1;
    }
    TEST_ASSERT_TRUE(saw);
}

void test_mdparse_nested_list_stamps_depth(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    const char src[] = "- a\n  - b\n";
    unsigned char depths_seen[8];
    int n_depths = 0;
    size_t i;

    mdparse_run(src, sizeof src - 1, &s, 1);
    for (i = 0; i < r.count && n_depths < 8; i++) {
        if (r.events[i].kind == kRecBlockOpen &&
            r.events[i].block_kind == kBlockListItem) {
            depths_seen[n_depths++] = r.events[i].attrs.list_depth;
        }
    }
    TEST_ASSERT_EQUAL_INT(2, n_depths);
    TEST_ASSERT_EQUAL_INT(1, depths_seen[0]);
    TEST_ASSERT_EQUAL_INT(2, depths_seen[1]);
}

void test_mdparse_blockquote_stamps_quote_depth(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    const char src[] = "> text\n";
    size_t i;
    int saw = 0;

    mdparse_run(src, sizeof src - 1, &s, 1);
    for (i = 0; i < r.count; i++) {
        if (r.events[i].kind == kRecBlockOpen &&
            r.events[i].block_kind == kBlockParagraph &&
            r.events[i].attrs.quote_depth == 1) saw = 1;
    }
    TEST_ASSERT_TRUE(saw);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mdparse_empty_source_produces_no_events);
    RUN_TEST(test_mdparse_h1_emits_heading_with_level_1);
    RUN_TEST(test_mdparse_paragraph_emits_paragraph_block);
    RUN_TEST(test_mdparse_nested_list_stamps_depth);
    RUN_TEST(test_mdparse_blockquote_stamps_quote_depth);
    /* Additional tests added in subsequent tasks. */
    return UNITY_END();
}

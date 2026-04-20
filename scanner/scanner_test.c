/*
 * scanner/scanner_test.c — host unit tests for scanner.
 *
 * Feeds synthetic MdParseSink events (NOT real md4c output) into the
 * scanner's sink and asserts on the accumulated StyleRun[]. Bypassing
 * md4c keeps tests fast and isolates scanner behavior from any md4c
 * semantic quirks.
 */
#include "unity.h"
#include "test/fake_syscalls.h"
#include "render/arena.h"
#include "scanner.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) { fake_syscalls_activate(0); }

void test_scanner_empty_event_stream_produces_zero_runs(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    {
        Arena* a = 0;
        Scanner* s;
        size_t count = 99;
        const StyleRun* runs;
        arena_init(&a, 4096, (const MacSyscalls*)&f);

        s = scanner_new(a);
        TEST_ASSERT_NOT_NULL(s);

        runs = scanner_runs(s, &count);
        (void)runs;
        TEST_ASSERT_EQUAL_INT(0, count);

        scanner_free(s);
        arena_destroy(a);
    }
}

void test_scanner_records_one_run_per_span(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    {
        Arena* a = 0;
        Scanner* s;
        const MdParseSink* sink;
        BlockAttrs attrs;
        size_t count = 0;
        const StyleRun* runs;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        s = scanner_new(a);
        sink = scanner_sink(s);

        memset(&attrs, 0, sizeof attrs);
        sink->on_block_open(sink->ctx, kBlockParagraph, &attrs);
        sink->on_span(sink->ctx, kStyleStrong, 4, 6, 0, 0);
        sink->on_block_close(sink->ctx, kBlockParagraph);

        runs = scanner_runs(s, &count);
        TEST_ASSERT_EQUAL_INT(1, count);
        TEST_ASSERT_EQUAL_INT(4, runs[0].start);
        TEST_ASSERT_EQUAL_INT(6, runs[0].length);
        TEST_ASSERT_EQUAL_INT(kStyleStrong, runs[0].kind);
        TEST_ASSERT_EQUAL_INT(-1, runs[0].link_index);

        arena_destroy(a);
    }
}

void test_scanner_link_run_has_link_index_minus_one(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    {
        Arena* a = 0;
        Scanner* s;
        const MdParseSink* sink;
        size_t count = 0;
        const StyleRun* runs;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        s = scanner_new(a);
        sink = scanner_sink(s);

        sink->on_span(sink->ctx, kStyleLink, 0, 5, "http://x", 8);

        runs = scanner_runs(s, &count);
        TEST_ASSERT_EQUAL_INT(1, count);
        TEST_ASSERT_EQUAL_INT(kStyleLink, runs[0].kind);
        TEST_ASSERT_EQUAL_INT(-1, runs[0].link_index);  /* scanner doesn't track URLs */

        arena_destroy(a);
    }
}

void test_scanner_html_block_emits_one_run_covering_all_text_events(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    {
        Arena* a = 0;
        Scanner* s;
        const MdParseSink* sink;
        size_t count = 0;
        const StyleRun* runs;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        s = scanner_new(a);
        sink = scanner_sink(s);

        sink->on_block_open(sink->ctx, kBlockHtml, 0);
        sink->on_text(sink->ctx, "<aside>", 7, 10);
        sink->on_text(sink->ctx, "hello", 5, 20);
        sink->on_text(sink->ctx, "</aside>", 8, 29);
        sink->on_block_close(sink->ctx, kBlockHtml);

        runs = scanner_runs(s, &count);
        TEST_ASSERT_EQUAL_INT(1, count);
        TEST_ASSERT_EQUAL_INT(10, runs[0].start);
        TEST_ASSERT_EQUAL_INT(27, runs[0].length);  /* 29 + 8 - 10 */
        TEST_ASSERT_EQUAL_INT(kStyleHtmlSpan, runs[0].kind);
        TEST_ASSERT_EQUAL_INT(-1, runs[0].link_index);

        arena_destroy(a);
    }
}

void test_scanner_empty_html_block_emits_no_run(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    {
        Arena* a = 0;
        Scanner* s;
        const MdParseSink* sink;
        size_t count = 99;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        s = scanner_new(a);
        sink = scanner_sink(s);

        sink->on_block_open(sink->ctx, kBlockHtml, 0);
        sink->on_block_close(sink->ctx, kBlockHtml);

        scanner_runs(s, &count);
        TEST_ASSERT_EQUAL_INT(0, count);

        arena_destroy(a);
    }
}

void test_scanner_reset_clears_runs(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    {
        Arena* a = 0;
        Scanner* s;
        const MdParseSink* sink;
        size_t count = 0;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        s = scanner_new(a);
        sink = scanner_sink(s);

        sink->on_span(sink->ctx, kStyleStrong, 0, 4, 0, 0);
        sink->on_span(sink->ctx, kStyleEmph,   4, 4, 0, 0);
        scanner_runs(s, &count);
        TEST_ASSERT_EQUAL_INT(2, count);

        scanner_reset(s);
        scanner_runs(s, &count);
        TEST_ASSERT_EQUAL_INT(0, count);

        arena_destroy(a);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_scanner_empty_event_stream_produces_zero_runs);
    RUN_TEST(test_scanner_records_one_run_per_span);
    RUN_TEST(test_scanner_link_run_has_link_index_minus_one);
    RUN_TEST(test_scanner_html_block_emits_one_run_covering_all_text_events);
    RUN_TEST(test_scanner_empty_html_block_emits_no_run);
    RUN_TEST(test_scanner_reset_clears_runs);
    return UNITY_END();
}

/*
 * render/render_test.c — host unit tests for the render module.
 *
 * Tests fall into two camps:
 *   1. Model-construction tests — feed synthetic MdParseSink events,
 *      assert on the resulting Block[] contents.
 *   2. Layout+draw tests — build a tiny model, invoke render_layout_
 *      and_draw against a Recorder, assert on recorded DrawOps calls.
 */
#include "unity.h"
#include "test/fake_syscalls.h"
#include "test/recorder.h"
#include "render/arena.h"
#include "render/render.h"
#include "mdparse/mdparse.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) { fake_syscalls_activate(0); }

void test_render_model_empty_has_zero_blocks(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);
    {
        Arena* a = 0;
        RenderModel* m;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        m = render_model_new(a);
        TEST_ASSERT_NOT_NULL(m);
        TEST_ASSERT_EQUAL_INT(0, render_model_block_count(m));
        arena_destroy(a);
    }
}

void test_render_model_builds_single_heading(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);
    {
        Arena* a = 0;
        RenderModel* m;
        const MdParseSink* sink;
        const Block* b;
        BlockAttrs attrs;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        m = render_model_new(a);
        sink = render_model_sink(m);

        memset(&attrs, 0, sizeof attrs);
        attrs.h_level = 1;
        sink->on_block_open(sink->ctx, kBlockHeading, &attrs);
        sink->on_text(sink->ctx, "Hello", 5, 2);
        sink->on_block_close(sink->ctx, kBlockHeading);

        TEST_ASSERT_EQUAL_INT(1, render_model_block_count(m));
        b = render_model_block_at(m, 0);
        TEST_ASSERT_EQUAL_INT(kBlockHeading, b->kind);
        TEST_ASSERT_EQUAL_INT(1, b->h_level);
        TEST_ASSERT_EQUAL_INT(5, b->text_length);
        TEST_ASSERT_EQUAL_MEMORY("Hello", b->text, 5);

        arena_destroy(a);
    }
}

void test_render_model_nested_list_has_list_depth_stamps(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);
    {
        Arena* a = 0;
        RenderModel* m;
        const MdParseSink* sink;
        BlockAttrs a1, a2;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        m = render_model_new(a);
        sink = render_model_sink(m);

        memset(&a1, 0, sizeof a1); a1.list_depth = 1;
        memset(&a2, 0, sizeof a2); a2.list_depth = 2;
        sink->on_block_open(sink->ctx, kBlockListItem, &a1);
        sink->on_text(sink->ctx, "a", 1, 0);
        sink->on_block_close(sink->ctx, kBlockListItem);
        sink->on_block_open(sink->ctx, kBlockListItem, &a2);
        sink->on_text(sink->ctx, "b", 1, 0);
        sink->on_block_close(sink->ctx, kBlockListItem);

        TEST_ASSERT_EQUAL_INT(2, render_model_block_count(m));
        TEST_ASSERT_EQUAL_INT(1, render_model_block_at(m, 0)->list_depth);
        TEST_ASSERT_EQUAL_INT(2, render_model_block_at(m, 1)->list_depth);

        arena_destroy(a);
    }
}

void test_render_model_block_quote_stamps_quote_depth(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);
    {
        Arena* a = 0;
        RenderModel* m;
        const MdParseSink* sink;
        const Block* b;
        BlockAttrs attr;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        m = render_model_new(a);
        sink = render_model_sink(m);

        memset(&attr, 0, sizeof attr); attr.quote_depth = 1;
        sink->on_block_open(sink->ctx, kBlockParagraph, &attr);
        sink->on_text(sink->ctx, "x", 1, 0);
        sink->on_block_close(sink->ctx, kBlockParagraph);

        b = render_model_block_at(m, 0);
        TEST_ASSERT_EQUAL_INT(1, b->quote_depth);
        arena_destroy(a);
    }
}

void test_render_model_code_block_captures_text(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);
    {
        Arena* a = 0;
        RenderModel* m;
        const MdParseSink* sink;
        const Block* b;
        BlockAttrs attr;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        m = render_model_new(a);
        sink = render_model_sink(m);

        memset(&attr, 0, sizeof attr);
        sink->on_block_open(sink->ctx, kBlockCodeBlock, &attr);
        sink->on_text(sink->ctx, "int x;", 6, 0);
        sink->on_block_close(sink->ctx, kBlockCodeBlock);

        b = render_model_block_at(m, 0);
        TEST_ASSERT_EQUAL_INT(kBlockCodeBlock, b->kind);
        TEST_ASSERT_EQUAL_INT(6, b->text_length);

        arena_destroy(a);
    }
}

void test_render_model_hr_block_has_no_text(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);
    {
        Arena* a = 0;
        RenderModel* m;
        const MdParseSink* sink;
        BlockAttrs attr;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        m = render_model_new(a);
        sink = render_model_sink(m);

        memset(&attr, 0, sizeof attr);
        sink->on_block_open(sink->ctx, kBlockHr, &attr);
        sink->on_block_close(sink->ctx, kBlockHr);

        TEST_ASSERT_EQUAL_INT(1, render_model_block_count(m));
        TEST_ASSERT_EQUAL_INT(kBlockHr, render_model_block_at(m, 0)->kind);
        TEST_ASSERT_EQUAL_INT(0, render_model_block_at(m, 0)->text_length);

        arena_destroy(a);
    }
}

void test_render_model_paragraph_with_bold_run(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);
    {
        Arena* a = 0;
        RenderModel* m;
        const MdParseSink* sink;
        const Block* b;
        BlockAttrs attr;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        m = render_model_new(a);
        sink = render_model_sink(m);

        memset(&attr, 0, sizeof attr);
        sink->on_block_open(sink->ctx, kBlockParagraph, &attr);
        sink->on_text(sink->ctx, "plain bold rest", 15, 0);
        sink->on_span(sink->ctx, kStyleStrong, 6, 4, 0, 0);
        sink->on_block_close(sink->ctx, kBlockParagraph);

        b = render_model_block_at(m, 0);
        TEST_ASSERT_EQUAL_INT(1, b->run_count);
        TEST_ASSERT_EQUAL_INT(kStyleStrong, b->runs[0].kind);
        TEST_ASSERT_EQUAL_INT(6, b->runs[0].start);
        TEST_ASSERT_EQUAL_INT(4, b->runs[0].length);

        arena_destroy(a);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_render_model_empty_has_zero_blocks);
    RUN_TEST(test_render_model_builds_single_heading);
    RUN_TEST(test_render_model_nested_list_has_list_depth_stamps);
    RUN_TEST(test_render_model_block_quote_stamps_quote_depth);
    RUN_TEST(test_render_model_code_block_captures_text);
    RUN_TEST(test_render_model_hr_block_has_no_text);
    RUN_TEST(test_render_model_paragraph_with_bold_run);
    return UNITY_END();
}

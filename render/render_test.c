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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_render_model_empty_has_zero_blocks);
    RUN_TEST(test_render_model_builds_single_heading);
    return UNITY_END();
}

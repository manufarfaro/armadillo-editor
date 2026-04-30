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

static int count_ops(const Recorder* r, RecOpKind op) {
    int n = 0;
    size_t i;
    for (i = 0; i < r->count; i++) if (r->calls[i].op == op) n++;
    return n;
}

void test_render_layout_empty_model_emits_nothing(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);
    {
        Arena* a = 0;
        RenderModel* m;
        Recorder r;
        DrawContext ctx;
        LayoutParams p;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        m = render_model_new(a);

        memset(&r, 0, sizeof r);
        ctx = recorder_context(&r);
        p = layout_params_defaults();
        render_layout_and_draw(m, &p, &ctx);

        /* Allowed to call get_font_metrics for setup but nothing else. */
        TEST_ASSERT_EQUAL_INT(0, count_ops(&r, kRecDrawText));

        recorder_free(&r);
        arena_destroy(a);
    }
}

void test_render_layout_single_paragraph_draws_text(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);
    {
        Arena* a = 0;
        RenderModel* m;
        const MdParseSink* sink;
        BlockAttrs attr;
        Recorder r;
        DrawContext ctx;
        LayoutParams p;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        m = render_model_new(a);
        sink = render_model_sink(m);

        memset(&attr, 0, sizeof attr);
        sink->on_block_open(sink->ctx, kBlockParagraph, &attr);
        sink->on_text(sink->ctx, "hello", 5, 0);
        sink->on_block_close(sink->ctx, kBlockParagraph);

        memset(&r, 0, sizeof r);
        ctx = recorder_context(&r);
        p = layout_params_defaults();
        render_layout_and_draw(m, &p, &ctx);

        TEST_ASSERT_EQUAL_INT(1, count_ops(&r, kRecDrawText));

        recorder_free(&r);
        arena_destroy(a);
    }
}

void test_render_layout_h1_uses_chicago_17_bold(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);
    {
        Arena* a = 0;
        RenderModel* m;
        const MdParseSink* sink;
        BlockAttrs attr;
        Recorder r;
        DrawContext ctx;
        LayoutParams p;
        int saw = 0;
        size_t i;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        m = render_model_new(a);
        sink = render_model_sink(m);

        memset(&attr, 0, sizeof attr); attr.h_level = 1;
        sink->on_block_open(sink->ctx, kBlockHeading, &attr);
        sink->on_text(sink->ctx, "H", 1, 0);
        sink->on_block_close(sink->ctx, kBlockHeading);

        memset(&r, 0, sizeof r);
        ctx = recorder_context(&r);
        p = layout_params_defaults();
        render_layout_and_draw(m, &p, &ctx);

        /* Find a set_font with size=17 and a bold face bit (bit 0). */
        for (i = 0; i < r.count; i++) {
            if (r.calls[i].op == kRecSetFont &&
                r.calls[i].font_size == 17 &&
                (r.calls[i].font_face & 0x01)) saw = 1;
        }
        TEST_ASSERT_TRUE(saw);

        recorder_free(&r);
        arena_destroy(a);
    }
}

void test_render_layout_bold_run_wraps_with_set_font_calls(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);
    {
        Arena* a = 0;
        RenderModel* m;
        const MdParseSink* sink;
        BlockAttrs attr;
        Recorder r;
        DrawContext ctx;
        LayoutParams p;
        int saw_bold_font = 0;
        size_t i;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        m = render_model_new(a);
        sink = render_model_sink(m);

        memset(&attr, 0, sizeof attr);
        sink->on_block_open(sink->ctx, kBlockParagraph, &attr);
        sink->on_text(sink->ctx, "plain bold rest", 15, 0);
        sink->on_span(sink->ctx, kStyleStrong, 6, 4, 0, 0);
        sink->on_block_close(sink->ctx, kBlockParagraph);

        memset(&r, 0, sizeof r);
        ctx = recorder_context(&r);
        p = layout_params_defaults();
        render_layout_and_draw(m, &p, &ctx);

        /* Spec scenario "Bold run in a paragraph" (render/spec.md §Inline
         * style run application): expect the draw_text calls in order —
         * "plain " (6 bytes, base style), "bold" (4 bytes, bold style
         * applied), " rest" (5 bytes, base style restored). Walk the
         * recorded calls; find each draw_text; verify length and bytes;
         * verify the set_font immediately preceding "bold" has the bold
         * bit and the set_font preceding " rest" does not. */
        {
            int found_plain = 0, found_bold = 0, found_rest = 0;
            int bold_set_font_before_bold = 0;
            int plain_set_font_before_rest = 0;
            RecCall* prev_set_font = 0;
            for (i = 0; i < r.count; i++) {
                RecCall* c = &r.calls[i];
                if (c->op == kRecSetFont) {
                    prev_set_font = c;
                    if (c->font_face & 0x01) saw_bold_font = 1;
                } else if (c->op == kRecDrawText) {
                    if (c->text_len == 6 && memcmp(c->text, "plain ", 6) == 0) {
                        found_plain = 1;
                    } else if (c->text_len == 4 && memcmp(c->text, "bold", 4) == 0) {
                        found_bold = 1;
                        if (prev_set_font && (prev_set_font->font_face & 0x01))
                            bold_set_font_before_bold = 1;
                    } else if (c->text_len == 5 && memcmp(c->text, " rest", 5) == 0) {
                        found_rest = 1;
                        if (prev_set_font && !(prev_set_font->font_face & 0x01))
                            plain_set_font_before_rest = 1;
                    }
                }
            }
            TEST_ASSERT_TRUE(found_plain);
            TEST_ASSERT_TRUE(found_bold);
            TEST_ASSERT_TRUE(found_rest);
            TEST_ASSERT_TRUE(bold_set_font_before_bold);
            TEST_ASSERT_TRUE(plain_set_font_before_rest);
        }
        TEST_ASSERT_TRUE(saw_bold_font);

        recorder_free(&r);
        arena_destroy(a);
    }
}

void test_render_layout_hr_emits_a_line(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);
    {
        Arena* a = 0;
        RenderModel* m;
        const MdParseSink* sink;
        BlockAttrs attr;
        Recorder r;
        DrawContext ctx;
        LayoutParams p;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        m = render_model_new(a);
        sink = render_model_sink(m);

        memset(&attr, 0, sizeof attr);
        sink->on_block_open(sink->ctx, kBlockHr, &attr);
        sink->on_block_close(sink->ctx, kBlockHr);

        memset(&r, 0, sizeof r);
        ctx = recorder_context(&r);
        p = layout_params_defaults();
        render_layout_and_draw(m, &p, &ctx);

        TEST_ASSERT_EQUAL_INT(1, count_ops(&r, kRecLine));

        recorder_free(&r);
        arena_destroy(a);
    }
}

void test_render_layout_list_item_draws_bullet_and_text(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);
    {
        Arena* a = 0;
        RenderModel* m;
        const MdParseSink* sink;
        BlockAttrs attr;
        Recorder r;
        DrawContext ctx;
        LayoutParams p;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        m = render_model_new(a);
        sink = render_model_sink(m);

        memset(&attr, 0, sizeof attr); attr.list_depth = 1;
        sink->on_block_open(sink->ctx, kBlockListItem, &attr);
        sink->on_text(sink->ctx, "item", 4, 0);
        sink->on_block_close(sink->ctx, kBlockListItem);

        memset(&r, 0, sizeof r);
        ctx = recorder_context(&r);
        p = layout_params_defaults();
        render_layout_and_draw(m, &p, &ctx);

        /* Two draw_text calls: bullet "*" and text "item". */
        TEST_ASSERT_TRUE(count_ops(&r, kRecDrawText) >= 2);

        recorder_free(&r);
        arena_destroy(a);
    }
}

void test_render_layout_blockquote_draws_vertical_bar(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);
    {
        Arena* a = 0;
        RenderModel* m;
        const MdParseSink* sink;
        BlockAttrs attr;
        Recorder r;
        DrawContext ctx;
        LayoutParams p;
        arena_init(&a, 4096, (const MacSyscalls*)&f);
        m = render_model_new(a);
        sink = render_model_sink(m);

        memset(&attr, 0, sizeof attr); attr.quote_depth = 1;
        sink->on_block_open(sink->ctx, kBlockBlockQuote, &attr);
        sink->on_text(sink->ctx, "quoted", 6, 0);
        sink->on_block_close(sink->ctx, kBlockBlockQuote);

        memset(&r, 0, sizeof r);
        ctx = recorder_context(&r);
        p = layout_params_defaults();
        render_layout_and_draw(m, &p, &ctx);

        TEST_ASSERT_EQUAL_INT(1, count_ops(&r, kRecLine));

        recorder_free(&r);
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
    RUN_TEST(test_render_layout_empty_model_emits_nothing);
    RUN_TEST(test_render_layout_single_paragraph_draws_text);
    RUN_TEST(test_render_layout_h1_uses_chicago_17_bold);
    RUN_TEST(test_render_layout_bold_run_wraps_with_set_font_calls);
    RUN_TEST(test_render_layout_hr_emits_a_line);
    RUN_TEST(test_render_layout_list_item_draws_bullet_and_text);
    RUN_TEST(test_render_layout_blockquote_draws_vertical_bar);
    return UNITY_END();
}

/*
 * render/render.c — flat block model + layout + draw emission.
 *
 * Implementation lands in pieces across Phase 8 tasks; each task
 * fleshes out one responsibility cluster. This file starts with
 * model construction (Task 41) and grows to include the layout
 * pass (Tasks 43-44).
 */
#include "render.h"
#include "mdparse/mdparse.h"
#include <string.h>

#define kInitialBlockCapacity 32

struct RenderModel {
    Arena*     arena;
    Block*     blocks;
    size_t     block_count;
    size_t     block_capacity;

    /* Current in-flight block during construction */
    int        cur_active;
    BlockKind  cur_kind;
    BlockAttrs cur_attrs;
    char*      cur_text_buf;
    size_t     cur_text_len;
    size_t     cur_text_capacity;

    StyleRun*  cur_runs;
    size_t     cur_run_count;
    size_t     cur_run_capacity;
};

static int grow_blocks(RenderModel* m, size_t needed) {
    size_t next;
    size_t bytes;
    Block* nb;
    if (needed <= m->block_capacity) return 0;
    next = m->block_capacity ? m->block_capacity : kInitialBlockCapacity;
    while (next < needed) next *= 2;
    bytes = next * sizeof(Block);
    if (arena_ensure(m->arena, bytes) != 0) return -1;
    nb = (Block*)arena_alloc(m->arena, bytes);
    if (!nb) return -1;
    if (m->blocks && m->block_count > 0) {
        memcpy(nb, m->blocks, m->block_count * sizeof(Block));
    }
    m->blocks = nb;
    m->block_capacity = next;
    return 0;
}

/* Sink callbacks */
static int rm_block_open(void* ctx, BlockKind k, const BlockAttrs* a) {
    RenderModel* m = (RenderModel*)ctx;
    m->cur_active = 1;
    m->cur_kind = k;
    if (a) m->cur_attrs = *a;
    else   memset(&m->cur_attrs, 0, sizeof(BlockAttrs));
    m->cur_text_len = 0;
    m->cur_text_capacity = 0;
    m->cur_text_buf = 0;
    m->cur_runs = 0;
    m->cur_run_count = 0;
    m->cur_run_capacity = 0;
    return 0;
}

static int rm_text(void* ctx, const char* bytes, unsigned short length,
                   unsigned short source_offset) {
    RenderModel* m = (RenderModel*)ctx;
    size_t need;
    size_t next;
    char* nb;
    (void)source_offset;
    if (!m->cur_active) return 0;
    need = m->cur_text_len + length;
    if (need > m->cur_text_capacity) {
        next = m->cur_text_capacity ? m->cur_text_capacity : 64;
        while (next < need) next *= 2;
        if (arena_ensure(m->arena, next) != 0) return -1;
        nb = (char*)arena_alloc(m->arena, next);
        if (!nb) return -1;
        if (m->cur_text_buf && m->cur_text_len > 0) {
            memcpy(nb, m->cur_text_buf, m->cur_text_len);
        }
        m->cur_text_buf = nb;
        m->cur_text_capacity = next;
    }
    if (length > 0 && bytes) {
        memcpy(m->cur_text_buf + m->cur_text_len, bytes, length);
    }
    m->cur_text_len += length;
    return 0;
}

static int rm_span(void* ctx, StyleKind k, unsigned short start,
                   unsigned short length, const char* url,
                   unsigned short url_len) {
    RenderModel* m = (RenderModel*)ctx;
    size_t next;
    size_t bytes;
    StyleRun* nb;
    StyleRun r;
    (void)url; (void)url_len;
    if (!m->cur_active) return 0;
    if (m->cur_run_count >= m->cur_run_capacity) {
        next = m->cur_run_capacity ? m->cur_run_capacity * 2 : 4;
        bytes = next * sizeof(StyleRun);
        if (arena_ensure(m->arena, bytes) != 0) return -1;
        nb = (StyleRun*)arena_alloc(m->arena, bytes);
        if (!nb) return -1;
        if (m->cur_runs && m->cur_run_count > 0) {
            memcpy(nb, m->cur_runs, m->cur_run_count * sizeof(StyleRun));
        }
        m->cur_runs = nb;
        m->cur_run_capacity = next;
    }
    r.start = start;
    r.length = length;
    r.kind = k;
    r.link_index = -1;
    m->cur_runs[m->cur_run_count++] = r;
    return 0;
}

static int rm_block_close(void* ctx, BlockKind k) {
    RenderModel* m = (RenderModel*)ctx;
    Block* b;
    if (!m->cur_active || m->cur_kind != k) return 0;
    if (grow_blocks(m, m->block_count + 1) != 0) return -1;
    b = &m->blocks[m->block_count++];
    memset(b, 0, sizeof *b);
    b->kind = m->cur_kind;
    b->h_level = m->cur_attrs.h_level;
    b->list_depth = m->cur_attrs.list_depth;
    b->quote_depth = m->cur_attrs.quote_depth;
    b->list_ordered = m->cur_attrs.list_ordered;
    b->text = m->cur_text_buf;
    b->text_length = (unsigned short)m->cur_text_len;
    b->run_count = (unsigned short)m->cur_run_count;
    b->runs = m->cur_runs;
    m->cur_active = 0;
    return 0;
}

RenderModel* render_model_new(Arena* a) {
    RenderModel* m;
    if (!a) return 0;
    if (arena_ensure(a, sizeof(RenderModel)) != 0) return 0;
    m = (RenderModel*)arena_alloc(a, sizeof(RenderModel));
    if (!m) return 0;
    memset(m, 0, sizeof *m);
    m->arena = a;
    return m;
}

const MdParseSink* render_model_sink(RenderModel* m) {
    MdParseSink* s;
    if (!m) return 0;
    if (arena_ensure(m->arena, sizeof(MdParseSink)) != 0) return 0;
    s = (MdParseSink*)arena_alloc(m->arena, sizeof(MdParseSink));
    if (!s) return 0;
    s->on_block_open  = rm_block_open;
    s->on_block_close = rm_block_close;
    s->on_span        = rm_span;
    s->on_text        = rm_text;
    s->ctx            = m;
    return (const MdParseSink*)s;
}

size_t render_model_block_count(const RenderModel* m) {
    return m ? m->block_count : 0;
}

const Block* render_model_block_at(const RenderModel* m, size_t i) {
    if (!m || i >= m->block_count) return 0;
    return &m->blocks[i];
}

LayoutParams layout_params_defaults(void) {
    LayoutParams p;
    p.content_width = 464;
    p.indent_list   = 14;
    p.indent_quote  = 8;
    p.left_margin   = 8;
    p.top_margin    = 8;
    p.block_spacing = 6;
    return p;
}

/* render_layout_and_draw — stubbed; filled in Tasks 43-44. */
void render_layout_and_draw(const RenderModel* m, const LayoutParams* p,
                            DrawContext* ctx) {
    (void)m; (void)p; (void)ctx;
}

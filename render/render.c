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

/* Font constants — match System 7 defaults. */
enum { kFontChicago = 0, kFontGeneva = 3, kFontMonaco = 4 };
enum { kFacePlain = 0, kFaceBold = 1, kFaceItalic = 2, kFaceUnderline = 4 };

typedef struct {
    short         font_id;
    short         size;
    unsigned char face;
    unsigned short fg_r, fg_g, fg_b;
} Style;

static Style style_for_block(const Block* b) {
    Style s;
    s.fg_r = s.fg_g = s.fg_b = 0;  /* black */
    switch (b->kind) {
        case kBlockHeading:
            s.font_id = kFontChicago;
            s.face = kFaceBold;
            s.size = (b->h_level == 1) ? 17 :
                     (b->h_level == 2) ? 14 : 12;
            if (b->h_level >= 4) s.face = kFacePlain;
            return s;
        case kBlockCodeBlock:
            s.font_id = kFontMonaco; s.size = 10; s.face = kFacePlain;
            return s;
        case kBlockHtml:
            s.font_id = kFontMonaco; s.size = 10; s.face = kFacePlain;
            /* Purple-ish */
            s.fg_r = 0x8A8A; s.fg_g = 0x7A7A; s.fg_b = 0xA8A8;
            return s;
        default:
            s.font_id = kFontGeneva; s.size = 12; s.face = kFacePlain;
            return s;
    }
}

static Style style_for_run(const Block* b, StyleKind k) {
    Style base = style_for_block(b);
    switch (k) {
        case kStyleStrong:   base.face |= kFaceBold;   break;
        case kStyleEmph:     base.face |= kFaceItalic; break;
        case kStyleCodeSpan: base.font_id = kFontMonaco; base.size = 10; break;
        case kStyleLink:
            base.face |= kFaceUnderline;
            /* sky blue */
            base.fg_r = 0x4A4A; base.fg_g = 0x8080; base.fg_b = 0xA0A0;
            break;
        case kStyleHtmlSpan:
            base.fg_r = 0x8A8A; base.fg_g = 0x7A7A; base.fg_b = 0xA8A8;
            break;
        default: break;
    }
    return base;
}

static void emit_set_style(DrawContext* ctx, const Style* s) {
    ctx->ops->set_font(ctx->ctx, s->font_id, s->size, s->face);
    ctx->ops->set_fg(ctx->ctx, s->fg_r, s->fg_g, s->fg_b);
}

static void emit_text_with_runs(DrawContext* ctx, const Block* b) {
    Style base;
    Style rs;
    unsigned short cursor;
    unsigned short i;
    unsigned short next_start;

    base = style_for_block(b);
    cursor = 0;
    for (i = 0; i <= b->run_count; i++) {
        next_start = (i < b->run_count) ? b->runs[i].start : b->text_length;
        if (next_start > cursor) {
            emit_set_style(ctx, &base);
            ctx->ops->draw_text(ctx->ctx, b->text + cursor,
                                (unsigned short)(next_start - cursor));
        }
        if (i < b->run_count) {
            rs = style_for_run(b, b->runs[i].kind);
            emit_set_style(ctx, &rs);
            ctx->ops->draw_text(ctx->ctx, b->text + b->runs[i].start,
                                b->runs[i].length);
            cursor = (unsigned short)(b->runs[i].start + b->runs[i].length);
        }
    }
}

void render_layout_and_draw(const RenderModel* m, const LayoutParams* p,
                            DrawContext* ctx) {
    size_t i;
    short y;
    short ascent;
    short descent;
    short line_height;
    const Block* b;
    short x;

    if (!m || !p || !ctx || !ctx->ops) return;
    y = p->top_margin;
    ctx->ops->get_font_metrics(ctx->ctx, &ascent, &descent, &line_height);

    for (i = 0; i < m->block_count; i++) {
        b = &m->blocks[i];
        x = p->left_margin
          + (short)b->list_depth  * p->indent_list
          + (short)b->quote_depth * p->indent_quote;

        if (b->kind == kBlockHr) {
            ctx->ops->line(ctx->ctx, x, y + line_height / 2,
                           (short)(x + p->content_width),
                           y + line_height / 2);
            y = (short)(y + line_height + p->block_spacing);
            continue;
        }
        if (b->kind == kBlockListItem) {
            Style base = style_for_block(b);
            emit_set_style(ctx, &base);
            ctx->ops->move_to(ctx->ctx, x, y + ascent);
            ctx->ops->draw_text(ctx->ctx, "*", 1);
            x = (short)(x + p->indent_list);
        }
        if (b->kind == kBlockBlockQuote || b->quote_depth > 0) {
            ctx->ops->line(ctx->ctx, x, y, x, y + line_height);
            x = (short)(x + p->indent_quote);
        }

        ctx->ops->move_to(ctx->ctx, x, y + ascent);
        if (b->text_length > 0) emit_text_with_runs(ctx, b);

        y = (short)(y + line_height + p->block_spacing);
    }
}

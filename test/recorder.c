/*
 * test/recorder.c — recording DrawOps for render unit tests.
 */
#include "recorder.h"
#include <stdlib.h>
#include <string.h>

static void grow(Recorder* r) {
    size_t next;
    if (r->count < r->capacity) return;
    next = r->capacity ? r->capacity * 2 : 16;
    r->calls = (RecCall*)realloc(r->calls, next * sizeof(RecCall));
    r->capacity = next;
}

static void rec_set_font(void* ctx, short fid, short size, unsigned char face) {
    Recorder* r = (Recorder*)ctx;
    RecCall* c;
    grow(r);
    c = &r->calls[r->count++];
    memset(c, 0, sizeof *c);
    c->op = kRecSetFont;
    c->font_id = fid;
    c->font_size = size;
    c->font_face = face;
}

static void rec_set_fg(void* ctx, unsigned short red, unsigned short green,
                       unsigned short blue) {
    Recorder* r = (Recorder*)ctx;
    RecCall* c;
    grow(r);
    c = &r->calls[r->count++];
    memset(c, 0, sizeof *c);
    c->op = kRecSetFg;
    c->color_r = red;
    c->color_g = green;
    c->color_b = blue;
}

static void rec_move_to(void* ctx, short h, short v) {
    Recorder* r = (Recorder*)ctx;
    RecCall* c;
    grow(r);
    c = &r->calls[r->count++];
    memset(c, 0, sizeof *c);
    c->op = kRecMoveTo;
    c->x = h;
    c->y = v;
}

static void rec_draw_text(void* ctx, const char* bytes, unsigned short length) {
    Recorder* r = (Recorder*)ctx;
    RecCall* c;
    unsigned short n;
    grow(r);
    c = &r->calls[r->count++];
    memset(c, 0, sizeof *c);
    c->op = kRecDrawText;
    n = length < sizeof(c->text) - 1 ? length :
        (unsigned short)(sizeof(c->text) - 1);
    if (n > 0 && bytes) memcpy(c->text, bytes, n);
    c->text[n] = '\0';
    c->text_len = length;
}

static void rec_line(void* ctx, short x0, short y0, short x1, short y1) {
    Recorder* r = (Recorder*)ctx;
    RecCall* c;
    grow(r);
    c = &r->calls[r->count++];
    memset(c, 0, sizeof *c);
    c->op = kRecLine;
    c->x = x0; c->y = y0; c->x2 = x1; c->y2 = y1;
}

static void rec_frame_rect(void* ctx, short l, short t, short rr, short b) {
    Recorder* r = (Recorder*)ctx;
    RecCall* c;
    grow(r);
    c = &r->calls[r->count++];
    memset(c, 0, sizeof *c);
    c->op = kRecFrameRect;
    c->rect_l = l; c->rect_t = t; c->rect_r = rr; c->rect_b = b;
}

static void rec_get_font_metrics(void* ctx, short* a, short* d, short* h) {
    Recorder* r = (Recorder*)ctx;
    RecCall* c;
    grow(r);
    c = &r->calls[r->count++];
    memset(c, 0, sizeof *c);
    c->op = kRecGetFontMetrics;
    if (a) *a = r->metrics_ascent      ? r->metrics_ascent      : 12;
    if (d) *d = r->metrics_descent     ? r->metrics_descent     : 3;
    if (h) *h = r->metrics_line_height ? r->metrics_line_height : 16;
}

static const DrawOps g_ops = {
    rec_set_font, rec_set_fg, rec_move_to, rec_draw_text,
    rec_line, rec_frame_rect, rec_get_font_metrics
};

DrawContext recorder_context(Recorder* r) {
    DrawContext ctx;
    ctx.ops = &g_ops;
    ctx.ctx = r;
    return ctx;
}

void recorder_free(Recorder* r) {
    if (!r) return;
    if (r->calls) free(r->calls);
    r->calls = 0;
    r->count = 0;
    r->capacity = 0;
}

/*
 * render/draw_qd_real.c — real-QuickDraw DrawOps vtable (skeleton).
 *
 * Plan 2a: DrawOps callbacks are present but no-op (they cast the
 * grafport and return). Plan 2b fills them in with real QuickDraw
 * calls (SetFont, MoveTo, DrawText, LineTo, FrameRect, GetFontInfo)
 * and RGBForeColor for set_fg. The skeleton lets us include this in
 * the build now without exercising any unfinished behaviour.
 */
#include "draw_qd_real.h"

static void real_set_font(void* ctx, short font_id, short size,
                          unsigned char face) {
    (void)ctx; (void)font_id; (void)size; (void)face;
}

static void real_set_fg(void* ctx, unsigned short red, unsigned short green,
                        unsigned short blue) {
    (void)ctx; (void)red; (void)green; (void)blue;
}

static void real_move_to(void* ctx, short h, short v) {
    (void)ctx; (void)h; (void)v;
}

static void real_draw_text(void* ctx, const char* bytes,
                           unsigned short length) {
    (void)ctx; (void)bytes; (void)length;
}

static void real_line(void* ctx, short x0, short y0, short x1, short y1) {
    (void)ctx; (void)x0; (void)y0; (void)x1; (void)y1;
}

static void real_frame_rect(void* ctx, short l, short t, short r, short b) {
    (void)ctx; (void)l; (void)t; (void)r; (void)b;
}

static void real_get_font_metrics(void* ctx, short* a, short* d, short* h) {
    (void)ctx;
    if (a) *a = 12;       /* sane defaults so render math doesn't hit zeros */
    if (d) *d = 3;
    if (h) *h = 16;
}

static const DrawOps g_real_ops = {
    real_set_font, real_set_fg, real_move_to, real_draw_text,
    real_line, real_frame_rect, real_get_font_metrics
};

DrawContext draw_qd_real_make_context(void* grafport_opaque) {
    DrawContext ctx;
    ctx.ops = &g_real_ops;
    ctx.ctx = grafport_opaque;
    return ctx;
}

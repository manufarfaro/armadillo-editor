/*
 * render/draw_qd.c — production DrawOps that forwards to QuickDraw.
 *
 * Plan 2a: DrawOps callbacks are present but no-op (they cast the
 * grafport and return). Plan 2b fills them in with real QuickDraw
 * calls (SetFont, MoveTo, DrawText, LineTo, FrameRect, GetFontInfo)
 * and RGBForeColor for set_fg. The skeleton lets us include this in
 * the build now without exercising any unfinished behaviour.
 *
 * Test fake equivalent: test/recorder.c (records every call instead
 * of forwarding). Both consume the same DrawOps interface from
 * render/draw_qd.h.
 */
#include "draw_qd.h"

static void qd_set_font(void* ctx, short font_id, short size,
                        unsigned char face) {
    (void)ctx; (void)font_id; (void)size; (void)face;
}

static void qd_set_fg(void* ctx, unsigned short red, unsigned short green,
                      unsigned short blue) {
    (void)ctx; (void)red; (void)green; (void)blue;
}

static void qd_move_to(void* ctx, short h, short v) {
    (void)ctx; (void)h; (void)v;
}

static void qd_draw_text(void* ctx, const char* bytes,
                         unsigned short length) {
    (void)ctx; (void)bytes; (void)length;
}

static void qd_line(void* ctx, short x0, short y0, short x1, short y1) {
    (void)ctx; (void)x0; (void)y0; (void)x1; (void)y1;
}

static void qd_frame_rect(void* ctx, short l, short t, short r, short b) {
    (void)ctx; (void)l; (void)t; (void)r; (void)b;
}

static void qd_get_font_metrics(void* ctx, short* a, short* d, short* h) {
    (void)ctx;
    if (a) *a = 12;       /* sane defaults so render math doesn't hit zeros */
    if (d) *d = 3;
    if (h) *h = 16;
}

static const DrawOps g_qd_ops = {
    qd_set_font, qd_set_fg, qd_move_to, qd_draw_text,
    qd_line, qd_frame_rect, qd_get_font_metrics
};

DrawContext draw_qd_make_context(void* grafport_opaque) {
    DrawContext ctx;
    ctx.ops = &g_qd_ops;
    ctx.ctx = grafport_opaque;
    return ctx;
}

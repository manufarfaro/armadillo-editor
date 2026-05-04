/*
 * render/draw_qd.c — production DrawOps that forwards to QuickDraw.
 *
 * Each callback sets the supplied GrafPort as the current drawing
 * port (cheap: SetPort just swaps a global), then issues the
 * matching QuickDraw call. Color set_fg goes through RGBForeColor;
 * the others use plain QuickDraw.
 *
 * Test fake equivalent: test/recorder.c (records every call instead
 * of forwarding). Both consume the same DrawOps interface from
 * render/draw_qd.h.
 */
#include "draw_qd.h"
#include <Quickdraw.h>
#include <Fonts.h>

static void qd_set_font(void* ctx, short font_id, short size,
                        unsigned char face) {
    SetPort((GrafPtr)ctx);
    TextFont(font_id);
    TextSize(size);
    TextFace(face);
}

static void qd_set_fg(void* ctx, unsigned short red, unsigned short green,
                      unsigned short blue) {
    RGBColor c;
    SetPort((GrafPtr)ctx);
    c.red   = red;
    c.green = green;
    c.blue  = blue;
    RGBForeColor(&c);
}

static void qd_move_to(void* ctx, short h, short v) {
    SetPort((GrafPtr)ctx);
    MoveTo(h, v);
}

static void qd_draw_text(void* ctx, const char* bytes,
                         unsigned short length) {
    SetPort((GrafPtr)ctx);
    DrawText((const Ptr)bytes, 0, length);
}

static void qd_line(void* ctx, short x0, short y0, short x1, short y1) {
    SetPort((GrafPtr)ctx);
    MoveTo(x0, y0);
    LineTo(x1, y1);
}

static void qd_frame_rect(void* ctx, short l, short t, short r, short b) {
    Rect rr;
    SetPort((GrafPtr)ctx);
    SetRect(&rr, l, t, r, b);
    FrameRect(&rr);
}

static void qd_get_font_metrics(void* ctx, short* a, short* d, short* h) {
    FontInfo fi;
    SetPort((GrafPtr)ctx);
    GetFontInfo(&fi);
    if (a) *a = fi.ascent;
    if (d) *d = fi.descent;
    if (h) *h = fi.ascent + fi.descent + fi.leading;
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

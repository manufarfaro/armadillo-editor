/*
 * render/draw_qd.h — swappable QuickDraw emitter.
 *
 * The render/layout pass emits drawing operations through a vtable
 * (DrawOps) instead of calling QuickDraw directly. Two implementations
 * fit the same interface:
 *   - Production (render/draw_qd.c): forwards to real QuickDraw on a
 *     supplied GrafPort.
 *   - Tests (test/recorder.c): records every call for assertion.
 *
 * Callers consume DrawOps via a DrawContext (`ops` + opaque `ctx`).
 * The vtable uses only plain types (short / unsigned short / char*) so
 * tests don't need to include QuickDraw headers. Colors are passed as
 * three 16-bit RGB components, matching the layout of Toolbox RGBColor
 * without naming the type.
 */
#ifndef ARMA_DRAW_QD_H
#define ARMA_DRAW_QD_H

typedef struct DrawOps {
    void (*set_font)(void* ctx, short font_id, short size,
                     unsigned char face);
    void (*set_fg)(void* ctx, unsigned short red,
                   unsigned short green, unsigned short blue);
    void (*move_to)(void* ctx, short h, short v);
    void (*draw_text)(void* ctx, const char* bytes, unsigned short length);
    void (*line)(void* ctx, short x0, short y0, short x1, short y1);
    void (*frame_rect)(void* ctx, short l, short t, short r, short b);
    void (*get_font_metrics)(void* ctx, short* out_ascent,
                             short* out_descent, short* out_line_height);
} DrawOps;

typedef struct DrawContext {
    const DrawOps* ops;
    void*          ctx;
} DrawContext;

/* Build a DrawContext that emits into the supplied Toolbox GrafPort.
 * The port pointer is opaque here (vendor-free header) — caller passes
 * a WindowPtr (which IS-A GrafPtr in Toolbox terms) cast to void*.
 * Implementation: render/draw_qd.c. */
DrawContext draw_qd_make_context(void* grafport_opaque);

#endif /* ARMA_DRAW_QD_H */

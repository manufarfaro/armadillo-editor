/*
 * render/draw_qd.h — swappable QuickDraw emitter.
 *
 * The render/layout pass emits drawing operations through a vtable
 * instead of calling QuickDraw directly. Production wires this to real
 * QuickDraw (src/draw_qd_real.c in Plan 2). Tests wire it to a
 * recording sink (test/recorder.c) that captures every call for
 * assertion.
 *
 * The vtable uses only plain types (short/unsigned short/char*) so
 * tests don't need to include QuickDraw headers. The "color" arguments
 * are passed as three 16-bit RGB components (matching real Toolbox
 * RGBColor without naming the type).
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

#endif /* ARMA_DRAW_QD_H */

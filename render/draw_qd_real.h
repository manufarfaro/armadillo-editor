/*
 * render/draw_qd_real.h — real-QuickDraw DrawOps vtable.
 *
 * Production wiring of the DrawOps test seam. The vtable is filled
 * with thin forwarders to QuickDraw + the Color Manager. Used by the
 * Read pane when render_layout_and_draw runs against a real GrafPort.
 *
 * Plan 2a creates this file but does NOT yet wire it into win_editor —
 * the read pane is implemented in Plan 2b. Until then this is a
 * compile-only target so it can't drift relative to DrawOps' shape.
 */
#ifndef ARMA_DRAW_QD_REAL_H
#define ARMA_DRAW_QD_REAL_H

#include "render/draw_qd.h"

/* Build a DrawContext that draws into the supplied GrafPort.
 * The port pointer is opaque here (vendor-free header) — caller passes
 * a WindowPtr (which IS-A GrafPtr in Toolbox terms) cast to void*. */
DrawContext draw_qd_real_make_context(void* grafport_opaque);

#endif /* ARMA_DRAW_QD_REAL_H */

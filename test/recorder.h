/*
 * test/recorder.h — recording DrawOps for render unit tests.
 *
 * Wraps a DrawContext whose ops record every call into a growable
 * array for assertion. Allocates the RecordedCall[] on the host heap
 * (via malloc) since tests don't need to exercise the arena for this.
 */
#ifndef ARMA_TEST_RECORDER_H
#define ARMA_TEST_RECORDER_H

#include <stddef.h>
#include "render/draw_qd.h"

typedef enum {
    kRecSetFont = 1,
    kRecSetFg,
    kRecMoveTo,
    kRecDrawText,
    kRecLine,
    kRecFrameRect,
    kRecGetFontMetrics
} RecOpKind;

typedef struct RecCall {
    RecOpKind op;
    /* union-ish fields — only those relevant to .op are meaningful */
    short font_id;
    short font_size;
    unsigned char font_face;
    unsigned short color_r, color_g, color_b;
    short x, y, x2, y2;
    short rect_l, rect_t, rect_r, rect_b;
    char  text[256];
    unsigned short text_len;
} RecCall;

typedef struct Recorder {
    RecCall* calls;
    size_t   count;
    size_t   capacity;
    /* Font metrics returned by get_font_metrics — tests can override. */
    short    metrics_ascent;
    short    metrics_descent;
    short    metrics_line_height;
} Recorder;

DrawContext recorder_context(Recorder* r);   /* wire ops into a DrawContext */
void        recorder_free(Recorder* r);

#endif /* ARMA_TEST_RECORDER_H */

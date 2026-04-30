/*
 * render/render.h — flat block model construction + layout + draw.
 *
 * RenderModel is built by feeding an MdParseSink (from mdparse) events
 * into render_model_sink(). After construction, render_layout_and_draw
 * walks the model and emits through a DrawContext.
 */
#ifndef ARMA_RENDER_H
#define ARMA_RENDER_H

#include <stddef.h>
#include "arena.h"
#include "blocks.h"
#include "draw_qd.h"

/* Forward declare MdParseSink from mdparse — consumers link against
 * mdparse's typedef. This header doesn't include mdparse.h to avoid
 * a dependency cycle (mdparse sinks are owned by consumers). */
struct MdParseSink;

typedef struct RenderModel RenderModel;    /* opaque */

typedef enum {
    kRenderOk             =  0,
    kRenderErrArenaOOM    = -1,
    kRenderErrBadModel    = -2,
    kRenderErrEmitAborted = -3
} RenderError;

typedef struct LayoutParams {
    short content_width;    /* px; total usable content area width       */
    short indent_list;      /* px per list_depth step                     */
    short indent_quote;     /* px per quote_depth step                    */
    short left_margin;      /* px from the window's left edge             */
    short top_margin;       /* px from the window's top edge              */
    short block_spacing;    /* px gap between blocks                      */
} LayoutParams;

/* Construct a fresh model inside the given arena. */
RenderModel* render_model_new(Arena* a);

/* Return the MdParseSink to plug into mdparse_run for this model. */
const struct MdParseSink* render_model_sink(RenderModel* m);

/* Query the model after parse. */
size_t render_model_block_count(const RenderModel* m);
const Block* render_model_block_at(const RenderModel* m, size_t i);

/* Defaults helper — returns sensible values for a 480px-wide window. */
LayoutParams layout_params_defaults(void);

/* Walk the model and emit through the DrawContext. */
void render_layout_and_draw(const RenderModel* m,
                            const LayoutParams* params,
                            DrawContext* ctx);

#endif /* ARMA_RENDER_H */

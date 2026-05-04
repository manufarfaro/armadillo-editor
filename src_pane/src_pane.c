/*
 * src_pane/src_pane.c — TextEdit-backed source pane.
 *
 * Plan 2a wires TENew + the basic event handlers (TEKey, TEClick,
 * TEActivate, TEDeactivate, TEUpdate). Style-run application is
 * Plan 2b (TESetStyle drives it once the scanner produces runs).
 */
#include "src_pane.h"
#include <TextEdit.h>
#include <Quickdraw.h>
#include <stdlib.h>
#include <string.h>

struct SrcPane {
    TEHandle            te;
    Rect                view_rect;
    void*               window_ref;
    MacSyscalls         sys;
    SrcPaneEditCallback edit_cb;
    void*               edit_cb_ctx;
};

SrcPane* src_pane_new(const SrcPaneParams* params, const MacSyscalls* sys) {
    SrcPane* p;
    Rect     view, dest;
    GrafPtr  saved;

    if (!params || !sys) return 0;
    p = (SrcPane*)calloc(1, sizeof(SrcPane));
    if (!p) return 0;
    p->sys        = *sys;
    p->window_ref = params->window_ref;

    SetRect(&view,
            params->left, params->top,
            params->right, params->bottom);
    p->view_rect = view;
    dest = view;
    InsetRect(&dest, 4, 4);     /* small interior margin */

    GetPort(&saved);
    SetPort((WindowPtr)params->window_ref);
    /* Style-aware TextEdit: lets src_pane_apply_runs use TESetStyle.
     * (Plain TENew would silently no-op style operations.) */
    p->te = TEStyleNew(&dest, &view);
    SetPort(saved);
    if (!p->te) { free(p); return 0; }

    return p;
}

void src_pane_free(SrcPane* p) {
    if (!p) return;
    if (p->te) TEDispose(p->te);
    free(p);
}

const char* src_pane_get_text(const SrcPane* p, unsigned short* out_len) {
    if (!p || !p->te) {
        if (out_len) *out_len = 0;
        return "";
    }
    {
        Handle  h   = (**p->te).hText;
        long    len = (**p->te).teLength;
        if (out_len) *out_len = (unsigned short)(len > 0xFFFF ? 0xFFFF : len);
        return h ? *(const char**)h : "";
    }
}

void src_pane_set_text(SrcPane* p, const char* bytes, unsigned short len) {
    if (!p || !p->te) return;
    TESetText((const Ptr)bytes, len, p->te);
    TECalText(p->te);
    InvalRect(&p->view_rect);
}

void src_pane_apply_runs(SrcPane* p, const MdStyleRun* runs, size_t count) {
    short  orig_start, orig_end;
    size_t i;
    long   te_len;

    if (!p || !p->te || !runs || count == 0) return;

    /* Save selection to restore after styling (TESetStyle operates on
     * the current selection). */
    orig_start = (**p->te).selStart;
    orig_end   = (**p->te).selEnd;
    te_len     = (**p->te).teLength;

    /* Reset everything to plain Geneva 12 first; per-run styles below
     * override only the attributes the run cares about. */
    {
        TextStyle base;
        base.tsFont  = kFontIDGeneva;
        base.tsFace  = 0;
        base.tsSize  = 12;
        base.tsColor.red = base.tsColor.green = base.tsColor.blue = 0;
        TESetSelect(0, (short)te_len, p->te);
        TESetStyle(doFont | doFace | doSize | doColor, &base, false, p->te);
    }

    for (i = 0; i < count; i++) {
        const MdStyleRun* r = &runs[i];
        TextStyle ts;
        short doFlags = 0;
        short start_off, end_off;

        ts.tsFont = kFontIDGeneva;
        ts.tsFace = 0;
        ts.tsSize = 12;
        ts.tsColor.red = ts.tsColor.green = ts.tsColor.blue = 0;

        switch (r->kind) {
        case kStyleEmph:
            ts.tsFace = italic;
            doFlags = doFace;
            break;
        case kStyleStrong:
            ts.tsFace = bold;
            doFlags = doFace;
            break;
        case kStyleCodeSpan:
            ts.tsFont = kFontIDMonaco;
            ts.tsSize = 10;
            doFlags = doFont | doSize;
            break;
        case kStyleLink:
            ts.tsFace = underline;
            ts.tsColor.red   = 0x0000;
            ts.tsColor.green = 0x4000;
            ts.tsColor.blue  = 0xFFFF;        /* sky blue */
            doFlags = doFace | doColor;
            break;
        case kStyleHtmlSpan:
            ts.tsFont = kFontIDMonaco;
            ts.tsSize = 10;
            ts.tsColor.red   = 0x6000;
            ts.tsColor.green = 0x0000;
            ts.tsColor.blue  = 0x8000;        /* purple — distinct from links */
            doFlags = doFont | doSize | doColor;
            break;
        default:
            continue;                          /* kStylePlain: no override */
        }

        start_off = (short)r->start;
        end_off   = (short)(r->start + r->length);
        TESetSelect(start_off, end_off, p->te);
        TESetStyle(doFlags, &ts, false, p->te);
    }

    TESetSelect(orig_start, orig_end, p->te);
    InvalRect(&p->view_rect);
}

void src_pane_get_selection(const SrcPane* p,
                            unsigned short* out_start,
                            unsigned short* out_end) {
    if (!p || !p->te) {
        if (out_start) *out_start = 0;
        if (out_end)   *out_end   = 0;
        return;
    }
    if (out_start) *out_start = (unsigned short)(**p->te).selStart;
    if (out_end)   *out_end   = (unsigned short)(**p->te).selEnd;
}

void src_pane_set_selection(SrcPane* p,
                            unsigned short start,
                            unsigned short end) {
    if (!p || !p->te) return;
    TESetSelect(start, end, p->te);
}

void src_pane_on_mouse_down(SrcPane* p, short h, short v, short modifiers) {
    Point pt;
    if (!p || !p->te) return;
    pt.h = h; pt.v = v;
    TEClick(pt, (modifiers & shiftKey) != 0, p->te);
}

void src_pane_on_key(SrcPane* p, short char_code, short key_code,
                     short modifiers) {
    if (!p || !p->te) return;
    (void)key_code; (void)modifiers;
    TEKey((char)char_code, p->te);
}

void src_pane_on_activate(SrcPane* p, int is_active) {
    if (!p || !p->te) return;
    if (is_active) TEActivate(p->te); else TEDeactivate(p->te);
}

void src_pane_on_update(SrcPane* p) {
    if (!p || !p->te) return;
    EraseRect(&p->view_rect);
    TEUpdate(&p->view_rect, p->te);
}

void src_pane_on_idle(SrcPane* p) {
    if (!p || !p->te) return;
    TEIdle(p->te);
}

void src_pane_set_edit_callback(SrcPane* p, SrcPaneEditCallback cb,
                                void* ctx) {
    if (!p) return;
    p->edit_cb     = cb;
    p->edit_cb_ctx = ctx;
}

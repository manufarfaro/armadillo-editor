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
    TEHandle    te;
    Rect        view_rect;
    void*       window_ref;
    MacSyscalls sys;
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
    p->te = TENew(&dest, &view);
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
    /* Plan 2b walks runs[] and calls TESetStyle. */
    (void)p; (void)runs; (void)count;
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
    /* Plan 2b stores cb/ctx and fires after TEKey produces a doc
     * mutation, so debounce_on_edit can observe the keystroke. */
    (void)p; (void)cb; (void)ctx;
}

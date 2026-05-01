/*
 * src/win_editor.c — editor window (Plan 2a skeleton).
 *
 * Plan 2a fills in: WIND 128 open in win_editor_new, SrcPane creation
 * sized to the window's content area, event dispatch to the SrcPane.
 * Plan 2b layers on the parse cycle drive and view toggle.
 *
 * MacSyscalls is held by value (per the long-lived-owners convention
 * established in arena-owns-macsyscalls).
 */
#include "win_editor.h"
#include "src_pane/src_pane.h"
#include "src/doc.h"
#include <stdlib.h>

struct WinEditor {
    void*       window_ref;     /* WindowPtr, type-erased */
    Doc*        doc;
    SrcPane*    src_pane;
    MacSyscalls sys;            /* by-value snapshot */
};

WinEditor* win_editor_new(const MacSyscalls* sys) {
    WinEditor* w;
    if (!sys) return 0;
    w = (WinEditor*)calloc(1, sizeof(WinEditor));
    if (!w) return 0;
    w->sys = *sys;
    /* Plan 2a Step 13 fills in: window_ref, doc, src_pane. */
    return w;
}

void win_editor_free(WinEditor* w) {
    if (!w) return;
    if (w->src_pane) src_pane_free(w->src_pane);
    if (w->doc)      doc_free(w->doc);
    /* window_ref disposal added in the wiring task. */
    free(w);
}

void* win_editor_window_ref(const WinEditor* w) {
    return w ? w->window_ref : 0;
}

void win_editor_handle_event(WinEditor* w, const void* event_record) {
    (void)w; (void)event_record;
}

int win_editor_close(WinEditor* w) {
    (void)w;
    return 1;
}

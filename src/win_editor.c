/*
 * src/win_editor.c — editor window controller.
 *
 * Plan 2a wires:
 *   - win_editor_new opens WIND 128, builds an empty Doc, and creates
 *     the SrcPane sized to the window's content area.
 *   - win_editor_handle_event routes mouseDown / keyDown / updateEvt /
 *     activateEvt to the SrcPane (Step 14 fills the pane handlers).
 *   - win_editor_close disposes the window, the SrcPane, and the Doc.
 */
#include "win_editor.h"
#include "src_pane/src_pane.h"
#include "src/doc.h"
#include <Windows.h>
#include <Events.h>
#include <stdlib.h>

struct WinEditor {
    void*       window_ref;     /* WindowPtr, type-erased */
    Doc*        doc;
    SrcPane*    src_pane;
    MacSyscalls sys;
};

WinEditor* win_editor_new(const MacSyscalls* sys) {
    WinEditor*    w;
    WindowPtr     wp;
    SrcPaneParams params;
    Rect          content;

    if (!sys) return 0;

    w = (WinEditor*)calloc(1, sizeof(WinEditor));
    if (!w) return 0;
    w->sys = *sys;

    wp = GetNewWindow(128, 0L, (WindowPtr)-1L);
    if (!wp) { free(w); return 0; }
    w->window_ref = (void*)wp;
    SetPort(wp);
    ShowWindow(wp);

    w->doc = doc_new();
    if (!w->doc) { DisposeWindow(wp); free(w); return 0; }

    /* Source pane fills the entire content area for now (no toolbar yet). */
    content = wp->portRect;
    params.left      = content.left;
    params.top       = content.top;
    params.right     = content.right;
    params.bottom    = content.bottom;
    params.window_ref = (void*)wp;
    w->src_pane = src_pane_new(&params, sys);
    if (!w->src_pane) {
        doc_free(w->doc);
        DisposeWindow(wp);
        free(w);
        return 0;
    }

    return w;
}

void win_editor_free(WinEditor* w) {
    if (!w) return;
    if (w->src_pane) src_pane_free(w->src_pane);
    if (w->doc)      doc_free(w->doc);
    if (w->window_ref) DisposeWindow((WindowPtr)w->window_ref);
    free(w);
}

void* win_editor_window_ref(const WinEditor* w) {
    return w ? w->window_ref : 0;
}

void win_editor_handle_event(WinEditor* w, const void* event_record) {
    const EventRecord* ev = (const EventRecord*)event_record;
    if (!w || !ev) return;

    switch (ev->what) {
    case updateEvt:
        BeginUpdate((WindowPtr)w->window_ref);
        src_pane_on_update(w->src_pane);
        EndUpdate((WindowPtr)w->window_ref);
        break;
    case activateEvt:
        src_pane_on_activate(w->src_pane,
                             (ev->modifiers & activeFlag) != 0);
        break;
    case mouseDown: {
        Point pt = ev->where;
        GlobalToLocal(&pt);
        src_pane_on_mouse_down(w->src_pane, pt.h, pt.v, ev->modifiers);
        break;
    }
    case keyDown:
    case autoKey: {
        char ch = ev->message & charCodeMask;
        src_pane_on_key(w->src_pane, ch,
                        (ev->message & keyCodeMask) >> 8,
                        ev->modifiers);
        break;
    }
    default: break;
    }
}

int win_editor_close(WinEditor* w) {
    /* Plan 2a: no dirty guard. Plan 2b adds StopAlert(257). */
    win_editor_free(w);
    return 1;
}

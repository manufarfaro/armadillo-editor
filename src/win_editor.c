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
#include "src/debounce.h"
#include "render/arena.h"
#include "render/render.h"
#include "render/draw_qd.h"
#include "scanner/scanner.h"
#include "mdparse/mdparse.h"
#include "src/file_io.h"
#include <Windows.h>
#include <Events.h>
#include <Menus.h>
#include <Dialogs.h>
#include <stdlib.h>
#include <string.h>

#define kArenaInitialSize (8u * 1024u)

struct WinEditor {
    void*         window_ref;     /* WindowPtr, type-erased */
    Doc*          doc;
    SrcPane*      src_pane;
    MacSyscalls   sys;
    Arena*        arena;
    RenderModel*  current_model;  /* lives inside arena; reset between cycles */
    DebounceState debounce;
    WinMode       mode;
};

static void win_editor_on_edit(void* ctx) {
    WinEditor* w = (WinEditor*)ctx;
    if (w) debounce_on_edit(&w->debounce, &w->sys);
}

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
    if (!w->doc) goto fail;

    if (arena_init(&w->arena, kArenaInitialSize, sys) != 0) goto fail;

    content = wp->portRect;
    params.left      = content.left;
    params.top       = content.top;
    params.right     = content.right;
    params.bottom    = content.bottom;
    params.window_ref = (void*)wp;
    w->src_pane = src_pane_new(&params, sys);
    if (!w->src_pane) goto fail;
    src_pane_set_edit_callback(w->src_pane, win_editor_on_edit, w);

    return w;

fail:
    if (w->src_pane) src_pane_free(w->src_pane);
    if (w->arena)    arena_destroy(w->arena);
    if (w->doc)      doc_free(w->doc);
    DisposeWindow(wp);
    free(w);
    return 0;
}

void win_editor_free(WinEditor* w) {
    if (!w) return;
    if (w->src_pane) src_pane_free(w->src_pane);
    if (w->arena)    arena_destroy(w->arena);
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
        if (w->mode == kWinModeRead && w->current_model) {
            DrawContext  ctx = draw_qd_make_context(w->window_ref);
            LayoutParams params = layout_params_defaults();
            EraseRect(&((WindowPtr)w->window_ref)->portRect);
            (void)render_layout_and_draw(w->current_model, &params, &ctx);
        } else {
            src_pane_on_update(w->src_pane);
        }
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
    short hit;
    int   save_rc;

    if (!w) return 1;
    if (!w->doc || !doc_is_dirty(w->doc)) {
        win_editor_free(w);
        return 1;
    }

    /* StopAlert 257 — DITL items: 1=Save, 2=static text, 3="Don't Save",
     * 4=Cancel. Save uses file_io_save with fall-through to Save As if
     * the doc has no stored FSSpec yet. */
    hit = StopAlert(257, 0L);
    switch (hit) {
    case 1:                                   /* Save */
        save_rc = file_io_save(w->doc, &w->sys);
        if (save_rc == kFileIoErrOpen) {
            save_rc = file_io_save_as(w->doc, &w->sys);
        }
        if (save_rc != kFileIoOk) return 0;   /* save failed/cancelled */
        win_editor_free(w);
        return 1;
    case 3:                                   /* Don't Save */
        win_editor_free(w);
        return 1;
    case 4:                                   /* Cancel */
    default:
        return 0;
    }
}

Doc* win_editor_doc(WinEditor* w) {
    return w ? w->doc : 0;
}

void win_editor_set_doc(WinEditor* w, Doc* new_doc) {
    if (!w || !new_doc) return;
    if (w->doc && w->doc != new_doc) doc_free(w->doc);
    w->doc = new_doc;
    if (w->src_pane) {
        unsigned short len = 0;
        const char* text = doc_text(new_doc, &len);
        src_pane_set_text(w->src_pane, text, len);
    }
    if (w->window_ref) InvalRect(&((WindowPtr)w->window_ref)->portRect);
}

DebounceState* win_editor_debounce_state(WinEditor* w) {
    return w ? &w->debounce : 0;
}

WinMode win_editor_mode(const WinEditor* w) {
    return w ? w->mode : kWinModeSource;
}

void win_editor_set_mode(WinEditor* w, WinMode mode) {
    MenuHandle view_menu;
    if (!w) return;
    if (w->mode == mode) return;
    w->mode = mode;
    /* Activate / deactivate the source pane so its caret blinks only
     * when it's the visible mode. */
    if (w->src_pane) {
        src_pane_on_activate(w->src_pane, mode == kWinModeSource);
    }
    /* Update the View menu's check marks (item 1 = Source, 2 = Read). */
    view_menu = GetMenuHandle(131);
    if (view_menu) {
        CheckItem(view_menu, 1, mode == kWinModeSource);
        CheckItem(view_menu, 2, mode == kWinModeRead);
    }
    /* Force a full redraw with the new mode's renderer. */
    if (w->window_ref) {
        InvalRect(&((WindowPtr)w->window_ref)->portRect);
    }
}

void win_editor_run_parse(WinEditor* w) {
    unsigned short     text_len = 0;
    const char*        text;
    Scanner*           scanner;
    MdParseSink        sinks[2];
    const MdParseSink* model_sink;
    const MdParseSink* scan_sink;
    size_t             grow_bytes;
    int                rc;

    if (!w || !w->src_pane || !w->arena || !w->doc) return;

    /* Sync doc from src_pane (TextEdit's buffer is the source of truth
     * during editing; the Doc's copy is the snapshot we parse). */
    text = src_pane_get_text(w->src_pane, &text_len);
    doc_set_text(w->doc, text, text_len);

    /* Old model lived in the arena that we're about to reset; clear
     * the pointer before reset so a concurrent Read-pane redraw can't
     * see freed memory. */
    w->current_model = 0;
    arena_reset(w->arena);

    /* Worst-case sizing: per design.md §4, source_len * 4 + 16 KB
     * scratch covers blocks + style runs + text slices. */
    grow_bytes = (size_t)text_len * 4u + 16384u;
    if (arena_ensure(w->arena, grow_bytes) != 0) return;

    scanner          = scanner_new(w->arena);
    w->current_model = render_model_new(w->arena);
    if (!scanner || !w->current_model) { w->current_model = 0; return; }

    model_sink = render_model_sink(w->current_model);
    scan_sink  = scanner_sink(scanner);
    sinks[0]   = *model_sink;
    sinks[1]   = *scan_sink;

    rc = mdparse_run(text, text_len, sinks, 2);
    if (rc != kMdParseOk) { w->current_model = 0; return; }

    /* Apply syntax-coloring runs to the source pane. Plan 2c fills
     * src_pane_apply_runs in; until then this is a no-op. */
    {
        size_t            run_count = 0;
        const MdStyleRun* runs      = scanner_runs(scanner, &run_count);
        src_pane_apply_runs(w->src_pane, runs, run_count);
    }

    /* Invalidate so the Read pane (Plan 2d) redraws on next updateEvt. */
    if (w->window_ref) {
        InvalRect(&((WindowPtr)w->window_ref)->portRect);
    }
}

void win_editor_refresh_title(WinEditor* w) {
    Str255         title;
    unsigned char  fn_len = 0;
    const char*    fn;
    static const char kDefaultTitle[] = "Untitled.md";

    if (!w || !w->window_ref) return;
    fn = doc_filename(w->doc, &fn_len);
    if (fn_len > 0) {
        title[0] = fn_len;
        memcpy(title + 1, fn, fn_len);
    } else {
        title[0] = (unsigned char)(sizeof kDefaultTitle - 1);
        memcpy(title + 1, kDefaultTitle, sizeof kDefaultTitle - 1);
    }
    SetWTitle((WindowPtr)w->window_ref, title);
}

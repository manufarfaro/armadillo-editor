/*
 * src/win_editor.h — editor window (one .md document at a time).
 *
 * Vendor-free public API. Owns one Doc*, one SrcPane*, and (Plan 2b)
 * one RenderModel* + one Arena*. Holds MacSyscalls by value because
 * the editor outlives any single function call.
 *
 * Plan 2a: window opens with an empty Doc and an empty TE-backed
 * SrcPane. handle_event routes mouse/key/update/activate to the right
 * sub-component. close() and free() tear it down cleanly.
 *
 * Plan 2b adds: Doc swap on file open, parse cycle drive, view toggle,
 * unsaved-changes guard before close.
 */
#ifndef ARMA_WIN_EDITOR_H
#define ARMA_WIN_EDITOR_H

#include "src/mac_syscalls.h"

typedef struct WinEditor WinEditor;

/* Open the editor window from WIND 128 with an empty Doc.
 * Returns NULL on failure (resource missing, OOM, or TENew failure). */
WinEditor* win_editor_new(const MacSyscalls* sys);

/* Free everything: SrcPane, Doc, the window. NULL-safe. */
void win_editor_free(WinEditor* w);

/* The editor's Mac WindowPtr, type-erased. Callers cast back inside
 * their own Toolbox-aware .c file. */
void* win_editor_window_ref(const WinEditor* w);

/* Forward-declared opaque Doc — defined in src/doc.h. We forward-decl
 * here rather than including doc.h to keep this header lean for
 * callers that don't otherwise touch Doc. */
struct Doc;
typedef struct Doc Doc;

/* Borrowed pointer to the editor's current Doc. Caller MUST NOT free
 * — the WinEditor owns its lifetime. */
Doc* win_editor_doc(WinEditor* w);

/* Replace the editor's current Doc with `new_doc`. The old Doc is
 * freed; `new_doc` ownership transfers to the WinEditor. The source
 * pane's text is updated to match the new Doc; the window is
 * invalidated for redraw. NULL `new_doc` is a no-op. */
void win_editor_set_doc(WinEditor* w, Doc* new_doc);

/* Re-read the Doc's filename and update the window's title to
 * match (falls back to "Untitled.md" when the Doc has no filename). */
void win_editor_refresh_title(WinEditor* w);

/* Dispatch a Mac event (mouseDown/keyDown/updateEvt/activateEvt) that
 * has already been determined to belong to this window. The event
 * pointer is opaque here (vendor-free header). */
void win_editor_handle_event(WinEditor* w, const void* event_record);

/* User-initiated close. Plan 2a: just frees the window. Plan 2b: shows
 * the unsaved-changes alert if the doc is dirty. Returns 1 if the
 * close happened, 0 if the user cancelled. */
int win_editor_close(WinEditor* w);

#endif /* ARMA_WIN_EDITOR_H */

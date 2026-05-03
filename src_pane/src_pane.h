/*
 * src_pane/src_pane.h — editable source-text pane (vendor-free API).
 *
 * Opaque SrcPane wrapper around whichever text engine is in use
 * internally (MVP: Mac Toolbox TextEdit; future: custom piece-table).
 * No Toolbox types (TEHandle, WindowPtr) appear in this header — the
 * window_ref on SrcPaneParams is passed as void* and cast inside the
 * implementation.
 *
 * Implementation lives in Plan 2 (src_pane/src_pane.c — target-only).
 * Host tests don't cover src_pane internals; the type is opaque and
 * callers mock SrcPane* as a NULL pointer when verifying their own
 * logic.
 */
#ifndef ARMA_SRC_PANE_H
#define ARMA_SRC_PANE_H

#include <stddef.h>
#include "render/inlines.h"
#include "src/mac_syscalls.h"

typedef struct SrcPaneParams {
    short left;
    short top;
    short right;
    short bottom;
    void* window_ref;       /* opaque; internally cast to WindowPtr */
} SrcPaneParams;

typedef struct SrcPane SrcPane;    /* opaque */

typedef void (*SrcPaneEditCallback)(void* ctx);

SrcPane* src_pane_new(const SrcPaneParams* params, const MacSyscalls* sys);
void     src_pane_free(SrcPane* p);

const char* src_pane_get_text(const SrcPane* p, unsigned short* out_len);
void        src_pane_set_text(SrcPane* p, const char* bytes,
                              unsigned short len);

void src_pane_apply_runs(SrcPane* p, const MdStyleRun* runs, size_t count);

void src_pane_get_selection(const SrcPane* p,
                            unsigned short* out_start,
                            unsigned short* out_end);
void src_pane_set_selection(SrcPane* p,
                            unsigned short start,
                            unsigned short end);

void src_pane_on_mouse_down(SrcPane* p, short h, short v, short modifiers);
void src_pane_on_key(SrcPane* p, short char_code, short key_code,
                     short modifiers);
void src_pane_on_activate(SrcPane* p, int is_active);
void src_pane_on_update(SrcPane* p);
void src_pane_on_idle(SrcPane* p);

void src_pane_set_edit_callback(SrcPane* p, SrcPaneEditCallback cb,
                                void* ctx);

#endif /* ARMA_SRC_PANE_H */

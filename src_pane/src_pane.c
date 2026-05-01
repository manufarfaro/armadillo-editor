/*
 * src_pane/src_pane.c — TextEdit-backed source pane (Plan 2a skeleton).
 *
 * Header lives in src_pane/src_pane.h (vendor-free; opaque SrcPane*).
 * Plan 2a's TENew + event handlers go in the wiring tasks below; this
 * file's first commit is just stubs so the .APPL links.
 *
 * Implementation choice (per CLAUDE.md): MVP uses Mac Toolbox TextEdit
 * under the hood. A future custom piece-table engine will swap in
 * behind the same opaque API without touching callers.
 */
#include "src_pane.h"
#include <stdlib.h>

struct SrcPane {
    /* Filled in by Plan 2a's TENew wiring task. */
    int placeholder;
};

SrcPane* src_pane_new(const SrcPaneParams* params, const MacSyscalls* sys) {
    (void)params; (void)sys;
    return (SrcPane*)calloc(1, sizeof(SrcPane));
}

void src_pane_free(SrcPane* p) {
    free(p);
}

const char* src_pane_get_text(const SrcPane* p, unsigned short* out_len) {
    (void)p;
    if (out_len) *out_len = 0;
    return "";
}

void src_pane_set_text(SrcPane* p, const char* bytes, unsigned short len) {
    (void)p; (void)bytes; (void)len;
}

void src_pane_apply_runs(SrcPane* p, const StyleRun* runs, size_t count) {
    (void)p; (void)runs; (void)count;
}

void src_pane_get_selection(const SrcPane* p,
                            unsigned short* out_start,
                            unsigned short* out_end) {
    (void)p;
    if (out_start) *out_start = 0;
    if (out_end)   *out_end   = 0;
}

void src_pane_set_selection(SrcPane* p,
                            unsigned short start,
                            unsigned short end) {
    (void)p; (void)start; (void)end;
}

void src_pane_on_mouse_down(SrcPane* p, short h, short v, short modifiers) {
    (void)p; (void)h; (void)v; (void)modifiers;
}

void src_pane_on_key(SrcPane* p, short char_code, short key_code,
                     short modifiers) {
    (void)p; (void)char_code; (void)key_code; (void)modifiers;
}

void src_pane_on_activate(SrcPane* p, int is_active) {
    (void)p; (void)is_active;
}

void src_pane_on_update(SrcPane* p) {
    (void)p;
}

void src_pane_on_idle(SrcPane* p) {
    (void)p;
}

void src_pane_set_edit_callback(SrcPane* p, SrcPaneEditCallback cb,
                                void* ctx) {
    (void)p; (void)cb; (void)ctx;
}

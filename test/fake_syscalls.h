/*
 * test/fake_syscalls.h — host-test fakes for MacSyscalls.
 *
 * FakeSyscalls is a MacSyscalls with controllable behavior. Cast
 * (const MacSyscalls*) &fake.vtable when passing to code under test;
 * the vtable field MUST be first in the struct so the cast is valid.
 */
#ifndef ARMA_FAKE_SYSCALLS_H
#define ARMA_FAKE_SYSCALLS_H

#include "src/mac_syscalls.h"

typedef struct FakeSyscalls {
    MacSyscalls vtable;   /* MUST be first — callers cast &fake.vtable */

    /* Controls (set before invoking code under test) */
    unsigned long tick_count_now;
    int           set_handle_size_fail_after;  /* -1 = never; N = Nth call fails */
    short         fsp_open_df_result;          /* 0 = ok; else errno */
    short         fs_read_result;
    short         fs_write_result;
    short         get_eof_result;
    long          get_eof_size;                /* value returned via *out_size */
    short         mem_error_result;
    short         new_handle_fail_after;       /* -1 = never */
    short         gestalt_result;
    long          gestalt_response;            /* value returned via *out_response */

    /* Call counters (read after to assert) */
    int new_handle_calls;
    int dispose_handle_calls;
    int hlock_calls;
    int hunlock_calls;
    int set_handle_size_calls;
    int fsp_open_df_calls;
    int fs_close_calls;
    int fs_read_calls;
    int fs_write_calls;
    int get_eof_calls;
    int set_eof_calls;
    int set_fpos_calls;
    int fsp_create_calls;
    int gestalt_calls;
    int note_alert_calls;
    int stop_alert_calls;
} FakeSyscalls;

/* Returns a FakeSyscalls with defaults wired (all succeed, counters 0). */
FakeSyscalls fake_syscalls_init(void);

/* Mark `f` as the currently-active fake, so the callback functions see
 * its counters and controls. Call after fake_syscalls_init() and
 * before passing the fake to code under test. */
void fake_syscalls_activate(FakeSyscalls* f);

#endif /* ARMA_FAKE_SYSCALLS_H */

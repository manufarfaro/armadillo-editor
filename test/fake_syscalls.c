/*
 * test/fake_syscalls.c — fake MacSyscalls for host unit tests.
 *
 * Backing Handles are malloc-backed. Each call increments a counter
 * and respects its fail-after control. A "Handle" here is a pointer
 * to a pointer to malloc'd memory (two-level indirection, mirroring
 * the real Mac Memory Manager shape).
 */
#include "fake_syscalls.h"
#include <stdlib.h>
#include <string.h>

/* The real MacSyscalls struct doesn't carry a back-pointer to our fake,
 * so we use a file-scope singleton for the call-counter state. This
 * means FakeSyscalls is NOT reentrant — one test at a time. Good
 * enough for deterministic host unit tests. */
static FakeSyscalls* g_current = 0;

static unsigned long fake_tick_count(void) {
    return g_current ? g_current->tick_count_now : 0;
}

static void* fake_new_handle(long size) {
    if (!g_current) return 0;
    g_current->new_handle_calls++;
    if (g_current->new_handle_fail_after >= 0 &&
        g_current->new_handle_calls > g_current->new_handle_fail_after) {
        return 0;
    }
    /* Double-indirect: handle = malloc(sizeof(void*)); *handle = malloc(size) */
    {
        void** h = (void**)malloc(sizeof(void*));
        if (!h) return 0;
        *h = malloc((size_t)size);
        if (!*h) { free(h); return 0; }
        return h;
    }
}

static void fake_dispose_handle(void* handle) {
    if (!g_current) return;
    g_current->dispose_handle_calls++;
    if (handle) {
        void** h = (void**)handle;
        if (*h) free(*h);
        free(h);
    }
}

static void fake_hlock(void* handle) {
    if (g_current) g_current->hlock_calls++;
    (void)handle;  /* no-op; malloc'd memory is already "locked" */
}

static void fake_hunlock(void* handle) {
    if (g_current) g_current->hunlock_calls++;
    (void)handle;
}

static int fake_set_handle_size(void* handle, long size) {
    void** h;
    void* new_ptr;
    if (!g_current) return -1;
    g_current->set_handle_size_calls++;
    if (g_current->set_handle_size_fail_after >= 0 &&
        g_current->set_handle_size_calls > g_current->set_handle_size_fail_after) {
        return -1;
    }
    h = (void**)handle;
    new_ptr = realloc(*h, (size_t)size);
    if (!new_ptr && size > 0) return -1;
    *h = new_ptr;
    return 0;
}

static short fake_mem_error(void) {
    return g_current ? g_current->mem_error_result : 0;
}

static short fake_fsp_open_df(const void* spec, char perm, short* out_ref) {
    if (g_current) g_current->fsp_open_df_calls++;
    (void)spec; (void)perm;
    if (out_ref) *out_ref = 42;
    return g_current ? g_current->fsp_open_df_result : 0;
}

static short fake_fs_close(short ref) {
    if (g_current) g_current->fs_close_calls++;
    (void)ref;
    return 0;
}

static short fake_fs_read(short ref, long* io_count, void* buf) {
    if (g_current) g_current->fs_read_calls++;
    (void)ref; (void)io_count; (void)buf;
    return g_current ? g_current->fs_read_result : 0;
}

static short fake_fs_write(short ref, long* io_count, const void* buf) {
    if (g_current) g_current->fs_write_calls++;
    (void)ref; (void)io_count; (void)buf;
    return g_current ? g_current->fs_write_result : 0;
}

static short fake_get_eof(short ref, long* out_size) {
    if (g_current) g_current->get_eof_calls++;
    if (out_size) *out_size = g_current ? g_current->get_eof_size : 0;
    (void)ref;
    return g_current ? g_current->get_eof_result : 0;
}

static short fake_set_eof(short ref, long size) {
    if (g_current) g_current->set_eof_calls++;
    (void)ref; (void)size;
    return 0;
}

static short fake_set_fpos(short ref, short mode, long off) {
    if (g_current) g_current->set_fpos_calls++;
    (void)ref; (void)mode; (void)off;
    return 0;
}

static short fake_fsp_create(const void* spec, unsigned long creator,
                             unsigned long type, short script) {
    if (g_current) g_current->fsp_create_calls++;
    (void)spec; (void)creator; (void)type; (void)script;
    return 0;
}

static void fake_standard_get_file(void* out_spec, int* out_good) {
    (void)out_spec;
    if (out_good) *out_good = 0;  /* user always "cancels" in tests */
}

static void fake_standard_put_file(const char* prompt, const char* defname,
                                   void* out_spec, int* out_good) {
    (void)prompt; (void)defname; (void)out_spec;
    if (out_good) *out_good = 0;
}

static short fake_gestalt(unsigned long selector, long* out_response) {
    if (g_current) g_current->gestalt_calls++;
    (void)selector;
    if (out_response) *out_response = g_current ? g_current->gestalt_response : 0;
    return g_current ? g_current->gestalt_result : 0;
}

static short fake_note_alert(short id, void* filter) {
    if (g_current) g_current->note_alert_calls++;
    (void)id; (void)filter;
    return 1;
}

static short fake_stop_alert(short id, void* filter) {
    if (g_current) g_current->stop_alert_calls++;
    (void)id; (void)filter;
    return 1;
}

FakeSyscalls fake_syscalls_init(void) {
    FakeSyscalls f;
    memset(&f, 0, sizeof f);
    f.set_handle_size_fail_after = -1;
    f.new_handle_fail_after      = -1;

    f.vtable.tick_count        = fake_tick_count;
    f.vtable.new_handle        = fake_new_handle;
    f.vtable.dispose_handle    = fake_dispose_handle;
    f.vtable.hlock             = fake_hlock;
    f.vtable.hunlock           = fake_hunlock;
    f.vtable.set_handle_size   = fake_set_handle_size;
    f.vtable.mem_error         = fake_mem_error;
    f.vtable.fsp_open_df       = fake_fsp_open_df;
    f.vtable.fs_close          = fake_fs_close;
    f.vtable.fs_read           = fake_fs_read;
    f.vtable.fs_write          = fake_fs_write;
    f.vtable.get_eof           = fake_get_eof;
    f.vtable.set_eof           = fake_set_eof;
    f.vtable.set_fpos          = fake_set_fpos;
    f.vtable.fsp_create        = fake_fsp_create;
    f.vtable.standard_get_file = fake_standard_get_file;
    f.vtable.standard_put_file = fake_standard_put_file;
    f.vtable.gestalt           = fake_gestalt;
    f.vtable.note_alert        = fake_note_alert;
    f.vtable.stop_alert        = fake_stop_alert;

    g_current = 0;  /* caller must bind via fake_syscalls_activate */
    return f;
}

/* Bind `f` as the currently-active fake. Callers pass a stack-local
 * FakeSyscalls whose address lives as long as the enclosing test
 * function; `g_current` MUST NOT outlive that scope, which is why
 * every test's tearDown calls fake_syscalls_activate(0) to unbind.
 * CodeQL flags the parameter-to-file-static assignment as a "stack
 * address stored in non-local memory" smell — that's the singleton
 * pattern described at the top of this file, not a bug. */
void fake_syscalls_activate(FakeSyscalls* f) {
    g_current = f;
}

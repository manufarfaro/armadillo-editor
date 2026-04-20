/*
 * src/mac_syscalls.h — mockable Mac Toolbox wrappers
 *
 * Every function in this struct corresponds to a Toolbox call we use.
 * Modules that touch the OS take `const MacSyscalls*` and dispatch
 * through it instead of calling the Toolbox directly. Host tests inject
 * a FakeSyscalls struct that satisfies this signature; production code
 * injects a real-Toolbox struct defined in src/app.c (Plan 2).
 *
 * All function-pointer fields use plain types (not Toolbox types) so
 * this header stays compilable on the host without Mac headers. The
 * cost: a few `void*` / `short` / `long` parameters that the real impl
 * casts back to FSSpec*, ParmBlkPtr, etc.
 */
#ifndef ARMA_MAC_SYSCALLS_H
#define ARMA_MAC_SYSCALLS_H

#include <stddef.h>

typedef struct MacSyscalls {
    /* --- Time & events --- */
    unsigned long (*tick_count)(void);                       /* TickCount()            */

    /* --- Memory Manager --- */
    void*  (*new_handle)(long size);                         /* NewHandle()            */
    void   (*dispose_handle)(void* handle);                  /* DisposeHandle()        */
    void   (*hlock)(void* handle);                           /* HLock()                */
    void   (*hunlock)(void* handle);                         /* HUnlock()              */
    int    (*set_handle_size)(void* handle, long size);      /* SetHandleSize(); 0=ok  */
    short  (*mem_error)(void);                               /* MemError()             */

    /* --- File Manager (data path) --- */
    short  (*fsp_open_df)(const void* spec, char perm, short* out_ref);    /* FSpOpenDF    */
    short  (*fs_close)(short ref);                                          /* FSClose      */
    short  (*fs_read)(short ref, long* io_count, void* buf);                /* FSRead       */
    short  (*fs_write)(short ref, long* io_count, const void* buf);         /* FSWrite      */
    short  (*get_eof)(short ref, long* out_size);                           /* GetEOF       */
    short  (*set_eof)(short ref, long size);                                /* SetEOF       */
    short  (*set_fpos)(short ref, short mode, long off);                    /* SetFPos      */
    short  (*fsp_create)(const void* spec, unsigned long creator,
                         unsigned long type, short script);                 /* FSpCreate    */

    /* --- Standard File Package --- */
    /* standard_get_file fills *out_spec with an opaque FSSpec if good,
     * sets *out_good non-zero. Type list is NULL-or-'TEXT'-filtered. */
    void   (*standard_get_file)(void* out_spec, int* out_good);
    void   (*standard_put_file)(const char* prompt_ptr, const char* default_name_ptr,
                                void* out_spec, int* out_good);

    /* --- Gestalt --- */
    short  (*gestalt)(unsigned long selector, long* out_response);

    /* --- Alerts --- */
    short  (*note_alert)(short id, void* filter);                           /* NoteAlert    */
    short  (*stop_alert)(short id, void* filter);                           /* StopAlert    */
} MacSyscalls;

#endif /* ARMA_MAC_SYSCALLS_H */

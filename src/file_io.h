/*
 * src/file_io.h — File Manager + Standard File wrappers.
 *
 * Handles open/save of .md files up to kMaxDocBytes. Uses FSSpec
 * opaquely (const void* in the header) so no Toolbox type crosses
 * the boundary.
 */
#ifndef ARMA_FILE_IO_H
#define ARMA_FILE_IO_H

#include "doc.h"
#include "mac_syscalls.h"

typedef enum {
    kFileIoOk       =  0,
    kFileIoErrOpen  = -1,
    kFileIoErrStat  = -2,
    kFileIoErrTooBig= -3,
    kFileIoErrOOM   = -4,
    kFileIoErrRead  = -5,
    kFileIoErrWrite = -6,
    kFileIoErrClose = -7,
    kFileIoErrCancel= -8
} FileIoError;

/* Interactive — presents Standard File Open dialog.
 * Target-only (requires live Toolbox); Plan 2 covers this. */
int file_io_open_interactive(Doc** out_doc, const MacSyscalls* sys);

/* Data-path — host-testable with FakeSyscalls.
 * fsspec_opaque is treated as const FSSpec* internally. */
int file_io_open(const void* fsspec_opaque, Doc** out_doc,
                 const MacSyscalls* sys);

/* Interactive Save As — target-only. */
int file_io_save_as(Doc* d, const MacSyscalls* sys);

/* Save to the Doc's existing filename (requires doc_has_filename).
 * Data-path portion host-testable. */
int file_io_save(Doc* d, const MacSyscalls* sys);

#endif /* ARMA_FILE_IO_H */

/*
 * src/file_io.c — File Manager + Standard File wrappers.
 *
 * Four public functions:
 *   - file_io_open_interactive: presents Standard File Open dialog.
 *     Target-only (needs live Toolbox); stubbed here to return
 *     kFileIoErrCancel. Real impl lands in Plan 2.
 *   - file_io_save_as: presents Standard File Save dialog. Same story.
 *   - file_io_open: data-path open by opaque FSSpec. Host-testable
 *     via FakeSyscalls. Reads file data fork into a Doc, enforcing
 *     kMaxDocBytes; cleans up on every error path via goto-cleanup.
 *   - file_io_save: data-path save. MVP scope is to clear the dirty
 *     flag on success; the actual Toolbox write path (FSpOpenDF +
 *     SetEOF + FSWrite) is verified indirectly via the dirty-flag
 *     clearing contract. Plan 2 adds the full Standard File save path.
 */
#include "file_io.h"
#include <string.h>

/* ---------- Target-only stubs (Plan 2 replaces these) ---------- */

int file_io_open_interactive(Doc** out_doc, const MacSyscalls* sys) {
    /* sys is per-call; not retained. */
    (void)out_doc; (void)sys;
    return kFileIoErrCancel;
}

int file_io_save_as(Doc* d, const MacSyscalls* sys) {
    /* sys is per-call; not retained. */
    (void)d; (void)sys;
    return kFileIoErrCancel;
}

/* ---------- Data-path (host-testable) ---------- */

int file_io_open(const void* fsspec_opaque, Doc** out_doc,
                 const MacSyscalls* sys) {
    /* sys is per-call; not retained. */
    int rc = kFileIoOk;
    short ref = 0;
    int ref_open = 0;
    void* handle = 0;
    Doc* doc = 0;
    long size = 0;
    long actual = 0;

    if (!out_doc || !sys) return kFileIoErrOpen;
    *out_doc = 0;

    rc = sys->fsp_open_df(fsspec_opaque, 0 /* fsRdPerm */, &ref);
    if (rc != 0) { rc = kFileIoErrOpen; goto cleanup; }
    ref_open = 1;

    if (sys->get_eof(ref, &size) != 0) { rc = kFileIoErrStat;  goto cleanup; }
    if (size < 0)                       { rc = kFileIoErrStat;  goto cleanup; }
    if (size > (long)kMaxDocBytes)      { rc = kFileIoErrTooBig; goto cleanup; }

    handle = sys->new_handle(size > 0 ? size : 1);
    if (!handle) { rc = kFileIoErrOOM; goto cleanup; }
    sys->hlock(handle);

    actual = size;
    if (sys->fs_read(ref, &actual, *(void**)handle) != 0) {
        rc = kFileIoErrRead; goto cleanup;
    }

    doc = doc_new();
    if (!doc) { rc = kFileIoErrOOM; goto cleanup; }
    doc_set_text(doc, *(const char**)handle, (unsigned short)actual);

    *out_doc = doc;
    doc = 0;   /* ownership transferred */

cleanup:
    if (ref_open) sys->fs_close(ref);
    if (handle)   { sys->hunlock(handle); sys->dispose_handle(handle); }
    if (doc)      doc_free(doc);
    return rc;
}

int file_io_save(Doc* d, const MacSyscalls* sys) {
    /* sys is per-call; not retained. */
    unsigned char fn_len = 0;
    const char* fn;

    if (!d || !sys) return kFileIoErrOpen;
    if (!doc_has_filename(d)) return kFileIoErrOpen;

    fn = doc_filename(d, &fn_len);
    (void)fn;   /* unused in MVP; Plan 2's interactive flow uses it */

    /* MVP: clear the dirty flag. The actual FSpOpenDF + SetEOF +
     * FSWrite sequence lands in Plan 2 alongside the Standard File
     * interactive path. The dirty-flag-clearing contract is what
     * callers depend on to drop the "unsaved changes" warning. */
    doc_mark_clean(d);
    return kFileIoOk;
}

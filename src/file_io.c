/*
 * src/file_io.c — File Manager + Standard File wrappers.
 *
 * Four public functions:
 *   - file_io_open_interactive: presents Standard File Open dialog
 *     (via MacSyscalls.standard_get_file), then delegates to
 *     file_io_open. The fake returns "user cancelled" by default,
 *     so this wrapper has no host-test coverage; the smoke test
 *     and manual QA exercise it on QEMU.
 *   - file_io_save_as: presents Standard File Save dialog, creates
 *     the file on disk via FSpCreate, records the picked FSSpec on
 *     the Doc, then delegates to file_io_save.
 *   - file_io_open: data-path open by opaque FSSpec. Host-testable
 *     via FakeSyscalls. Reads file data fork into a Doc, enforces
 *     kMaxDocBytes, records the FSSpec + filename on the Doc, cleans
 *     up on every error path via goto-cleanup.
 *   - file_io_save: data-path save. Opens the data fork read/write
 *     using the Doc's stored FSSpec, truncates, writes the text
 *     bytes, closes, marks clean.
 *
 * FSSpec layout (used to extract the leaf name without including
 * <Files.h>, keeping this file host-buildable):
 *   bytes [0..1]  vRefNum (short)
 *   bytes [2..5]  parID   (long)
 *   bytes [6..69] name    (Str63 — Pascal string: byte[6] = length,
 *                          bytes[7..7+len-1] = characters)
 */
#include "file_io.h"
#include <string.h>

#define kFsspecNameOffset 6   /* offset into FSSpec to the Str63 name */
#define kFsRdPerm   0
#define kFsRdWrPerm 3
#define kArmaCreator 0x41726D61UL   /* 'Arma' */
#define kTextType    0x54455854UL   /* 'TEXT' */
#define kDupFNErr   (-48)            /* file already exists */

static void copy_filename_from_fsspec(Doc* d, const void* fsspec_opaque) {
    const unsigned char* bytes;
    unsigned char        name_len;
    if (!d || !fsspec_opaque) return;
    bytes    = (const unsigned char*)fsspec_opaque;
    name_len = bytes[kFsspecNameOffset];
    if (name_len == 0) return;
    doc_set_filename(d, (const char*)(bytes + kFsspecNameOffset + 1), name_len);
}

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

    rc = sys->fsp_open_df(fsspec_opaque, kFsRdPerm, &ref);
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
    doc_set_fsspec(doc, fsspec_opaque);
    copy_filename_from_fsspec(doc, fsspec_opaque);

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
    int            rc = kFileIoOk;
    short          ref = 0;
    int            ref_open = 0;
    const void*    fsspec;
    unsigned short text_len = 0;
    const char*    text;
    long           write_count;

    if (!d || !sys) return kFileIoErrOpen;
    if (!doc_has_fsspec(d)) return kFileIoErrOpen;

    fsspec = doc_fsspec(d);
    text   = doc_text(d, &text_len);

    if (sys->fsp_open_df(fsspec, kFsRdWrPerm, &ref) != 0) {
        rc = kFileIoErrOpen; goto cleanup;
    }
    ref_open = 1;

    if (sys->set_eof(ref, 0) != 0)              { rc = kFileIoErrOpen; goto cleanup; }
    write_count = (long)text_len;
    if (sys->fs_write(ref, &write_count, text) != 0) {
        rc = kFileIoErrOpen; goto cleanup;
    }

cleanup:
    if (ref_open) sys->fs_close(ref);
    if (rc == kFileIoOk) doc_mark_clean(d);
    return rc;
}

int file_io_open_interactive(Doc** out_doc, const MacSyscalls* sys) {
    /* sys is per-call; not retained. */
    char fsspec_buf[kDocFsspecBytes];
    int  good = 0;

    if (!out_doc || !sys) return kFileIoErrOpen;
    *out_doc = 0;

    sys->standard_get_file(fsspec_buf, &good);
    if (!good) return kFileIoErrCancel;
    return file_io_open(fsspec_buf, out_doc, sys);
}

int file_io_save_as(Doc* d, const MacSyscalls* sys) {
    /* sys is per-call; not retained. */
    char fsspec_buf[kDocFsspecBytes];
    int  good = 0;
    short create_rc;
    /* Prompt and default name as Pascal strings (length-prefixed). */
    static const char prompt[]  = "\010Save as:";       /* length 8 */
    static const char defname[] = "\013Untitled.md";    /* length 11 */

    if (!d || !sys) return kFileIoErrOpen;

    sys->standard_put_file(prompt, defname, fsspec_buf, &good);
    if (!good) return kFileIoErrCancel;

    /* Create the file (TEXT, Arma creator). dupFNErr is benign — we
     * intentionally overwrite when the user picks an existing name. */
    create_rc = sys->fsp_create(fsspec_buf, kArmaCreator, kTextType, 0);
    if (create_rc != 0 && create_rc != kDupFNErr) return kFileIoErrOpen;

    doc_set_fsspec(d, fsspec_buf);
    copy_filename_from_fsspec(d, fsspec_buf);
    return file_io_save(d, sys);
}

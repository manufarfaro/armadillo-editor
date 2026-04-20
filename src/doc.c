/*
 * src/doc.c — dumb document data container.
 *
 * Opaque Doc with text buffer + filename bytes + dirty flag. No OS
 * dependencies — fully host-buildable. Internal invariants:
 *   - text_buf is always non-NULL after doc_new (points at a malloc'd
 *     zero-sized region initially so doc_text never hands out NULL).
 *   - text_len <= kMaxDocBytes always.
 *   - filename_len <= sizeof(filename).
 *   - Dirty flag tracks unsaved changes; set by doc_set_text,
 *     cleared by doc_mark_clean.
 */
#include "doc.h"
#include <stdlib.h>
#include <string.h>

struct Doc {
    char*          text_buf;
    unsigned short text_len;
    int            dirty;
    char           filename[64];
    unsigned char  filename_len;
};

Doc* doc_new(void) {
    Doc* d = (Doc*)calloc(1, sizeof(Doc));
    if (!d) return 0;
    /* Pre-allocate a 1-byte buffer so doc_text never returns NULL. */
    d->text_buf = (char*)calloc(1, 1);
    if (!d->text_buf) { free(d); return 0; }
    return d;
}

void doc_free(Doc* d) {
    if (!d) return;
    if (d->text_buf) free(d->text_buf);
    free(d);
}

const char* doc_text(const Doc* d, unsigned short* out_len) {
    if (!d) {
        if (out_len) *out_len = 0;
        return "";
    }
    if (out_len) *out_len = d->text_len;
    return d->text_buf;
}

void doc_set_text(Doc* d, const char* bytes, unsigned short len) {
    char* buf;
    if (!d) return;
    if (len > kMaxDocBytes) return;   /* reject, no mutation */
    buf = (char*)realloc(d->text_buf, (size_t)len + 1);
    if (!buf && len > 0) return;       /* reject, no mutation */
    if (buf) d->text_buf = buf;
    if (len > 0 && bytes) memcpy(d->text_buf, bytes, len);
    d->text_buf[len] = '\0';           /* convenience; len is authoritative */
    d->text_len = len;
    d->dirty = 1;
}

int  doc_is_dirty(const Doc* d)  { return d ? d->dirty : 0; }
void doc_mark_dirty(Doc* d)      { if (d) d->dirty = 1; }
void doc_mark_clean(Doc* d)      { if (d) d->dirty = 0; }

void doc_set_filename(Doc* d, const char* bytes, unsigned char len) {
    if (!d) return;
    if (len > sizeof(d->filename)) len = (unsigned char)sizeof(d->filename);
    if (len > 0 && bytes) memcpy(d->filename, bytes, len);
    d->filename_len = len;
}

int doc_has_filename(const Doc* d) {
    return d ? (d->filename_len > 0) : 0;
}

const char* doc_filename(const Doc* d, unsigned char* out_len) {
    if (!d) {
        if (out_len) *out_len = 0;
        return "";
    }
    if (out_len) *out_len = d->filename_len;
    return d->filename;
}

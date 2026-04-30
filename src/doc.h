/*
 * src/doc.h — dumb document data container.
 *
 * Text buffer + filename + dirty flag. No OS dependencies — fully
 * host-buildable. Stateless beyond its fields; host tests own its full
 * surface.
 */
#ifndef ARMA_DOC_H
#define ARMA_DOC_H

#ifndef kMaxDocBytes
#define kMaxDocBytes 32767u
#endif

typedef struct Doc Doc;    /* opaque */

Doc* doc_new(void);
void doc_free(Doc* d);

const char* doc_text(const Doc* d, unsigned short* out_len);

/* Copies up to kMaxDocBytes bytes into the Doc. Over-limit calls
 * leave the Doc's state unchanged. Marks dirty on success. */
void doc_set_text(Doc* d, const char* bytes, unsigned short len);

int  doc_is_dirty(const Doc* d);
void doc_mark_dirty(Doc* d);
void doc_mark_clean(Doc* d);

/* Filename storage is opaque bytes — no FSSpec in the header.
 * The file_io module converts between FSSpec and this byte form. */
void doc_set_filename(Doc* d, const char* bytes, unsigned char len);
int  doc_has_filename(const Doc* d);
const char* doc_filename(const Doc* d, unsigned char* out_len);

#endif /* ARMA_DOC_H */

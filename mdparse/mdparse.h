/*
 * mdparse/mdparse.h — project-owned adapter over md4c.
 *
 * Hides md4c types entirely. Exposes a fan-out sink interface that
 * scanner and render plug into for a single-pass parse. See design.md
 * §2 (module boundaries) and the mdparse capability spec.
 */
#ifndef ARMA_MDPARSE_H
#define ARMA_MDPARSE_H

#include <stddef.h>
#include "render/blocks.h"
#include "render/inlines.h"

typedef struct BlockAttrs {
    unsigned char h_level;       /* 1..6 for kBlockHeading, 0 otherwise */
    unsigned char list_depth;    /* 0 = not in list; 1+ = nesting        */
    unsigned char quote_depth;   /* 0 = not in quote; 1+ = nesting       */
    unsigned char list_ordered;  /* 0 = bullet, 1 = numbered             */
} BlockAttrs;

/* Sink callback return convention: 0 = continue parsing, non-zero =
 * abort. When any sink returns non-zero, mdparse_run stops dispatching
 * further events and returns kMdParseErrSinkAbort to the caller. */
typedef struct MdParseSink {
    int (*on_block_open)(void* ctx, BlockKind kind,
                         const BlockAttrs* attrs);
    int (*on_block_close)(void* ctx, BlockKind kind);
    int (*on_span)(void* ctx, StyleKind kind,
                   unsigned short start, unsigned short length,
                   const char* link_url, unsigned short link_url_len);
    int (*on_text)(void* ctx, const char* bytes, unsigned short length,
                   unsigned short source_offset);
    void* ctx;
} MdParseSink;

typedef enum {
    kMdParseOk           =  0,
    kMdParseErrArenaOOM  = -1,
    kMdParseErrMd4c      = -2,
    kMdParseErrSinkAbort = -3
} MdParseError;

/* Parse source[0..source_len] and dispatch every event to every sink
 * in sinks[0..sink_count-1] in array order. Returns kMdParseOk on
 * success or a negative MdParseError on failure. */
int mdparse_run(const char* source, unsigned short source_len,
                const MdParseSink* sinks, size_t sink_count);

#endif /* ARMA_MDPARSE_H */

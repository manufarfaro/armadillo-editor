/*
 * render/blocks.h — flat block model types.
 *
 * A RenderModel is a flat array of Block structs. Nesting is expressed
 * via list_depth and quote_depth scalars on each block — NOT via a
 * tree. See design.md §1 for rationale.
 */
#ifndef ARMA_BLOCKS_H
#define ARMA_BLOCKS_H

#include "inlines.h"

typedef enum {
    kBlockParagraph  = 0,
    kBlockHeading    = 1,    /* h_level = 1..6                          */
    kBlockListItem   = 2,
    kBlockBlockQuote = 3,
    kBlockCodeBlock  = 4,
    kBlockHr         = 5,
    kBlockHtml       = 6     /* md4c-detected raw HTML block            */
} BlockKind;

typedef struct Block {
    BlockKind       kind;
    unsigned char   h_level;       /* 1..6 for kBlockHeading, else 0   */
    unsigned char   list_depth;    /* 0 = not in list; 1+ = nesting    */
    unsigned char   quote_depth;   /* 0 = not in quote; 1+ = nesting   */
    unsigned char   list_ordered;  /* 0 = bullet, 1 = numbered         */
    const char*     text;          /* arena-alloc'd; NOT NUL-terminated */
    unsigned short  text_length;
    unsigned short  run_count;
    const StyleRun* runs;          /* arena-alloc'd; NULL if 0         */
} Block;

#endif /* ARMA_BLOCKS_H */

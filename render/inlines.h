/*
 * render/inlines.h — inline style runs inside a block's text.
 *
 * A StyleRun is a slice of a block's text carrying a single visual
 * style. Scanner produces StyleRun arrays for the source pane; render
 * produces them inside each Block for the read pane. The two uses
 * share the type but live in different arenas.
 */
#ifndef ARMA_INLINES_H
#define ARMA_INLINES_H

typedef enum {
    kStylePlain    = 0,
    kStyleEmph     = 1,    /* _italic_                                  */
    kStyleStrong   = 2,    /* **bold**                                  */
    kStyleCodeSpan = 3,    /* `code`                                    */
    kStyleLink     = 4,    /* [text](url)                               */
    kStyleHtmlSpan = 5     /* inline <tag>; MVP: raw                    */
} StyleKind;

typedef struct StyleRun {
    unsigned short start;       /* byte offset into containing text       */
    unsigned short length;      /* bytes                                  */
    StyleKind      kind;
    short          link_index;  /* index into per-model link table; -1 = N/A */
} StyleRun;

#endif /* ARMA_INLINES_H */

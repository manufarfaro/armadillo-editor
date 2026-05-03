/*
 * scanner/scanner.h — style-run producer for the source pane.
 *
 * Scanner consumes MdParseSink events and accumulates a flat
 * MdStyleRun[] for application to the source pane via src_pane_apply_
 * runs. Arena-backed; no direct allocation.
 */
#ifndef ARMA_SCANNER_H
#define ARMA_SCANNER_H

#include <stddef.h>
#include "render/arena.h"
#include "render/inlines.h"
#include "mdparse/mdparse.h"

typedef struct Scanner Scanner;    /* opaque */

/* Construct a scanner that allocates runs out of the given arena. */
Scanner* scanner_new(Arena* a);

/* Release the Scanner struct itself. Does NOT destroy the arena. */
void scanner_free(Scanner* s);

/* Return the MdParseSink to plug into mdparse_run. Pointer is valid
 * for the scanner's lifetime. */
const MdParseSink* scanner_sink(Scanner* s);

/* After mdparse_run returns, retrieve the accumulated runs. Pointer
 * valid until the next scanner_reset or arena_reset. */
const MdStyleRun* scanner_runs(const Scanner* s, size_t* out_count);

/* Clear accumulated runs so the scanner is ready for the next parse
 * cycle. Does NOT free arena memory — arena_reset is the caller's job. */
void scanner_reset(Scanner* s);

#endif /* ARMA_SCANNER_H */

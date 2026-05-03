/*
 * render/arena.h — Handle-backed bump allocator for the render pipeline.
 *
 * All variable-length data in the render pipeline (Blocks, MdStyleRun
 * arrays, text slices, link URL strings) lives in one arena. The arena
 * is backed by a single Mac OS Handle that we HLock for the arena's
 * entire lifetime. Growth is "grow-before-parse": callers must
 * arena_ensure() to a pre-estimated size before starting to allocate,
 * and arena_alloc() never grows — it returns NULL on overflow.
 *
 * See design.md §4 for full rationale.
 */
#ifndef ARMA_ARENA_H
#define ARMA_ARENA_H

#include <stddef.h>
#include "src/mac_syscalls.h"

typedef struct Arena Arena;    /* opaque */

/* Initialize an arena backed by a Handle of initial_size bytes.
 * Returns 0 on success, negative on allocation failure.
 * On success *out is non-NULL and the Handle is HLocked.
 * On failure *out is NULL and no resources leak. */
int arena_init(Arena** out, size_t initial_size, const MacSyscalls* sys);

/* Release all resources. arena_destroy(NULL) is a no-op. */
void arena_destroy(Arena* a);

/* Grow the backing Handle so at least `bytes_needed` bytes are
 * available above the current high_water. Call BEFORE any allocation
 * cycle to pre-size. Doubling-then-linear growth per design.md §4;
 * capped at kArenaHardCap.
 * Returns 0 on success, negative if grow failed (state unchanged). */
int arena_ensure(Arena* a, size_t bytes_needed);

/* Bump-allocate n_bytes; rounded up to 4-byte alignment.
 * Returns a pointer into the arena on success, NULL on overflow.
 * Never grows — caller must arena_ensure first. */
void* arena_alloc(Arena* a, size_t n_bytes);

/* Reset high_water to 0. Keeps the backing Handle intact so the next
 * parse cycle reuses the memory. */
void arena_reset(Arena* a);

/* Diagnostics — do not drive decisions off these except in tests. */
size_t arena_high_water(const Arena* a);
size_t arena_capacity(const Arena* a);

/* Peak high_water since init. Survives arena_reset — useful for tuning
 * kArenaInitialSize against the real workload. */
size_t arena_max_ever(const Arena* a);

#endif /* ARMA_ARENA_H */

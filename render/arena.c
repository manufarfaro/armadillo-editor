/*
 * render/arena.c — Handle-backed bump allocator.
 *
 * See design.md §4 for full policy rationale.
 */
#include "arena.h"
#include <stdlib.h>
#include <string.h>

#define kArenaDoublingCap   (64u * 1024u)
#define kArenaLinearStep    (32u * 1024u)
#define kArenaHardCap      (512u * 1024u)

struct Arena {
    void*       backing;         /* Handle (void**) */
    char*       base;            /* = *backing while HLocked */
    size_t      size;
    size_t      high_water;
    size_t      max_ever;
    MacSyscalls sys;             /* by-value copy taken at arena_init */
};

int arena_init(Arena** out, size_t initial_size, const MacSyscalls* sys) {
    Arena* a;
    if (!out || !sys) return -1;
    *out = 0;

    a = (Arena*)calloc(1, sizeof(Arena));
    if (!a) return -1;

    a->backing = sys->new_handle((long)initial_size);
    if (!a->backing) { free(a); return -1; }
    sys->hlock(a->backing);
    a->base       = *(char**)a->backing;
    a->size       = initial_size;
    a->high_water = 0;
    a->max_ever   = 0;
    /* Copy the syscall vtable into the Arena by value. After this point
     * the Arena owns its own snapshot and does not depend on `sys`'s
     * storage lifetime. The 80-byte cost (20 function pointers x 4 B
     * on 68k) is negligible against the 4 MB RAM budget and eliminates
     * the dangling-pointer bug class CodeQL flagged here. */
    a->sys = *sys;

    *out = a;
    return 0;
}

void arena_destroy(Arena* a) {
    if (!a) return;
    if (a->backing) {
        a->sys.hunlock(a->backing);
        a->sys.dispose_handle(a->backing);
    }
    free(a);
}

size_t arena_capacity(const Arena* a)   { return a ? a->size : 0; }
size_t arena_high_water(const Arena* a) { return a ? a->high_water : 0; }
size_t arena_max_ever(const Arena* a)   { return a ? a->max_ever : 0; }

void* arena_alloc(Arena* a, size_t n_bytes) {
    void* p;
    if (!a) return 0;
    /* Round up to 4-byte alignment. 68020+ requires 4-byte alignment
     * for longword access; we round every allocation up. */
    n_bytes = (n_bytes + 3u) & ~(size_t)3u;
    if (a->high_water + n_bytes > a->size) return 0;
    p = a->base + a->high_water;
    a->high_water += n_bytes;
    if (a->high_water > a->max_ever) a->max_ever = a->high_water;
    return p;
}

int arena_ensure(Arena* a, size_t bytes_needed) {
    size_t needed_total;
    size_t next;
    int rc;

    if (!a) return -1;
    needed_total = a->high_water + bytes_needed;
    if (needed_total <= a->size) return 0;

    /* Compute next size using the design's growth policy. */
    next = a->size;
    while (next < needed_total) {
        if (next < kArenaDoublingCap) {
            next = next < 1 ? 1 : next * 2;
        } else {
            next += kArenaLinearStep;
        }
        if (next > kArenaHardCap) return -1;
    }

    /* SetHandleSize may relocate the block. HUnlock first, resize,
     * rebind base after relocking. If resize fails, relock at old
     * size and return failure. */
    a->sys.hunlock(a->backing);
    rc = a->sys.set_handle_size(a->backing, (long)next);
    a->sys.hlock(a->backing);
    a->base = *(char**)a->backing;
    if (rc != 0) return -1;

    a->size = next;
    return 0;
}

void  arena_reset(Arena* a)           { if (a) a->high_water = 0; }

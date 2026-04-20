/*
 * render/arena.c — Handle-backed bump allocator.
 *
 * See design.md §4 for full policy rationale.
 */
#include "arena.h"
#include <stdlib.h>
#include <string.h>

struct Arena {
    void*              backing;         /* Handle (void**) */
    char*              base;            /* = *backing while HLocked */
    size_t             size;
    size_t             high_water;
    size_t             max_ever;
    const MacSyscalls* sys;
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
    a->sys        = sys;

    *out = a;
    return 0;
}

void arena_destroy(Arena* a) {
    if (!a) return;
    if (a->backing) {
        a->sys->hunlock(a->backing);
        a->sys->dispose_handle(a->backing);
    }
    free(a);
}

size_t arena_capacity(const Arena* a)   { return a ? a->size : 0; }
size_t arena_high_water(const Arena* a) { return a ? a->high_water : 0; }

/* Stubs for the rest of the API — fleshed out in later tasks. */
int  arena_ensure(Arena* a, size_t n) { (void)a; (void)n; return -1; }
void* arena_alloc(Arena* a, size_t n) { (void)a; (void)n; return 0; }
void  arena_reset(Arena* a)           { if (a) a->high_water = 0; }

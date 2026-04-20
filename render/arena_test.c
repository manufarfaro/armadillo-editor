/*
 * render/arena_test.c — host unit tests for the arena allocator.
 *
 * Every test covers one behavior described in openspec/specs/render/
 * spec.md's "Arena allocator" requirement. Unity drives the asserts.
 * FakeSyscalls provides a malloc-backed Handle.
 *
 * What's a Handle?
 *   On classic Mac, a Handle is a pointer to a master pointer (a
 *   double indirection). The Memory Manager can move the block of
 *   memory the master pointer points to in order to compact the heap.
 *   To keep *handle stable across calls, HLock the handle. Our arena
 *   HLocks once at init and keeps the lock for the arena's lifetime,
 *   so *backing is always a stable pointer into memory we own.
 */
#include "unity.h"
#include "test/fake_syscalls.h"
#include "render/arena.h"

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    /* Tests added in subsequent tasks. */
    return UNITY_END();
}

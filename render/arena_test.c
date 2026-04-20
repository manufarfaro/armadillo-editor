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

/* arena_init: allocation + HLock
 * ───────────────────────────────
 * arena_init SHALL allocate a Handle of the requested size via
 * MacSyscalls.new_handle and HLock it. Post-conditions: the arena
 * returned is non-NULL; new_handle was called exactly once; hlock
 * was called exactly once; the arena reports the initial capacity. */
void test_arena_init_allocates_and_hlocks_backing_handle(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    int rc = arena_init(&a, 4096, (const MacSyscalls*)&f);

    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_EQUAL_INT(1, f.new_handle_calls);
    TEST_ASSERT_EQUAL_INT(1, f.hlock_calls);
    TEST_ASSERT_EQUAL_INT(4096, arena_capacity(a));
    TEST_ASSERT_EQUAL_INT(0, arena_high_water(a));

    arena_destroy(a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_arena_init_allocates_and_hlocks_backing_handle);
    return UNITY_END();
}

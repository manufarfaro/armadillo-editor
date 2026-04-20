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

/* arena_alloc: alignment and bump semantics
 * ──────────────────────────────────────────
 * arena_alloc SHALL return pointers aligned to 4 bytes and bump
 * high_water by the requested size rounded up to 4. A 5-byte request
 * consumes 8 bytes of arena space. */
void test_arena_alloc_returns_4byte_aligned_pointer(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    void* p; void* q;
    arena_init(&a, 4096, (const MacSyscalls*)&f);

    p = arena_alloc(a, 5);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_INT(0, (size_t)p & 3u);
    TEST_ASSERT_EQUAL_INT(8, arena_high_water(a));

    q = arena_alloc(a, 1);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_EQUAL_INT(0, (size_t)q & 3u);
    TEST_ASSERT_EQUAL_INT(12, arena_high_water(a));

    arena_destroy(a);
}

void test_arena_alloc_beyond_capacity_returns_null(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    void* p;
    arena_init(&a, 128, (const MacSyscalls*)&f);

    (void)arena_alloc(a, 120);   /* bumps high_water to 120 */
    p = arena_alloc(a, 16);       /* would bump to 136 > 128 */
    TEST_ASSERT_NULL(p);
    TEST_ASSERT_EQUAL_INT(120, arena_high_water(a));

    arena_destroy(a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_arena_init_allocates_and_hlocks_backing_handle);
    RUN_TEST(test_arena_alloc_returns_4byte_aligned_pointer);
    RUN_TEST(test_arena_alloc_beyond_capacity_returns_null);
    return UNITY_END();
}

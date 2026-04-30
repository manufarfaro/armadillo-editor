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

/* Clear the FakeSyscalls singleton between tests. Without this, a
 * test that forgets to call fake_syscalls_activate inherits the
 * previous test's (now-stack-gone) pointer and every fake callback
 * reads freed memory. With this, forgetting activate produces a
 * visible failure (all callbacks see NULL and return 0 / fail) rather
 * than silent UB. */
void tearDown(void) { fake_syscalls_activate(0); }

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

/* arena_ensure: pre-parse growth
 * ───────────────────────────────
 * arena_ensure SHALL grow the backing Handle so at least bytes_needed
 * bytes are available above high_water. Doubling up to 64KB, then
 * +32KB linear, capped at 512KB. */
void test_arena_ensure_doubles_when_under_cap(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    int rc;
    arena_init(&a, 4096, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(4096, arena_capacity(a));

    rc = arena_ensure(a, 6000);   /* needs >= 6000 available */
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(arena_capacity(a) >= 6000);
    /* After doubling from 4096 we expect at least 8192. */
    TEST_ASSERT_EQUAL_INT(8192, arena_capacity(a));

    arena_destroy(a);
}

void test_arena_ensure_grow_failure_preserves_state(void) {
    FakeSyscalls f = fake_syscalls_init();
    f.set_handle_size_fail_after = 0;  /* the FIRST SetHandleSize fails */
    fake_syscalls_activate(&f);

    Arena* a = 0;
    int rc;
    arena_init(&a, 128, (const MacSyscalls*)&f);
    (void)arena_alloc(a, 64);

    rc = arena_ensure(a, 10000);
    TEST_ASSERT_NOT_EQUAL(0, rc);
    TEST_ASSERT_EQUAL_INT(128, arena_capacity(a));
    TEST_ASSERT_EQUAL_INT(64, arena_high_water(a));

    arena_destroy(a);
}

void test_arena_ensure_no_op_when_already_sized(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    int rc;
    arena_init(&a, 4096, (const MacSyscalls*)&f);

    rc = arena_ensure(a, 1024);   /* already have 4096 > 1024 */
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(4096, arena_capacity(a));
    TEST_ASSERT_EQUAL_INT(0, f.set_handle_size_calls);

    arena_destroy(a);
}

/* arena_reset: watermark back to 0, backing preserved
 * ─────────────────────────────────────────────────────
 * arena_reset SHALL clear high_water so the next parse cycle starts
 * fresh. The backing Handle MUST NOT be disposed — its memory is
 * reused, saving a NewHandle per parse. */
void test_arena_reset_clears_watermark_but_preserves_capacity(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    void* p;
    arena_init(&a, 4096, (const MacSyscalls*)&f);
    (void)arena_alloc(a, 1000);
    TEST_ASSERT_EQUAL_INT(1000, arena_high_water(a));
    TEST_ASSERT_EQUAL_INT(4096, arena_capacity(a));

    arena_reset(a);
    TEST_ASSERT_EQUAL_INT(0, arena_high_water(a));
    TEST_ASSERT_EQUAL_INT(4096, arena_capacity(a));
    TEST_ASSERT_EQUAL_INT(0, f.dispose_handle_calls);

    /* Next alloc reuses the same memory region. */
    p = arena_alloc(a, 500);
    TEST_ASSERT_NOT_NULL(p);

    arena_destroy(a);
}

/* arena_destroy: single DisposeHandle call
 * ──────────────────────────────────────────
 * arena_destroy disposes the backing Handle exactly once. No-op on
 * NULL. Calling it is the only path that ever disposes — destroy is
 * terminal. */
void test_arena_destroy_disposes_backing_exactly_once(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    arena_init(&a, 2048, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(0, f.dispose_handle_calls);

    arena_destroy(a);
    TEST_ASSERT_EQUAL_INT(1, f.dispose_handle_calls);

    /* NULL is a no-op */
    arena_destroy(0);
    TEST_ASSERT_EQUAL_INT(1, f.dispose_handle_calls);
}

/* Reset + alloc cycle: reuses memory
 * ─────────────────────────────────────
 * Simulates the parse pipeline's inner loop: many reset+alloc cycles
 * should NOT trigger additional NewHandle calls. Only the initial
 * init and any grow events should. */
void test_arena_reset_alloc_cycle_does_not_allocate_new_handles(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    int initial_new_handles;
    int i;
    arena_init(&a, 4096, (const MacSyscalls*)&f);
    initial_new_handles = f.new_handle_calls;  /* should be 1 */
    TEST_ASSERT_EQUAL_INT(1, initial_new_handles);

    for (i = 0; i < 10; i++) {
        arena_reset(a);
        (void)arena_alloc(a, 1000);
        (void)arena_alloc(a, 500);
    }

    TEST_ASSERT_EQUAL_INT(1, f.new_handle_calls);     /* still 1 */
    TEST_ASSERT_EQUAL_INT(0, f.set_handle_size_calls); /* never grew */

    arena_destroy(a);
}

/* arena_max_ever: peak high_water since init
 * ────────────────────────────────────────────
 * arena_max_ever SHALL return the highest value high_water has ever
 * reached since arena_init, even across arena_reset calls. Useful for
 * tuning kArenaInitialSize against the real workload. */
void test_arena_max_ever_tracks_peak_across_resets(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    arena_init(&a, 4096, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(0, arena_max_ever(a));

    (void)arena_alloc(a, 1000);
    TEST_ASSERT_EQUAL_INT(1000, arena_max_ever(a));

    /* Reset clears high_water but NOT max_ever. */
    arena_reset(a);
    TEST_ASSERT_EQUAL_INT(0, arena_high_water(a));
    TEST_ASSERT_EQUAL_INT(1000, arena_max_ever(a));

    /* A smaller allocation in the second cycle doesn't move the peak. */
    (void)arena_alloc(a, 500);
    TEST_ASSERT_EQUAL_INT(1000, arena_max_ever(a));

    /* A larger allocation does. */
    (void)arena_alloc(a, 2000);
    TEST_ASSERT_EQUAL_INT(2500, arena_max_ever(a));

    arena_destroy(a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_arena_init_allocates_and_hlocks_backing_handle);
    RUN_TEST(test_arena_alloc_returns_4byte_aligned_pointer);
    RUN_TEST(test_arena_alloc_beyond_capacity_returns_null);
    RUN_TEST(test_arena_ensure_doubles_when_under_cap);
    RUN_TEST(test_arena_ensure_grow_failure_preserves_state);
    RUN_TEST(test_arena_ensure_no_op_when_already_sized);
    RUN_TEST(test_arena_reset_clears_watermark_but_preserves_capacity);
    RUN_TEST(test_arena_destroy_disposes_backing_exactly_once);
    RUN_TEST(test_arena_reset_alloc_cycle_does_not_allocate_new_handles);
    RUN_TEST(test_arena_max_ever_tracks_peak_across_resets);
    return UNITY_END();
}

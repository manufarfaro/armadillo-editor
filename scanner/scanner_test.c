/*
 * scanner/scanner_test.c — host unit tests for scanner.
 *
 * Feeds synthetic MdParseSink events (NOT real md4c output) into the
 * scanner's sink and asserts on the accumulated StyleRun[]. Bypassing
 * md4c keeps tests fast and isolates scanner behavior from any md4c
 * semantic quirks.
 */
#include "unity.h"
#include "test/fake_syscalls.h"
#include "render/arena.h"
#include "scanner.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) { fake_syscalls_activate(0); }

void test_scanner_empty_event_stream_produces_zero_runs(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    {
        Arena* a = 0;
        Scanner* s;
        size_t count = 99;
        const StyleRun* runs;
        arena_init(&a, 4096, (const MacSyscalls*)&f);

        s = scanner_new(a);
        TEST_ASSERT_NOT_NULL(s);

        runs = scanner_runs(s, &count);
        (void)runs;
        TEST_ASSERT_EQUAL_INT(0, count);

        scanner_free(s);
        arena_destroy(a);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_scanner_empty_event_stream_produces_zero_runs);
    return UNITY_END();
}

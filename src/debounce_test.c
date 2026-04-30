/*
 * src/debounce_test.c — host unit tests for parse debounce.
 *
 * Uses FakeSyscalls's tick_count_now to drive the clock deterministically.
 * No real TickCount involved; debounce never calls TickCount directly.
 */
#include "unity.h"
#include "test/fake_syscalls.h"
#include "debounce.h"

void setUp(void) {}
void tearDown(void) { fake_syscalls_activate(0); }

void test_debounce_fresh_state_poll_returns_no_parse(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    {
        DebounceState s = {0, 0};
        int fire = debounce_poll(&s, (const MacSyscalls*)&f);
        TEST_ASSERT_EQUAL_INT(0, fire);
    }
}

void test_debounce_edit_then_short_idle_does_not_fire(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    {
        DebounceState s = {0, 0};
        int fire;
        f.tick_count_now = 100;
        debounce_on_edit(&s, (const MacSyscalls*)&f);
        f.tick_count_now = 110;  /* 10 ticks elapsed; less than kParseDebounceTicks (15) */

        fire = debounce_poll(&s, (const MacSyscalls*)&f);
        TEST_ASSERT_EQUAL_INT(0, fire);
    }
}

void test_debounce_fires_after_idle(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    {
        DebounceState s = {0, 0};
        int fire;
        f.tick_count_now = 100;
        debounce_on_edit(&s, (const MacSyscalls*)&f);
        f.tick_count_now = 120;   /* 20 ticks elapsed >= 15; should fire */

        fire = debounce_poll(&s, (const MacSyscalls*)&f);
        TEST_ASSERT_EQUAL_INT(1, fire);
        TEST_ASSERT_EQUAL_INT(0, s.dirty);  /* cleared */

        /* Subsequent poll: dirty cleared, no fire. */
        f.tick_count_now = 130;
        fire = debounce_poll(&s, (const MacSyscalls*)&f);
        TEST_ASSERT_EQUAL_INT(0, fire);
    }
}

void test_debounce_new_edit_resets_window(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    {
        DebounceState s = {0, 0};
        int fire;
        f.tick_count_now = 100;
        debounce_on_edit(&s, (const MacSyscalls*)&f);
        f.tick_count_now = 112;   /* still within debounce window */
        debounce_on_edit(&s, (const MacSyscalls*)&f);   /* reset */
        f.tick_count_now = 120;   /* 120 - 112 = 8 ticks, still within window */

        fire = debounce_poll(&s, (const MacSyscalls*)&f);
        TEST_ASSERT_EQUAL_INT(0, fire);

        f.tick_count_now = 128;   /* 128 - 112 = 16 ticks, fires */
        fire = debounce_poll(&s, (const MacSyscalls*)&f);
        TEST_ASSERT_EQUAL_INT(1, fire);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_debounce_fresh_state_poll_returns_no_parse);
    RUN_TEST(test_debounce_edit_then_short_idle_does_not_fire);
    RUN_TEST(test_debounce_fires_after_idle);
    RUN_TEST(test_debounce_new_edit_resets_window);
    return UNITY_END();
}

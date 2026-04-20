/*
 * src/file_io_test.c — host tests for file-io data-path.
 *
 * Tests drive the file_io_open and file_io_save code paths via
 * FakeSyscalls. Interactive dialog functions (file_io_open_interactive,
 * file_io_save_as) are stubs today and not tested here; they land in
 * Plan 2 alongside the real Standard File wiring.
 */
#include "unity.h"
#include "test/fake_syscalls.h"
#include "file_io.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) { fake_syscalls_activate(0); }

void test_file_io_open_rejects_over_limit_file(void) {
    FakeSyscalls f = fake_syscalls_init();
    f.get_eof_size = 50000;   /* > kMaxDocBytes (32767) */
    fake_syscalls_activate(&f);

    {
        Doc* d = 0;
        int rc = file_io_open((void*)"spec", &d, (const MacSyscalls*)&f);
        TEST_ASSERT_EQUAL_INT(kFileIoErrTooBig, rc);
        TEST_ASSERT_NULL(d);
        /* Cleanup: fs_close called exactly once; no fs_read attempted. */
        TEST_ASSERT_EQUAL_INT(1, f.fs_close_calls);
        TEST_ASSERT_EQUAL_INT(0, f.fs_read_calls);
    }
}

void test_file_io_open_open_failure_returns_err_open(void) {
    FakeSyscalls f = fake_syscalls_init();
    f.fsp_open_df_result = -43;   /* fnfErr */
    fake_syscalls_activate(&f);

    {
        Doc* d = 0;
        int rc = file_io_open((void*)"spec", &d, (const MacSyscalls*)&f);
        TEST_ASSERT_EQUAL_INT(kFileIoErrOpen, rc);
        TEST_ASSERT_NULL(d);
    }
}

void test_file_io_open_read_failure_cleans_up(void) {
    FakeSyscalls f = fake_syscalls_init();
    f.get_eof_size = 10;
    f.fs_read_result = -36;   /* ioErr */
    fake_syscalls_activate(&f);

    {
        Doc* d = 0;
        int rc = file_io_open((void*)"spec", &d, (const MacSyscalls*)&f);
        TEST_ASSERT_EQUAL_INT(kFileIoErrRead, rc);
        TEST_ASSERT_NULL(d);
        /* Handle disposed, file closed on error path. */
        TEST_ASSERT_EQUAL_INT(1, f.dispose_handle_calls);
        TEST_ASSERT_EQUAL_INT(1, f.fs_close_calls);
    }
}

void test_file_io_save_clears_dirty_flag(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    {
        Doc* d = doc_new();
        int rc;
        doc_set_text(d, "x", 1);
        doc_set_filename(d, "a.md", 4);
        TEST_ASSERT_EQUAL_INT(1, doc_is_dirty(d));

        rc = file_io_save(d, (const MacSyscalls*)&f);
        TEST_ASSERT_EQUAL_INT(kFileIoOk, rc);
        TEST_ASSERT_EQUAL_INT(0, doc_is_dirty(d));

        doc_free(d);
    }
}

void test_file_io_save_without_filename_fails(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    {
        Doc* d = doc_new();
        int rc;
        doc_set_text(d, "x", 1);
        /* No filename set. */
        rc = file_io_save(d, (const MacSyscalls*)&f);
        TEST_ASSERT_EQUAL_INT(kFileIoErrOpen, rc);
        doc_free(d);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_file_io_open_rejects_over_limit_file);
    RUN_TEST(test_file_io_open_open_failure_returns_err_open);
    RUN_TEST(test_file_io_open_read_failure_cleans_up);
    RUN_TEST(test_file_io_save_clears_dirty_flag);
    RUN_TEST(test_file_io_save_without_filename_fails);
    return UNITY_END();
}

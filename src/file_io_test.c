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

void test_file_io_save_writes_via_syscalls_and_clears_dirty(void) {
    FakeSyscalls  f = fake_syscalls_init();
    char          fsspec_buf[kDocFsspecBytes];
    Doc*          d = doc_new();
    int           rc;

    fake_syscalls_activate(&f);
    memset(fsspec_buf, 0, sizeof fsspec_buf);
    /* Pascal-string filename "a.md" at offset 6 of the FSSpec. */
    fsspec_buf[6] = 4;
    memcpy(fsspec_buf + 7, "a.md", 4);
    doc_set_fsspec(d, fsspec_buf);
    doc_set_text(d, "x", 1);
    TEST_ASSERT_EQUAL_INT(1, doc_is_dirty(d));

    rc = file_io_save(d, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(kFileIoOk, rc);
    TEST_ASSERT_EQUAL_INT(0, doc_is_dirty(d));
    /* Save sequence verified via syscall counters: open the data fork
     * read/write, truncate to 0, write the text, close. */
    TEST_ASSERT_EQUAL_INT(1, f.fsp_open_df_calls);
    TEST_ASSERT_EQUAL_INT(1, f.set_eof_calls);
    TEST_ASSERT_EQUAL_INT(1, f.fs_write_calls);
    TEST_ASSERT_EQUAL_INT(1, f.fs_close_calls);

    doc_free(d);
}

void test_file_io_save_without_fsspec_fails(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    {
        Doc* d = doc_new();
        int rc;
        doc_set_text(d, "x", 1);
        /* No FSSpec set. */
        rc = file_io_save(d, (const MacSyscalls*)&f);
        TEST_ASSERT_EQUAL_INT(kFileIoErrOpen, rc);
        /* No I/O attempted on the no-FSSpec path. */
        TEST_ASSERT_EQUAL_INT(0, f.fsp_open_df_calls);
        TEST_ASSERT_EQUAL_INT(0, f.fs_write_calls);
        doc_free(d);
    }
}

void test_file_io_open_happy_path(void) {
    /* Happy path: valid FSSpec, valid size under the cap, successful
     * read. Expect kFileIoOk, non-NULL Doc, text length equals
     * get_eof_size. The fake's fs_read doesn't populate bytes (it's a
     * stub that records the call), so we assert on length + cleanup
     * counters rather than content — still enough to catch regressions
     * where the happy path stops returning success or leaks resources.
     *
     * Plan 2b: file_io_open now records the FSSpec on the Doc and
     * extracts the leaf filename from the FSSpec's Pascal-string name
     * field (offset 6). The test FSSpec carries "notes.md" so we can
     * round-trip it through doc_filename. */
    FakeSyscalls f = fake_syscalls_init();
    char         fsspec_buf[kDocFsspecBytes];

    f.get_eof_size = 42;
    fake_syscalls_activate(&f);
    memset(fsspec_buf, 0, sizeof fsspec_buf);
    fsspec_buf[6] = 8;                          /* Pascal-string length */
    memcpy(fsspec_buf + 7, "notes.md", 8);

    {
        Doc* d = 0;
        int rc;
        unsigned short len = 99;
        rc = file_io_open(fsspec_buf, &d, (const MacSyscalls*)&f);
        TEST_ASSERT_EQUAL_INT(kFileIoOk, rc);
        TEST_ASSERT_NOT_NULL(d);
        (void)doc_text(d, &len);
        TEST_ASSERT_EQUAL_INT(42, len);
        /* Cleanup: ref closed; handle allocated, locked, unlocked,
         * disposed (all on the happy path, not on failure). */
        TEST_ASSERT_EQUAL_INT(1, f.fs_close_calls);
        TEST_ASSERT_EQUAL_INT(1, f.new_handle_calls);
        TEST_ASSERT_EQUAL_INT(1, f.hlock_calls);
        TEST_ASSERT_EQUAL_INT(1, f.hunlock_calls);
        TEST_ASSERT_EQUAL_INT(1, f.dispose_handle_calls);
        TEST_ASSERT_EQUAL_INT(1, f.fs_read_calls);
        /* FSSpec stored, filename extracted from the FSSpec name field. */
        TEST_ASSERT_EQUAL_INT(1, doc_has_fsspec(d));
        TEST_ASSERT_EQUAL_INT(1, doc_has_filename(d));
        {
            unsigned char fn_len;
            const char* fn = doc_filename(d, &fn_len);
            TEST_ASSERT_EQUAL_INT(8, fn_len);
            TEST_ASSERT_EQUAL_MEMORY("notes.md", fn, 8);
        }

        doc_free(d);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_file_io_open_rejects_over_limit_file);
    RUN_TEST(test_file_io_open_open_failure_returns_err_open);
    RUN_TEST(test_file_io_open_read_failure_cleans_up);
    RUN_TEST(test_file_io_open_happy_path);
    RUN_TEST(test_file_io_save_writes_via_syscalls_and_clears_dirty);
    RUN_TEST(test_file_io_save_without_fsspec_fails);
    return UNITY_END();
}

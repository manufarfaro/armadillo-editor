/*
 * src/doc_test.c — host unit tests for the document data container.
 *
 * Doc is a dumb data holder. Tests cover new/free, text get/set
 * (including over-limit rejection), dirty flag, filename accessors.
 * No Toolbox dependency.
 */
#include "unity.h"
#include "doc.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_doc_new_returns_non_null_empty_clean_doc(void) {
    Doc* d = doc_new();
    TEST_ASSERT_NOT_NULL(d);

    {
        unsigned short len = 99;
        const char* p = doc_text(d, &len);
        TEST_ASSERT_NOT_NULL(p);   /* buffer exists even if empty */
        TEST_ASSERT_EQUAL_INT(0, len);
        TEST_ASSERT_EQUAL_INT(0, doc_is_dirty(d));
        TEST_ASSERT_EQUAL_INT(0, doc_has_filename(d));
    }

    doc_free(d);
}

void test_doc_free_null_is_noop(void) {
    doc_free(0);   /* should not crash */
    TEST_ASSERT_TRUE(1);
}

void test_doc_set_text_round_trip(void) {
    Doc* d = doc_new();
    doc_set_text(d, "Hello", 5);

    {
        unsigned short len;
        const char* p = doc_text(d, &len);
        TEST_ASSERT_EQUAL_INT(5, len);
        TEST_ASSERT_EQUAL_MEMORY("Hello", p, 5);
        TEST_ASSERT_EQUAL_INT(1, doc_is_dirty(d));
    }

    doc_free(d);
}

void test_doc_set_text_overwrite(void) {
    Doc* d = doc_new();
    doc_set_text(d, "First", 5);
    doc_set_text(d, "Second!", 7);

    {
        unsigned short len;
        const char* p = doc_text(d, &len);
        TEST_ASSERT_EQUAL_INT(7, len);
        TEST_ASSERT_EQUAL_MEMORY("Second!", p, 7);
    }
    doc_free(d);
}

void test_doc_set_text_over_limit_is_rejected(void) {
    Doc* d = doc_new();
    char big[100];
    memset(big, 'x', sizeof big);

    doc_set_text(d, "ok", 2);
    doc_mark_clean(d);

    /* 65535 > kMaxDocBytes (32767). Over-limit call must leave state
     * unchanged — text still "ok", dirty still 0. */
    doc_set_text(d, big, 65535);

    {
        unsigned short len;
        const char* p = doc_text(d, &len);
        TEST_ASSERT_EQUAL_INT(2, len);
        TEST_ASSERT_EQUAL_MEMORY("ok", p, 2);
        TEST_ASSERT_EQUAL_INT(0, doc_is_dirty(d));
    }

    doc_free(d);
}

void test_doc_mark_clean_clears_dirty(void) {
    Doc* d = doc_new();
    doc_set_text(d, "x", 1);
    TEST_ASSERT_EQUAL_INT(1, doc_is_dirty(d));
    doc_mark_clean(d);
    TEST_ASSERT_EQUAL_INT(0, doc_is_dirty(d));
    doc_free(d);
}

void test_doc_filename_round_trip(void) {
    Doc* d = doc_new();
    TEST_ASSERT_EQUAL_INT(0, doc_has_filename(d));
    doc_set_filename(d, "notes.md", 8);
    TEST_ASSERT_EQUAL_INT(1, doc_has_filename(d));

    {
        unsigned char len;
        const char* fn = doc_filename(d, &len);
        TEST_ASSERT_EQUAL_INT(8, len);
        TEST_ASSERT_EQUAL_MEMORY("notes.md", fn, 8);
    }

    doc_free(d);
}

void test_doc_fresh_has_no_fsspec(void) {
    Doc* d = doc_new();
    TEST_ASSERT_EQUAL_INT(0, doc_has_fsspec(d));
    TEST_ASSERT_NULL(doc_fsspec(d));
    doc_free(d);
}

void test_doc_set_fsspec_round_trip(void) {
    Doc* d = doc_new();
    /* Fill a recognizable byte pattern: 0, 1, 2, ..., 69 (70 bytes,
     * matching the actual sizeof(FSSpec) on classic Mac). The remaining
     * kDocFsspecBytes - 70 bytes are slack and may be uninitialized. */
    unsigned char input[kDocFsspecBytes];
    int i;
    for (i = 0; i < kDocFsspecBytes; i++) input[i] = (unsigned char)i;

    doc_set_fsspec(d, input);
    TEST_ASSERT_EQUAL_INT(1, doc_has_fsspec(d));

    {
        const unsigned char* stored = (const unsigned char*)doc_fsspec(d);
        TEST_ASSERT_NOT_NULL(stored);
        /* The first 70 bytes (the FSSpec content) MUST match. */
        TEST_ASSERT_EQUAL_MEMORY(input, stored, 70);
    }

    doc_free(d);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_doc_new_returns_non_null_empty_clean_doc);
    RUN_TEST(test_doc_free_null_is_noop);
    RUN_TEST(test_doc_set_text_round_trip);
    RUN_TEST(test_doc_set_text_overwrite);
    RUN_TEST(test_doc_set_text_over_limit_is_rejected);
    RUN_TEST(test_doc_mark_clean_clears_dirty);
    RUN_TEST(test_doc_filename_round_trip);
    RUN_TEST(test_doc_fresh_has_no_fsspec);
    RUN_TEST(test_doc_set_fsspec_round_trip);
    return UNITY_END();
}

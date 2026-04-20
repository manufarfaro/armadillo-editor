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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_doc_new_returns_non_null_empty_clean_doc);
    RUN_TEST(test_doc_free_null_is_noop);
    RUN_TEST(test_doc_set_text_round_trip);
    RUN_TEST(test_doc_set_text_overwrite);
    RUN_TEST(test_doc_set_text_over_limit_is_rejected);
    return UNITY_END();
}

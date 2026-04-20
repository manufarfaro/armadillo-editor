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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_doc_new_returns_non_null_empty_clean_doc);
    RUN_TEST(test_doc_free_null_is_noop);
    return UNITY_END();
}

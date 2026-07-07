#include "unity/unity.h"
#include "core.h"
#include <stdlib.h>
#include <string.h>

static void test_valid_uppercase_tags(void) {
    const char* url = GetURLFromMessage("[URL]https://example.com[/URL]");
    TEST_ASSERT_NOT_NULL(url);
    TEST_ASSERT_EQUAL_STRING("https://example.com", url);
    free((void*)url);
}

static void test_valid_lowercase_tags(void) {
    const char* url = GetURLFromMessage("[url]https://example.com[/url]");
    TEST_ASSERT_NOT_NULL(url);
    TEST_ASSERT_EQUAL_STRING("https://example.com", url);
    free((void*)url);
}

static void test_text_before_url_returns_null(void) {
    const char* url = GetURLFromMessage("hello [URL]https://example.com[/URL]");
    TEST_ASSERT_NULL(url);
}

static void test_text_after_url_returns_null(void) {
    const char* url = GetURLFromMessage("[URL]https://example.com[/URL] tail");
    TEST_ASSERT_NULL(url);
}

static void test_missing_closing_tag_returns_null(void) {
    const char* url = GetURLFromMessage("[URL]https://example.com");
    TEST_ASSERT_NULL(url);
}

static void test_empty_message_returns_null(void) {
    const char* url = GetURLFromMessage("");
    TEST_ASSERT_NULL(url);
}

static void test_no_url_tags_returns_null(void) {
    const char* url = GetURLFromMessage("https://example.com");
    TEST_ASSERT_NULL(url);
}

static void test_mixed_case_tags_returns_null(void) {
    /* Neither [URL] nor [url] — mixed case is not accepted */
    const char* url = GetURLFromMessage("[Url]https://example.com[/Url]");
    TEST_ASSERT_NULL(url);
}

static void test_url_with_query_string(void) {
    const char* url = GetURLFromMessage("[URL]https://www.youtube.com/watch?v=abc123&t=39s[/URL]");
    TEST_ASSERT_NOT_NULL(url);
    TEST_ASSERT_EQUAL_STRING("https://www.youtube.com/watch?v=abc123&t=39s", url);
    free((void*)url);
}

static void test_empty_url_between_tags(void) {
    const char* url = GetURLFromMessage("[URL][/URL]");
    TEST_ASSERT_NOT_NULL(url);
    TEST_ASSERT_EQUAL_STRING("", url);
    free((void*)url);
}

void RunURLTests(void) {
    RUN_TEST(test_valid_uppercase_tags);
    RUN_TEST(test_valid_lowercase_tags);
    RUN_TEST(test_text_before_url_returns_null);
    RUN_TEST(test_text_after_url_returns_null);
    RUN_TEST(test_missing_closing_tag_returns_null);
    RUN_TEST(test_empty_message_returns_null);
    RUN_TEST(test_no_url_tags_returns_null);
    RUN_TEST(test_mixed_case_tags_returns_null);
    RUN_TEST(test_url_with_query_string);
    RUN_TEST(test_empty_url_between_tags);
}

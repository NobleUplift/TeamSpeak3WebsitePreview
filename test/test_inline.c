#include "unity/unity.h"
#include "core.h"
#include <string.h>
#include <stdlib.h>

/* ── FindURLsInMessage ─────────────────────────────────────────────────────── */

static void test_find_no_urls(void) {
    char* urls[5];
    int count = FindURLsInMessage("hello world", urls, 5);
    TEST_ASSERT_EQUAL_INT(0, count);
}

static void test_find_one_url_only(void) {
    char* urls[5];
    int count = FindURLsInMessage("[URL]https://example.com[/URL]", urls, 5);
    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_EQUAL_STRING("https://example.com", urls[0]);
    free(urls[0]);
}

static void test_find_url_embedded(void) {
    char* urls[5];
    int count = FindURLsInMessage("Check [URL]https://example.com[/URL] please", urls, 5);
    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_EQUAL_STRING("https://example.com", urls[0]);
    free(urls[0]);
}

static void test_find_two_urls(void) {
    char* urls[5];
    int count = FindURLsInMessage("[URL]https://a.com[/URL] and [URL]https://b.com[/URL]", urls, 5);
    TEST_ASSERT_EQUAL_INT(2, count);
    TEST_ASSERT_EQUAL_STRING("https://a.com", urls[0]);
    TEST_ASSERT_EQUAL_STRING("https://b.com", urls[1]);
    free(urls[0]);
    free(urls[1]);
}

static void test_find_respects_max(void) {
    char* urls[2];
    int count = FindURLsInMessage(
        "[URL]https://a.com[/URL] [URL]https://b.com[/URL] [URL]https://c.com[/URL]",
        urls, 2);
    TEST_ASSERT_EQUAL_INT(2, count);
    free(urls[0]);
    free(urls[1]);
}

/* ── BuildMessageWithInlineTitles ─────────────────────────────────────────── */

static void test_build_single_url_replaced(void) {
    char out[512];
    const char* urls[]   = { "https://example.com" };
    const char* titles[] = { "Example Site" };
    BuildMessageWithInlineTitles(
        "Check [URL]https://example.com[/URL] please",
        urls, titles, 1, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING(
        "Check \"Example Site\" <[URL]https://example.com[/URL]> please", out);
}

static void test_build_two_urls_replaced(void) {
    char out[512];
    const char* urls[]   = { "https://a.com", "https://b.com" };
    const char* titles[] = { "Site A", "Site B" };
    BuildMessageWithInlineTitles(
        "[URL]https://a.com[/URL] and [URL]https://b.com[/URL]",
        urls, titles, 2, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING(
        "\"Site A\" <[URL]https://a.com[/URL]> and \"Site B\" <[URL]https://b.com[/URL]>", out);
}

static void test_build_null_title_keeps_tag(void) {
    char out[512];
    const char* urls[]   = { "https://example.com" };
    const char* titles[] = { NULL };
    BuildMessageWithInlineTitles(
        "Check [URL]https://example.com[/URL] please",
        urls, titles, 1, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Check [URL]https://example.com[/URL] please", out);
}

static void test_build_does_not_overflow(void) {
    char out[32];
    const char* urls[]   = { "https://very-long-url.example.com/path?q=1" };
    const char* titles[] = { "A Very Long Page Title That Exceeds The Buffer" };
    BuildMessageWithInlineTitles(
        "[URL]https://very-long-url.example.com/path?q=1[/URL]",
        urls, titles, 1, out, sizeof(out));
    TEST_ASSERT_LESS_OR_EQUAL(31, (int)strlen(out));
    TEST_ASSERT_EQUAL('\0', out[sizeof(out) - 1]);
}

void RunInlineTests(void) {
    RUN_TEST(test_find_no_urls);
    RUN_TEST(test_find_one_url_only);
    RUN_TEST(test_find_url_embedded);
    RUN_TEST(test_find_two_urls);
    RUN_TEST(test_find_respects_max);
    RUN_TEST(test_build_single_url_replaced);
    RUN_TEST(test_build_two_urls_replaced);
    RUN_TEST(test_build_null_title_keeps_tag);
    RUN_TEST(test_build_does_not_overflow);
}

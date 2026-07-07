#include "unity/unity.h"
#include "core.h"
#include <string.h>

static void test_title_and_url_only(void) {
    char out[512];
    BuildPreviewMessage("Example Site", "https://example.com", NULL, NULL, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("\"Example Site\" <[URL]https://example.com[/URL]>", out);
}

static void test_with_og_desc(void) {
    char out[512];
    BuildPreviewMessage("Example Site", "https://example.com", "A description.", NULL, out, sizeof(out));
    const char* expected = "\"Example Site\" <[URL]https://example.com[/URL]>\nA description.";
    TEST_ASSERT_EQUAL_STRING(expected, out);
}

static void test_with_og_image(void) {
    char out[512];
    BuildPreviewMessage("Example Site", "https://example.com", NULL, "https://example.com/img.png", out, sizeof(out));
    /* og_image is ignored — TS3 chat has no tag that renders images */
    const char* expected = "\"Example Site\" <[URL]https://example.com[/URL]>";
    TEST_ASSERT_EQUAL_STRING(expected, out);
}

static void test_all_fields(void) {
    char out[512];
    BuildPreviewMessage("Title", "https://example.com", "Desc", "https://example.com/img.png", out, sizeof(out));
    const char* expected =
        "\"Title\" <[URL]https://example.com[/URL]>"
        "\nDesc";
    TEST_ASSERT_EQUAL_STRING(expected, out);
}

static void test_output_does_not_overflow(void) {
    char out[32];
    /* Write into a small buffer — must not write past out_size */
    BuildPreviewMessage("LongTitle", "https://very-long-url-that-exceeds-buffer.example.com/path", NULL, NULL, out, sizeof(out));
    /* Buffer should be null-terminated within bounds */
    TEST_ASSERT_LESS_OR_EQUAL(31, (int)strlen(out));
    /* Verify null terminator is present (no overflow) */
    TEST_ASSERT_EQUAL('\0', out[sizeof(out) - 1]);
}

void RunMessageTests(void) {
    RUN_TEST(test_title_and_url_only);
    RUN_TEST(test_with_og_desc);
    RUN_TEST(test_with_og_image);
    RUN_TEST(test_all_fields);
    RUN_TEST(test_output_does_not_overflow);
}

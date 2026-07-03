#include "unity/unity.h"
#include "libxml/HTMLparser.h"
#include "libxml/globals.h"
#include "libxml/xpath.h"
#include <string.h>

static char* GetOGProperty(htmlDocPtr doc, xmlXPathContextPtr context, const char* property) {
    char xpath[128];
    xmlXPathObjectPtr result;
    char* value = NULL;
#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
    sprintf_s(xpath, sizeof(xpath), "//meta[@property='%s']/@content", property);
#else
    snprintf(xpath, sizeof(xpath), "//meta[@property='%s']/@content", property);
#endif
    result = xmlXPathEvalExpression((const xmlChar*)xpath, context);
    if (result && !xmlXPathNodeSetIsEmpty(result->nodesetval)) {
        value = (char*)xmlNodeListGetString(doc, result->nodesetval->nodeTab[0]->children, 1);
    }
    if (result) xmlXPathFreeObject(result);
    return value;
}

static const char HTML_ALL_OG[] =
    "<html><head>"
    "<meta property='og:title' content='OG Title'/>"
    "<meta property='og:description' content='OG Description'/>"
    "<meta property='og:image' content='https://example.com/img.jpg'/>"
    "<title>HTML Title</title>"
    "</head><body></body></html>";

static const char HTML_NO_OG[] =
    "<html><head><title>HTML Only Title</title></head><body></body></html>";

static const char HTML_MALFORMED[] =
    "<html><head><title>Malformed</title><body>no closing tags";

static void test_og_title_extracted(void) {
    htmlDocPtr doc = htmlReadMemory(HTML_ALL_OG, (int)strlen(HTML_ALL_OG), NULL, NULL,
                                   HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    TEST_ASSERT_NOT_NULL(doc);

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    char* title = GetOGProperty(doc, ctx, "og:title");
    TEST_ASSERT_NOT_NULL(title);
    TEST_ASSERT_EQUAL_STRING("OG Title", title);

    xmlFree(title);
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);
}

static void test_og_desc_and_image_extracted(void) {
    htmlDocPtr doc = htmlReadMemory(HTML_ALL_OG, (int)strlen(HTML_ALL_OG), NULL, NULL,
                                   HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    TEST_ASSERT_NOT_NULL(doc);

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    char* desc  = GetOGProperty(doc, ctx, "og:description");
    char* image = GetOGProperty(doc, ctx, "og:image");

    TEST_ASSERT_NOT_NULL(desc);
    TEST_ASSERT_EQUAL_STRING("OG Description", desc);
    TEST_ASSERT_NOT_NULL(image);
    TEST_ASSERT_EQUAL_STRING("https://example.com/img.jpg", image);

    xmlFree(desc);
    xmlFree(image);
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);
}

static void test_no_og_title_falls_back_to_html_title(void) {
    htmlDocPtr doc = htmlReadMemory(HTML_NO_OG, (int)strlen(HTML_NO_OG), NULL, NULL,
                                   HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    TEST_ASSERT_NOT_NULL(doc);

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    char* og_title = GetOGProperty(doc, ctx, "og:title");
    TEST_ASSERT_NULL(og_title);

    /* Fall back to <title> */
    char* html_title = NULL;
    xmlXPathObjectPtr result = xmlXPathEvalExpression("/html/head/title", ctx);
    if (result && !xmlXPathNodeSetIsEmpty(result->nodesetval)) {
        html_title = (char*)xmlNodeListGetString(doc,
            result->nodesetval->nodeTab[result->nodesetval->nodeNr - 1]->xmlChildrenNode, 1);
    }
    if (result) xmlXPathFreeObject(result);

    TEST_ASSERT_NOT_NULL(html_title);
    TEST_ASSERT_EQUAL_STRING("HTML Only Title", html_title);

    xmlFree(html_title);
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);
}

static void test_malformed_html_does_not_crash(void) {
    htmlDocPtr doc = htmlReadMemory(HTML_MALFORMED, (int)strlen(HTML_MALFORMED), NULL, NULL,
                                   HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    /* libxml2 tolerates malformed HTML — doc may be non-null */
    if (doc) {
        xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
        char* title = GetOGProperty(doc, ctx, "og:title");
        /* og:title absent — just verify no crash */
        TEST_ASSERT_NULL(title);
        xmlXPathFreeContext(ctx);
        xmlFreeDoc(doc);
    }
    /* If doc is null, test passes — parser rejected it, no crash */
    TEST_PASS();
}

void RunParseTests(void) {
    RUN_TEST(test_og_title_extracted);
    RUN_TEST(test_og_desc_and_image_extracted);
    RUN_TEST(test_no_og_title_falls_back_to_html_title);
    RUN_TEST(test_malformed_html_does_not_crash);
}

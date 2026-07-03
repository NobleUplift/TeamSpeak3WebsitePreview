#include "core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    mem->memory = (char *) realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL) {
        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

const char* GetURLFromMessage(const char* message) {
    const char *urlStart;
    const char *urlEnd;
    char *url;
    size_t length;

    urlStart = strstr(message, "[URL]");
    if (urlStart == NULL) {
        urlStart = strstr(message, "[url]");
    }

    if (urlStart == NULL) {
        return NULL;
    }

    /* Message must begin with [URL] — no leading text allowed */
    if (message != urlStart) {
        return NULL;
    }

    urlStart += 5;

    urlEnd = strstr(urlStart, "[/URL]");
    if (urlEnd == NULL) {
        urlEnd = strstr(urlStart, "[/url]");
    }

    if (urlEnd == NULL) {
        return NULL;
    }

    /* [/URL] must be the very end of the message — no trailing text */
    if (urlEnd[6] != '\0') {
        return NULL;
    }

    length = urlEnd - urlStart;

    url = (char *) malloc(length + 1);
    memcpy((void *) url, urlStart, length);
    url[length] = 0;

    return (const char *) url;
}

void BuildPreviewMessage(const char* title, const char* url,
                         const char* og_desc, const char* og_image,
                         char* out, size_t out_size) {
    /* snprintf truncates safely on overflow; sprintf_s aborts via invalid-parameter handler */
    snprintf(out, out_size, "\"%s\" <[URL]%s[/URL]>", title, url);
#if defined(_WIN32) || defined(WIN32) || defined(WIN64) || defined(_WIN64)
    if (og_desc) {
        strncat_s(out, out_size, "\n", _TRUNCATE);
        strncat_s(out, out_size, og_desc, _TRUNCATE);
    }
    if (og_image) {
        strncat_s(out, out_size, "\n<img src=\"", _TRUNCATE);
        strncat_s(out, out_size, og_image, _TRUNCATE);
        strncat_s(out, out_size, "\">", _TRUNCATE);
    }
#else
    if (og_desc) {
        strncat(out, "\n", out_size - strlen(out) - 1);
        strncat(out, og_desc, out_size - strlen(out) - 1);
    }
    if (og_image) {
        strncat(out, "\n<img src=\"", out_size - strlen(out) - 1);
        strncat(out, og_image, out_size - strlen(out) - 1);
        strncat(out, "\">", out_size - strlen(out) - 1);
    }
#endif
}

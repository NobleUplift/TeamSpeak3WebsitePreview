#include "core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    int n;
    (void)og_image; /* TS3 chat has no tag that renders images */
    /* snprintf is portable (C99 / MSVC>=2015) and truncates safely on overflow. */
    n = snprintf(out, out_size, "\"%s\" <[URL]%s[/URL]>", title, url);
    if (og_desc && n > 0 && (size_t)n < out_size) {
        snprintf(out + n, out_size - (size_t)n, "\n%s", og_desc);
    }
}

int FindURLsInMessage(const char* message, char** urls_out, int max_urls) {
    const char* p = message;
    int count = 0;

    while (*p && count < max_urls) {
        const char* url_start;
        const char* url_end;
        size_t url_len;
        char* url;

        if (strncmp(p, "[URL]", 5) == 0) {
            url_start = p + 5;
        } else if (strncmp(p, "[url]", 5) == 0) {
            url_start = p + 5;
        } else {
            p++;
            continue;
        }

        url_end = strstr(url_start, "[/URL]");
        if (!url_end) url_end = strstr(url_start, "[/url]");
        if (!url_end) { p++; continue; }

        url_len = (size_t)(url_end - url_start);
        url = (char*)malloc(url_len + 1);
        if (!url) break;
        memcpy(url, url_start, url_len);
        url[url_len] = '\0';
        urls_out[count++] = url;

        p = url_end + 6; /* advance past [/URL] */
    }

    return count;
}

void BuildMessageWithInlineTitles(const char* message,
                                  const char** urls, const char** titles,
                                  int url_count, char* out, size_t out_size) {
    const char* p = message;
    size_t out_pos = 0;
    int url_idx = 0;

    if (!out || out_size == 0) return;

    while (*p && out_pos < out_size - 1) {
        const char* tag_content = NULL;

        if (strncmp(p, "[URL]", 5) == 0)      tag_content = p + 5;
        else if (strncmp(p, "[url]", 5) == 0) tag_content = p + 5;

        if (tag_content) {
            const char* close = strstr(tag_content, "[/URL]");
            if (!close) close = strstr(tag_content, "[/url]");

            if (close && url_idx < url_count) {
                if (titles[url_idx]) {
                    int written = snprintf(out + out_pos, out_size - out_pos,
                        "\"%s\" <[URL]%s[/URL]>", titles[url_idx], urls[url_idx]);
                    if (written > 0) {
                        size_t w = (size_t)written;
                        out_pos += w < out_size - out_pos ? w : out_size - out_pos - 1;
                    }
                } else {
                    /* No title — copy tag verbatim */
                    size_t tag_len = (size_t)(close + 6 - p);
                    size_t copy_len = tag_len < out_size - 1 - out_pos ? tag_len : out_size - 1 - out_pos;
                    memcpy(out + out_pos, p, copy_len);
                    out_pos += copy_len;
                }
                p = close + 6;
                url_idx++;
                continue;
            }
        }

        out[out_pos++] = *p++;
    }

    out[out_pos] = '\0';
}

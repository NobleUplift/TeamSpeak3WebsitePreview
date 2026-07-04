#pragma once
#include <stddef.h>

#define MAX_URLS_PER_MESSAGE 5

struct MemoryStruct {
    char *memory;
    size_t size;
};

size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
const char* GetURLFromMessage(const char* message);
void BuildPreviewMessage(const char* title, const char* url,
                         const char* og_desc, const char* og_image,
                         char* out, size_t out_size);

/* Find all [URL]...[/URL] tags embedded anywhere in message.
 * Fills urls_out[0..max_urls-1] with heap-allocated URL strings (caller frees each).
 * Returns the count of URLs found (may be less than max_urls). */
int FindURLsInMessage(const char* message, char** urls_out, int max_urls);

/* Rebuild message replacing each [URL]...[/URL] tag with "title" <[URL]url[/URL]>.
 * urls and titles are parallel arrays of length url_count, matched left-to-right.
 * Where titles[i] is NULL the original tag is copied verbatim.
 * Output is always NUL-terminated and bounded by out_size. */
void BuildMessageWithInlineTitles(const char* message,
                                  const char** urls, const char** titles,
                                  int url_count, char* out, size_t out_size);

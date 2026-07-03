#pragma once
#include <stddef.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
const char* GetURLFromMessage(const char* message);
void BuildPreviewMessage(const char* title, const char* url,
                         const char* og_desc, const char* og_image,
                         char* out, size_t out_size);

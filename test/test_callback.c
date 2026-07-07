#include "unity/unity.h"
#include "core.h"
#include <stdlib.h>
#include <string.h>

static void test_single_chunk_appended(void) {
    struct MemoryStruct mem;
    mem.memory = (char*)malloc(1);
    mem.size = 0;

    const char data[] = "hello";
    size_t written = WriteMemoryCallback((void*)data, 1, 5, &mem);

    TEST_ASSERT_EQUAL_size_t(5, written);
    TEST_ASSERT_EQUAL_size_t(5, mem.size);
    TEST_ASSERT_EQUAL_STRING("hello", mem.memory);

    free(mem.memory);
}

static void test_multiple_chunks_accumulate(void) {
    struct MemoryStruct mem;
    mem.memory = (char*)malloc(1);
    mem.size = 0;

    WriteMemoryCallback((void*)"foo", 1, 3, &mem);
    WriteMemoryCallback((void*)"bar", 1, 3, &mem);

    TEST_ASSERT_EQUAL_size_t(6, mem.size);
    TEST_ASSERT_EQUAL_STRING("foobar", mem.memory);

    free(mem.memory);
}

static void test_zero_nmemb_returns_zero(void) {
    struct MemoryStruct mem;
    mem.memory = (char*)malloc(1);
    mem.memory[0] = '\0';
    mem.size = 0;

    size_t written = WriteMemoryCallback((void*)"data", 1, 0, &mem);

    TEST_ASSERT_EQUAL_size_t(0, written);
    TEST_ASSERT_EQUAL_size_t(0, mem.size);

    free(mem.memory);
}

static void test_return_value_equals_size_times_nmemb(void) {
    struct MemoryStruct mem;
    mem.memory = (char*)malloc(1);
    mem.size = 0;

    const char data[16] = "0123456789abcdef";
    size_t written = WriteMemoryCallback((void*)data, 2, 8, &mem);

    TEST_ASSERT_EQUAL_size_t(16, written);
    TEST_ASSERT_EQUAL_size_t(16, mem.size);

    free(mem.memory);
}

void RunCallbackTests(void) {
    RUN_TEST(test_single_chunk_appended);
    RUN_TEST(test_multiple_chunks_accumulate);
    RUN_TEST(test_zero_nmemb_returns_zero);
    RUN_TEST(test_return_value_equals_size_times_nmemb);
}

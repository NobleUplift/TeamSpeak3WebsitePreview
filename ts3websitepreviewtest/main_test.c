#include "unity/unity.h"
#include <stdio.h>

void RunURLTests(void);
void RunCallbackTests(void);
void RunMessageTests(void);
void RunParseTests(void);

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    int result;
    UNITY_BEGIN();
    RunURLTests();
    RunCallbackTests();
    RunMessageTests();
    RunParseTests();
    result = UNITY_END();
    printf("\nPress Enter to exit...");
    getchar();
    return result;
}

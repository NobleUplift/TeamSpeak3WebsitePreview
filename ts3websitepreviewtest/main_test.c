#include "unity/unity.h"

void RunURLTests(void);
void RunCallbackTests(void);
void RunMessageTests(void);
void RunParseTests(void);

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RunURLTests();
    RunCallbackTests();
    RunMessageTests();
    RunParseTests();
    return UNITY_END();
}

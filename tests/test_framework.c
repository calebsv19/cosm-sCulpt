#include "test_framework.h"

bool run_test_cases(const char* groupName, const TestCase* cases, size_t count) {
    bool allPassed = true;
    for (size_t i = 0; i < count; ++i) {
        const TestCase* tc = &cases[i];
        bool passed = tc->fn();
        if (!passed) {
            fprintf(stderr, "[FAIL] %s::%s\n", groupName, tc->name);
            allPassed = false;
        }
    }

    if (allPassed) {
        printf("[PASS] %s (%zu tests)\n", groupName, count);
    }

    return allPassed;
}

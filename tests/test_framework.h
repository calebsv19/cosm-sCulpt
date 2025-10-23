#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef bool (*TestFunction)(void);

typedef struct {
    const char* name;
    TestFunction fn;
} TestCase;

bool run_test_cases(const char* groupName, const TestCase* cases, size_t count);

#define TEST_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            return false; \
        } \
    } while (0)

#define TEST_ASSERT_MSG(expr, msg) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "[FAIL] %s:%d: %s — %s\n", __FILE__, __LINE__, #expr, msg); \
            return false; \
        } \
    } while (0)

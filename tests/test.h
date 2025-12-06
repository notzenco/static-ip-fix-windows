/*
 * test.h - Simple test framework for C
 *
 * Usage:
 *   TEST(test_name) {
 *       ASSERT(condition);
 *       ASSERT_EQ(expected, actual);
 *       ASSERT_STR_EQ(expected, actual);
 *       ASSERT_NULL(ptr);
 *       ASSERT_NOT_NULL(ptr);
 *   }
 *
 *   int main(void) {
 *       TEST_INIT();
 *       RUN_TEST(test_name);
 *       TEST_REPORT();
 *       return TEST_EXIT_CODE();
 *   }
 */

#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* ============================================================================
 * TEST STATE
 * ============================================================================ */

static int _test_passed = 0;
static int _test_failed = 0;
static int _test_current_failed = 0;
static const char *_test_current_name = NULL;

/* ============================================================================
 * CORE MACROS
 * ============================================================================ */

#define TEST_INIT() \
    do { \
        _test_passed = 0; \
        _test_failed = 0; \
        printf("Running tests...\n\n"); \
    } while (0)

#define TEST(name) static void name(void)

#define RUN_TEST(name) \
    do { \
        _test_current_failed = 0; \
        _test_current_name = #name; \
        name(); \
        if (_test_current_failed == 0) { \
            _test_passed++; \
            printf("  [PASS] %s\n", #name); \
        } else { \
            _test_failed++; \
        } \
    } while (0)

#define TEST_REPORT() \
    do { \
        printf("\n----------------------------------------\n"); \
        printf("Results: %d passed, %d failed\n", _test_passed, _test_failed); \
        if (_test_failed == 0) { \
            printf("All tests passed!\n"); \
        } \
    } while (0)

#define TEST_EXIT_CODE() (_test_failed > 0 ? 1 : 0)

/* ============================================================================
 * ASSERTION MACROS
 * ============================================================================ */

#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            printf("  [FAIL] %s\n", _test_current_name); \
            printf("         Assert failed: %s\n", #cond); \
            printf("         At: %s:%d\n", __FILE__, __LINE__); \
            _test_current_failed = 1; \
            return; \
        } \
    } while (0)

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf("  [FAIL] %s\n", _test_current_name); \
            printf("         Expected: %d, Actual: %d\n", (int)(expected), (int)(actual)); \
            printf("         At: %s:%d\n", __FILE__, __LINE__); \
            _test_current_failed = 1; \
            return; \
        } \
    } while (0)

#define ASSERT_STR_EQ(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            printf("  [FAIL] %s\n", _test_current_name); \
            printf("         Expected: \"%s\"\n", (expected)); \
            printf("         Actual:   \"%s\"\n", (actual)); \
            printf("         At: %s:%d\n", __FILE__, __LINE__); \
            _test_current_failed = 1; \
            return; \
        } \
    } while (0)

#define ASSERT_WSTR_EQ(expected, actual) \
    do { \
        if (wcscmp((expected), (actual)) != 0) { \
            printf("  [FAIL] %s\n", _test_current_name); \
            wprintf(L"         Expected: \"%ls\"\n", (expected)); \
            wprintf(L"         Actual:   \"%ls\"\n", (actual)); \
            printf("         At: %s:%d\n", __FILE__, __LINE__); \
            _test_current_failed = 1; \
            return; \
        } \
    } while (0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            printf("  [FAIL] %s\n", _test_current_name); \
            printf("         Expected NULL, got non-NULL\n"); \
            printf("         At: %s:%d\n", __FILE__, __LINE__); \
            _test_current_failed = 1; \
            return; \
        } \
    } while (0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            printf("  [FAIL] %s\n", _test_current_name); \
            printf("         Expected non-NULL, got NULL\n"); \
            printf("         At: %s:%d\n", __FILE__, __LINE__); \
            _test_current_failed = 1; \
            return; \
        } \
    } while (0)

#endif /* TEST_H */

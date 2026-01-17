/*
 * SUM Chain Ledger App - Test Utilities
 */

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Test result tracking - defined in test_main.c */
extern int g_tests_run;
extern int g_tests_passed;
extern int g_tests_failed;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        g_tests_failed++; \
    } else { \
        printf("  PASS: %s\n", msg); \
        g_tests_passed++; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, msg) TEST_ASSERT((a) == (b), msg)
#define TEST_ASSERT_NEQ(a, b, msg) TEST_ASSERT((a) != (b), msg)
#define TEST_ASSERT_TRUE(cond, msg) TEST_ASSERT((cond), msg)
#define TEST_ASSERT_FALSE(cond, msg) TEST_ASSERT(!(cond), msg)

#define TEST_ASSERT_MEM_EQ(a, b, len, msg) do { \
    g_tests_run++; \
    if (memcmp((a), (b), (len)) != 0) { \
        printf("  FAIL: %s\n", msg); \
        g_tests_failed++; \
    } else { \
        printf("  PASS: %s\n", msg); \
        g_tests_passed++; \
    } \
} while(0)

#define TEST_ASSERT_STR_EQ(a, b, msg) do { \
    g_tests_run++; \
    if (strcmp((a), (b)) != 0) { \
        printf("  FAIL: %s (got '%s', expected '%s')\n", msg, (a), (b)); \
        g_tests_failed++; \
    } else { \
        printf("  PASS: %s\n", msg); \
        g_tests_passed++; \
    } \
} while(0)

#define TEST_SUITE_START(name) do { \
    printf("\n=== Test Suite: %s ===\n", name); \
} while(0)

#define TEST_SUITE_END() do { \
    printf("\n"); \
} while(0)

static inline void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

static inline void print_test_summary(void) {
    printf("\n========================================\n");
    printf("Tests run: %d, Passed: %d, Failed: %d\n",
           g_tests_run, g_tests_passed, g_tests_failed);
    printf("========================================\n");
}

#endif /* TEST_UTILS_H */

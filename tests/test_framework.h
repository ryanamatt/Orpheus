/*
 * Orpheus - a small ncurses text editor
 *
 * Copyright (C) 2026 Ryan Mattson
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Minimal hand-rolled test framework — no external dependencies.
 *
 * Usage:
 *   TEST(my_test) { ASSERT_EQ(1 + 1, 2); }
 *
 *   int main(void) {
 *       RUN(my_test);
 *       SUMMARY();
 *       return g_failed > 0 ? 1 : 0;
 *   }
 * ----------------------------------------------------------------------- */

static int g_passed = 0;
static int g_failed = 0;
static int g_test_failed = 0;   /* set per-test, reset by RUN() */

#define RED   "\x1b[0;31m"
#define GREEN "\x1b[0;32m"
#define RESET "\x1b[0m"

/* Declare a test function. */
#define TEST(name) static void name(void)

/* Run a named test, print result, update counters. */
#define RUN(name)                                                      \
    do {                                                               \
        g_test_failed = 0;                                             \
        name();                                                        \
        if (g_test_failed) {                                           \
            printf("  " RED "FAIL" RESET "  %s\n", #name);             \
            g_failed++;                                                \
        } else {                                                       \
            printf("  " GREEN "pass" RESET "  %s\n", #name);           \
            g_passed++;                                                \
        }                                                              \
    } while (0)

/* Print the summary line and set the process exit code via the caller. */
#define SUMMARY()                                                      \
    printf("\n%d passed, %d failed\n", g_passed, g_failed)

/* --- Assertion macros ---
 * Each macro marks the test as failed and prints a message but does NOT
 * return early, so multiple failures in one test are all reported. */

#define ASSERT_EQ(a, b)                                                \
    do {                                                               \
        if ((a) != (b)) {                                              \
            printf("    ASSERT_EQ failed at %s:%d: "                  \
                   "%s == %s  (%d != %d)\n",                          \
                   __FILE__, __LINE__, #a, #b, (int)(a), (int)(b));   \
            g_test_failed = 1;                                         \
        }                                                              \
    } while (0)

#define ASSERT_NE(a, b)                                                \
    do {                                                               \
        if ((a) == (b)) {                                              \
            printf("    ASSERT_NE failed at %s:%d: "                  \
                   "%s != %s  (both == %d)\n",                        \
                   __FILE__, __LINE__, #a, #b, (int)(a));             \
            g_test_failed = 1;                                         \
        }                                                              \
    } while (0)

#define ASSERT_TRUE(expr)                                              \
    do {                                                               \
        if (!(expr)) {                                                 \
            printf("    ASSERT_TRUE failed at %s:%d: %s\n",           \
                   __FILE__, __LINE__, #expr);                         \
            g_test_failed = 1;                                         \
        }                                                              \
    } while (0)

#define ASSERT_FALSE(expr)                                             \
    do {                                                               \
        if (expr) {                                                    \
            printf("    ASSERT_FALSE failed at %s:%d: %s\n",          \
                   __FILE__, __LINE__, #expr);                         \
            g_test_failed = 1;                                         \
        }                                                              \
    } while (0)

#define ASSERT_STR_EQ(a, b)                                            \
    do {                                                               \
        if (strcmp((a), (b)) != 0) {                                   \
            printf("    ASSERT_STR_EQ failed at %s:%d: "              \
                   "%s == %s  (\"%s\" != \"%s\")\n",                  \
                   __FILE__, __LINE__, #a, #b, (a), (b));             \
            g_test_failed = 1;                                         \
        }                                                              \
    } while (0)

#define ASSERT_NULL(ptr)                                               \
    do {                                                               \
        if ((ptr) != NULL) {                                           \
            printf("    ASSERT_NULL failed at %s:%d: %s\n",           \
                   __FILE__, __LINE__, #ptr);                          \
            g_test_failed = 1;                                         \
        }                                                              \
    } while (0)

#define ASSERT_NOT_NULL(ptr)                                           \
    do {                                                               \
        if ((ptr) == NULL) {                                           \
            printf("    ASSERT_NOT_NULL failed at %s:%d: %s\n",       \
                   __FILE__, __LINE__, #ptr);                          \
            g_test_failed = 1;                                         \
        }                                                              \
    } while (0)

/* Print a suite header so output is easy to skim. */
#define SUITE(name) printf("\n=== %s ===\n", name)

#endif /* TEST_FRAMEWORK_H */
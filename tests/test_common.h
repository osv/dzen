#ifndef DZEN_TEST_COMMON_H
#define DZEN_TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>

/*
 * Test checks must remain active under -DNDEBUG. Keep side effects inside
 * CHECK safe: unlike assert(), the condition is always evaluated once.
 */
static inline void test_fail(const char *file, int line, const char *condition) {
    fprintf(stderr, "%s:%d: check failed: %s\n", file, line, condition);
    fflush(stderr);
    exit(EXIT_FAILURE);
}

#define CHECK(condition)                               \
    do {                                               \
        if (!(condition))                              \
            test_fail(__FILE__, __LINE__, #condition); \
    } while (0)

#endif

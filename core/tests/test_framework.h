#ifndef ALTCROSS_TEST_FRAMEWORK_H
#define ALTCROSS_TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

extern int altcross_test_failures;
extern int altcross_test_count;

#define RUN_TEST(fn)                                                         \
    do {                                                                     \
        altcross_test_count++;                                               \
        printf("RUN  %s\n", #fn);                                            \
        fn();                                                                \
    } while (0)

#define ASSERT_TRUE(cond)                                                    \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
            altcross_test_failures++;                                        \
        }                                                                    \
    } while (0)

#define ASSERT_EQ(expected, actual) ASSERT_TRUE((expected) == (actual))
#define ASSERT_GT(actual, expected) ASSERT_TRUE((actual) > (expected))
#define ASSERT_STREQ(expected, actual)                                       \
    ASSERT_TRUE(strcmp((expected), (actual)) == 0)

#endif

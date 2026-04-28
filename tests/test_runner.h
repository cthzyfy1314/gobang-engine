/* tests/test_runner.h — 自写极简测试 runner，零第三方依赖。
 * 用法：每个 test_*.c 文件在 main() 里调用各 test_* 函数。
 * 函数内用 ASSERT_EQ / ASSERT_TRUE / ASSERT_FALSE 检查。
 * main() 末尾打印 X/Y passed。
 */
#ifndef TEST_RUNNER_H_
#define TEST_RUNNER_H_

#include <stdio.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_EQ(actual, expected, msg) do {                              \
    tests_run++;                                                           \
    if ((actual) == (expected)) {                                          \
        tests_passed++;                                                    \
        printf("  [PASS] %s\n", msg);                                      \
    } else {                                                               \
        printf("  [FAIL] %s  (got %d, expected %d)  at %s:%d\n",           \
               msg, (int)(actual), (int)(expected), __FILE__, __LINE__);   \
    }                                                                      \
} while(0)

#define ASSERT_TRUE(cond, msg)  ASSERT_EQ((cond) ? 1 : 0, 1, msg)
#define ASSERT_FALSE(cond, msg) ASSERT_EQ((cond) ? 1 : 0, 0, msg)

#define TEST_REPORT(suite_name) do {                                       \
    printf("\n=== %s: %d / %d passed ===\n",                               \
           suite_name, tests_passed, tests_run);                           \
    return (tests_run == tests_passed) ? 0 : 1;                            \
} while(0)

#endif /* TEST_RUNNER_H_ */

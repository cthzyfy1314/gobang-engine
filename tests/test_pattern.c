/* tests/test_pattern.c */
#include <string.h>
#include "test_runner.h"
#include "../src/pattern.h"

/* 辅助：用字符串字面量构造一条线。'_' = EMPTY, 'X' = BLACK, 'O' = WHITE */
static int build_line(const char *s, int8_t *out) {
    int n = 0;
    for (int i = 0; s[i]; i++) {
        switch (s[i]) {
            case '_': out[n++] = 0; break;
            case 'X': out[n++] = 1; break;
            case 'O': out[n++] = 2; break;
            default:  break;
        }
    }
    return n;
}

static void test_five_isolated(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("__XXXXX__", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "five: __XXXXX__ -> FIVE x1");
    ASSERT_EQ(s.counts[PAT_OPEN_FOUR], 0, "five: not counted as OPEN_FOUR");
    ASSERT_EQ(s.counts[PAT_OPEN_THREE], 0, "five: not counted as OPEN_THREE");
}

static void test_five_at_edge(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("XXXXX", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "five: XXXXX (full line) -> FIVE x1");
}

static void test_six_overline_counts_as_five(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("_XXXXXX_", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "overline: _XXXXXX_ -> FIVE x1");
}

static void test_white_five(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("__OOOOO__", line);
    pattern_count_in_line(line, n, 2, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "white five: __OOOOO__ -> FIVE x1");
}

static void test_color_isolation(void) {
    int8_t line[16]; PatternStats sB = {0}, sW = {0};
    int n = build_line("__XXXXX__", line);
    pattern_count_in_line(line, n, 1, &sB);
    pattern_count_in_line(line, n, 2, &sW);
    ASSERT_EQ(sB.counts[PAT_FIVE], 1, "iso: black sees XXXXX as FIVE");
    ASSERT_EQ(sW.counts[PAT_FIVE], 0, "iso: white sees XXXXX as nothing");
}

static void test_open_four(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("__XXXX__", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_FOUR], 1, "open_four: __XXXX__ -> OPEN_FOUR x1");
    ASSERT_EQ(s.counts[PAT_FIVE], 0, "open_four: not five");
}

static void test_simple_four_blocked_left(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("OXXXX_", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_SIMPLE_FOUR], 1, "simple_four: OXXXX_ -> SIMPLE_FOUR x1");
    ASSERT_EQ(s.counts[PAT_OPEN_FOUR], 0, "simple_four: not OPEN_FOUR");
}

static void test_simple_four_blocked_right(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("_XXXXO", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_SIMPLE_FOUR], 1, "simple_four: _XXXXO -> SIMPLE_FOUR x1");
}

static void test_simple_four_at_edge(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("XXXX_", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_SIMPLE_FOUR], 1, "simple_four: edge XXXX_ -> SIMPLE_FOUR x1");
}

static void test_dead_four(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("OXXXXO", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_FOUR], 0, "dead_four: not open");
    ASSERT_EQ(s.counts[PAT_SIMPLE_FOUR], 0, "dead_four: not simple");
}

int main(void) {
    test_five_isolated();
    test_five_at_edge();
    test_six_overline_counts_as_five();
    test_white_five();
    test_color_isolation();
    test_open_four();
    test_simple_four_blocked_left();
    test_simple_four_blocked_right();
    test_simple_four_at_edge();
    test_dead_four();
    TEST_REPORT("test_pattern");
}

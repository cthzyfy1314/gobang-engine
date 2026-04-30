/* tests/test_pattern.c */
#include <string.h>
#include "test_runner.h"
#include "../src/board.h"
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

static void test_open_three(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("___XXX___", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_THREE], 1, "open_three: ___XXX___ -> OPEN_THREE x1");
    ASSERT_EQ(s.counts[PAT_SLEEP_THREE], 0, "open_three: not sleep_three");
}

static void test_sleep_three_blocked(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("OXXX__", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_SLEEP_THREE], 1, "sleep_three: OXXX__ -> SLEEP_THREE x1");
    ASSERT_EQ(s.counts[PAT_OPEN_THREE], 0, "sleep_three: not open");
}

static void test_sleep_three_at_edge(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("XXX___", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_SLEEP_THREE], 1, "sleep_three: edge XXX___ -> SLEEP_THREE x1");
}

static void test_dead_three(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("OXXXO", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_THREE], 0, "dead_three: not open");
    ASSERT_EQ(s.counts[PAT_SLEEP_THREE], 0, "dead_three: not sleep");
}

static void test_open_two(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("___XX___", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_TWO], 1, "open_two: ___XX___ -> OPEN_TWO x1");
}

static void test_sleep_two_blocked(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("OXX___", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_SLEEP_TWO], 1, "sleep_two: OXX___ -> SLEEP_TWO x1");
}

static void test_dead_two(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("OXXO", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_TWO], 0, "dead_two: not open");
    ASSERT_EQ(s.counts[PAT_SLEEP_TWO], 0, "dead_two: not sleep");
}

static void test_single_stone_no_pattern(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("__X__", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_TWO], 0, "single: not OPEN_TWO");
    ASSERT_EQ(s.counts[PAT_SLEEP_TWO], 0, "single: not SLEEP_TWO");
}

static void test_count_for_color_horizontal(void) {
    Board b; board_init(&b);
    for (int c = 0; c < 5; c++) b.cells[7][c] = 1;
    PatternStats s = {0};
    pattern_count_for_color(&b, 1, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "for_color: horizontal black 5 -> FIVE x1");
}

static void test_count_for_color_vertical(void) {
    Board b; board_init(&b);
    for (int r = 0; r < 5; r++) b.cells[r][7] = 1;
    PatternStats s = {0};
    pattern_count_for_color(&b, 1, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "for_color: vertical black 5 -> FIVE x1");
}

static void test_count_for_color_diagonal_main(void) {
    Board b; board_init(&b);
    for (int i = 0; i < 5; i++) b.cells[3 + i][3 + i] = 1;
    PatternStats s = {0};
    pattern_count_for_color(&b, 1, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "for_color: main-diag black 5 -> FIVE x1");
}

static void test_count_for_color_diagonal_anti(void) {
    Board b; board_init(&b);
    for (int i = 0; i < 5; i++) b.cells[3 + i][11 - i] = 1;
    PatternStats s = {0};
    pattern_count_for_color(&b, 1, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "for_color: anti-diag black 5 -> FIVE x1");
}

static void test_count_for_color_multiple_threes(void) {
    Board b; board_init(&b);
    for (int c = 4; c < 7; c++) b.cells[7][c] = 1;
    for (int r = 9; r < 12; r++) b.cells[r][7] = 1;
    PatternStats s = {0};
    pattern_count_for_color(&b, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_THREE], 2, "for_color: 2 separate open threes -> OPEN_THREE x2");
}

static void test_evaluate_empty(void) {
    Board b; board_init(&b);
    ASSERT_EQ(pattern_evaluate(&b), 0, "evaluate: empty board -> 0");
}

static void test_evaluate_black_open_three_advantage(void) {
    Board b; board_init(&b);
    for (int c = 4; c < 7; c++) b.cells[7][c] = 1;
    b.move_count = 3;

    b.side_to_move = 1;
    int sB = pattern_evaluate(&b);
    b.side_to_move = 2;
    int sW = pattern_evaluate(&b);

    ASSERT_TRUE(sB > 0, "evaluate: black open_three from black view > 0");
    ASSERT_TRUE(sW < 0, "evaluate: black open_three from white view < 0");
    ASSERT_EQ(sB, -sW, "evaluate: viewpoint symmetric");
}

static void test_evaluate_open_four_dominates_open_three(void) {
    Board b; board_init(&b);
    for (int c = 3; c < 7; c++) b.cells[5][c] = 1;
    for (int c = 3; c < 6; c++) b.cells[10][c] = 2;
    b.move_count = 7;
    b.side_to_move = 1;

    int score = pattern_evaluate(&b);
    ASSERT_TRUE(score > 5000, "evaluate: open_four >> open_three");
}

static void test_evaluate_score_table_values(void) {
    ASSERT_EQ(PATTERN_SCORE[PAT_FIVE],        100000, "score: FIVE = 100000");
    ASSERT_EQ(PATTERN_SCORE[PAT_OPEN_FOUR],    10000, "score: OPEN_FOUR = 10000");
    ASSERT_EQ(PATTERN_SCORE[PAT_OPEN_THREE],    1000, "score: OPEN_THREE = 1000");
    ASSERT_EQ(PATTERN_SCORE[PAT_SIMPLE_FOUR],   1000, "score: SIMPLE_FOUR = 1000");
    ASSERT_EQ(PATTERN_SCORE[PAT_OPEN_TWO],       100, "score: OPEN_TWO = 100");
    ASSERT_EQ(PATTERN_SCORE[PAT_SLEEP_THREE],    100, "score: SLEEP_THREE = 100");
    ASSERT_EQ(PATTERN_SCORE[PAT_SLEEP_TWO],       10, "score: SLEEP_TWO = 10");
    ASSERT_EQ(PATTERN_SCORE[PAT_NONE],             0, "score: NONE = 0");
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
    test_open_three();
    test_sleep_three_blocked();
    test_sleep_three_at_edge();
    test_dead_three();
    test_open_two();
    test_sleep_two_blocked();
    test_dead_two();
    test_single_stone_no_pattern();
    test_count_for_color_horizontal();
    test_count_for_color_vertical();
    test_count_for_color_diagonal_main();
    test_count_for_color_diagonal_anti();
    test_count_for_color_multiple_threes();
    test_evaluate_empty();
    test_evaluate_black_open_three_advantage();
    test_evaluate_open_four_dominates_open_three();
    test_evaluate_score_table_values();
    TEST_REPORT("test_pattern");
}

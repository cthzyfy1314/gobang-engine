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
    ASSERT_EQ(PATTERN_SCORE[PAT_PROTO_THREE],    300, "score: PROTO_THREE = 300");
}

/* Proto-open-three: 2 consec with both immediates open AND at least one
 * side extendable. Uses pattern_count_for_color so the run-scan + broken-scan
 * dedup logic is exercised end-to-end.
 */
static void test_proto_three_both_sides_extendable(void) {
    Board b; board_init(&b);
    /* ___XX___ on row 7, cols 3,4. Both sides have lots of empties. */
    b.cells[7][3] = 1; b.cells[7][4] = 1;
    PatternStats s = {0};
    pattern_count_for_color(&b, 1, &s);
    ASSERT_EQ(s.counts[PAT_PROTO_THREE], 1, "proto3: ___XX___ -> PROTO_THREE x1");
    /* Dedup: the basic OPEN_TWO should be decremented to 0 for this run. */
    ASSERT_EQ(s.counts[PAT_OPEN_TWO], 0, "proto3: dedup OPEN_TWO -> 0");
}

static void test_proto_three_blocked_no_extension(void) {
    Board b; board_init(&b);
    /* _OXX_O on row 7, cols 0..5 — left immediate occupied by white, so not
     * both-immediates-open => not a proto-three (and not an OPEN_TWO either).
     */
    b.cells[7][1] = 2; b.cells[7][2] = 1; b.cells[7][3] = 1; b.cells[7][5] = 2;
    PatternStats s = {0};
    pattern_count_for_color(&b, 1, &s);
    ASSERT_EQ(s.counts[PAT_PROTO_THREE], 0, "proto3: _OXX_O -> 0");
    ASSERT_EQ(s.counts[PAT_OPEN_TWO],    0, "proto3: _OXX_O -> OPEN_TWO 0");
}

static void test_proto_three_minimal_window(void) {
    /* Direct line test: __XX__ should be exactly 1 proto-three.
     * Use pattern_count_for_color on a small inset to avoid edge artefacts.
     */
    Board b; board_init(&b);
    /* row 7 cols 4,5 = X. Cols 2,3,6,7 all empty. Beyond cols 1, 8 also empty. */
    b.cells[7][4] = 1; b.cells[7][5] = 1;
    PatternStats s = {0};
    pattern_count_for_color(&b, 1, &s);
    ASSERT_EQ(s.counts[PAT_PROTO_THREE], 1, "proto3: __XX__ -> PROTO_THREE x1");
    ASSERT_EQ(s.counts[PAT_OPEN_TWO], 0, "proto3: __XX__ -> OPEN_TWO 0 (deduped)");
}

static void test_proto_three_one_side_only(void) {
    /* _XX__ pattern with left immediate at edge.
     * Use line: place XX at cols 0,1 (left edge) with col 2,3 empty, col 4 != color.
     * Left immediate at col -1 is "boundary" = treated as non-color, so left
     * is NOT open. So this is a SLEEP_TWO, not OPEN_TWO. Use cols 1,2 instead.
     */
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("_XX__", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_TWO], 1, "proto3 helper: _XX__ has OPEN_TWO x1 pre-dedup");
    /* Direct exercise of count_broken via count_for_color requires a Board.
     * Below: place black at col 7 and 8 on row 5, cols 5,6,9,10 empty.
     * Layout horizontally: ..__XX__.. — both sides extendable.
     */
    Board b; board_init(&b);
    b.cells[5][7] = 1; b.cells[5][8] = 1;
    PatternStats s2 = {0};
    pattern_count_for_color(&b, 1, &s2);
    ASSERT_EQ(s2.counts[PAT_PROTO_THREE], 1, "proto3 helper: ..__XX__.. -> proto3 x1");
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
    test_proto_three_both_sides_extendable();
    test_proto_three_blocked_no_extension();
    test_proto_three_minimal_window();
    test_proto_three_one_side_only();
    TEST_REPORT("test_pattern");
}

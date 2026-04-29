/* tests/test_search.c */
#include "test_runner.h"
#include "../src/board.h"
#include "../src/search.h"

static void test_evaluate_empty(void) {
    Board b; board_init(&b);
    int score = search_evaluate(&b);
    ASSERT_EQ(score, 0, "evaluate: empty board score = 0");
}

static void test_evaluate_center_better_than_corner(void) {
    /* 黑下中央 vs 黑下角落，从白方视角，中央对白更不利 */
    Board b1; board_init(&b1);
    b1.cells[7][7] = BLACK;
    b1.move_count = 1;
    b1.side_to_move = WHITE;

    Board b2; board_init(&b2);
    b2.cells[0][0] = BLACK;
    b2.move_count = 1;
    b2.side_to_move = WHITE;

    int s1 = search_evaluate(&b1);
    int s2 = search_evaluate(&b2);
    /* 视角是白方，黑下中央对白更不利，所以 s1 < s2 */
    ASSERT_TRUE(s1 < s2, "evaluate: white-perspective with black at center < black at corner");
}

static void test_generate_empty_board(void) {
    Board b; board_init(&b);
    Move out[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(&b, out);
    ASSERT_EQ(n, 1, "generate: empty board returns center only");
    ASSERT_EQ(out[0].row, 7, "generate: center is (7,7) row");
    ASSERT_EQ(out[0].col, 7, "generate: center is (7,7) col");
}

static void test_generate_one_stone(void) {
    /* 棋盘只有 (7,7) 黑子，周围 2 圈应是 5x5-1 = 24 个候选 */
    Board b; board_init(&b);
    b.cells[7][7] = BLACK;
    b.move_count = 1;
    Move out[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(&b, out);
    ASSERT_EQ(n, 24, "generate: one stone at center -> 24 neighbors (5x5-1)");
    int found_center = 0;
    for (int i = 0; i < n; i++) if (out[i].row == 7 && out[i].col == 7) found_center = 1;
    ASSERT_EQ(found_center, 0, "generate: occupied cell excluded");
}

static void test_generate_corner_stone(void) {
    /* 角落 (0,0) 的 2 圈范围是 (0..2, 0..2) = 9 - 1 = 8 个 */
    Board b; board_init(&b);
    b.cells[0][0] = BLACK;
    b.move_count = 1;
    Move out[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(&b, out);
    ASSERT_EQ(n, 8, "generate: corner stone -> 3x3-1 = 8 neighbors");
}

static void test_search_returns_legal_move(void) {
    Board b; board_init(&b);
    b.cells[7][7] = BLACK; b.move_count = 1; b.side_to_move = WHITE;
    SearchResult r = search_best_move(&b, 2);
    ASSERT_TRUE(board_in_bounds(r.best_move.row, r.best_move.col),
                "search: returns in-bounds move");
    ASSERT_EQ(b.cells[r.best_move.row][r.best_move.col], EMPTY,
              "search: returns empty cell");
}

static void test_search_finds_immediate_win(void) {
    /* 黑方 4 连，下一步必胜 */
    Board b; board_init(&b);
    b.cells[7][3] = BLACK;
    b.cells[7][4] = BLACK;
    b.cells[7][5] = BLACK;
    b.cells[7][6] = BLACK;
    b.move_count = 4;
    b.side_to_move = BLACK;

    SearchResult r = search_best_move(&b, 1);
    int found_winning = (r.best_move.row == 7 && (r.best_move.col == 7 || r.best_move.col == 2));
    ASSERT_TRUE(found_winning, "search: finds winning move at (7,2) or (7,7)");
}

static void test_search_nodes_counted(void) {
    Board b; board_init(&b);
    b.cells[7][7] = BLACK; b.move_count = 1; b.side_to_move = WHITE;
    SearchResult r = search_best_move(&b, 2);
    ASSERT_TRUE(r.nodes_searched > 0, "search: nodes_searched > 0");
}

int main(void) {
    test_evaluate_empty();
    test_evaluate_center_better_than_corner();
    test_generate_empty_board();
    test_generate_one_stone();
    test_generate_corner_stone();
    test_search_returns_legal_move();
    test_search_finds_immediate_win();
    test_search_nodes_counted();
    TEST_REPORT("test_search");
}

/* tests/test_search.c */
#include "test_runner.h"
#include "../src/board.h"
#include "../src/search.h"

static void test_evaluate_empty(void) {
    Board b; board_init(&b);
    int score = search_evaluate(&b);
    ASSERT_EQ(score, 0, "evaluate: empty board score = 0");
}

static void test_evaluate_open_three_beats_dead_three(void) {
    /* 活三应比死三分高（pattern evaluator 不再做中央倾向）*/
    Board b1; board_init(&b1);
    for (int c = 4; c < 7; c++) b1.cells[7][c] = BLACK;  /* 活三 */
    b1.move_count = 3;
    b1.side_to_move = BLACK;

    Board b2; board_init(&b2);
    b2.cells[7][4] = WHITE;
    for (int c = 5; c < 8; c++) b2.cells[7][c] = BLACK;
    b2.cells[7][8] = WHITE;
    b2.move_count = 5;
    b2.side_to_move = BLACK;

    int s1 = search_evaluate(&b1);
    int s2 = search_evaluate(&b2);
    ASSERT_TRUE(s1 > s2, "evaluate: open_three score > dead_three score");
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
    /* 棋盘只有 (7,7) 黑子。v2 候选 = 5x5-1 = 24 (2 圈) + 8 个方向各延伸 3/4 cell（= 2×8 = 16）
     * = 24 + 16 = 40
     */
    Board b; board_init(&b);
    b.cells[7][7] = BLACK;
    b.move_count = 1;
    Move out[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(&b, out);
    ASSERT_EQ(n, 40, "generate: one stone at center -> 40 neighbors (5x5-1 + 8dir*2 extend)");
    int found_center = 0;
    for (int i = 0; i < n; i++) if (out[i].row == 7 && out[i].col == 7) found_center = 1;
    ASSERT_EQ(found_center, 0, "generate: occupied cell excluded");
}

static void test_generate_corner_stone(void) {
    /* 角落 (0,0)：2 圈 8 个 + 3 个方向（右/下/右下）各延伸 2 cell = 8 + 6 = 14
     * （左/上/左上方向越界，不计；左下、右上越界对角延伸也不计） */
    Board b; board_init(&b);
    b.cells[0][0] = BLACK;
    b.move_count = 1;
    Move out[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(&b, out);
    ASSERT_EQ(n, 14, "generate: corner stone -> 8 + 6 extend = 14 neighbors");
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

static void test_vcf_immediate_five(void) {
    /* 白方 4 连，下一步必胜，VCF 应该立刻找到 */
    Board b; board_init(&b);
    b.cells[7][3] = WHITE;
    b.cells[7][4] = WHITE;
    b.cells[7][5] = WHITE;
    b.cells[7][6] = WHITE;
    b.move_count = 4;
    b.side_to_move = WHITE;
    b.forbid_enabled = false;  /* 白不需要 forbid */

    Move m;
    int found = search_vcf(&b, 5, &m, NULL);
    ASSERT_TRUE(found, "vcf: finds immediate win for white");
    int legal = (m.row == 7 && (m.col == 2 || m.col == 7));
    ASSERT_TRUE(legal, "vcf: returns (7,2) or (7,7)");
}

static void test_search_mate_distance_short(void) {
    /* 黑方活四，下一步必成 5。mate 距离应极短（root 出发 1 ply）。
     * 验证 mate-by-ply + TT mate-distance 归一化：必须返回 mate-magnitude 分
     * 且接近 SEARCH_INF（短 mate 高分）。若 TT 归一化坏了（用陈旧 ply 偏移
     * 错算距离），分数会被错误缩水离 SEARCH_INF 更远。 */
    Board b; board_init(&b);
    b.cells[7][3] = BLACK;
    b.cells[7][4] = BLACK;
    b.cells[7][5] = BLACK;
    b.cells[7][6] = BLACK;
    b.move_count = 4;
    b.side_to_move = BLACK;
    b.forbid_enabled = false;

    SearchResult r = search_best_move(&b, 4);
    ASSERT_TRUE(IS_MATE_SCORE(r.score), "mate-dist: forced win returns mate-magnitude score");
    ASSERT_TRUE(r.score > 0, "mate-dist: winning side (BLACK to move) gets positive mate");
    /* 1-ply forced win → score 应非常接近 SEARCH_INF。宽松上界容忍搜索路径
     * 差异，但能抓住"归一化把短 mate 错算成远 mate"的回归。 */
    ASSERT_TRUE(r.score >= SEARCH_INF - 16, "mate-dist: immediate win scores as SHORT mate (near SEARCH_INF)");
}

static void test_search_mate_score_not_overflow_eval(void) {
    /* eval 量级（最多 ~1e6）远小于 mate threshold (1e8-1e4)，
     * 普通残局 search_best_move 不应误判为 mate（防止 P0-4 旧实现的虚假 mate）
     */
    Board b; board_init(&b);
    b.cells[7][7] = BLACK; b.move_count = 1; b.side_to_move = WHITE;
    SearchResult r = search_best_move(&b, 2);
    ASSERT_TRUE(!IS_MATE_SCORE(r.score), "search: normal position score is not mate-magnitude");
}

int main(void) {
    test_evaluate_empty();
    test_evaluate_open_three_beats_dead_three();
    test_generate_empty_board();
    test_generate_one_stone();
    test_generate_corner_stone();
    test_search_returns_legal_move();
    test_search_finds_immediate_win();
    test_search_nodes_counted();
    test_vcf_immediate_five();
    test_search_mate_distance_short();
    test_search_mate_score_not_overflow_eval();
    TEST_REPORT("test_search");
}

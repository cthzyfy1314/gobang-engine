/* tests/test_search_n_strikes.c — boundary tests for search_find_n_distinct.
 *
 * search_find_n_distinct 是国规 7 的"五手 N 打"实现：找出当前 side_to_move
 * 在 neighbor 候选里得分最高的 N 个 distinct moves。
 *
 * 本测试覆盖以下边界：
 *   1. n_want=1 → 返回 top-1
 *   2. n_want 远大于实际合法候选数 → clamp 到合法数
 *   3. 空盘 + n_want=5 → search_generate_neighbor_moves 只生成中心 (7,7) 1 个候选 → 1
 *   4. 极短 time_budget_ms (100ms) → Pass 1 prefilter 保证 ≥1 候选
 */
#include "test_runner.h"
#include "../src/board.h"
#include "../src/search.h"

/* 用棋子填满除少数空格的棋盘，制造"实际合法 cell 远少于 n_want"的局面。
 * 注意：search_generate_neighbor_moves 只生成 neighbor 候选 + 越界检查。
 * 我们填一大圈黑白让"邻近圈"覆盖很少 EMPTY cell。
 */
static void put(Board *b, int r, int c, int color) {
    b->cells[r][c] = (uint8_t)color;
}

/* 1. n_want=1，普通局面：返回 top-1 only */
static void test_n_want_1(void) {
    Board b; board_init(&b);
    b.side_to_move = WHITE;
    b.forbid_enabled = false;
    put(&b, 7, 7, BLACK);
    put(&b, 7, 8, BLACK);
    put(&b, 8, 7, WHITE);
    b.move_count = 3;  /* 必须 > 0；否则 generate_neighbor_moves 走空盘路径 */

    Move out[16];
    int scores[16];
    int n = search_find_n_distinct(&b, 2, 1, out, scores, 500);
    ASSERT_EQ(n, 1, "n_distinct: n_want=1 -> exactly 1 result");
    ASSERT_TRUE(board_in_bounds(out[0].row, out[0].col),
                "n_distinct: result move in bounds");
    ASSERT_EQ(b.cells[out[0].row][out[0].col], EMPTY,
              "n_distinct: result cell is empty");
}

/* 2. n_want=10，但实际合法 cell 很少 (artificial constraint: 填满大部分棋盘) → clamped。
 *    构造：填满 14x15 棋盘只留几格 EMPTY，但要注意 neighbor 候选生成机制只看"已有子周围 2 圈"。
 *    填满整盘后 neighbor 候选 = 所有 EMPTY cell；我们留 5 个 empty cell。
 *    side_to_move = WHITE 避免黑 forbid 把所有 cell 干掉。
 */
static void test_n_want_exceeds_legal(void) {
    Board b; board_init(&b);
    b.forbid_enabled = false;
    b.side_to_move = WHITE;

    /* 全棋盘填棋子，但保留 5 个 EMPTY cell。
     * 用 alternating 颜色避免胜负判定带来副作用（即使有 5 连 search 仍只搜邻近候选，不会 early-return）。
     * 实际上 alphabeta 内部会 check board_check_winner，若已胜负则返回 mate score。
     * 我们关掉这种 noise：用 sparse 间隔填法，让无连续 5 子。
     * 简化：横向每行 BWBWBW... 模式，无 5 连。
     */
    int blanks[5][2] = { {3,3}, {5,5}, {9,9}, {11,11}, {13,13} };
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            int is_blank = 0;
            for (int k = 0; k < 5; k++) {
                if (blanks[k][0] == r && blanks[k][1] == c) { is_blank = 1; break; }
            }
            if (is_blank) continue;
            /* alternating；保证无 5 连 */
            put(&b, r, c, ((r + c) % 2 == 0) ? BLACK : WHITE);
        }
    }
    b.move_count = BOARD_SIZE * BOARD_SIZE - 5;

    Move out[32];
    int scores[32];
    int n = search_find_n_distinct(&b, 1, 10, out, scores, 500);
    ASSERT_TRUE(n <= 5, "n_distinct: result clamped to <= 5 legal cells");
    ASSERT_TRUE(n >= 1, "n_distinct: at least 1 result returned");
    /* 所有返回 cell 应是 blanks 之一 */
    for (int i = 0; i < n; i++) {
        int matched = 0;
        for (int k = 0; k < 5; k++) {
            if (out[i].row == blanks[k][0] && out[i].col == blanks[k][1]) {
                matched = 1; break;
            }
        }
        ASSERT_TRUE(matched, "n_distinct: returned cell is one of the empty cells");
    }
    /* distinct 检查 */
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            int same = (out[i].row == out[j].row && out[i].col == out[j].col);
            ASSERT_FALSE(same, "n_distinct: results are distinct");
        }
    }
}

/* 3. 空盘 + n_want=5：search_generate_neighbor_moves 只生成中心 (7,7)，
 *    Pass 1 后 valid_n=1，最终 picked=1（clamp 到 valid_n）。
 */
static void test_empty_board_clamp(void) {
    Board b; board_init(&b);
    Move out[16];
    int scores[16];
    int n = search_find_n_distinct(&b, 2, 5, out, scores, 500);
    ASSERT_EQ(n, 1, "n_distinct: empty board returns 1 candidate (center only)");
    ASSERT_EQ(out[0].row, 7, "n_distinct: empty board candidate row=7");
    ASSERT_EQ(out[0].col, 7, "n_distinct: empty board candidate col=7");
}

/* 4. 极短 time_budget_ms (100ms)：Pass 1 prefilter 保证至少返回 1 候选。
 *    用一个有几颗棋子的局面让 generate_neighbor_moves 返回 >1，
 *    Pass 2 ID 在 100ms 内可能完成不了多少层但 scores 已经被 Pass 1 占位为 0。
 */
static void test_short_time_budget(void) {
    Board b; board_init(&b);
    b.side_to_move = BLACK;
    put(&b, 7, 7, WHITE);
    put(&b, 7, 8, BLACK);
    put(&b, 8, 7, WHITE);
    put(&b, 8, 8, BLACK);
    b.move_count = 4;

    Move out[16];
    int scores[16];
    int n = search_find_n_distinct(&b, 8, 3, out, scores, 100);
    ASSERT_TRUE(n >= 1, "n_distinct: short time budget -> at least 1 candidate (Pass 1 guarantee)");
    ASSERT_TRUE(n <= 3, "n_distinct: short time budget -> at most n_want=3");
    /* 所有返回的 move 必须 valid（empty + in-bounds）*/
    for (int i = 0; i < n; i++) {
        ASSERT_TRUE(board_in_bounds(out[i].row, out[i].col),
                    "n_distinct: short-budget result in bounds");
        ASSERT_EQ(b.cells[out[i].row][out[i].col], EMPTY,
                  "n_distinct: short-budget result cell empty");
    }
}

int main(void) {
    test_n_want_1();
    test_n_want_exceeds_legal();
    test_empty_board_clamp();
    test_short_time_budget();
    TEST_REPORT("test_search_n_strikes");
}

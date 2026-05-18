/* tests/test_4_3_fix.c
 * 回归测试：KNOWN_BUGS.md #29 — 四三 eval blindness.
 *
 * 装载该游戏在 B11 之前的局面，调 search_best_move_timed，
 * 断言引擎 NOT 选 K11（即旧的失误选择）。
 * 可接受的防守选择：K9 / L10 / H6 / J10 等，能切断白棋 anti-diag + row-9 双重威胁。
 */
#include "test_runner.h"
#include "../src/board.h"
#include "../src/search.h"
#include "../src/zobrist.h"

static void place(Board *b, int r, int c, int color) {
    b->cells[r][c] = (uint8_t)color;
    b->history[b->move_count].row = (int8_t)r;
    b->history[b->move_count].col = (int8_t)c;
    b->history[b->move_count].color = (int8_t)color;
    b->move_count++;
}

static void test_4_3_avoidance(void) {
    Board b; board_init(&b);

    /* 花月 opening + 中盘到 B11 之前 (10 stones on board)
     * B: H8(7,7), I10(5,8), H11(4,7), G9(6,6), I11(4,8)
     * W: H9(6,7), I9(6,8)→ wait, original W2=H9 then W4=I9 then W6=J9
     * 重建：
     *   B1=H8=(7,7), W2=H9=(6,7), B3=I10=(5,8), W4=I9=(6,8), B5=H11=(4,7),
     *   W6=J9=(6,9), B7=G9=(6,6), W8=I7=(8,8), B9=I11=(4,8), W10=J8=(7,9)
     */
    place(&b, 7, 7, BLACK);   /* B1 H8 */
    place(&b, 6, 7, WHITE);   /* W2 H9 */
    place(&b, 5, 8, BLACK);   /* B3 I10 */
    place(&b, 6, 8, WHITE);   /* W4 I9 */
    place(&b, 4, 7, BLACK);   /* B5 H11 */
    place(&b, 6, 9, WHITE);   /* W6 J9 */
    place(&b, 6, 6, BLACK);   /* B7 G9 */
    place(&b, 8, 8, WHITE);   /* W8 I7 */
    place(&b, 4, 8, BLACK);   /* B9 I11 */
    place(&b, 7, 9, WHITE);   /* W10 J8 */

    b.side_to_move = BLACK;  /* B11 to move */
    b.zobrist_hash = zobrist_compute(&b);
    b.forbid_enabled = true;

    /* 用 depth=8 t=3000ms（够看到 W12=K9 后的 4-3）*/
    search_reset();
    SearchResult r = search_best_move_timed(&b, 8, 3000);

    /* 失误选择是 K11=(4,10) */
    int picked_K11 = (r.best_move.row == 4 && r.best_move.col == 10);
    ASSERT_EQ(picked_K11, 0, "4-3 fix: B11 NOT K11 (old mistake)");

    /* 应当选 K9 / L10 / H6 / J10 / 其他切断对方双威胁的着 */
    int picked_K9  = (r.best_move.row == 6 && r.best_move.col == 10);
    int picked_L10 = (r.best_move.row == 5 && r.best_move.col == 11);
    int picked_H6  = (r.best_move.row == 9 && r.best_move.col == 7);
    int picked_J10 = (r.best_move.row == 5 && r.best_move.col == 9);
    int picked_acceptable = picked_K9 || picked_L10 || picked_H6 || picked_J10;
    /* 不强求 acceptable — 引擎可能找到别的等价好招。只 log 出来 */
    printf("  [INFO] 4-3 fix: picked (%d,%d) score=%d nodes=%ld   K9=%d L10=%d H6=%d J10=%d\n",
           r.best_move.row, r.best_move.col, r.score, r.nodes_searched,
           picked_K9, picked_L10, picked_H6, picked_J10);
    (void)picked_acceptable;
}

int main(void) {
    zobrist_init();
    test_4_3_avoidance();
    TEST_REPORT("test_4_3_fix");
}

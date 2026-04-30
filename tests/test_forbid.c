/* tests/test_forbid.c */
#include "test_runner.h"
#include "../src/board.h"
#include "../src/forbid.h"

/* 辅助：在棋盘上手动布置棋子 */
static void put(Board *b, int r, int c, int color) {
    b->cells[r][c] = (uint8_t)color;
}

/* === 长连禁手 === */
static void test_overline_horizontal(void) {
    Board b; board_init(&b);
    /* 黑 4 子在 (7,3)(7,4)(7,5)(7,6)，再下 (7,7) 形成 5 连
     * 但若在 (7,2) 已有黑则下 (7,7) 形成 6 连（长连禁手）
     * 构造：黑 (7,2)(7,3)(7,4)(7,5)(7,7)（缺 (7,6) 不成连），
     *       但要让 (7,6) 落黑后形成 ≥6 连：需要在 (7,2)..(7,5) + (7,7) 是黑，落 (7,6) 后 6 连
     */
    put(&b, 7, 2, BLACK);
    put(&b, 7, 3, BLACK);
    put(&b, 7, 4, BLACK);
    put(&b, 7, 5, BLACK);
    put(&b, 7, 7, BLACK);
    /* 在 (7,6) 落黑：(7,2)..(7,7) 6 连 → 长连禁手 */
    ASSERT_EQ(forbid_check_black(&b, 7, 6), FORBID_OVERLINE,
              "overline: 6 in a row -> FORBID_OVERLINE");
}

static void test_five_only_no_forbid(void) {
    /* 4 黑子 + 落黑 = 5 连，应非禁手 */
    Board b; board_init(&b);
    put(&b, 7, 3, BLACK);
    put(&b, 7, 4, BLACK);
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    ASSERT_EQ(forbid_check_black(&b, 7, 7), FORBID_NONE,
              "five: 5 in a row not forbidden");
}

static void test_five_overrides_overline(void) {
    /* 国规 9.2-c：落黑后既形成 5 连又形成 6 连 → 五连优先
     * 构造：横向有 5 连成立点 + 同时另一方向有 6 连
     * 实际较难单步构造，简化测试：(7,3)(7,4)(7,5)(7,6) 已 4 连，落 (7,7) 形成 5 连
     * 同时 (3,3)(4,4)(5,5)(6,6)(8,8) 黑 + 落 (7,7) 形成对角 5 连——但这不是 6 连
     * 改：让对角形成 6 连：(2,2)..(6,6) 5 连黑 + (8,8) 黑，落 (7,7) → 对角 (2,2)..(8,8) 7 连
     * 同时横向 (7,3)..(7,7) 5 连
     */
    Board b; board_init(&b);
    put(&b, 7, 3, BLACK);
    put(&b, 7, 4, BLACK);
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    put(&b, 2, 2, BLACK);
    put(&b, 3, 3, BLACK);
    put(&b, 4, 4, BLACK);
    put(&b, 5, 5, BLACK);
    put(&b, 6, 6, BLACK);
    put(&b, 8, 8, BLACK);
    /* 落 (7,7)：横 5 连 + 对角 7 连。五连优先 → FORBID_NONE */
    ASSERT_EQ(forbid_check_black(&b, 7, 7), FORBID_NONE,
              "9.2-c: 5 + overline simultaneously -> NOT forbidden (5 wins)");
}

/* === 四四禁手 === */
static void test_double_four(void) {
    /* 在 (7,7) 落黑同时形成两个冲四：
     *   横向：(7,3)(7,4)(7,5)(7,6) + (7,7) = 5 连？不行，5 连不算"四"。
     *   要"冲四"即落子后形成"4 连一端被堵或边界"。
     *   构造：横向 (7,3)(7,4)(7,5) 黑 + (7,2) 白堵 + 落 (7,7) 黑 → 横向 (7,3)..(7,7) = 5 连，又是五连
     *   换思路：让落子完成两个独立"4"。
     *   横：(7,4)(7,5)(7,7) 黑 + 落 (7,6) 黑 → (7,4..7) 4 连 + (7,3) 空 / (7,8) 空 → 活四
     *   竖：(4,6)(5,6)(7,6) 黑 + 落 (7,6)？同一格不行
     *
     * 改用经典双四 case：
     *   假定中心 (7,7) 落黑。
     *   横：(7,5)(7,6) 黑 + (7,8)(7,9) 黑 → 落 (7,7) 后 (7,5)..(7,9) 5 连？是 5 连，不算双四
     *
     * 用 V 形：两个方向各形成"冲四 _XXXX_ 或 XXXX_"
     *   横：(7,3)(7,4)(7,6) 黑 → 落 (7,7) 后 ..横向 (7,3)(7,4)_(7,6)(7,7) → 不是 4 连
     *
     * 简单 case：落 (7,7) 同时形成横冲四 + 竖冲四
     *   横：(7,3)(7,4)(7,5)(7,6) → 落 (7,7) 是 5 连，不算
     *   横：(7,4)(7,5)(7,6) + (7,8) 黑 → 落 (7,7) 后 (7,4..8) 5 连
     *
     * 双冲四（不形成五）：跳冲四
     *   横：(7,4)(7,6)(7,8) 黑 → 落 (7,5)？(7,4)(7,5)(7,6)_(7,8) 跳冲四
     *   太复杂。直接构造：
     *   横：(7,3)(7,4) 黑，(7,9)(7,10) 黑 → 不是冲四
     *
     * 改用"活四"组合：
     *   横：(7,4)(7,5)(7,6) 黑, (7,3) 空, (7,8) 空。落 (7,7) → (7,4)(7,5)(7,6)(7,7) 4 连，两端 (7,3) 空, (7,8) 空 → 活四
     *   竖：(4,7)(5,7)(6,7) 黑, (3,7) 空, (8,7) 空。落 (7,7) → (4..7,7) 4 连，两端 (3,7)(8,7) 空 → 活四
     *   双活四 → FORBID_DOUBLE_FOUR
     */
    Board b; board_init(&b);
    put(&b, 7, 4, BLACK);
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    put(&b, 4, 7, BLACK);
    put(&b, 5, 7, BLACK);
    put(&b, 6, 7, BLACK);
    ASSERT_EQ(forbid_check_black(&b, 7, 7), FORBID_DOUBLE_FOUR,
              "double_four: two open fours simultaneously -> FORBID_DOUBLE_FOUR");
}

static void test_single_four_not_forbid(void) {
    Board b; board_init(&b);
    put(&b, 7, 4, BLACK);
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    /* 落 (7,7) 仅形成单方向 4 连 → 非禁手（仅冲四不构成禁手） */
    ASSERT_EQ(forbid_check_black(&b, 7, 7), FORBID_NONE,
              "single_four: 1 four-direction -> not forbidden");
}

/* === 三三禁手（简化版）=== */
static void test_double_three(void) {
    /* 落 (7,7) 同时形成两个 _XXX_ 形式
     * 横：(7,5)(7,6) 黑, (7,4) 空, (7,8) 空 → 落 (7,7) → (7,5)(7,6)(7,7) 3 连，两端空 → 活三
     * 竖：(5,7)(6,7) 黑, (4,7) 空, (8,7) 空 → 落 (7,7) → (5,7)(6,7)(7,7) 3 连，两端空 → 活三
     */
    Board b; board_init(&b);
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    put(&b, 5, 7, BLACK);
    put(&b, 6, 7, BLACK);
    ASSERT_EQ(forbid_check_black(&b, 7, 7), FORBID_DOUBLE_THREE,
              "double_three: two open threes -> FORBID_DOUBLE_THREE");
}

static void test_single_three_not_forbid(void) {
    Board b; board_init(&b);
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    /* 落 (7,7) 仅形成单方向活三 → 非禁手 */
    ASSERT_EQ(forbid_check_black(&b, 7, 7), FORBID_NONE,
              "single_three: 1 three-direction -> not forbidden");
}

static void test_blocked_three_not_forbid(void) {
    /* 一端被白堵的"眠三"不算活三 */
    Board b; board_init(&b);
    put(&b, 7, 4, WHITE);  /* 左端被堵 */
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    put(&b, 5, 7, BLACK);
    put(&b, 6, 7, BLACK);
    /* 横向被堵→眠三, 竖向活三。只有 1 活三 → 非禁手 */
    ASSERT_EQ(forbid_check_black(&b, 7, 7), FORBID_NONE,
              "blocked_three: 1 dir is sleep_three -> not double_three");
}

/* === 边界 case === */
static void test_corner_no_false_positive(void) {
    Board b; board_init(&b);
    /* 角落附近不构造禁手，应 NONE */
    ASSERT_EQ(forbid_check_black(&b, 0, 0), FORBID_NONE, "corner: empty board -> NONE");
}

static void test_occupied_returns_none(void) {
    Board b; board_init(&b);
    put(&b, 7, 7, WHITE);
    ASSERT_EQ(forbid_check_black(&b, 7, 7), FORBID_NONE,
              "occupied: cell taken -> NONE");
}

int main(void) {
    test_overline_horizontal();
    test_five_only_no_forbid();
    test_five_overrides_overline();
    test_double_four();
    test_single_four_not_forbid();
    test_double_three();
    test_single_three_not_forbid();
    test_blocked_three_not_forbid();
    test_corner_no_false_positive();
    test_occupied_returns_none();
    TEST_REPORT("test_forbid");
}

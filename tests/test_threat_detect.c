/* tests/test_threat_detect.c — exercise color_has_winning_setup via thin wrapper.
 * (file renamed from test_winning_setup.c: Windows AppCompat installer-detection
 *  heuristic refuses to launch any executable matching *setup* without UAC elevation.)
 *
 * "Winning setup" 定义（见 search.c 的 color_has_winning_setup 注释）：
 *   该 side 存在一手能形成：FIVE (fours==99) / 双四 (fours>=2) / 四三 (fours>=1 && threes>=1)。
 *
 * 注意：纯活四 (单 OPEN_FOUR 但无伴生 three) 当前实现归在 fours>=1，
 *   但需要 fours>=2 或 fours>=1 && threes>=1 才返回 1。这是 search.c 注释里明说的保守策略。
 *   所以本测试遵循实现，不期望"单活四"被识别为 winning setup。
 */
#include "test_runner.h"
#include "../src/board.h"
#include "../src/search.h"

static void put(Board *b, int r, int c, int color) {
    b->cells[r][c] = (uint8_t)color;
}

/* 1. 空盘：无任何威胁，应 = 0 */
static void test_empty_board(void) {
    Board b; board_init(&b);
    int r = search_color_has_winning_setup(&b, BLACK);
    ASSERT_EQ(r, 0, "winning_setup: empty board -> 0");
    int r2 = search_color_has_winning_setup(&b, WHITE);
    ASSERT_EQ(r2, 0, "winning_setup: empty board (white) -> 0");
}

/* 2. 白方落子可形成活四 + 同时活三（"四三"）：
 *   横向 (7,5)(7,6)(7,8) 白 + 落 (7,7) → 横向 (7,5)..(7,8) 4 连，两端空 → OPEN_FOUR
 *      Wait, (7,5)(7,6)_(7,8) + 落 (7,7) = (7,5)(7,6)(7,7)(7,8) 4 连，两端 (7,4)(7,9) 空 → 活四
 *   竖向 (5,7)(6,7) 白 + 落 (7,7) → (5..7,7) 3 连，两端 (4,7)(8,7) 空 → 活三
 *   → fours==1 && threes==1 → 四三 → winning setup
 */
static void test_white_four_three_setup(void) {
    Board b; board_init(&b);
    b.side_to_move = WHITE;
    b.forbid_enabled = false;  /* 白无禁手 */
    put(&b, 7, 5, WHITE);
    put(&b, 7, 6, WHITE);
    put(&b, 7, 8, WHITE);
    put(&b, 5, 7, WHITE);
    put(&b, 6, 7, WHITE);
    b.move_count = 5;
    int r = search_color_has_winning_setup(&b, WHITE);
    ASSERT_EQ(r, 1, "winning_setup: white 4-3 setup -> 1");
}

/* 3. 黑方双四：落 (7,7) 同时形成横 4 + 竖 4（两个活四） → fours==2 → 双四 winning
 *   但落 (7,7) 在双活四同时是 FORBID_DOUBLE_FOUR；color_has_winning_setup 内部对黑会跳过禁手 cell。
 *   → 这个 (7,7) 会被 forbid 排除，函数返回 0（除非别处还有 winning cell）。
 *   所以这个 case 我们关掉 forbid 来纯测"双四识别"逻辑。
 */
static void test_black_double_four_no_forbid(void) {
    Board b; board_init(&b);
    b.side_to_move = BLACK;
    b.forbid_enabled = false;  /* 关 forbid 以纯测 helper 识别能力 */
    put(&b, 7, 4, BLACK);
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    put(&b, 4, 7, BLACK);
    put(&b, 5, 7, BLACK);
    put(&b, 6, 7, BLACK);
    b.move_count = 6;
    int r = search_color_has_winning_setup(&b, BLACK);
    ASSERT_EQ(r, 1, "winning_setup: black double-four (forbid off) -> 1");
}

/* 4. 黑方四三 winning（一活四 + 一活三，不是双四）：
 *   横向 (7,5)(7,6)(7,8) 黑 + 落 (7,7) → 横向 4 连两端空 → 活四
 *   竖向 (5,7)(6,7) 黑 + 落 (7,7) → 竖向 3 连两端空 → 活三
 *   → fours==1 && threes==1 → 四三 → winning
 *   forbid 检查：落 (7,7) 形成 1 个 four + 1 个 three，是否禁手？
 *     双四需要 2 个 four；单 four 不构成四四禁手。
 *     双三需要 2 个 three；单 three 不构成三三禁手。
 *     → 非禁手，不会被排除。
 */
static void test_black_four_three_setup(void) {
    Board b; board_init(&b);
    b.side_to_move = BLACK;
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    put(&b, 7, 8, BLACK);
    put(&b, 5, 7, BLACK);
    put(&b, 6, 7, BLACK);
    b.move_count = 5;
    int r = search_color_has_winning_setup(&b, BLACK);
    ASSERT_EQ(r, 1, "winning_setup: black 4-3 setup -> 1");
}

/* 5. 黑方仅有单活三（没有任何 winning setup） — 应返回 0 */
static void test_black_only_open_three(void) {
    Board b; board_init(&b);
    b.side_to_move = BLACK;
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    b.move_count = 2;
    /* 落 (7,7) 仅形成单方向活三 → fours==0, threes==1 → 不是 winning setup */
    int r = search_color_has_winning_setup(&b, BLACK);
    ASSERT_EQ(r, 0, "winning_setup: black only open-three available -> 0");
}

/* 6. 黑方有"5-in-row-already-on-board"：(7,3)..(7,7) 已是黑 5 连。
 *    helper 仅看"能否在 1 步内造出威胁"；棋盘已有 5 连意味着上一手已经赢了，
 *    黑下一手是否有 winning setup 取决于邻近 cell 落黑后是否再次制造威胁。
 *    必须设置 move_count > 0，否则 search_generate_neighbor_moves 走空盘路径只返回 (7,7)。
 *    落 (7,8) 黑：横向 (7,3)..(7,8) 6 连 → pattern_count_in_line 当 FIVE（PAT_FIVE 包含 ≥6 长连）。
 *    forbid_enabled=true 时 (7,8) 是 OVERLINE 禁手，会被排除 → 但 (7,2) 也能形成 6 连（同样禁手）。
 *    因此 forbid_enabled=false 才能让 helper 返回 1。
 */
static void test_black_already_won_helper_returns_one_when_forbid_off(void) {
    Board b; board_init(&b);
    b.side_to_move = BLACK;
    b.forbid_enabled = false;
    put(&b, 7, 3, BLACK);
    put(&b, 7, 4, BLACK);
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    put(&b, 7, 7, BLACK);
    b.move_count = 5;
    int r = search_color_has_winning_setup(&b, BLACK);
    ASSERT_EQ(r, 1, "winning_setup: black-already-5-in-row, (7,8)/(7,2) makes 6-line classified as FIVE -> 1");
}

/* 7. 白方 5 连已成（白长连也算白胜）— helper 看下一手是否能赢
 *    白方再落子若能制造 FIVE 或 OPEN_FOUR 双向威胁 → return 1
 *    (7,3)..(7,7) 白 5 连：落 (7,8) 白 → 6 连 → pattern 当 FIVE → 99 → return 1
 */
static void test_white_already_won_extends_to_overline(void) {
    Board b; board_init(&b);
    b.side_to_move = WHITE;
    b.forbid_enabled = false;
    put(&b, 7, 3, WHITE);
    put(&b, 7, 4, WHITE);
    put(&b, 7, 5, WHITE);
    put(&b, 7, 6, WHITE);
    put(&b, 7, 7, WHITE);
    b.move_count = 5;
    int r = search_color_has_winning_setup(&b, WHITE);
    ASSERT_EQ(r, 1, "winning_setup: white-already-5, (7,8) extends to 6-line -> 1");
}

/* 8. 黑方双四，但落点 (7,7) 是禁手（双四） — forbid_enabled=true 应排除该 cell。
 *    周围若没有其它 winning cell → helper 返回 0。
 */
static void test_black_double_four_forbid_excluded(void) {
    Board b; board_init(&b);
    b.side_to_move = BLACK;
    b.forbid_enabled = true;
    put(&b, 7, 4, BLACK);
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    put(&b, 4, 7, BLACK);
    put(&b, 5, 7, BLACK);
    put(&b, 6, 7, BLACK);
    b.move_count = 6;
    int r = search_color_has_winning_setup(&b, BLACK);
    ASSERT_EQ(r, 0, "winning_setup: black double-four cell is FORBID_DOUBLE_FOUR -> excluded -> 0");
}

int main(void) {
    test_empty_board();
    test_white_four_three_setup();
    test_black_double_four_no_forbid();
    test_black_four_three_setup();
    test_black_only_open_three();
    test_black_already_won_helper_returns_one_when_forbid_off();
    test_white_already_won_extends_to_overline();
    test_black_double_four_forbid_excluded();
    TEST_REPORT("test_threat_detect");
}

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

static void test_four_plus_three_not_double_anything(void) {
    /* 四 + 三 (不同方向) 不构成双四也不构成双三 → FORBID_NONE
     * 横：(7,4)(7,5)(7,6) 黑 + 落 (7,7) → 横向 4 连开放（活四，记 1 个四）
     * 竖：(5,7)(6,7) 黑 + 落 (7,7) → 竖向 3 连开放（活三，记 1 个三）
     * → 1 四 + 1 三 ≠ 双四 ≠ 双三 → NONE
     */
    Board b; board_init(&b);
    put(&b, 7, 4, BLACK);
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    put(&b, 5, 7, BLACK);
    put(&b, 6, 7, BLACK);
    ASSERT_EQ(forbid_check_black(&b, 7, 7), FORBID_NONE,
              "four+three: single four + single three (different dirs) -> NONE");
}

static void test_occupied_returns_none(void) {
    Board b; board_init(&b);
    put(&b, 7, 7, WHITE);
    ASSERT_EQ(forbid_check_black(&b, 7, 7), FORBID_NONE,
              "occupied: cell taken -> NONE");
}

/* === 回归测试：同方向双四（2026-05-16 修复的 bug）===
 *
 * 落 (7,7) 黑后，行 7 形成两个独立的"四"，threat 位置不同：
 *   col:    3 4 5 6 7 8 9 10 11
 *   piece:  B B B _ B _ B B  B
 *
 *   窗 [3..7] = (B B B _ B) → 4 黑 + 1 空（threat at col 6）→ four #1
 *   窗 [7..11] = (B _ B B B) → 4 黑 + 1 空（threat at col 8）→ four #2
 *
 * 两个独立威胁，对手只能堵一个 → Renju 国规判 FORBID_DOUBLE_FOUR。
 *
 * 老版本 has_four_through_center 返回 boolean 漏掉这种 case。
 * 现 count_fours_through_center 按 threat 去重计数后正确。
 */
static void test_double_four_same_direction(void) {
    Board b; board_init(&b);
    put(&b, 7, 3,  BLACK);
    put(&b, 7, 4,  BLACK);
    put(&b, 7, 5,  BLACK);
    put(&b, 7, 9,  BLACK);
    put(&b, 7, 10, BLACK);
    put(&b, 7, 11, BLACK);
    /* (7,6) (7,7) (7,8) 都空，落 (7,7) */
    ASSERT_EQ(forbid_check_black(&b, 7, 7), FORBID_DOUBLE_FOUR,
              "same_direction_double_four: 2 distinct fours in row -> FORBID_DOUBLE_FOUR");
}

/* === P2-9: forbid four 计数边界 case ===
 * 巩固 count_fours_through_center 在贴边 + 相邻 window 共享黑子的边沿情况。
 *
 * 测试 1: 贴边活四
 *   落 (0,4) 黑后行 0 有 (0,1)(0,2)(0,3)(0,5) = B，加 center 形成 X X X X 在 col 1..4 + col 5=B
 *   → 实际是 (0,1)..(0,5) 连续 5 黑 = 五连 → 优先五连规则
 *   改：(0,1)(0,2)(0,3) + center (0,4) → 4 连黑（col 1..4），(0,0)=越界，(0,5)=_
 *   → 一端被边界堵 + 一端空 = SIMPLE_FOUR (1 four)；单方向单 four → 非禁手
 */
static void test_edge_four_not_double(void) {
    Board b; board_init(&b);
    put(&b, 0, 1, BLACK);
    put(&b, 0, 2, BLACK);
    put(&b, 0, 3, BLACK);
    /* 落 (0,4)：横向 (0,1..4) = 4 连黑，左侧边界堵，右侧空 → 1 个 four
     * 其他方向都无威胁 → NONE */
    ASSERT_EQ(forbid_check_black(&b, 0, 4), FORBID_NONE,
              "edge_four: 4 in row at edge, only 1 four-dir -> not forbidden");
}

/* 测试 2: 同方向 _BBBB_ 活四 mask 去重
 *   落 (7,7) 黑后行 7 形成 _ B B B B _ B B _：但要避免成 5 连。
 *   构造 (7,4)(7,5)(7,6) 黑 + center (7,7) → 横 col 4..7 = 4 黑连
 *   (7,3)=_, (7,8)=_：活四，2 个 5-window 共享 mask → 应记 1 个 four
 *   另外造 1 个不相干方向的活四 → 跨方向 total_fours=2 → DOUBLE_FOUR
 *   竖向 (4,7)(5,7)(6,7) 黑 + center (7,7) → 竖 row 4..7 4 黑连 + (3,7)=_ (8,7)=_ 活四
 *   横 1 + 竖 1 = 2 four → DOUBLE_FOUR。
 *   这就是 test_double_four 已覆盖的 case。
 *
 * 真正巩固 mask 去重：让横向有两个相邻活四 5-window 共享 mask 但确实算 1 个，
 *   保证不被误算成 2。+ 另一方向再 1 four → 应 total=2 → DOUBLE_FOUR（不是 3）。
 *   实际效果一致；只是验证 mask 去重在贴边窗口边界仍正确。
 */
static void test_open_four_mask_dedup_at_edge(void) {
    Board b; board_init(&b);
    /* 横向 4 连贴近边界 (0,0..3)：落 (0,0)，(0,1)(0,2)(0,3) 黑
     *   line center=col 0，line[5]=B(假定), line[6..8]=B(col 1..3), line[9]=_(col 4)
     *   line[0..4]=越界(-1)，line[5]=1, line[6..8]=1, line[9]=0, line[10]=0
     *   5-windows: i=1..5（要 line[i..i+4] 全合法）
     *     i=1..4 都含 -1 跳过
     *     i=5: [1,1,1,1,0] → 4 黑 1 空(at 9)？mask=0b...，5+6+7+8 位 → 1 个 four
     *     count_fours_through_center 返回 1
     *   单方向 1 four → 不双四 → NONE
     */
    put(&b, 0, 1, BLACK);
    put(&b, 0, 2, BLACK);
    put(&b, 0, 3, BLACK);
    ASSERT_EQ(forbid_check_black(&b, 0, 0), FORBID_NONE,
              "edge_open_four_dedup: BBBB at corner, 1 four-dir -> not forbidden");
}

/* === P1-2: 真活三 vs 形式活三 ===
 * 形式 `_XXX_` 但两端 _ 再外侧都被 WHITE 堵 → 落子任一端只能成冲四（一端被白堵），
 * 不是真活三。两个方向都这种"形式但非真"的活三 → 不应触发双三禁手。
 *
 * 构造（横方向）：落 (7,7) 黑后 line=[?,W,_,B,B,B,_,W,?]，center=line[5]=col 7=B。
 *   (7,3)=W, (7,5)(7,6)=B, (7,9)=W；中心 (7,7) 假定落黑后 _XXX_ 在 col 5..7，
 *   但 col 8=_ 后面 col 9=W → 右侧延伸 _XXXX_ 不可成真活四。
 *   类似左侧 col 4=_ 后面 col 3=W → 左侧延伸也不可。
 *
 * 同样构造另一方向（竖）。两方向"形式三"但"非真活三" → 应 FORBID_NONE。
 */
static void test_true_open_three_vs_formal(void) {
    Board b; board_init(&b);
    /* 横向 */
    put(&b, 7, 3, WHITE);
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    put(&b, 7, 9, WHITE);
    /* 竖向 */
    put(&b, 3, 7, WHITE);
    put(&b, 5, 7, BLACK);
    put(&b, 6, 7, BLACK);
    put(&b, 9, 7, WHITE);
    /* 形式上两方向都活三，但延伸成不了真活四 → 非真活三 → 非禁手 */
    ASSERT_EQ(forbid_check_black(&b, 7, 7), FORBID_NONE,
              "true_open_three: formal _XXX_ blocked by outer-outer WHITE -> not forbidden");
}

/* === P1-3: 贴边 _XXX_ 边界假阳性 ===
 * 落 (0,2) 黑后横向贴边 line=[B,B,B,_,?,?,?,...]，center=col 2 是 B。
 *   line 居中坐标：center=line[5]=(0,2)=B(假定)。
 *   k=-2 → (0,0)=B, k=-1 → (0,1)=B, k=0 → center=B, k=1 → (0,3)=_,
 *   k=-3..k=-5 越界 → line[0..2]=-1
 *   即 line = [-1,-1,-1,1,1,1,0,?,?,?,?]，
 *   形式 _XXX_ 在 i=2: line[2]=-1, line[3..5]=B, line[4]=0 → 但 line[2]≠0 → 不匹配。
 *   再考虑 i=3: line[3..7]=1,1,1,0,? → line[3]≠0 → 不匹配。
 *   贴边 BBB_ 在形式判定中本来就不是 _XXX_（左侧无 _）。
 *
 * 真正的"贴边假阳性"场景：(0,0)..(0,2)=_,B,B + center=(0,3)=B → 形式 _XXX_，左端是真 _，
 *   但向左延伸 _XXXX_ 需要 (0,-1)=越界 → outer_left = -1。
 *   老版本 outer_left==1 才排除，-1 不排除 → 形式三仍计入 → 假阳性。
 *   新 P1-3 修复后 outer_left == -1 也排除。
 *
 * 测试：落 (0,3) 黑，横向 (0,1)(0,2)=B + 形式 _XXX_ 在 col 1..3。
 *       但 (0,0) 是 _，左外为越界。再加竖向相同形态 (1,3)(2,3)=B，竖向边界类似。
 *       双方向都贴边 → 老版双三假阳性；新版应 NONE。
 */
static void test_edge_open_three_no_false_positive(void) {
    Board b; board_init(&b);
    /* 横向 (0,1)(0,2)=B，落 (0,3)，(0,0)=_, (0,4)=_ */
    put(&b, 0, 1, BLACK);
    put(&b, 0, 2, BLACK);
    /* 竖向 (1,3)(2,3)=B，落 (0,3)，(3,3)=_, 向上越界 */
    put(&b, 1, 3, BLACK);
    put(&b, 2, 3, BLACK);
    /* (0,3) center：横 line center=col 3
     *   line=[-1,-1,-1,-1,1,1,0,0,0,0,0]: center=line[5]=B(假定)，左 line[3]=B(0,2), line[4]=?(0,1)=B
     *   wait 重新: k=-5..5 对应 col=-2..8。col=-2,-1 越界(-1), col=0=_, col=1=B, col=2=B, col=3=center=B, col=4..8=_
     *   line[0]=-1, line[1]=-1, line[2]=0(col 0), line[3]=1(col 1), line[4]=1(col 2),
     *   line[5]=1(center), line[6..10]=0(col 4..8)
     *   形式 _XXX_ 在 i=2: line[2]=0, line[3..5]=1,1,1, line[6]=0 → ✓
     *   center 5 ∈ [3..5] ✓; outer_left=line[1]=-1 → 新版排除；outer_right=line[6]=0 → OK
     *   新版排除 → 0 个形式三 → 真三 0
     * 类似竖方向。两方向都因贴边被排除 → 非双三 → FORBID_NONE
     */
    ASSERT_EQ(forbid_check_black(&b, 0, 3), FORBID_NONE,
              "edge_open_three: _XXX_ adjacent to board edge -> not true open three");
}

/* === P1-2: 真活三确实算（正向验证）===
 * 真正的双真活三（_XXX_ 两端外侧都是真 EMPTY）→ 应触发 FORBID_DOUBLE_THREE
 * 跟 test_double_three 类似，但显式确认我们没把所有活三都判废。
 * 落 (7,7)：横 (7,5)(7,6)=B → _XXX_ in col 4..8，外侧 col 3..9 都空；
 *           竖同。两个真真活三 → DOUBLE_THREE
 */
static void test_true_double_three_still_forbidden(void) {
    Board b; board_init(&b);
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    put(&b, 5, 7, BLACK);
    put(&b, 6, 7, BLACK);
    /* 周围都空 → 两方向真活三 */
    ASSERT_EQ(forbid_check_black(&b, 7, 7), FORBID_DOUBLE_THREE,
              "true_double_three: two truly open threes -> FORBID_DOUBLE_THREE");
}

/* ===== 中盘 forbid 场景 ===== */

/* 中盘双三：周围有几个白棋子（不阻塞活三延伸），落黑形成双活三 → 仍 DOUBLE_THREE。
 *   横向 (7,5)(7,6) 黑，外侧 col 3,4,8,9 都空（不影响）；
 *   竖向 (5,7)(6,7) 黑，外侧 row 3,4,8,9 都空；
 *   远处加一些 BW 棋子模拟中盘。
 */
static void test_midgame_double_three(void) {
    Board b; board_init(&b);
    /* 中盘环境：放一些远离 (7,7) 的非干扰棋子 */
    put(&b, 1, 1, WHITE);
    put(&b, 2, 13, BLACK);
    put(&b, 13, 2, WHITE);
    put(&b, 12, 12, BLACK);
    put(&b, 10, 3, WHITE);
    put(&b, 3, 10, BLACK);

    /* 关键 setup（不被远处棋子影响）*/
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    put(&b, 5, 7, BLACK);
    put(&b, 6, 7, BLACK);

    ASSERT_EQ(forbid_check_black(&b, 7, 7), FORBID_DOUBLE_THREE,
              "midgame: double_three with surrounding stones still triggers");
}

/* 中盘长连：连续 6 黑（同方向连续 6 子）→ FORBID_OVERLINE
 *   构造 (7,2)(7,3)(7,4)(7,5)(7,7) 黑 + 落 (7,6) → (7,2)..(7,7) 6 连
 *   长度 7 overline：(7,1)(7,2)(7,3)(7,4)(7,5)(7,7) 黑 + 落 (7,6) → (7,1)..(7,7) 7 连
 */
static void test_midgame_overline_seven(void) {
    Board b; board_init(&b);
    /* 中盘环境 */
    put(&b, 10, 10, WHITE);
    put(&b, 5, 10, WHITE);

    /* 7 长连 setup */
    put(&b, 7, 1, BLACK);
    put(&b, 7, 2, BLACK);
    put(&b, 7, 3, BLACK);
    put(&b, 7, 4, BLACK);
    put(&b, 7, 5, BLACK);
    put(&b, 7, 7, BLACK);
    /* 落 (7,6)：(7,1)..(7,7) = 7 连黑 → 长连禁手 */
    ASSERT_EQ(forbid_check_black(&b, 7, 6), FORBID_OVERLINE,
              "midgame: 7-in-a-row -> FORBID_OVERLINE");
}

/* 双三可用但黑落到别处（非 winning cell）→ 那个 cell 是 NONE。
 *   双三 winning cell 是 (7,7)（参见 test_double_three 构造）。
 *   黑若改落到远处的 (0,0)，该 cell 无威胁 → 非禁手。
 *   注：仅验证"非 winning cell 也没 forbid"，不验证 winning cell 仍禁。
 */
static void test_midgame_double_three_available_but_play_elsewhere(void) {
    Board b; board_init(&b);
    put(&b, 7, 5, BLACK);
    put(&b, 7, 6, BLACK);
    put(&b, 5, 7, BLACK);
    put(&b, 6, 7, BLACK);
    /* (7,7) 是双三禁手点。但黑选择落到 (0,0)（远离任何 setup）→ 应 NONE */
    ASSERT_EQ(forbid_check_black(&b, 0, 0), FORBID_NONE,
              "midgame: black plays elsewhere (0,0) -> NONE despite (7,7) being forbidden");
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
    test_four_plus_three_not_double_anything();
    test_double_four_same_direction();   /* 回归：2026-05-16 修复的 bug */
    test_true_open_three_vs_formal();    /* P1-2: 真活三 vs 形式活三 */
    test_edge_open_three_no_false_positive(); /* P1-3: 贴边假阳性 */
    test_true_double_three_still_forbidden(); /* P1-2 正向：真双活三仍判禁 */
    test_edge_four_not_double();         /* P2-9: 贴边 four 边界 */
    test_open_four_mask_dedup_at_edge(); /* P2-9: mask 去重在边界 */
    test_midgame_double_three();         /* 中盘双三 */
    test_midgame_overline_seven();       /* 中盘 7 长连 */
    test_midgame_double_three_available_but_play_elsewhere(); /* 双三可用但落别处 */
    TEST_REPORT("test_forbid");
}

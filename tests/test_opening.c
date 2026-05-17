/* tests/test_opening.c — 国规 26 种指定开局数据自检 */
#include <string.h>
#include "../src/opening.h"
#include "../src/board.h"
#include "test_runner.h"

static void test_count(void) {
    printf("\n-- test_count --\n");
    ASSERT_EQ(opening_count(), 26, "opening_count == 26");
    ASSERT_EQ(OPENING_COUNT, 26, "OPENING_COUNT macro == 26");
}

static void test_b1_always_h8(void) {
    printf("\n-- test_b1_always_h8 --\n");
    for (int i = 0; i < OPENING_COUNT; i++) {
        const RenjuOpening *o = opening_get(i);
        ASSERT_TRUE(o != NULL, "opening_get non-null");
        ASSERT_EQ(o->b1[0], 7, "B1 row == 7 (H8)");
        ASSERT_EQ(o->b1[1], 7, "B1 col == 7 (H8)");
    }
}

static void test_w2_position(void) {
    printf("\n-- test_w2_position --\n");
    /* 直指：W2 = (6,7) = H9；斜指：W2 = (6,8) = I9 */
    int direct = 0, indirect = 0;
    for (int i = 0; i < OPENING_COUNT; i++) {
        const RenjuOpening *o = opening_get(i);
        if (o->is_direct) {
            ASSERT_EQ(o->w2[0], 6, "direct W2 row");
            ASSERT_EQ(o->w2[1], 7, "direct W2 col == 7 (H9)");
            direct++;
        } else {
            ASSERT_EQ(o->w2[0], 6, "indirect W2 row");
            ASSERT_EQ(o->w2[1], 8, "indirect W2 col == 8 (I9)");
            indirect++;
        }
    }
    ASSERT_EQ(direct, 13, "13 direct openings");
    ASSERT_EQ(indirect, 13, "13 indirect openings");
}

static void test_b3_in_5x5(void) {
    printf("\n-- test_b3_in_5x5 --\n");
    for (int i = 0; i < OPENING_COUNT; i++) {
        const RenjuOpening *o = opening_get(i);
        int dr = o->b3[0] - 7; if (dr < 0) dr = -dr;
        int dc = o->b3[1] - 7; if (dc < 0) dc = -dc;
        ASSERT_TRUE(dr <= 2 && dc <= 2, "B3 within 5x5 around H8");
        /* B3 不能与 B1 或 W2 重合 */
        ASSERT_FALSE(o->b3[0] == o->b1[0] && o->b3[1] == o->b1[1], "B3 != B1");
        ASSERT_FALSE(o->b3[0] == o->w2[0] && o->b3[1] == o->w2[1], "B3 != W2");
    }
}

static void test_all_unique(void) {
    printf("\n-- test_all_unique --\n");
    /* 三元组 (B1, W2, B3) 应当全部互不相同 */
    int dups = 0;
    for (int i = 0; i < OPENING_COUNT; i++) {
        const RenjuOpening *a = opening_get(i);
        for (int j = i + 1; j < OPENING_COUNT; j++) {
            const RenjuOpening *b = opening_get(j);
            if (a->w2[0] == b->w2[0] && a->w2[1] == b->w2[1] &&
                a->b3[0] == b->b3[0] && a->b3[1] == b->b3[1]) {
                dups++;
            }
        }
    }
    ASSERT_EQ(dups, 0, "no duplicate (W2,B3) tuples among 26");
}

static void test_names_unique_and_nonempty(void) {
    printf("\n-- test_names_unique_and_nonempty --\n");
    for (int i = 0; i < OPENING_COUNT; i++) {
        const RenjuOpening *o = opening_get(i);
        ASSERT_TRUE(o->name_cn != NULL && o->name_cn[0] != '\0', "name_cn non-empty");
        ASSERT_TRUE(o->name_jp != NULL && o->name_jp[0] != '\0', "name_jp non-empty");
    }
    /* 名字唯一性 */
    int name_dups = 0;
    for (int i = 0; i < OPENING_COUNT; i++) {
        for (int j = i + 1; j < OPENING_COUNT; j++) {
            if (strcmp(RENJU_OPENINGS[i].name_cn, RENJU_OPENINGS[j].name_cn) == 0) name_dups++;
            if (strcmp(RENJU_OPENINGS[i].name_jp, RENJU_OPENINGS[j].name_jp) == 0) name_dups++;
        }
    }
    ASSERT_EQ(name_dups, 0, "no duplicate names");
}

static void test_get_bounds(void) {
    printf("\n-- test_get_bounds --\n");
    ASSERT_TRUE(opening_get(0) != NULL, "get(0) non-null");
    ASSERT_TRUE(opening_get(25) != NULL, "get(25) non-null");
    ASSERT_TRUE(opening_get(-1) == NULL, "get(-1) NULL");
    ASSERT_TRUE(opening_get(26) == NULL, "get(26) NULL");
    ASSERT_TRUE(opening_get(100) == NULL, "get(100) NULL");
}

static void test_matches(void) {
    printf("\n-- test_matches --\n");
    /* 寒星 = idx 0: B1=H8(7,7), W2=H9(6,7), B3=H10(5,7) */
    int idx = -1;
    ASSERT_TRUE(opening_matches(7, 7, 6, 7, 5, 7, &idx), "寒星 matches");
    ASSERT_EQ(idx, 0, "寒星 idx == 0");
    /* 花月 = idx 3: B1=H8, W2=H9, B3=I10(5,8) */
    ASSERT_TRUE(opening_matches(7, 7, 6, 7, 5, 8, &idx), "花月 matches");
    ASSERT_EQ(idx, 3, "花月 idx == 3");
    /* 长星 = idx 13: B1=H8, W2=I9(6,8), B3=J10(5,9) */
    ASSERT_TRUE(opening_matches(7, 7, 6, 8, 5, 9, &idx), "长星 matches");
    ASSERT_EQ(idx, 13, "长星 idx == 13");
    /* 彗星 = idx 25: B1=H8, W2=I9, B3=J6(9,9) */
    ASSERT_TRUE(opening_matches(7, 7, 6, 8, 9, 9, &idx), "彗星 matches");
    ASSERT_EQ(idx, 25, "彗星 idx == 25");
    /* 不存在的组合 —— B3=H8 自身 */
    ASSERT_FALSE(opening_matches(7, 7, 6, 7, 7, 7, &idx), "B3=B1 not match");
    ASSERT_EQ(idx, -1, "no-match idx == -1");
    /* W2 不在 3x3 内（H8 直接相邻）—— W2=H11(4,7) 不是合法 W2 */
    ASSERT_FALSE(opening_matches(7, 7, 4, 7, 5, 7, NULL), "W2 too far not match");
}

static void test_choose_strategy(void) {
    printf("\n-- test_choose_strategy --\n");
    int idx = opening_choose_by_black_strategy();
    ASSERT_TRUE(idx >= 0 && idx < OPENING_COUNT, "strategy idx in range");
}

static void test_first_move_h8(void) {
    printf("\n-- test_first_move_h8 --\n");
    Move m = opening_first_move();
    ASSERT_EQ(m.row, 7, "first move row 7");
    ASSERT_EQ(m.col, 7, "first move col 7");
    ASSERT_EQ(m.color, BLACK, "first move BLACK");
}

int main(void) {
    test_count();
    test_b1_always_h8();
    test_w2_position();
    test_b3_in_5x5();
    test_all_unique();
    test_names_unique_and_nonempty();
    test_get_bounds();
    test_matches();
    test_choose_strategy();
    test_first_move_h8();
    TEST_REPORT("test_opening");
}

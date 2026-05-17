/* src/opening.c — 国规 26 种指定开局数据 + 接口实现
 *
 * 坐标解释器（内部 row,col → 显示）：
 *   字母 = 'A' + col (本工程使用 A..O 含 I)；数字 = 15 - row。
 *   H8 = (7,7), H9 = (6,7), I9 = (6,8), G10 = (5,6), H10 = (5,7), J10 = (5,9), ...
 *
 * --- 26 珠形枚举原理 ---
 * 国规 4: B1 = H8 (天元); W2 在 H8 周围 3x3; B3 在 H8 周围 5x5。
 * 经对称归约后:
 *   - W2 = H9 (北邻) → 称"直指"。其它 7 个 3x3 邻居通过旋转/镜像等价到 H9。
 *   - W2 = I9 (东北邻) → 称"斜指"。其它 3 个斜邻通过旋转等价到 I9。
 *
 * 对每个 W2 位置，B3 候选 = 5x5 内 23 个空位。再经过沿 W2-B1 轴的镜像对称归约
 * → 直指剩 13 个，斜指剩 13 个，合计 26 种。
 *
 * --- 数据来源 ---
 * 名称取自用户给定的 26 中文名，按"直指 13 + 斜指 13"分组对应到 RIF 罗马字
 * 日文名 (RenjuNet /openings/)。
 * B3 坐标按 RIF 规范枚举：
 *   - 直指: B3 在 5x5 右半 (col >= 7)。从 W2(H9) 邻位起按行优先扫描。
 *   - 斜指: B3 在 5x5 内沿 W2-B1 轴东南侧（含轴）。
 * 注：本表的"名 ↔ 位置"对应关系采用 RIF 中常见的编号顺序；如比赛裁判使用
 * 不同教材的编号映射，只需调整本表中 name_cn/name_jp 字段顺序即可，B3 集合
 * 本身（26 个坐标）与官方完全一致。
 */
#include "opening.h"
#include <stddef.h>

#define B1_H8 { 7, 7 }
#define W2_H9 { 6, 7 }   /* 直指: W2 在 B1 正北 */
#define W2_I9 { 6, 8 }   /* 斜指: W2 在 B1 东北 */

/* 直指 13 个 B3 位置 (W2=H9, col>=7, 不含 B1=(7,7) 和 W2=(6,7))：
 *   col=7:  (5,7) (8,7) (9,7)                  -- 3 个
 *   col=8:  (5,8) (6,8) (7,8) (8,8) (9,8)      -- 5 个
 *   col=9:  (5,9) (6,9) (7,9) (8,9) (9,9)      -- 5 个
 *
 * 斜指 13 个 B3 位置 (W2=I9, 5x5 = rows 5..9, cols 5..9, 沿 H8-I9 对称轴
 * 取一半 + 轴上点，去除 B1=(7,7) 和 W2=(6,8))：
 *   对角轴方向 = (-1, +1)。对称轴上的点：(5,5)(6,6)*(7,7)*(8,8)(9,9)
 *     (* = B1; (6,6) 镜像对应 B1 反方向；(8,8) 也在轴上)
 *   实际枚举选 5x5 中 (row + col >= 14) 的一半即 (row+col) ∈ {14,15,16,17,18}:
 *     (5,9)(6,8)*(6,9)(7,7)*(7,8)(7,9)(8,6)(8,7)(8,8)(8,9)(9,5)(9,6)(9,7)(9,8)(9,9)
 *   去掉 B1,W2 = 13 个: (5,9)(6,9)(7,8)(7,9)(8,6)(8,7)(8,8)(8,9)(9,5)(9,6)(9,7)(9,8)(9,9)
 */

const RenjuOpening RENJU_OPENINGS[OPENING_COUNT] = {
    /* === 直指 (W2 = H9) — 13 种 ===
     * 按"列 7 → 列 8 → 列 9，每列内 row 5→9"的扫描顺序对应名字。
     * 这里 row 升序 = 显示 y 降序（远离 W2 北 → 接近 W2 南）。
     */
    /*  0 */ { "寒星",   "Kansei",    B1_H8, W2_H9, { 5, 7 }, true },   /* B3 = H10 (W2 正北一步) */
    /*  1 */ { "溪月",   "Keigetsu",  B1_H8, W2_H9, { 8, 7 }, true },   /* B3 = H7  (B1 正南) */
    /*  2 */ { "燕月",   "Sosei",     B1_H8, W2_H9, { 9, 7 }, true },   /* B3 = H6  (B1 正南两步) */
    /*  3 */ { "花月",   "Kagetsu",   B1_H8, W2_H9, { 5, 8 }, true },   /* B3 = I10 (W2 东北) */
    /*  4 */ { "残月",   "Zangetsu",  B1_H8, W2_H9, { 6, 8 }, true },   /* B3 = I9  (W2 正东) */
    /*  5 */ { "雨月",   "Ugetsu",    B1_H8, W2_H9, { 7, 8 }, true },   /* B3 = I8  (B1 正东) */
    /*  6 */ { "金星",   "Kinsei",    B1_H8, W2_H9, { 8, 8 }, true },   /* B3 = I7  (B1 东南) */
    /*  7 */ { "松月",   "Shougetsu", B1_H8, W2_H9, { 9, 8 }, true },   /* B3 = I6  (B1 东南远) */
    /*  8 */ { "丘月",   "Kyugetsu",  B1_H8, W2_H9, { 5, 9 }, true },   /* B3 = J10 (W2 东北远) */
    /*  9 */ { "新月",   "Shingetsu", B1_H8, W2_H9, { 6, 9 }, true },   /* B3 = J9  (B1 东北) */
    /* 10 */ { "瑞星",   "Zuigetsu",  B1_H8, W2_H9, { 7, 9 }, true },   /* B3 = J8  (B1 正东远) */
    /* 11 */ { "山月",   "Sangetsu",  B1_H8, W2_H9, { 8, 9 }, true },   /* B3 = J7  (B1 东南远 2) */
    /* 12 */ { "游星",   "Yusei",     B1_H8, W2_H9, { 9, 9 }, true },   /* B3 = J6  (B1 东南角) */

    /* === 斜指 (W2 = I9) — 13 种 ===
     * 按 row+col 升序 + 同和值内 col 升序 扫描，跳过 B1 和 W2。
     */
    /* 13 */ { "长星",   "Chosei",    B1_H8, W2_I9, { 5, 9 }, false },  /* B3 = J10 (W2 正北) */
    /* 14 */ { "长月",   "Kyogetsu",  B1_H8, W2_I9, { 6, 9 }, false },  /* B3 = J9  (W2 正东) */
    /* 15 */ { "恒星",   "Kosei",     B1_H8, W2_I9, { 7, 8 }, false },  /* B3 = I8  (B1 正东) */
    /* 16 */ { "水月",   "Suigetsu",  B1_H8, W2_I9, { 7, 9 }, false },  /* B3 = J8  (B1 正东远) */
    /* 17 */ { "流星",   "Ryusei",    B1_H8, W2_I9, { 8, 6 }, false },  /* B3 = G7  (B1 西南) */
    /* 18 */ { "云月",   "Ungetsu",   B1_H8, W2_I9, { 8, 7 }, false },  /* B3 = H7  (B1 正南) */
    /* 19 */ { "浦月",   "Hogetsu",   B1_H8, W2_I9, { 8, 8 }, false },  /* B3 = I7  (B1 东南，对称轴) */
    /* 20 */ { "岚月",   "Rangetsu",  B1_H8, W2_I9, { 8, 9 }, false },  /* B3 = J7  (B1 东南远) */
    /* 21 */ { "银月",   "Gingetsu",  B1_H8, W2_I9, { 9, 5 }, false },  /* B3 = F6  (B1 西南远) */
    /* 22 */ { "明星",   "Myojo",     B1_H8, W2_I9, { 9, 6 }, false },  /* B3 = G6  (B1 西南) */
    /* 23 */ { "夕月",   "Shagetsu",  B1_H8, W2_I9, { 9, 7 }, false },  /* B3 = H6  (B1 正南远) */
    /* 24 */ { "明月",   "Meigetsu",  B1_H8, W2_I9, { 9, 8 }, false },  /* B3 = I6  (B1 东南远 2) */
    /* 25 */ { "彗星",   "Suisei",    B1_H8, W2_I9, { 9, 9 }, false },  /* B3 = J6  (B1 东南角) */
};

int opening_count(void) {
    return OPENING_COUNT;
}

const RenjuOpening *opening_get(int idx) {
    if (idx < 0 || idx >= OPENING_COUNT) return NULL;
    return &RENJU_OPENINGS[idx];
}

bool opening_matches(int b1r, int b1c, int w2r, int w2c, int b3r, int b3c, int *out_idx) {
    for (int i = 0; i < OPENING_COUNT; i++) {
        const RenjuOpening *o = &RENJU_OPENINGS[i];
        if (o->b1[0] == b1r && o->b1[1] == b1c &&
            o->w2[0] == w2r && o->w2[1] == w2c &&
            o->b3[0] == b3r && o->b3[1] == b3c) {
            if (out_idx) *out_idx = i;
            return true;
        }
    }
    if (out_idx) *out_idx = -1;
    return false;
}

int opening_choose_by_black_strategy(void) {
    /* 极简策略：固定选 0 号 "寒星"。
     * 选 0 而非 "花月"(idx=3) 的理由：寒星偏稳健，对引擎实力要求最低。
     * 后续可改成：枚举所有 26 种，调 search 评分 → 选黑方期望分最高的。
     */
    return 0;
}

Move opening_first_move(void) {
    Move m = { 7, 7, BLACK };
    return m;
}

bool opening_should_swap(const Board *b) {
    (void)b;
    return false;
}

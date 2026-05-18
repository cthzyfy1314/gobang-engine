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
 * --- 名称-坐标的"规范映射" ---
 * 名称取自 RIF (Renju International Federation) 官方 26 开局表 + 中国连珠协会
 * 教材中文译名。本表 idx 顺序对应 RenjuNet /openings/ 上的 D1..D13 与 I1..I13
 * 编号 (Direct 1..13, Indirect 1..13)，参考维基百科 commons:Renju_vertical_openings-.png
 * 与 commons:Renju_diagonal_openings-.png 中的标号 (1..13)。
 *
 * 命名规则（黑1 与黑3 关系，与"星/月"分类一致）：
 *   "星" = 间打：B1 与 B3 在同一直线 / 斜线上，且中间隔 1 格。
 *   "月-连" = 连打：B1 与 B3 紧邻 (8 邻接)。
 *   "月-桂" = 桂马打：B1 与 B3 成马步 (日字形)。
 * 直指 13 = 5 星 + 4 连 + 4 桂；斜指 13 = 5 星 + 4 连 + 4 桂。
 *
 * 注：26 个 B3 坐标 (相对 H8) 与 RIF 完全一致；本表 idx → 名称的对应也已校准
 * 至 RIF 编号顺序，故"engine 报"花月""与"对手报"花月""指代的就是同一珠形。
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
     * idx 顺序对应几何扫描 (列 7→8→9, 每列内 row 5→9), 名称按"该 B3 坐标
     * 在 RIF 编号 (D1..D13) 中的对应名"。
     */
    /*  0 (D1)  */ { "寒星", "Kansei",    B1_H8, W2_H9, { 5, 7 }, true },  /* B3=H10  间 (W2 正北一步) */
    /*  1 (D8)  */ { "松月", "Shogetsu",  B1_H8, W2_H9, { 8, 7 }, true },  /* B3=H7   连 (B1 正南邻) */
    /*  2 (D11) */ { "瑞星", "Zuisei",    B1_H8, W2_H9, { 9, 7 }, true },  /* B3=H6   间 (B1 正南两步) */
    /*  3 (D2)  */ { "溪月", "Keigetsu",  B1_H8, W2_H9, { 5, 8 }, true },  /* B3=I10  桂 (B1 东北马步) */
    /*  4 (D4)  */ { "花月", "Kagetsu",   B1_H8, W2_H9, { 6, 8 }, true },  /* B3=I9   连 (B1 东北邻) */
    /*  5 (D6)  */ { "雨月", "Ugetsu",    B1_H8, W2_H9, { 7, 8 }, true },  /* B3=I8   连 (B1 正东邻) */
    /*  6 (D9)  */ { "丘月", "Kyugetsu",  B1_H8, W2_H9, { 8, 8 }, true },  /* B3=I7   连 (B1 东南邻) */
    /*  7 (D12) */ { "山月", "Sangetsu",  B1_H8, W2_H9, { 9, 8 }, true },  /* B3=I6   桂 (B1 东南马步) */
    /*  8 (D3)  */ { "疏星", "Sosei",     B1_H8, W2_H9, { 5, 9 }, true },  /* B3=J10  间 (B1 东北两步) */
    /*  9 (D5)  */ { "残月", "Zangetsu",  B1_H8, W2_H9, { 6, 9 }, true },  /* B3=J9   桂 (B1 东北远马步) */
    /* 10 (D7)  */ { "金星", "Kinsei",    B1_H8, W2_H9, { 7, 9 }, true },  /* B3=J8   间 (B1 正东两步) */
    /* 11 (D10) */ { "新月", "Shingetsu", B1_H8, W2_H9, { 8, 9 }, true },  /* B3=J7   桂 (B1 东南远马步) */
    /* 12 (D13) */ { "游星", "Yusei",     B1_H8, W2_H9, { 9, 9 }, true },  /* B3=J6   间 (B1 东南两步) */

    /* === 斜指 (W2 = I9) — 13 种 ===
     * idx 顺序对应几何扫描 (按 row+col 升序, 同和值内 col 升序), 名称按
     * "该 B3 坐标在 RIF 编号 (I1..I13) 中的对应名"。
     */
    /* 13 (I1)  */ { "长星", "Chosei",    B1_H8, W2_I9, { 5, 9 }, false }, /* B3=J10  间 (B1 东北两步) */
    /* 14 (I2)  */ { "峡月", "Kyogetsu",  B1_H8, W2_I9, { 6, 9 }, false }, /* B3=J9   桂 (W2 正东; B1 东北马步) */
    /* 15 (I6)  */ { "云月", "Ungetsu",   B1_H8, W2_I9, { 7, 8 }, false }, /* B3=I8   连 (B1 正东邻) */
    /* 16 (I3)  */ { "恒星", "Kosei",     B1_H8, W2_I9, { 7, 9 }, false }, /* B3=J8   间 (B1 正东两步) */
    /* 17 (I11) */ { "斜月", "Shagetsu",  B1_H8, W2_I9, { 8, 6 }, false }, /* B3=G7   连 (B1 西南邻; 对称轴外侧) */
    /* 18 (I9)  */ { "银月", "Gingetsu",  B1_H8, W2_I9, { 8, 7 }, false }, /* B3=H7   连 (B1 正南邻) */
    /* 19 (I7)  */ { "浦月", "Hogetsu",   B1_H8, W2_I9, { 8, 8 }, false }, /* B3=I7   连 (B1 东南邻, 对称轴) */
    /* 20 (I4)  */ { "水月", "Suigetsu",  B1_H8, W2_I9, { 8, 9 }, false }, /* B3=J7   桂 (B1 东南马步) */
    /* 21 (I13) */ { "彗星", "Suisei",    B1_H8, W2_I9, { 9, 5 }, false }, /* B3=F6   间 (B1 西南两步) */
    /* 22 (I12) */ { "名月", "Meigetsu",  B1_H8, W2_I9, { 9, 6 }, false }, /* B3=G6   桂 (B1 西南远马步) */
    /* 23 (I10) */ { "明星", "Myojo",     B1_H8, W2_I9, { 9, 7 }, false }, /* B3=H6   间 (B1 正南两步) */
    /* 24 (I8)  */ { "岚月", "Rangetsu",  B1_H8, W2_I9, { 9, 8 }, false }, /* B3=I6   桂 (B1 东南远马步) */
    /* 25 (I5)  */ { "流星", "Ryusei",    B1_H8, W2_I9, { 9, 9 }, false }, /* B3=J6   间 (B1 东南两步) */
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
    /* 简单"轮转"策略：在 5 个广泛研究、平衡或对黑方有利的珠形中按
     * 当局 (B1, W2, B3) 三手发出前的全局调用次数循环取一个。
     *
     * Pool (idx → name)：
     *   0  寒星 (Kansei, D1)   — 间, 黑方有利, 古典稳健
     *   4  花月 (Kagetsu, D4)  — 连, 黑必胜
     *   8  疏星 (Sosei, D3)    — 间, 平衡, 进攻空间大
     *  13  长星 (Chosei, I1)   — 间, 持白略优, 多变化
     *  19  浦月 (Hogetsu, I7)  — 连, 黑必胜
     *
     * 设计意图（修 #32）：避免引擎每局都开"寒星"被对手提前准备。引擎实力
     * 不来自珠形本身——5 个候选中即使个别对黑方稍差，整体搜索深度仍能补偿。
     * 用 static 计数器而非 time() 是为了在同一 process 内可复现 (测试友好)。
     */
    static const int pool[] = { 0, 4, 8, 13, 19 };
    static int call_idx = 0;
    int picked = pool[call_idx % (int)(sizeof pool / sizeof pool[0])];
    call_idx++;
    return picked;
}

Move opening_first_move(void) {
    Move m = { 7, 7, BLACK };
    return m;
}

bool opening_should_swap(const Board *b) {
    (void)b;
    return false;
}

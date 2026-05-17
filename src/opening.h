/* src/opening.h — 国规 26 种指定开局库
 *
 * 国规规定 26 种指定开局（珠形）+ 三手交换 + 五手 N 打。
 * 每种开局由 B1+W2+B3 三手坐标定义；B1 永远是 H8 (天元)。
 *
 * 13 种"直指" (W2 = H9, 即与 B1 垂直对齐) + 13 种"斜指" (W2 = I9, 即与 B1 斜向相邻)。
 *
 * 坐标约定（与 board.h / ui.c 一致）：
 *   row 范围 0..14，col 范围 0..14。
 *   H8 对应 row=7, col=7。
 *   显示坐标 字母 = 'A' + col，数字 = BOARD_SIZE - row = 15 - row。
 *   即 (row=7, col=7) → "H8"，(row=6, col=7) → "H9"，(row=6, col=8) → "I9"。
 *
 * 名称对应（中→日罗马字）参考 RIF (Renju International Federation) 26 种规范开局表，
 * 命名与中国连珠协会发布的开局教材一致。
 */
#ifndef OPENING_H_
#define OPENING_H_

#include "board.h"

#define OPENING_COUNT 26

typedef struct {
    const char *name_cn;     /* 中文名，如 "花月" */
    const char *name_jp;     /* 罗马字日文名，如 "Kagetsu" */
    int b1[2];               /* {row, col} —— 永远 {7, 7} (H8) */
    int w2[2];               /* {row, col} —— 直指 = {6,7} (H9)；斜指 = {6,8} (I9) */
    int b3[2];               /* {row, col} —— 因开局而异 */
    bool is_direct;          /* true = 直指 (W2 = H9)；false = 斜指 (W2 = I9) */
} RenjuOpening;

/* 26 种指定开局数据（按 直指 1-13 + 斜指 1-13 顺序）。索引 0..25。 */
extern const RenjuOpening RENJU_OPENINGS[OPENING_COUNT];

/* 返回开局总数（恒为 26）。 */
int opening_count(void);

/* 按编号 (0..25) 取开局。索引越界返回 NULL。 */
const RenjuOpening *opening_get(int idx);

/* 给定 3 个坐标判断是否为 26 种之一。
 * 匹配返回 true 且 *out_idx 填入索引；否则返回 false。out_idx 可为 NULL。
 */
bool opening_matches(int b1r, int b1c, int w2r, int w2c, int b3r, int b3c, int *out_idx);

/* 引擎执黑时的开局选择策略（极简版）。
 * 当前固定返回 0（花月之前的第一种：寒星）。
 * 未来可加：基于深度评估挑最优开局。
 */
int opening_choose_by_black_strategy(void);

/* 黑方第一手位置 — 国规要求天元 H8 = (7,7)。保留旧 API。 */
Move opening_first_move(void);

/* 三手交换决策（白方第 4 手前判断是否换边）。
 * 当前简化：永远返回 false（不交换）。保留旧 API。
 */
bool opening_should_swap(const Board *b);

#endif /* OPENING_H_ */

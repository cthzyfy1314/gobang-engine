/* src/pattern.h — 棋型识别 + 评估函数
 * 对应 Spec § 3.2 棋型评估 / § 4.1 pattern 模块
 *
 * 简化版（7 棋型 + NONE）：
 *   只识别连续段，不识别跳子（_XX_X_、_X_XXX_）。
 *   跳活三 / 真活三的精确判定留给 forbid 模块（5/3-5/5）。
 */
#ifndef PATTERN_H_
#define PATTERN_H_

#include <stdint.h>
#include "board.h"

typedef enum {
    PAT_NONE             = 0,
    PAT_SLEEP_TWO        = 1,   /* 眠二：一端被堵的 2 连 */
    PAT_OPEN_TWO         = 2,   /* 活二：双端开放的 2 连 */
    PAT_SLEEP_THREE      = 3,   /* 眠三：一端被堵的 3 连 */
    PAT_OPEN_THREE       = 4,   /* 活三：双端开放的 3 连 */
    PAT_SIMPLE_FOUR      = 5,   /* 冲四：一端被堵的 4 连 */
    PAT_OPEN_FOUR        = 6,   /* 活四：双端开放的 4 连 */
    PAT_FIVE             = 7,   /* 五连（含 ≥6 长连）*/
    PAT_BROKEN_FOUR      = 8,   /* 跳冲四：5 窗内 4 同色 + 1 空（空不在边）→ 填缝成 5 */
    PAT_JUMP_OPEN_THREE  = 9,   /* 跳活三：_XX_X_ / _X_XX_ → 下一手成活四 */
    PAT_JUMP_OPEN_FOUR   = 10,  /* 跳活四：_X_XXX_ / _XX_XX_ / _XXX_X_ → 填缝成五连 */
    PAT_COUNT            = 11
} Pattern;

/* 评分表（spec § 3.2）。索引由 Pattern 枚举决定。 */
extern const int PATTERN_SCORE[PAT_COUNT];

/* 棋型计数容器：counts[p] = 棋型 p 在当前局面出现的次数 */
typedef struct {
    int counts[PAT_COUNT];
} PatternStats;

/* 在 1D 线 line[0..len-1] 上统计 color 的棋型计数，累加到 out。
 * line 元素：0=EMPTY, 1=BLACK, 2=WHITE。
 * 边界视为"非 color"（不开放）。
 */
void pattern_count_in_line(const int8_t *line, int len, int color, PatternStats *out);

/* 在整盘上统计 color 的棋型计数（4 方向：横/竖/主斜/副斜）。
 * out 在调用前应清零（{0}）。
 */
void pattern_count_for_color(const Board *b, int color, PatternStats *out);

/* 整盘评估：黑总分 − 白总分，再按 side_to_move 视角翻转。
 * 返回值 > 0 表示对 side_to_move 有利。
 */
int pattern_evaluate(const Board *b);

#endif /* PATTERN_H_ */

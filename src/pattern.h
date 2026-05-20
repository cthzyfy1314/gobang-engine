/* src/pattern.h — 棋型识别 + 手工评估函数
 * 对应 Spec § 3.2 棋型评估 / § 4.1 pattern 模块
 *
 * 13 类棋型（含 NONE）：连续段（活/眠 二~五）+ 跳子型（跳冲四 / 跳活三 / 跳活四 /
 * 原活三 / 眠跳四，见下方枚举）。真活三 / 长连禁手的精确判定仍归 forbid 模块。
 *
 * ⚠️ 已知不完美（Phase 2 NNUE 路线下属 throwaway，刻意不修）：
 *   跳子型的 dedup 在某些分裂型上会重复计数（如 _XX_XX_），eval 量级偏大；且 dedup
 *   作用于全局累加器、跨方向/跨线共享。这些是 eval 调优问题，不影响合法性/胜负判定。
 *   整个 pattern_evaluate 将被 NNUE（nn/）替换，故不在此投入修复。
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
    PAT_PROTO_THREE      = 11,  /* 原活三：双端开放 2 连且至少一端可扩展到 5（__XX_ / _XX__）→ 一手成活三 */
    PAT_SLEEP_JUMP_FOUR  = 12,  /* 眠跳四：X_XXX / XXX_X 一端被堵（边界或对方子）→ 填缝仍可成 5 */
    PAT_COUNT            = 13
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

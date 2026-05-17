/* src/search.h — αβ minimax 搜索框架（v2）
 * 对应 Spec § 4.1 search 模块 / § 3.1 主算法
 *
 * v2 改动：
 *   - SEARCH_INF 提到 1e8，与 pattern eval 量级分离；新增 IS_MATE_SCORE
 *   - search_best_move 增加 time_budget_ms 形参（向后兼容包装）
 *   - 新增 VCF / VCT 专用搜索 (search_vcf, search_vct)
 *   - TT 跨调用复用（不再每次 tt_clear）
 *   - 邻近候选扩展为"2 圈 + 同方向 4 cell 延伸"
 *   - tactical move ordering（threat-create / threat-defend）
 *   - ID 引入 aspiration window
 */
#ifndef SEARCH_H_
#define SEARCH_H_

#include "board.h"

#define SEARCH_INF          100000000        /* 1e8：远高于 pattern eval 累积量级 */
#define SEARCH_MATE_MARGIN  10000            /* mate score buffer */
#define IS_MATE_SCORE(s)    ((s) >= SEARCH_INF - SEARCH_MATE_MARGIN || \
                             (s) <= -(SEARCH_INF - SEARCH_MATE_MARGIN))

#define SEARCH_MAX_MOVES    256              /* 一次 generate 最多返回的候选数 */
#define SEARCH_MAX_PLY      64               /* killer/history 数组维度（VCF 可深） */

#define SEARCH_DEFAULT_TIME_MS 1000          /* 默认 1 秒预算 */

typedef struct {
    Move best_move;
    int  score;
    long nodes_searched;
} SearchResult;

/* 找当前 side_to_move 的最佳着子。
 * max_depth: 上限深度；ID 会从 1 逐层加到 max_depth
 * time_budget_ms: 时间预算（毫秒）；<=0 表示不限时
 * 返回 best_move 和 score（从 side_to_move 视角）。
 */
SearchResult search_best_move_timed(Board *b, int max_depth, int time_budget_ms);

/* 老接口：等价于 search_best_move_timed(b, depth, SEARCH_DEFAULT_TIME_MS) */
SearchResult search_best_move(Board *b, int depth);

/* VCF 专用搜索：仅扩展冲四/活四/连五，分支因子小可搜深度 20+
 * 找到时 out_seq 填入第一步着法，返回 1；否则返回 0。
 */
int search_vcf(Board *b, int max_depth, Move *out_first);

/* VCT 专用搜索：扩展所有 forcing moves（四威胁 + 活三威胁）
 * 找到时 out_first 填入第一步着法，返回 1；否则返回 0。
 */
int search_vct(Board *b, int max_depth, Move *out_first);

/* Placeholder 评估函数（4/30 起转发 pattern_evaluate）。
 * 返回值 > 0 表示对 side_to_move 有利。
 */
int search_evaluate(const Board *b);

/* 生成候选着子：邻近 2 圈 + 沿 4 个方向各延伸 4 cell。
 * out 数组容量至少 SEARCH_MAX_MOVES。
 * 返回实际候选数。
 * 棋盘空时只返回中心点 (7,7)。
 */
int search_generate_neighbor_moves(const Board *b, Move *out);

/* 重置搜索状态（清 TT + killer + history + tt_generation）。新游戏开始时调用。 */
void search_reset(void);

/* 五手 N 打（国规 7） */
int search_find_n_distinct(Board *b, int depth, int n_want, Move *out, int *out_scores);

#endif /* SEARCH_H_ */

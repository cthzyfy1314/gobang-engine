/* src/search.h — αβ minimax 搜索框架
 * 对应 Spec § 4.1 search 模块 / § 3.1 主算法
 * Day 1 状态：评估函数是 placeholder（中央距离倾向）
 *             pattern 模块完成后 (4/30) 会替换 evaluate
 */
#ifndef SEARCH_H_
#define SEARCH_H_

#include "board.h"

#define SEARCH_INF      1000000
#define SEARCH_MAX_MOVES 256       /* 一次 generate 最多返回的候选数 */
#define SEARCH_MAX_PLY  32         /* killer/history 数组维度 */

typedef struct {
    Move best_move;
    int  score;
    long nodes_searched;
} SearchResult;

/* 找当前 side_to_move 的最佳着子，搜索深度 depth 层。
 * 返回 best_move 和 score（从 side_to_move 视角）。
 */
SearchResult search_best_move(Board *b, int depth);

/* Placeholder 评估函数。
 * Day 1: 中央距离倾向（越靠中心分越高）+ 简单子力差
 * 4/30 会被 pattern 模块完整替换。
 * 评估视角：返回值 > 0 表示对 side_to_move 有利。
 */
int search_evaluate(const Board *b);

/* 生成"邻近 2 圈"候选着子。
 * out 数组容量至少 SEARCH_MAX_MOVES。
 * 返回实际候选数。
 * 如果棋盘空，只返回中心点 (7,7)。
 */
int search_generate_neighbor_moves(const Board *b, Move *out);

/* 重置搜索状态（清 TT + killer + history）。新游戏开始时调用。 */
void search_reset(void);

#endif /* SEARCH_H_ */

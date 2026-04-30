/* src/search.c */
#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <limits.h>
#include "search.h"
#include "board.h"
#include "pattern.h"
#include "zobrist.h"

/* 4/30 起：转发给 pattern_evaluate（替换 4/29 的中央倾向 placeholder） */
int search_evaluate(const Board *b) {
    return pattern_evaluate(b);
}

int search_generate_neighbor_moves(const Board *b, Move *out) {
    if (b->move_count == 0) {
        out[0].row = 7;
        out[0].col = 7;
        out[0].color = (int8_t)b->side_to_move;
        return 1;
    }

    /* "邻近 2 圈"标记表 */
    uint8_t mark[BOARD_SIZE][BOARD_SIZE] = {{0}};

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (b->cells[r][c] == EMPTY) continue;
            for (int dr = -2; dr <= 2; dr++) {
                for (int dc = -2; dc <= 2; dc++) {
                    int nr = r + dr, nc = c + dc;
                    if (!board_in_bounds(nr, nc)) continue;
                    if (b->cells[nr][nc] != EMPTY) continue;
                    mark[nr][nc] = 1;
                }
            }
        }
    }

    int count = 0;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (mark[r][c] && count < SEARCH_MAX_MOVES) {
                out[count].row = (int8_t)r;
                out[count].col = (int8_t)c;
                out[count].color = (int8_t)b->side_to_move;
                count++;
            }
        }
    }
    return count;
}

/* αβ minimax 递归（negamax 风格简化版）。
 * 返回值是从"当前到走方"角度的评分。
 *   side_to_move = 接下来要走的玩家
 *   alpha/beta = 当前最佳可行下界/上界
 *   depth_left = 还要往深搜几层
 *   nodes = 节点计数累加器
 */
static int alphabeta(Board *b, int depth_left, int alpha, int beta, long *nodes) {
    (*nodes)++;

    /* TT 查询 */
    const TTEntry *e = tt_get(b->zobrist_hash);
    if (e != NULL && e->depth >= depth_left) {
        if (e->flag == TT_FLAG_EXACT) return e->score;
        if (e->flag == TT_FLAG_LOWER && e->score >= beta)  return e->score;
        if (e->flag == TT_FLAG_UPPER && e->score <= alpha) return e->score;
    }

    /* 终局检查（注意：此时 side_to_move 已经是"对手"，因为上一手刚落完） */
    GameResult res = board_check_winner(b);
    if (res == RESULT_BLACK_WIN) {
        return (b->side_to_move == BLACK) ? SEARCH_INF - b->move_count : -(SEARCH_INF - b->move_count);
    }
    if (res == RESULT_WHITE_WIN_NORMAL || res == RESULT_WHITE_WIN_BY_BLACK_FORBID) {
        return (b->side_to_move == WHITE) ? SEARCH_INF - b->move_count : -(SEARCH_INF - b->move_count);
    }
    if (res == RESULT_DRAW) return 0;

    if (depth_left == 0) return search_evaluate(b);

    Move moves[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(b, moves);
    if (n == 0) return search_evaluate(b);

    int orig_alpha = alpha;
    int best_score = -SEARCH_INF;
    int best_r = -1, best_c = -1;
    for (int i = 0; i < n; i++) {
        int color = b->side_to_move;
        if (!board_place(b, moves[i].row, moves[i].col, color)) continue;
        int score = -alphabeta(b, depth_left - 1, -beta, -alpha, nodes);
        board_undo(b);

        if (score > best_score) {
            best_score = score;
            best_r = moves[i].row;
            best_c = moves[i].col;
        }
        if (best_score > alpha) alpha = best_score;
        if (alpha >= beta) break;  /* β 剪枝 */
    }

    /* TT 写入：根据 alpha/beta 边界判定 flag */
    TTFlag flag;
    if (best_score <= orig_alpha)      flag = TT_FLAG_UPPER;
    else if (best_score >= beta)       flag = TT_FLAG_LOWER;
    else                                flag = TT_FLAG_EXACT;
    tt_put(b->zobrist_hash, depth_left, best_score, flag, best_r, best_c);

    return best_score;
}

SearchResult search_best_move(Board *b, int depth) {
    SearchResult result = { .best_move = { -1, -1, (int8_t)b->side_to_move },
                            .score = -SEARCH_INF, .nodes_searched = 0 };

    /* 每次新搜索清表（5/2 加迭代加深后改为只在新游戏清）*/
    tt_clear();

    Move moves[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(b, moves);
    if (n == 0) return result;

    int alpha = -SEARCH_INF, beta = SEARCH_INF;
    for (int i = 0; i < n; i++) {
        int color = b->side_to_move;
        if (!board_place(b, moves[i].row, moves[i].col, color)) continue;
        int score = -alphabeta(b, depth - 1, -beta, -alpha, &result.nodes_searched);
        board_undo(b);

        if (score > result.score) {
            result.score = score;
            result.best_move = moves[i];
        }
        if (score > alpha) alpha = score;
    }
    return result;
}

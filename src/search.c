/* src/search.c */
#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <limits.h>
#include "search.h"
#include "board.h"

/* Day 1 placeholder evaluator:
 *   "中央倾向"——离 (7,7) 越近的子值越高。
 *   评估返回值是从 side_to_move 视角。
 *   对每个棋子计算 (7-|7-r|) + (7-|7-c|) 的中央距离值。
 */
int search_evaluate(const Board *b) {
    int black_value = 0, white_value = 0;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            uint8_t cell = b->cells[r][c];
            if (cell == EMPTY) continue;
            int dr = r - 7; if (dr < 0) dr = -dr;
            int dc = c - 7; if (dc < 0) dc = -dc;
            int center_score = (7 - dr) + (7 - dc);
            if (cell == BLACK) black_value += center_score;
            else white_value += center_score;
        }
    }
    int diff = black_value - white_value;
    return (b->side_to_move == BLACK) ? diff : -diff;
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

    int best = -SEARCH_INF;
    for (int i = 0; i < n; i++) {
        int color = b->side_to_move;
        if (!board_place(b, moves[i].row, moves[i].col, color)) continue;
        int score = -alphabeta(b, depth_left - 1, -beta, -alpha, nodes);
        board_undo(b);

        if (score > best) best = score;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;  /* β 剪枝 */
    }
    return best;
}

SearchResult search_best_move(Board *b, int depth) {
    SearchResult result = { .best_move = { -1, -1, (int8_t)b->side_to_move },
                            .score = -SEARCH_INF, .nodes_searched = 0 };

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

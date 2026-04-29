/* src/search.c */
#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include "search.h"

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

SearchResult search_best_move(Board *b, int depth) {
    (void)depth;
    SearchResult r = { .best_move = { -1, -1, (int8_t)b->side_to_move },
                       .score = 0, .nodes_searched = 0 };
    return r;
}

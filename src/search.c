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

/* 占位实现，后续 task 完整实现 */
int search_generate_neighbor_moves(const Board *b, Move *out) {
    (void)b; (void)out;
    return 0;
}

SearchResult search_best_move(Board *b, int depth) {
    (void)depth;
    SearchResult r = { .best_move = { -1, -1, (int8_t)b->side_to_move },
                       .score = 0, .nodes_searched = 0 };
    return r;
}

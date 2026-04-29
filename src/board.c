/* src/board.c */
#define _CRT_SECURE_NO_WARNINGS
#include <string.h>
#include "board.h"

void board_init(Board *b) {
    memset(b->cells, EMPTY, sizeof(b->cells));
    b->move_count = 0;
    b->zobrist_hash = 0;
    b->side_to_move = BLACK;
    b->forbid_enabled = true;
    /* history 不必清零，靠 move_count 控制有效范围 */
}

bool board_in_bounds(int row, int col) {
    return row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE;
}

/* 占位实现，让 link 通过。后续 Task 会完整实现。 */
bool board_place(Board *b, int row, int col, int color) {
    (void)b; (void)row; (void)col; (void)color;
    return false;
}

bool board_undo(Board *b) {
    (void)b;
    return false;
}

GameResult board_check_winner(const Board *b) {
    (void)b;
    return RESULT_NONE;
}

bool board_is_full(const Board *b) {
    (void)b;
    return false;
}

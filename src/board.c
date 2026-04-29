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

bool board_place(Board *b, int row, int col, int color) {
    if (!board_in_bounds(row, col)) return false;
    if (b->cells[row][col] != EMPTY) return false;
    if (color != BLACK && color != WHITE) return false;

    b->cells[row][col] = (uint8_t)color;
    b->history[b->move_count].row = (int8_t)row;
    b->history[b->move_count].col = (int8_t)col;
    b->history[b->move_count].color = (int8_t)color;
    b->move_count++;
    b->side_to_move = (color == BLACK) ? WHITE : BLACK;
    return true;
}

bool board_undo(Board *b) {
    if (b->move_count == 0) return false;
    b->move_count--;
    Move *m = &b->history[b->move_count];
    b->cells[m->row][m->col] = EMPTY;
    b->side_to_move = m->color;   /* 撤销后下一个轮到的还是当时落子那方 */
    return true;
}

GameResult board_check_winner(const Board *b) {
    (void)b;
    return RESULT_NONE;
}

bool board_is_full(const Board *b) {
    (void)b;
    return false;
}

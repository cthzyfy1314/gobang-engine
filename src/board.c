/* src/board.c */
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <string.h>
#include "board.h"
#include "zobrist.h"

void board_init(Board *b) {
    zobrist_init();   /* idempotent; ensures Z_KEY/Z_SIDE_KEY are populated
                       * before any board operations rely on incremental hash. */
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
    /* Out-of-turn placement would desync incremental zobrist from
     * compute() — same hash invariant broken silently. Assertion
     * makes the precondition explicit. Disabled in NDEBUG release
     * builds (zero cost). */
    assert(color == b->side_to_move);

    if (!board_in_bounds(row, col)) return false;
    if (b->cells[row][col] != EMPTY) return false;
    if (color != BLACK && color != WHITE) return false;
    if (b->move_count >= BOARD_SIZE * BOARD_SIZE) return false;  /* board full */

    b->cells[row][col] = (uint8_t)color;
    b->history[b->move_count].row = (int8_t)row;
    b->history[b->move_count].col = (int8_t)col;
    b->history[b->move_count].color = (int8_t)color;
    b->move_count++;
    b->side_to_move = (color == BLACK) ? WHITE : BLACK;

    /* Zobrist 增量更新 */
    b->zobrist_hash = zobrist_xor_piece(b->zobrist_hash, row, col, color);
    b->zobrist_hash = zobrist_xor_side(b->zobrist_hash);
    return true;
}

bool board_undo(Board *b) {
    if (b->move_count == 0) return false;
    b->move_count--;
    Move *m = &b->history[b->move_count];

    /* Zobrist 增量更新（XOR 自反） */
    b->zobrist_hash = zobrist_xor_piece(b->zobrist_hash, m->row, m->col, m->color);
    b->zobrist_hash = zobrist_xor_side(b->zobrist_hash);

    b->cells[m->row][m->col] = EMPTY;
    b->side_to_move = m->color;   /* 撤销后下一个轮到的还是当时落子那方 */
    return true;
}

/* 给定起点 (r,c) 和方向 (dr,dc)，统计沿该方向 color 的最长连续子数。
 * 包括起点本身（如果起点是 color）。
 */
static int count_run(const Board *b, int r, int c, int dr, int dc, int color) {
    int count = 0;
    while (board_in_bounds(r, c) && b->cells[r][c] == (uint8_t)color) {
        count++;
        r += dr;
        c += dc;
    }
    return count;
}

/* 对每个 cells[r][c]==color 的格子，沿 4 方向扫描所有 maximal run。
 * 只从一段的"起点"开始计数（前一格不是同色），避免重复。
 * 输出：
 *   *out_max_run        — 该 color 所有 run 中的最大值（0 表示无该色子）
 *   *out_has_exact_five — 是否存在某 maximal run 恰好等于 5
 *
 * 关键：必须用 maximal run（前一格非同色 + 沿方向扫到非同色为止），
 * 才能把"6 连"识别为长度 6 而不是误认为"包含一个长度 5 的子段"。
 * 这是国规 9.2-c 判定的几何基础——"存在恰好 5 连"必须是真正独立的 5 连段。
 */
static void runs_for_color(const Board *b, int color,
                           int *out_max_run, int *out_has_exact_five) {
    int max_run = 0;
    int has_five = 0;
    static const int dirs[4][2] = { {0,1}, {1,0}, {1,1}, {1,-1} };
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (b->cells[r][c] != (uint8_t)color) continue;
            for (int d = 0; d < 4; d++) {
                int dr = dirs[d][0], dc = dirs[d][1];
                int prev_r = r - dr, prev_c = c - dc;
                if (board_in_bounds(prev_r, prev_c) &&
                    b->cells[prev_r][prev_c] == (uint8_t)color) {
                    continue;  /* 不是 maximal run 的起点 */
                }
                int run = count_run(b, r, c, dr, dc, color);
                if (run > max_run) max_run = run;
                if (run == 5) has_five = 1;
            }
        }
    }
    if (out_max_run)        *out_max_run        = max_run;
    if (out_has_exact_five) *out_has_exact_five = has_five;
}

GameResult board_check_winner(const Board *b) {
    int black_max = 0, black_has_5 = 0;
    int white_max = 0;
    runs_for_color(b, BLACK, &black_max, &black_has_5);
    runs_for_color(b, WHITE, &white_max, NULL);

    /* 国规不对称（Spec § 1.4 国规 9.1 + 9.2-c）：
     *   白 ≥5 连（含长连）→ 白胜（白长连视同五连）
     *   黑 存在某方向恰好 5 连 → 黑胜（9.2-c：五连优先，即便另一方向同时形成长连）
     *   黑 ≥6 连且无任何方向恰好 5 连 → 白胜（长连禁手）
     *   全盘满 → 和棋
     */
    if (white_max >= 5) return RESULT_WHITE_WIN_NORMAL;
    if (black_has_5)    return RESULT_BLACK_WIN;
    if (black_max >= 6) return RESULT_WHITE_WIN_NORMAL;
    if (board_is_full(b)) return RESULT_DRAW;
    return RESULT_NONE;
}

bool board_is_full(const Board *b) {
    return b->move_count >= (uint16_t)(BOARD_SIZE * BOARD_SIZE);
}

/* tests/test_board.c */
#include <string.h>
#include "test_runner.h"
#include "../src/board.h"
#include "../src/zobrist.h"

static void test_board_init(void) {
    Board b;
    board_init(&b);
    ASSERT_EQ(b.move_count, 0, "init: move_count == 0");
    ASSERT_EQ(b.side_to_move, BLACK, "init: black to move first");
    ASSERT_EQ(b.zobrist_hash, 0, "init: zobrist 0 (placeholder)");
    ASSERT_TRUE(b.forbid_enabled, "init: forbid_enabled = true by default");
    ASSERT_EQ(b.cells[7][7], EMPTY, "init: center cell empty");
    ASSERT_EQ(b.cells[0][0], EMPTY, "init: corner cell empty");
}

static void test_board_in_bounds(void) {
    ASSERT_TRUE(board_in_bounds(0, 0), "bounds: (0,0) ok");
    ASSERT_TRUE(board_in_bounds(14, 14), "bounds: (14,14) ok");
    ASSERT_TRUE(board_in_bounds(7, 7), "bounds: (7,7) ok");
    ASSERT_FALSE(board_in_bounds(-1, 0), "bounds: (-1,0) out");
    ASSERT_FALSE(board_in_bounds(0, -1), "bounds: (0,-1) out");
    ASSERT_FALSE(board_in_bounds(15, 0), "bounds: (15,0) out");
    ASSERT_FALSE(board_in_bounds(0, 15), "bounds: (0,15) out");
}

static void test_board_place_basic(void) {
    Board b; board_init(&b);
    ASSERT_TRUE(board_place(&b, 7, 7, BLACK), "place: black at (7,7) ok");
    ASSERT_EQ(b.cells[7][7], BLACK, "place: cell[7][7] is BLACK");
    ASSERT_EQ(b.move_count, 1, "place: move_count = 1");
    ASSERT_EQ(b.side_to_move, WHITE, "place: side flipped to WHITE");
}

static void test_board_place_invalid(void) {
    Board b; board_init(&b);
    board_place(&b, 7, 7, BLACK);
    ASSERT_FALSE(board_place(&b, 7, 7, WHITE), "place: occupied cell rejected");
    ASSERT_FALSE(board_place(&b, -1, 0, WHITE), "place: out-of-bounds rejected");
    ASSERT_FALSE(board_place(&b, 15, 0, WHITE), "place: out-of-bounds (15) rejected");
    ASSERT_EQ(b.move_count, 1, "place: invalid moves don't increment");
}

static void test_board_undo(void) {
    Board b; board_init(&b);
    board_place(&b, 7, 7, BLACK);
    board_place(&b, 7, 8, WHITE);
    ASSERT_EQ(b.move_count, 2, "undo: pre-state move_count = 2");
    ASSERT_TRUE(board_undo(&b), "undo: returns true");
    ASSERT_EQ(b.cells[7][8], EMPTY, "undo: (7,8) cleared");
    ASSERT_EQ(b.cells[7][7], BLACK, "undo: (7,7) preserved");
    ASSERT_EQ(b.move_count, 1, "undo: move_count = 1");
    ASSERT_EQ(b.side_to_move, WHITE, "undo: side back to WHITE (next mover)");
    ASSERT_TRUE(board_undo(&b), "undo: again ok");
    ASSERT_EQ(b.move_count, 0, "undo: empty board");
    ASSERT_FALSE(board_undo(&b), "undo: empty stack returns false");
}

static void test_board_place_undo_idempotent(void) {
    /* place 后立即 undo 应该回到初始状态 */
    Board b; board_init(&b);
    Board snap = b;
    board_place(&b, 5, 5, BLACK);
    board_undo(&b);
    ASSERT_EQ(memcmp(b.cells, snap.cells, sizeof(b.cells)), 0,
              "round-trip: cells identical");
    ASSERT_EQ(b.move_count, snap.move_count, "round-trip: move_count");
    ASSERT_EQ(b.side_to_move, snap.side_to_move, "round-trip: side");
}

static void test_board_is_full(void) {
    Board b; board_init(&b);
    ASSERT_FALSE(board_is_full(&b), "is_full: empty board not full");

    /* 填满整个棋盘（225 格） */
    int color = BLACK;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            board_place(&b, r, c, color);
            color = (color == BLACK) ? WHITE : BLACK;
        }
    }
    ASSERT_TRUE(board_is_full(&b), "is_full: 225 stones placed");
    ASSERT_EQ(b.move_count, 225, "is_full: move_count == 225");
}

/* 辅助：水平连续放 count 个 color 棋子，从 (start_r, start_c) 开始。
 * 直接改 cells，避开 place 的 turn-switching 逻辑，方便构造任意局面。
 */
static void place_horizontal_run(Board *b, int start_r, int start_c, int count, int color) {
    for (int i = 0; i < count; i++) {
        b->cells[start_r][start_c + i] = (uint8_t)color;
    }
    b->move_count += (uint16_t)count;
}

static void test_winner_none_on_empty(void) {
    Board b; board_init(&b);
    ASSERT_EQ(board_check_winner(&b), RESULT_NONE, "winner: empty board = none");
}

static void test_winner_black_5_horizontal(void) {
    Board b; board_init(&b);
    place_horizontal_run(&b, 7, 5, 5, BLACK);
    ASSERT_EQ(board_check_winner(&b), RESULT_BLACK_WIN, "winner: black 5-in-row = BLACK_WIN");
}

static void test_winner_black_6_overline_negative(void) {
    /* 国规 8 + 9.1：黑 6 连（长连）= 黑负 = 白胜 */
    Board b; board_init(&b);
    place_horizontal_run(&b, 7, 5, 6, BLACK);
    ASSERT_EQ(board_check_winner(&b), RESULT_WHITE_WIN_NORMAL,
              "winner: black 6-in-row (overline) = WHITE_WIN");
}

static void test_winner_white_5(void) {
    Board b; board_init(&b);
    place_horizontal_run(&b, 7, 5, 5, WHITE);
    ASSERT_EQ(board_check_winner(&b), RESULT_WHITE_WIN_NORMAL, "winner: white 5 = WHITE_WIN");
}

static void test_winner_white_6_overline_still_win(void) {
    /* 国规 9.1：白长连视同五连，白方仍胜 */
    Board b; board_init(&b);
    place_horizontal_run(&b, 7, 5, 6, WHITE);
    ASSERT_EQ(board_check_winner(&b), RESULT_WHITE_WIN_NORMAL,
              "winner: white 6-in-row = WHITE_WIN (long connect counts as 5)");
}

static void test_winner_diagonal(void) {
    Board b; board_init(&b);
    for (int i = 0; i < 5; i++) b.cells[3 + i][3 + i] = BLACK;
    b.move_count = 5;
    ASSERT_EQ(board_check_winner(&b), RESULT_BLACK_WIN, "winner: diagonal black 5 = BLACK_WIN");
}

static void test_winner_anti_diagonal(void) {
    Board b; board_init(&b);
    for (int i = 0; i < 5; i++) b.cells[3 + i][10 - i] = WHITE;
    b.move_count = 5;
    ASSERT_EQ(board_check_winner(&b), RESULT_WHITE_WIN_NORMAL,
              "winner: anti-diagonal white 5 = WHITE_WIN");
}

static void test_winner_vertical(void) {
    Board b; board_init(&b);
    for (int i = 0; i < 5; i++) b.cells[5 + i][7] = BLACK;
    b.move_count = 5;
    ASSERT_EQ(board_check_winner(&b), RESULT_BLACK_WIN, "winner: vertical black 5 = BLACK_WIN");
}

static void test_winner_4_in_row_no_win(void) {
    Board b; board_init(&b);
    place_horizontal_run(&b, 7, 5, 4, BLACK);
    ASSERT_EQ(board_check_winner(&b), RESULT_NONE, "winner: only 4 in row = no win");
}

static void test_board_zobrist_round_trip(void) {
    /* place + undo 后 zobrist_hash 应回到 0 */
    Board b; board_init(&b);
    ASSERT_EQ((int)(b.zobrist_hash == 0), 1, "zobrist: init hash = 0");
    board_place(&b, 7, 7, BLACK);
    ASSERT_TRUE(b.zobrist_hash != 0, "zobrist: place changes hash");
    uint64_t h_after_first = b.zobrist_hash;
    board_place(&b, 7, 8, WHITE);
    ASSERT_TRUE(b.zobrist_hash != h_after_first, "zobrist: 2nd place changes hash again");
    board_undo(&b);
    ASSERT_TRUE(b.zobrist_hash == h_after_first, "zobrist: undo restores prior hash");
    board_undo(&b);
    ASSERT_EQ((int)(b.zobrist_hash == 0), 1, "zobrist: full undo returns to 0");
}

int main(void) {
    zobrist_init();
    test_board_init();
    test_board_in_bounds();
    test_board_place_basic();
    test_board_place_invalid();
    test_board_undo();
    test_board_place_undo_idempotent();
    test_board_is_full();
    test_winner_none_on_empty();
    test_winner_black_5_horizontal();
    test_winner_black_6_overline_negative();
    test_winner_white_5();
    test_winner_white_6_overline_still_win();
    test_winner_diagonal();
    test_winner_anti_diagonal();
    test_winner_vertical();
    test_winner_4_in_row_no_win();
    test_board_zobrist_round_trip();
    TEST_REPORT("test_board");
}

/* tests/test_board.c */
#include <string.h>
#include "test_runner.h"
#include "../src/board.h"

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

int main(void) {
    test_board_init();
    test_board_in_bounds();
    test_board_place_basic();
    test_board_place_invalid();
    test_board_undo();
    test_board_place_undo_idempotent();
    test_board_is_full();
    TEST_REPORT("test_board");
}

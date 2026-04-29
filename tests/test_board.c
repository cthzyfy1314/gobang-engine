/* tests/test_board.c */
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

int main(void) {
    test_board_init();
    test_board_in_bounds();
    TEST_REPORT("test_board");
}

/* tests/test_zobrist.c */
#include "test_runner.h"
#include "../src/board.h"
#include "../src/zobrist.h"

static void test_compute_empty_board(void) {
    Board b; board_init(&b);
    uint64_t h = zobrist_compute(&b);
    /* 空盘 + side_to_move=BLACK → 0 */
    ASSERT_EQ((int)(h == 0), 1, "compute: empty board (black to move) -> 0");
}

static void test_compute_changes_with_piece(void) {
    Board b; board_init(&b);
    uint64_t h0 = zobrist_compute(&b);
    b.cells[7][7] = BLACK;
    uint64_t h1 = zobrist_compute(&b);
    ASSERT_TRUE(h0 != h1, "compute: placing black changes hash");
}

static void test_xor_piece_self_inverse(void) {
    /* XOR 自反：对同一 (r,c,color) XOR 两次应该回到原值 */
    uint64_t h = 0xDEADBEEFCAFEBABEULL;
    uint64_t h2 = zobrist_xor_piece(h, 7, 7, BLACK);
    uint64_t h3 = zobrist_xor_piece(h2, 7, 7, BLACK);
    ASSERT_TRUE(h == h3, "xor_piece: self-inverse");
}

static void test_xor_side_self_inverse(void) {
    uint64_t h = 0x1234567890ABCDEFULL;
    uint64_t h2 = zobrist_xor_side(h);
    uint64_t h3 = zobrist_xor_side(h2);
    ASSERT_TRUE(h == h3, "xor_side: self-inverse");
}

static void test_compute_order_independent(void) {
    /* 两种落子顺序（黑 H8 + 白 H9）应得到相同 hash */
    Board b1; board_init(&b1);
    b1.cells[7][7] = BLACK;
    b1.cells[7][8] = WHITE;
    b1.move_count = 2;
    b1.side_to_move = BLACK;

    Board b2; board_init(&b2);
    b2.cells[7][8] = WHITE;
    b2.cells[7][7] = BLACK;
    b2.move_count = 2;
    b2.side_to_move = BLACK;

    ASSERT_TRUE(zobrist_compute(&b1) == zobrist_compute(&b2), "compute: order-independent");
}

static void test_tt_put_get_roundtrip(void) {
    tt_clear();
    uint64_t key = 0xAABBCCDDEEFF0011ULL;
    tt_put(key, 5, 1234, TT_FLAG_EXACT, 7, 7);
    const TTEntry *e = tt_get(key);
    ASSERT_TRUE(e != NULL, "tt: get returns entry after put");
    if (e) {
        ASSERT_EQ(e->depth, 5, "tt: depth round-trip");
        ASSERT_EQ(e->score, 1234, "tt: score round-trip");
        ASSERT_EQ(e->flag, TT_FLAG_EXACT, "tt: flag round-trip");
    }
}

static void test_tt_get_missing(void) {
    tt_clear();
    const TTEntry *e = tt_get(0xDEADBEEFCAFEBABEULL);
    ASSERT_TRUE(e == NULL, "tt: get returns NULL when not present");
}

static void test_incremental_matches_recompute(void) {
    /* 增量 hash（board_place）与从头算 hash（zobrist_compute）应相等 */
    Board b; board_init(&b);
    board_place(&b, 7, 7, BLACK);
    board_place(&b, 7, 8, WHITE);
    board_place(&b, 8, 8, BLACK);
    board_place(&b, 5, 5, WHITE);
    uint64_t inc = b.zobrist_hash;
    uint64_t recomp = zobrist_compute(&b);
    ASSERT_TRUE(inc == recomp, "incremental hash equals recomputed hash after 4 plies");
    /* 撤一步后再校验 */
    board_undo(&b);
    inc = b.zobrist_hash;
    recomp = zobrist_compute(&b);
    ASSERT_TRUE(inc == recomp, "incremental hash equals recomputed hash after undo");
}

int main(void) {
    zobrist_init();
    test_compute_empty_board();
    test_compute_changes_with_piece();
    test_xor_piece_self_inverse();
    test_xor_side_self_inverse();
    test_compute_order_independent();
    test_tt_put_get_roundtrip();
    test_tt_get_missing();
    test_incremental_matches_recompute();
    TEST_REPORT("test_zobrist");
}

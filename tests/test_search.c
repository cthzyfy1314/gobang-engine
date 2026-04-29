/* tests/test_search.c */
#include "test_runner.h"
#include "../src/board.h"
#include "../src/search.h"

static void test_evaluate_empty(void) {
    Board b; board_init(&b);
    int score = search_evaluate(&b);
    ASSERT_EQ(score, 0, "evaluate: empty board score = 0");
}

static void test_evaluate_center_better_than_corner(void) {
    /* 黑下中央 vs 黑下角落，从白方视角，中央对白更不利 */
    Board b1; board_init(&b1);
    b1.cells[7][7] = BLACK;
    b1.move_count = 1;
    b1.side_to_move = WHITE;

    Board b2; board_init(&b2);
    b2.cells[0][0] = BLACK;
    b2.move_count = 1;
    b2.side_to_move = WHITE;

    int s1 = search_evaluate(&b1);
    int s2 = search_evaluate(&b2);
    /* 视角是白方，黑下中央对白更不利，所以 s1 < s2 */
    ASSERT_TRUE(s1 < s2, "evaluate: white-perspective with black at center < black at corner");
}

int main(void) {
    test_evaluate_empty();
    test_evaluate_center_better_than_corner();
    TEST_REPORT("test_search");
}

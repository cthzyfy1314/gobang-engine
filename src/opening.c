/* src/opening.c — 极简开局实现 */
#include "opening.h"

Move opening_first_move(void) {
    Move m = { 7, 7, BLACK };
    return m;
}

bool opening_should_swap(const Board *b) {
    (void)b;
    return false;
}

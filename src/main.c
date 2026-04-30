/* src/main.c — Day 1 minimal self-play demo
 * 让黑白方各自调用 search_best_move 互下 10 步，打印过程。
 * Day 4 ui 模块完成后会被替换成正式 GUI。
 */
#include <stdio.h>
#include "board.h"
#include "search.h"
#include "zobrist.h"

static void print_board_minimal(const Board *b) {
    printf("    ");
    for (int c = 0; c < BOARD_SIZE; c++) printf("%c ", 'A' + c);
    printf("\n");
    for (int r = 0; r < BOARD_SIZE; r++) {
        printf("%2d  ", BOARD_SIZE - r);
        for (int c = 0; c < BOARD_SIZE; c++) {
            uint8_t v = b->cells[r][c];
            printf("%c ", v == EMPTY ? '.' : v == BLACK ? 'X' : 'O');
        }
        printf("\n");
    }
    printf("\n");
}

int main(void) {
    zobrist_init();

    Board b;
    board_init(&b);

    printf("gobang-engine self-play demo (10 plies, depth=2)\n\n");

    for (int ply = 0; ply < 10; ply++) {
        SearchResult r = search_best_move(&b, 2);
        if (r.best_move.row < 0) {
            printf("No legal move. Stopping.\n");
            break;
        }
        const char *who = (b.side_to_move == BLACK) ? "BLACK(X)" : "WHITE(O)";
        printf("Ply %2d: %s plays %c%d  (score=%d, nodes=%ld)\n",
               ply + 1, who, 'A' + r.best_move.col, BOARD_SIZE - r.best_move.row,
               r.score, r.nodes_searched);
        board_place(&b, r.best_move.row, r.best_move.col, r.best_move.color);

        GameResult res = board_check_winner(&b);
        if (res != RESULT_NONE) {
            print_board_minimal(&b);
            printf("Game ended: result = %d\n", res);
            return 0;
        }
    }

    print_board_minimal(&b);
    printf("[Demo finished -- 10 plies, no winner yet]\n");
    return 0;
}

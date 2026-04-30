/* src/main.c — 入口
 * 默认进入交互式 UI 主循环（人肉协议对战）。
 *   --white      我方执白（默认黑）
 *   --black      我方执黑（默认）
 *   --depth=N    搜索深度（默认 4）
 *   --demo       跑 self-play demo 而非 UI（回归测试用）
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "board.h"
#include "search.h"
#include "zobrist.h"
#include "ui.h"

static void run_demo(int depth) {
    Board b;
    board_init(&b);

    printf("gobang-engine self-play demo (10 plies, depth=%d)\n\n", depth);

    long total_nodes = 0;
    for (int ply = 0; ply < 10; ply++) {
        SearchResult r = search_best_move(&b, depth);
        total_nodes += r.nodes_searched;
        if (r.best_move.row < 0) { printf("No legal move.\n"); break; }
        const char *who = (b.side_to_move == BLACK) ? "BLACK(X)" : "WHITE(O)";
        printf("Ply %2d: %s plays %c%d  (score=%d, nodes=%ld)\n",
               ply + 1, who, 'A' + r.best_move.col, BOARD_SIZE - r.best_move.row,
               r.score, r.nodes_searched);
        board_place(&b, r.best_move.row, r.best_move.col, r.best_move.color);

        GameResult res = board_check_winner(&b);
        if (res != RESULT_NONE) {
            ui_draw_board(&b);
            printf("Game ended: result = %d\n", res);
            return;
        }
    }

    ui_draw_board(&b);
    printf("[Demo finished -- 10 plies, total nodes = %ld]\n", total_nodes);
}

int main(int argc, char **argv) {
#ifdef _WIN32
    /* Windows console 默认 GBK (codepage 936) → 源码中的 UTF-8 字符乱码。
     * 切到 UTF-8 (65001)。如果切换失败也无伤大雅（程序输出已尽量 ASCII-safe）。
     */
    SetConsoleOutputCP(65001);
#endif
    zobrist_init();

    int my_color = BLACK;
    int depth = 4;
    int demo_mode = 0;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "--white"))         my_color = WHITE;
        else if (!strcmp(argv[i], "--black"))         my_color = BLACK;
        else if (!strncmp(argv[i], "--depth=", 8)) {
            depth = atoi(argv[i] + 8);
            if (depth < 1)  depth = 1;
            if (depth > 12) depth = 12;
        }
        else if (!strcmp(argv[i], "--demo"))          demo_mode = 1;
        else {
            printf("Unknown option: %s\n", argv[i]);
            printf("Usage: gobang-engine [--white|--black] [--depth=N] [--demo]\n");
            return 1;
        }
    }

    if (demo_mode) run_demo(depth);
    else           ui_main_loop(my_color, depth);

    return 0;
}

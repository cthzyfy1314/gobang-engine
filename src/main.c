/* src/main.c — 入口
 * 默认进入交互式 UI 主循环（人肉协议对战）。
 *   --white      我方执白（默认黑）
 *   --black      我方执黑（默认）
 *   --depth=N    搜索深度上限（默认 4）
 *   --time=N     每步思考时间预算 (ms)（默认 1000；0 = 不限时）
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

static int _g_time_budget_ms = 1000;

static void run_demo(int depth) {
    Board b;
    board_init(&b);

    printf("gobang-engine self-play demo (10 plies, depth=%d, time=%dms)\n\n",
           depth, _g_time_budget_ms);

    long total_nodes = 0;
    for (int ply = 0; ply < 10; ply++) {
        SearchResult r = search_best_move_timed(&b, depth, _g_time_budget_ms);
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
    /* 启用 ANSI VT 转义（Win10+），让 ui.c 里的颜色 escape sequence 真的渲染。*/
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode)) {
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
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
            if (depth > 20) depth = 20;
        }
        else if (!strncmp(argv[i], "--time=", 7)) {
            _g_time_budget_ms = atoi(argv[i] + 7);
            if (_g_time_budget_ms < 0) _g_time_budget_ms = 0;
        }
        else if (!strcmp(argv[i], "--demo"))          demo_mode = 1;
        else {
            printf("Unknown option: %s\n", argv[i]);
            printf("Usage: gobang-engine [--white|--black] [--depth=N] [--time=MS] [--demo]\n");
            return 1;
        }
    }

    if (demo_mode) run_demo(depth);
    else           ui_main_loop(my_color, depth, _g_time_budget_ms);

    return 0;
}

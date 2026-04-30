/* src/ui.c — 交互式对战 UI（人肉协议）*/
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "ui.h"
#include "search.h"
#include "forbid.h"
#include "pattern.h"
#include "zobrist.h"

/* 把内部 (row, col) 转成 H8 风格字符串（写入 buf，假定 buf >= 4 字节）*/
static const char *coord_to_str(int row, int col, char *buf) {
    sprintf(buf, "%c%d", 'A' + col, BOARD_SIZE - row);
    return buf;
}

void ui_draw_board(const Board *b) {
    printf("\n     A  B  C  D  E  F  G  H  I  J  K  L  M  N  O\n");
    for (int r = 0; r < BOARD_SIZE; r++) {
        printf("%2d ", BOARD_SIZE - r);
        for (int c = 0; c < BOARD_SIZE; c++) {
            uint8_t v = b->cells[r][c];
            const char *s = (v == EMPTY) ? " . " :
                            (v == BLACK) ? " X " :
                                           " O ";
            printf("%s", s);
        }
        printf(" %2d\n", BOARD_SIZE - r);
    }
    printf("     A  B  C  D  E  F  G  H  I  J  K  L  M  N  O\n");
    printf("    X = BLACK, O = WHITE.  moves=%u  side=%s%s\n\n",
           b->move_count,
           b->side_to_move == BLACK ? "BLACK" : "WHITE",
           b->forbid_enabled ? "  [renju forbid: ON]" : "  [forbid: OFF]");
}

static int letter_to_col(char ch) {
    ch = (char)toupper((unsigned char)ch);
    if (ch >= 'A' && ch <= 'O') return ch - 'A';
    return -1;
}

bool ui_parse_input(const char *line, int *row, int *col, UICommand *cmd) {
    *cmd = UI_CMD_NONE;

    /* 去 UTF-8 BOM（某些 shell pipe 会加 EF BB BF 在 stdin 头）*/
    const unsigned char *u = (const unsigned char *)line;
    if (u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF) line += 3;

    /* 去前导空白 */
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '\n' || *line == '\r') {
        *cmd = UI_CMD_INVALID;
        return false;
    }

    /* 单字母命令（仅当后跟空白/换行/EOF）*/
    char first = (char)tolower((unsigned char)*line);
    char second = line[1];
    int single = (second == '\0' || second == '\n' || second == '\r' ||
                  second == ' '  || second == '\t');
    if (single) {
        if (first == 'q') { *cmd = UI_CMD_QUIT;   return false; }
        if (first == 'u') { *cmd = UI_CMD_UNDO;   return false; }
        if (first == 'h') { *cmd = UI_CMD_HINT;   return false; }
        if (first == 'r') { *cmd = UI_CMD_RESIGN; return false; }
    }

    /* 坐标格式 1: 字母 + 数字 (H8 / h08) */
    int c = letter_to_col(*line);
    if (c >= 0) {
        line++;
        int n = 0, digits = 0;
        while (*line >= '0' && *line <= '9') {
            n = n * 10 + (*line - '0');
            line++; digits++;
        }
        if (digits >= 1 && n >= 1 && n <= BOARD_SIZE) {
            *col = c;
            *row = BOARD_SIZE - n;
            return true;
        }
        *cmd = UI_CMD_INVALID;
        return false;
    }

    /* 坐标格式 2: 全数字 row,col 或 row col */
    int rr = 0, cc = 0;
    if (sscanf(line, "%d,%d", &rr, &cc) == 2 ||
        sscanf(line, "%d %d", &rr, &cc) == 2) {
        if (rr >= 0 && rr < BOARD_SIZE && cc >= 0 && cc < BOARD_SIZE) {
            *row = rr;
            *col = cc;
            return true;
        }
    }

    *cmd = UI_CMD_INVALID;
    return false;
}

static void show_engine_suggestion(Board *b, int depth) {
    SearchResult r = search_best_move(b, depth);
    char buf[8];
    if (r.best_move.row < 0) {
        printf(">>> Engine: no legal move available.\n");
        return;
    }
    coord_to_str(r.best_move.row, r.best_move.col, buf);
    printf(">>> Engine suggests: %s  (score=%d, nodes=%ld)\n",
           buf, r.score, r.nodes_searched);

    /* 若我方执黑且该建议位置是禁手 → 给个 sanity warning（理论上 search 已 filter）*/
    if (b->side_to_move == BLACK && b->forbid_enabled) {
        ForbidType ft = forbid_check_black(b, r.best_move.row, r.best_move.col);
        if (ft != FORBID_NONE) {
            printf("    [warn] suggestion is FORBID (search bug?) — please report.\n");
        }
    }
}

static void warn_forbid_for_black(const Board *b, int row, int col) {
    if (b->side_to_move != BLACK || !b->forbid_enabled) return;
    ForbidType ft = forbid_check_black(b, row, col);
    if (ft == FORBID_NONE) return;
    const char *name = (ft == FORBID_DOUBLE_THREE) ? "DOUBLE THREE (33)" :
                       (ft == FORBID_DOUBLE_FOUR)  ? "DOUBLE FOUR (44)"  :
                                                     "OVERLINE (>=6)";
    printf("\n*** FORBID warning at this position: %s ***\n", name);
}

void ui_main_loop(int my_color, int search_depth) {
    Board b;
    board_init(&b);

    printf("=== gobang-engine — Renju (national rules) ===\n");
    printf("You play: %s   Search depth: %d\n",
           my_color == BLACK ? "BLACK (X)" : "WHITE (O)", search_depth);
    printf("Commands per turn:\n");
    printf("    <coord> like H8 / 7,7    place stone at that point\n");
    printf("    h    show engine hint without playing\n");
    printf("    u    undo last move\n");
    printf("    r    resign\n");
    printf("    q    quit immediately\n");

    /* 黑 1 天元 hardcode（spec § 6） */
    if (my_color == BLACK) {
        printf("\nNational rule: BLACK 1 must be at tengen (H8). Auto-playing.\n");
        board_place(&b, 7, 7, BLACK);
    } else {
        printf("\nWaiting for BLACK opponent. Their first move (per national rules) should be H8.\n");
    }

    char line[128];
    while (1) {
        ui_draw_board(&b);

        GameResult res = board_check_winner(&b);
        if (res != RESULT_NONE) {
            const char *msg = (res == RESULT_BLACK_WIN)               ? "BLACK wins!" :
                              (res == RESULT_WHITE_WIN_NORMAL)        ? "WHITE wins!" :
                              (res == RESULT_WHITE_WIN_BY_BLACK_FORBID) ? "WHITE wins (BLACK played a forbid)!" :
                                                                        "DRAW";
            printf("*** Game over: %s ***\n", msg);
            return;
        }

        int active = b.side_to_move;
        const char *who = (active == my_color) ? "YOU" : "OPPONENT";
        printf("[%s] (%s to move) > ", who, active == BLACK ? "BLACK" : "WHITE");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n(EOF) Quit.\n");
            return;
        }

        int row = 0, col = 0;
        UICommand cmd = UI_CMD_NONE;
        bool got = ui_parse_input(line, &row, &col, &cmd);

        if (!got) {
            switch (cmd) {
                case UI_CMD_QUIT:
                    printf("Quit.\n");
                    return;
                case UI_CMD_UNDO:
                    if (board_undo(&b)) printf("Undo OK.\n");
                    else                printf("Cannot undo: empty history.\n");
                    continue;
                case UI_CMD_HINT:
                    show_engine_suggestion(&b, search_depth);
                    continue;
                case UI_CMD_RESIGN:
                    printf("You resigned. %s wins.\n",
                           my_color == BLACK ? "WHITE" : "BLACK");
                    return;
                default:
                    printf("Invalid input. Try H8 / 7,7 / u / h / q / r.\n");
                    continue;
            }
        }

        /* 若我方执黑落子且该点是禁手，警告但仍允许（让用户决定）*/
        if (active == my_color) warn_forbid_for_black(&b, row, col);

        if (!board_place(&b, row, col, active)) {
            printf("Illegal move (occupied or out of range).\n");
            continue;
        }

        char buf[8];
        coord_to_str(row, col, buf);
        printf("%s plays %s.\n", who, buf);

        /* 我方刚下完后不给建议；对手下完后给我方建议 */
        if (active != my_color) {
            show_engine_suggestion(&b, search_depth);
        }
    }
}

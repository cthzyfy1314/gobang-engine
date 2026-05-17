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
#include "opening.h"

/* 把内部 (row, col) 转成 H8 风格字符串（写入 buf，假定 buf >= 4 字节）*/
static const char *coord_to_str(int row, int col, char *buf) {
    sprintf(buf, "%c%d", 'A' + col, BOARD_SIZE - row);
    return buf;
}

/* ANSI VT escape macros — main.c 已开 ENABLE_VIRTUAL_TERMINAL_PROCESSING */
#define ANSI_RESET   "\x1b[0m"
#define ANSI_DIM     "\x1b[2m"
#define ANSI_BOLD    "\x1b[1m"
#define ANSI_GRAY    "\x1b[90m"     /* dim gray — 空交叉点 */
#define ANSI_BLACK   "\x1b[1;36m"   /* bold cyan — 黑子（深色背景下也可见）*/
#define ANSI_WHITE   "\x1b[1;33m"   /* bold yellow — 白子 */
#define ANSI_STAR    "\x1b[35m"     /* magenta — 星点 */
#define ANSI_LAST    "\x1b[1;91m"   /* bold bright red — 最后一手高亮 */

/* 国规 Renju 15x15 的 5 个星点：H8 天元 + 4 角星 D4/L4/D12/L12 */
static int is_star_point(int r, int c) {
    return (r == 7  && c == 7)  ||  /* H8 tengen */
           (r == 3  && c == 3)  ||  /* D12 */
           (r == 3  && c == 11) ||  /* L12 */
           (r == 11 && c == 3)  ||  /* D4 */
           (r == 11 && c == 11);    /* L4 */
}

void ui_draw_board(const Board *b) {
    /* 最后一手坐标（用于高亮）*/
    int last_r = -1, last_c = -1;
    if (b->move_count > 0) {
        last_r = b->history[b->move_count - 1].row;
        last_c = b->history[b->move_count - 1].col;
    }

    /* 顶部字母列标 */
    printf("\n      ");
    for (int c = 0; c < BOARD_SIZE; c++) printf(" %c ", 'A' + c);
    printf("\n");

    for (int r = 0; r < BOARD_SIZE; r++) {
        printf("  %2d  ", BOARD_SIZE - r);
        for (int c = 0; c < BOARD_SIZE; c++) {
            uint8_t v = b->cells[r][c];
            int is_last = (r == last_r && c == last_c);

            if (v == EMPTY) {
                if (is_star_point(r, c)) {
                    printf(" " ANSI_STAR "+" ANSI_RESET " ");
                } else {
                    printf(" " ANSI_GRAY "." ANSI_RESET " ");
                }
            } else if (v == BLACK) {
                if (is_last) printf(ANSI_LAST "[X]" ANSI_RESET);
                else         printf(" " ANSI_BLACK "X" ANSI_RESET " ");
            } else { /* WHITE */
                if (is_last) printf(ANSI_LAST "[O]" ANSI_RESET);
                else         printf(" " ANSI_WHITE "O" ANSI_RESET " ");
            }
        }
        printf("\n");
    }

    printf("\n  " ANSI_DIM "moves=%u  side=" ANSI_RESET "%s%s  " ANSI_DIM "%s" ANSI_RESET "\n\n",
           b->move_count,
           b->side_to_move == BLACK ? ANSI_BLACK "BLACK" ANSI_RESET : ANSI_WHITE "WHITE" ANSI_RESET,
           "",
           b->forbid_enabled ? "[renju forbid: ON]" : "[forbid: OFF]");
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

static void show_engine_suggestion(Board *b, int depth, int time_budget_ms) {
    SearchResult r = search_best_move_timed(b, depth, time_budget_ms);
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
            printf("    [warn] suggestion is FORBID (search bug?) -- please report.\n");
        }
    }
}

/* 返回 FORBID_NONE 表示该格落黑合法；否则返回禁手类型并把人类可读名写入 *out_name。
 * out_name 缓冲区由调用方提供（>=32 字节）。
 */
static ForbidType check_black_forbid(const Board *b, int row, int col, char *out_name) {
    if (b->side_to_move != BLACK || !b->forbid_enabled) return FORBID_NONE;
    ForbidType ft = forbid_check_black(b, row, col);
    if (ft == FORBID_NONE) return FORBID_NONE;
    const char *name = (ft == FORBID_DOUBLE_THREE) ? "DOUBLE THREE (33)" :
                       (ft == FORBID_DOUBLE_FOUR)  ? "DOUBLE FOUR (44)"  :
                                                     "OVERLINE (>=6)";
    if (out_name) {
        size_t i = 0;
        while (name[i] && i < 31) { out_name[i] = name[i]; i++; }
        out_name[i] = '\0';
    }
    return ft;
}

/* === 国规序号显示辅助：根据 move_count + color 给出 "黑1/白2/黑3..." === */
static void move_label(int move_count, int color, char *buf) {
    /* move_count 是已落子数（含本步）。color 是这一步的颜色 */
    sprintf(buf, "%s%d", color == BLACK ? "B" : "W", move_count);
}

/* 读一行从 stdin。返回 true 表示读到内容（可能空行）；false 表示 EOF。
 * 自动 strip BOM + 末尾 \r\n。
 */
static bool read_line(char *buf, int sz) {
    if (!fgets(buf, sz, stdin)) return false;
    /* strip BOM */
    if ((unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF)
        memmove(buf, buf + 3, strlen(buf + 3) + 1);
    /* strip \r\n */
    int n = (int)strlen(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' ' || buf[n-1] == '\t'))
        buf[--n] = '\0';
    return true;
}

/* 解析坐标。返回 true 成功，*row/*col 填入。 */
static bool parse_coord(const char *s, int *row, int *col) {
    UICommand dummy;
    return ui_parse_input(s, row, col, &dummy);
}

/* === 阶段 1：开局录入 ===
 * 录入前 3 手（黑1/白2/黑3）+ N（五手 N 打的 N）。
 * 国规要求 B1+W2+B3 必须形成 26 种指定开局之一（见 opening.c）。
 * AI 执黑：用户报自己想下的开局；AI 执白：用户报对方报的开局。
 * 返回：成功返回 true，board 已落 3 子；失败/quit 返回 false。
 */

/* 列出全部 26 种开局，附编号 + 名字 + 三手坐标。 */
static void list_all_openings(void) {
    printf("\nAvailable 26 openings (B1=H8 always). Pick by name or by 3 coords:\n");
    printf("  --- Direct (W2=H9) ---                       --- Indirect (W2=I9) ---\n");
    char b1s[8], w2s[8], b3s[8];
    /* 13 直指 + 13 斜指 = 13 行 * 2 列 = 13 行 */
    for (int row = 0; row < 13; row++) {
        const RenjuOpening *d = opening_get(row);          /* 直指 0..12 */
        const RenjuOpening *i = opening_get(row + 13);     /* 斜指 13..25 */
        coord_to_str(d->b1[0], d->b1[1], b1s);
        coord_to_str(d->w2[0], d->w2[1], w2s);
        coord_to_str(d->b3[0], d->b3[1], b3s);
        printf("  %2d) %-8s %-10s %s %s %s   |", row + 1, d->name_cn, d->name_jp, b1s, w2s, b3s);
        coord_to_str(i->b1[0], i->b1[1], b1s);
        coord_to_str(i->w2[0], i->w2[1], w2s);
        coord_to_str(i->b3[0], i->b3[1], b3s);
        printf("  %2d) %-8s %-10s %s %s %s\n", row + 14, i->name_cn, i->name_jp, b1s, w2s, b3s);
    }
}

static bool phase1_opening(Board *b, int my_color, int *out_N) {
    char line[128];
    int target_N = 5;

    printf("\n=== Phase 1: Opening (national rule 4) ===\n");
    if (my_color == BLACK) {
        printf("You play BLACK. Decide opening: enter 3 coords (B1 W2 B3) and N.\n");
        printf("  Format: <B1> <W2> <B3> <N>     e.g. H8 H9 I10 5    (= 花月, N=5)\n");
        printf("  Rule:   (B1, W2, B3) must be one of the 26 national-rule openings.\n");
        printf("          B1 is always H8 (tengen).\n");
        printf("  Type 'list' to see all 26 openings.  Type 'auto' to let engine pick one.\n");
        printf("  Note:   N = number of black-5 candidates you'll offer in phase 4.\n");
    } else {
        printf("You play WHITE. Enter the opening BLACK reported (3 coords + N).\n");
        printf("  Format: <B1> <W2> <B3> <N>     e.g. H8 H9 I10 5    (= 花月, N=5)\n");
        printf("  Type 'list' to see all 26 openings.\n");
    }

    while (1) {
        printf("[Phase 1] > ");
        fflush(stdout);
        if (!read_line(line, sizeof(line))) return false;
        if (line[0] == 'q' || line[0] == 'Q') return false;

        /* 'list' 命令：列出全部 26 种 */
        if (strncmp(line, "list", 4) == 0 || strncmp(line, "LIST", 4) == 0) {
            list_all_openings();
            continue;
        }

        /* 'auto' 命令仅 AI 执黑时可用 */
        if (strncmp(line, "auto", 4) == 0 || strncmp(line, "AUTO", 4) == 0) {
            if (my_color != BLACK) {
                printf("'auto' is only available when AI plays BLACK. "
                       "You play BLACK; enter the opening BLACK reported.\n");
                continue;
            }
            /* 解析: "auto" 或 "auto N" 或 "auto N <trailing garbage>" */
            int N_in = 5;
            char extra[64] = {0};
            int parsed = sscanf(line, "%*s %d %63s", &N_in, extra);
            if (parsed < 1) N_in = 5;
            if (parsed >= 2 && extra[0] != '\0') {
                printf("Warning: extra tokens after 'auto N' ignored: '%s'\n", extra);
            }
            if (N_in < 1 || N_in > 16) {
                printf("N must be in [1, 16]. Got %d. Default to 5.\n", N_in);
                N_in = 5;
            }
            int idx = opening_choose_by_black_strategy();
            const RenjuOpening *o = opening_get(idx);
            if (!o) { printf("opening_choose_by_black_strategy returned bad idx.\n"); continue; }
            if (!board_place(b, o->b1[0], o->b1[1], BLACK))    { printf("B1 conflict.\n"); continue; }
            if (!board_place(b, o->w2[0], o->w2[1], WHITE))    { printf("W2 conflict.\n"); board_undo(b); continue; }
            if (!board_place(b, o->b3[0], o->b3[1], BLACK))    { printf("B3 conflict.\n"); board_undo(b); board_undo(b); continue; }
            char b1s[8], b2s[8], b3s[8];
            coord_to_str(o->b1[0], o->b1[1], b1s);
            coord_to_str(o->w2[0], o->w2[1], b2s);
            coord_to_str(o->b3[0], o->b3[1], b3s);
            target_N = N_in;
            printf(">>> Engine picked opening #%d: %s (%s)\n", idx + 1, o->name_cn, o->name_jp);
            printf(">>> Phase 1 done: B1=%s W2=%s B3=%s, N=%d. Report this to opponent.\n",
                   b1s, b2s, b3s, target_N);
            *out_N = target_N;
            return true;
        }

        /* 解析 4 个 token: 3 个坐标 + N */
        char t1[16], t2[16], t3[16];
        int N_in;
        if (sscanf(line, "%15s %15s %15s %d", t1, t2, t3, &N_in) != 4) {
            printf("Need 4 tokens: 3 coords + N. Type 'list' to see all 26 openings.\n");
            continue;
        }
        int r1, c1, r2, c2, r3, c3;
        if (!parse_coord(t1, &r1, &c1) || !parse_coord(t2, &r2, &c2) || !parse_coord(t3, &r3, &c3)) {
            printf("Bad coordinate. Try again.\n");
            continue;
        }
        /* 验证 B1 = H8 (tengen) — 26 种全要求 */
        if (r1 != 7 || c1 != 7) {
            printf("Rule 4: B1 must be H8 (tengen). Got %s. Try again.\n", t1);
            continue;
        }
        /* 验证 (B1, W2, B3) 是 26 种之一 */
        int opening_idx = -1;
        if (!opening_matches(r1, c1, r2, c2, r3, c3, &opening_idx)) {
            printf("Rule 4: (B1=%s, W2=%s, B3=%s) is NOT one of the 26 national-rule openings.\n",
                   t1, t2, t3);
            printf("        Type 'list' to see the valid 26.  Try again.\n");
            continue;
        }
        if (N_in < 1 || N_in > 16) {
            printf("N must be in [1, 16]. Got %d. Try again.\n", N_in);
            continue;
        }
        /* 落 3 子 */
        if (!board_place(b, r1, c1, BLACK)) { printf("B1 invalid.\n"); continue; }
        if (!board_place(b, r2, c2, WHITE)) { printf("W2 invalid (occupied?).\n"); board_undo(b); continue; }
        if (!board_place(b, r3, c3, BLACK)) { printf("B3 invalid.\n"); board_undo(b); board_undo(b); continue; }
        target_N = N_in;
        const RenjuOpening *o = opening_get(opening_idx);
        char b1s[8], b2s[8], b3s[8];
        coord_to_str(r1, c1, b1s); coord_to_str(r2, c2, b2s); coord_to_str(r3, c3, b3s);
        printf(">>> Opening #%d: %s (%s)\n", opening_idx + 1, o->name_cn, o->name_jp);
        printf(">>> Phase 1 done: B1=%s W2=%s B3=%s, N=%d\n", b1s, b2s, b3s, target_N);
        *out_N = target_N;
        return true;
    }
}

/* === 阶段 2：三手交换（国规 6） ===
 * 白方有权决定是否换边。
 * 返回：true 表示发生了交换，调用方需把 my_color 取反。
 */
static bool phase2_swap(Board *b, int *my_color) {
    char line[128];
    printf("\n=== Phase 2: 3rd-move swap (national rule 6) ===\n");
    printf("WHITE has the right to swap sides (one chance per game).\n");

    if (*my_color == WHITE) {
        /* 我执白：引擎评估并建议 + 用户决定 */
        printf("[Engine evaluating swap...]\n");
        /* 简化评估：当前局面分（白视角）。> 0 说明白当前不亏，不应换；< 0 说明白亏，应换 */
        b->zobrist_hash = zobrist_compute(b);
        int score_as_white = pattern_evaluate(b);   /* 白视角 */
        printf(">>> Engine score (current as WHITE): %d\n", score_as_white);
        const char *advice = (score_as_white < -300) ? "**SWAP** (WHITE seems losing)" : "**KEEP WHITE** (no swap)";
        printf(">>> Engine advice: %s\n", advice);
        /* 严格 1/2 校验，错误输入循环重提 */
        while (1) {
            printf("Your decision (1 = keep WHITE, 2 = swap to BLACK) > ");
            fflush(stdout);
            if (!read_line(line, sizeof(line))) return false;
            /* trim leading whitespace then check single char + only-whitespace-after */
            const char *p = line; while (*p == ' ' || *p == '\t') p++;
            if ((*p == '1' || *p == '2') && (p[1] == '\0' || p[1] == '\n' || p[1] == '\r')) {
                if (*p == '2') {
                    *my_color = BLACK;
                    printf(">>> You swapped. You now play BLACK. Tell opponent.\n");
                    return true;
                }
                printf(">>> You kept WHITE. Tell opponent.\n");
                return false;
            }
            printf("Invalid. Enter exactly '1' or '2'. Try again.\n");
        }
    } else {
        /* 我执黑：等对方报告是否换 */
        printf("Wait for opponent's decision and enter:\n");
        printf("  1 = opponent keeps WHITE (no swap)\n");
        printf("  2 = opponent SWAPS (you become WHITE)\n");
        while (1) {
            printf("[Phase 2] > ");
            fflush(stdout);
            if (!read_line(line, sizeof(line))) return false;
            const char *p = line; while (*p == ' ' || *p == '\t') p++;
            if ((*p == '1' || *p == '2') && (p[1] == '\0' || p[1] == '\n' || p[1] == '\r')) {
                if (*p == '2') {
                    *my_color = WHITE;
                    printf(">>> Opponent swapped. You now play WHITE.\n");
                    return true;
                }
                printf(">>> Opponent kept WHITE. You stay BLACK.\n");
                return false;
            }
            printf("Invalid. Enter exactly '1' or '2'. Try again.\n");
        }
    }
}

/* === 阶段 4：五手 N 打（国规 7） ===
 * 黑方提供 N 个候选，白方挑一个作黑5。
 */
static bool phase4_n_strikes(Board *b, int my_color, int N, int search_depth, int time_budget_ms) {
    char line[128];
    printf("\n=== Phase 4: 5th-move N-strikes (national rule 7) ===\n");

    if (my_color == BLACK) {
        /* 我执黑：引擎找 N 个候选，让用户报给对方 */
        Move cands[16];
        int scores[16];
        printf("[Engine computing %d candidate B5 moves...]\n", N);
        int got = search_find_n_distinct(b, search_depth, N, cands, scores, time_budget_ms);
        if (got == 0) {
            printf("No legal candidate. Resign.\n");
            return false;
        }
        printf(">>> %d candidates (report ALL of them to opponent):\n", got);
        for (int i = 0; i < got; i++) {
            char buf[8]; coord_to_str(cands[i].row, cands[i].col, buf);
            printf("    %d) %s   (score=%d)%s\n", i + 1, buf, scores[i], i == 0 ? "  [engine top pick]" : "");
        }
        /* 错误输入不退出，循环重提 */
        int idx = 0;
        while (1) {
            printf("Enter the index (1..%d) opponent picked, or 'q' to quit > ", got);
            fflush(stdout);
            if (!read_line(line, sizeof(line))) return false;
            if (line[0] == 'q' || line[0] == 'Q') return false;
            idx = atoi(line);
            if (idx >= 1 && idx <= got) break;
            printf("Invalid index. Need 1..%d (the position number, not a coordinate). Try again.\n", got);
        }
        Move pick = cands[idx - 1];
        char buf[8]; coord_to_str(pick.row, pick.col, buf);
        if (!board_place(b, pick.row, pick.col, BLACK)) { printf("B5 conflict.\n"); return false; }
        printf(">>> B5 = %s (chosen by opponent)\n", buf);
        return true;
    } else {
        /* 我执白：录入对方报的 N 个候选，引擎挑最不利黑方的一个。
         * 校验：每个候选必须是空格、未占用、未与之前候选重复。
         */
        printf("Opponent will report %d candidate B5 positions. Enter them one per line.\n", N);
        Move cands[16];
        int got = 0;
        while (got < N) {
            printf("Candidate %d/%d > ", got + 1, N);
            fflush(stdout);
            if (!read_line(line, sizeof(line))) return false;
            if (line[0] == 'q' || line[0] == 'Q') return false;
            int r, c;
            if (!parse_coord(line, &r, &c)) { printf("Bad coord. Re-enter.\n"); continue; }
            if (b->cells[r][c] != EMPTY) { printf("Cell already occupied. Re-enter.\n"); continue; }
            int dup = 0;
            for (int k = 0; k < got; k++) {
                if (cands[k].row == r && cands[k].col == c) { dup = 1; break; }
            }
            if (dup) { printf("Duplicate of an earlier candidate. Re-enter.\n"); continue; }
            cands[got].row = (int8_t)r;
            cands[got].col = (int8_t)c;
            cands[got].color = BLACK;
            got++;
        }
        /* 引擎挑：对每个 candidate 模拟黑落子后从白视角的分（越高对白越好 = 越差给黑）*/
        int best_idx = 0;
        int best_score_for_white = -SEARCH_INF;
        for (int i = 0; i < got; i++) {
            if (!board_place(b, cands[i].row, cands[i].col, BLACK)) continue;
            int s = pattern_evaluate(b);  /* side_to_move 此时是 WHITE，pattern_evaluate 已经从白视角 */
            if (s > best_score_for_white) {
                best_score_for_white = s;
                best_idx = i;
            }
            board_undo(b);
        }
        char buf[8]; coord_to_str(cands[best_idx].row, cands[best_idx].col, buf);
        printf(">>> Engine picks candidate %d (= %s) for B5. Tell opponent.\n", best_idx + 1, buf);
        if (!board_place(b, cands[best_idx].row, cands[best_idx].col, BLACK)) {
            printf("B5 placement failed.\n"); return false;
        }
        return true;
    }
}

/* === 单步：要么 AI 自动出，要么录入对方坐标 === */
static bool phase5_one_turn(Board *b, int my_color, int search_depth, int time_budget_ms) {
    char line[128];
    int active = b->side_to_move;
    int next_move_no = b->move_count + 1;
    char label[8]; move_label(next_move_no, active, label);

    if (active == my_color) {
        printf("[AI thinking %s @ d=%d t=%dms ...]\n", label, search_depth, time_budget_ms);
        fflush(stdout);
        SearchResult r = search_best_move_timed(b, search_depth, time_budget_ms);
        if (r.best_move.row < 0) {
            printf(">>> AI: no legal move. Resigning.\n"); return false;
        }
        char buf[8]; coord_to_str(r.best_move.row, r.best_move.col, buf);
        printf(">>> AI plays %s = %s   (score=%d, nodes=%ld)\n", label, buf, r.score, r.nodes_searched);
        board_place(b, r.best_move.row, r.best_move.col, active);
        return true;
    }
    /* 对手回合 */
    while (1) {
        printf("[Opponent %s] enter coord (H8/7,7), u=undo pair, q=quit > ", label);
        fflush(stdout);
        if (!read_line(line, sizeof(line))) return false;
        int row, col;
        UICommand cmd;
        bool got = ui_parse_input(line, &row, &col, &cmd);
        if (!got) {
            if (cmd == UI_CMD_QUIT) return false;
            if (cmd == UI_CMD_UNDO) {
                if (board_undo(b)) {
                    if (board_undo(b)) printf("Undo OK (1 opp + 1 AI).\n");
                    else                printf("Undo OK (1 move).\n");
                    return true;
                }
                printf("Cannot undo: empty.\n"); continue;
            }
            if (cmd == UI_CMD_RESIGN) { printf("Resigned.\n"); return false; }
            printf("Invalid. Re-enter.\n"); continue;
        }
        if (active == BLACK) {
            char fname[32];
            ForbidType ft = check_black_forbid(b, row, col, fname);
            if (ft != FORBID_NONE) {
                /* 国规 9.x：黑棋禁手 → 白胜（不允许落子，直接判负） */
                char buf[8]; coord_to_str(row, col, buf);
                printf("\n*** BLACK plays forbidden move %s at %s ***\n", fname, buf);
                printf("*** Game over: WHITE wins (BLACK forbid) ***\n");
                /* 真的落子，让后续的 board_check_winner 也能拿到一致状态。
                 * 但 winner 已经先在这里决出，主循环的 board_check_winner 会因为没五连而返回 NONE。
                 * 因此我们在这里直接 return false（结束）+ 自己打印结果。
                 */
                return false;
            }
        }
        if (!board_place(b, row, col, active)) {
            printf("Illegal (occupied/oob).\n"); continue;
        }
        char buf[8]; coord_to_str(row, col, buf);
        printf("Opponent plays %s = %s   (recorded)\n", label, buf);
        return true;
    }
}

void ui_main_loop(int my_color, int search_depth, int time_budget_ms) {
    Board b;
    board_init(&b);

    printf("=== gobang-engine -- Renju (national rules) ===\n");
    printf("AI plays:        %s\n", my_color == BLACK ? "BLACK (X)" : "WHITE (O)");
    printf("You relay for:   %s (the opponent)\n", my_color == BLACK ? "WHITE (O)" : "BLACK (X)");
    printf("Search depth:    %d\n", search_depth);
    printf("Time budget/mv:  %s\n", time_budget_ms > 0 ? "see ms below" : "unlimited");
    if (time_budget_ms > 0) printf("                 %d ms\n", time_budget_ms);
    printf("Flow follows spec section 6 phases 0-5: opening / swap / W4 / N-strikes / loop.\n");

    /* 阶段 1：开局 (3 手 + N) */
    int N = 5;
    if (!phase1_opening(&b, my_color, &N)) { printf("Quit at phase 1.\n"); return; }
    ui_draw_board(&b);

    /* 阶段 2：三手交换 */
    if (!phase2_swap(&b, &my_color)) {
        /* phase2_swap 返回 false 不意味着退出，只意味着没换。检查 EOF/quit 已被它打印 */
    }
    ui_draw_board(&b);

    /* 阶段 3-4：白4 + 五手N打。
     * 阶段 3 = 一次 phase5_one_turn（white to move）
     * 阶段 4 = phase4_n_strikes
     */
    /* 阶段 3：W4 */
    GameResult res = board_check_winner(&b);
    if (res == RESULT_NONE) {
        printf("\n=== Phase 3: White 4 (national rule 4) ===\n");
        if (!phase5_one_turn(&b, my_color, search_depth, time_budget_ms)) return;
        ui_draw_board(&b);
    }

    /* 阶段 4：B5 = 五手 N 打 */
    res = board_check_winner(&b);
    if (res == RESULT_NONE) {
        if (!phase4_n_strikes(&b, my_color, N, search_depth, time_budget_ms)) return;
        ui_draw_board(&b);
    }

    /* 阶段 5：正常对局循环 */
    printf("\n=== Phase 5: regular play (national rule 8/9 -- forbid + win check) ===\n");
    while (1) {
        res = board_check_winner(&b);
        if (res != RESULT_NONE) {
            /* P2-2 note: RESULT_WHITE_WIN_BY_BLACK_FORBID is currently never returned
             * by board_check_winner (国规 9.1 长连禁手 → 已直接映射成 RESULT_WHITE_WIN_NORMAL）。
             * 此分支保留作 future-proof：若后续禁手判负独立成"白胜（黑禁手）"语义，
             * board.c 可直接返回该值，UI 文案无需改。
             */
            const char *msg = (res == RESULT_BLACK_WIN)                 ? "BLACK wins!" :
                              (res == RESULT_WHITE_WIN_NORMAL)          ? "WHITE wins!" :
                              (res == RESULT_WHITE_WIN_BY_BLACK_FORBID) ? "WHITE wins (BLACK forbid)!" :
                                                                          "DRAW";
            printf("\n*** Game over: %s ***\n", msg);
            return;
        }
        if (!phase5_one_turn(&b, my_color, search_depth, time_budget_ms)) return;
        ui_draw_board(&b);
    }
}

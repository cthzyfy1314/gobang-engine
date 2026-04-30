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
 * 简化：让用户直接输 3 个坐标（manual 模式），不强制 26 种开局表。
 * AI 执黑：用户输自己想下的开局；AI 执白：用户输对方报的开局。
 * 返回：成功返回 true，board 已落 3 子；失败/quit 返回 false。
 */
static bool phase1_opening(Board *b, int my_color, int *out_N) {
    char line[128];
    int target_N = 5;

    printf("\n=== Phase 1: Opening (national rule 4) ===\n");
    if (my_color == BLACK) {
        printf("You play BLACK. Decide opening: enter 3 coords (B1 W2 B3) and N.\n");
        printf("  Format: <coord1> <coord2> <coord3> <N>     e.g. H8 I9 I7 5\n");
        printf("  Rule:   B1 must be H8 (tengen). B3 must be inside 5x5 around H8.\n");
        printf("  Note:   N = number of black-5 candidates you'll offer in phase 4.\n");
    } else {
        printf("You play WHITE. Enter the opening BLACK reported (3 coords + N).\n");
        printf("  Format: <coord1> <coord2> <coord3> <N>     e.g. H8 I9 I7 5\n");
    }

    while (1) {
        printf("[Phase 1] > ");
        fflush(stdout);
        if (!read_line(line, sizeof(line))) return false;
        if (line[0] == 'q' || line[0] == 'Q') return false;

        /* 解析 4 个 token */
        char t1[16], t2[16], t3[16];
        int N_in;
        if (sscanf(line, "%15s %15s %15s %d", t1, t2, t3, &N_in) != 4) {
            printf("Need 4 tokens: 3 coords + N. Try again.\n");
            continue;
        }
        int r1, c1, r2, c2, r3, c3;
        if (!parse_coord(t1, &r1, &c1) || !parse_coord(t2, &r2, &c2) || !parse_coord(t3, &r3, &c3)) {
            printf("Bad coordinate. Try again.\n");
            continue;
        }
        /* 验证 B1 = H8 (tengen) */
        if (r1 != 7 || c1 != 7) {
            printf("Rule 4: B1 must be H8 (tengen). Got %s. Try again.\n", t1);
            continue;
        }
        /* 验证 B3 在 5x5 内 (|dr|<=2 && |dc|<=2) */
        int dr = r3 - 7, dc = c3 - 7;
        if (dr < 0) dr = -dr; if (dc < 0) dc = -dc;
        if (dr > 2 || dc > 2) {
            printf("Rule 4: B3 must be within 5x5 around tengen. Got %s. Try again.\n", t3);
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
        char b1s[8], b2s[8], b3s[8];
        coord_to_str(r1, c1, b1s); coord_to_str(r2, c2, b2s); coord_to_str(r3, c3, b3s);
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
        printf("Your decision (1 = keep WHITE, 2 = swap to BLACK) > ");
        fflush(stdout);
        if (!read_line(line, sizeof(line))) return false;
        if (line[0] == '2') {
            *my_color = BLACK;
            printf(">>> You swapped. You now play BLACK. Tell opponent.\n");
            return true;
        }
        printf(">>> You kept WHITE. Tell opponent.\n");
        return false;
    } else {
        /* 我执黑：等对方报告是否换 */
        printf("Wait for opponent's decision and enter:\n");
        printf("  1 = opponent keeps WHITE (no swap)\n");
        printf("  2 = opponent SWAPS (you become WHITE)\n");
        printf("[Phase 2] > ");
        fflush(stdout);
        if (!read_line(line, sizeof(line))) return false;
        if (line[0] == '2') {
            *my_color = WHITE;
            printf(">>> Opponent swapped. You now play WHITE.\n");
            return true;
        }
        printf(">>> Opponent kept WHITE. You stay BLACK.\n");
        return false;
    }
}

/* === 阶段 4：五手 N 打（国规 7） ===
 * 黑方提供 N 个候选，白方挑一个作黑5。
 */
static bool phase4_n_strikes(Board *b, int my_color, int N, int search_depth) {
    char line[128];
    printf("\n=== Phase 4: 5th-move N-strikes (national rule 7) ===\n");

    if (my_color == BLACK) {
        /* 我执黑：引擎找 N 个候选，让用户报给对方 */
        Move cands[16];
        int scores[16];
        printf("[Engine computing %d candidate B5 moves...]\n", N);
        int got = search_find_n_distinct(b, search_depth, N, cands, scores);
        if (got == 0) {
            printf("No legal candidate. Resign.\n");
            return false;
        }
        printf(">>> %d candidates (report ALL of them to opponent):\n", got);
        for (int i = 0; i < got; i++) {
            char buf[8]; coord_to_str(cands[i].row, cands[i].col, buf);
            printf("    %d) %s   (score=%d)%s\n", i + 1, buf, scores[i], i == 0 ? "  [engine top pick]" : "");
        }
        printf("Enter the index (1..%d) opponent picked > ", got);
        fflush(stdout);
        if (!read_line(line, sizeof(line))) return false;
        int idx = atoi(line);
        if (idx < 1 || idx > got) { printf("Invalid index.\n"); return false; }
        Move pick = cands[idx - 1];
        char buf[8]; coord_to_str(pick.row, pick.col, buf);
        if (!board_place(b, pick.row, pick.col, BLACK)) { printf("B5 conflict.\n"); return false; }
        printf(">>> B5 = %s (chosen by opponent)\n", buf);
        return true;
    } else {
        /* 我执白：录入对方报的 N 个候选，引擎挑最不利黑方的一个 */
        printf("Opponent will report %d candidate B5 positions. Enter them one per line.\n", N);
        Move cands[16];
        int got = 0;
        while (got < N) {
            printf("Candidate %d/%d > ", got + 1, N);
            fflush(stdout);
            if (!read_line(line, sizeof(line))) return false;
            int r, c;
            if (!parse_coord(line, &r, &c)) { printf("Bad coord. Re-enter.\n"); continue; }
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
static bool phase5_one_turn(Board *b, int my_color, int search_depth) {
    char line[128];
    int active = b->side_to_move;
    int next_move_no = b->move_count + 1;
    char label[8]; move_label(next_move_no, active, label);

    if (active == my_color) {
        printf("[AI thinking %s @ d=%d ...]\n", label, search_depth);
        fflush(stdout);
        SearchResult r = search_best_move(b, search_depth);
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
        if (active == BLACK) warn_forbid_for_black(b, row, col);
        if (!board_place(b, row, col, active)) {
            printf("Illegal (occupied/oob).\n"); continue;
        }
        char buf[8]; coord_to_str(row, col, buf);
        printf("Opponent plays %s = %s   (recorded)\n", label, buf);
        return true;
    }
}

void ui_main_loop(int my_color, int search_depth) {
    Board b;
    board_init(&b);

    printf("=== gobang-engine — Renju (national rules) ===\n");
    printf("AI plays:        %s\n", my_color == BLACK ? "BLACK (X)" : "WHITE (O)");
    printf("You relay for:   %s (the opponent)\n", my_color == BLACK ? "WHITE (O)" : "BLACK (X)");
    printf("Search depth:    %d\n", search_depth);
    printf("Flow follows spec § 6 phases 0-5: opening / swap / W4 / N-strikes / loop.\n");

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
        if (!phase5_one_turn(&b, my_color, search_depth)) return;
        ui_draw_board(&b);
    }

    /* 阶段 4：B5 = 五手 N 打 */
    res = board_check_winner(&b);
    if (res == RESULT_NONE) {
        if (!phase4_n_strikes(&b, my_color, N, search_depth)) return;
        ui_draw_board(&b);
    }

    /* 阶段 5：正常对局循环 */
    printf("\n=== Phase 5: regular play (national rule 8/9 — forbid + win check) ===\n");
    while (1) {
        res = board_check_winner(&b);
        if (res != RESULT_NONE) {
            const char *msg = (res == RESULT_BLACK_WIN)                 ? "BLACK wins!" :
                              (res == RESULT_WHITE_WIN_NORMAL)          ? "WHITE wins!" :
                              (res == RESULT_WHITE_WIN_BY_BLACK_FORBID) ? "WHITE wins (BLACK forbid)!" :
                                                                          "DRAW";
            printf("\n*** Game over: %s ***\n", msg);
            return;
        }
        if (!phase5_one_turn(&b, my_color, search_depth)) return;
        ui_draw_board(&b);
    }
}

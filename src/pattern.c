/* src/pattern.c */
#define _CRT_SECURE_NO_WARNINGS
#include <string.h>
#include "pattern.h"

/* 评分表（spec § 3.2）—— 索引必须与 Pattern 枚举对齐 */
const int PATTERN_SCORE[PAT_COUNT] = {
    0,         /* PAT_NONE */
    10,        /* PAT_SLEEP_TWO */
    100,       /* PAT_OPEN_TWO */
    100,       /* PAT_SLEEP_THREE */
    1000,      /* PAT_OPEN_THREE */
    1000,      /* PAT_SIMPLE_FOUR */
    10000,     /* PAT_OPEN_FOUR */
    100000,    /* PAT_FIVE */
    900,       /* PAT_BROKEN_FOUR — 比冲四稍弱，能成5的潜在威胁 */
    800,       /* PAT_JUMP_OPEN_THREE — 比活三稍弱，会变活四 */
    9000       /* PAT_JUMP_OPEN_FOUR — 跟活四接近，填缝可直接成5 */
};

void pattern_count_in_line(const int8_t *line, int len, int color, PatternStats *out) {
    int i = 0;
    while (i < len) {
        if (line[i] != (int8_t)color) { i++; continue; }
        int j = i;
        while (j < len && line[j] == (int8_t)color) j++;
        int run_len = j - i;

        int left_open  = (i - 1 >= 0)  && (line[i - 1] == 0);
        int right_open = (j     <  len) && (line[j]     == 0);
        int open_count = left_open + right_open;

        if (run_len >= 5) {
            out->counts[PAT_FIVE]++;
        } else if (run_len == 4) {
            if (open_count == 2)      out->counts[PAT_OPEN_FOUR]++;
            else if (open_count == 1) out->counts[PAT_SIMPLE_FOUR]++;
        } else if (run_len == 3) {
            if (open_count == 2)      out->counts[PAT_OPEN_THREE]++;
            else if (open_count == 1) out->counts[PAT_SLEEP_THREE]++;
        } else if (run_len == 2) {
            if (open_count == 2)      out->counts[PAT_OPEN_TWO]++;
            else if (open_count == 1) out->counts[PAT_SLEEP_TWO]++;
        }
        /* run_len == 1 不计分（活一价值太低） */

        i = j;
    }
}

/* 跳/缝棋型识别：扫描固定大小的窗口，找带缝的潜在威胁。
 * 与 pattern_count_in_line 互补——后者只看连续段，这里看带空格的同色组合。
 *
 * 检测：
 *   5-window:  4 同色 + 1 空（且空不在窗口边）→ PAT_BROKEN_FOUR
 *              (XX_XX / X_XXX / XXX_X — 填缝成五)
 *   6-window: _XX_X_ / _X_XX_  → PAT_JUMP_OPEN_THREE
 *   7-window: _X_XXX_ / _XX_XX_ / _XXX_X_  → PAT_JUMP_OPEN_FOUR
 *
 * 排除重复：5-window 中如果同色子全连续（gap 在边），由原 pattern_count_in_line 处理，
 *           这里通过 "gap_pos 不能在 0 或 4" 来避免重复计数。
 */
static void count_broken_in_line(const int8_t *line, int len, int color, PatternStats *out) {
    int8_t op = (color == 1) ? 2 : 1;

    /* 5-window: 4 same + 1 gap (gap not on edge) */
    for (int i = 0; i + 5 <= len; i++) {
        int same = 0, empty = 0, gap_pos = -1, bad = 0;
        for (int k = 0; k < 5; k++) {
            int8_t v = line[i + k];
            if      (v == (int8_t)color) same++;
            else if (v == 0)             { empty++; gap_pos = k; }
            else                          { bad = 1; break; }
        }
        if (bad || same != 4 || empty != 1) continue;
        if (gap_pos == 0 || gap_pos == 4) continue;   /* 边上 gap 是连续段，交给 run-scan 处理 */
        out->counts[PAT_BROKEN_FOUR]++;
    }

    /* 6-window: _XX_X_ / _X_XX_ (jump open three) */
    for (int i = 0; i + 6 <= len; i++) {
        if (line[i] != 0 || line[i + 5] != 0) continue;
        /* 内部 4 格：3 same + 1 empty，且 same 不全连续 */
        int8_t a = line[i+1], b = line[i+2], c = line[i+3], d = line[i+4];
        /* _XX_X_ */
        if (a == (int8_t)color && b == (int8_t)color && c == 0 && d == (int8_t)color) {
            out->counts[PAT_JUMP_OPEN_THREE]++;
        }
        /* _X_XX_ */
        else if (a == (int8_t)color && b == 0 && c == (int8_t)color && d == (int8_t)color) {
            out->counts[PAT_JUMP_OPEN_THREE]++;
        }
    }

    /* 7-window: _X_XXX_ / _XX_XX_ / _XXX_X_ (jump open four) */
    for (int i = 0; i + 7 <= len; i++) {
        if (line[i] != 0 || line[i + 6] != 0) continue;
        int8_t a = line[i+1], b = line[i+2], c = line[i+3], d = line[i+4], e = line[i+5];
        int B = (int8_t)color;
        /* _X_XXX_ */
        if (a==B && b==0 && c==B && d==B && e==B) { out->counts[PAT_JUMP_OPEN_FOUR]++; continue; }
        /* _XX_XX_ */
        if (a==B && b==B && c==0 && d==B && e==B) { out->counts[PAT_JUMP_OPEN_FOUR]++; continue; }
        /* _XXX_X_ */
        if (a==B && b==B && c==B && d==0 && e==B) { out->counts[PAT_JUMP_OPEN_FOUR]++; continue; }
    }
    (void)op;  /* opponent color reserved for future use */
}

void pattern_count_for_color(const Board *b, int color, PatternStats *out) {
    int8_t line[BOARD_SIZE];

    /* 横向：每行扫一次 */
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) line[c] = (int8_t)b->cells[r][c];
        pattern_count_in_line(line, BOARD_SIZE, color, out);
        count_broken_in_line(line, BOARD_SIZE, color, out);
    }

    /* 竖向：每列扫一次 */
    for (int c = 0; c < BOARD_SIZE; c++) {
        for (int r = 0; r < BOARD_SIZE; r++) line[r] = (int8_t)b->cells[r][c];
        pattern_count_in_line(line, BOARD_SIZE, color, out);
        count_broken_in_line(line, BOARD_SIZE, color, out);
    }

    /* 主对角（左上→右下，r-c = const） */
    for (int diag = -(BOARD_SIZE - 1); diag <= BOARD_SIZE - 1; diag++) {
        int n = 0;
        for (int r = 0; r < BOARD_SIZE; r++) {
            int c = r - diag;
            if (c < 0 || c >= BOARD_SIZE) continue;
            line[n++] = (int8_t)b->cells[r][c];
        }
        if (n >= 5) {
            pattern_count_in_line(line, n, color, out);
            count_broken_in_line(line, n, color, out);
        }
    }

    /* 副对角（右上→左下，r+c = const） */
    for (int diag = 0; diag <= 2 * (BOARD_SIZE - 1); diag++) {
        int n = 0;
        for (int r = 0; r < BOARD_SIZE; r++) {
            int c = diag - r;
            if (c < 0 || c >= BOARD_SIZE) continue;
            line[n++] = (int8_t)b->cells[r][c];
        }
        if (n >= 5) {
            pattern_count_in_line(line, n, color, out);
            count_broken_in_line(line, n, color, out);
        }
    }
}

int pattern_evaluate(const Board *b) {
    PatternStats sB = {0}, sW = {0};
    pattern_count_for_color(b, BLACK, &sB);
    pattern_count_for_color(b, WHITE, &sW);

    int black_score = 0, white_score = 0;
    for (int p = 0; p < PAT_COUNT; p++) {
        black_score += sB.counts[p] * PATTERN_SCORE[p];
        white_score += sW.counts[p] * PATTERN_SCORE[p];
    }
    int diff = black_score - white_score;
    return (b->side_to_move == BLACK) ? diff : -diff;
}

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
    100000     /* PAT_FIVE */
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

void pattern_count_for_color(const Board *b, int color, PatternStats *out) {
    int8_t line[BOARD_SIZE];

    /* 横向：每行扫一次 */
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) line[c] = (int8_t)b->cells[r][c];
        pattern_count_in_line(line, BOARD_SIZE, color, out);
    }

    /* 竖向：每列扫一次 */
    for (int c = 0; c < BOARD_SIZE; c++) {
        for (int r = 0; r < BOARD_SIZE; r++) line[r] = (int8_t)b->cells[r][c];
        pattern_count_in_line(line, BOARD_SIZE, color, out);
    }

    /* 主对角（左上→右下，r-c = const） */
    for (int diag = -(BOARD_SIZE - 1); diag <= BOARD_SIZE - 1; diag++) {
        int n = 0;
        for (int r = 0; r < BOARD_SIZE; r++) {
            int c = r - diag;
            if (c < 0 || c >= BOARD_SIZE) continue;
            line[n++] = (int8_t)b->cells[r][c];
        }
        if (n >= 5) pattern_count_in_line(line, n, color, out);
    }

    /* 副对角（右上→左下，r+c = const） */
    for (int diag = 0; diag <= 2 * (BOARD_SIZE - 1); diag++) {
        int n = 0;
        for (int r = 0; r < BOARD_SIZE; r++) {
            int c = diag - r;
            if (c < 0 || c >= BOARD_SIZE) continue;
            line[n++] = (int8_t)b->cells[r][c];
        }
        if (n >= 5) pattern_count_in_line(line, n, color, out);
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

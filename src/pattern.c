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

        if (run_len >= 5) {
            out->counts[PAT_FIVE]++;
        }
        /* 后续 task 在此处加 4/3/2 的分支 */

        i = j;
    }
}

/* stub —— Task 7 实现 */
void pattern_count_for_color(const Board *b, int color, PatternStats *out) {
    (void)b; (void)color; (void)out;
}

/* stub —— Task 8 实现 */
int pattern_evaluate(const Board *b) {
    (void)b;
    return 0;
}

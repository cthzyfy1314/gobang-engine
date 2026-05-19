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
    9000,      /* PAT_JUMP_OPEN_FOUR — 跟活四接近，填缝可直接成5 */
    300,       /* PAT_PROTO_THREE — 原活三：__XX_ / _XX__ / __XX__，一手成活三 */
    900        /* PAT_SLEEP_JUMP_FOUR — 眠跳四：一端被堵的 X_XXX / XXX_X，填缝成 5 */
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
 * P0-2 dedup：pattern_count_in_line 已经计入所有连续段（含 3-/4-/5-连），
 * 本函数只计入 run-scan 看不见的"独立带缝威胁"，并对嵌入 3-consec 重复算的
 * OPEN_THREE 做扣减。
 *
 * 规则：
 *   5-window 只取 gap_pos==2 (XX_XX) —— split-four，run-scan 完全看不见
 *     gap_pos=1 (X_XXX) / gap_pos=3 (XXX_X) 的内 3-consec 已被 run-scan 算 3，跳过
 *   6-window _XX_X_ / _X_XX_ —— jump open three，3-consec 不存在所以无重复
 *   7-window _X_XXX_ / _XXX_X_ —— jump open four，减 1 OPEN_THREE 去重内 3-consec
 *           _XX_XX_ —— 3-consec 不存在所以无重复
 */
static void count_broken_in_line(const int8_t *line, int len, int color, PatternStats *out) {
    int8_t op = (color == 1) ? 2 : 1;
    int B = (int8_t)color;

    /* 5-window XX_XX：4 same + gap at center (gap_pos=2) */
    for (int i = 0; i + 5 <= len; i++) {
        if (line[i]==B && line[i+1]==B && line[i+2]==0 && line[i+3]==B && line[i+4]==B) {
            out->counts[PAT_BROKEN_FOUR]++;
        }
    }

    /* 6-window: _XX_X_ / _X_XX_ (jump open three) */
    for (int i = 0; i + 6 <= len; i++) {
        if (line[i] != 0 || line[i + 5] != 0) continue;
        int8_t a = line[i+1], b = line[i+2], c = line[i+3], d = line[i+4];
        if ((a==B && b==B && c==0 && d==B) ||
            (a==B && b==0 && c==B && d==B)) {
            out->counts[PAT_JUMP_OPEN_THREE]++;
            /* Dedup: the embedded XX subrun (双端开放) was counted as OPEN_TWO
             * by pattern_count_in_line — subtract it. */
            if (out->counts[PAT_OPEN_TWO] > 0) out->counts[PAT_OPEN_TWO]--;
        }
    }

    /* 7-window: _X_XXX_ / _XX_XX_ / _XXX_X_ (jump open four)
     * For _X_XXX_ and _XXX_X_, the embedded 3-consec was already counted by
     * run-scan as OPEN_THREE; decrement to avoid double-count.
     */
    for (int i = 0; i + 7 <= len; i++) {
        if (line[i] != 0 || line[i + 6] != 0) continue;
        int8_t a = line[i+1], b = line[i+2], c = line[i+3], d = line[i+4], e = line[i+5];
        /* _X_XXX_ : 3-consec at line[i+3..i+5] → already OPEN_THREE */
        if (a==B && b==0 && c==B && d==B && e==B) {
            out->counts[PAT_JUMP_OPEN_FOUR]++;
            out->counts[PAT_OPEN_THREE]--;
            continue;
        }
        /* _XX_XX_ : no 3-consec, but two XX subruns each counted as OPEN_TWO
         * by pattern_count_in_line — subtract both. */
        if (a==B && b==B && c==0 && d==B && e==B) {
            out->counts[PAT_JUMP_OPEN_FOUR]++;
            if (out->counts[PAT_OPEN_TWO] >= 2) out->counts[PAT_OPEN_TWO] -= 2;
            else out->counts[PAT_OPEN_TWO] = 0;
            continue;
        }
        /* _XXX_X_ : 3-consec at line[i+1..i+3] → already OPEN_THREE */
        if (a==B && b==B && c==B && d==0 && e==B) {
            out->counts[PAT_JUMP_OPEN_FOUR]++;
            out->counts[PAT_OPEN_THREE]--;
            continue;
        }
    }

    /* Sleep jump-four: 5-cell core X_XXX or XXX_X, with at least one outer end
     * BLOCKED (boundary or opponent). Filling the single gap → FIVE, so threat
     * is similar to BROKEN_FOUR. JUMP_OPEN_FOUR already covered the both-ends-
     * empty case; here we cover one-or-both blocked.
     *
     * Dedup the embedded 3-consec (XXX) which pattern_count_in_line already
     * counted as OPEN_THREE or SLEEP_THREE depending on the 3-consec's own
     * boundary (its inner neighbor is the gap = empty, outer neighbor is the
     * outer end of the 5-window).
     */
    for (int i = 0; i + 5 <= len; i++) {
        int8_t a = line[i], b = line[i+1], c = line[i+2], d = line[i+3], e = line[i+4];
        int left_pos  = i - 1;
        int right_pos = i + 5;
        int left_empty  = (left_pos  >= 0)  && (line[left_pos]  == 0);
        int right_empty = (right_pos <  len) && (line[right_pos] == 0);
        /* Sleep variant: NOT both ends empty (JUMP_OPEN_FOUR handles both-empty) */
        if (left_empty && right_empty) continue;

        /* X_XXX core: 3-consec at line[i+2..i+4] (right side of window).
         * The 3-consec's left inner neighbor is line[i+1]=0 (gap).
         * Its right outer neighbor is line[i+5] = right_pos's cell.
         * → 3-consec was OPEN_THREE iff right_empty, else SLEEP_THREE. */
        if (a==B && b==0 && c==B && d==B && e==B) {
            out->counts[PAT_SLEEP_JUMP_FOUR]++;
            if (right_empty) {
                if (out->counts[PAT_OPEN_THREE] > 0) out->counts[PAT_OPEN_THREE]--;
            } else {
                if (out->counts[PAT_SLEEP_THREE] > 0) out->counts[PAT_SLEEP_THREE]--;
            }
            continue;
        }
        /* XXX_X core: 3-consec at line[i..i+2] (left side of window).
         * Its left outer neighbor is line[i-1] = left_pos's cell.
         * Its right inner neighbor is line[i+3]=0 (gap).
         * → 3-consec was OPEN_THREE iff left_empty, else SLEEP_THREE. */
        if (a==B && b==B && c==B && d==0 && e==B) {
            out->counts[PAT_SLEEP_JUMP_FOUR]++;
            if (left_empty) {
                if (out->counts[PAT_OPEN_THREE] > 0) out->counts[PAT_OPEN_THREE]--;
            } else {
                if (out->counts[PAT_SLEEP_THREE] > 0) out->counts[PAT_SLEEP_THREE]--;
            }
            continue;
        }
    }

    /* Proto-open-three: 2-连 with both immediates open AND at least one side
     * has 2+ consecutive empties beyond the immediate (i.e. can grow into
     * _XXX_ in one move).
     *
     * 我们只在 exactly-2 的 XX-run 上 fire（不在 3-/4-/5-run 上 fire，否则
     * 与 OPEN_THREE/SIMPLE_FOUR/OPEN_FOUR/FIVE 的计数语义会混乱）。
     * Window patterns matched (per XX-run at positions p, p+1):
     *   - line[p-1]==0 AND line[p+2]==0           (run has both immediates open)
     *   - AND ( (p-2 >= 0 AND line[p-2]==0)  OR  (p+3 < len AND line[p+3]==0) )
     *   - AND p-2 != X  AND p+3 != X (already implied if extension empty side)
     *   - run is exactly 2 (line[p-2]!=X via we don't extend run; same for p+3)
     *
     * Dedup (option b): when proto-three fires, decrement the OPEN_TWO that
     * pattern_count_in_line already counted for this same run (exactly 1).
     *
     * Concrete window patterns this matches (5/6-window views, '?' = any non-X):
     *   _XX__   (5-window, no left extension OR left-blocked by X/edge)
     *   __XX_   (5-window, no right extension)
     *   __XX__  (6-window, both sides extendable — same proto-three counted once)
     */
    {
        int p = 0;
        while (p < len) {
            if (line[p] != B) { p++; continue; }
            int q = p;
            while (q < len && line[q] == B) q++;
            int rlen = q - p;
            if (rlen == 2) {
                int left_open  = (p - 1 >= 0)  && (line[p - 1] == 0);
                int right_open = (q     < len) && (line[q]     == 0);
                if (left_open && right_open) {
                    int left_ext  = (p - 2 >= 0)  && (line[p - 2] == 0);
                    int right_ext = (q + 1 < len) && (line[q + 1] == 0);
                    if (left_ext || right_ext) {
                        out->counts[PAT_PROTO_THREE]++;
                        if (out->counts[PAT_OPEN_TWO] > 0) out->counts[PAT_OPEN_TWO]--;
                    }
                }
            }
            p = q;
        }
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

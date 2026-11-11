/* src/forbid.c — Renju 黑方禁手判定（国规）
 *
 * 定义（均针对"假设在 (row,col) 落黑"之后的局面）：
 *   五连   = 恰好 5 连。出现五连则无禁手（国规 9.2-c 五连优先）。
 *   长连   = ≥6 连。
 *   四     = 再落一手可成"恰好五连"的 4 子组合（填上后成长连的不算四）。
 *            活四 _XXXX_ 两个成五点共享同一组 4 子，只算一个四。
 *   活三   = 再落一手可成"活四"的 3 子组合，且那一手本身不是禁手。
 *            活四 = _XXXX_ 且两个空端填上都恰好五连（空端外侧不能是黑子）。
 *   四四   = 四的总数 ≥2（可在同一条线上）；三三 = 有活三的方向数 ≥2。
 */
#define _CRT_SECURE_NO_WARNINGS
#include <string.h>
#include "forbid.h"

/* 4 个方向：横 / 竖 / 主斜 / 副斜 */
static const int FORBID_DIRS[4][2] = { {0,1}, {1,0}, {1,1}, {1,-1} };

typedef uint8_t Grid[BOARD_SIZE][BOARD_SIZE];

/* 提取以 (cr,cc) 为中心、沿 (dr,dc) 方向的 11 长度居中线。
 * line[5] 为中心点，强制设为 BLACK（=1）以"假装"落黑。越界用 -1 表示。 */
static void extract_centered(Grid g, int cr, int cc, int dr, int dc, int8_t line[11]) {
    for (int k = -5; k <= 5; k++) {
        int r = cr + k * dr, c = cc + k * dc;
        if (k == 0)                      line[5] = 1;
        else if (board_in_bounds(r, c))  line[k + 5] = (int8_t)g[r][c];
        else                             line[k + 5] = -1;
    }
}

/* 包含 center 的连续 BLACK 段长度 */
static int center_run_length(const int8_t *line) {
    int len = 1;
    for (int i = 4; i >= 0; i--) { if (line[i] == 1) len++; else break; }
    for (int i = 6; i <= 10; i++) { if (line[i] == 1) len++; else break; }
    return len;
}

/* 该方向上经过 center 的"四"的数量。
 * 5-window 内 4 黑 1 空，且填空后恰好五连（window 两侧紧邻不能是黑子，否则成长连）。
 * 用 4 子位置 bitmask 去重：活四的两个 window 共享同一组 4 子 → 1 个四；
 * X_XXX_X 之类 4 子组合不同 → 2 个四（同线四四）。 */
static int count_fours_through_center(const int8_t *line) {
    int seen_masks[8];
    int n_seen = 0;
    for (int i = 1; i <= 5; i++) {
        int blacks = 0, empties = 0, invalid = 0;
        int mask = 0;
        for (int k = 0; k < 5; k++) {
            int v = line[i + k];
            if (v == 1) { blacks++; mask |= (1 << (i + k)); }
            else if (v == 0) empties++;
            else { invalid = 1; break; }
        }
        if (invalid) continue;
        if (blacks != 4 || empties != 1) continue;
        if (line[i - 1] == 1) continue;                  /* 填上成长连，不是四 */
        if (i + 5 <= 10 && line[i + 5] == 1) continue;

        int duplicate = 0;
        for (int s = 0; s < n_seen; s++) {
            if (seen_masks[s] == mask) { duplicate = 1; break; }
        }
        if (!duplicate) seen_masks[n_seen++] = mask;
    }
    return n_seen;
}

/* 返回 bitmask：line 上哪些空位 k 落黑后，形成经过 center 的活四
 * （_XXXX_，两空端外侧都不是黑子）。非 0 即该方向存在"几何活三"；
 * 是否为真活三还需调用方验证成四点本身合法。 */
static int open_four_points_through_center(const int8_t *line) {
    int pts = 0;
    for (int k = 2; k <= 8; k++) {
        if (line[k] != 0) continue;
        int8_t s[11];
        memcpy(s, line, 11);
        s[k] = 1;
        for (int i = 1; i <= 4; i++) {          /* 4 子占 i+1..i+4，必含 center=5 */
            if (k < i + 1 || k > i + 4) continue;
            if (s[i] != 0 || s[i + 5] != 0) continue;
            if (s[i+1] != 1 || s[i+2] != 1 || s[i+3] != 1 || s[i+4] != 1) continue;
            if (s[i - 1] == 1) continue;                    /* 该端填上成长连 */
            if (i + 6 <= 10 && s[i + 6] == 1) continue;
            pts |= 1 << k;
        }
    }
    return pts;
}

/* 在 g 上判定 (row,col) 落黑是否禁手。g[row][col] 必须为 EMPTY。
 * 递归：活三的成四点必须自身合法（每层多落一子，深度天然有界）。 */
static ForbidType check_on_grid(Grid g, int row, int col) {
    int8_t line[4][11];
    int skip[4] = {0};
    int five = 0, overline = 0, fours = 0, three_dirs = 0;

    for (int d = 0; d < 4; d++) {
        extract_centered(g, row, col, FORBID_DIRS[d][0], FORBID_DIRS[d][1], line[d]);
        int run = center_run_length(line[d]);
        if (run == 5)      { five++;     skip[d] = 1; }
        else if (run >= 6) { overline++; skip[d] = 1; }
    }
    if (five) return FORBID_NONE;            /* 国规 9.2-c：五连优先 */
    if (overline) return FORBID_OVERLINE;

    for (int d = 0; d < 4; d++)
        if (!skip[d]) fours += count_fours_through_center(line[d]);
    if (fours >= 2) return FORBID_DOUBLE_FOUR;

    for (int d = 0; d < 4; d++) {
        if (skip[d]) continue;
        int pts = open_four_points_through_center(line[d]);
        if (!pts) continue;
        int ok = 0;
        g[row][col] = BLACK;
        for (int k = 2; k <= 8 && !ok; k++) {
            if (!(pts & (1 << k))) continue;
            int r = row + (k - 5) * FORBID_DIRS[d][0];
            int c = col + (k - 5) * FORBID_DIRS[d][1];
            if (check_on_grid(g, r, c) == FORBID_NONE) ok = 1;
        }
        g[row][col] = EMPTY;
        three_dirs += ok;
        if (three_dirs >= 2) return FORBID_DOUBLE_THREE;
    }
    return FORBID_NONE;
}

ForbidType forbid_check_black(const Board *b, int row, int col) {
    if (!board_in_bounds(row, col)) return FORBID_NONE;
    if (b->cells[row][col] != EMPTY) return FORBID_NONE;
    Grid g;
    memcpy(g, b->cells, sizeof g);
    return check_on_grid(g, row, col);
}

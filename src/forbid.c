/* src/forbid.c — Renju 黑方禁手判定（简化版） */
#define _CRT_SECURE_NO_WARNINGS
#include "forbid.h"

/* 4 个方向：横 / 竖 / 主斜 / 副斜 */
static const int FORBID_DIRS[4][2] = { {0,1}, {1,0}, {1,1}, {1,-1} };

/* 提取以 (cr,cc) 为中心、沿 (dr,dc) 方向的 11 长度居中线。
 * line[5] 为中心点，强制设为 BLACK（=1）以"假装"落黑。
 * 越界用 -1 表示。
 */
static void extract_centered(const Board *b, int cr, int cc, int dr, int dc, int8_t line[11]) {
    for (int k = -5; k <= 5; k++) {
        if (k == 0) {
            line[5] = 1;  /* center = 假定 BLACK */
            continue;
        }
        int r = cr + k * dr, c = cc + k * dc;
        if (board_in_bounds(r, c)) line[k + 5] = (int8_t)b->cells[r][c];
        else                        line[k + 5] = -1;
    }
}

/* 包含 center 的连续 BLACK 段长度（向左右扩展直到非 BLACK） */
static int center_run_length(const int8_t *line) {
    int len = 1;
    for (int i = 4; i >= 0; i--) { if (line[i] == 1) len++; else break; }
    for (int i = 6; i <= 10; i++) { if (line[i] == 1) len++; else break; }
    return len;
}

/* 统计该方向（line）上经过 center 的"四"棋型数（活四或冲四，含跳冲四）。
 * 返回 ≥ 1 表示该方向至少有一个四。简化：每个方向最多记 1 个有效四。
 */
static int has_four_through_center(const int8_t *line) {
    /* 遍历所有 5 长度子串 line[i..i+4]，i 范围使窗口包含 center=5 即 i in [1,5] */
    for (int i = 1; i <= 5; i++) {
        int blacks = 0, empties = 0, invalid = 0;
        for (int k = 0; k < 5; k++) {
            int v = line[i + k];
            if (v == 1) blacks++;
            else if (v == 0) empties++;
            else { invalid = 1; break; }
        }
        if (invalid) continue;
        if (blacks == 4 && empties == 1) {
            /* 找到一个"四"形式（5 长度，4 黑 + 1 空，无对手或边界）*/
            int empty_pos = -1;
            for (int k = 0; k < 5; k++) if (line[i + k] == 0) { empty_pos = k; break; }

            if (empty_pos == 0 || empty_pos == 4) {
                /* 空在端：4 子连续 → 简化版不严格区分活四/冲四，统一记一个"四" */
                return 1;
            } else {
                /* 跳冲四（空在中间）*/
                return 1;
            }
        }
    }
    return 0;
}

/* 统计该方向（line）上经过 center 的"形式上活三"数。
 * 形式：
 *   _XXX_   连续活三（6 长度窗口 0,1,1,1,0,?）
 *   _XX_X_  / _X_XX_  跳活三（6 长度窗口）
 * 返回该方向匹配数（最多 2，但通常 ≤ 1）。
 *
 * 索引说明：
 *   连续 _XXX_：3 黑子在 i+1..i+3，"center 必须在 3 黑子之中" → 5 ∈ [i+1, i+3]
 *   跳 _XX_X_ / _X_XX_：黑子分布在 i+1..i+4 范围内，"center 在黑子区间" → 5 ∈ [i+1, i+4]
 */
static int count_open_threes_through_center(const int8_t *line) {
    int count = 0;
    /* 形式 1: 连续活三 _XXX__ 或 __XXX_。要求 center 在 3 黑子之中 */
    for (int i = 0; i <= 5; i++) {
        if (i + 5 > 10) break;
        if (line[i] == 0 && line[i+1] == 1 && line[i+2] == 1 && line[i+3] == 1
            && line[i+4] == 0) {
            /* center=5 必须在 3 黑子位置 i+1..i+3 之中 */
            if (5 < i + 1 || 5 > i + 3) continue;
            /* 排除"被冲四吞噬"的情形：左右扩展若是 BLACK 则该 _XXX_ 实际是更长段的子集，跳过 */
            int outer_left  = (i - 1 >= 0)  ? line[i - 1]  : -1;
            int outer_right = (i + 5 <= 10) ? line[i + 5] : -1;
            if (outer_left == 1 || outer_right == 1) continue;
            count++;
        }
    }
    /* 形式 2: 跳活三 _XX_X_ 或 _X_XX_（6 长度窗口）*/
    for (int i = 0; i <= 4; i++) {
        if (i + 5 > 10) continue;
        /* _XX_X_ */
        if (line[i] == 0 && line[i+1] == 1 && line[i+2] == 1
            && line[i+3] == 0 && line[i+4] == 1 && line[i+5] == 0) {
            if (5 >= i+1 && 5 <= i+4) count++;
        }
        /* _X_XX_ */
        else if (line[i] == 0 && line[i+1] == 1 && line[i+2] == 0
                 && line[i+3] == 1 && line[i+4] == 1 && line[i+5] == 0) {
            if (5 >= i+1 && 5 <= i+4) count++;
        }
    }
    return count;
}

ForbidType forbid_check_black(const Board *b, int row, int col) {
    if (!board_in_bounds(row, col)) return FORBID_NONE;
    if (b->cells[row][col] != EMPTY) return FORBID_NONE;

    int five_count = 0;
    int overline_count = 0;
    int four_dirs = 0;
    int three_dirs = 0;

    for (int d = 0; d < 4; d++) {
        int8_t line[11];
        extract_centered(b, row, col, FORBID_DIRS[d][0], FORBID_DIRS[d][1], line);

        int run = center_run_length(line);
        if (run == 5) {
            five_count++;
            continue;  /* 五连方向不再分析其他棋型 */
        }
        if (run >= 6) {
            overline_count++;
            continue;
        }

        /* 非五连方向：统计四 / 三 */
        if (has_four_through_center(line)) four_dirs++;
        if (count_open_threes_through_center(line) >= 1) three_dirs++;
    }

    /* 国规 9.2-c：五连优先 */
    if (five_count > 0) return FORBID_NONE;

    if (overline_count > 0) return FORBID_OVERLINE;
    if (four_dirs >= 2)    return FORBID_DOUBLE_FOUR;
    if (three_dirs >= 2)   return FORBID_DOUBLE_THREE;

    return FORBID_NONE;
}

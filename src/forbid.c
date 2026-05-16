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

/* 统计该方向（line）上经过 center 的"四"棋型 *数量*（活四或冲四，含跳冲四）。
 *
 * 同方向可能存在多个独立的"四"（threat 位置不同），都要计入。
 * 用 threat position（5-window 内 empty 的位置）去重，避免同一个 four 被多个
 * 重叠 5-window 重复算（典型：相邻两个 window 共享 4 个 black 子，threat 位置相同）。
 *
 * 这个 count 让 forbid_check_black 能识别"同一方向上的双四禁手"。
 * 老版本返回 boolean 漏掉这种 case（2026-05-16 修复）。
 */
static int count_fours_through_center(const int8_t *line) {
    /* 用 4 个 black 子位置组合（bitmask）作为 four 的指纹去重：
     *   活四 _XXXX_：相邻 2 个 5-window 共享同一组 4 子 → 1 个 four
     *   跳冲四组合：4 子位置不同 → distinct fours
     * 老版按 threat 位置去重把活四误算 2 → 引起 single_four 测试回归。
     */
    int seen_masks[8] = {0};
    int n_seen = 0;
    int count = 0;
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

        int duplicate = 0;
        for (int s = 0; s < n_seen; s++) {
            if (seen_masks[s] == mask) { duplicate = 1; break; }
        }
        if (!duplicate) {
            seen_masks[n_seen++] = mask;
            count++;
        }
    }
    return count;
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
    int total_fours = 0;     /* 跨所有方向累加"四"的总数（同方向多个也算）*/
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

        /* 非五连方向：累加四的个数 / 累加方向上"三"的有/无 */
        total_fours += count_fours_through_center(line);
        if (count_open_threes_through_center(line) >= 1) three_dirs++;
    }

    /* 国规 9.2-c：五连优先 */
    if (five_count > 0) return FORBID_NONE;

    if (overline_count > 0) return FORBID_OVERLINE;
    if (total_fours >= 2)  return FORBID_DOUBLE_FOUR;
    if (three_dirs >= 2)   return FORBID_DOUBLE_THREE;

    return FORBID_NONE;
}

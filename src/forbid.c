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

/* 真活三判定：以 line 居中（center=line[5]=BLACK 假定）为前提，
 * 是否存在经过 center 的"真活三"——即可在 1 手内变成"真活四"（连续 4 子双端开放）。
 *
 * 算法（双层模拟）：
 *   1. 枚举本方向上经过 center 的所有"3 子黑棋组合"（含跳形）
 *      —— 形式 `_XXX_` 与 `_XX_X_` / `_X_XX_`
 *   2. 对每个形式活三组合，尝试在它的"延伸位"再落一手 BLACK
 *      —— 即 6 长度窗口内的两个 _ 位置之一，以及（对连续 _XXX_）内部不需要填
 *      —— 实际上：对每种形式，可以填的"成四位"是固定的几个 slot
 *   3. 落第二手后，调用 line_has_true_open_four 看是否形成 4 连双端开放
 *   4. 任一延伸位可形成 → 真活三
 *
 * 简化实现：遍历所有 line 上的 EMPTY 位置 (line[k]==0)，模拟在 k 处放 BLACK，
 *           看新 line 上是否出现真活四 + center 仍在该 four 中。这样省去枚举形式。
 */
static int is_true_open_three_through_center(const int8_t *line) {
    int len = 11;
    /* 必须先满足"形式活三"的存在（中心方向上的形式 _XXX_ / _XX_X_ / _X_XX_），
     * 否则即使有可成四的延伸位，也不是经过 center 的活三威胁。
     * 沿用旧的 count_open_threes_through_center 形式检测：找到 ≥1 个形式三再做延伸验证。
     */
    int has_formal_three = 0;
    /* 形式 1: 连续活三 */
    for (int i = 0; i <= len - 5; i++) {
        if (line[i] == 0 && line[i+1] == 1 && line[i+2] == 1 && line[i+3] == 1
            && line[i+4] == 0) {
            if (5 < i + 1 || 5 > i + 3) continue;
            int outer_left  = (i - 1 >= 0)  ? line[i - 1]  : -1;
            int outer_right = (i + 5 < len) ? line[i + 5] : -1;
            if (outer_left == 1 || outer_left == -1) continue;
            if (outer_right == 1 || outer_right == -1) continue;
            has_formal_three = 1;
            break;
        }
    }
    /* 形式 2: 跳活三 */
    if (!has_formal_three) {
        for (int i = 0; i <= len - 6; i++) {
            int8_t a = line[i], f = line[i+5];
            if (a != 0 || f != 0) continue;
            int8_t b = line[i+1], c = line[i+2], d = line[i+3], e = line[i+4];
            int hit = 0;
            if (b==1 && c==1 && d==0 && e==1) hit = 1;        /* _XX_X_ */
            else if (b==1 && c==0 && d==1 && e==1) hit = 1;   /* _X_XX_ */
            if (!hit) continue;
            if (5 < i + 1 || 5 > i + 4) continue;
            has_formal_three = 1;
            break;
        }
    }
    if (!has_formal_three) return 0;

    /* 延伸验证：尝试每个 EMPTY 位置 (line[k]==0) 落 BLACK，看是否出现真活四
     * 且新形成的 4 连包含 center=5（保证是"经过 center 的活四"，而非旁路活四）。
     */
    int8_t sim[11];
    for (int k = 0; k < len; k++) {
        if (line[k] != 0) continue;
        if (k == 5) continue;  /* center 已是 BLACK，跳过 */
        for (int j = 0; j < len; j++) sim[j] = line[j];
        sim[k] = 1;

        /* 检查 sim 上是否有真活四 _XXXX_，且该 4 子段包含 center=5 */
        for (int i = 0; i + 5 <= len; i++) {
            if (sim[i] != 0) continue;
            if (sim[i+1] != 1 || sim[i+2] != 1 || sim[i+3] != 1 || sim[i+4] != 1) continue;
            if (i + 5 >= len) continue;
            if (sim[i + 5] != 0) continue;
            /* 4 子段 [i+1..i+4] 必须包含 center=5 */
            if (5 < i + 1 || 5 > i + 4) continue;
            return 1;
        }
    }
    return 0;
}

/* 统计该方向（line）上经过 center 的"真活三"数。
 * 真活三 = 1 手内可形成"真活四"（连续 4 子双端开放）的活三。
 *
 * P1-2: 旧实现只做形式判定（`_XXX_` / `_XX_X_` / `_X_XX_`），现升级为递归式
 *       双层模拟：找到形式三后再尝试每个延伸位看能否成真活四。
 *
 * 注意：同一方向上通常 ≤ 1 真活三（多个形式三共享 center 极罕见），
 *       返回 0 / 1 即可。
 */
static int count_open_threes_through_center(const int8_t *line) {
    return is_true_open_three_through_center(line) ? 1 : 0;
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

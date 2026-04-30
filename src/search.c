/* src/search.c */
#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <limits.h>
#include "search.h"
#include "board.h"
#include "pattern.h"
#include "zobrist.h"
#include "forbid.h"

/* Move ordering helpers — 跨 ID 迭代保留 */
typedef struct {
    int8_t row, col;
} Killer;

static Killer _killers[SEARCH_MAX_PLY][2];
/* long long: 在 MSVC 32-bit long 下避免 (1LL << depth_left) 溢出 */
static long long _history[2][BOARD_SIZE * BOARD_SIZE];

static void clear_search_state(void) {
    for (int p = 0; p < SEARCH_MAX_PLY; p++) {
        _killers[p][0].row = _killers[p][0].col = -1;
        _killers[p][1].row = _killers[p][1].col = -1;
    }
    for (int c = 0; c < 2; c++)
        for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++)
            _history[c][i] = 0;
}

static int is_killer(int ply, int row, int col) {
    return (_killers[ply][0].row == row && _killers[ply][0].col == col) ||
           (_killers[ply][1].row == row && _killers[ply][1].col == col);
}

static void record_killer(int ply, int row, int col) {
    if (is_killer(ply, row, col)) return;
    _killers[ply][1] = _killers[ply][0];
    _killers[ply][0].row = (int8_t)row;
    _killers[ply][0].col = (int8_t)col;
}

void search_reset(void) {
    tt_clear();
    clear_search_state();
}

int search_find_n_distinct(Board *b, int depth, int n_want, Move *out, int *out_scores) {
    /* 入口同步 hash，清搜索状态 */
    b->zobrist_hash = zobrist_compute(b);
    tt_clear();
    clear_search_state();

    Move moves[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(b, moves);
    if (n == 0) return 0;

    /* 对每个候选 move 在固定深度搜索得分 */
    int scores[SEARCH_MAX_MOVES];
    long nodes_dummy = 0;
    int valid_n = 0;
    int valid_idx[SEARCH_MAX_MOVES];

    int alpha = -SEARCH_INF, beta = SEARCH_INF;
    for (int i = 0; i < n; i++) {
        int color = b->side_to_move;
        if (color == BLACK && b->forbid_enabled) {
            if (forbid_check_black(b, moves[i].row, moves[i].col) != FORBID_NONE) continue;
        }
        if (!board_place(b, moves[i].row, moves[i].col, color)) continue;
        int score = -alphabeta(b, 1, depth - 1, -beta, -alpha, &nodes_dummy);
        board_undo(b);
        scores[valid_n] = score;
        valid_idx[valid_n] = i;
        valid_n++;
    }

    /* 按 score 降序选前 n_want，输出位置 */
    int picked = 0;
    int used[SEARCH_MAX_MOVES] = {0};
    while (picked < n_want && picked < valid_n) {
        int best = -1;
        for (int i = 0; i < valid_n; i++) {
            if (used[i]) continue;
            if (best == -1 || scores[i] > scores[best]) best = i;
        }
        if (best == -1) break;
        used[best] = 1;
        out[picked] = moves[valid_idx[best]];
        if (out_scores) out_scores[picked] = scores[best];
        picked++;
    }
    return picked;
}

/* 4/30 起：转发给 pattern_evaluate（替换 4/29 的中央倾向 placeholder） */
int search_evaluate(const Board *b) {
    return pattern_evaluate(b);
}

int search_generate_neighbor_moves(const Board *b, Move *out) {
    if (b->move_count == 0) {
        out[0].row = 7;
        out[0].col = 7;
        out[0].color = (int8_t)b->side_to_move;
        return 1;
    }

    /* "邻近 2 圈"标记表 */
    uint8_t mark[BOARD_SIZE][BOARD_SIZE] = {{0}};

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (b->cells[r][c] == EMPTY) continue;
            for (int dr = -2; dr <= 2; dr++) {
                for (int dc = -2; dc <= 2; dc++) {
                    int nr = r + dr, nc = c + dc;
                    if (!board_in_bounds(nr, nc)) continue;
                    if (b->cells[nr][nc] != EMPTY) continue;
                    mark[nr][nc] = 1;
                }
            }
        }
    }

    int count = 0;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (mark[r][c] && count < SEARCH_MAX_MOVES) {
                out[count].row = (int8_t)r;
                out[count].col = (int8_t)c;
                out[count].color = (int8_t)b->side_to_move;
                count++;
            }
        }
    }
    return count;
}

/* αβ minimax 递归（negamax 风格简化版）。
 * 返回值是从"当前到走方"角度的评分。
 *   side_to_move = 接下来要走的玩家
 *   alpha/beta = 当前最佳可行下界/上界
 *   depth_left = 还要往深搜几层
 *   nodes = 节点计数累加器
 */
static int alphabeta(Board *b, int ply, int depth_left, int alpha, int beta, long *nodes) {
    (*nodes)++;

    if (ply >= SEARCH_MAX_PLY) return search_evaluate(b);

    /* TT 查询 */
    const TTEntry *e = tt_get(b->zobrist_hash);
    if (e != NULL && e->depth >= depth_left) {
        if (e->flag == TT_FLAG_EXACT) return e->score;
        if (e->flag == TT_FLAG_LOWER && e->score >= beta)  return e->score;
        if (e->flag == TT_FLAG_UPPER && e->score <= alpha) return e->score;
    }

    /* 终局检查
     * 注意：进入此函数时 b->side_to_move 已是"上一手对方落子后翻转"的玩家，
     * 即"刚刚落子"的玩家是 (side_to_move == BLACK ? WHITE : BLACK)。
     * 若 winner == 刚刚落子者 → 返回 +SEARCH_INF（对当前 side 而言，对手赢了 = 自己输 = 负值）
     * 若 winner == 当前 side → 几乎不可能（对手刚下完但反而是自己赢？只发生于 forbid 反向判罚），仍正确处理
     */
    GameResult res = board_check_winner(b);
    if (res == RESULT_BLACK_WIN) {
        return (b->side_to_move == BLACK) ? SEARCH_INF - b->move_count : -(SEARCH_INF - b->move_count);
    }
    if (res == RESULT_WHITE_WIN_NORMAL || res == RESULT_WHITE_WIN_BY_BLACK_FORBID) {
        return (b->side_to_move == WHITE) ? SEARCH_INF - b->move_count : -(SEARCH_INF - b->move_count);
    }
    if (res == RESULT_DRAW) return 0;

    if (depth_left == 0) return search_evaluate(b);

    Move moves[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(b, moves);
    if (n == 0) return search_evaluate(b);

    /* Move ordering: TT best > killer1 > killer2 > history score 降序 */
    int8_t tt_r = (e != NULL) ? e->best_row : (int8_t)-1;
    int8_t tt_c = (e != NULL) ? e->best_col : (int8_t)-1;
    int color_idx = b->side_to_move - 1;
    long long priority[SEARCH_MAX_MOVES];
    for (int i = 0; i < n; i++) {
        int r = moves[i].row, c = moves[i].col;
        long long score;
        if (r == tt_r && c == tt_c)        score = 1000000000LL;
        else if (is_killer(ply, r, c))     score = 100000000LL;
        else                                score = _history[color_idx][r * BOARD_SIZE + c];
        priority[i] = score;
    }
    /* 选择排序，n ≤ ~100 */
    for (int i = 0; i < n - 1; i++) {
        int best = i;
        for (int j = i + 1; j < n; j++) if (priority[j] > priority[best]) best = j;
        if (best != i) {
            Move tm = moves[i]; moves[i] = moves[best]; moves[best] = tm;
            long long tp = priority[i]; priority[i] = priority[best]; priority[best] = tp;
        }
    }

    int orig_alpha = alpha;
    int best_score = -SEARCH_INF;
    int best_r = -1, best_c = -1;
    for (int i = 0; i < n; i++) {
        int color = b->side_to_move;
        /* 国规黑禁手：排除黑方禁手位置（forbid_check_black 已处理"五连优先"）*/
        if (color == BLACK && b->forbid_enabled) {
            if (forbid_check_black(b, moves[i].row, moves[i].col) != FORBID_NONE) continue;
        }
        if (!board_place(b, moves[i].row, moves[i].col, color)) continue;
        int score = -alphabeta(b, ply + 1, depth_left - 1, -beta, -alpha, nodes);
        board_undo(b);

        if (score > best_score) {
            best_score = score;
            best_r = moves[i].row;
            best_c = moves[i].col;
        }
        if (best_score > alpha) alpha = best_score;
        if (alpha >= beta) {
            /* β 剪枝：记录 killer + history */
            record_killer(ply, moves[i].row, moves[i].col);
            /* clamp shift 避免 (1 << large) UB */
            int sh = depth_left < 30 ? depth_left : 30;
            _history[color - 1][moves[i].row * BOARD_SIZE + moves[i].col] += (1LL << sh);
            break;
        }
    }

    /* TT 写入：根据 alpha/beta 边界判定 flag */
    TTFlag flag;
    if (best_score <= orig_alpha)      flag = TT_FLAG_UPPER;
    else if (best_score >= beta)       flag = TT_FLAG_LOWER;
    else                                flag = TT_FLAG_EXACT;
    tt_put(b->zobrist_hash, depth_left, best_score, flag, best_r, best_c);

    return best_score;
}

SearchResult search_best_move(Board *b, int max_depth) {
    SearchResult result = { .best_move = { -1, -1, (int8_t)b->side_to_move },
                            .score = -SEARCH_INF, .nodes_searched = 0 };

    /* 入口同步 hash + 清 TT/killer/history。
     * TT 跨 search 调用复用是更优策略，但需要全局保证 hash 同步——
     * 当前测试直接改 cells 不调 board_place，所以保守地每次清表。
     * （ID 内部仍能享受 d=1→d=2→...逐层复用 TT 的加速。）
     */
    b->zobrist_hash = zobrist_compute(b);
    tt_clear();
    clear_search_state();

    Move moves[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(b, moves);
    if (n == 0) return result;

    /* 迭代加深：1 → max_depth */
    for (int d = 1; d <= max_depth; d++) {
        SearchResult cur = { .best_move = result.best_move,
                             .score = -SEARCH_INF, .nodes_searched = result.nodes_searched };
        int alpha = -SEARCH_INF, beta = SEARCH_INF;

        /* 顶层用 TT best 排在第一（PV-first）*/
        const TTEntry *root_tt = tt_get(b->zobrist_hash);
        if (root_tt && root_tt->best_row >= 0) {
            for (int i = 0; i < n; i++) {
                if (moves[i].row == root_tt->best_row && moves[i].col == root_tt->best_col) {
                    Move t = moves[0]; moves[0] = moves[i]; moves[i] = t;
                    break;
                }
            }
        }

        for (int i = 0; i < n; i++) {
            int color = b->side_to_move;
            /* 国规黑禁手：根节点也要排除禁手位置 */
            if (color == BLACK && b->forbid_enabled) {
                if (forbid_check_black(b, moves[i].row, moves[i].col) != FORBID_NONE) continue;
            }
            if (!board_place(b, moves[i].row, moves[i].col, color)) continue;
            int score = -alphabeta(b, 1, d - 1, -beta, -alpha, &cur.nodes_searched);
            board_undo(b);

            if (score > cur.score) {
                cur.score = score;
                cur.best_move = moves[i];
            }
            if (score > alpha) alpha = score;
        }

        result = cur;

        /* 早停：发现必胜（剩余分阈值）*/
        if (result.score > SEARCH_INF / 2) break;
    }
    return result;
}

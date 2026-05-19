/* src/search.c — αβ minimax 搜索 v2
 *
 * 主要升级（vs v1）：
 *   1. Tactical move ordering: threat-create / threat-defend 加大 priority
 *      history clamp + 周期 aging，避免溢出超过 TT-best
 *   2. VCF / VCT 专用搜索：先跑窄分支因子的强迫序列搜索，再 fallback αβ
 *   3. Time-budget ID + aspiration window：每层结束检查耗时；窗口 fail 时再扩
 *   4. TT 跨调用复用：不再 tt_clear()；killer/history aging
 *   5. Ghost-mate fix：循环前预计数合法 move，0 时直接 return eval（不写 TT）
 *   6. Mate score 与 eval 分离：SEARCH_INF=1e8，IS_MATE_SCORE macro
 *   7. 邻近候选扩展：2 圈方块 + 4 个方向各延伸 4 cell
 */
#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include "search.h"
#include "board.h"
#include "pattern.h"
#include "zobrist.h"
#include "forbid.h"

/* ============== Move ordering 状态（killer / history） ============== */
typedef struct {
    int8_t row, col;
} Killer;

static Killer _killers[SEARCH_MAX_PLY][2];
static long long _history[2][BOARD_SIZE * BOARD_SIZE];

/* History clamp：保证不会超过 1e11，远低于 TT-best / killer / threat 优先级 */
#define HISTORY_MAX 100000000000LL   /* 1e11 */

static void clear_killers_history(void) {
    for (int p = 0; p < SEARCH_MAX_PLY; p++) {
        _killers[p][0].row = _killers[p][0].col = -1;
        _killers[p][1].row = _killers[p][1].col = -1;
    }
    for (int c = 0; c < 2; c++)
        for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++)
            _history[c][i] = 0;
}

static void age_history(void) {
    for (int c = 0; c < 2; c++)
        for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++)
            _history[c][i] /= 2;
}

static int is_killer(int ply, int row, int col) {
    if (ply >= SEARCH_MAX_PLY) return 0;
    return (_killers[ply][0].row == row && _killers[ply][0].col == col) ||
           (_killers[ply][1].row == row && _killers[ply][1].col == col);
}

static void record_killer(int ply, int row, int col) {
    if (ply >= SEARCH_MAX_PLY) return;
    if (is_killer(ply, row, col)) return;
    _killers[ply][1] = _killers[ply][0];
    _killers[ply][0].row = (int8_t)row;
    _killers[ply][0].col = (int8_t)col;
}

void search_reset(void) {
    tt_clear();
    clear_killers_history();
}

/* ============== Time-budget helpers ============== */
static clock_t _deadline = 0;       /* 0 表示不限时 */
static int     _time_up  = 0;
static int     _budget_ms = 0;

static int check_time(void) {
    if (_deadline == 0) return 0;
    if (_time_up) return 1;
    if (clock() >= _deadline) { _time_up = 1; return 1; }
    return 0;
}

/* ============== 邻近候选生成（v2：2 圈 + 同向延伸 4 cell） ============== */
int search_generate_neighbor_moves(const Board *b, Move *out) {
    if (b->move_count == 0) {
        out[0].row = 7;
        out[0].col = 7;
        out[0].color = (int8_t)b->side_to_move;
        return 1;
    }

    uint8_t mark[BOARD_SIZE][BOARD_SIZE] = {{0}};

    /* 邻近 2 圈方块 */
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
            /* 4 个方向各延伸到 ≤4 cell（捕捉跳子威胁的延伸点） */
            static const int dirs[8][2] = {
                {0,1},{0,-1},{1,0},{-1,0},{1,1},{-1,-1},{1,-1},{-1,1}
            };
            for (int d = 0; d < 8; d++) {
                int dr = dirs[d][0], dc = dirs[d][1];
                for (int k = 3; k <= 4; k++) {  /* 3、4 cell 延伸（1/2 已由 2 圈覆盖） */
                    int nr = r + dr * k, nc = c + dc * k;
                    if (!board_in_bounds(nr, nc)) break;
                    if (b->cells[nr][nc] != EMPTY) break;
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

/* ============== 威胁检测 helpers ==============
 * 给定 (b, row, col, color)：把 color 临时放到 (row,col)，看是否形成 FIVE / OPEN_FOUR / SIMPLE_FOUR / OPEN_THREE
 * 然后撤回。不调 board_place（避免动 zobrist / history）。
 *
 * 返回威胁等级：
 *   3 = FIVE / OPEN_FOUR / SIMPLE_FOUR（"四级威胁" — 必应或制胜）
 *   2 = OPEN_THREE（"三级威胁"）
 *   0 = 无威胁
 */
static int classify_move_threat(Board *b, int row, int col, int color) {
    if (b->cells[row][col] != EMPTY) return 0;

    /* 直接计 8 个半线（向 4 方向各看 ≤5 子），构造长度 9 的 1D 线扫 pattern_count_in_line */
    static const int dirs[4][2] = { {0,1}, {1,0}, {1,1}, {1,-1} };
    int max_level = 0;

    b->cells[row][col] = (uint8_t)color;
    for (int d = 0; d < 4; d++) {
        int dr = dirs[d][0], dc = dirs[d][1];
        int8_t line[11];
        int n = 0;
        for (int k = -5; k <= 5; k++) {
            int nr = row + dr * k, nc = col + dc * k;
            if (!board_in_bounds(nr, nc)) {
                /* 边界视为非 color 阻塞 */
                line[n++] = (int8_t)(3 - color);  /* 任意非 color、非空的标记 */
            } else {
                line[n++] = (int8_t)b->cells[nr][nc];
            }
        }
        PatternStats st = {0};
        pattern_count_in_line(line, n, color, &st);
        int level = 0;
        if (st.counts[PAT_FIVE] >= 1)             level = 3;
        else if (st.counts[PAT_OPEN_FOUR] >= 1)   level = 3;
        else if (st.counts[PAT_SIMPLE_FOUR] >= 1) level = 3;
        else if (st.counts[PAT_OPEN_THREE] >= 1)  level = 2;
        if (level > max_level) max_level = level;
    }
    b->cells[row][col] = EMPTY;
    return max_level;
}

/* 该 move 是否制造 FIVE / 四 (3)、活三 (2)、或无 (0) */
static int threat_create_level(Board *b, int row, int col) {
    return classify_move_threat(b, row, col, b->side_to_move);
}

/* 该 move 是否在挡对方的威胁（对方若在此点走会形成的威胁等级） */
static int threat_defend_level(Board *b, int row, int col) {
    int opp = (b->side_to_move == BLACK) ? WHITE : BLACK;
    return classify_move_threat(b, row, col, opp);
}

/* compute_lmr — Late Move Reductions reduction amount.
 *
 * 返回 R ≥ 0。R == 0 表示不 reduce（move 进 whitelist 或上下文不满足触发条件）。
 *
 * Whitelist（任一命中 → R = 0，不 reduce）:
 *   - depth_left < 3            （剩余深度太浅，reduce 无收益）
 *   - searched < 3              （前 3 个 move 顺序最好，不能 reduce）
 *   - is_pv_node                （PV 节点不 reduce，避免破坏主变）
 *   - threat_create >= 2        （该 move 制造活三或更强威胁）
 *   - threat_defend >= 3        （该 move 挡对方四级威胁，关键防守）
 *   - is_killer                 （killer move）
 *   - is_tt_best                （TT-best move）
 *
 * 公式（基于已 searched 的 move 数 + depth_left）:
 *   searched ∈ [3, 6]   → R = 1
 *   searched ∈ [7, 11]  → R = 2
 *   searched >= 12      → R = 2 + (depth_left >= 6 ? 1 : 0)
 * 上限: R = min(R, depth_left - 2)（保证 depth_left - 1 - R >= 1）。
 */
static int compute_lmr(int searched, int depth_left, int threat_create,
                       int threat_defend, int is_killer_move, int is_tt_best) {
    if (depth_left < 3)        return 0;
    if (searched < 3)          return 0;
    if (threat_create >= 2)    return 0;
    if (threat_defend >= 3)    return 0;
    if (is_killer_move)        return 0;
    if (is_tt_best)            return 0;

    int R;
    if (searched <= 6)         R = 1;
    else if (searched <= 11)   R = 2;
    else                       R = 2 + (depth_left >= 6 ? 1 : 0);

    int cap = depth_left - 2;
    if (R > cap) R = cap;
    if (R < 0)   R = 0;
    return R;
}

/* ============== 四三 / 双四 / 五连 setup 检测（leaf-level threat extension）==============
 * classify_move_multi: 在 (row, col) 落 color 后，统计 4 个方向上各自的威胁。
 *   out_fours  = 该 cell 落子后形成 four（SIMPLE_FOUR / OPEN_FOUR）的方向数
 *   out_threes = 形成 OPEN_THREE 的方向数
 *   特殊：若任一方向形成 FIVE，out_fours = 99（哨兵值，表示直接 mate）
 *
 * 不动 zobrist / history（cells 直接 toggle）。仅用于 leaf eval 内部检测。
 */
static void classify_move_multi(Board *b, int row, int col, int color,
                                int *out_fours, int *out_threes) {
    *out_fours = 0; *out_threes = 0;
    if (b->cells[row][col] != EMPTY) return;

    static const int dirs[4][2] = { {0,1}, {1,0}, {1,1}, {1,-1} };
    int fours = 0, threes = 0;

    b->cells[row][col] = (uint8_t)color;
    for (int d = 0; d < 4; d++) {
        int dr = dirs[d][0], dc = dirs[d][1];
        int8_t line[11];
        int n = 0;
        for (int k = -5; k <= 5; k++) {
            int nr = row + dr * k, nc = col + dc * k;
            if (!board_in_bounds(nr, nc)) line[n++] = (int8_t)(3 - color);
            else                            line[n++] = (int8_t)b->cells[nr][nc];
        }
        PatternStats st = {0};
        pattern_count_in_line(line, n, color, &st);
        if (st.counts[PAT_FIVE] >= 1) { fours = 99; threes = 0; break; }
        if (st.counts[PAT_OPEN_FOUR] >= 1 || st.counts[PAT_SIMPLE_FOUR] >= 1) fours++;
        else if (st.counts[PAT_OPEN_THREE] >= 1) threes++;
    }
    b->cells[row][col] = EMPTY;
    *out_fours  = fours;
    *out_threes = threes;
}

/* color_has_winning_setup: side==color 是否有"1 步内造出必胜威胁"的着法？
 * 必胜威胁定义（标准 Renju 战术）：
 *   - 直接五连（fours==99）
 *   - 活四（一手不可挡）
 *   - 双四（两个四，对方只能挡一个）
 *   - 四三（一个四 + 一个活三）
 *
 * 黑棋如果禁手则跳过该 cell。返回 1 = 有；0 = 无。
 *
 * 注意：仅扫 search_generate_neighbor_moves 的邻近候选（性能考量）。
 * 这意味着远离任何棋子的"创造性"着法看不见，但 Renju 战术几乎全在已有
 * 棋子附近发生，所以漏检概率很低。
 */
static int color_has_winning_setup(Board *b, int color) {
    Move cands[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(b, cands);
    for (int i = 0; i < n; i++) {
        int r = cands[i].row, c = cands[i].col;
        if (b->cells[r][c] != EMPTY) continue;
        if (color == BLACK && b->forbid_enabled) {
            if (forbid_check_black(b, r, c) != FORBID_NONE) continue;
        }
        int fours, threes;
        classify_move_multi(b, r, c, color, &fours, &threes);
        if (fours >= 99) return 1;                /* 五连 */
        if (fours >= 2)  return 1;                /* 双四 */
        if (fours >= 1 && threes >= 1) return 1;  /* 四三 */
        /* OPEN_FOUR 也是必胜，但 OPEN_FOUR 已在 fours 计数里（>=1）。
         * 单 OPEN_FOUR 必胜需要额外判定 — 这里保守只把"明显胜"标出来。
         */
    }
    return 0;
}

/* 测试专用 wrapper（仅薄包装；不改变 helper 语义）*/
int search_color_has_winning_setup(Board *b, int color) {
    return color_has_winning_setup(b, color);
}

/* TACTICAL_WIN_SCORE: leaf 检测到 side_to_move 有必胜 setup 时返回的分数。
 * 介于普通 pattern eval 上限（~2e7）和 mate threshold（SEARCH_INF - 1e4 = 9.999e7）之间。
 * 选 5e7 让"必胜 setup"明显压过常规威胁分但不被 IS_MATE_SCORE 当作 mate。
 */
#define TACTICAL_WIN_SCORE 50000000

/* ============== Forward decls ============== */
static int alphabeta(Board *b, int ply, int depth_left, int alpha, int beta,
                     int is_pv_node, long *nodes);
static int vcx_search(Board *b, int ply, int depth_left, int allow_three, long *nodes, Move *root_choice);

/* ============== search_find_n_distinct ============== */
int search_find_n_distinct(Board *b, int depth, int n_want, Move *out, int *out_scores,
                           int time_budget_ms) {
    b->zobrist_hash = zobrist_compute(b);
    tt_clear();
    clear_killers_history();

    Move moves[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(b, moves);
    if (n == 0) return 0;

    int scores[SEARCH_MAX_MOVES];
    long nodes_dummy = 0;
    int valid_n = 0;
    int valid_idx[SEARCH_MAX_MOVES];

    int alpha = -SEARCH_INF, beta = SEARCH_INF;

    /* time budget setup（与 search_best_move_timed 同机制）*/
    _budget_ms = time_budget_ms;
    if (time_budget_ms > 0) {
        _deadline = clock() + (clock_t)((long long)time_budget_ms * CLOCKS_PER_SEC / 1000);
    } else {
        _deadline = 0;
    }
    _time_up = 0;

    /* Pass 1：合法性预筛（不搜索），保证 valid_n > 0 不依赖于时间 */
    int color = b->side_to_move;
    for (int i = 0; i < n; i++) {
        if (color == BLACK && b->forbid_enabled) {
            if (forbid_check_black(b, moves[i].row, moves[i].col) != FORBID_NONE) continue;
        }
        if (!board_place(b, moves[i].row, moves[i].col, color)) continue;
        board_undo(b);
        scores[valid_n] = 0;            /* 占位分数，被 ID 覆盖 */
        valid_idx[valid_n] = i;
        valid_n++;
    }
    if (valid_n == 0) return 0;

    /* Pass 2：iterative deepening，每完整搜完一层才提交分数。
     * 时间不够时退回最后一个完整层的结果。
     */
    int tmp_scores[SEARCH_MAX_MOVES];
    for (int d = 1; d <= depth; d++) {
        if (check_time()) break;
        int complete = 1;
        for (int j = 0; j < valid_n; j++) {
            if (check_time()) { complete = 0; break; }
            int i = valid_idx[j];
            if (!board_place(b, moves[i].row, moves[i].col, color)) { complete = 0; break; }
            int s = -alphabeta(b, 1, d - 1, -beta, -alpha, 1, &nodes_dummy);
            board_undo(b);
            if (_time_up) { complete = 0; break; }
            tmp_scores[j] = s;
        }
        if (complete) {
            for (int j = 0; j < valid_n; j++) scores[j] = tmp_scores[j];
        }
    }

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

    /* Time-state hygiene: 清掉 _deadline / _time_up，避免遗留状态污染下次调用。
     * 与 search_best_move_timed 的出口约定保持一致。
     */
    _deadline = 0;
    _time_up = 0;
    return picked;
}

int search_evaluate(const Board *b) {
    return pattern_evaluate(b);
}

/* ============== alphabeta main ============== */
/* alphabeta — negamax with αβ pruning + TT + killer/history + tactical move ordering.
 *
 * PVS (Principal Variation Search): first move from current node uses full window
 * (-beta, -alpha) and inherits is_pv_node. Subsequent moves first try null-window
 * scout (-alpha-1, -alpha) with is_pv_node=0; only re-search with full window +
 * is_pv_node=1 if the scout score lands in (alpha, beta).
 *
 * Root caller passes is_pv_node=1. Recursive calls only the FIRST move stays PV;
 * all others are forced non-PV scouts.
 */
static int alphabeta(Board *b, int ply, int depth_left, int alpha, int beta,
                     int is_pv_node, long *nodes) {
    (*nodes)++;

    /* 时间检查（每 1024 节点一次，避免 clock() 过于频繁） */
    if (((*nodes) & 1023) == 0) check_time();
    if (_time_up) return search_evaluate(b);

    if (ply >= SEARCH_MAX_PLY) return search_evaluate(b);

    /* TT 查询 */
    const TTEntry *e = tt_get(b->zobrist_hash);
    if (e != NULL && e->depth >= depth_left) {
        if (e->flag == TT_FLAG_EXACT) return e->score;
        if (e->flag == TT_FLAG_LOWER && e->score >= beta)  return e->score;
        if (e->flag == TT_FLAG_UPPER && e->score <= alpha) return e->score;
    }

    /* 终局检查 */
    GameResult res = board_check_winner(b);
    if (res == RESULT_BLACK_WIN) {
        return (b->side_to_move == BLACK) ? SEARCH_INF - b->move_count : -(SEARCH_INF - b->move_count);
    }
    if (res == RESULT_WHITE_WIN_NORMAL || res == RESULT_WHITE_WIN_BY_BLACK_FORBID) {
        return (b->side_to_move == WHITE) ? SEARCH_INF - b->move_count : -(SEARCH_INF - b->move_count);
    }
    if (res == RESULT_DRAW) return 0;

    if (depth_left == 0) {
        /* Threat extension: 在 leaf 处做 1-ply 战术检测。
         * 若 side_to_move 有必胜 setup（五连 / 双四 / 四三），返回近 mate 分。
         * 这让 αβ 在不增加深度的情况下"看到"对方的 forcing 战术，避免被 双重威胁 偷袭。
         */
        if (color_has_winning_setup(b, b->side_to_move)) {
            return TACTICAL_WIN_SCORE - (int)b->move_count;
        }
        return search_evaluate(b);
    }

    Move moves[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(b, moves);
    if (n == 0) return search_evaluate(b);

    /* ====== Ghost-mate fix: 预过滤合法 move ======
     * 若所有 move 都被 forbid 排除 / board_place 失败，不能写 TT 一个 mate 分。
     */
    Move legal[SEARCH_MAX_MOVES];
    int legal_n = 0;
    int color = b->side_to_move;
    for (int i = 0; i < n; i++) {
        if (color == BLACK && b->forbid_enabled) {
            if (forbid_check_black(b, moves[i].row, moves[i].col) != FORBID_NONE) continue;
        }
        if (b->cells[moves[i].row][moves[i].col] != EMPTY) continue;
        legal[legal_n++] = moves[i];
    }
    if (legal_n == 0) {
        /* 无合法着，不写 TT，直接返回 eval（stalemate-ish） */
        return search_evaluate(b);
    }

    /* ====== Move ordering（v2）======
     * Priority layers (大 → 小):
     *   TT best      = 1e15
     *   Threat win   = 1e14  （这步直接形成 FIVE / OPEN_FOUR）
     *   Killer       = 1e13
     *   Threat defend (lvl 3) = 5e12
     *   Threat create lvl 2 (open 3) = 1e12
     *   Threat defend lvl 2          = 5e11
     *   History (clamped < 1e11)
     */
    int8_t tt_r = (e != NULL) ? e->best_row : (int8_t)-1;
    int8_t tt_c = (e != NULL) ? e->best_col : (int8_t)-1;
    int color_idx = color - 1;

    long long priority[SEARCH_MAX_MOVES];
    for (int i = 0; i < legal_n; i++) {
        int r = legal[i].row, c = legal[i].col;
        long long pri = _history[color_idx][r * BOARD_SIZE + c];
        if (pri > HISTORY_MAX) pri = HISTORY_MAX;

        int tc = threat_create_level(b, r, c);
        int td = threat_defend_level(b, r, c);
        if (tc >= 3)      pri += 100000000000000LL;   /* 1e14: 制胜威胁 */
        else if (tc >= 2) pri +=   1000000000000LL;   /* 1e12: 活三 */
        if (td >= 3)      pri +=  50000000000000LL;   /* 5e13: 防对方四 */
        else if (td >= 2) pri +=    500000000000LL;   /* 5e11: 防对方活三 */

        if (is_killer(ply, r, c))          pri += 10000000000000LL;  /* 1e13 */
        if (r == tt_r && c == tt_c)         pri += 1000000000000000LL; /* 1e15 */

        priority[i] = pri;
    }
    /* 选择排序，legal_n ≤ ~100 */
    for (int i = 0; i < legal_n - 1; i++) {
        int best = i;
        for (int j = i + 1; j < legal_n; j++) if (priority[j] > priority[best]) best = j;
        if (best != i) {
            Move tm = legal[i]; legal[i] = legal[best]; legal[best] = tm;
            long long tp = priority[i]; priority[i] = priority[best]; priority[best] = tp;
        }
    }

    int orig_alpha = alpha;
    int best_score = -SEARCH_INF;
    int best_r = -1, best_c = -1;

    int searched = 0;  /* number of moves actually searched (post board_place success) */
    for (int i = 0; i < legal_n; i++) {
        int r = legal[i].row, c = legal[i].col;
        if (!board_place(b, r, c, color)) continue;

        int score;
        if (searched == 0) {
            /* First move: full window, inherit PV status */
            score = -alphabeta(b, ply + 1, depth_left - 1, -beta, -alpha, is_pv_node, nodes);
        } else {
            /* Subsequent moves: null-window scout, possibly LMR-reduced, forced non-PV.
             *
             * LMR: 对"很可能 fail-low"的后段着法降深度搜（reduced scout）；如果意外
             * fail-high，再用 full depth null-window 确认；若仍 fail-high 且在 PV
             * 节点，最后用 full-window full-depth re-search。
             *
             * compute_lmr 返回 R = 0 表示不 reduce（等价于原 PVS scout）。
             */
            int tc_lmr = threat_create_level(b, r, c);
            int td_lmr = threat_defend_level(b, r, c);
            int km     = is_killer(ply, r, c);
            int ttb    = (r == tt_r && c == tt_c);
            int R = compute_lmr(searched, depth_left, tc_lmr, td_lmr, km, ttb);

            score = -alphabeta(b, ply + 1, depth_left - 1 - R, -alpha - 1, -alpha, 0, nodes);
            /* Reduced scout fail-high → re-search full depth null-window to confirm */
            if (R > 0 && score > alpha) {
                score = -alphabeta(b, ply + 1, depth_left - 1, -alpha - 1, -alpha, 0, nodes);
            }
            /* fail-high in [alpha+1, beta) AND we are PV node → full-window re-search */
            if (score > alpha && score < beta && is_pv_node) {
                score = -alphabeta(b, ply + 1, depth_left - 1, -beta, -alpha, 1, nodes);
            }
        }
        board_undo(b);
        searched++;

        if (_time_up) return best_score > -SEARCH_INF ? best_score : search_evaluate(b);

        if (score > best_score) {
            best_score = score;
            best_r = r;
            best_c = c;
        }
        if (best_score > alpha) alpha = best_score;
        if (alpha >= beta) {
            record_killer(ply, r, c);
            int sh = depth_left < 20 ? depth_left : 20;
            long long add = 1LL << sh;
            _history[color_idx][r * BOARD_SIZE + c] += add;
            if (_history[color_idx][r * BOARD_SIZE + c] > HISTORY_MAX) {
                /* aging：除以 2 平摊到所有 */
                age_history();
            }
            break;
        }
    }

    /* TT 写入 */
    TTFlag flag;
    if (best_score <= orig_alpha)      flag = TT_FLAG_UPPER;
    else if (best_score >= beta)       flag = TT_FLAG_LOWER;
    else                                flag = TT_FLAG_EXACT;
    tt_put(b->zobrist_hash, depth_left, best_score, flag, best_r, best_c);

    return best_score;
}

/* ============== VCF / VCT 专用搜索 ==============
 *
 * VCx 思路：每一手只搜"强迫着"
 *   - 自己一方：必须制造威胁（VCF：四；VCT：四+活三）
 *   - 对方一方：必须应这个威胁（=只搜挡威胁的 move + 制造反威胁的 move）
 *
 * 返回从当前 side_to_move 视角的分数：
 *   +SEARCH_INF - move_count → 当前方必胜
 *   -SEARCH_INF + move_count → 当前方必负
 *   0                        → 未证明胜负（fail）
 *
 * root_choice (仅根节点用)：找到必胜手时填入第一步。
 */
static int vcx_search(Board *b, int ply, int depth_left, int allow_three, long *nodes, Move *root_choice) {
    (*nodes)++;

    if (((*nodes) & 1023) == 0) check_time();
    if (_time_up) return 0;

    if (ply >= SEARCH_MAX_PLY) return 0;

    /* 终局检查 */
    GameResult res = board_check_winner(b);
    if (res == RESULT_BLACK_WIN)
        return (b->side_to_move == BLACK) ? SEARCH_INF - b->move_count : -(SEARCH_INF - b->move_count);
    if (res == RESULT_WHITE_WIN_NORMAL || res == RESULT_WHITE_WIN_BY_BLACK_FORBID)
        return (b->side_to_move == WHITE) ? SEARCH_INF - b->move_count : -(SEARCH_INF - b->move_count);
    if (res == RESULT_DRAW) return 0;

    if (depth_left <= 0) return 0;

    int color = b->side_to_move;
    int opp   = (color == BLACK) ? WHITE : BLACK;

    /* === 检查"对方在上一手是否制造了 OPEN_FOUR / FIVE 必胜威胁" ===
     * 若有，本方必须挡；挡不掉返回必负
     * 简化：扫所有空格，看对方走那里能否形成 FIVE
     */
    /* 先看本方能否直接 FIVE（立刻赢） */
    Move all_moves[SEARCH_MAX_MOVES];
    int n_moves = search_generate_neighbor_moves(b, all_moves);

    /* 候选着法：本方的 forcing moves */
    Move forcing[SEARCH_MAX_MOVES];
    int n_forcing = 0;

    /* 第一步：是否本方有直接 FIVE？ */
    for (int i = 0; i < n_moves; i++) {
        int r = all_moves[i].row, c = all_moves[i].col;
        if (color == BLACK && b->forbid_enabled) {
            if (forbid_check_black(b, r, c) != FORBID_NONE) continue;
        }
        if (b->cells[r][c] != EMPTY) continue;
        int tc = threat_create_level(b, r, c);
        if (tc >= 3) {
            /* 模拟落子，看是否真 FIVE */
            b->cells[r][c] = (uint8_t)color;
            GameResult tmp_res = board_check_winner(b);
            b->cells[r][c] = EMPTY;
            int is_five = (tmp_res != RESULT_NONE) &&
                          ((color == BLACK && tmp_res == RESULT_BLACK_WIN) ||
                           (color == WHITE && tmp_res == RESULT_WHITE_WIN_NORMAL));
            if (is_five) {
                if (root_choice) { *root_choice = all_moves[i]; }
                return SEARCH_INF - b->move_count - 1;  /* 当前方必胜 */
            }
            forcing[n_forcing++] = all_moves[i];   /* 四级威胁先收下 */
        } else if (allow_three && tc >= 2) {
            forcing[n_forcing++] = all_moves[i];   /* VCT: 活三也收 */
        }
    }

    /* === 检查对方威胁 ===
     * 若对方在某点能直接 FIVE → 本方必须挡该点（且只搜该点）
     */
    int opp_five_r = -1, opp_five_c = -1;
    for (int i = 0; i < n_moves; i++) {
        int r = all_moves[i].row, c = all_moves[i].col;
        if (b->cells[r][c] != EMPTY) continue;
        b->cells[r][c] = (uint8_t)opp;
        GameResult tmp_res = board_check_winner(b);
        b->cells[r][c] = EMPTY;
        int is_five = (tmp_res != RESULT_NONE) &&
                      ((opp == BLACK && tmp_res == RESULT_BLACK_WIN) ||
                       (opp == WHITE && (tmp_res == RESULT_WHITE_WIN_NORMAL ||
                                          tmp_res == RESULT_WHITE_WIN_BY_BLACK_FORBID)));
        if (is_five) {
            if (opp_five_r >= 0) {
                /* 对方有双重 FIVE 威胁，本方挡不掉 */
                return -(SEARCH_INF - b->move_count);
            }
            opp_five_r = r; opp_five_c = c;
        }
    }

    if (opp_five_r >= 0) {
        /* 必须挡这一点；只搜这一手 */
        int r = opp_five_r, c = opp_five_c;
        if (color == BLACK && b->forbid_enabled &&
            forbid_check_black(b, r, c) != FORBID_NONE) {
            /* 挡的点是黑禁手 → 必负 */
            return -(SEARCH_INF - b->move_count);
        }
        if (!board_place(b, r, c, color)) return 0;
        int score = -vcx_search(b, ply + 1, depth_left - 1, allow_three, nodes, NULL);
        board_undo(b);
        if (root_choice && score > 0 && !IS_MATE_SCORE(-score)) {
            /* 不是必胜，不写 root_choice */
        }
        if (root_choice && score >= SEARCH_INF - SEARCH_MATE_MARGIN) {
            root_choice->row = (int8_t)r;
            root_choice->col = (int8_t)c;
            root_choice->color = (int8_t)color;
        }
        return score;
    }

    /* 普通节点：只扩展 forcing moves；若本方一个都没有 → fail (0) */
    if (n_forcing == 0) return 0;

    int best = -SEARCH_INF * 2;  /* 留 sentinel；我们关心的是 >= mate threshold */
    Move best_move_local = forcing[0];
    int proven_win = 0;
    for (int i = 0; i < n_forcing; i++) {
        int r = forcing[i].row, c = forcing[i].col;
        if (!board_place(b, r, c, color)) continue;
        int score = -vcx_search(b, ply + 1, depth_left - 1, allow_three, nodes, NULL);
        board_undo(b);
        if (_time_up) return 0;
        if (score >= SEARCH_INF - SEARCH_MATE_MARGIN) {
            /* 本方必胜 */
            if (root_choice) {
                root_choice->row = (int8_t)r;
                root_choice->col = (int8_t)c;
                root_choice->color = (int8_t)color;
            }
            return score;
        }
        if (score > best) { best = score; best_move_local = forcing[i]; }
        (void)best_move_local;
        if (score > 0) proven_win = 1; /* 部分赢，但不是 mate */
        (void)proven_win;
    }
    /* 没找到必胜 */
    return 0;
}

int search_vcf(Board *b, int max_depth, Move *out_first, long *out_nodes) {
    b->zobrist_hash = zobrist_compute(b);
    Move choice = { -1, -1, (int8_t)b->side_to_move };
    long nodes = 0;
    int score = vcx_search(b, 0, max_depth, 0 /*VCF: no threes*/, &nodes, &choice);
    if (out_nodes) *out_nodes += nodes;
    if (score >= SEARCH_INF - SEARCH_MATE_MARGIN && choice.row >= 0) {
        if (out_first) *out_first = choice;
        return 1;
    }
    return 0;
}

int search_vct(Board *b, int max_depth, Move *out_first, long *out_nodes) {
    b->zobrist_hash = zobrist_compute(b);
    Move choice = { -1, -1, (int8_t)b->side_to_move };
    long nodes = 0;
    int score = vcx_search(b, 0, max_depth, 1 /*VCT: allow open-three*/, &nodes, &choice);
    if (out_nodes) *out_nodes += nodes;
    if (score >= SEARCH_INF - SEARCH_MATE_MARGIN && choice.row >= 0) {
        if (out_first) *out_first = choice;
        return 1;
    }
    return 0;
}

/* ============== Root search with ID + aspiration window + time budget ============== */
SearchResult search_best_move_timed(Board *b, int max_depth, int time_budget_ms) {
    SearchResult result = { .best_move = { -1, -1, (int8_t)b->side_to_move },
                            .score = -SEARCH_INF, .nodes_searched = 0 };

    b->zobrist_hash = zobrist_compute(b);
    /* 不再 tt_clear() — TT 跨调用复用 */
    clear_killers_history();  /* killer/history per-call reset 仍合理 */

    /* time budget setup */
    _budget_ms = time_budget_ms;
    if (time_budget_ms > 0) {
        _deadline = clock() + (clock_t)((long long)time_budget_ms * CLOCKS_PER_SEC / 1000);
    } else {
        _deadline = 0;
    }
    _time_up = 0;

    Move moves[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(b, moves);
    if (n == 0) return result;

    /* ============= Step 1: 尝试 VCF（仅当 budget > 0 时；快搜） =============
     * VCF 节点数小，单独切一小段时间预算（最多 budget 的 30%）
     */
    if (time_budget_ms != 0) {
        Move vcf_choice;
        if (!_time_up) {
            int vcf_found = search_vcf(b, 20, &vcf_choice, &result.nodes_searched);
            if (vcf_found) {
                result.best_move = vcf_choice;
                result.score = SEARCH_INF - b->move_count - 1;
                return result;
            }
        }
    }

    /* ============= Step 2: 迭代加深 αβ =============
     * 每层用 aspiration window：[prev-50, prev+50]
     * fail-high/low 时退化到全窗
     */
    int prev_score = 0;
    int has_prev = 0;
    const int ASP_WINDOW = 50;

    for (int d = 1; d <= max_depth; d++) {
        if (check_time()) break;

        int alpha, beta;
        if (has_prev && !IS_MATE_SCORE(prev_score)) {
            alpha = prev_score - ASP_WINDOW;
            beta  = prev_score + ASP_WINDOW;
        } else {
            alpha = -SEARCH_INF;
            beta  = SEARCH_INF;
        }

        SearchResult cur;
        int iteration_done = 0;

        while (!iteration_done) {
            cur.best_move = result.best_move;
            cur.score = -SEARCH_INF;
            cur.nodes_searched = result.nodes_searched;

            /* 顶层 TT-best first */
            const TTEntry *root_tt = tt_get(b->zobrist_hash);
            if (root_tt && root_tt->best_row >= 0) {
                for (int i = 0; i < n; i++) {
                    if (moves[i].row == root_tt->best_row && moves[i].col == root_tt->best_col) {
                        Move t = moves[0]; moves[0] = moves[i]; moves[i] = t;
                        break;
                    }
                }
            }

            int color = b->side_to_move;
            int local_alpha = alpha;
            int legal_seen = 0;
            for (int i = 0; i < n; i++) {
                if (color == BLACK && b->forbid_enabled) {
                    if (forbid_check_black(b, moves[i].row, moves[i].col) != FORBID_NONE) continue;
                }
                if (!board_place(b, moves[i].row, moves[i].col, color)) continue;
                legal_seen++;
                /* Root is itself a PV node — pass is_pv=1 to first call.
                 * PVS at root level (scout siblings) intentionally not added here for
                 * commit-1 simplicity; aspiration window re-search interaction would
                 * compound complexity. PVS still kicks in at depth 1+ inside alphabeta.
                 */
                int score = -alphabeta(b, 1, d - 1, -beta, -local_alpha, 1, &cur.nodes_searched);
                board_undo(b);

                if (_time_up) break;

                if (score > cur.score) {
                    cur.score = score;
                    cur.best_move = moves[i];
                }
                if (score > local_alpha) local_alpha = score;
            }

            if (_time_up) break;

            /* aspiration window 判定 */
            if (cur.score <= alpha) {
                /* fail-low：扩低 */
                if (alpha == -SEARCH_INF) { iteration_done = 1; }
                else { alpha = -SEARCH_INF; }
            } else if (cur.score >= beta) {
                /* fail-high：扩高 */
                if (beta == SEARCH_INF) { iteration_done = 1; }
                else { beta = SEARCH_INF; }
            } else {
                iteration_done = 1;
            }

            if (legal_seen == 0) { iteration_done = 1; break; }
        }

        if (_time_up) break;

        /* 接受这一层结果 */
        result.best_move = cur.best_move;
        result.score = cur.score;
        result.nodes_searched = cur.nodes_searched;
        prev_score = cur.score;
        has_prev = 1;

        /* 早停：发现 mate */
        if (IS_MATE_SCORE(result.score)) break;
    }

    /* 关掉计时器，避免影响后续静态分析调用 */
    _deadline = 0;
    _time_up = 0;
    return result;
}

SearchResult search_best_move(Board *b, int depth) {
    return search_best_move_timed(b, depth, SEARCH_DEFAULT_TIME_MS);
}

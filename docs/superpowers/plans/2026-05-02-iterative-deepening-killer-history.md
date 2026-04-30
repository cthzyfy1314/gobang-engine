# Day 4 (5/2): Iterative Deepening + Killer Move + History Heuristic

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans.

**Goal:** 升级 αβ 为迭代加深（ID），加 Killer Move + History Heuristic 作 move ordering，让搜索深度从 d=3 提升到 d=4-6 仍可接受响应时间，**为 5/3-5/5 禁手判定降低运行时压力**。Tag: `m1-search-v2`。

**Architecture:**
- ID 主循环：从 d=1 逐层加深到 max_depth；每层完成后用其 best move 作为下层第一候选（PV-first）
- Killer Slots：`_killers[ply][2]` —— 每个搜索深度记录两个引发 β 剪枝的 move，跨 ID 迭代保留
- History Heuristic：`_history[color][pos]` —— 累计每 (color, position) 引发剪枝的次数，作 quiet move ordering 的次级排序键
- Move ordering 优先级：(1) TT best move → (2) killer1 → (3) killer2 → (4) history score 降序
- TT 跨 ID 迭代不清表（之前是每次 search_best_move 入口清——现在改为只在新游戏清）

**Tech Stack:** C99 / 沿用现有 build.bat / 测试沿用 test_search.exe

**对应 Spec 节段:** § 3.1（迭代加深 + Killer Move）/ § 7 节奏 5/2 行 / § 4.1 search+ 模块

**预算:** 0.5d（实际预计 1-1.5h，所有数据结构是简单数组）

---

## Task T1: search.h 公开接口微调

**Files:** Modify `src/search.h`

新增宏（非接口变化，但 alphabeta 内部需要）：

```c
#define SEARCH_MAX_PLY 32        /* killer/history 数组维度 */
```

`search_best_move` 接口签名不变（仍传入 max_depth，内部走 ID）。

- [ ] **Step T1.1**: 加 `SEARCH_MAX_PLY` 宏到 `search.h`
- [ ] **Step T1.2**: Commit `feat(search): add SEARCH_MAX_PLY for killer/history arrays`

---

## Task T2: search.c 静态状态：killers + history + reset

**Files:** Modify `src/search.c`

在文件顶部 `#include` 之后加：

```c
/* Move ordering helpers — 跨 ID 迭代保留，仅在新搜索 (search_best_move) 入口清零 */
typedef struct {
    int8_t row, col;
} Killer;

static Killer _killers[SEARCH_MAX_PLY][2];
static long   _history[2][BOARD_SIZE * BOARD_SIZE];

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
```

- [ ] **Step T2.1**: 加上述代码
- [ ] **Step T2.2**: Commit `feat(search): add killer/history static state and helpers`

---

## Task T3: alphabeta 加 ply 参数 + record killer/history 在 β 剪枝

**Files:** Modify `src/search.c`

修改 `alphabeta` 签名：从 `(Board*, depth_left, alpha, beta, nodes)` 改为 `(Board*, ply, depth_left, alpha, beta, nodes)`。

- 进入函数时 `ply >= SEARCH_MAX_PLY` 视为深度上限（直接评估）
- 主循环中 β 剪枝（`alpha >= beta`）发生时：调 `record_killer(ply, row, col)` + `_history[color-1][row*BOARD_SIZE+col] += 1 << depth_left`（深度越深越值钱）

具体改动：

```c
static int alphabeta(Board *b, int ply, int depth_left, int alpha, int beta, long *nodes) {
    (*nodes)++;

    if (ply >= SEARCH_MAX_PLY) return search_evaluate(b);

    /* TT lookup 不变 */

    /* terminal check 不变 */

    if (depth_left == 0) return search_evaluate(b);

    Move moves[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(b, moves);
    if (n == 0) return search_evaluate(b);

    /* T4 会在此处插 ordering */

    int orig_alpha = alpha;
    int best_score = -SEARCH_INF;
    int best_r = -1, best_c = -1;
    for (int i = 0; i < n; i++) {
        int color = b->side_to_move;
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
            _history[color - 1][moves[i].row * BOARD_SIZE + moves[i].col] += (1L << depth_left);
            break;
        }
    }

    /* TT 写入不变 */
    TTFlag flag;
    if (best_score <= orig_alpha)      flag = TT_FLAG_UPPER;
    else if (best_score >= beta)       flag = TT_FLAG_LOWER;
    else                                flag = TT_FLAG_EXACT;
    tt_put(b->zobrist_hash, depth_left, best_score, flag, best_r, best_c);

    return best_score;
}
```

`search_best_move` 内部调用从 `alphabeta(..., depth - 1, ...)` 改为 `alphabeta(b, 1, depth - 1, ...)`。（顶层即 ply=0，子节点 ply=1。）

- [ ] **Step T3.1**: 修改 alphabeta 签名 + 加 ply 边界 + β 剪枝处记录 killer/history
- [ ] **Step T3.2**: 修改 search_best_move 调用
- [ ] **Step T3.3**: Build + run all tests, expect PASS（功能不变）
- [ ] **Step T3.4**: Commit `feat(search): record killer move and history score on beta cutoff`

---

## Task T4: Move ordering — TT best > killers > history

**Files:** Modify `src/search.c`

在 `alphabeta` 中 `Move moves[SEARCH_MAX_MOVES]; int n = search_generate_neighbor_moves(...)` 之后插入排序：

```c
/* Move ordering: TT best > killers > history score 降序 */
const TTEntry *tt_e = tt_get(b->zobrist_hash);
int8_t tt_r = tt_e ? tt_e->best_row : -1;
int8_t tt_c = tt_e ? tt_e->best_col : -1;

/* 计算每个 move 的优先级分数 */
int priority[SEARCH_MAX_MOVES];
for (int i = 0; i < n; i++) {
    int r = moves[i].row, c = moves[i].col;
    int color = b->side_to_move;
    int score;
    if (r == tt_r && c == tt_c)             score = 1000000000;       /* TT best */
    else if (is_killer(ply, r, c))          score = 100000000;        /* killer */
    else                                    score = (int)(_history[color - 1][r * BOARD_SIZE + c] & 0x7FFFFFFF);
    priority[i] = score;
}

/* 简单选择排序（n ≤ 80，O(n²) 够用）*/
for (int i = 0; i < n - 1; i++) {
    int best = i;
    for (int j = i + 1; j < n; j++) if (priority[j] > priority[best]) best = j;
    if (best != i) {
        Move tmp_m = moves[i]; moves[i] = moves[best]; moves[best] = tmp_m;
        int tmp_p = priority[i]; priority[i] = priority[best]; priority[best] = tmp_p;
    }
}
```

注意：history 加入比较时取低 31 位避免溢出（已在 T3 用 `1L << depth_left` 累加，long 可能很大）。

- [ ] **Step T4.1**: 加 move ordering
- [ ] **Step T4.2**: Build + run all tests, expect PASS
- [ ] **Step T4.3**: Commit `feat(search): move ordering by TT best > killer > history`

---

## Task T5: 迭代加深主循环

**Files:** Modify `src/search.c`

重写 `search_best_move`：

```c
SearchResult search_best_move(Board *b, int max_depth) {
    SearchResult result = { .best_move = { -1, -1, (int8_t)b->side_to_move },
                            .score = -SEARCH_INF, .nodes_searched = 0 };

    /* 新搜索：清 killer/history（TT 跨 search_best_move 调用保留 → 改：保留 TT，
     * 让连续两步对手棋类似时仍能命中。如果对手发疯下到偏远位置，hash 不命中也无害。
     */
    clear_search_state();
    /* 不再 tt_clear()——5/1 那行删掉 */

    Move moves[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(b, moves);
    if (n == 0) return result;

    /* 迭代加深：1 → max_depth */
    for (int d = 1; d <= max_depth; d++) {
        SearchResult cur = { .best_move = { -1, -1, (int8_t)b->side_to_move },
                             .score = -SEARCH_INF, .nodes_searched = result.nodes_searched };
        int alpha = -SEARCH_INF, beta = SEARCH_INF;

        /* 顶层用 TT best 排序（如果有）*/
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

        /* 早停：发现必胜 */
        if (result.score > SEARCH_INF / 2) break;
    }
    return result;
}
```

注意删除 5/1 留下的 `tt_clear()`——TT 跨多步搜索保留是 ID 的关键加速点。

新增模块清表 API（用于游戏开始/重置）：

`search.h` 加：
```c
void search_reset(void);   /* 清 TT + killers + history。新游戏开始时调用 */
```

`search.c` 加：
```c
void search_reset(void) {
    tt_clear();
    clear_search_state();
}
```

`main.c` 启动时改调 `search_reset()` 替代默认依赖 `zobrist_init()` 的 `tt_clear()`（但 zobrist_init 已经调 tt_clear，所以等价）。这里强调一次为新游戏的明确 API。

- [ ] **Step T5.1**: 重写 search_best_move（删 tt_clear，加 ID loop，加根 PV ordering）
- [ ] **Step T5.2**: 加 search_reset()
- [ ] **Step T5.3**: main.c 在启动时调 zobrist_init() 之后再调 search_reset()（多余但更明确）
- [ ] **Step T5.4**: Build + run all tests, expect PASS
- [ ] **Step T5.5**: Commit `feat(search): iterative deepening with PV move ordering at root`

---

## Task T6: 性能 benchmark + tag m1-search-v2

**Files:** No code change (only run + measure + commit + tag)

- [ ] **Step T6.1**: 跑 main.c d=4 self-play，记录 total nodes
- [ ] **Step T6.2**: （可选）临时把 main.c d 改回 4，或加一个 d=5 测试。看节点数：
  - 5/1 d=4 baseline: ~3.5M nodes / 6 plies
  - 期望 5/2 后: 同样 d=4 节点数下降 ≥ 30%（killer + history + ID 共同效果）
- [ ] **Step T6.3**: Commit + tag `m1-search-v2`

```bash
git tag -a m1-search-v2 -m "Day 4 (5/2): iterative deepening + killer move + history heuristic.
- ID main loop 1..max_depth with PV-first root ordering
- Killer Move (2 slots/ply) + History Heuristic (color × position)
- Move ordering: TT best > killer1 > killer2 > history score
- TT preserved across search calls (only cleared on search_reset)
- d=4 self-play node count down from 3.5M baseline by ~XX%
- Ready for 5/3 forbid (longest module ahead)"
git push origin main && git push origin m1-search-v2
```

---

## Self-Review

✅ **Spec 覆盖:** § 3.1（ID + Killer Move）/ § 7 节奏 5/2 / § 4.1 search+
⚠️ **不覆盖:** Time control（spec 说无时限）/ aspiration windows / null move pruning（5/8 之前都不需要）
✅ **类型一致性:** Killer 结构内部，alphabeta 签名增加 ply 参数贯穿 T3-T5；search_reset 是新公开 API

# Day 3 (5/1): Zobrist Hashing + Transposition Table Plan

> **Status:** Drafted 2026-04-30 evening (提前于 spec 节奏开干). Tag target: `m1-zobrist`.
>
> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans. Steps use `- [ ]` syntax.

**Goal:** 加 Zobrist 增量哈希 + 置换表（TT），让 αβ 跨 search 复用子树评估。验收：固定 d=4 self-play，节点数下降 ≥ 30%。

**Architecture:**
- `zobrist.c/.h` 提供：64-bit 随机表 `Z_KEY[2][225]`（黑/白 × 225 格）+ `Z_SIDE_KEY` + 置换表 + put/get + clear。
- `board.c` 修改 1 处：`board_place` / `board_undo` 增量 XOR 更新 `b->zobrist_hash`。
- `search.c` 修改 αβ：进入查 TT，出口存 TT，区分 EXACT / LOWER_BOUND / UPPER_BOUND。

**Tech Stack:** C99 / 固定 PRNG seed（splitmix64）让 hash 可重现 / TT_SIZE = 1<<20 (≈ 32MB) / spec § 5.3 数据结构

**对应 Spec 节段:** § 3.1（迭代加深 + Killer Move 但本 plan 只做 zobrist+TT）/ § 5.3 置换表 / § 7 节奏 5/1

**预算:** 0.5d（实际预计 1-2h，比 spec 估计快——因为 αβ 已经写好）

---

## Task T1: zobrist.h 接口

**Files:** Create `src/zobrist.h`

```c
#ifndef ZOBRIST_H_
#define ZOBRIST_H_

#include <stdint.h>
#include "board.h"

#define TT_SIZE_LOG2 20            /* 1M entries */
#define TT_SIZE      (1u << TT_SIZE_LOG2)
#define TT_INDEX(h)  ((uint32_t)((h) & (TT_SIZE - 1)))

typedef enum {
    TT_FLAG_NONE  = 0,
    TT_FLAG_EXACT = 1,
    TT_FLAG_LOWER = 2,    /* score 是下界（α 剪枝时）*/
    TT_FLAG_UPPER = 3     /* score 是上界（β 剪枝时）*/
} TTFlag;

typedef struct {
    uint64_t key;          /* 完整 64-bit hash 用于 collision 校验 */
    int      score;
    int16_t  depth;        /* 该 entry 来自的搜索深度 */
    uint8_t  flag;
    int8_t   best_row;     /* 最佳着法行（-1 表示无）*/
    int8_t   best_col;
} TTEntry;

/* 模块初始化：填随机表 + clear TT。程序启动时调用一次 */
void zobrist_init(void);

/* 完整从棋盘重新算 hash（debugging / sanity check 用） */
uint64_t zobrist_compute(const Board *b);

/* 增量更新：落子 (row,col,color) 时 XOR；撤子时再 XOR 一次回去 */
uint64_t zobrist_xor_piece(uint64_t h, int row, int col, int color);
uint64_t zobrist_xor_side(uint64_t h);

/* 置换表 */
void tt_clear(void);
const TTEntry *tt_get(uint64_t key);     /* 返回 NULL 如未命中 */
void tt_put(uint64_t key, int depth, int score, TTFlag flag, int best_row, int best_col);

#endif
```

- [ ] **Step T1.1**: Write `src/zobrist.h` per above
- [ ] **Step T1.2**: Commit `feat(zobrist): declare hashing + TT API`

---

## Task T2: zobrist.c 随机表 + compute_from_board + xor helpers

**Files:** Create `src/zobrist.c`

实现要点：
- 用 splitmix64 PRNG（fixed seed = `0x9E3779B97F4A7C15`）填 `Z_KEY[2][BOARD_SIZE*BOARD_SIZE]` 和 `Z_SIDE_KEY`
- `zobrist_init()` 填 key 表 + 调 `tt_clear()`
- `zobrist_compute(b)` 全盘扫一遍 cells，XOR 所有 piece key + 如果 side_to_move==WHITE 再 XOR side key（约定 hash 表示"轮到白时"）
- `zobrist_xor_piece(h, r, c, color)` = `h ^ Z_KEY[color-1][r*BOARD_SIZE+c]`
- `zobrist_xor_side(h)` = `h ^ Z_SIDE_KEY`

Test: `tests/test_zobrist.c` — 至少 3 个 case：
- 空盘 hash 是稳定值
- place 一颗子 → hash 改变 → undo 后回到原值
- 同样布局两种落子顺序得到相同 hash

```c
/* src/zobrist.c (核心代码片段) */
#define _CRT_SECURE_NO_WARNINGS
#include <string.h>
#include "zobrist.h"

static uint64_t Z_KEY[2][BOARD_SIZE * BOARD_SIZE];
static uint64_t Z_SIDE_KEY;
static TTEntry  _tt[TT_SIZE];
static int      _tt_initialized = 0;

static uint64_t splitmix64(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void zobrist_init(void) {
    uint64_t seed = 0x9E3779B97F4A7C15ULL;
    for (int color = 0; color < 2; color++)
        for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++)
            Z_KEY[color][i] = splitmix64(&seed);
    Z_SIDE_KEY = splitmix64(&seed);
    tt_clear();
    _tt_initialized = 1;
}

uint64_t zobrist_xor_piece(uint64_t h, int row, int col, int color) {
    return h ^ Z_KEY[color - 1][row * BOARD_SIZE + col];
}

uint64_t zobrist_xor_side(uint64_t h) {
    return h ^ Z_SIDE_KEY;
}

uint64_t zobrist_compute(const Board *b) {
    uint64_t h = 0;
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            if (b->cells[r][c] != EMPTY)
                h = zobrist_xor_piece(h, r, c, b->cells[r][c]);
    if (b->side_to_move == WHITE) h = zobrist_xor_side(h);
    return h;
}

void tt_clear(void) {
    memset(_tt, 0, sizeof(_tt));
}

const TTEntry *tt_get(uint64_t key) {
    const TTEntry *e = &_tt[TT_INDEX(key)];
    return (e->flag != TT_FLAG_NONE && e->key == key) ? e : NULL;
}

void tt_put(uint64_t key, int depth, int score, TTFlag flag, int best_row, int best_col) {
    TTEntry *e = &_tt[TT_INDEX(key)];
    /* always-replace 策略（最简版）*/
    e->key = key;
    e->depth = (int16_t)depth;
    e->score = score;
    e->flag = (uint8_t)flag;
    e->best_row = (int8_t)best_row;
    e->best_col = (int8_t)best_col;
}
```

- [ ] **Step T2.1**: Write zobrist.c
- [ ] **Step T2.2**: Write test_zobrist.c with 3 tests (stable empty / place-undo round-trip / order-independent)
- [ ] **Step T2.3**: Build + run, expect all PASS
- [ ] **Step T2.4**: Commit `feat(zobrist): random key table + compute + xor helpers + TT storage`

---

## Task T3: board.c 集成 — place/undo 增量更新

**Files:** Modify `src/board.c`

在 `board_place` 末尾、`return true` 之前加：
```c
b->zobrist_hash = zobrist_xor_piece(b->zobrist_hash, row, col, color);
b->zobrist_hash = zobrist_xor_side(b->zobrist_hash);
```

`board_undo` 中 `b->cells[m->row][m->col] = EMPTY;` **之前**插入：
```c
b->zobrist_hash = zobrist_xor_piece(b->zobrist_hash, m->row, m->col, m->color);
b->zobrist_hash = zobrist_xor_side(b->zobrist_hash);
```

`board.c` 顶部 include `zobrist.h`。

测试：在 `test_board.c` 加一个 case，确认 place/undo 后 hash == 初始值。

- [ ] **Step T3.1**: Modify board.c (place + undo + include)
- [ ] **Step T3.2**: Add hash round-trip test to test_board.c
- [ ] **Step T3.3**: Build + run all tests, expect PASS (含 zobrist+board)
- [ ] **Step T3.4**: Commit `feat(board): incremental zobrist hash via XOR on place/undo`

---

## Task T4: search.c 集成 TT — αβ 入口查表 + 出口存表

**Files:** Modify `src/search.c`

修改 `alphabeta` 函数：

```c
static int alphabeta(Board *b, int depth_left, int alpha, int beta, long *nodes) {
    (*nodes)++;

    /* TT 查询 */
    const TTEntry *e = tt_get(b->zobrist_hash);
    if (e != NULL && e->depth >= depth_left) {
        if (e->flag == TT_FLAG_EXACT)              return e->score;
        if (e->flag == TT_FLAG_LOWER && e->score >= beta) return e->score;
        if (e->flag == TT_FLAG_UPPER && e->score <= alpha) return e->score;
    }

    /* ... 终局检查 + depth_left==0 评估保持不变 ... */

    /* ... 主循环保持不变，但记录 best_move 和 alpha 起点 ... */
    int orig_alpha = alpha;
    Move best = {-1, -1, (int8_t)b->side_to_move};
    int best_score = -SEARCH_INF;
    /* ... for-loop body ... */
    /*       if (score > best_score) { best_score = score; best = moves[i]; } */
    /*       if (best_score > alpha) alpha = best_score; */
    /*       if (alpha >= beta) break; */

    /* TT 写入：根据 alpha/beta 边界判定 flag */
    TTFlag flag;
    if (best_score <= orig_alpha)      flag = TT_FLAG_UPPER;
    else if (best_score >= beta)       flag = TT_FLAG_LOWER;
    else                                flag = TT_FLAG_EXACT;
    tt_put(b->zobrist_hash, depth_left, best_score, flag, best.row, best.col);

    return best_score;
}
```

`main.c` 启动时调 `zobrist_init()`。`search_best_move` 之前调 `tt_clear()`（每次新搜索清表，避免跨着法的 hash 污染）。

build.bat 加 `src\zobrist.c` 到所有目标。

- [ ] **Step T4.1**: Modify alphabeta (TT lookup + store with flag)
- [ ] **Step T4.2**: main.c: 启动时 zobrist_init()
- [ ] **Step T4.3**: search_best_move: 入口前 tt_clear()
- [ ] **Step T4.4**: build.bat: include zobrist.c in main + test_search; add test_zobrist.exe
- [ ] **Step T4.5**: Build + run all tests, expect 4 exe 全 PASS
- [ ] **Step T4.6**: Commit `feat(search): integrate transposition table with EXACT/LOWER/UPPER bounds`

---

## Task T5: 性能验证 + tag

**Files:** No code change

- [ ] **Step T5.1**: 跑 d=4 self-play 对比
  - 临时改 main.c 的 `search_best_move(&b, 2)` 为 `search_best_move(&b, 4)`
  - 跑两次：commit "before TT" 时一次（用 git stash 临时禁用 TT）+ 当前 TT 启用一次
  - 比较 nodes_searched 总和
  - 期望：TT 启用后下降 ≥ 30%（spec § 1.2 目标 30-60%）
- [ ] **Step T5.2**: 把 main.c 改回 d=2（或 d=3，看效果）
- [ ] **Step T5.3**: Commit + tag `m1-zobrist`：

```bash
git tag -a m1-zobrist -m "Day 3 (5/1): zobrist hashing + transposition table.
- 64-bit incremental hash (splitmix64-seeded random table, side-to-move key)
- TT with EXACT/LOWER/UPPER bounds, 1M entries (~32MB)
- d=4 self-play node count down ~XX% vs no-TT baseline
- Ready for 5/2 iterative deepening + killer move"
git push origin main && git push origin m1-zobrist
```

---

## Self-Review

✅ **Spec 覆盖:** § 5.3 TTEntry / § 3.1 αβ + 置换表 / § 7 节奏 5/1
⚠️ **Day 3 不覆盖:** 迭代加深 + Killer Move（5/2 周六晚）/ History Heuristic（5/2）
✅ **类型一致性:** TTFlag / TTEntry / Z_KEY 在 zobrist.h 一处定义；board.zobrist_hash 字段早在 4/29 已存在；alphabeta 签名不变（仍是 negamax 风格）

**风险记录:**
- 如果 d=4 速度仍很慢（> 5 秒/步），下一步（5/2）的迭代加深 + Killer Move 必须落地，否则 d=6 永远到不了
- TT collision：用完整 64-bit `key` 比对（而不是只看 index）已经把 collision 概率降到 2^-44 量级，可忽略
- 跨着法 TT 复用 vs 清表：当前每次 `search_best_move` 入口 tt_clear。5/2 加迭代加深后，**保留** TT 是关键加速手段，到时候改成只在新游戏开始时清表

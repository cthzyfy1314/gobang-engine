# Day 1 (4/29): Board + Search Framework Implementation Plan

> ✅ **STATUS: DONE 2026-04-29** — tag `m1-prep-day1` pushed. 45/45 board + 12/12 search tests pass. Self-play demo runs.
>
> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 1 个工作日内完成 `board.c/.h`（含国规不对称胜负判定）+ `search.c/.h` 框架（含 αβ 剪枝 + 邻近 2 圈剪枝 + placeholder 评估），让项目能跑出"虽然棋力弱但功能正确"的最小自我对战。

**Architecture:** 模块化 C99 项目。`board` 提供棋盘操作；`search` 调用 `board` + placeholder evaluator 做 αβ 搜索。两个模块通过单元测试驱动 (TDD)，最后整合成最小 main 验证管线打通。

**Tech Stack:** C99 / MSVC / Windows / 自写极简测试 runner（assert + 计数 + 颜色输出，零依赖）/ build.bat

**对应 Spec 节段:** 第 4 节模块（board + search）/ 第 5.1 数据结构 / 第 7 节 4/29 行 / 第 8.1 单元测试

**对应里程碑:** M1 准备阶段（M1 完整版需要 4/30 完成 pattern 后才能"第一次能自我对战"）

**预算:** 1 工作日（4/29 周三全天）

---

## File Structure

下面是本 plan 完成后项目应有的文件树：

```
gobang-engine/
├── .gitignore                     (已有)
├── README.md                      (已有)
├── build.bat                      ★ 新建
├── src/
│   ├── main.c                     ★ 新建（最小 stub）
│   ├── board.h                    ★ 新建
│   ├── board.c                    ★ 新建
│   ├── search.h                   ★ 新建
│   └── search.c                   ★ 新建
├── tests/
│   ├── test_runner.h              ★ 新建（共享宏 + 计数）
│   ├── test_board.c               ★ 新建
│   └── test_search.c              ★ 新建
└── docs/
    └── superpowers/
        ├── specs/2026-04-28-gobang-engine-design.md   (已有)
        └── plans/2026-04-29-board-and-search-framework.md   (本文档)
```

**职责划分:**
- `board.h/.c` — 棋盘状态 + 落子/撤子 + **不对称胜负判定**（黑长连负 / 白长连胜）
- `search.h/.c` — αβ minimax 框架 + 邻近 2 圈候选生成 + placeholder evaluator
- `tests/test_runner.h` — 共享测试宏 (`ASSERT_EQ` 等)
- `tests/test_board.c` — board 模块单元测试
- `tests/test_search.c` — search 模块单元测试
- `main.c` — 暂时只是个能跑起来证明编译链通的 stub

**关键决策:**
- 暂时**不实现** zobrist 哈希（5/1 才做），但 `Board.zobrist_hash` 字段先放着
- 暂时**不实现** forbid（5/3-5/5 才做），但 `Board.forbid_enabled` 字段先放着
- 评估函数用最简陋的"中央距离倾向"占位，后天 (4/30) pattern 模块替换

---

## Task 1: 创建项目骨架（src/、tests/、main.c stub）

**Files:**
- Create: `src/main.c`
- Modify: 无

- [ ] **Step 1.1: 在项目根创建 `src/` 和 `tests/` 目录**

```bash
cd /c/Users/cthzy/Desktop/gobang-engine
mkdir -p src tests
```

- [ ] **Step 1.2: 写 `src/main.c` stub（仅证明链路能编译）**

```c
/* src/main.c — Day 1 stub. 后续由 ui 模块接管。 */
#include <stdio.h>

int main(void) {
    printf("gobang-engine: build OK (Day 1 stub)\n");
    return 0;
}
```

- [ ] **Step 1.3: Commit**

```bash
git add src/main.c
git commit -m "chore: scaffold src/ and tests/ directories with main.c stub"
```

---

## Task 2: 写 `tests/test_runner.h`（共享测试宏）

**Files:**
- Create: `tests/test_runner.h`

- [ ] **Step 2.1: 写 test_runner.h**

```c
/* tests/test_runner.h — 自写极简测试 runner，零第三方依赖。
 * 用法：每个 test_*.c 文件在 main() 里调用各 test_* 函数。
 * 函数内用 ASSERT_EQ / ASSERT_TRUE / ASSERT_FALSE 检查。
 * main() 末尾打印 X/Y passed。
 */
#ifndef TEST_RUNNER_H_
#define TEST_RUNNER_H_

#include <stdio.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_EQ(actual, expected, msg) do {                              \
    tests_run++;                                                           \
    if ((actual) == (expected)) {                                          \
        tests_passed++;                                                    \
        printf("  [PASS] %s\n", msg);                                      \
    } else {                                                               \
        printf("  [FAIL] %s  (got %d, expected %d)  at %s:%d\n",           \
               msg, (int)(actual), (int)(expected), __FILE__, __LINE__);   \
    }                                                                      \
} while(0)

#define ASSERT_TRUE(cond, msg)  ASSERT_EQ((cond) ? 1 : 0, 1, msg)
#define ASSERT_FALSE(cond, msg) ASSERT_EQ((cond) ? 1 : 0, 0, msg)

#define TEST_REPORT(suite_name) do {                                       \
    printf("\n=== %s: %d / %d passed ===\n",                               \
           suite_name, tests_passed, tests_run);                           \
    return (tests_run == tests_passed) ? 0 : 1;                            \
} while(0)

#endif /* TEST_RUNNER_H_ */
```

- [ ] **Step 2.2: Commit**

```bash
git add tests/test_runner.h
git commit -m "test: add minimal zero-dependency test runner header"
```

---

## Task 3: 写 `src/board.h` 接口

**Files:**
- Create: `src/board.h`

- [ ] **Step 3.1: 写 board.h**

```c
/* src/board.h — 五子棋棋盘数据结构 + 操作接口
 * 对应 Spec § 4.1 board 模块 / § 5.1 Board 结构 / § 1.4 国规 9.1
 */
#ifndef BOARD_H_
#define BOARD_H_

#include <stdint.h>
#include <stdbool.h>

#define BOARD_SIZE 15
#define EMPTY 0
#define BLACK 1
#define WHITE 2

typedef struct {
    int8_t row;     /* 0-14 */
    int8_t col;     /* 0-14 */
    int8_t color;   /* BLACK or WHITE */
} Move;

typedef struct {
    uint8_t cells[BOARD_SIZE][BOARD_SIZE];   /* 0=EMPTY, 1=BLACK, 2=WHITE */
    uint16_t move_count;
    uint64_t zobrist_hash;                   /* Day 1 placeholder, 留 0 */
    int side_to_move;                        /* BLACK or WHITE */
    Move history[BOARD_SIZE * BOARD_SIZE];
    bool forbid_enabled;                     /* Day 1 placeholder, 默认 true */
} Board;

/* GameResult — 对应 Spec § 3.4 + § 1.4 国规 9.1 / 9.2 */
typedef enum {
    RESULT_NONE = 0,
    RESULT_BLACK_WIN = 1,                    /* 黑五连，包括"黑五连+禁手同时"特例 */
    RESULT_WHITE_WIN_NORMAL = 2,             /* 白五连或白长连 */
    RESULT_WHITE_WIN_BY_BLACK_FORBID = 3,    /* 黑禁手判负，Day 1 暂不触发 */
    RESULT_DRAW = 4                          /* 全盘满 */
} GameResult;

/* 初始化空棋盘，黑先 */
void board_init(Board *b);

/* 在 (row,col) 落 color。成功返回 true；位置非法或已占用返回 false。 */
bool board_place(Board *b, int row, int col, int color);

/* 撤销最近一步。栈空返回 false。 */
bool board_undo(Board *b);

/* 检查胜负。
 * 国规不对称：
 *   黑方 5 连 → RESULT_BLACK_WIN
 *   黑方 ≥6 连（长连）→ RESULT_WHITE_WIN_NORMAL（长连禁手判黑负）
 *   白方 ≥5 连 → RESULT_WHITE_WIN_NORMAL（白长连视同五连胜）
 *   全盘满 → RESULT_DRAW
 *   其他 → RESULT_NONE
 */
GameResult board_check_winner(const Board *b);

/* 棋盘是否全部下满 */
bool board_is_full(const Board *b);

/* 坐标越界检查 */
bool board_in_bounds(int row, int col);

#endif /* BOARD_H_ */
```

- [ ] **Step 3.2: Commit**

```bash
git add src/board.h
git commit -m "feat(board): declare Board struct and public API"
```

---

## Task 4: TDD `board_init` + `board_in_bounds`

**Files:**
- Create: `tests/test_board.c`
- Create: `src/board.c`

- [ ] **Step 4.1: 写 test_board.c 测试 init + in_bounds（先失败）**

```c
/* tests/test_board.c */
#include "test_runner.h"
#include "../src/board.h"

static void test_board_init(void) {
    Board b;
    board_init(&b);
    ASSERT_EQ(b.move_count, 0, "init: move_count == 0");
    ASSERT_EQ(b.side_to_move, BLACK, "init: black to move first");
    ASSERT_EQ(b.zobrist_hash, 0, "init: zobrist 0 (placeholder)");
    ASSERT_TRUE(b.forbid_enabled, "init: forbid_enabled = true by default");
    ASSERT_EQ(b.cells[7][7], EMPTY, "init: center cell empty");
    ASSERT_EQ(b.cells[0][0], EMPTY, "init: corner cell empty");
}

static void test_board_in_bounds(void) {
    ASSERT_TRUE(board_in_bounds(0, 0), "bounds: (0,0) ok");
    ASSERT_TRUE(board_in_bounds(14, 14), "bounds: (14,14) ok");
    ASSERT_TRUE(board_in_bounds(7, 7), "bounds: (7,7) ok");
    ASSERT_FALSE(board_in_bounds(-1, 0), "bounds: (-1,0) out");
    ASSERT_FALSE(board_in_bounds(0, -1), "bounds: (0,-1) out");
    ASSERT_FALSE(board_in_bounds(15, 0), "bounds: (15,0) out");
    ASSERT_FALSE(board_in_bounds(0, 15), "bounds: (0,15) out");
}

int main(void) {
    test_board_init();
    test_board_in_bounds();
    TEST_REPORT("test_board");
}
```

- [ ] **Step 4.2: 试编译，预期"undefined reference"**

```bash
cd /c/Users/cthzy/Desktop/gobang-engine
cl /W3 /TC /utf-8 /nologo tests/test_board.c /Fe:test_board.exe 2>&1 | head
```

Expected: linker error 因为还没有 board.c。

- [ ] **Step 4.3: 写最小 board.c 让前两个 test 过**

```c
/* src/board.c */
#define _CRT_SECURE_NO_WARNINGS
#include <string.h>
#include "board.h"

void board_init(Board *b) {
    memset(b->cells, EMPTY, sizeof(b->cells));
    b->move_count = 0;
    b->zobrist_hash = 0;
    b->side_to_move = BLACK;
    b->forbid_enabled = true;
    /* history 不必清零，靠 move_count 控制有效范围 */
}

bool board_in_bounds(int row, int col) {
    return row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE;
}

/* 占位实现，让 link 通过。后续 Task 会完整实现。 */
bool board_place(Board *b, int row, int col, int color) {
    (void)b; (void)row; (void)col; (void)color;
    return false;
}

bool board_undo(Board *b) {
    (void)b;
    return false;
}

GameResult board_check_winner(const Board *b) {
    (void)b;
    return RESULT_NONE;
}

bool board_is_full(const Board *b) {
    (void)b;
    return false;
}
```

- [ ] **Step 4.4: 编译并运行测试**

```bash
cl /W3 /TC /utf-8 /nologo src/board.c tests/test_board.c /Fe:test_board.exe
./test_board.exe
```

Expected: `=== test_board: 9 / 9 passed ===`，退出码 0。

- [ ] **Step 4.5: Commit**

```bash
git add src/board.c src/board.h tests/test_board.c
git commit -m "feat(board): implement board_init and board_in_bounds with tests"
```

---

## Task 5: TDD `board_place` + `board_undo`

**Files:**
- Modify: `tests/test_board.c`
- Modify: `src/board.c`

- [ ] **Step 5.1: 在 test_board.c 加 place/undo 测试**

在 `int main(void) {` 前插入：

```c
static void test_board_place_basic(void) {
    Board b; board_init(&b);
    ASSERT_TRUE(board_place(&b, 7, 7, BLACK), "place: black at (7,7) ok");
    ASSERT_EQ(b.cells[7][7], BLACK, "place: cell[7][7] is BLACK");
    ASSERT_EQ(b.move_count, 1, "place: move_count = 1");
    ASSERT_EQ(b.side_to_move, WHITE, "place: side flipped to WHITE");
}

static void test_board_place_invalid(void) {
    Board b; board_init(&b);
    board_place(&b, 7, 7, BLACK);
    ASSERT_FALSE(board_place(&b, 7, 7, WHITE), "place: occupied cell rejected");
    ASSERT_FALSE(board_place(&b, -1, 0, WHITE), "place: out-of-bounds rejected");
    ASSERT_FALSE(board_place(&b, 15, 0, WHITE), "place: out-of-bounds (15) rejected");
    ASSERT_EQ(b.move_count, 1, "place: invalid moves don't increment");
}

static void test_board_undo(void) {
    Board b; board_init(&b);
    board_place(&b, 7, 7, BLACK);
    board_place(&b, 7, 8, WHITE);
    ASSERT_EQ(b.move_count, 2, "undo: pre-state move_count = 2");
    ASSERT_TRUE(board_undo(&b), "undo: returns true");
    ASSERT_EQ(b.cells[7][8], EMPTY, "undo: (7,8) cleared");
    ASSERT_EQ(b.cells[7][7], BLACK, "undo: (7,7) preserved");
    ASSERT_EQ(b.move_count, 1, "undo: move_count = 1");
    ASSERT_EQ(b.side_to_move, WHITE, "undo: side back to WHITE (next mover)");
    ASSERT_TRUE(board_undo(&b), "undo: again ok");
    ASSERT_EQ(b.move_count, 0, "undo: empty board");
    ASSERT_FALSE(board_undo(&b), "undo: empty stack returns false");
}

static void test_board_place_undo_idempotent(void) {
    /* place 后立即 undo 应该回到初始状态 */
    Board b; board_init(&b);
    Board snap = b;
    board_place(&b, 5, 5, BLACK);
    board_undo(&b);
    /* 比较关键字段（cells 和 move_count 和 side_to_move） */
    ASSERT_EQ(memcmp(b.cells, snap.cells, sizeof(b.cells)), 0,
              "round-trip: cells identical");
    ASSERT_EQ(b.move_count, snap.move_count, "round-trip: move_count");
    ASSERT_EQ(b.side_to_move, snap.side_to_move, "round-trip: side");
}
```

在 `main()` 里 init/in_bounds 之后调用：

```c
    test_board_place_basic();
    test_board_place_invalid();
    test_board_undo();
    test_board_place_undo_idempotent();
```

并在文件顶部加 `#include <string.h>`（用于 memcmp）。

- [ ] **Step 5.2: 编译运行测试，预期 place/undo 全 FAIL**

```bash
cl /W3 /TC /utf-8 /nologo src/board.c tests/test_board.c /Fe:test_board.exe
./test_board.exe
```

Expected: init/bounds 通过，place/undo 全 FAIL（因为还是 stub）。

- [ ] **Step 5.3: 实现 board_place 和 board_undo**

替换 board.c 中 board_place 和 board_undo 的 stub：

```c
bool board_place(Board *b, int row, int col, int color) {
    if (!board_in_bounds(row, col)) return false;
    if (b->cells[row][col] != EMPTY) return false;
    if (color != BLACK && color != WHITE) return false;
    
    b->cells[row][col] = (uint8_t)color;
    b->history[b->move_count].row = (int8_t)row;
    b->history[b->move_count].col = (int8_t)col;
    b->history[b->move_count].color = (int8_t)color;
    b->move_count++;
    b->side_to_move = (color == BLACK) ? WHITE : BLACK;
    return true;
}

bool board_undo(Board *b) {
    if (b->move_count == 0) return false;
    b->move_count--;
    Move *m = &b->history[b->move_count];
    b->cells[m->row][m->col] = EMPTY;
    b->side_to_move = m->color;   /* 撤销后下一个轮到的还是当时落子那方 */
    return true;
}
```

- [ ] **Step 5.4: 编译运行，预期全部通过**

```bash
cl /W3 /TC /utf-8 /nologo src/board.c tests/test_board.c /Fe:test_board.exe
./test_board.exe
```

Expected: `=== test_board: 24 / 24 passed ===`（具体数字以你实际计数为准，全 pass）

- [ ] **Step 5.5: Commit**

```bash
git add src/board.c tests/test_board.c
git commit -m "feat(board): implement place/undo with idempotency tests"
```

---

## Task 6: TDD `board_is_full`

**Files:**
- Modify: `tests/test_board.c`
- Modify: `src/board.c`

- [ ] **Step 6.1: 加 is_full 测试**

```c
static void test_board_is_full(void) {
    Board b; board_init(&b);
    ASSERT_FALSE(board_is_full(&b), "is_full: empty board not full");
    
    /* 填满整个棋盘（225 格）。轮流黑白以维持 side_to_move 不影响。 */
    int color = BLACK;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            board_place(&b, r, c, color);
            color = (color == BLACK) ? WHITE : BLACK;
        }
    }
    ASSERT_TRUE(board_is_full(&b), "is_full: 225 stones placed");
    ASSERT_EQ(b.move_count, 225, "is_full: move_count == 225");
}
```

main 里加：

```c
    test_board_is_full();
```

- [ ] **Step 6.2: 实现 is_full**

```c
bool board_is_full(const Board *b) {
    return b->move_count >= (uint16_t)(BOARD_SIZE * BOARD_SIZE);
}
```

- [ ] **Step 6.3: 编译运行**

```bash
cl /W3 /TC /utf-8 /nologo src/board.c tests/test_board.c /Fe:test_board.exe
./test_board.exe
```

Expected: 全过

- [ ] **Step 6.4: Commit**

```bash
git add src/board.c tests/test_board.c
git commit -m "feat(board): implement board_is_full"
```

---

## Task 7: TDD `board_check_winner`（**关键，国规不对称**）

**Files:**
- Modify: `tests/test_board.c`
- Modify: `src/board.c`

- [ ] **Step 7.1: 加 6 个胜负判定测试（覆盖国规 9.1 不对称）**

```c
/* 辅助：水平连续放 count 个 color 棋子，从 (start_r, start_c) 开始 */
static void place_horizontal_run(Board *b, int start_r, int start_c, int count, int color) {
    for (int i = 0; i < count; i++) {
        b->cells[start_r][start_c + i] = (uint8_t)color;
    }
    /* 直接改 cells 避开 place 的 turn-switching 逻辑，方便构造任意局面 */
    b->move_count += (uint16_t)count;
}

static void test_winner_none_on_empty(void) {
    Board b; board_init(&b);
    ASSERT_EQ(board_check_winner(&b), RESULT_NONE, "winner: empty board = none");
}

static void test_winner_black_5_horizontal(void) {
    Board b; board_init(&b);
    place_horizontal_run(&b, 7, 5, 5, BLACK);  /* 黑 5 连，行 7，列 5-9 */
    ASSERT_EQ(board_check_winner(&b), RESULT_BLACK_WIN, "winner: black 5-in-row = BLACK_WIN");
}

static void test_winner_black_6_overline_negative(void) {
    /* 国规 8 + 9.1：黑 6 连（长连）= 黑负 = 白胜 */
    Board b; board_init(&b);
    place_horizontal_run(&b, 7, 5, 6, BLACK);  /* 黑 6 连 */
    ASSERT_EQ(board_check_winner(&b), RESULT_WHITE_WIN_NORMAL,
              "winner: black 6-in-row (overline) = WHITE_WIN");
}

static void test_winner_white_5(void) {
    Board b; board_init(&b);
    place_horizontal_run(&b, 7, 5, 5, WHITE);
    ASSERT_EQ(board_check_winner(&b), RESULT_WHITE_WIN_NORMAL, "winner: white 5 = WHITE_WIN");
}

static void test_winner_white_6_overline_still_win(void) {
    /* 国规 9.1：白长连视同五连，白方仍胜 */
    Board b; board_init(&b);
    place_horizontal_run(&b, 7, 5, 6, WHITE);
    ASSERT_EQ(board_check_winner(&b), RESULT_WHITE_WIN_NORMAL,
              "winner: white 6-in-row = WHITE_WIN (long connect counts as 5)");
}

static void test_winner_diagonal(void) {
    /* 测试斜方向（左上→右下）的检测 */
    Board b; board_init(&b);
    for (int i = 0; i < 5; i++) b.cells[3 + i][3 + i] = BLACK;
    b.move_count = 5;
    ASSERT_EQ(board_check_winner(&b), RESULT_BLACK_WIN, "winner: diagonal black 5 = BLACK_WIN");
}

static void test_winner_anti_diagonal(void) {
    /* 反斜方向（右上→左下） */
    Board b; board_init(&b);
    for (int i = 0; i < 5; i++) b.cells[3 + i][10 - i] = WHITE;
    b.move_count = 5;
    ASSERT_EQ(board_check_winner(&b), RESULT_WHITE_WIN_NORMAL,
              "winner: anti-diagonal white 5 = WHITE_WIN");
}

static void test_winner_vertical(void) {
    Board b; board_init(&b);
    for (int i = 0; i < 5; i++) b.cells[5 + i][7] = BLACK;
    b.move_count = 5;
    ASSERT_EQ(board_check_winner(&b), RESULT_BLACK_WIN, "winner: vertical black 5 = BLACK_WIN");
}

static void test_winner_4_in_row_no_win(void) {
    Board b; board_init(&b);
    place_horizontal_run(&b, 7, 5, 4, BLACK);
    ASSERT_EQ(board_check_winner(&b), RESULT_NONE, "winner: only 4 in row = no win");
}
```

main 里加：

```c
    test_winner_none_on_empty();
    test_winner_black_5_horizontal();
    test_winner_black_6_overline_negative();
    test_winner_white_5();
    test_winner_white_6_overline_still_win();
    test_winner_diagonal();
    test_winner_anti_diagonal();
    test_winner_vertical();
    test_winner_4_in_row_no_win();
```

- [ ] **Step 7.2: 编译并跑（预期 winner 测试全 FAIL）**

```bash
cl /W3 /TC /utf-8 /nologo src/board.c tests/test_board.c /Fe:test_board.exe
./test_board.exe
```

Expected: 之前的测试通过，winner_* 全部 FAIL。

- [ ] **Step 7.3: 实现 board_check_winner（4 方向扫描 + 不对称判定）**

替换 board.c 里的 board_check_winner stub：

```c
/* 给定起点 (r,c) 和方向 (dr,dc)，统计沿该方向 color 的最长连续子数。
 * 注意：包括起点本身（如果起点是 color）。
 */
static int count_run(const Board *b, int r, int c, int dr, int dc, int color) {
    int count = 0;
    while (board_in_bounds(r, c) && b->cells[r][c] == (uint8_t)color) {
        count++;
        r += dr;
        c += dc;
    }
    return count;
}

/* 对每个 cells[r][c]==color 的格子，沿 4 方向(向后)看最长连子。
 * 这样会重复计算但实现简单，BOARD_SIZE=15 性能够用。
 * 返回该 color 在棋盘上找到的最大连子数。
 */
static int max_run_for_color(const Board *b, int color) {
    int max_run = 0;
    static const int dirs[4][2] = { {0,1}, {1,0}, {1,1}, {1,-1} };
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (b->cells[r][c] != (uint8_t)color) continue;
            for (int d = 0; d < 4; d++) {
                int dr = dirs[d][0], dc = dirs[d][1];
                /* 只从一段的"起点"开始计数（前一格不是同色），避免重复 */
                int prev_r = r - dr, prev_c = c - dc;
                if (board_in_bounds(prev_r, prev_c) &&
                    b->cells[prev_r][prev_c] == (uint8_t)color) {
                    continue;
                }
                int run = count_run(b, r, c, dr, dc, color);
                if (run > max_run) max_run = run;
            }
        }
    }
    return max_run;
}

GameResult board_check_winner(const Board *b) {
    int black_max = max_run_for_color(b, BLACK);
    int white_max = max_run_for_color(b, WHITE);
    
    /* 国规不对称（Spec § 1.4 国规 9.1）：
     *   白 ≥5 连（含长连）→ 白胜
     *   黑 ==5 连 → 黑胜
     *   黑 ≥6 连（长连禁手）→ 白胜
     *   全盘满 → 和棋
     *
     * 注意：本 Day 1 版本不处理"黑五连+禁手同时形成→五连优先"的细节
     *       那是 forbid 模块的事（§ 3.4 + § 1.4 国规 9.2-c），Day 5/3 实现
     */
    if (white_max >= 5) return RESULT_WHITE_WIN_NORMAL;
    if (black_max == 5) return RESULT_BLACK_WIN;
    if (black_max >= 6) return RESULT_WHITE_WIN_NORMAL;  /* 黑长连 = 黑负 */
    if (board_is_full(b)) return RESULT_DRAW;
    return RESULT_NONE;
}
```

- [ ] **Step 7.4: 编译运行，预期全过**

```bash
cl /W3 /TC /utf-8 /nologo src/board.c tests/test_board.c /Fe:test_board.exe
./test_board.exe
```

Expected: 全部 PASS。如果 black_5 测试失败但 black_6 通过，可能是 `if` 顺序问题——`white_max >= 5` 检查时，黑棋 5 连不会触发；这就对了。如果黑 5 连跟黑 6 连测试都判 WHITE_WIN，说明你 black_max==5 的 if 写在了 black_max>=6 之后被遮蔽。仔细查 if 顺序。

- [ ] **Step 7.5: Commit**

```bash
git add src/board.c tests/test_board.c
git commit -m "feat(board): implement asymmetric check_winner per Renju rule 9.1"
```

---

## Task 8: 写 `build.bat`（基于井字棋的版本扩展）

**Files:**
- Create: `build.bat`

- [ ] **Step 8.1: 写 build.bat**

```batch
@echo off
rem =====================================================================
rem  gobang-engine - MSVC build script
rem  Usage: open "x64 Native Tools Command Prompt for VS", cd to root, run build.bat
rem  Output: gobang-engine.exe in project root
rem =====================================================================

cd /d %~dp0

rem 如果 cl 不在 PATH，定位 vs 工具链
where cl >nul 2>&1
if not errorlevel 1 goto compile

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] cl not found and vswhere missing.
    echo         Please open "x64 Native Tools Command Prompt for VS" and re-run.
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
    echo [ERROR] could not detect Visual Studio installation
    exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

:compile
if not exist build mkdir build

rem ========== 主程序 ==========
echo Building gobang-engine.exe...
cl /W3 /TC /utf-8 /nologo /Fe:gobang-engine.exe /Fo:build\ src\*.c
if errorlevel 1 ( echo [ERROR] main build failed & exit /b 1 )

rem ========== 单元测试 ==========
echo Building tests/test_board.exe...
cl /W3 /TC /utf-8 /nologo /Fe:test_board.exe /Fo:build\ ^
   src\board.c tests\test_board.c
if errorlevel 1 ( echo [ERROR] test_board build failed & exit /b 1 )

rem 后续 task 9 之后会加 test_search.exe

echo.
echo [OK] build complete.
echo Run:
echo   gobang-engine.exe
echo   test_board.exe
```

- [ ] **Step 8.2: 跑一次 build.bat 验证**

在 `x64 Native Tools Command Prompt for VS` 里：
```cmd
cd C:\Users\cthzy\Desktop\gobang-engine
build.bat
```

或在 git bash + 已激活 vcvars 的情况下：
```bash
./build.bat
```

Expected: 输出 `[OK] build complete.`，根目录有 `gobang-engine.exe` 和 `test_board.exe`。

- [ ] **Step 8.3: 跑测试**

```bash
./test_board.exe
```

Expected: 全 PASS。

- [ ] **Step 8.4: Commit**

```bash
git add build.bat
git commit -m "build: add build.bat for MSVC compilation"
```

---

## Task 9: 写 `src/search.h` 接口

**Files:**
- Create: `src/search.h`

- [ ] **Step 9.1: 写 search.h**

```c
/* src/search.h — αβ minimax 搜索框架
 * 对应 Spec § 4.1 search 模块 / § 3.1 主算法
 * Day 1 状态：评估函数是 placeholder（中央距离倾向）
 *             pattern 模块完成后 (4/30) 会替换 evaluate
 */
#ifndef SEARCH_H_
#define SEARCH_H_

#include "board.h"

#define SEARCH_INF      1000000
#define SEARCH_MAX_MOVES 256       /* 一次 generate 最多返回的候选数 */

typedef struct {
    Move best_move;
    int  score;
    long nodes_searched;
} SearchResult;

/* 找当前 side_to_move 的最佳着子，搜索深度 depth 层。
 * 返回 best_move 和 score（从 side_to_move 视角）。
 */
SearchResult search_best_move(Board *b, int depth);

/* Placeholder 评估函数。
 * Day 1: 中央距离倾向（越靠中心分越高）+ 简单子力差
 * 4/30 会被 pattern 模块完整替换。
 * 评估视角：返回值 > 0 表示对 side_to_move 有利。
 */
int search_evaluate(const Board *b);

/* 生成"邻近 2 圈"候选着子。
 * out 数组容量至少 SEARCH_MAX_MOVES。
 * 返回实际候选数。
 * 如果棋盘空，只返回中心点 (7,7)。
 */
int search_generate_neighbor_moves(const Board *b, Move *out);

#endif /* SEARCH_H_ */
```

- [ ] **Step 9.2: Commit**

```bash
git add src/search.h
git commit -m "feat(search): declare search API"
```

---

## Task 10: TDD `search_evaluate`（placeholder 中央倾向）

**Files:**
- Create: `tests/test_search.c`
- Create: `src/search.c`

- [ ] **Step 10.1: 写 test_search.c**

```c
/* tests/test_search.c */
#include "test_runner.h"
#include "../src/board.h"
#include "../src/search.h"

static void test_evaluate_empty(void) {
    Board b; board_init(&b);
    int score = search_evaluate(&b);
    ASSERT_EQ(score, 0, "evaluate: empty board score = 0");
}

static void test_evaluate_center_better_than_corner(void) {
    /* 黑下中央 vs 黑下角落，中央应分更高（从黑视角） */
    Board b1; board_init(&b1);
    b1.cells[7][7] = BLACK;
    b1.move_count = 1;
    b1.side_to_move = WHITE;  /* 评估时视角是下一手方 = WHITE */
    
    Board b2; board_init(&b2);
    b2.cells[0][0] = BLACK;
    b2.move_count = 1;
    b2.side_to_move = WHITE;
    
    int s1 = search_evaluate(&b1);
    int s2 = search_evaluate(&b2);
    /* 视角是白方，黑下中央对白不利，所以 s1 < s2 */
    ASSERT_TRUE(s1 < s2, "evaluate: white-perspective with black at center < black at corner");
}

int main(void) {
    test_evaluate_empty();
    test_evaluate_center_better_than_corner();
    /* 后续 task 还会加 test_generate_neighbor_moves / test_search_finds_winning_move */
    TEST_REPORT("test_search");
}
```

- [ ] **Step 10.2: 编译预期 link 失败（search.c 不存在）**

```bash
cl /W3 /TC /utf-8 /nologo src/board.c src/search.c tests/test_search.c /Fe:test_search.exe 2>&1 | head
```

Expected: cannot open source 'src/search.c'

- [ ] **Step 10.3: 写最小 search.c**

```c
/* src/search.c */
#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include "search.h"

/* Day 1 placeholder evaluator:
 *   "中央倾向"——离 (7,7) 越近的子值越高。
 *   评估返回值是从 side_to_move 视角。
 *   对每个棋子计算 (7-|7-r|) + (7-|7-c|) 的中央距离值。
 *   黑子贡献正值（如果 side_to_move 是黑）或负值（反之），白子相反。
 */
int search_evaluate(const Board *b) {
    int black_value = 0, white_value = 0;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            uint8_t cell = b->cells[r][c];
            if (cell == EMPTY) continue;
            int dr = r - 7; if (dr < 0) dr = -dr;
            int dc = c - 7; if (dc < 0) dc = -dc;
            int center_score = (7 - dr) + (7 - dc);
            if (cell == BLACK) black_value += center_score;
            else white_value += center_score;
        }
    }
    int diff = black_value - white_value;
    return (b->side_to_move == BLACK) ? diff : -diff;
}

/* 占位实现，后续 task 完整实现 */
int search_generate_neighbor_moves(const Board *b, Move *out) {
    (void)b; (void)out;
    return 0;
}

SearchResult search_best_move(Board *b, int depth) {
    (void)depth;
    SearchResult r = { .best_move = { -1, -1, b->side_to_move }, .score = 0, .nodes_searched = 0 };
    return r;
}
```

- [ ] **Step 10.4: 编译运行测试**

```bash
cl /W3 /TC /utf-8 /nologo src/board.c src/search.c tests/test_search.c /Fe:test_search.exe
./test_search.exe
```

Expected: `=== test_search: 2 / 2 passed ===`

- [ ] **Step 10.5: Commit**

```bash
git add src/search.c src/search.h tests/test_search.c
git commit -m "feat(search): placeholder evaluator with center-bias scoring"
```

---

## Task 11: TDD `search_generate_neighbor_moves`（邻近 2 圈剪枝）

**Files:**
- Modify: `tests/test_search.c`
- Modify: `src/search.c`

- [ ] **Step 11.1: 加测试**

在 test_search.c 加：

```c
static void test_generate_empty_board(void) {
    Board b; board_init(&b);
    Move out[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(&b, out);
    ASSERT_EQ(n, 1, "generate: empty board returns center only");
    ASSERT_EQ(out[0].row, 7, "generate: center is (7,7) row");
    ASSERT_EQ(out[0].col, 7, "generate: center is (7,7) col");
}

static void test_generate_one_stone(void) {
    /* 棋盘只有 (7,7) 黑子，周围 2 圈应是 5x5-1 = 24 个候选 */
    Board b; board_init(&b);
    b.cells[7][7] = BLACK;
    b.move_count = 1;
    Move out[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(&b, out);
    ASSERT_EQ(n, 24, "generate: one stone at center -> 24 neighbors (5x5-1)");
    /* 验证 (7,7) 不在候选里 */
    int found_center = 0;
    for (int i = 0; i < n; i++) if (out[i].row == 7 && out[i].col == 7) found_center = 1;
    ASSERT_EQ(found_center, 0, "generate: occupied cell excluded");
}

static void test_generate_corner_stone(void) {
    /* 角落 (0,0) 的 2 圈范围是 (0..2, 0..2) = 9 - 1 = 8 个 */
    Board b; board_init(&b);
    b.cells[0][0] = BLACK;
    b.move_count = 1;
    Move out[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(&b, out);
    ASSERT_EQ(n, 8, "generate: corner stone -> 3x3-1 = 8 neighbors");
}
```

main 里加调用这 3 个 test。

- [ ] **Step 11.2: 编译跑（预期 generate 全 FAIL）**

```bash
cl /W3 /TC /utf-8 /nologo src/board.c src/search.c tests/test_search.c /Fe:test_search.exe
./test_search.exe
```

- [ ] **Step 11.3: 实现 generate_neighbor_moves**

替换 search.c 里 `search_generate_neighbor_moves` 的 stub：

```c
int search_generate_neighbor_moves(const Board *b, Move *out) {
    if (b->move_count == 0) {
        /* 空棋盘只返回中心 */
        out[0].row = 7;
        out[0].col = 7;
        out[0].color = (int8_t)b->side_to_move;
        return 1;
    }
    
    /* "邻近 2 圈"标记表：mark[r][c]=1 表示 (r,c) 是某已落子的 2 圈内空位 */
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
```

- [ ] **Step 11.4: 编译运行**

```bash
cl /W3 /TC /utf-8 /nologo src/board.c src/search.c tests/test_search.c /Fe:test_search.exe
./test_search.exe
```

Expected: 全过

- [ ] **Step 11.5: Commit**

```bash
git add src/search.c tests/test_search.c
git commit -m "feat(search): generate_neighbor_moves with 2-ring radius pruning"
```

---

## Task 12: TDD `search_best_move`（αβ minimax）

**Files:**
- Modify: `tests/test_search.c`
- Modify: `src/search.c`

- [ ] **Step 12.1: 加 search 测试**

```c
static void test_search_returns_legal_move(void) {
    Board b; board_init(&b);
    b.cells[7][7] = BLACK; b.move_count = 1; b.side_to_move = WHITE;
    SearchResult r = search_best_move(&b, 2);
    ASSERT_TRUE(board_in_bounds(r.best_move.row, r.best_move.col),
                "search: returns in-bounds move");
    ASSERT_EQ(b.cells[r.best_move.row][r.best_move.col], EMPTY,
              "search: returns empty cell");
}

static void test_search_finds_immediate_win(void) {
    /* 构造黑方 4 连，下一步必胜（黑下完成 5 连） */
    Board b; board_init(&b);
    b.cells[7][3] = BLACK;
    b.cells[7][4] = BLACK;
    b.cells[7][5] = BLACK;
    b.cells[7][6] = BLACK;
    b.move_count = 4;
    b.side_to_move = BLACK;
    
    SearchResult r = search_best_move(&b, 1);  /* 深度 1 即可发现 */
    /* 黑方应选 (7,7) 或 (7,2) 完成 5 连 */
    int found_winning = (r.best_move.row == 7 && (r.best_move.col == 7 || r.best_move.col == 2));
    ASSERT_TRUE(found_winning, "search: finds winning move at (7,2) or (7,7)");
}

static void test_search_nodes_counted(void) {
    Board b; board_init(&b);
    b.cells[7][7] = BLACK; b.move_count = 1; b.side_to_move = WHITE;
    SearchResult r = search_best_move(&b, 2);
    ASSERT_TRUE(r.nodes_searched > 0, "search: nodes_searched > 0");
}
```

main 里加：

```c
    test_search_returns_legal_move();
    test_search_finds_immediate_win();
    test_search_nodes_counted();
```

- [ ] **Step 12.2: 编译跑预期 FAIL**

- [ ] **Step 12.3: 实现 αβ minimax**

替换 search.c 中 `search_best_move` 的 stub，并在文件顶部加：

```c
#include "board.h"
#include <limits.h>
```

新增内部函数 + 替换 search_best_move：

```c
/* αβ minimax 递归。
 * 返回值是从"当前到走方"角度的评分（negamax 风格简化版）。
 *   side_to_move = 接下来要走的玩家
 *   alpha/beta = 当前最佳可行下界/上界
 *   depth_left = 还要往深搜几层
 *
 * 返回值 ∈ [-INF, +INF]，正值表示"对当前到走方有利"。
 */
static int alphabeta(Board *b, int depth_left, int alpha, int beta, long *nodes) {
    (*nodes)++;
    
    /* 终局检查 */
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
    
    int best = -SEARCH_INF;
    for (int i = 0; i < n; i++) {
        int color = b->side_to_move;
        if (!board_place(b, moves[i].row, moves[i].col, color)) continue;
        int score = -alphabeta(b, depth_left - 1, -beta, -alpha, nodes);
        board_undo(b);
        
        if (score > best) best = score;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;  /* β 剪枝 */
    }
    return best;
}

SearchResult search_best_move(Board *b, int depth) {
    SearchResult result = { .best_move = { -1, -1, (int8_t)b->side_to_move },
                            .score = -SEARCH_INF, .nodes_searched = 0 };
    
    Move moves[SEARCH_MAX_MOVES];
    int n = search_generate_neighbor_moves(b, moves);
    if (n == 0) return result;
    
    int alpha = -SEARCH_INF, beta = SEARCH_INF;
    for (int i = 0; i < n; i++) {
        int color = b->side_to_move;
        if (!board_place(b, moves[i].row, moves[i].col, color)) continue;
        int score = -alphabeta(b, depth - 1, -beta, -alpha, &result.nodes_searched);
        board_undo(b);
        
        if (score > result.score) {
            result.score = score;
            result.best_move = moves[i];
        }
        if (score > alpha) alpha = score;
    }
    return result;
}
```

- [ ] **Step 12.4: 编译跑**

```bash
cl /W3 /TC /utf-8 /nologo src/board.c src/search.c tests/test_search.c /Fe:test_search.exe
./test_search.exe
```

Expected: 全过。如果 `test_search_finds_immediate_win` FAIL，意味着深度 1 的搜索没正确识别五连——检查 `board_check_winner` 在 alphabeta 顶部是否被正确调用，特别是落子后 side_to_move 已经翻转的情况。

- [ ] **Step 12.5: Commit**

```bash
git add src/search.c tests/test_search.c
git commit -m "feat(search): alpha-beta minimax with terminal detection"
```

---

## Task 13: 在 build.bat 加 test_search 编译

**Files:**
- Modify: `build.bat`

- [ ] **Step 13.1: 在 `rem 后续 task 9 之后会加 test_search.exe` 那行替换为**

```batch
echo Building test_search.exe...
cl /W3 /TC /utf-8 /nologo /Fe:test_search.exe /Fo:build\ ^
   src\board.c src\search.c tests\test_search.c
if errorlevel 1 ( echo [ERROR] test_search build failed & exit /b 1 )
```

并在最后的 echo Run 部分加一行 `echo   test_search.exe`。

- [ ] **Step 13.2: 跑 build.bat 验证**

```bash
./build.bat
./test_board.exe && ./test_search.exe
```

Expected: 两个测试 exe 都全过。

- [ ] **Step 13.3: Commit**

```bash
git add build.bat
git commit -m "build: include test_search in build.bat"
```

---

## Task 14: 让 main.c 跑一个最小的 self-play 演示

**Files:**
- Modify: `src/main.c`

这是当日的"集成验证"——证明 board + search 拼起来真能下棋（虽然棋力是 random+center bias 级别）。

- [ ] **Step 14.1: 改 main.c 跑 5 步自我对战**

```c
/* src/main.c — Day 1 minimal self-play demo
 * 让黑白方各自调用 search_best_move 互下 10 步，打印过程。
 * Day 4 ui 模块完成后会被替换成正式 GUI。
 */
#include <stdio.h>
#include "board.h"
#include "search.h"

static void print_board_minimal(const Board *b) {
    printf("    ");
    for (int c = 0; c < BOARD_SIZE; c++) printf("%c ", 'A' + c);
    printf("\n");
    for (int r = 0; r < BOARD_SIZE; r++) {
        printf("%2d  ", BOARD_SIZE - r);
        for (int c = 0; c < BOARD_SIZE; c++) {
            uint8_t v = b->cells[r][c];
            printf("%c ", v == EMPTY ? '.' : v == BLACK ? 'X' : 'O');
        }
        printf("\n");
    }
    printf("\n");
}

int main(void) {
    Board b;
    board_init(&b);
    
    printf("gobang-engine Day 1 self-play demo (10 plies, depth=2)\n\n");
    
    for (int ply = 0; ply < 10; ply++) {
        SearchResult r = search_best_move(&b, 2);
        if (r.best_move.row < 0) {
            printf("No legal move. Stopping.\n");
            break;
        }
        const char *who = (b.side_to_move == BLACK) ? "BLACK(X)" : "WHITE(O)";
        printf("Ply %2d: %s plays %c%d  (score=%d, nodes=%ld)\n",
               ply + 1, who, 'A' + r.best_move.col, BOARD_SIZE - r.best_move.row,
               r.score, r.nodes_searched);
        board_place(&b, r.best_move.row, r.best_move.col, r.best_move.color);
        
        GameResult res = board_check_winner(&b);
        if (res != RESULT_NONE) {
            print_board_minimal(&b);
            printf("Game ended: result = %d\n", res);
            return 0;
        }
    }
    
    print_board_minimal(&b);
    printf("[Demo finished — 10 plies, no winner yet]\n");
    return 0;
}
```

- [ ] **Step 14.2: 修改 build.bat 让 main 编译时也包含 search.c**

build.bat 里 main 编译那行已经是 `src\*.c`，会自动包含 search.c。无需改动。但确认一下：

```bash
./build.bat
```

如果 link 报 search 函数 undefined，把那一行改成：
```batch
cl /W3 /TC /utf-8 /nologo /Fe:gobang-engine.exe /Fo:build\ src\main.c src\board.c src\search.c
```

- [ ] **Step 14.3: 跑 demo**

```bash
./gobang-engine.exe
```

Expected: 输出 10 步自我对战 + 棋盘可视化。具体着子位置可能在中央周围（因为评估函数倾向中央）。

- [ ] **Step 14.4: Commit**

```bash
git add src/main.c build.bat
git commit -m "feat(main): minimal self-play demo for Day 1 integration check"
```

---

## Task 15: 打 Day 1 完成 tag 并 push

**Files:**
- 无（仅 git 操作）

- [ ] **Step 15.1: 跑全套测试 + demo 最后验证**

```bash
./build.bat && ./test_board.exe && ./test_search.exe && ./gobang-engine.exe
```

Expected: 两测试 exe 全 PASS，demo 输出 10 步对战。

- [ ] **Step 15.2: 打 tag**

```bash
git tag -a m1-prep-day1 -m "Day 1 (4/29): board + search framework done.
- board.c/.h with asymmetric Renju winner detection (rule 9.1)
- search.c/.h with alpha-beta + 2-ring move generation + placeholder evaluator
- All unit tests pass, minimal self-play demo runs"
```

- [ ] **Step 15.3: push**

```bash
git push origin main
git push origin m1-prep-day1
```

- [ ] **Step 15.4: 验证 GitHub 上的状态**

```bash
gh repo view cthzyfy1314/gobang-engine --json defaultBranchRef,latestRelease | head
```

或浏览器打开 https://github.com/cthzyfy1314/gobang-engine/tags 看 tag。

---

## Day 1 Done — 验收清单

完成本 plan 后你应该有：

- ✅ `gobang-engine.exe` — 跑得起的最小自我对战 demo
- ✅ `test_board.exe` — board 模块单元测试，全 PASS（含国规 9.1 不对称胜负 4 个 case）
- ✅ `test_search.exe` — search 模块单元测试，全 PASS（含立即胜检测）
- ✅ git 仓库 main 分支 ≥ 13 commits + 1 个 tag `m1-prep-day1`
- ✅ 项目骨架 src/、tests/、build.bat 完整
- ✅ Board 数据结构 含 forbid_enabled / zobrist_hash 占位字段，4/30 后续 task 可直接接入

**4/30 周三的 plan 会做**：pattern.c/.h（棋型识别 + 评估函数），替换 search.c 里的 placeholder evaluator，让搜索真正"下五子棋"而不是"占中央"。

**风险记录**：如果 4/29 的某个 task 卡住超 1 小时，立刻评估是否吃 buffer 还是简化。Task 7（不对称胜负）和 Task 12（αβ minimax）是当日两个最难关卡，建议预留连续 2 小时无打扰时间。

---

## Self-Review

✅ **Spec 覆盖检查**：
- § 4.1 board 模块 → Tasks 3-7 ✅
- § 4.1 search 模块（不含 Killer Move/迭代加深，那是 P1） → Tasks 9-12 ✅
- § 5.1 Board 结构（含 forbid_enabled） → Task 3 ✅
- § 8.1 board 单元测试（不对称胜负 4 case + 落子撤子可逆） → Task 5/7 ✅
- § 8.1 search 单元测试 → Task 12 ✅
- § 1.4 国规 9.1（不对称胜负） → Task 7 测试覆盖 ✅
- § 1.4 国规 9.2-c（黑五连+禁手同时） → 注释说明留给 forbid 模块（5/3-5/5）✅

⚠️ **Day 1 不覆盖**（已声明）：
- pattern 模块（4/30）
- forbid 模块（5/3-5/5）
- ui 模块（5/6-5/7）
- VCF（5/8）
- 完整 main.c（5/6 ui 完成后）

✅ **Placeholder 扫描**：plan 中无 "TBD"/"TODO"，每个 step 含完整代码或精确命令。

✅ **类型一致性**：
- `Board`、`Move`、`GameResult`、`SearchResult` 在 board.h / search.h 一处声明，被各 task 引用一致
- `BOARD_SIZE = 15` 一次定义
- `EMPTY/BLACK/WHITE = 0/1/2` 一次定义
- 函数命名 `board_xxx` / `search_xxx` 全程一致

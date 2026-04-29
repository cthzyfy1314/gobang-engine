# Day 2 (4/30): Pattern Recognition + Evaluator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 写完 `pattern.c/.h`（7 种棋型识别 + 评分函数），替换 `search.c` 里的中央倾向 placeholder evaluator，让 self-play 表现出"会下棋"的特征（聚焦攻防而非占中央），达成 spec § 7.3 **M1 里程碑**——第一次能自我对战（不带禁手）。

**Architecture:** 简化版连续段识别——对每条线（横/竖/主斜/副斜）扫描连续同色段，根据"段长 + 两端开放性"分类到 7 种棋型，再按 spec § 3.2 评分表加权求和。**不抄** xl-engine 的 18 棋型 + 3 进制查表（那 2700 行 C++ 在 9d 预算下嚼不动）；**不实现**跳活三/跳活四（`_XX_X_`、`_X_XXX_`），那个精度损失能接受，禁手模块（5/3-5/5）会单独补"真活三"判定。

**Tech Stack:** C99 / MSVC（沿用 4/29 的 build.bat）/ 自写测试 runner（4/29 已建）/ TDD 全程

**对应 Spec 节段:** § 3.2 评估函数 / § 4.1 pattern 模块 / § 8.1 pattern 单元测试 / § 7 节奏 4/30 行

**对应里程碑:** **M1 (4/30 晚)** —— 第一次能自我对战（不带禁手）

**预算:** 1 工作日（4/30 周四全天，约 7-8 小时）

---

## File Structure

本 plan 完成后的文件树新增/修改：

```
gobang-engine/
├── src/
│   ├── board.h           (已有，不改)
│   ├── board.c           (已有，不改)
│   ├── search.h          (已有，不改)
│   ├── search.c          ★ 修改：替换 search_evaluate
│   ├── pattern.h         ★ 新建
│   ├── pattern.c         ★ 新建
│   └── main.c            (已有，可能微调输出)
├── tests/
│   ├── test_runner.h     (已有)
│   ├── test_board.c      (已有)
│   ├── test_search.c     (已有)
│   └── test_pattern.c    ★ 新建
└── build.bat             ★ 修改：加 test_pattern.exe
```

**职责划分:**
- `pattern.h/.c` — 棋型识别（7 种）+ 整盘评估函数 + 评分表
- `test_pattern.c` — pattern 模块单元测试
- `search.c` 改动只有 1 处：`search_evaluate` 调用 `pattern_evaluate`

**棋型枚举（7 种 + NONE）：**
| 枚举值 | 中文名 | 定义 | 评分（spec § 3.2）|
|---|---|---|---|
| `PAT_NONE` | — | 不计分 | 0 |
| `PAT_SLEEP_TWO` | 眠二 | 一端被堵的连续 2 子 | 10 |
| `PAT_OPEN_TWO` | 活二 | 双端开放的连续 2 子 | 100 |
| `PAT_SLEEP_THREE` | 眠三 | 一端被堵的连续 3 子 | 100 |
| `PAT_OPEN_THREE` | 活三 | 双端开放的连续 3 子 | 1,000 |
| `PAT_SIMPLE_FOUR` | 冲四 | 一端被堵的连续 4 子 | 1,000 |
| `PAT_OPEN_FOUR` | 活四 | 双端开放的连续 4 子 | 10,000 |
| `PAT_FIVE` | 五连 | 连续 ≥5 子 | 100,000 |

**关键决策:**
- 跳子棋型（`_XX_X_` 跳活三 / `_X_XXX_` 跳活四）**不识别** —— 精度损失留给 forbid 模块补
- 黑长连（≥6 连）评估时计入 `PAT_FIVE`（不影响搜索决策——`board_check_winner` 已处理黑长连=白胜，αβ 不会选）
- 整盘评估：黑总分 − 白总分，再按 `side_to_move` 视角翻转（与 4/29 placeholder 保持视角一致）

---

## Task 1: pattern.h 接口 + Pattern 枚举 + 评分表

**Files:**
- Create: `src/pattern.h`

- [ ] **Step 1.1: 写 pattern.h**

```c
/* src/pattern.h — 棋型识别 + 评估函数
 * 对应 Spec § 3.2 棋型评估 / § 4.1 pattern 模块
 *
 * 简化版（7 棋型 + NONE）：
 *   只识别连续段，不识别跳子（_XX_X_、_X_XXX_）。
 *   跳活三 / 真活三的精确判定留给 forbid 模块（5/3-5/5）。
 */
#ifndef PATTERN_H_
#define PATTERN_H_

#include <stdint.h>
#include "board.h"

typedef enum {
    PAT_NONE        = 0,
    PAT_SLEEP_TWO   = 1,   /* 眠二：一端被堵的 2 连 */
    PAT_OPEN_TWO    = 2,   /* 活二：双端开放的 2 连 */
    PAT_SLEEP_THREE = 3,   /* 眠三：一端被堵的 3 连 */
    PAT_OPEN_THREE  = 4,   /* 活三：双端开放的 3 连 */
    PAT_SIMPLE_FOUR = 5,   /* 冲四：一端被堵的 4 连 */
    PAT_OPEN_FOUR   = 6,   /* 活四：双端开放的 4 连 */
    PAT_FIVE        = 7,   /* 五连（含 ≥6 长连）*/
    PAT_COUNT       = 8
} Pattern;

/* 评分表（spec § 3.2）。索引由 Pattern 枚举决定。 */
extern const int PATTERN_SCORE[PAT_COUNT];

/* 棋型计数容器：counts[p] = 棋型 p 在当前局面出现的次数 */
typedef struct {
    int counts[PAT_COUNT];
} PatternStats;

/* 在 1D 线 line[0..len-1] 上统计 color 的棋型计数，累加到 out。
 * line 元素：0=EMPTY, 1=BLACK, 2=WHITE。
 * 边界视为"非 color"（不开放）。
 */
void pattern_count_in_line(const int8_t *line, int len, int color, PatternStats *out);

/* 在整盘上统计 color 的棋型计数（4 方向：横/竖/主斜/副斜）。
 * out 在调用前应清零（{0}）。
 */
void pattern_count_for_color(const Board *b, int color, PatternStats *out);

/* 整盘评估：黑总分 − 白总分，再按 side_to_move 视角翻转。
 * 返回值 > 0 表示对 side_to_move 有利。
 * 设计意图：替代 search.c 中 4/29 的 placeholder evaluator。
 */
int pattern_evaluate(const Board *b);

#endif /* PATTERN_H_ */
```

- [ ] **Step 1.2: Commit**

```bash
git add src/pattern.h
git commit -m "feat(pattern): declare Pattern enum and evaluator API"
```

---

## Task 2: pattern.c 起手 + 评分表 + stub 函数（让 link 通过）

**Files:**
- Create: `src/pattern.c`

- [ ] **Step 2.1: 写 pattern.c 骨架**

```c
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
    100000     /* PAT_FIVE */
};

/* stub —— Task 3-6 实现 */
void pattern_count_in_line(const int8_t *line, int len, int color, PatternStats *out) {
    (void)line; (void)len; (void)color; (void)out;
}

/* stub —— Task 7 实现 */
void pattern_count_for_color(const Board *b, int color, PatternStats *out) {
    (void)b; (void)color; (void)out;
}

/* stub —— Task 8 实现 */
int pattern_evaluate(const Board *b) {
    (void)b;
    return 0;
}
```

- [ ] **Step 2.2: 编译验证（让 search 不报 link error）**

```bash
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && cl /W3 /TC /utf-8 /nologo src\board.c src\search.c src\pattern.c src\main.c /Fe:gobang-engine.exe /Fo:build\\ 2>&1'
```

Expected: 无 error。warning C4100（unused parameter）允许，因为 stub。

- [ ] **Step 2.3: Commit**

```bash
git add src/pattern.c
git commit -m "feat(pattern): pattern.c skeleton with score table and stubs"
```

---

## Task 3: TDD `pattern_count_in_line` —— 五连

**Files:**
- Create: `tests/test_pattern.c`
- Modify: `src/pattern.c`

- [ ] **Step 3.1: 写 test_pattern.c（先测五连）**

```c
/* tests/test_pattern.c */
#include <string.h>
#include "test_runner.h"
#include "../src/pattern.h"

/* 辅助：用字符串字面量构造一条线。'_' = EMPTY, 'X' = BLACK, 'O' = WHITE */
static int build_line(const char *s, int8_t *out) {
    int n = 0;
    for (int i = 0; s[i]; i++) {
        switch (s[i]) {
            case '_': out[n++] = 0; break;
            case 'X': out[n++] = 1; break;
            case 'O': out[n++] = 2; break;
            default:  break;
        }
    }
    return n;
}

static void test_five_isolated(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("__XXXXX__", line);
    pattern_count_in_line(line, n, 1 /*BLACK*/, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "five: __XXXXX__ -> FIVE x1");
    ASSERT_EQ(s.counts[PAT_OPEN_FOUR], 0, "five: not counted as OPEN_FOUR");
    ASSERT_EQ(s.counts[PAT_OPEN_THREE], 0, "five: not counted as OPEN_THREE");
}

static void test_five_at_edge(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("XXXXX", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "five: XXXXX (full line) -> FIVE x1");
}

static void test_six_overline_counts_as_five(void) {
    /* 黑长连 6 子在 evaluate 视角同样算 PAT_FIVE（不影响搜索，board.c 已判负）*/
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("_XXXXXX_", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "overline: _XXXXXX_ -> FIVE x1");
}

static void test_white_five(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("__OOOOO__", line);
    pattern_count_in_line(line, n, 2 /*WHITE*/, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "white five: __OOOOO__ -> FIVE x1");
}

static void test_color_isolation(void) {
    /* 黑五连不应被计入白方棋型 */
    int8_t line[16]; PatternStats sB = {0}, sW = {0};
    int n = build_line("__XXXXX__", line);
    pattern_count_in_line(line, n, 1, &sB);
    pattern_count_in_line(line, n, 2, &sW);
    ASSERT_EQ(sB.counts[PAT_FIVE], 1, "iso: black sees XXXXX as FIVE");
    ASSERT_EQ(sW.counts[PAT_FIVE], 0, "iso: white sees XXXXX as nothing");
}

int main(void) {
    test_five_isolated();
    test_five_at_edge();
    test_six_overline_counts_as_five();
    test_white_five();
    test_color_isolation();
    TEST_REPORT("test_pattern");
}
```

- [ ] **Step 3.2: 编译跑测试，预期五连测试 FAIL（stub 全 0）**

```bash
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && cl /W3 /TC /utf-8 /nologo src\pattern.c tests\test_pattern.c /Fe:test_pattern.exe /Fo:build\\ >nul 2>&1 && .\test_pattern.exe'
```

Expected: 5 个 FAIL（全 stub）。

- [ ] **Step 3.3: 实现 pattern_count_in_line（仅五连）**

替换 `pattern.c` 中 `pattern_count_in_line` 的 stub：

```c
/* 内部：扫线找连续同色段。返回段数，每段写入 (start, len) */
void pattern_count_in_line(const int8_t *line, int len, int color, PatternStats *out) {
    int i = 0;
    while (i < len) {
        if (line[i] != (int8_t)color) { i++; continue; }
        /* 找段起点 i 的段长 */
        int j = i;
        while (j < len && line[j] == (int8_t)color) j++;
        int run_len = j - i;

        /* 段长 ≥5 → 五连（含长连）*/
        if (run_len >= 5) {
            out->counts[PAT_FIVE]++;
        }
        /* 后续 task 在此处加 4/3/2 的分支 */

        i = j;
    }
}
```

- [ ] **Step 3.4: 跑测试，预期 5 个全 PASS**

```bash
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && cl /W3 /TC /utf-8 /nologo src\pattern.c tests\test_pattern.c /Fe:test_pattern.exe /Fo:build\\ >nul 2>&1 && .\test_pattern.exe'
```

Expected: `=== test_pattern: 5 / 5 passed ===`

- [ ] **Step 3.5: Commit**

```bash
git add src/pattern.c tests/test_pattern.c
git commit -m "feat(pattern): detect FIVE in single line (incl. overline)"
```

---

## Task 4: TDD —— 活四 / 冲四

**Files:**
- Modify: `tests/test_pattern.c`
- Modify: `src/pattern.c`

- [ ] **Step 4.1: 加活四 / 冲四测试**

在 `test_pattern.c` 的 `int main(void)` 之前插入：

```c
static void test_open_four(void) {
    /* _XXXX_ 双端空 → 活四 */
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("__XXXX__", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_FOUR], 1, "open_four: __XXXX__ -> OPEN_FOUR x1");
    ASSERT_EQ(s.counts[PAT_FIVE], 0, "open_four: not five");
}

static void test_simple_four_blocked_left(void) {
    /* OXXXX_ → 冲四 */
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("OXXXX_", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_SIMPLE_FOUR], 1, "simple_four: OXXXX_ -> SIMPLE_FOUR x1");
    ASSERT_EQ(s.counts[PAT_OPEN_FOUR], 0, "simple_four: not OPEN_FOUR");
}

static void test_simple_four_blocked_right(void) {
    /* _XXXXO → 冲四 */
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("_XXXXO", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_SIMPLE_FOUR], 1, "simple_four: _XXXXO -> SIMPLE_FOUR x1");
}

static void test_simple_four_at_edge(void) {
    /* XXXX_ 起点是边界 → 冲四（边界等同被堵）*/
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("XXXX_", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_SIMPLE_FOUR], 1, "simple_four: edge XXXX_ -> SIMPLE_FOUR x1");
}

static void test_dead_four(void) {
    /* OXXXXO → 死四（不计任何分） */
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("OXXXXO", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_FOUR], 0, "dead_four: not open");
    ASSERT_EQ(s.counts[PAT_SIMPLE_FOUR], 0, "dead_four: not simple");
}
```

在 `int main(void) {` 中追加：

```c
    test_open_four();
    test_simple_four_blocked_left();
    test_simple_four_blocked_right();
    test_simple_four_at_edge();
    test_dead_four();
```

- [ ] **Step 4.2: 编译跑预期 5 个新增 FAIL**

- [ ] **Step 4.3: 扩展 pattern_count_in_line 处理 4 连**

在 Task 3 留的 `/* 后续 task 在此处加 4/3/2 的分支 */` 处替换为完整的"段分类"逻辑——重写 `pattern_count_in_line`：

```c
void pattern_count_in_line(const int8_t *line, int len, int color, PatternStats *out) {
    int i = 0;
    while (i < len) {
        if (line[i] != (int8_t)color) { i++; continue; }

        int j = i;
        while (j < len && line[j] == (int8_t)color) j++;
        int run_len = j - i;

        /* 两端开放性：left_open = 起点之前一格是 EMPTY；right_open = 段尾之后一格是 EMPTY */
        int left_open  = (i - 1 >= 0)  && (line[i - 1] == 0);
        int right_open = (j     <  len) && (line[j]     == 0);
        int open_count = left_open + right_open;

        if (run_len >= 5) {
            out->counts[PAT_FIVE]++;
        } else if (run_len == 4) {
            if (open_count == 2)      out->counts[PAT_OPEN_FOUR]++;
            else if (open_count == 1) out->counts[PAT_SIMPLE_FOUR]++;
            /* open_count == 0 → 死四，不计分 */
        }
        /* run_len == 3, 2 在 Task 5/6 处理 */

        i = j;
    }
}
```

- [ ] **Step 4.4: 跑测试，预期 10/10 PASS**

```bash
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && cl /W3 /TC /utf-8 /nologo src\pattern.c tests\test_pattern.c /Fe:test_pattern.exe /Fo:build\\ >nul 2>&1 && .\test_pattern.exe'
```

Expected: `=== test_pattern: 10 / 10 passed ===`

- [ ] **Step 4.5: Commit**

```bash
git add src/pattern.c tests/test_pattern.c
git commit -m "feat(pattern): detect OPEN_FOUR and SIMPLE_FOUR via run-length classification"
```

---

## Task 5: TDD —— 活三 / 眠三

**Files:**
- Modify: `tests/test_pattern.c`
- Modify: `src/pattern.c`

- [ ] **Step 5.1: 加 3 连测试**

```c
static void test_open_three(void) {
    /* _XXX_ 双端空 → 活三 */
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("___XXX___", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_THREE], 1, "open_three: ___XXX___ -> OPEN_THREE x1");
    ASSERT_EQ(s.counts[PAT_SLEEP_THREE], 0, "open_three: not sleep_three");
}

static void test_sleep_three_blocked(void) {
    /* OXXX_ → 眠三 */
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("OXXX__", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_SLEEP_THREE], 1, "sleep_three: OXXX__ -> SLEEP_THREE x1");
    ASSERT_EQ(s.counts[PAT_OPEN_THREE], 0, "sleep_three: not open");
}

static void test_sleep_three_at_edge(void) {
    /* XXX_ 起点是边界 → 眠三 */
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("XXX___", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_SLEEP_THREE], 1, "sleep_three: edge XXX___ -> SLEEP_THREE x1");
}

static void test_dead_three(void) {
    /* OXXXO → 死三 */
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("OXXXO", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_THREE], 0, "dead_three: not open");
    ASSERT_EQ(s.counts[PAT_SLEEP_THREE], 0, "dead_three: not sleep");
}
```

在 `int main(void) {` 中追加：

```c
    test_open_three();
    test_sleep_three_blocked();
    test_sleep_three_at_edge();
    test_dead_three();
```

- [ ] **Step 5.2: 编译跑预期新增 FAIL**

- [ ] **Step 5.3: 扩展分类逻辑——加 run_len == 3 分支**

在 `pattern_count_in_line` 的 4 连分支后插入 3 连分支：

```c
        } else if (run_len == 3) {
            if (open_count == 2)      out->counts[PAT_OPEN_THREE]++;
            else if (open_count == 1) out->counts[PAT_SLEEP_THREE]++;
            /* open_count == 0 → 死三 */
        }
```

完整的 `pattern_count_in_line` 现在长这样：

```c
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
        }
        /* run_len == 2 在 Task 6 处理 */

        i = j;
    }
}
```

- [ ] **Step 5.4: 跑测试，预期 14/14 PASS**

- [ ] **Step 5.5: Commit**

```bash
git add src/pattern.c tests/test_pattern.c
git commit -m "feat(pattern): detect OPEN_THREE and SLEEP_THREE"
```

---

## Task 6: TDD —— 活二 / 眠二

**Files:**
- Modify: `tests/test_pattern.c`
- Modify: `src/pattern.c`

- [ ] **Step 6.1: 加 2 连测试**

```c
static void test_open_two(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("___XX___", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_TWO], 1, "open_two: ___XX___ -> OPEN_TWO x1");
}

static void test_sleep_two_blocked(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("OXX___", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_SLEEP_TWO], 1, "sleep_two: OXX___ -> SLEEP_TWO x1");
}

static void test_dead_two(void) {
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("OXXO", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_TWO], 0, "dead_two: not open");
    ASSERT_EQ(s.counts[PAT_SLEEP_TWO], 0, "dead_two: not sleep");
}

static void test_single_stone_no_pattern(void) {
    /* 孤立单子不计入任何棋型 */
    int8_t line[16]; PatternStats s = {0};
    int n = build_line("__X__", line);
    pattern_count_in_line(line, n, 1, &s);
    ASSERT_EQ(s.counts[PAT_NONE], 0, "single: no pattern (NONE never incremented)");
    ASSERT_EQ(s.counts[PAT_OPEN_TWO], 0, "single: not OPEN_TWO");
}
```

在 `int main(void) {` 中追加 4 个测试调用。

- [ ] **Step 6.2: 编译跑预期新增 FAIL**

- [ ] **Step 6.3: 加 run_len == 2 分支**

在 3 连分支后插入：

```c
        } else if (run_len == 2) {
            if (open_count == 2)      out->counts[PAT_OPEN_TWO]++;
            else if (open_count == 1) out->counts[PAT_SLEEP_TWO]++;
        }
        /* run_len == 1 不计分（活一价值太低） */
```

- [ ] **Step 6.4: 跑测试，预期 18/18 PASS**

- [ ] **Step 6.5: Commit**

```bash
git add src/pattern.c tests/test_pattern.c
git commit -m "feat(pattern): detect OPEN_TWO and SLEEP_TWO"
```

---

## Task 7: TDD `pattern_count_for_color` —— 整盘 4 方向抽线

**Files:**
- Modify: `tests/test_pattern.c`
- Modify: `src/pattern.c`

- [ ] **Step 7.1: 加 4 方向测试**

```c
static void test_count_for_color_horizontal(void) {
    Board b; board_init(&b);
    /* 行 7 黑 5 连 */
    for (int c = 0; c < 5; c++) b.cells[7][c] = 1;
    PatternStats s = {0};
    pattern_count_for_color(&b, 1, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "for_color: horizontal black 5 -> FIVE x1");
}

static void test_count_for_color_vertical(void) {
    Board b; board_init(&b);
    /* 列 7 黑 5 连 */
    for (int r = 0; r < 5; r++) b.cells[r][7] = 1;
    PatternStats s = {0};
    pattern_count_for_color(&b, 1, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "for_color: vertical black 5 -> FIVE x1");
}

static void test_count_for_color_diagonal_main(void) {
    Board b; board_init(&b);
    /* 主对角 (3,3)-(7,7) 黑 5 连 */
    for (int i = 0; i < 5; i++) b.cells[3 + i][3 + i] = 1;
    PatternStats s = {0};
    pattern_count_for_color(&b, 1, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "for_color: main-diag black 5 -> FIVE x1");
}

static void test_count_for_color_diagonal_anti(void) {
    Board b; board_init(&b);
    /* 副对角 (3,11)-(7,7) 黑 5 连 */
    for (int i = 0; i < 5; i++) b.cells[3 + i][11 - i] = 1;
    PatternStats s = {0};
    pattern_count_for_color(&b, 1, &s);
    ASSERT_EQ(s.counts[PAT_FIVE], 1, "for_color: anti-diag black 5 -> FIVE x1");
}

static void test_count_for_color_multiple_threes(void) {
    /* 行 7 一个活三 + 列 7 一个活三 */
    Board b; board_init(&b);
    for (int c = 4; c < 7; c++) b.cells[7][c] = 1;   /* 行 7：(7,4)(7,5)(7,6) 黑活三 */
    for (int r = 9; r < 12; r++) b.cells[r][7] = 1;  /* 列 7：(9,7)(10,7)(11,7) 黑活三 */
    /* 注意：行 7 上 (7,7) 仍为 EMPTY，所以列 7 的 3 子和行 7 的 3 子互不影响 */
    PatternStats s = {0};
    pattern_count_for_color(&b, 1, &s);
    ASSERT_EQ(s.counts[PAT_OPEN_THREE], 2, "for_color: 2 separate open threes -> OPEN_THREE x2");
}
```

在 `int main(void) {` 中追加 5 个调用。

- [ ] **Step 7.2: 编译预期 FAIL（pattern_count_for_color 是 stub）**

- [ ] **Step 7.3: 实现 pattern_count_for_color（4 方向抽线）**

替换 `pattern.c` 中 `pattern_count_for_color` 的 stub：

```c
void pattern_count_for_color(const Board *b, int color, PatternStats *out) {
    int8_t line[BOARD_SIZE];

    /* 横向：每行扫一次 */
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) line[c] = (int8_t)b->cells[r][c];
        pattern_count_in_line(line, BOARD_SIZE, color, out);
    }

    /* 竖向：每列扫一次 */
    for (int c = 0; c < BOARD_SIZE; c++) {
        for (int r = 0; r < BOARD_SIZE; r++) line[r] = (int8_t)b->cells[r][c];
        pattern_count_in_line(line, BOARD_SIZE, color, out);
    }

    /* 主对角（左上→右下，r-c = const，范围 -(BS-1) .. (BS-1)） */
    for (int diag = -(BOARD_SIZE - 1); diag <= BOARD_SIZE - 1; diag++) {
        int n = 0;
        for (int r = 0; r < BOARD_SIZE; r++) {
            int c = r - diag;
            if (c < 0 || c >= BOARD_SIZE) continue;
            line[n++] = (int8_t)b->cells[r][c];
        }
        if (n >= 5) pattern_count_in_line(line, n, color, out);
    }

    /* 副对角（右上→左下，r+c = const，范围 0 .. 2*(BS-1)） */
    for (int diag = 0; diag <= 2 * (BOARD_SIZE - 1); diag++) {
        int n = 0;
        for (int r = 0; r < BOARD_SIZE; r++) {
            int c = diag - r;
            if (c < 0 || c >= BOARD_SIZE) continue;
            line[n++] = (int8_t)b->cells[r][c];
        }
        if (n >= 5) pattern_count_in_line(line, n, color, out);
    }
}
```

- [ ] **Step 7.4: 跑测试**

```bash
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && cl /W3 /TC /utf-8 /nologo src\board.c src\pattern.c tests\test_pattern.c /Fe:test_pattern.exe /Fo:build\\ >nul 2>&1 && .\test_pattern.exe'
```

Expected: `=== test_pattern: 23 / 23 passed ===`

注意：从此 task 开始 test_pattern 必须 link `board.c`，因为用了 Board 结构。

- [ ] **Step 7.5: Commit**

```bash
git add src/pattern.c tests/test_pattern.c
git commit -m "feat(pattern): scan all 4 directions (h/v/main-diag/anti-diag)"
```

---

## Task 8: TDD `pattern_evaluate` —— 整盘评估

**Files:**
- Modify: `tests/test_pattern.c`
- Modify: `src/pattern.c`

- [ ] **Step 8.1: 加 evaluate 测试**

```c
static void test_evaluate_empty(void) {
    Board b; board_init(&b);
    ASSERT_EQ(pattern_evaluate(&b), 0, "evaluate: empty board -> 0");
}

static void test_evaluate_black_open_three_advantage(void) {
    /* 黑活三，白无 → 黑视角分高，白视角分低 */
    Board b; board_init(&b);
    for (int c = 4; c < 7; c++) b.cells[7][c] = 1;  /* 黑活三 */
    b.move_count = 3;

    b.side_to_move = 1; /* 黑视角 */
    int sB = pattern_evaluate(&b);
    b.side_to_move = 2; /* 白视角 */
    int sW = pattern_evaluate(&b);

    ASSERT_TRUE(sB > 0, "evaluate: black open_three from black view > 0");
    ASSERT_TRUE(sW < 0, "evaluate: black open_three from white view < 0");
    ASSERT_EQ(sB, -sW, "evaluate: viewpoint symmetric");
}

static void test_evaluate_open_four_dominates_open_three(void) {
    /* 黑活四 vs 白活三：黑应远超 */
    Board b; board_init(&b);
    for (int c = 3; c < 7; c++) b.cells[5][c] = 1;  /* 黑活四 */
    for (int c = 3; c < 6; c++) b.cells[10][c] = 2; /* 白活三 */
    b.move_count = 7;
    b.side_to_move = 1;

    int score = pattern_evaluate(&b);
    /* 黑活四 10000 - 白活三 1000 = 9000，正向 */
    ASSERT_TRUE(score > 5000, "evaluate: open_four >> open_three");
}

static void test_evaluate_score_table_values(void) {
    /* 单独验证评分表值（spec § 3.2） */
    ASSERT_EQ(PATTERN_SCORE[PAT_FIVE],        100000, "score: FIVE = 100000");
    ASSERT_EQ(PATTERN_SCORE[PAT_OPEN_FOUR],    10000, "score: OPEN_FOUR = 10000");
    ASSERT_EQ(PATTERN_SCORE[PAT_OPEN_THREE],    1000, "score: OPEN_THREE = 1000");
    ASSERT_EQ(PATTERN_SCORE[PAT_SIMPLE_FOUR],   1000, "score: SIMPLE_FOUR = 1000");
    ASSERT_EQ(PATTERN_SCORE[PAT_OPEN_TWO],       100, "score: OPEN_TWO = 100");
    ASSERT_EQ(PATTERN_SCORE[PAT_SLEEP_THREE],    100, "score: SLEEP_THREE = 100");
    ASSERT_EQ(PATTERN_SCORE[PAT_SLEEP_TWO],       10, "score: SLEEP_TWO = 10");
    ASSERT_EQ(PATTERN_SCORE[PAT_NONE],             0, "score: NONE = 0");
}
```

在 `int main(void) {` 追加 4 个调用。

- [ ] **Step 8.2: 编译跑预期 FAIL**

- [ ] **Step 8.3: 实现 pattern_evaluate**

替换 `pattern.c` 中 `pattern_evaluate` 的 stub：

```c
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
```

- [ ] **Step 8.4: 跑测试预期 27/27 PASS**

- [ ] **Step 8.5: Commit**

```bash
git add src/pattern.c tests/test_pattern.c
git commit -m "feat(pattern): integrate pattern_evaluate (black-vs-white score diff)"
```

---

## Task 9: 替换 `search.c` 的 placeholder evaluator

**Files:**
- Modify: `src/search.c`
- Modify: `tests/test_search.c`（确认 4/29 测试仍 PASS）

- [ ] **Step 9.1: 修改 search.c 让 search_evaluate 调 pattern_evaluate**

替换 `src/search.c` 中现有的 `search_evaluate` 函数（约 14-26 行那段）：

```c
/* 改为转发给 pattern_evaluate（4/30 替换 placeholder） */
int search_evaluate(const Board *b) {
    return pattern_evaluate(b);
}
```

并在 `src/search.c` 顶部 `#include "board.h"` 后追加：

```c
#include "pattern.h"
```

- [ ] **Step 9.2: 调整 test_search.c 中"中央倾向"测试**

`tests/test_search.c` 里的 `test_evaluate_center_better_than_corner` 不再适用（pattern 不看中央位置）。改成检查活三优势：

替换该测试函数为：

```c
static void test_evaluate_open_three_beats_dead_three(void) {
    /* 活三应该比死三分更高（不再是中央倾向） */
    Board b1; board_init(&b1);
    for (int c = 4; c < 7; c++) b1.cells[7][c] = BLACK;  /* 活三 */
    b1.move_count = 3;
    b1.side_to_move = BLACK;

    Board b2; board_init(&b2);
    b2.cells[7][4] = WHITE; /* 左堵 */
    for (int c = 5; c < 8; c++) b2.cells[7][c] = BLACK;  /* 眠三 */
    b2.cells[7][8] = WHITE; /* 右堵 → 死三 */
    b2.move_count = 5;
    b2.side_to_move = BLACK;

    int s1 = search_evaluate(&b1);
    int s2 = search_evaluate(&b2);
    ASSERT_TRUE(s1 > s2, "evaluate: open_three score > dead_three score");
}
```

并把 `int main(void) {` 中的 `test_evaluate_center_better_than_corner();` 改为 `test_evaluate_open_three_beats_dead_three();`。

- [ ] **Step 9.3: 编译两个测试，预期都全过**

```bash
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && cl /W3 /TC /utf-8 /nologo src\board.c src\search.c src\pattern.c tests\test_search.c /Fe:test_search.exe /Fo:build\\ >nul 2>&1 && .\test_search.exe'
```

Expected: `=== test_search: 12 / 12 passed ===`（与 4/29 同样数量，新测试替换了旧的中央测试）

- [ ] **Step 9.4: Commit**

```bash
git add src/search.c tests/test_search.c
git commit -m "feat(search): replace center-bias evaluator with pattern_evaluate"
```

---

## Task 10: 更新 `build.bat` —— 加 pattern.c 到 main + 新增 test_pattern.exe

**Files:**
- Modify: `build.bat`

- [ ] **Step 10.1: 编辑 build.bat**

`build.bat` 中"主程序"段已经用 `src\*.c` 通配符（自动包含 pattern.c）。只需在"单元测试"段后加 test_pattern：

在 `if errorlevel 1 ( echo [ERROR] test_search build failed & exit /b 1 )` 这行**之后**插入：

```batch
echo Building test_pattern.exe...
cl /W3 /TC /utf-8 /nologo /Fe:test_pattern.exe /Fo:build\ ^
   src\board.c src\pattern.c tests\test_pattern.c
if errorlevel 1 ( echo [ERROR] test_pattern build failed & exit /b 1 )
```

并在末尾的 `echo   test_search.exe` **之后**追加：

```batch
echo   test_pattern.exe
```

- [ ] **Step 10.2: 转 CRLF 行尾（Edit 工具默认 LF，cmd 解析 .bat 会出错）**

```bash
sed -i 's/\r$//' build.bat && sed -i 's/$/\r/' build.bat
```

或者用 PowerShell：

```powershell
$c = Get-Content -Raw build.bat; $c = $c -replace "`r?`n", "`r`n"; [System.IO.File]::WriteAllText((Resolve-Path 'build.bat'), $c)
```

确认 file 报告 CRLF：

```bash
file build.bat
```

Expected: `DOS batch file, ..., with CRLF line terminators`

- [ ] **Step 10.3: 清理旧 .exe 防 LNK1104 + 跑 build**

```bash
rm -f gobang-engine.exe test_board.exe test_search.exe test_pattern.exe
```

```powershell
cmd /c "cd /d C:\Users\cthzy\Desktop\gobang-engine & .\build.bat"
```

Expected: 末尾有 `[OK] build complete.` 并列出 4 个 exe。

- [ ] **Step 10.4: 跑全部测试**

```powershell
.\test_board.exe | Select-Object -Last 1
.\test_search.exe | Select-Object -Last 1
.\test_pattern.exe | Select-Object -Last 1
```

Expected:
```
=== test_board: 45 / 45 passed ===
=== test_search: 12 / 12 passed ===
=== test_pattern: 27 / 27 passed ===
```

- [ ] **Step 10.5: Commit**

```bash
git add build.bat
git commit -m "build: include pattern.c in main and add test_pattern.exe"
```

---

## Task 11: M1 集成验收 —— self-play 应"会下棋" + tag

**Files:**
- 无代码改动（仅运行验证 + tag）

这是当日的"达成 M1 里程碑"——验证替换 evaluator 后 αβ 真的"会下棋"（聚焦攻防而非占中央）。

- [ ] **Step 11.1: 跑 self-play demo**

```powershell
.\gobang-engine.exe
```

观察输出。**期望特征**：
- 黑首手仍下中央（H8）—— 因为 αβ 在 d=2 时也没差异，活二等更高棋型只在多步后出现
- 后续步数应能看到双方"成连"——比如黑 4-5 步后形成活二/活三，白方主动堵
- **不应该**出现"双方都聚成中央十字"那种纯中央倾向的局面（4/29 demo 那种）

- [ ] **Step 11.2: 主观判定（用户协助）**

把 demo 输出贴给用户看，**如果**双方有"明显攻防互动"（黑想成三，白来堵），就算达标。如果还是聚中央，可能 evaluator 实现有 bug——回头查 pattern_evaluate 的视角翻转。

判定通过后继续 11.3。

- [ ] **Step 11.3: 跑全部测试一次最终验收**

```bash
rm -f *.exe && cmd /c "cd /d $(pwd) & .\build.bat" >/dev/null
```

```powershell
.\test_board.exe | Select-Object -Last 1
.\test_search.exe | Select-Object -Last 1
.\test_pattern.exe | Select-Object -Last 1
```

Expected: 三个 exe 全过，总数 45 + 12 + 27 = 84。

- [ ] **Step 11.4: 打 tag + push**

```bash
git tag -a m1-pattern -m "M1 milestone (4/30): pattern + evaluator integrated.

- pattern.c/.h with 7 pattern types (FIVE/OPEN_FOUR/SIMPLE_FOUR/OPEN_THREE/SLEEP_THREE/OPEN_TWO/SLEEP_TWO)
- 4-direction line scan (h/v/main-diag/anti-diag)
- pattern_evaluate replaces placeholder in search.c
- Self-play now exhibits attack/defense patterns
- 84/84 tests pass (45 board + 12 search + 27 pattern)
- Forbidden detection still pending (5/3-5/5)"
```

```bash
git push origin main && git push origin m1-pattern
```

- [ ] **Step 11.5: 验证 GitHub**

```bash
gh repo view cthzyfy1314/gobang-engine --json defaultBranchRef
```

或浏览器看 tags 页面确认 `m1-pattern` 已上去。

---

## Day 2 Done — 验收清单

完成本 plan 后你应该有：

- ✅ `gobang-engine.exe` — self-play 表现出真实攻防（活二/活三聚集，对方堵）
- ✅ `test_pattern.exe` — pattern 模块单元测试 27 个 case 全 PASS（含 4 方向 + 7 棋型）
- ✅ `test_board.exe` / `test_search.exe` 仍全 PASS（无回归）
- ✅ git 仓库 ≥ 11 个新 commit + tag `m1-pattern`
- ✅ **达成 spec § 7.3 M1 里程碑**：第一次能自我对战（不带禁手）

**5/1 周五（酒店晚）的 plan 会做**：`zobrist.c/.h`（哈希 + 置换表），让 αβ 能跨 search 共享子树评估，深度从 d=2 提升到 d=4-6 不变慢。

**风险记录**：
- 如果 self-play 还是看起来"傻"，可能是 pattern 评分表没拉开档次。spec § 3.2 表已经分了 100k → 10k → 1k → 100 → 10 五档，理论够用。如果不行，可考虑把 OPEN_THREE 拉到 1500 让"三"更具威胁。
- 如果搜索变慢明显（d=2 节点数 > 5000），是 pattern_count_for_color 每次扫 88 条线导致——可以先忍到 5/1 zobrist 之后用置换表缓存 evaluate 结果。

---

## Self-Review

✅ **Spec 覆盖检查**：
- § 3.2 棋型评估（5/活四/冲四/活三/眠三/活二/眠二/活一） → Tasks 3-6 + 评分表 ✅
- § 4.1 pattern 模块（活四/冲四/活三/眠三/活二识别 + 评估）→ Tasks 1-8 ✅
- § 8.1 pattern 单元测试（每种棋型识别正确 + 边界条件）→ Tasks 3-7 含边界 case ✅
- § 7 节奏 4/30 行（pattern.c/.h + pattern + search 联调 → 第一次能自我对战）→ Task 9 + Task 11 ✅
- § 7.3 M1 里程碑 → Task 11 ✅

⚠️ **Day 2 不覆盖**（已声明）：
- 跳子棋型（`_XX_X_` 跳活三 / `_X_XXX_` 跳活四）—— 留给 forbid 模块（5/3-5/5）补
- 复合棋型（双活三 / 双冲四 = 单独高分项）—— 当前用"两个 OPEN_THREE 计数 = 2000 分"近似；spec § 3.2 表里"双活三 = 10000"暂时未单独识别。如果 self-play 表现差，5/2 调参时再加 `if (counts[PAT_OPEN_THREE] >= 2) score += 8000;` 的补丁
- 增量评估（spec § 3.2 提到的"落子时只更新受影响的 4 条线"）—— 当前每次评估扫整盘，O(1320)。等 5/1 zobrist 加置换表后这个开销会被缓存掩盖

✅ **Placeholder 扫描**：plan 中无 "TBD"/"TODO"/"implement later"。每个 step 含完整代码或精确命令。

✅ **类型一致性**：
- `Pattern` 枚举、`PatternStats`、`PATTERN_SCORE` 在 Task 1 一处定义，后续 Task 引用一致
- `pattern_count_in_line` / `pattern_count_for_color` / `pattern_evaluate` 函数签名贯穿 Task 3-9 不变
- 与 4/29 已有代码兼容：`Board *`、`Move`、`BLACK/WHITE/EMPTY` 引用方式不变

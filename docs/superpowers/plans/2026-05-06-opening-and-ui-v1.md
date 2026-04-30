# Day 8 (5/6): Opening Library + UI v1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans.

**Goal:** 替换 self-play demo 为**交互式人肉协议对战 UI**——用户输入对手坐标，引擎显示建议 + 棋型威胁 + 禁手警告。Opening 模块仅做最小必要（黑 1 天元 hardcode + 三手交换/五手 N 打存根）；如果校赛裁判要求 26 种开局严格，5/9 再补。

**Architecture:**
- `opening.h/.c`：3 个函数（`opening_first_move` 返回天元 / `opening_should_swap` 当前返 false / `opening_pick_n_strikes` 当前不实现），spec 26 种开局表暂存为占位结构
- `ui.h/.c`：交互循环 + 棋盘绘制 + 输入解析 + 命令菜单
- 修改 `main.c`：从 self-play 改为 UI 主循环

**Tech Stack:** C99 stdio，无 ncurses（保持纯 stdio 跨编译器），CRLF 输入兼容 Windows 命令行

**对应 Spec 节段:** § 4.1 opening + ui 模块 / § 6 接口设计 / § 7 节奏 5/6 / § 7.3 M3 里程碑（完整国规对局可跑通）

**预算:** 1d（实际预计 2-3h，UI 是主体）

---

## Task O1: opening.h/.c 极简版

**Files:** Create `src/opening.h` + `src/opening.c`

```c
/* src/opening.h */
#ifndef OPENING_H_
#define OPENING_H_
#include "board.h"

/* 返回黑方第一手位置 — 国规要求天元 (7,7) */
Move opening_first_move(void);

/* 三手交换决策（白方第 4 手前判断是否换边）。
 * 当前简化：永远返回 false（不交换）。如有比赛需要再用 αβ 评估。
 */
bool opening_should_swap(const Board *b);

#endif

/* src/opening.c */
#include "opening.h"

Move opening_first_move(void) {
    Move m = { 7, 7, BLACK };
    return m;
}

bool opening_should_swap(const Board *b) {
    (void)b;
    return false;
}
```

- [ ] **Step O1.1**: 写 opening.h + opening.c
- [ ] **Step O1.2**: Commit `feat(opening): minimal stub - tengen first move + no-swap`

---

## Task U1: ui.h 接口

**Files:** Create `src/ui.h`

```c
/* src/ui.h — 交互式对战 UI（人肉协议）*/
#ifndef UI_H_
#define UI_H_
#include "board.h"

/* 启动 UI 主循环。返回时游戏已结束。
 * my_color: 我方执子颜色（BLACK 或 WHITE）。校赛由裁判抽签决定。
 * search_depth: αβ 搜索深度。
 */
void ui_main_loop(int my_color, int search_depth);

/* 绘制当前棋盘到 stdout */
void ui_draw_board(const Board *b);

/* 解析用户输入的坐标。支持格式：
 *   "H8" / "h8"  / "h08"  → row=15-8=7, col='H'-'A'=7
 *   "7,7"        → row=7, col=7（0-indexed）
 *   "quit" / "q" → 返回 false 让上层处理
 *   "undo" / "u" → 返回 false 让上层处理
 * 成功解析坐标返回 true 并填 *row *col。
 * 命令字符串返回 false，*cmd 填命令枚举（COMMAND_QUIT / COMMAND_UNDO / COMMAND_NONE）。
 */
typedef enum {
    UI_CMD_NONE = 0,
    UI_CMD_QUIT,
    UI_CMD_UNDO,
    UI_CMD_HINT,
    UI_CMD_RESIGN,
    UI_CMD_INVALID
} UICommand;

bool ui_parse_input(const char *line, int *row, int *col, UICommand *cmd);

#endif
```

- [ ] **Step U1.1**: 写 ui.h
- [ ] **Step U1.2**: Commit `feat(ui): declare interactive UI API`

---

## Task U2: ui.c 棋盘绘制（增强版）

**Files:** Create `src/ui.c`

```c
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ui.h"
#include "search.h"
#include "forbid.h"
#include "pattern.h"
#include "zobrist.h"

void ui_draw_board(const Board *b) {
    printf("\n     A  B  C  D  E  F  G  H  I  J  K  L  M  N  O\n");
    for (int r = 0; r < BOARD_SIZE; r++) {
        printf("%2d ", BOARD_SIZE - r);
        for (int c = 0; c < BOARD_SIZE; c++) {
            uint8_t v = b->cells[r][c];
            const char *s = (v == EMPTY) ? " . " :
                            (v == BLACK) ? " X " :
                                            " O ";
            printf("%s", s);
        }
        printf(" %2d\n", BOARD_SIZE - r);
    }
    printf("     A  B  C  D  E  F  G  H  I  J  K  L  M  N  O\n\n");
    printf("X = BLACK, O = WHITE. Move count = %u, side to move = %s\n",
           b->move_count, b->side_to_move == BLACK ? "BLACK" : "WHITE");
}
```

- [ ] **Step U2.1**: 写 ui.c 起手 + ui_draw_board
- [ ] **Step U2.2**: Commit `feat(ui): basic board renderer with column letters and row numbers`

---

## Task U3: ui_parse_input

**Files:** Modify `src/ui.c`

```c
static int letter_to_col(char ch) {
    ch = (char)toupper((unsigned char)ch);
    if (ch >= 'A' && ch <= 'O') return ch - 'A';
    return -1;
}

bool ui_parse_input(const char *line, int *row, int *col, UICommand *cmd) {
    *cmd = UI_CMD_NONE;
    /* 跳过前导空白 */
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '\n') { *cmd = UI_CMD_INVALID; return false; }

    /* 命令检查 */
    if (*line == 'q' || *line == 'Q') { *cmd = UI_CMD_QUIT;  return false; }
    if (*line == 'u' || *line == 'U') { *cmd = UI_CMD_UNDO;  return false; }
    if (*line == 'h' && line[1] != '\0' && !isalpha((unsigned char)line[1])) {
        *cmd = UI_CMD_HINT; return false;
    }
    if (*line == 'r' || *line == 'R') { *cmd = UI_CMD_RESIGN; return false; }

    /* 坐标格式 1: 字母 + 数字, 如 H8 */
    int c = letter_to_col(*line);
    if (c >= 0) {
        line++;
        int n = 0;
        while (*line >= '0' && *line <= '9') { n = n * 10 + (*line - '0'); line++; }
        if (n >= 1 && n <= BOARD_SIZE) {
            *col = c;
            *row = BOARD_SIZE - n;
            return true;
        }
        *cmd = UI_CMD_INVALID;
        return false;
    }

    /* 坐标格式 2: row,col 全数字 */
    int rr = 0, cc = 0;
    if (sscanf(line, "%d,%d", &rr, &cc) == 2 ||
        sscanf(line, "%d %d", &rr, &cc) == 2) {
        if (rr >= 0 && rr < BOARD_SIZE && cc >= 0 && cc < BOARD_SIZE) {
            *row = rr;
            *col = cc;
            return true;
        }
    }

    *cmd = UI_CMD_INVALID;
    return false;
}
```

- [ ] **Step U3.1**: 加 ui_parse_input
- [ ] **Step U3.2**: 测试（test_ui.c 加几个 case：H8 → (7,7) / Q → CMD_QUIT / 7,7 → (7,7) / 7 7 → (7,7) / 异常输入 → INVALID）
- [ ] **Step U3.3**: Commit `feat(ui): parse coordinate input H8/numeric and command letters`

---

## Task U4: ui_main_loop 主循环

**Files:** Modify `src/ui.c`

```c
static const char *coord_str(int row, int col, char *buf) {
    sprintf(buf, "%c%d", 'A' + col, BOARD_SIZE - row);
    return buf;
}

static void show_engine_suggestion(Board *b, int depth) {
    SearchResult r = search_best_move(b, depth);
    char buf[8];
    if (r.best_move.row < 0) {
        printf("Engine: no move available.\n");
        return;
    }
    coord_str(r.best_move.row, r.best_move.col, buf);
    printf("\n>>> Engine suggests: %s  (score=%d, nodes=%ld)\n", buf, r.score, r.nodes_searched);
}

static void warn_forbid_for_black(Board *b, int row, int col) {
    if (b->side_to_move != BLACK || !b->forbid_enabled) return;
    ForbidType ft = forbid_check_black(b, row, col);
    if (ft == FORBID_NONE) return;
    const char *name = (ft == FORBID_DOUBLE_THREE) ? "DOUBLE THREE (33)" :
                       (ft == FORBID_DOUBLE_FOUR)  ? "DOUBLE FOUR (44)"  :
                                                     "OVERLINE (>=6)";
    printf("⚠ FORBID warning at this position: %s\n", name);
}

void ui_main_loop(int my_color, int search_depth) {
    Board b;
    board_init(&b);

    printf("=== gobang-engine — Renju (national rules) ===\n");
    printf("You play: %s\n", my_color == BLACK ? "BLACK (X)" : "WHITE (O)");
    printf("Commands: <coord like H8> | u(ndo) | h(int) | q(uit) | r(esign)\n");

    /* 黑 1 天元 hardcode */
    if (my_color == BLACK) {
        Move m = { 7, 7, BLACK };
        printf("\nOpening move (国规 black 1): play H8 (tengen)\n");
        board_place(&b, m.row, m.col, BLACK);
    } else {
        printf("\nOpponent plays BLACK. Their first move (per national rules) should be H8.\n");
    }

    char line[64];
    while (1) {
        ui_draw_board(&b);

        GameResult res = board_check_winner(&b);
        if (res != RESULT_NONE) {
            const char *msg = (res == RESULT_BLACK_WIN) ? "BLACK wins!" :
                              (res == RESULT_WHITE_WIN_NORMAL) ? "WHITE wins!" :
                              (res == RESULT_WHITE_WIN_BY_BLACK_FORBID) ? "WHITE wins (BLACK forbid)!" :
                                                                   "DRAW";
            printf("\n*** Game over: %s ***\n", msg);
            break;
        }

        int active = b.side_to_move;
        const char *who = (active == my_color) ? "YOU" : "OPPONENT";
        printf("[%s] (%s to move) > ", who, active == BLACK ? "BLACK" : "WHITE");

        if (!fgets(line, sizeof(line), stdin)) break;

        int row, col;
        UICommand cmd;
        bool got_coord = ui_parse_input(line, &row, &col, &cmd);

        if (!got_coord) {
            if (cmd == UI_CMD_QUIT) { printf("Quit.\n"); return; }
            if (cmd == UI_CMD_UNDO) {
                if (board_undo(&b)) printf("Undo OK.\n");
                else                printf("Cannot undo: empty history.\n");
                continue;
            }
            if (cmd == UI_CMD_HINT) {
                show_engine_suggestion(&b, search_depth);
                continue;
            }
            if (cmd == UI_CMD_RESIGN) {
                printf("You resigned.\n");
                return;
            }
            printf("Invalid input. Try H8 / Q / U / H / R.\n");
            continue;
        }

        warn_forbid_for_black(&b, row, col);
        if (!board_place(&b, row, col, active)) {
            printf("Illegal move (occupied or out of range).\n");
            continue;
        }

        /* 我方刚下完，自动给出引擎对对手的预测 / 自己的推荐建议
         * 简化：只在 active == my_color 之后，给出引擎对当前局面（轮到对手）的"应招"建议
         */
        if (active == my_color) {
            printf("Move recorded: %c%d.\n", 'A' + col, BOARD_SIZE - row);
            /* 引擎为对手分析（其实是对手的最佳应招——你可以预知对手）*/
        } else {
            /* 对手刚下完，引擎为我推荐 */
            show_engine_suggestion(&b, search_depth);
        }
    }
}
```

- [ ] **Step U4.1**: 实现 ui_main_loop
- [ ] **Step U4.2**: Commit `feat(ui): interactive main loop with hint/undo/quit/resign commands`

---

## Task M1: 改 main.c 启动 UI

**Files:** Modify `src/main.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"
#include "search.h"
#include "zobrist.h"

int main(int argc, char **argv) {
    zobrist_init();

    /* 默认我方执黑，深度 4。可命令行覆盖 */
    int my_color = BLACK;
    int depth = 4;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--white")) my_color = WHITE;
        else if (!strcmp(argv[i], "--black")) my_color = BLACK;
        else if (!strncmp(argv[i], "--depth=", 8)) depth = atoi(argv[i] + 8);
        else if (!strcmp(argv[i], "--demo")) {
            /* 保留 self-play demo 用于回归测试 */
            /* fall through to demo code */
        }
    }

    ui_main_loop(my_color, depth);
    return 0;
}
```

- [ ] **Step M1.1**: 重写 main.c
- [ ] **Step M1.2**: Commit `feat(main): replace self-play demo with interactive UI loop`

---

## Task M2: build.bat 加 opening.c + ui.c

**Files:** Modify `build.bat`

主程序 `src\*.c` 通配符已经覆盖。但 test_search 等需要 link ui.c 吗？不需要，因为只 main.c 用 ui。

但 `gobang-engine.exe` 的命令需要确保编译所有 src/*.c。当前 build.bat 已经 `cl ... src\*.c`，OK。

直接 build 验证。

- [ ] **Step M2.1**: 跑 `.\build.bat` 验证 4 个 .exe 都生成
- [ ] **Step M2.2**: Commit (no change if build.bat 不变)

---

## Task V1: 手测 + tag m1-ui-v1

- [ ] **Step V1.1**: 跑 `.\gobang-engine.exe`，手玩几手测试：
  - 黑 1 天元自动落
  - 输入 "h7" 让白下 H7
  - 引擎给出建议
  - 输入 "u" 撤一步
  - 输入 "q" 退出
- [ ] **Step V1.2**: 跑 `.\gobang-engine.exe --white`：用户执白，对手执黑
- [ ] **Step V1.3**: 全套测试 PASS（129/129 之前）
- [ ] **Step V1.4**: Tag `m1-ui-v1` + push

---

## Self-Review

✅ **核心交互覆盖:** 棋盘绘制 + 坐标输入 + 命令（u/h/q/r）+ 禁手警告 + 引擎建议
⚠️ **不实现:** 26 种开局选择菜单 / 三手交换决策 / 五手 N 打候选 / 棋型威胁标注 / 复盘文件
⚠️ **简化:** 黑 1 天元 hardcode；三手交换默认不换；五手 N 打不做候选生成
✅ **测试覆盖:** ui_parse_input 单元 + 手测主循环

**风险:**
- 校赛裁判若严格要求 26 种开局展示，需要 5/9 早晨再补 26 局表 + 选择菜单
- 当前 UI 是英文 `X/O` 棋子；如果裁判想看●○可后期改 UTF-8 输出

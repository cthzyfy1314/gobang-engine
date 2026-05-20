# gobang-engine — 项目级 Claude Code 指南

## What this is

竞技五子棋（Renju）引擎，**中国国家标准规则（国规）**。C99，单线程，~3100 LOC。
作者陈天航（cthzyfy1314）。**动机不是比赛**——是技术深度挑战 + 留学申请（AI alignment 方向）portfolio。最终目标走 NNUE 路线。

实力基线：~2050-2150 ELO（业余 2-3 段），50/50 vs Mintaka，0-99 vs Rapfi（NN 引擎，差 ~700 ELO）。

## Build & Test

```bat
build.bat          :: MSVC，产出 gobang-engine.exe + 9 个 test exe
```

- 用 MSVC（不是 MinGW）。`build.bat` 会自动 `vcvars64.bat`；VS 装在 `C:\Program Files\Microsoft Visual Studio\18\Community`。
- **⚠️ EXE LOCK 陷阱**：跑 benchmark match（`Tools/benchmark/external_match.py`）时，`gobang-engine.exe` 被子进程占用，`build.bat` 的 LINK 步骤会 `LNK1104 无法打开文件`。解决：等 match 结束，或 build 到临时名 `cl ... /Fe:gobang-engine-tmp.exe /Fo:build\ src\*.c`。test exe 名字不同不受影响。
- 测试**不是 GoogleTest**，是自研 `tests/test_runner.h`（`ASSERT_TRUE`/`ASSERT_EQ` + `TEST_REPORT`）。9 套：board / search / pattern / zobrist / forbid / opening / 4_3_fix / threat_detect / search_n_strikes。当前 **492+ 断言全过**（test_search 18）。
- 跑测试：直接 `./test_<name>.exe`，看 `=== test_X: N/N passed ===`。

## 架构（模块 map）

| 文件 | 职责 |
|---|---|
| `board.c/h` | 棋盘状态、落子/悔棋、胜负判定（`board_check_winner`） |
| `zobrist.c/h` | Zobrist 增量 hash + 置换表（TT） |
| `pattern.c/h` | 手工评估函数（棋型计数 → 分数）。**NNUE 路线会替换这个** |
| `forbid.c/h` | 黑禁手判定：长连/双四/双三（真活三递归） |
| `opening.c/h` | 26 国规开局表 + D4 对称匹配 + 黑方开局轮转 |
| `search.c/h` | αβ + PVS + LMR + 迭代加深 + null-move + killer/history + VCF/VCT |
| `ui.c/h` | 国规 5-phase 协议交互 UI（开局/换边/W4/五手N打/对局循环） |
| `main.c` | CLI 解析（`--black/--white --depth=N --time=MS --auto-w4=XX --demo`）、Windows console 设置 |

## 关键约定 / 不变量（改代码前必读）

1. **国规规则**：黑先；黑恰好 5 连胜，≥6 连是长连禁手判负；白 ≥5 连胜（无禁手）。9.2-c：黑同时成 5 连 + 长连时，5 连优先（不算禁手）。
2. **Mate score**：`SEARCH_INF - ply`（root-relative，越短的杀分越高）。`SEARCH_INF=1e8`，`IS_MATE_SCORE` 阈值 `SEARCH_INF - 1e4`。TACTICAL_WIN_SCORE=5e7 在阈值下，不算 mate。**TT 存 mate 分要 ply 归一化**（`tt_score_for_store/load`，store +ply / load -ply）。
3. **Zobrist 不变量**：`board_place(color)` 要求 `color == b->side_to_move`（有 assert）。`board_undo` 用 XOR 自逆维护增量 hash。`board_init` 自动 idempotent 调 `zobrist_init`。
4. **Forbid 过滤**：黑禁手在搜索所有入口（root/αβ/VCx/N-打候选）+ UI 层 defense-in-depth 都过滤。WHITE 永远无禁手——`forbid_check_black` 只用于 BLACK。
5. **TT cutoff**：PV node 只接受 `TT_FLAG_EXACT` 切断（aspiration 窄窗保护）；非 PV 接受 LOWER/UPPER bound。
6. `search_find_n_distinct`：五手 N 打候选要 Chebyshev ≥2 distinctness（国规 rule 7）。

## 当前状态 & 路线图

- 最新 tag `v1.1-audit-fixes`；最新工作见 `git log`。
- **`NEXT_SESSION.md`** — 接续清单（Phase 1-3 todo）。
- **`KNOWN_BUGS.md`** — issue tracker（#29-#36，多数已 FIXED）。
- **`REVIEW.md`** — 国规 compliance audit 报告。
- 路线：Phase 1（手工冲 ~2400）→ Phase 2（NNUE，~2700）→ Phase 3（publish）。NNUE 路线下**不要再投 pattern.c eval 调优**（会被网络替换），优先 search.c 改进（转移到 NNUE 引擎）。

## Gotchas

- **UTF-8 console**：`main.c` 调 `SetConsoleOutputCP(65001)` + 开 ANSI VT。源码字符串混 UTF-8 中文，旧 Win10（<1607）VT 不支持会显示乱码 escape。
- **动态 CRT**：默认 `/MD`，产出依赖 `vcruntime140.dll`。要分发到干净机器加 `/MT`。
- **benchmark adapter**：`C:\Users\cthzy\Tools\benchmark\external_match.py`（**在本 repo 之外**），对接 Gomocup 协议引擎（Mintaka/Rapfi）。`tools/selfplay.py` 是引擎自对弈。
- **OPENINGS 表重复**：`external_match.py` 和 `tools/selfplay.py` 各有一份 26 开局副本，与 `src/opening.c` 可能漂移——改开局表要三处同步。

## 工作约定

- 改动后跑 `build.bat` + 全部 9 套测试确认 492/492。
- 每个有意义的改动单独 commit，conventional commits 风格（`fix(search):` / `feat(pattern):`）。
- ELO 影响的改动跑 `external_match.py` vs Mintaka（同档对手，50-100 局能测出 ±几 pp）。Rapfi 太强（几乎全输）不适合做 A/B。
- commit message 结尾：`Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>`
- 不要 push 除非用户明说。

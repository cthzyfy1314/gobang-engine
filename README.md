# gobang-engine

沈阳航空航天大学校内计算机博弈大赛 2026 — 五子棋（Renju）引擎。

## 项目状态

✅ **核心功能已完成**（2026-04-28 初始化 → 2026-04-30 完成 M1+M2+M3 里程碑）

| Tag | 完成日期 | 内容 |
|-----|---------|------|
| `m1-prep-day1` | 4/29 | board + αβ search 框架 + 不对称胜负判定 |
| `m1-pattern` | 4/30 | 7 种棋型识别 + 评估函数（替换 placeholder） |
| `m1-zobrist` | 4/30 | Zobrist 哈希 + 1M 置换表 + EXACT/LOWER/UPPER bounds |
| `m1-search-v2` | 4/30 | 迭代加深 + Killer Move + History Heuristic（节点数下降 84%） |
| `m1-forbid-complete` | 4/30 | 国规黑禁手判定（长连 / 双四 / 双三 + 国规 9.2-c）|
| `m1-ui-v1` | 4/30 | 交互式 UI（人肉协议）+ 黑1天元开局 + 禁手警告 |

测试覆盖：**129 / 129 全过**（50 board + 12 search + 47 pattern + 10 zobrist + 10 forbid）

## 规则

国规简化版：
- 15×15 棋盘，黑 1 天元（H8）
- 三种禁手（仅黑方）：长连（≥6 连）/ 四四 / 三三
- 国规 9.2-c：黑五连 + 禁手同时形成 → 五连优先（禁手失效）
- 不对称胜负：黑 5 连=胜 / 黑 ≥6=负 / 白 ≥5=胜

**已知简化**（5/9 调参时按需补）：
- 26 种指定开局 / 三手交换 / 五手 N 打：极简存根，黑 1 天元 hardcode
- 真活三递归判定：用形式判定（≥2 个 _XXX_ 或跳活三 _XX_X_ / _X_XX_ 即视为禁手）
- 跳活四 `_X_XXX_` `_XX_XX_` 等不识别（pattern 简化版）

## 技术路线

- **语言**：C99，Windows 平台，MSVC 编译
- **算法**：α-β minimax + 邻近 2 圈剪枝 + 棋型评估（7 种）+ Zobrist TT + 迭代加深 + Killer/History
- **比赛形式**：AI vs AI（人当信使，无自动协议；裁判人眼判禁手）

## 编译

需要 Visual Studio 2022/18 + 已安装 C++ 工具集。

**方式 1：直接跑 build.bat**（自动定位 vcvars64）：
```batch
build.bat
```

**方式 2：从 Developer Prompt 启动**：
```batch
"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
build.bat
```

产出：
- `gobang-engine.exe`（主程序）
- `test_board.exe` / `test_search.exe` / `test_pattern.exe` / `test_zobrist.exe` / `test_forbid.exe`

## 运行

### 交互式对战（默认）
```
gobang-engine.exe              # 我方执黑，深度 4
gobang-engine.exe --white      # 我方执白
gobang-engine.exe --depth=5    # 调深度
```

每回合的命令：
- `H8` / `h8` / `7,7` / `7 7`：在该位置落子
- `h`：让引擎给出当前局面的建议（不落子）
- `u`：撤销最后一步
- `r`：投降
- `q`：立即退出

人肉协议流程：对手下完后告诉你坐标 → 你输入对手坐标 → 引擎自动给出"我方建议" → 你口头报给对手 → 对手输入 → 循环。

### 自我对战 demo
```
gobang-engine.exe --demo --depth=3
```
跑 10 步自我对战 + 节点数统计。

## 设计文档

- 完整 spec：[`docs/superpowers/specs/2026-04-28-gobang-engine-design.md`](docs/superpowers/specs/2026-04-28-gobang-engine-design.md)
- 每日执行 plan：[`docs/superpowers/plans/`](docs/superpowers/plans/)

## 模块结构

```
src/
├── main.c          入口（CLI 参数解析）
├── board.c/.h      棋盘 + 落子/撤子 + 不对称胜负判定
├── pattern.c/.h    7 棋型识别 + 整盘评估
├── search.c/.h     αβ minimax + ID + Killer/History + Move Ordering
├── zobrist.c/.h    Zobrist 增量哈希 + 1M TT
├── forbid.c/.h     国规黑禁手（长连/双四/双三）
├── opening.c/.h    开局（极简：黑1天元）
└── ui.c/.h         交互式 UI（棋盘绘制 + 输入解析 + 主循环）

tests/
├── test_runner.h   零依赖测试宏
├── test_board.c    50 cases
├── test_pattern.c  47 cases
├── test_search.c   12 cases
├── test_zobrist.c  10 cases
└── test_forbid.c   10 cases
```

## 作者

陈天航 · 沈阳航空航天大学 · 飞设 2511

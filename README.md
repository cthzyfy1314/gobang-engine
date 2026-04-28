# gobang-engine

沈阳航空航天大学校内计算机博弈大赛 2026 — 五子棋（Renju）引擎。

## 项目状态

🚧 开发中（2026-04-28 初始化，2026-05-10 比赛）

## 规则

完整国规：
- 15×15 棋盘
- 26 种指定开局（直指 13 + 斜指 13）
- 三手交换 / 五手 N 打
- 三种禁手（黑专属）：三三禁手、四四禁手、长连禁手

## 技术路线

- **语言**：C99，Windows 平台，MSVC 编译
- **算法**：α-β minimax + 棋型评估 + 简化 VCF 算杀
- **比赛形式**：AI vs AI（人当信使，无自动协议）

## 设计文档

完整 spec 见 [`docs/superpowers/specs/2026-04-28-gobang-engine-design.md`](docs/superpowers/specs/2026-04-28-gobang-engine-design.md)

## 编译

```bash
build.bat
```

## 作者

陈天航 · 沈阳航空航天大学 · 飞设 2511

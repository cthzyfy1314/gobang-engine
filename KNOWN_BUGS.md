# Known Engine Weaknesses

Tracking issues that aren't crashes / wrong output but represent strength gaps.
Each entry is reproducible — open the engine with the indicated config and replay the moves.

---

## #29 — 四三 (four-three) double-threat blindness

**Status:** PARTIALLY FIXED via leaf-level threat extension in
`src/search.c` (`color_has_winning_setup` at depth_left == 0,
returns `TACTICAL_WIN_SCORE`). Regression test in
`tests/test_4_3_fix.c` confirms engine now picks **K9** (correct
defensive cut) instead of K11 on the reproducer below. Score went
from +1030 (old wrong choice) to +810 (new correct K9).

Remaining gap: mid-search nodes (depth_left > 0) still use plain
`pattern_evaluate`, which doesn't score "proto-open-three"
(2 stones with both extensions free). Real fix is in pattern.c.
For now the leaf extension catches the worst cases.

**Reproducer:**

```
./gobang-engine.exe --depth=12 --time=15000  (AI = BLACK)

Phase 1 opening: 花月 (Kagetsu)
  B1=H8  W2=H9  B3=I10  N=5
  W4=I9 (opponent picks)
  B5=H11 (engine's top candidate from 5)

Phase 5:
  W6=J9     (opponent extends row 9)
  B7=G9     (score=230)         engine blocks row-9 sleep-three from left
  W8=I7     (opponent diag seed)
  B9=I11    (score=130)         engine builds row 11
  W10=J8    (opponent diag grows: I7-J8 anti-diagonal)
  B11=K11   (score=1030)  *** MISTAKE ***
            engine pursues own row-11 attack;
            ignores that white has 2 stones on anti-diag (I7-J8)
            with K9 free, setting up the 四三 fork.
  W12=K9    *** 四三 double threat ***
            row 9:        H9-I9-J9-K9 = sleep four (must block L9)
            anti-diag:    I7-J8-K9    = open three (H6 / L10 free)
  B13=L9    (score = -99999984, mate-known)  forced block
  W14=J11   (column-J four threat setup: J8-J9-J11)
  B15=J12
  W16=J10   (column J sleep-four: J8-J9-J10-J11)
  B17=J7    (forced block)
  W18=L10   *** anti-diag open four: I7-J8-K9-L10 ***
  B19=H10   (mate already, score = -99999980, nodes=129)
  W20=M11   *** five: I7-J8-K9-L10-M11, WHITE wins ***
```

**Root cause:**

1. `pattern.c` evaluation treats 2 same-color stones on an open line as
   `PAT_OPEN_TWO` (score 100) only. It does not detect that the line is
   one move away from becoming an open three that combines with another
   threat to form 四三.

2. Tactical move ordering in `search.c` boosts threat-create candidates
   (own attack) more than threat-defend (cutting opponent's growing line).
   At B11, the engine never seriously evaluates `K9` as a defensive option
   because K11 (own attack) scores higher in move ordering.

3. Nominal `--depth=12` under 15s budget effectively reaches d≈9 (see P7
   report). The forced-loss sequence from B11 to mate is 10 plies. Right
   at / past the horizon — alpha-beta likely prunes the relevant branch.

**Fix candidates (not implemented):**

- **Pre-threat eval:** in `pattern.c`, add scoring for 2 stones on an open
  line of length ≥ 5 with both extension ends free. Treat as
  proto-open-three. Score ~300.
- **Threat extension at root:** before normal search, run a 1-ply
  opponent-VCT scan: "if I do nothing here, what's the best forcing
  sequence opponent has?". If they have a winning four-three sequence,
  restrict candidate moves to those that *block* the threat (not pursue
  own attack).
- **Selective deepening on opponent threats:** in `alphabeta`, if
  opponent's recent move created a stone on a line where they already
  have ≥ 2 stones with open ends, extend search by +2 plies on
  defensive replies. Cheap and tactic-specific.

The first option is the simplest; the second is the most powerful. Don't
mix without measuring against this game and other known positions.

**Regression target:**

After any eval / search change, replay the moves above and verify that
the engine's B11 choice is *not* `K11`. Acceptable defensive moves:
`K9`, `L10`, `H6`, `J10` (each cuts at least one of white's emerging
threats). If B11 still picks K11 the fix didn't help; if B11 picks
something silly (e.g., a corner), the eval change broke something
unrelated. Automated test: `test_4_3_fix.exe`.

---

## #31 — Opening name → coordinate mapping not canonical RIF order

**Status:** CLOSED (2026-05-19, no actual mismatch found).

国规 compliance audit (2026-05-19) hand-verified all 26 (idx, RIF code,
Chinese/Japanese name, B3 coord) tuples in `src/opening.c:52-88` against
RIF canonical. Idx ordering is by row-major scan, but every entry carries
its correct RIF code + name in the comments. No actual name↔coord
mismatch exists. Risk in original issue was hypothetical only.

---

## #32 — Engine always opens as 寒星 when playing BLACK

**Status:** FIXED (`src/opening.c:113-133`).

`opening_choose_by_black_strategy` now rotates through a curated pool
of 5 openings (寒星 / 花月 / 疏星 / 长星 / 浦月) using a static
call counter. Avoids the always-寒星 footgun while staying
deterministic (test-friendly).

---

## #33 — Opening swap decision is shallow

**Status:** FIXED (2026-05-19, commit-pending).

Part 1 (originally identified): static `pattern_evaluate` → already
upgraded to αβ search at `ui.c:368`.

Part 2 (newly identified by 100-game vs-Mintaka data showing WHITE 41%
vs BLACK 49%): threshold tightened from `-300` to `-50` (`ui.c:381`).

Rationale: at search depth 4-6 the returned score already incorporates
each opening's intrinsic balance — the 26 国规 openings are designed
to be roughly balanced (|equilibrium| typically < 100). Old `-300`
threshold only triggered swap in disastrous-loss positions; new `-50`
triggers swap whenever WHITE is meaningfully worse than BLACK (with
50-point noise margin to avoid jitter).

If future data shows per-opening calibration is needed (e.g.,
Kagetsu/Hogetsu intrinsically BLACK-favored by >50), upgrade to a
per-opening threshold table indexed by `opening_idx`.

---

## #34 — Phase 4 AI=WHITE picks B5 with static pattern eval

**Status:** FIXED (2026-05-19, commit-pending).

`ui.c:482-516` now runs `search_best_move_timed(b, min(depth,4),
time_budget/N)` per candidate after `board_place(BLACK)`, picks the
candidate giving best score from WHITE's view. Time floor of 100ms
per candidate. Per-candidate score printed for diagnostic visibility.

Mirrors phase 2 swap's αβ approach. Total phase 4 latency now bounded
by user's `--time` budget (each candidate gets ~budget/N).

Motivation data: vs Mintaka 100 games (seed=42 + seed=99 combined),
WHITE side won 16/39 (41%) vs BLACK side 30/61 (49%). The 8 ppt gap
points to WHITE-only weaknesses — this + #33 are the candidates.

---

## #35 — N-strike candidates restricted to neighbor moves

**Status:** open. Identified by 国规 audit (2026-05-19).

`search_find_n_distinct` (`search.c:316-398`) only considers moves from
`search_generate_neighbor_moves` (2-ring + 4-direction extension).
Far-board candidates are silently excluded. National rule 7 doesn't
require BLACK's N candidates to be adjacent to existing stones — a
strong opening play with a far-board strike is legal but our engine
will never propose it.

**Risk:** strategic ceiling cap, low (no校赛 weak opponent will exploit).
Only relevant if facing tactically aware opponents who recognize
distant candidates as legitimate.

**Fix:** widen `search_generate_neighbor_moves` to include all empty
cells with `legal_n < N` fallback, or add a separate "phase-4-only"
move generator.

---

## #36 — True open-three recursion: geometric-only, no legality check

**Status:** open. Identified by 国规 audit (2026-05-19).

`is_true_open_three_through_center` (`src/forbid.c:89-148`) verifies
that the candidate three could form a geometric `_XXXX_` via any
EMPTY extension cell. National rule defines a "true open three" as
one whose completing-to-four move is itself a *legal* move (not a
forbid).

Current impl is over-strict: it counts geometric three-patterns where
the four-completion is actually a禁手 (e.g., would form double-four)
as "true open threes" anyway. This causes AI to over-detect
double-three forbids and refuse some moves that国规 actually allows.

**Direction:** add second-layer legality verification — for each
candidate four-completion cell, recursively check it's not itself a
禁手. Beware infinite recursion (cap depth or use cycle detection).

**Compliance impact:** none — over-strict means AI refuses legal moves,
not the reverse. Strength impact: ~20-50 ELO loss in tight tactical
positions where the AI declines a strong move.

---

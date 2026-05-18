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

**Status:** open. Identified by 国规 audit.

`src/opening.c:47-82` assigns the 26 RIF opening names to coordinates
by row-major scan order, not by RIF canonical numbering. Comment at
`opening.c:21-24` acknowledges this. Geometry is correct (13 direct +
13 indirect, all B3s within 5×5 of H8, all distinct). Names are not
guaranteed to match the labels referees / opponents use.

**Risk:** in competition the engine may print "金星" for a board
position that the opponent calls "丘月" or vice versa. Same coords,
different labels.

**Fix:** cross-check against an authoritative RIF or 中国连珠协会
opening chart. Reorder the `RenjuOpening` array entries to match. Add
a `test_openings_canonical_names` referencing the standard table. No
code changes beyond data — geometry stays.

---

## #32 — Engine always opens as 寒星 when playing BLACK

**Status:** open.

`opening_choose_by_black_strategy` (`src/opening.c:107`) returns 0
unconditionally → engine always plays 寒星. A prepared opponent
trivially prepares one anti-寒星 line and exploits it every game.

**Fix candidates:**
- Random pick from a curated subset (3-5 openings the engine plays well)
- Pick based on engine self-play statistics (which opening gives best winrate)
- Pick based on opponent profile (if known)

Not load-bearing for校赛 if opponents are weak, but trivially fixable
and worth doing before serious matches.

---

## #33 — Opening swap decision is shallow

**Status:** open.

`phase2_swap` in `src/ui.c:354-407` decides whether AI-as-WHITE swaps
based on a single `pattern_evaluate < -300` threshold. No depth-search.
Strong play would do a 4-6 ply search before deciding. Currently the
AI may refuse advantageous swaps or accept disadvantageous ones.

**Fix:** call `search_best_move_timed(b, 6, time_budget/3)` before the
swap check; use that score instead of `pattern_evaluate`.

---

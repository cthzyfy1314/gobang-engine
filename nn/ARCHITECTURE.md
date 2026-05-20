# NNUE Evaluation for gobang-engine — Architecture & Phase 2 Blueprint

> Renju (国规) engine, C99 αβ+PVS+LMR+TT, ~3100 LOC. This doc designs the NNUE
> network that **replaces `pattern_evaluate`**. Baseline ~2050–2150 ELO; target
> ~2700 (Rapfi tier). Research-before-coding artifact — all design choices are
> grounded in cited prior art (Rapfi/Mixnet, Stockfish NNUE).

---

## 1. Decision summary (search-first matrix)

| Component | Decision | Rationale |
|---|---|---|
| Overall paradigm | **ADOPT** NNUE (not AlphaZero) | We keep αβ search. NNUE is a CPU, incrementally-updatable eval that drops into a make/unmake search with no MCTS/GPU rewrite. AlphaZero would force a full MCTS+GPU rewrite (see §3). |
| Input feature design | **EXTEND** Rapfi/Mixnet's line-pattern idea | Rapfi proves that *line-shaped local patterns* are the right inductive bias for gomoku (it won GomoCup 2024, ranked #1 of 520 on Botzone) [1][2]. We borrow the concept but use a simpler, directly-indexable feature set we can train ourselves. |
| Inference structure & quantization | **ADOPT** Stockfish NNUE structure | int16 accumulator + ClippedReLU + int8 linear layers + int32 dot-products is a battle-tested, SIMD-friendly recipe [3][4][5]. |
| Network weights | **BUILD** custom (train from scratch) | Rapfi's nets are tuned for *its* feature layout/codebook; not loadable into our engine. We train our own in PyTorch. |
| Training data | **BUILD** via teacher distillation from our own αβ | We have no strong oracle except Rapfi (impractical to query at scale); our deep-search scores are a usable teacher (see §4). |

**Net:** BUILD a custom NNUE, *informed by* Rapfi's feature philosophy and *structured like* Stockfish's quantized inference.

---

## 2. Recommended architecture (concrete numbers)

### 2.1 Input features — "HalfLine" set

Chess HalfKP keys every piece relation to king position. Gomoku has no king, so the
analogue is the **local line pattern**: gomoku tactics are fully described by what
happens along the 4 directions (`─ │ ╲ ╱`) through each cell. This is exactly the
inductive bias Rapfi exploits — it decomposes the board into 11-cell line segments
per direction and indexes a pattern codebook [1][2].

Our feature set, **HalfLine**, is the union of two groups:

1. **Raw stone planes** (dense, for the mapping head): one-hot `2 × 15 × 15`
   (BLACK plane, WHITE plane) — identical to Rapfi's `x ∈ {0,1}^{2×H×W}` input [1].
2. **Line-window features** (sparse, drive the accumulator): for each of the
   225 cells × 4 directions, take the length-9 window centered on the cell
   (`k ∈ [-4,4]`). Each cell in the window is one of {empty, own, opp, off-board} →
   encode the window as a **base-4 index**, but collapse to the canonical
   half-open trits used by pattern recognition. To keep the table small we key on
   the **6-cell window** (the maximal span that decides any single 5-in-a-row
   threat): `4^6 = 4096` patterns × 4 directions × 2 perspectives.

   **Active feature count per perspective:** 225 cells × 4 dirs = **900 active
   features** out of a feature space of `4096 × 4 = 16,384`. Sparsity ≈ 5.5%.

> Both perspectives (side-to-move and opponent) are accumulated separately, exactly
> like Stockfish's two-perspective accumulator [3][4]. The eval is always read from
> the side-to-move perspective, so the net never needs an explicit "whose turn" plane.

We also append **2 scalar planes** as direct inputs to the value head (bypassing the
accumulator): `move_count/225` (game phase) and a `black_to_move` bit. Renju
asymmetry (黑禁手) is *not* encoded in the net — forbidden-move filtering stays in
`forbid.c` at move generation, so the net only ever evaluates legal positions.

### 2.2 Topology

Mirrors Stockfish's proven shallow stack (most knowledge lives in layer 1) [4][5]:

```
HalfLine features (16,384 × 2 perspectives, ~900 active each)
        │  Feature Transformer (accumulator), incrementally updated
        ▼
   accumulator: 256 per perspective  →  concat = 512   (int16)
        │  ClippedReLU → int8
        ▼
   FC1: 512 → 32      ClippedReLU
        ▼
   FC2: (32 + 2 scalars) → 32   ClippedReLU
        ▼
   FC3: 32 → 1        (centipawn-equivalent score, int32 → rescale)
```

- **Accumulator width 256/perspective (512 total).** Stockfish's first NNUE shipped
  256×2 [3][4]; 256 is the smallest size that historically reached top strength and
  fits AVX2's 16×256-bit registers comfortably [5]. Gomoku's branching/eval is simpler
  than chess, so 256 is ample for ~2700 — we are not memory-bound, we are search-speed-bound.
- **Hidden layers 32→32→1**, same shape as classic Stockfish (`…→32→32→1`) [4].
- **Activation: ClippedReLU** `clamp(x,0,127)` everywhere (int8-friendly) [3][5].

### 2.3 Quantization (Stockfish recipe [3][4][5])

| Tensor | Type | Scale |
|---|---|---|
| Feature-transformer weights/bias, accumulator | **int16** | ×127 |
| Hidden-layer weights | **int8** | ×64 (weight_scale) |
| Hidden-layer bias | int32 | ×127×64 |
| Activations | int8 in `[0,127]` | — |
| Dot-product accumulation | int32 | — |
| Output rescale | right-shift `log2(64)=6` bits, then ×output_scale to map to engine centipawns | — |

Float weights repeatedly added/subtracted would accumulate error — hence integer
accumulator is mandatory, not just a speed trick [5].

---

## 3. Why NNUE, not AlphaZero

AlphaZero-style gomoku (AlphaGomoku, junxiaosong/AlphaZero_Gomoku, KataGo-derived nets)
uses a **policy + value** CNN driving **MCTS**, trained by self-play RL [6][7][8].
It is strong but mismatched to us:

- It needs **MCTS** — we'd discard our entire αβ+PVS+LMR+TT+VCF/VCT stack.
- It wants a **GPU** at inference for the CNN forward pass per node; our engine is
  single-threaded CPU.
- Value/policy nets are *not incrementally updatable* — every node is a full forward pass.

NNUE is the opposite: tiny int8 net, **incremental** accumulator, millions of
evals/sec/thread on CPU [5], and it slots straight into `evaluate()` where
`pattern_evaluate` lives today. Recommendation stands: **NNUE.**

---

## 4. Incremental update design

This is the whole point of "Efficiently **Updatable**." It mirrors our existing
**Zobrist incremental hash** pattern (XOR on place, XOR-inverse on undo).

A move at cell `(r,c)` only changes the *line windows that pass through (r,c)* — at
most 4 directions × (window length) cells, i.e. a bounded, small set of feature
indices flips. So on `board_place`:

```
for each of 4 directions through (r,c):
    for each window position W overlapping (r,c):
        old_feat = feature_index(W before the stone)
        new_feat = feature_index(W after the stone)
        for p in {SELF, OPP perspective}:
            acc[p] -= W_ft[old_feat]      // subtract stale column (int16 SIMD)
            acc[p] += W_ft[new_feat]      // add new column
```

On `board_undo`, apply the exact inverse (swap old/new). To stay bug-safe we keep an
**accumulator stack** parallel to `board.history[]`: `board_undo` pops the saved
accumulator instead of recomputing — same lifetime as the existing move stack. This
makes undo O(1) and removes a class of "incremental ≠ recompute" bugs.

**Invariant + safety net:** a debug build recomputes the accumulator from scratch
(`acc_refresh`) after every make and `assert`s it equals the incrementally-updated
one. This is the standard NNUE correctness guard [4][5] and directly addresses Risk R3.

Practically: ~10–20 feature add/sub pairs per move (a few hundred int16 ops),
negligible vs a node's other work.

---

## 5. Training pipeline

### 5.1 Data volume & generation
Rapfi trained on **~30.8M self-play positions** [1][2]. We target a comparable
**20–40M positions** for the ~2700 goal; a first working net needs only ~2–5M.

Generation: **self-play with the current engine** (`tools/selfplay.py` already exists)
using the 26 国规 openings + randomized early plies (temperature on the first ~6 moves)
for diversity. Renju forbidden-move filtering is on, so all positions are legal.

### 5.2 Labeling — teacher distillation (recommended)
We have **no strong oracle** besides Rapfi (impractical to query for tens of millions
of positions). So distill from **our own deep αβ search** — teacher-student on
*ourselves*, which is well-established for engine NNUE training [4] and aligns with
the "use a deeper/slower process to label a faster student" distillation principle [9].

**Combined loss** (modern engine practice — both Stockfish and Rapfi mix eval target
with game outcome [1][4]):

```
loss = (1-λ)·MSE(sigmoid(net), sigmoid(teacher_score))   # distill αβ depth-D eval
     +    λ ·MSE(sigmoid(net), game_result∈{0,0.5,1})     # ground-truth outcome
```

- `teacher_score`: our αβ score at a fixed depth (e.g. D=10–12) or fixed node budget,
  taken from the same self-play games (label-as-you-play, cheap reuse of search).
- `game_result`: final outcome of the self-play game the position came from.
- `λ ≈ 0.3` initially (lean on the teacher early; outcome regularizes), tune later.
- `sigmoid` with a scaling constant maps centipawns→win-prob so MSE is well-behaved.

This **bootstraps**: train net → drop into engine → engine plays stronger → relabel /
generate new self-play → retrain. 2–3 such generations is typical to climb.

### 5.3 PyTorch model sketch
```python
class HalfLineNNUE(nn.Module):
    def __init__(self, n_feat=16384, acc=256):
        super().__init__()
        self.ft  = nn.Linear(n_feat, acc)        # feature transformer, per perspective
        self.fc1 = nn.Linear(2*acc, 32)
        self.fc2 = nn.Linear(32 + 2, 32)         # +2 scalar planes
        self.fc3 = nn.Linear(32, 1)
    def forward(self, feat_self, feat_opp, scalars):
        a = torch.cat([self.ft(feat_self), self.ft(feat_opp)], dim=1)  # shared FT weights
        x = torch.clamp(a, 0, 1)                  # ClippedReLU (float domain [0,1])
        x = torch.clamp(self.fc1(x), 0, 1)
        x = torch.clamp(self.fc2(torch.cat([x, scalars], 1)), 0, 1)
        return self.fc3(x)
```
Train in float; **quantization-aware** clamping during training, then export
int16/int8 with the §2.3 scales. (nnue-pytorch implements exactly this clamp-then-quantize
flow [4].)

### 5.4 Export
Serialize to a **binary blob** (magic + version + arch hash + int16 FT block + int8 FC
blocks), loaded by `nn_load()` at startup via `mmap`/`fread`. A header-array fallback
(`weights.h` with `static const int16_t`) is fine for the very first prototype but
doesn't scale — go binary by Phase 2.4. (Rapfi ships lz4-compressed serialized blobs [2].)

---

## 6. C integration plan

- **New module `src/nn_eval.c` / `nn_eval.h`**: `nn_load(path)`, `nn_refresh(board)`,
  `nn_update_place(r,c,color)`, `nn_update_undo()`, `int nn_evaluate(const Board*)`.
- **Hook point — `search.c`**: today the leaf calls `pattern_evaluate(b)`. Replace with
  `nn_evaluate(b)`. Keep `pattern_evaluate` compiled and selectable behind a flag
  (`--eval=nn|pattern|blend`) for A/B regression vs Mintaka.
  - **Blend mode** during bring-up: `score = (1-α)·pattern + α·nn` to de-risk a bad net.
- **Accumulator lifecycle**: hook `nn_update_place`/`nn_update_undo` into the same call
  sites as `board_place`/`board_undo` so the accumulator and Zobrist hash move in lockstep.
  Push/pop accumulator on the stack alongside `board.history[]`.
- **SIMD level**: ship **scalar first** (correct + portable, validate ELO), then **AVX2**
  (`_mm256_add/sub_epi16` for the accumulator, `_mm256_maddubs_epi16` + `_mm256_madd_epi16`
  for int8×int8→int32 dot products) [5]. AVX-512/VNNI optional later. Target millions
  of evals/sec/thread [5]. MSVC: `/arch:AVX2`, runtime-guard with `__cpuid`.
- **Mate scores untouched**: `nn_evaluate` returns only non-terminal heuristic scores;
  `SEARCH_INF - ply` mate logic and VCF/VCT stay exactly as-is.

---

## 7. Phased implementation checklist (maps to Phase 2.1–2.6)

- **2.1 — PyTorch scaffold.** Implement `HalfLineNNUE`, feature extractor in Python,
  overfit a few hundred positions to verify the net can learn αβ scores. *(✓ = train
  loss → 0 on tiny set.)*
- **2.2 — Data pipeline.** Extend `tools/selfplay.py` to dump `(features, αβ_score,
  outcome)` in `packed_binary`; generate first 2–5M positions.
- **2.3 — Train v0.** Combined loss, λ=0.3; sanity-check eval correlates with pattern eval
  on held-out tactical positions.
- **2.4 — C inference (scalar) + export.** `nn_eval.c` scalar forward + binary loader;
  unit test: C `nn_evaluate` matches PyTorch (dequantized) within rounding on N positions.
- **2.5 — Incremental accumulator + debug recompute assert.** Wire into make/unmake;
  pass the refresh-equals-incremental assert across a full self-play game.
- **2.6 — Integration + AVX2 + A/B.** Swap into `search.c` (blend→full), add AVX2 path,
  run `external_match.py` vs Mintaka; iterate generations (relabel→retrain) toward 2700.

Existing test discipline applies: `build.bat` + 9 suites stay green (492+ assertions);
add `tests/test_nn.c` (loader, scalar==reference, incremental==refresh).

---

## 8. Risk register

| ID | Risk | Likelihood | Mitigation |
|---|---|---|---|
| R1 | **Training divergence / weak net** (loss plateaus, ELO regresses) | Med | Start in **blend mode** (α small); validate on tactical suite before trusting; keep `pattern_evaluate` as fallback; tune λ. |
| R2 | **Quantization accuracy loss** (int8 net much weaker than float) | Med | Quantization-aware clamp in training; verify quantized-C output ≈ float-PyTorch within tolerance (test 2.4); if loss >50 ELO, widen to int16 hidden or larger accumulator. |
| R3 | **Incremental-update bug** (acc drifts from true value → silent eval corruption) | High | Debug-build `assert(acc == acc_refresh())` after every move (§4); accumulator stack for undo, never recompute on undo. |
| R4 | **Speed regression** (NNUE slower than pattern eval → shallower search → net ELO loss) | Med | Measure nodes/sec before/after; scalar→AVX2; 256-wide accumulator is the speed/strength knob — shrink to 128 if needed. |
| R5 | **Teacher ceiling** (distilling our own ~2100 αβ can't reach 2700 alone) | High | Iterate generations (self-play with the *improved* net relabels at higher quality); increase teacher depth; the search itself adds strength beyond the net. |
| R6 | **Renju asymmetry leakage** (net learns illegal black moves are good) | Low | Forbidden-move filtering stays in `forbid.c` at movegen; net only ever sees/scores legal positions; add `black_to_move` scalar so the net can value the asymmetric win condition. |
| R7 | **Feature-extractor mismatch** (Python vs C compute different indices) | Med | Single golden-vector test: same position → identical feature index list in both (test 2.4). |

---

## 9. Sources

1. Li et al., *Rapfi: Distilling Efficient Neural Network for the Game of Gomoku*, arXiv:2503.13178 — https://arxiv.org/abs/2503.13178 (Mixnet: 2×H×W one-hot input, 4-direction 11-cell line patterns + pattern codebook, ~30.8M self-play positions, GomoCup 2024 winner, #1 of 520 on Botzone)
2. dhbloo, *pytorch-nnue-trainer* — https://github.com/dhbloo/pytorch-nnue-trainer (Mix6/Mix8/Mix9/NNUE models, `dim_middle/dim_policy/dim_value` config, packed_binary data, lz4 serialized export for Rapfi); *rapfi* — https://github.com/dhbloo/rapfi (αβ + classical/NNUE eval); *rapfi-networks* — https://github.com/dhbloo/rapfi-networks
3. *Efficiently updatable neural network* — Wikipedia — https://en.wikipedia.org/wiki/Efficiently_updatable_neural_network (Nasu 2018, HalfKP, 256×2 accumulator, int8 weights/int16 accumulator, incremental update)
4. *NNUE Architecture Reference*, official-stockfish/nnue-pytorch (DeepWiki) — https://deepwiki.com/official-stockfish/nnue-pytorch/9-nnue-architecture-reference (SFNNv1 256×2→32→32→1; int16 FT, int8 linear, int32 dot, ClippedReLU [0,127]; layer stacks)
5. *NNUE*, Stockfish docs (nnue-pytorch wiki) — https://official-stockfish.github.io/docs/nnue-pytorch-wiki/docs/nnue.html (quantization scales: FT ×127, weight_scale 64, right-shift 6; AVX2 `maddubs`/`madd`; accumulator refresh vs update; size/strength guidance; float-error rationale)
6. *AlphaZero Gomoku*, arXiv:2309.01294 — https://arxiv.org/abs/2309.01294 (policy+value + MCTS, GPU self-play RL)
7. junxiaosong, *AlphaZero_Gomoku* — https://github.com/junxiaosong/AlphaZero_Gomoku
8. Xie et al., *AlphaGomoku*, arXiv:1809.10595 — https://arxiv.org/abs/1809.10595 (AlphaGo-based, curriculum learning, GPU)
9. *Self-distillation* (teacher = deeper/slower process) — https://medium.com/better-ml/self-distillation-20a42c0be415

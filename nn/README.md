# nn/ — NNUE evaluation (Phase 2)

Replaces `pattern_evaluate` with a learned neural network. See
[`ARCHITECTURE.md`](ARCHITECTURE.md) for the full design + cited prior art.

## Status: Phase 2.1 scaffold

We follow **start-simple-then-complex**:
1. **2.1 (now)** — verify the *training pipeline* with the simplest possible
   net: raw `2×15×15` board planes → MLP → scalar eval. Overfit a few hundred
   positions to confirm the net can learn αβ scores at all. This de-risks the
   plumbing (data → train → loss converges) before investing in the fancy
   HalfLine feature set.
2. **2.2** — upgrade `encoding.py` to the HalfLine line-pattern features
   (4-dir windows, see ARCHITECTURE.md §2.1) + scale up self-play data.
3. **2.3** — train v0 with combined distill+outcome loss.
4. **2.4-2.6** — C inference (`src/nn_eval.c`), incremental accumulator, AVX2,
   A/B vs Mintaka. See ARCHITECTURE.md §7.

## Environment

```bash
# torch 2.10 CPU is already installed. For GPU training (RTX 5070), reinstall:
#   pip install torch --index-url https://download.pytorch.org/whl/cu124
# CPU is fine for the 2.1 overfit sanity; GPU matters at 2.3 scale (millions of positions).
python --version   # 3.13.13
python -c "import torch; print(torch.__version__, torch.cuda.is_available())"
```

No `uv` here yet (engine repo is C). If we add a real training dependency set,
introduce `nn/requirements.txt` or `nn/pyproject.toml` then.

## Files

- `model.py` — PyTorch models. `SimpleEvalMLP` (2.1 sanity) + `HalfLineNNUE` stub (2.2+).
- `encoding.py` — board → tensor. `encode_planes` (2.1, raw stone planes) + HalfLine TODO.
- `tutorial/` — throwaway learning scripts (overfit demos, MNIST, etc.). gitignored.
- `data/` — generated self-play datasets (.npz). gitignored.
- `checkpoints/` — trained weights. gitignored.

## 2.1 sanity workflow (target)

```
1. Generate ~500 positions: self-play with gobang-engine.exe, label each with
   its αβ depth-8 score (extend tools/selfplay.py — Phase 2.2 work).
2. encode_planes(board) -> (2,15,15) tensor; label = tanh(score/scale).
3. Train SimpleEvalMLP on the 500, confirm train-loss -> ~0 (overfit OK here —
   we're testing capacity/plumbing, not generalization yet).
4. ✓ gate: loss converges => pipeline works => proceed to 2.2.
```

## Key design constraints (from ARCHITECTURE.md)

- Eval is always **side-to-move perspective** (positive = good for mover).
- Renju 黑禁手 stays in `forbid.c` at movegen — the net only sees legal positions,
  no need to encode forbidden-ness as a feature for v0 (revisit in research ext).
- Final net must be **quantizable** (int16 acc / int8 layers) + **incrementally
  updatable** — but 2.1 trains in float; quantization is a 2.4 concern.
- C inference will mirror this Python forward pass exactly (golden-vector test).

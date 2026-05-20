"""Phase 2.1 ✓-gate trainer: overfit SimpleEvalMLP on a self-play .npz.

Goal here is NOT generalization — it's to confirm the data->encode->train->loss
plumbing works and a net CAN fit the αβ search scores. Success = training loss
drives toward ~0 on a few hundred positions.

Dataset is produced by tools/selfplay.py --nn-out (see nn/README.md). It stores
raw board `cells` + `stm` + raw `score` + `outcome`; we encode planes here and
build the distillation target = sigmoid(score/scale) so `scale` stays tunable.

Run (from repo root):
    python nn/train.py --data nn/data/selfplay_v1.npz --epochs 400
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Windows consoles default to GBK; force UTF-8 so status glyphs don't crash.
try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

import numpy as np
import torch
import torch.nn as nn

# Allow `python nn/train.py` from the repo root.
sys.path.insert(0, str(Path(__file__).resolve().parent))
import encoding  # noqa: E402
from model import SimpleEvalMLP  # noqa: E402


def load_dataset(path: str, scale: float):
    """Load .npz -> (planes float32 (N,2,15,15), target (N,1), outcome (N,1))."""
    d = np.load(path)
    cells = d["cells"]            # (N,15,15) int8
    stm = d["stm"].astype(np.int64)   # (N,)
    score = d["score"].astype(np.float32)
    outcome = d["outcome"].astype(np.float32)

    n = cells.shape[0]
    opp = np.where(stm == encoding.BLACK, encoding.WHITE, encoding.BLACK)
    # Perspective-relative planes, vectorized over the batch.
    own_plane = (cells == stm[:, None, None]).astype(np.float32)
    opp_plane = (cells == opp[:, None, None]).astype(np.float32)
    planes = np.stack([own_plane, opp_plane], axis=1)  # (N,2,15,15)

    # Stable sigmoid distillation target; clip to avoid overflow on mate scores.
    target = 1.0 / (1.0 + np.exp(-np.clip(score / scale, -30.0, 30.0)))
    return (
        torch.from_numpy(planes),
        torch.from_numpy(target.astype(np.float32)).unsqueeze(1),
        torch.from_numpy(outcome).unsqueeze(1),
        n,
    )


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--data", default="nn/data/selfplay_v1.npz")
    p.add_argument("--epochs", type=int, default=400)
    p.add_argument("--lr", type=float, default=1e-3)
    p.add_argument("--hidden", type=int, default=256)
    p.add_argument("--scale", type=float, default=800.0,
                   help="score_to_target sigmoid scale (Renju default ~800).")
    p.add_argument("--out", default="nn/checkpoints/simple_eval_overfit.pt")
    p.add_argument("--seed", type=int, default=0)
    args = p.parse_args(argv)

    torch.manual_seed(args.seed)
    device = "cuda" if torch.cuda.is_available() else "cpu"

    planes, target, outcome, n = load_dataset(args.data, args.scale)
    planes, target, outcome = planes.to(device), target.to(device), outcome.to(device)
    print(f"[train] {n} positions  device={device}  scale={args.scale}")
    print(f"[train] target mean={target.mean():.3f} std={target.std():.3f}  "
          f"outcome mean={outcome.mean():.3f}")

    model = SimpleEvalMLP(hidden=args.hidden).to(device)
    n_params = sum(q.numel() for q in model.parameters())
    print(f"[train] SimpleEvalMLP params={n_params}")

    opt = torch.optim.Adam(model.parameters(), lr=args.lr)
    loss_fn = nn.BCEWithLogitsLoss()  # soft labels in (0,1) are fine for BCE

    # Two reference points for the BCE (note: soft labels have a nonzero floor).
    #  - constant-mean baseline: predict the dataset-mean target everywhere.
    #  - entropy floor: BCE of a PERFECT predictor = mean target entropy. A
    #    correct overfit converges to THIS, not to 0 — so the real gate is
    #    target-MAE -> 0, not BCE -> 0.
    base_logit = torch.logit(target.mean().clamp(1e-4, 1 - 1e-4))
    base_loss = loss_fn(base_logit.expand_as(target), target).item()
    t = target.clamp(1e-6, 1 - 1e-6)
    entropy_floor = (-(t * t.log() + (1 - t) * (1 - t).log())).mean().item()
    print(f"[train] BCE: constant-mean baseline={base_loss:.4f}  "
          f"perfect-fit floor={entropy_floor:.4f}")

    model.train()
    for ep in range(1, args.epochs + 1):
        opt.zero_grad()
        logit = model(planes)
        loss = loss_fn(logit, target)
        loss.backward()
        opt.step()
        if ep == 1 or ep % max(1, args.epochs // 10) == 0 or ep == args.epochs:
            with torch.no_grad():
                pred = torch.sigmoid(logit)
                mae = (pred - target).abs().mean().item()
                # sign agreement vs game outcome (winprob>0.5 ↔ eventual win)
                agree = ((pred > 0.5) == (outcome > 0.5)).float().mean().item()
            print(f"  epoch {ep:>4}  BCE={loss.item():.5f}  "
                  f"target-MAE={mae:.4f}  outcome-agree={agree:.3f}")

    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    torch.save({"model": model.state_dict(), "hidden": args.hidden,
                "scale": args.scale}, args.out)

    # Final overfit metric: does the net reproduce the per-position targets?
    with torch.no_grad():
        final_mae = (torch.sigmoid(model(planes)) - target).abs().mean().item()
    passed = final_mae < 0.02
    print(f"[train] saved -> {args.out}")
    print(f"[train] final BCE={loss.item():.5f} (perfect-fit floor "
          f"{entropy_floor:.4f})  target-MAE={final_mae:.4f}")
    print("[train] " + ("PASS: net overfits the search-score targets "
                         "(MAE<0.02) -> pipeline works, proceed to 2.3"
                         if passed else
                         "FAIL: targets not fit (MAE>=0.02) -> check pipeline"))
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())

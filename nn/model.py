"""PyTorch models for the NNUE eval.

Phase 2.1: SimpleEvalMLP on raw stone planes — verifies the training pipeline.
Phase 2.2+: HalfLineNNUE (the real, incrementally-updatable, quantizable net).
"""

from __future__ import annotations

import torch
import torch.nn as nn

BOARD_SIZE = 15


class SimpleEvalMLP(nn.Module):
    """Phase 2.1 sanity net: 2x15x15 planes -> MLP -> scalar (win-prob logit).

    Deliberately simple. Goal is to confirm a net CAN learn αβ scores and the
    data->train->loss pipeline works. NOT incrementally updatable, NOT the
    final architecture — just plumbing validation.
    """

    def __init__(self, hidden: int = 256):
        super().__init__()
        in_dim = 2 * BOARD_SIZE * BOARD_SIZE  # 450
        self.net = nn.Sequential(
            nn.Flatten(),
            nn.Linear(in_dim, hidden),
            nn.ReLU(),
            nn.Linear(hidden, 32),
            nn.ReLU(),
            nn.Linear(32, 1),
        )

    def forward(self, planes: torch.Tensor) -> torch.Tensor:
        # planes: (B, 2, 15, 15) -> (B, 1) logit; caller applies sigmoid for win-prob
        return self.net(planes)


class HalfLineNNUE(nn.Module):
    """Phase 2.2+ real NNUE. Per ARCHITECTURE.md §2.2.

    HalfLine features (16384, ~900 active/perspective) -> 256/perspective
    accumulator -> 512 -> 32 -> 32 -> 1, ClippedReLU, two-perspective.
    Trained in float with quantization-aware clamping, then exported int16/int8.

    Stub for now — implement once 2.1 plumbing is verified and encode_halfline
    exists. Sketch retained from the design doc:
    """

    def __init__(self, n_feat: int = 16384, acc: int = 256):
        super().__init__()
        self.ft = nn.Linear(n_feat, acc)        # shared feature transformer
        self.fc1 = nn.Linear(2 * acc, 32)
        self.fc2 = nn.Linear(32 + 2, 32)        # +2 scalar planes (phase, side)
        self.fc3 = nn.Linear(32, 1)

    def forward(self, feat_self, feat_opp, scalars):
        a = torch.cat([self.ft(feat_self), self.ft(feat_opp)], dim=1)
        x = torch.clamp(a, 0.0, 1.0)            # ClippedReLU (float domain)
        x = torch.clamp(self.fc1(x), 0.0, 1.0)
        x = torch.clamp(self.fc2(torch.cat([x, scalars], dim=1)), 0.0, 1.0)
        return self.fc3(x)


if __name__ == "__main__":
    # smoke: forward pass shapes
    m = SimpleEvalMLP()
    x = torch.zeros(4, 2, BOARD_SIZE, BOARD_SIZE)
    y = m(x)
    print("SimpleEvalMLP output shape:", tuple(y.shape))  # (4, 1)
    n_params = sum(p.numel() for p in m.parameters())
    print("SimpleEvalMLP params:", n_params)

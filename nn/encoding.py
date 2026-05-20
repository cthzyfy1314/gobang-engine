"""Board -> tensor encoding for the NNUE eval.

Phase 2.1: raw stone planes (simplest input that lets us verify the training
pipeline). Phase 2.2 will add the HalfLine line-pattern features per
ARCHITECTURE.md §2.1.

Board convention (must match src/board.h):
  - 15x15, cells[r][c] in {EMPTY=0, BLACK=1, WHITE=2}
  - r=0 is the TOP row (display row 15); display = (chr('A'+c), 15-r)
  - side_to_move in {BLACK=1, WHITE=2}

Eval convention: always from the SIDE-TO-MOVE perspective (positive = good for
the player to move), matching pattern_evaluate / search.
"""

from __future__ import annotations

import numpy as np

BOARD_SIZE = 15
EMPTY, BLACK, WHITE = 0, 1, 2


def encode_planes(cells: np.ndarray, side_to_move: int) -> np.ndarray:
    """Raw stone planes from the side-to-move perspective.

    Args:
        cells: (15, 15) int array in {0,1,2}.
        side_to_move: BLACK(1) or WHITE(2).

    Returns:
        (2, 15, 15) float32 tensor: plane 0 = side-to-move's stones,
        plane 1 = opponent's stones. Perspective-relative so the net never
        needs an explicit "whose turn" plane (mirrors Stockfish two-perspective).
    """
    assert cells.shape == (BOARD_SIZE, BOARD_SIZE), cells.shape
    opp = WHITE if side_to_move == BLACK else BLACK
    own_plane = (cells == side_to_move).astype(np.float32)
    opp_plane = (cells == opp).astype(np.float32)
    return np.stack([own_plane, opp_plane], axis=0)  # (2, 15, 15)


def score_to_target(score: int, scale: float = 800.0) -> float:
    """Map an engine centipawn-ish score to a win-probability target in (0,1).

    sigmoid(score/scale). scale ~800 is a reasonable Renju default (tune at 2.3).
    Used as the regression target so MSE is well-behaved (bounded).
    """
    return float(1.0 / (1.0 + np.exp(-score / scale)))


# --- Phase 2.2 TODO: HalfLine line-pattern features -------------------------
# For each of 225 cells x 4 directions, take the length-6 window, base-4 encode
# (empty/own/opp/off-board), index into 4096-pattern x 4-dir feature table.
# Must produce IDENTICAL indices to the C extractor (golden-vector test in 2.4).
# Deferred until the raw-plane pipeline (2.1) is verified end-to-end.
def encode_halfline(cells: np.ndarray, side_to_move: int):
    raise NotImplementedError("Phase 2.2 — see ARCHITECTURE.md §2.1")

#!/usr/bin/env python3
"""Self-play tournament harness for gobang-engine (Renju, 5-phase protocol).

Spawns TWO engine subprocesses per game: one in --black mode (the AI to move
as black) and one in --white mode (the AI to move as white). The harness
relays each engine's chosen move to the other, plus orchestrates the 5-phase
opening / swap / W4 / N-strikes / regular-play protocol described in src/ui.c.

Modes:
  * Single-engine self-play (default): --engine PATH plays both sides.
  * A/B comparison:  --engine-a PATH --engine-b PATH. Half the games are
    A=black/B=white and half swap. ELO delta + 95% CI printed.

Output:
  * JSON log written to --out (default selfplay_results.json).
  * Summary line to stdout.
  * Per-game progress lines to stdout if --progress (default on).

Constraints (per spec):
  * Python 3 stdlib only.
  * Engine processes can hang; wall-clock per game capped (--game-timeout).
  * Subprocesses killed cleanly on exception / timeout.
  * Encoding utf-8 with errors=replace.
"""

import argparse
import concurrent.futures
import json
import math
import os
import queue
import random
import re
import subprocess
import sys
import threading
import time

# ---------------------------------------------------------------------------
# 26 national-rule openings, copy of src/opening.c's RENJU_OPENINGS table.
# Each entry: (display_name, B1 coord, W2 coord, B3 coord)
# Coords are H8-style strings the engine accepts in phase 1.
# ---------------------------------------------------------------------------

OPENINGS = [
    # Direct (W2=H9)
    ("Kansei",    "H8", "H9", "H10"),
    ("Shogetsu",  "H8", "H9", "H7"),
    ("Zuisei",    "H8", "H9", "H6"),
    ("Keigetsu",  "H8", "H9", "I10"),
    ("Kagetsu",   "H8", "H9", "I9"),
    ("Ugetsu",    "H8", "H9", "I8"),
    ("Kyugetsu",  "H8", "H9", "I7"),
    ("Sangetsu",  "H8", "H9", "I6"),
    ("Sosei",     "H8", "H9", "J10"),
    ("Zangetsu",  "H8", "H9", "J9"),
    ("Kinsei",    "H8", "H9", "J8"),
    ("Shingetsu", "H8", "H9", "J7"),
    ("Yusei",     "H8", "H9", "J6"),
    # Indirect (W2=I9)
    ("Chosei",    "H8", "I9", "J10"),
    ("Kyogetsu",  "H8", "I9", "J9"),
    ("Ungetsu",   "H8", "I9", "I8"),
    ("Kosei",     "H8", "I9", "J8"),
    ("Shagetsu",  "H8", "I9", "G7"),
    ("Gingetsu",  "H8", "I9", "H7"),
    ("Hogetsu",   "H8", "I9", "I7"),
    ("Suigetsu",  "H8", "I9", "J7"),
    ("Suisei",    "H8", "I9", "F6"),
    ("Meigetsu",  "H8", "I9", "G6"),
    ("Myojo",     "H8", "I9", "H6"),
    ("Rangetsu",  "H8", "I9", "I6"),
    ("Ryusei",    "H8", "I9", "J6"),
]

# ---------------------------------------------------------------------------
# I/O helpers — read engine stdout char-by-char in a thread so we can detect
# prompts ('> ' at tail of buffer after idle settle) without blocking.
# ---------------------------------------------------------------------------

ANSI_RE = re.compile(r"\x1b\[[0-9;]*[a-zA-Z]")


def strip_ansi(s: str) -> str:
    return ANSI_RE.sub("", s)


class EngineProc:
    """Wraps a subprocess.Popen with a background reader thread + send()/recv()."""

    def __init__(self, exe_path: str, args, cwd=None):
        self.exe_path = exe_path
        self.proc = subprocess.Popen(
            [exe_path] + list(args),
            cwd=cwd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0,
        )
        self._q = queue.Queue()
        self._reader = threading.Thread(target=self._reader_loop, daemon=True)
        self._reader.start()
        self.buf = []  # transcript

    def _reader_loop(self):
        try:
            while True:
                ch = self.proc.stdout.read(1)
                if not ch:
                    self._q.put(None)
                    return
                self._q.put(ch)
        except Exception:
            self._q.put(None)

    def send(self, line: str):
        if self.proc.poll() is not None:
            raise RuntimeError("engine process exited before send()")
        data = (line + "\n").encode("utf-8")
        try:
            self.proc.stdin.write(data)
            self.proc.stdin.flush()
        except (BrokenPipeError, OSError) as e:
            raise RuntimeError(f"engine stdin closed: {e}")

    def read_until(self, predicate, timeout=30.0, idle_settle=0.25):
        """Read chunks until predicate(buf_str) is True, or timeout."""
        end_time = time.time() + timeout
        last_data = time.time()
        while True:
            try:
                ch = self._q.get(timeout=0.10)
                last_data = time.time()
            except queue.Empty:
                if (time.time() - last_data) >= idle_settle:
                    txt = strip_ansi("".join(self.buf))
                    if predicate(txt):
                        return txt
                if time.time() > end_time:
                    txt = strip_ansi("".join(self.buf))
                    return txt  # caller checks predicate themselves
                continue
            if ch is None:
                txt = strip_ansi("".join(self.buf))
                return txt
            if isinstance(ch, bytes):
                ch = ch.decode("utf-8", errors="replace")
            self.buf.append(ch)
            if time.time() > end_time:
                txt = strip_ansi("".join(self.buf))
                return txt

    def read_until_prompt(self, timeout=30.0):
        """Wait until output ends with '> ' (engine prompt) or game-over banner."""
        def pred(txt: str) -> bool:
            t = txt.rstrip()
            return (
                txt.endswith("> ")
                or "*** Game over" in txt
                or "Resign." in txt
                or "no legal" in txt.lower()
            )
        return self.read_until(pred, timeout=timeout)

    def transcript(self) -> str:
        return strip_ansi("".join(self.buf))

    def consume_transcript(self) -> str:
        """Return transcript so far AND clear buffer (so next call only sees new).

        Crucially, drain the reader-thread queue first — chars sitting in queue
        haven't reached self.buf yet, and would otherwise leak into the NEXT
        read_until() as stale data from the previous phase.
        """
        # Drain pending queue into buf so we capture everything up to "now".
        while True:
            try:
                ch = self._q.get_nowait()
            except queue.Empty:
                break
            if ch is None:
                # Re-post EOF so reader_loop's termination is preserved
                self._q.put(None)
                break
            if isinstance(ch, bytes):
                ch = ch.decode("utf-8", errors="replace")
            self.buf.append(ch)
        txt = strip_ansi("".join(self.buf))
        self.buf = []
        return txt

    def close(self):
        try:
            self.proc.stdin.close()
        except Exception:
            pass
        try:
            self.proc.wait(timeout=2)
        except Exception:
            self.kill()

    def kill(self):
        try:
            self.proc.kill()
        except Exception:
            pass
        try:
            self.proc.wait(timeout=2)
        except Exception:
            pass


# ---------------------------------------------------------------------------
# Engine output parsing — match the formats produced by src/ui.c.
# ---------------------------------------------------------------------------

AI_PLAYS_RE = re.compile(
    r">>> AI plays (\w+) = (\w+)\s+\(score=(-?\d+),\s+nodes=(-?\d+)\)"
)
CAND_LINE_RE = re.compile(r"^\s*(\d+)\)\s+([A-O]\d{1,2})\s+\(score=(-?\d+)\)", re.MULTILINE)
GAME_OVER_RE = re.compile(r"\*\*\* Game over:\s*([^*]+?)\s*\*\*\*")


def parse_ai_move(txt: str):
    """Return last (label, coord, score, nodes) parsed from buffer, or None."""
    matches = AI_PLAYS_RE.findall(txt)
    if not matches:
        return None
    lbl, coord, score, nodes = matches[-1]
    return (lbl, coord, int(score), int(nodes))


def parse_candidates(txt: str):
    """Return list of (index, coord, score) for the most-recent candidate block."""
    matches = CAND_LINE_RE.findall(txt)
    # Take the trailing contiguous block of candidates (most recent prompt block).
    # In practice each phase-4 prompt prints them all together, so just return them.
    return [(int(idx), coord, int(score)) for idx, coord, score in matches]


def parse_game_over(txt: str):
    """Return one of 'BLACK_WIN', 'WHITE_WIN', 'DRAW', or None."""
    m = GAME_OVER_RE.search(txt)
    if not m:
        return None
    s = m.group(1).strip()
    if "BLACK wins" in s:
        return "BLACK_WIN"
    if "WHITE wins" in s:
        return "WHITE_WIN"
    if "DRAW" in s:
        return "DRAW"
    return "UNKNOWN"


# ---------------------------------------------------------------------------
# NN training-data capture — mirror the engine board in Python so we can dump
# (position, search-score) pairs per ply. Stays stdlib-only; numpy is only
# touched in main() when --nn-out is set. Board convention matches
# nn/encoding.py / src/board.h: cells[r][c], r=0 top row, display=(chr('A'+c),
# 15-r), {EMPTY=0, BLACK=1, WHITE=2}.
# ---------------------------------------------------------------------------

BOARD_N = 15
EMPTY_INT, BLACK_INT, WHITE_INT = 0, 1, 2


def coord_to_rc(coord: str):
    """'H8' -> (r, c). Letter = column (A..O), number = display row (1..15)."""
    coord = coord.strip().upper()
    col = ord(coord[0]) - ord("A")
    row_disp = int(coord[1:])
    r = BOARD_N - row_disp
    c = col
    if not (0 <= r < BOARD_N and 0 <= c < BOARD_N):
        raise ValueError(f"coord out of range: {coord}")
    return r, c


class BoardTracker:
    """Replays the move sequence to snapshot positions for NN labels.

    Placement order/colors MUST match the national-rule sequence the harness
    relays (B1,W2,B3 opening; W4; B5 pick; then alternating W6,B7,...).
    """

    def __init__(self):
        self.cells = [EMPTY_INT] * (BOARD_N * BOARD_N)

    def place(self, coord: str, color: int):
        r, c = coord_to_rc(coord)
        idx = r * BOARD_N + c
        if self.cells[idx] != EMPTY_INT:
            raise ValueError(f"cell already occupied at {coord}")
        self.cells[idx] = color

    def snapshot(self):
        """Flat row-major copy (len 225); reshape(15,15) gives cells[r][c]."""
        return list(self.cells)


# ---------------------------------------------------------------------------
# Game driver — runs one full game between two engine instances.
# ---------------------------------------------------------------------------


class GameError(Exception):
    pass


def run_one_game(
    game_id: int,
    engine_black: str,
    engine_white: str,
    opening_idx: int,
    depth: int,
    time_ms: int,
    game_timeout: float = 120.0,
    n_strikes: int = 5,
    record_samples: bool = False,
):
    """Play one game; return dict with result + stats.

    engine_black plays BLACK (started with --black).
    engine_white plays WHITE (started with --white).
    Both share `opening_idx` from OPENINGS table; no swap (phase 2 = '1' both
    sides).

    If record_samples, also collect per-ply (position, side-to-move, score)
    triples in result["samples"]; outcome labels are assigned later in main()
    once the game result is known. Capture is best-effort and never aborts the
    game — on any tracker error we silently stop recording this game.
    """
    opening = OPENINGS[opening_idx]
    name, b1, w2, b3 = opening
    pcmd = [f"--depth={depth}", f"--time={time_ms}"]

    t_start = time.time()
    plies = 0
    result = "UNKNOWN"
    last_error = None
    opp_kept_white = "1"  # phase 2 input: 1 = no swap, 2 = swap. We never swap.
    # Phase-1 line: "B1 W2 B3 N"
    phase1_line = f"{b1} {w2} {b3} {n_strikes}"

    # --- NN sample capture (best-effort; disabled on first tracker error) ----
    samples = []  # list of (cells_flat[225], stm_int, score_int)
    _trk = {"t": BoardTracker() if record_samples else None}

    def place(coord, color):
        t = _trk["t"]
        if t is None:
            return
        try:
            t.place(coord, color)
        except Exception:
            _trk["t"] = None  # bad relay/coord -> stop recording, keep prior

    def rec(color, score):
        t = _trk["t"]
        if t is None:
            return
        try:
            samples.append((t.snapshot(), int(color), int(score)))
        except Exception:
            _trk["t"] = None

    # Opening stones are book moves (no search score), just put them on board.
    place(b1, BLACK_INT)
    place(w2, WHITE_INT)
    place(b3, BLACK_INT)

    eb = EngineProc(engine_black, ["--black"] + pcmd)
    ew = EngineProc(engine_white, ["--white"] + pcmd)

    def deadline_left() -> float:
        return max(0.5, game_timeout - (time.time() - t_start))

    try:
        # ---- Phase 1 ----
        eb.read_until_prompt(timeout=deadline_left())
        ew.read_until_prompt(timeout=deadline_left())
        eb.send(phase1_line)
        ew.send(phase1_line)

        # ---- Phase 2 (swap) ----
        # Both engines now print the phase 2 prompt; we never swap.
        eb.read_until_prompt(timeout=deadline_left())
        ew.read_until_prompt(timeout=deadline_left())
        # NOTE: protocol asks both "1" = keep white. Black side reads "opponent
        # kept WHITE", white side reads "you kept WHITE". Both => stays as is.
        eb.send(opp_kept_white)
        ew.send(opp_kept_white)
        plies = 3  # after phase 1 there are 3 stones on the board

        # ---- Phase 3 (W4) ----
        # active=WHITE: engine_white's AI computes W4 itself; engine_black waits
        # for opponent coord to be entered.
        # Drain white until next prompt (after it printed its AI move).
        eb.consume_transcript()  # clear so next parse only sees Phase-3 chunk
        ew.consume_transcript()
        txt_w = ew.read_until_prompt(timeout=deadline_left())
        wmove = parse_ai_move(txt_w)
        if wmove is None:
            # Maybe the game ended (rare in phase 3) — check
            go = parse_game_over(txt_w)
            if go:
                result = go
                raise _GameEnded()
            raise GameError(f"phase3: white engine did not report AI move; tail=\n{txt_w[-400:]}")
        _, w4_coord, w4_score, _ = wmove
        rec(WHITE_INT, w4_score)   # position before W4, white to move
        place(w4_coord, WHITE_INT)
        eb.send(w4_coord)
        plies = 4

        # After feeding W4 to black, black should be prompted for phase 4.
        # ---- Phase 4 (B5 = N strikes) ----
        # active=BLACK: engine_black offers N candidates; engine_white reads
        # them one per line then picks.
        eb.consume_transcript()
        txt_b = eb.read_until_prompt(timeout=deadline_left())
        cands = parse_candidates(txt_b)
        if not cands:
            go = parse_game_over(txt_b)
            if go:
                result = go
                raise _GameEnded()
            raise GameError(f"phase4: black engine produced no candidates; tail=\n{txt_b[-500:]}")
        # Use exactly min(N, len(cands)) candidates — feed each to white.
        cand_coords = [c[1] for c in cands][:n_strikes]
        # white expects N inputs (it was started with same N from phase 1 line).
        # If black produced fewer than N (rare bug), pad with extras? Spec is
        # strict. Trust black; if mismatch we'll detect via timeout.
        ew.consume_transcript()
        for coord in cand_coords:
            ew.send(coord)
        # After N coords, white's engine picks and prints which one. Wait for it.
        txt_w = ew.read_until_prompt(timeout=deadline_left())
        # White prints: ">>> Engine picks candidate <i> (= <coord>) for B5."
        pick_match = re.search(r"Engine picks candidate (\d+)\s+\(=\s*(\w+)\)", txt_w)
        if not pick_match:
            go = parse_game_over(txt_w)
            if go:
                result = go
                raise _GameEnded()
            raise GameError(f"phase4: white did not pick a candidate; tail=\n{txt_w[-400:]}")
        picked_index = int(pick_match.group(1))  # 1-based
        # Record the B5 position (black to move) with the picked candidate's
        # search score, then place it. cands aligns with cand_coords.
        if 1 <= picked_index <= len(cands):
            rec(BLACK_INT, cands[picked_index - 1][2])
            place(cands[picked_index - 1][1], BLACK_INT)
        else:
            _trk["t"] = None  # unexpected index -> stop recording safely
        # Tell black which index was picked.
        eb.send(str(picked_index))
        plies = 5

        # ---- Phase 5 loop ----
        # After phase 4, active side is WHITE (move 6 = W6).
        # IMPORTANT: white's engine printed "AI plays W6 = ..." immediately
        # after picking, BEFORE we sent the picked_index to black. That text
        # is already in `txt_w` from the phase-4 pick read. Parse it now and
        # treat W6 as already-played.
        ew_pending_move = parse_ai_move(txt_w)
        if ew_pending_move is None:
            raise GameError(
                f"phase5 entry: white did not auto-play W6; tail=\n{txt_w[-400:]}"
            )
        # we'll process this move at the top of the loop without rereading.

        # Track move counts so each iteration can identify "the next AI move"
        # by counting AI-plays matches in cumulative transcript.
        # White already produced W6 (its 1st AI move). Black has 0 AI moves so far.
        b_ai_count = 0
        w_ai_count = 1  # W6 already played

        # Process W6 -> send to black.
        _, w6_coord, w6_score, _ = ew_pending_move
        rec(WHITE_INT, w6_score)   # position before W6, white to move
        place(w6_coord, WHITE_INT)
        eb.send(w6_coord)

        side_to_move = "BLACK"  # black to compute B7
        max_plies = 250  # board is 225 cells; should never exceed
        while plies < max_plies:
            if time.time() - t_start > game_timeout:
                raise GameError("game wall-clock timeout")
            if side_to_move == "BLACK":
                # wait for black's next AI move (b_ai_count + 1 matches in transcript)
                txt_b = eb.read_until_prompt(timeout=deadline_left())
                go = parse_game_over(txt_b)
                all_moves = AI_PLAYS_RE.findall(txt_b)
                if len(all_moves) > b_ai_count:
                    lbl, coord, score_s, _ = all_moves[b_ai_count]
                    b_ai_count += 1
                    plies += 1
                    rec(BLACK_INT, score_s)   # position before this black move
                    place(coord, BLACK_INT)
                    if go:
                        result = go
                        raise _GameEnded()
                    ew.send(coord)
                    side_to_move = "WHITE"
                else:
                    if go:
                        result = go
                        raise _GameEnded()
                    raise GameError(
                        f"phase5 black: no new AI move (count={b_ai_count}); tail=\n{txt_b[-400:]}"
                    )
            else:  # WHITE
                txt_w = ew.read_until_prompt(timeout=deadline_left())
                go = parse_game_over(txt_w)
                all_moves = AI_PLAYS_RE.findall(txt_w)
                if len(all_moves) > w_ai_count:
                    lbl, coord, score_s, _ = all_moves[w_ai_count]
                    w_ai_count += 1
                    plies += 1
                    rec(WHITE_INT, score_s)   # position before this white move
                    place(coord, WHITE_INT)
                    if go:
                        result = go
                        raise _GameEnded()
                    eb.send(coord)
                    side_to_move = "BLACK"
                else:
                    if go:
                        result = go
                        raise _GameEnded()
                    raise GameError(
                        f"phase5 white: no new AI move (count={w_ai_count}); tail=\n{txt_w[-400:]}"
                    )
        if result == "UNKNOWN":
            result = "TIMEOUT_MAX_PLIES"
    except _GameEnded:
        pass
    except GameError as e:
        last_error = str(e)
        result = "ERROR"
    except Exception as e:
        last_error = f"{type(e).__name__}: {e}"
        result = "ERROR"
    finally:
        # try a final drain to capture closing banner
        try:
            eb.read_until_prompt(timeout=1.0)
        except Exception:
            pass
        try:
            ew.read_until_prompt(timeout=1.0)
        except Exception:
            pass
        # one more pass at game-over parsing
        if result in ("UNKNOWN", "ERROR"):
            combined = eb.transcript() + "\n" + ew.transcript()
            go = parse_game_over(combined)
            if go:
                result = go
        eb.close()
        ew.close()

    total_time = time.time() - t_start
    return {
        "game_id": game_id,
        "opening_idx": opening_idx,
        "opening_name": name,
        "engine_black": os.path.basename(engine_black),
        "engine_white": os.path.basename(engine_white),
        "depth": depth,
        "time_ms": time_ms,
        "n_strikes": n_strikes,
        "plies": plies,
        "result": result,
        "total_time_s": round(total_time, 3),
        "error": last_error,
        "samples": samples,  # [] unless record_samples; outcome added in main()
    }


class _GameEnded(Exception):
    """Sentinel raised internally when a game-over banner is detected."""
    pass


# ---------------------------------------------------------------------------
# ELO computation (only used in A/B comparison mode).
# ---------------------------------------------------------------------------


def compute_elo_delta(score: float):
    """delta_elo = -400 * log10(1/score - 1). Returns +/- inf at extremes."""
    if score <= 0.0:
        return float("-inf")
    if score >= 1.0:
        return float("inf")
    return -400.0 * math.log10(1.0 / score - 1.0)


def compute_elo_ci(wins: int, draws: int, losses: int):
    """Return (elo_delta, lo, hi) with a rough 95% CI via normal approximation
    on the win-percentage. Standard chess approach (LOS / EloDiff).
    """
    n = wins + draws + losses
    if n == 0:
        return (0.0, 0.0, 0.0)
    score = (wins + 0.5 * draws) / n
    # variance of single-game outcome around `score`
    var = (wins * (1.0 - score) ** 2 + draws * (0.5 - score) ** 2 +
           losses * (0.0 - score) ** 2) / n
    se = math.sqrt(var / n) if n > 0 else 0.0
    lo_score = max(1e-6, score - 1.96 * se)
    hi_score = min(1.0 - 1e-6, score + 1.96 * se)
    return (compute_elo_delta(score), compute_elo_delta(lo_score),
            compute_elo_delta(hi_score))


# ---------------------------------------------------------------------------
# Argparse + main.
# ---------------------------------------------------------------------------


def build_parser():
    p = argparse.ArgumentParser(
        description="Self-play tournament harness for gobang-engine.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("--engine", default="./gobang-engine.exe",
                   help="Engine binary (used for both sides in self-play mode).")
    p.add_argument("--engine-a", default=None,
                   help="Engine A (enables A/B comparison; pair with --engine-b).")
    p.add_argument("--engine-b", default=None,
                   help="Engine B (enables A/B comparison; pair with --engine-a).")
    p.add_argument("--games", type=int, default=100, help="Number of games.")
    p.add_argument("--depth", type=int, default=6, help="Engine --depth=.")
    p.add_argument("--time", dest="time_ms", type=int, default=1000,
                   help="Engine --time= in ms.")
    p.add_argument("--out", default="selfplay_results.json",
                   help="JSON results file.")
    p.add_argument("--seed", type=int, default=None,
                   help="RNG seed (default: random).")
    p.add_argument("--parallel", type=int, default=4,
                   help="Concurrent games (process pool).")
    p.add_argument("--game-timeout", type=float, default=120.0,
                   help="Wall-clock per game (seconds).")
    p.add_argument("--n-strikes", type=int, default=5,
                   help="Phase-4 N value (number of B5 candidates).")
    p.add_argument("--opening-mode", choices=["rotate", "random"], default="rotate",
                   help="Pick openings by rotation through 26 or random.")
    p.add_argument("--quiet", action="store_true",
                   help="Suppress per-game progress lines.")
    p.add_argument("--nn-out", default=None,
                   help="If set, capture per-ply (position, score, outcome) "
                        "triples and write them to this .npz for NN training "
                        "(e.g. nn/data/selfplay_v1.npz). Requires numpy.")
    return p


def schedule_games(args, rng):
    """Return a list of game-spec dicts to dispatch."""
    games = []
    n = args.games
    ab_mode = args.engine_a is not None and args.engine_b is not None
    for i in range(n):
        if args.opening_mode == "rotate":
            op_idx = i % len(OPENINGS)
        else:
            op_idx = rng.randrange(len(OPENINGS))
        if ab_mode:
            # Half the games: A=black, B=white. Other half swap.
            if i % 2 == 0:
                eb, ew = args.engine_a, args.engine_b
                tag = "A=B,B=W"
            else:
                eb, ew = args.engine_b, args.engine_a
                tag = "B=B,A=W"
        else:
            eb = ew = args.engine
            tag = "selfplay"
        games.append({
            "game_id": i,
            "engine_black": eb,
            "engine_white": ew,
            "opening_idx": op_idx,
            "tag": tag,
        })
    return games


def _worker(spec, depth, time_ms, game_timeout, n_strikes, record_samples=False):
    return run_one_game(
        game_id=spec["game_id"],
        engine_black=spec["engine_black"],
        engine_white=spec["engine_white"],
        opening_idx=spec["opening_idx"],
        depth=depth,
        time_ms=time_ms,
        game_timeout=game_timeout,
        n_strikes=n_strikes,
        record_samples=record_samples,
    )


def summarize(results, ab_mode, engine_a=None, engine_b=None):
    n = len(results)
    bw = sum(1 for r in results if r["result"] == "BLACK_WIN")
    ww = sum(1 for r in results if r["result"] == "WHITE_WIN")
    dr = sum(1 for r in results if r["result"] == "DRAW")
    err = sum(1 for r in results if r["result"] in ("ERROR", "UNKNOWN", "TIMEOUT_MAX_PLIES"))
    avg_plies = sum(r["plies"] for r in results) / n if n else 0
    avg_time = sum(r["total_time_s"] for r in results) / n if n else 0

    summary = {
        "games": n,
        "black_wins": bw,
        "white_wins": ww,
        "draws": dr,
        "errors": err,
        "avg_plies": round(avg_plies, 2),
        "avg_time_s": round(avg_time, 3),
    }

    if ab_mode and engine_a is not None and engine_b is not None:
        # Count wins for engine_a, regardless of color.
        a_w = a_d = a_l = 0
        for r in results:
            if r["result"] == "ERROR":
                continue
            a_played_black = (r["engine_black"] == os.path.basename(engine_a))
            if r["result"] == "DRAW":
                a_d += 1
            elif r["result"] == "BLACK_WIN":
                if a_played_black:
                    a_w += 1
                else:
                    a_l += 1
            elif r["result"] == "WHITE_WIN":
                if a_played_black:
                    a_l += 1
                else:
                    a_w += 1
        elo, lo, hi = compute_elo_ci(a_w, a_d, a_l)
        summary["ab"] = {
            "engine_a": os.path.basename(engine_a),
            "engine_b": os.path.basename(engine_b),
            "a_wins": a_w,
            "a_draws": a_d,
            "a_losses": a_l,
            "elo_delta_a_vs_b": None if not math.isfinite(elo) else round(elo, 1),
            "elo_ci95_lo": None if not math.isfinite(lo) else round(lo, 1),
            "elo_ci95_hi": None if not math.isfinite(hi) else round(hi, 1),
        }
    return summary


def dump_nn_dataset(results, path):
    """Aggregate per-ply samples into an .npz training set.

    Only decisive/drawn games contribute (outcome label must be well-defined).
    Arrays (N = total positions):
      cells   (N,15,15) int8   {0,1,2}, cells[r][c]
      stm     (N,)      int8    side-to-move at that position {1,2}
      score   (N,)      float32 raw engine search score (stm perspective)
      outcome (N,)      float32 game result from stm perspective {0,0.5,1}
      game_id (N,)      int32   source game (for leakage-free train/val split)

    Labels: 2.1 distillation target = sigmoid(score/scale) (computed in
    train.py so scale is tunable); `outcome` is for 2.3 combined loss. We store
    raw cells (not pre-encoded planes) so the 2.2 HalfLine encoder can reuse the
    same dataset without re-running self-play.
    """
    import numpy as np  # local import: only --nn-out path needs numpy

    decisive = {"BLACK_WIN", "WHITE_WIN", "DRAW"}
    cells_l, stm_l, score_l, out_l, gid_l = [], [], [], [], []
    n_games_used = 0
    for r in results:
        res = r.get("result")
        if res not in decisive:
            continue
        samples = r.get("samples") or []
        if not samples:
            continue
        n_games_used += 1
        for cells_flat, stm, score in samples:
            if res == "DRAW":
                oc = 0.5
            elif (res == "BLACK_WIN" and stm == BLACK_INT) or \
                 (res == "WHITE_WIN" and stm == WHITE_INT):
                oc = 1.0
            else:
                oc = 0.0
            cells_l.append(cells_flat)
            stm_l.append(stm)
            score_l.append(score)
            out_l.append(oc)
            gid_l.append(r["game_id"])

    n = len(cells_l)
    if n == 0:
        print("[nn] no samples captured (no decisive games?) — nothing written.")
        return 0

    cells = np.asarray(cells_l, dtype=np.int8).reshape(-1, BOARD_N, BOARD_N)
    stm = np.asarray(stm_l, dtype=np.int8)
    score = np.asarray(score_l, dtype=np.float32)
    outcome = np.asarray(out_l, dtype=np.float32)
    game_id = np.asarray(gid_l, dtype=np.int32)

    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    np.savez_compressed(path, cells=cells, stm=stm, score=score,
                        outcome=outcome, game_id=game_id)
    print(f"[nn] wrote {n} positions from {n_games_used} games -> {path}")
    return n


def main(argv=None):
    args = build_parser().parse_args(argv)

    seed = args.seed if args.seed is not None else random.SystemRandom().randint(0, 2**31 - 1)
    rng = random.Random(seed)

    ab_mode = args.engine_a is not None and args.engine_b is not None
    if ab_mode:
        for path in (args.engine_a, args.engine_b):
            if not os.path.exists(path):
                print(f"[error] engine not found: {path}", file=sys.stderr)
                return 2
    else:
        if not os.path.exists(args.engine):
            print(f"[error] engine not found: {args.engine}", file=sys.stderr)
            return 2

    specs = schedule_games(args, rng)

    print(f"[selfplay] games={args.games} parallel={args.parallel} "
          f"depth={args.depth} time={args.time_ms}ms "
          f"opening_mode={args.opening_mode} seed={seed} "
          f"mode={'AB' if ab_mode else 'selfplay'}")

    results = []
    t0 = time.time()
    with concurrent.futures.ProcessPoolExecutor(max_workers=args.parallel) as ex:
        futures = {
            ex.submit(_worker, spec, args.depth, args.time_ms,
                      args.game_timeout, args.n_strikes,
                      args.nn_out is not None): spec
            for spec in specs
        }
        for fut in concurrent.futures.as_completed(futures):
            try:
                r = fut.result()
            except Exception as e:
                spec = futures[fut]
                r = {
                    "game_id": spec["game_id"],
                    "opening_idx": spec["opening_idx"],
                    "opening_name": OPENINGS[spec["opening_idx"]][0],
                    "engine_black": os.path.basename(spec["engine_black"]),
                    "engine_white": os.path.basename(spec["engine_white"]),
                    "depth": args.depth,
                    "time_ms": args.time_ms,
                    "n_strikes": args.n_strikes,
                    "plies": 0,
                    "result": "ERROR",
                    "total_time_s": 0.0,
                    "error": f"{type(e).__name__}: {e}",
                }
            results.append(r)
            if not args.quiet:
                tag = ""
                if r.get("error"):
                    tag = f"  ERR={r['error'][:80]}"
                print(f"  [{len(results):>3}/{args.games}] "
                      f"opening={r['opening_name']:>9s} "
                      f"plies={r['plies']:>3} "
                      f"result={r['result']:<14s} "
                      f"t={r['total_time_s']:>6.2f}s{tag}")

    results.sort(key=lambda r: r["game_id"])

    wall = time.time() - t0
    engine_a = args.engine_a if ab_mode else None
    engine_b = args.engine_b if ab_mode else None
    summary = summarize(results, ab_mode, engine_a, engine_b)
    summary["wall_clock_s"] = round(wall, 2)
    summary["seed"] = seed
    summary["depth"] = args.depth
    summary["time_ms"] = args.time_ms
    summary["parallel"] = args.parallel

    # Keep raw position samples out of the JSON (they go to the .npz); they'd
    # bloat the file by orders of magnitude.
    games_json = [{k: v for k, v in r.items() if k != "samples"} for r in results]
    out = {
        "summary": summary,
        "games": games_json,
    }
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2, ensure_ascii=False)

    print()
    print("=" * 60)
    print("SUMMARY")
    print("=" * 60)
    print(f"  games:        {summary['games']}")
    print(f"  black wins:   {summary['black_wins']}")
    print(f"  white wins:   {summary['white_wins']}")
    print(f"  draws:        {summary['draws']}")
    print(f"  errors:       {summary['errors']}")
    print(f"  avg plies:    {summary['avg_plies']}")
    print(f"  avg time/g:   {summary['avg_time_s']} s")
    print(f"  wall clock:   {summary['wall_clock_s']} s")
    if "ab" in summary:
        ab = summary["ab"]
        print()
        print(f"  A/B comparison ({ab['engine_a']} vs {ab['engine_b']}):")
        print(f"    A wins/draws/losses: {ab['a_wins']}/{ab['a_draws']}/{ab['a_losses']}")
        print(f"    ELO delta (A - B):   {ab['elo_delta_a_vs_b']}  "
              f"[95% CI {ab['elo_ci95_lo']} .. {ab['elo_ci95_hi']}]")
    print()
    print(f"  results JSON: {args.out}")

    if args.nn_out:
        dump_nn_dataset(results, args.nn_out)
    return 0


if __name__ == "__main__":
    sys.exit(main())

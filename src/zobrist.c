/* src/zobrist.c */
#define _CRT_SECURE_NO_WARNINGS
#include <string.h>
#include "zobrist.h"

static uint64_t Z_KEY[2][BOARD_SIZE * BOARD_SIZE];
static uint64_t Z_SIDE_KEY;
static TTEntry  _tt[TT_SIZE];
static int      _zob_initialized = 0;

/* splitmix64 PRNG（fixed seed → 跨平台跨运行可重现）*/
static uint64_t splitmix64(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void zobrist_init(void) {
    /* Idempotent guard: if a caller (e.g. board_init) auto-invokes us
     * and the user has also called zobrist_init() explicitly from main,
     * re-running would re-fill Z_KEY (same values, same seed — fine) but
     * would also wipe TT (tt_clear), losing accumulated entries across
     * a new game's first board_init. Early-return preserves both. */
    if (_zob_initialized) return;

    uint64_t seed = 0x9E3779B97F4A7C15ULL;
    for (int color = 0; color < 2; color++) {
        for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
            Z_KEY[color][i] = splitmix64(&seed);
        }
    }
    Z_SIDE_KEY = splitmix64(&seed);
    tt_clear();
    _zob_initialized = 1;
}

uint64_t zobrist_xor_piece(uint64_t h, int row, int col, int color) {
    return h ^ Z_KEY[color - 1][row * BOARD_SIZE + col];
}

uint64_t zobrist_xor_side(uint64_t h) {
    return h ^ Z_SIDE_KEY;
}

uint64_t zobrist_compute(const Board *b) {
    uint64_t h = 0;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            uint8_t cell = b->cells[r][c];
            if (cell != EMPTY) {
                h = zobrist_xor_piece(h, r, c, cell);
            }
        }
    }
    /* 约定：side_to_move == WHITE 时 XOR side key（黑先所以默认 hash 表示"轮到黑"）*/
    if (b->side_to_move == WHITE) h = zobrist_xor_side(h);
    return h;
}

void tt_clear(void) {
    memset(_tt, 0, sizeof(_tt));
}

const TTEntry *tt_get(uint64_t key) {
    const TTEntry *e = &_tt[TT_INDEX(key)];
    return (e->flag != TT_FLAG_NONE && e->key == key) ? e : NULL;
}

void tt_put(uint64_t key, int depth, int score, TTFlag flag, int best_row, int best_col) {
    TTEntry *e = &_tt[TT_INDEX(key)];
    /* always-replace 策略（最简版）*/
    e->key = key;
    e->depth = (int16_t)depth;
    e->score = score;
    e->flag = (uint8_t)flag;
    e->best_row = (int8_t)best_row;
    e->best_col = (int8_t)best_col;
}

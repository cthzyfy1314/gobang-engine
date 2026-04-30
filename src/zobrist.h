/* src/zobrist.h — Zobrist 增量哈希 + 置换表
 * 对应 Spec § 3.1 / § 5.3
 */
#ifndef ZOBRIST_H_
#define ZOBRIST_H_

#include <stdint.h>
#include "board.h"

#define TT_SIZE_LOG2 20            /* 1M entries */
#define TT_SIZE      (1u << TT_SIZE_LOG2)
#define TT_INDEX(h)  ((uint32_t)((h) & (TT_SIZE - 1)))

typedef enum {
    TT_FLAG_NONE  = 0,
    TT_FLAG_EXACT = 1,
    TT_FLAG_LOWER = 2,    /* score 是下界（β 剪枝时）*/
    TT_FLAG_UPPER = 3     /* score 是上界（α 剪枝时）*/
} TTFlag;

typedef struct {
    uint64_t key;          /* 完整 64-bit hash 用于 collision 校验 */
    int      score;
    int16_t  depth;        /* 该 entry 来自的搜索深度 */
    uint8_t  flag;
    int8_t   best_row;     /* 最佳着法行（-1 表示无）*/
    int8_t   best_col;
} TTEntry;

/* 模块初始化：填随机表 + clear TT。程序启动时调用一次。 */
void zobrist_init(void);

/* 完整从棋盘重新算 hash（debugging / sanity check 用） */
uint64_t zobrist_compute(const Board *b);

/* 增量更新：落子时 XOR；撤子时再 XOR 一次回去（XOR 自反） */
uint64_t zobrist_xor_piece(uint64_t h, int row, int col, int color);
uint64_t zobrist_xor_side(uint64_t h);

/* 置换表 */
void tt_clear(void);
const TTEntry *tt_get(uint64_t key);
void tt_put(uint64_t key, int depth, int score, TTFlag flag, int best_row, int best_col);

#endif /* ZOBRIST_H_ */

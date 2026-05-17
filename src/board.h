/* src/board.h — 五子棋棋盘数据结构 + 操作接口
 * 对应 Spec § 4.1 board 模块 / § 5.1 Board 结构 / § 1.4 国规 9.1
 */
#ifndef BOARD_H_
#define BOARD_H_

#include <stdint.h>
#include <stdbool.h>

#define BOARD_SIZE 15
#define EMPTY 0
#define BLACK 1
#define WHITE 2

typedef struct {
    int8_t row;     /* 0-14 */
    int8_t col;     /* 0-14 */
    int8_t color;   /* BLACK or WHITE */
} Move;

typedef struct {
    uint8_t cells[BOARD_SIZE][BOARD_SIZE];   /* 0=EMPTY, 1=BLACK, 2=WHITE */
    uint16_t move_count;
    uint64_t zobrist_hash;                   /* Day 1 placeholder, 留 0 */
    int side_to_move;                        /* BLACK or WHITE */
    Move history[BOARD_SIZE * BOARD_SIZE];
    bool forbid_enabled;                     /* Day 1 placeholder, 默认 true */
} Board;

/* GameResult — 对应 Spec § 3.4 + § 1.4 国规 9.1 / 9.2 */
typedef enum {
    RESULT_NONE = 0,
    RESULT_BLACK_WIN = 1,                    /* 黑五连，包括"黑五连+禁手同时"特例 */
    RESULT_WHITE_WIN_NORMAL = 2,             /* 白五连或白长连 */
    RESULT_WHITE_WIN_BY_BLACK_FORBID = 3,    /* 黑禁手判负，Day 1 暂不触发 */
    RESULT_DRAW = 4                          /* 全盘满 */
} GameResult;

/* 初始化空棋盘，黑先 */
void board_init(Board *b);

/* 在 (row,col) 落 color。成功返回 true；位置非法或已占用返回 false。 */
bool board_place(Board *b, int row, int col, int color);

/* 撤销最近一步。栈空返回 false。 */
bool board_undo(Board *b);

/* 检查胜负。
 * 国规不对称（Spec § 1.4 国规 9.1 + 9.2-c）：
 *   白方 ≥5 连 → RESULT_WHITE_WIN_NORMAL（白长连视同五连胜）
 *   黑方 存在某方向恰好 5 连 → RESULT_BLACK_WIN
 *       （即使另一方向同时形成长连，9.2-c：五连优先）
 *   黑方 ≥6 连且无任何方向恰好 5 连 → RESULT_WHITE_WIN_NORMAL（长连禁手）
 *   全盘满 → RESULT_DRAW
 *   其他 → RESULT_NONE
 */
GameResult board_check_winner(const Board *b);

/* 棋盘是否全部下满 */
bool board_is_full(const Board *b);

/* 坐标越界检查 */
bool board_in_bounds(int row, int col);

#endif /* BOARD_H_ */

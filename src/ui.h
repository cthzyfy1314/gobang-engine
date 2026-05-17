/* src/ui.h — 交互式对战 UI（人肉协议）*/
#ifndef UI_H_
#define UI_H_

#include "board.h"

typedef enum {
    UI_CMD_NONE = 0,
    UI_CMD_QUIT,
    UI_CMD_UNDO,
    UI_CMD_HINT,
    UI_CMD_RESIGN,
    UI_CMD_INVALID
} UICommand;

/* 启动 UI 主循环。返回时游戏已结束或用户退出。
 * my_color: 我方执子颜色（BLACK 或 WHITE）。
 * search_depth: αβ 最大搜索深度。
 * time_budget_ms: 每步思考预算(ms)；<=0 不限时。
 */
void ui_main_loop(int my_color, int search_depth, int time_budget_ms);

/* 绘制当前棋盘到 stdout。 */
void ui_draw_board(const Board *b);

/* 解析用户输入的一行。
 * 返回 true → 已解析为坐标，*row/*col 填入；*cmd = UI_CMD_NONE。
 * 返回 false → 是命令或非法输入；*cmd 填命令枚举（UI_CMD_QUIT/UNDO/HINT/RESIGN/INVALID）。
 */
bool ui_parse_input(const char *line, int *row, int *col, UICommand *cmd);

#endif /* UI_H_ */

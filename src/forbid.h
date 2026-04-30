/* src/forbid.h — Renju 黑方禁手判定
 * 对应 Spec § 1.4 国规 8/9.2 / § 3.4 / § 4.1 forbid 模块
 *
 * 三种禁手（仅黑方）：
 *   长连禁手（≥6 连）/ 四四禁手（≥2 个冲四以上）/ 三三禁手（≥2 个真活三）
 *
 * 边界规则（国规 9.2-c）：
 *   黑五连 + 禁手同时形成 → 五连优先（禁手失效，黑胜）
 */
#ifndef FORBID_H_
#define FORBID_H_

#include <stdbool.h>
#include "board.h"

typedef enum {
    FORBID_NONE         = 0,
    FORBID_DOUBLE_THREE = 1,
    FORBID_DOUBLE_FOUR  = 2,
    FORBID_OVERLINE     = 3
} ForbidType;

/* 判断在 (row, col) 落黑子是否构成禁手。
 * 不修改 board。
 * (row, col) 必须当前为 EMPTY 且在 bounds 内（否则返回 FORBID_NONE）。
 * 国规 9.2-c：若同时形成五连，返回 FORBID_NONE（五连优先）。
 */
ForbidType forbid_check_black(const Board *b, int row, int col);

#endif /* FORBID_H_ */

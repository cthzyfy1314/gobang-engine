/* src/opening.h — 开局库（极简版）
 * 国规规定 26 种指定开局 + 三手交换 + 五手 N 打。
 * 当前实现仅做最低必要：黑 1 天元 + 三手交换默认不换 + 五手 N 打存根。
 * 如果校赛裁判严格要求 26 局表，5/9 调参时再补。
 */
#ifndef OPENING_H_
#define OPENING_H_

#include "board.h"

/* 黑方第一手位置 — 国规要求天元 (7,7) */
Move opening_first_move(void);

/* 三手交换决策（白方第 4 手前判断是否换边）。
 * 当前简化：永远返回 false（不交换）。
 */
bool opening_should_swap(const Board *b);

#endif /* OPENING_H_ */

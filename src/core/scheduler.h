/**
 * モジュールスケジューラ
 *
 * g_modules を走査して init / update を呼ぶ。センサー名は一切知らない。
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief 全モジュールの init() を呼ぶ。失敗したモジュールは以後呼ばない。
 */
void scheduler_init(void);

/**
 * @brief 周期が来たモジュールの update() を呼ぶ。
 *
 * @param now 現在時刻 [ms]
 */
void scheduler_run(uint32_t now);

/**
 * @brief モジュールが初期化に成功し動作中か。
 *
 * @param index g_modules のインデックス
 */
bool scheduler_is_active(size_t index);

/**
 * @brief 次にいずれかのモジュールの update() が必要になるまでの時間。
 *
 * @param now 現在時刻 [ms]
 * @param max_ms 上限 [ms]
 */
uint32_t scheduler_ms_until_next(uint32_t now, uint32_t max_ms);

/**
 * @brief light sleep できない状態のモジュールがあれば true。
 */
bool scheduler_busy(void);

/**
 * @brief すぐ送信すべき変化を持つモジュールがあれば true（各モジュールのフラグは下ろす）。
 */
bool scheduler_take_events(void);

/**
 * 電源（電池残量）管理
 */
#pragma once

#include <stdint.h>

/**
 * @brief 電池残量の初回取得を行う。
 */
void power_init(void);

/**
 * @brief POWER_CHECK_PERIOD_MS ごとに電池残量を更新する。
 *
 * @param now 現在時刻 [ms]
 */
void power_update(uint32_t now);

/**
 * @brief 最後に取得した電池残量を返す。
 *
 * @return 0〜100 [%]。取得できない場合は負の値。
 */
int power_battery_level(void);

/**
 * @brief 次の処理まで待つ。allow_sleep なら light sleep（POWER_LIGHT_SLEEP_ENABLE 時）、
 *        そうでなければ delay()（CPU はアイドル）。
 *
 * @param ms 待ち時間 [ms]
 * @param allow_sleep light sleep してよいか（通信・PWM・画面が動いていないこと）
 */
void power_wait(uint32_t ms, bool allow_sleep);

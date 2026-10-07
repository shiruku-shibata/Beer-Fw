/**
 * 振動モジュール（M5StickC Vibration Hat / U159）の操作 API
 *
 * 出力系モジュールは他から呼ばれるため、例外的にヘッダを公開する。
 * MODULE_VIBRATION_ENABLE = 0 のときは何もしない inline 関数になるので、
 * 呼び出し側で #if を書く必要はない。
 */
#pragma once

#include <stdint.h>
#include "config.h"

#if MODULE_VIBRATION_ENABLE

/**
 * @brief 振動を開始する。動作中に呼ぶと強さと時間を上書きする。
 *
 * @param strength    強さ 0〜100 [%]。0 は停止
 * @param duration_ms 振動時間 [ms]。0 は vibration_stop() まで継続
 */
void vibration_start(uint8_t strength, uint32_t duration_ms);

/**
 * @brief 振動を止める。
 */
void vibration_stop(void);

/**
 * @brief 振動中か。
 */
bool vibration_is_active(void);

/**
 * @brief 警報振動（100 % 連続）の ON/OFF。ON の間は vibration_start / stop を無視する。
 */
void vibration_set_alarm(bool on);

#else

static inline void vibration_start(uint8_t, uint32_t) {}
static inline void vibration_stop(void) {}
static inline bool vibration_is_active(void) { return false; }
static inline void vibration_set_alarm(bool) {}

#endif

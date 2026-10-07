/**
 * 加速度モジュールの参照 API
 *
 * 加速度を他モジュール（drink など）から使うためのヘッダ。
 */
#pragma once

#include <stdbool.h>

/**
 * @brief 最後に取得した加速度を返す。
 *
 * @param x, y, z 出力先 [g]
 * @return 一度でも取得できていれば true
 */
bool accel_get(float *x, float *y, float *z);

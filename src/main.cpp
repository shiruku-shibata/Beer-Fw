/**
 * エントリポイント
 * Device : M5StickC Plus2
 * Board  : m5stick-c (PlatformIO)
 *
 * 起動順と呼び出しだけを持つ。役割ごとの処理は app/、
 * 共通処理は core/、センサー処理は modules/ に置く。
 */

#include <Arduino.h>
#include "config.h"
#include "core/system.h"
#include "app/app.h"

/**
 * @brief 起動時に 1 回呼ばれる。共通部分→役割別（node / gateway）の順に初期化する。
 */
void setup()
{
    system_init();
    app_init();
}

/**
 * @brief 起動後に繰り返し呼ばれる。共通の周期処理と役割別の処理を 1 回ずつ実行する。
 */
void loop()
{
    system_update();
    app_loop();
}

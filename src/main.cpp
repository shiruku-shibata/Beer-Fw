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

void setup()
{
    system_init();
    app_init();
}

void loop()
{
    system_update();
    app_loop();
}

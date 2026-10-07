#include "core/power.h"

#include <M5Unified.h>
#include <esp_sleep.h>
#include "config.h"
#include "core/log.h"

static int      s_level      = -1;
static uint32_t s_lastCheck  = 0;
static bool     s_lowWarned  = false;

void power_init(void)
{
    s_level     = M5.Power.getBatteryLevel();
    s_lastCheck = millis();
    LOG_I("Battery: %d%%", s_level);
}

void power_update(uint32_t now)
{
    if (now - s_lastCheck < POWER_CHECK_PERIOD_MS) return;
    s_lastCheck = now;

    s_level = M5.Power.getBatteryLevel();

    bool low = (s_level >= 0 && s_level <= POWER_LOW_PERCENT);
    if (low && !s_lowWarned) {
        LOG_W("Battery low: %d%%", s_level);
    }
    s_lowWarned = low;
}

int power_battery_level(void)
{
    return s_level;
}

void power_wait(uint32_t ms, bool allow_sleep)
{
    if (ms == 0) return;

#if POWER_LIGHT_SLEEP_ENABLE
    if (allow_sleep && ms >= 2) {
        Serial.flush();   // 送信途中のログを出し切ってから眠る
        esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000);
        esp_light_sleep_start();
        return;
    }
#else
    (void)allow_sleep;
#endif

    delay(ms);
}

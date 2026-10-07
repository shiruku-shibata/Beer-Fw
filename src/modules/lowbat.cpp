/**
 * 電池残量監視モジュール
 *
 * - LOWBAT_LOW_PERCENT 以下: 「残量低下」フラグを立て、すぐ gateway へ送る
 * - LOWBAT_ALARM_PERCENT 以下: 電源が落ちるまで振動し続ける（警報）
 * - いずれも LOWBAT_HYSTERESIS 分戻ったら解除する（充電時など）
 * 警報振動は vibration_set_alarm() で出すため、飲酒検出の振動より優先される。
 *
 * 【依存】core/power（残量）、vibration（振動）
 * 【送信データ】LowbatPayload 2 バイト
 * 【JSON】"battery":{"level":18,"low":1,"alarm":0}
 */

#include "config.h"
#if MODULE_LOWBAT_ENABLE

#include <stdio.h>
#include <string.h>
#include "core/log.h"
#include "core/power.h"
#include "core/protocol.h"
#include "modules/module.h"
#include "modules/vibration.h"

#define LOWBAT_FLAG_LOW    0x01
#define LOWBAT_FLAG_ALARM  0x02

typedef struct __attribute__((packed)) {
    int8_t  level;   // 電池残量 [%]（取得できなければ負）
    uint8_t flags;   // LOWBAT_FLAG_*
} LowbatPayload;

static bool s_low   = false;
static bool s_alarm = false;
static bool s_event = false;   // フラグが変わった（すぐ送る）

/**
 * @brief しきい値の判定（ヒステリシス付き）。
 *
 * @param on    現在の状態
 * @param level 電池残量 [%]
 * @param th    ON にするしきい値 [%]
 * @return 新しい状態
 */
static bool judge(bool on, int level, int th)
{
    if (!on && level <= th) return true;
    if (on && level >= th + LOWBAT_HYSTERESIS) return false;
    return on;
}

static void lowbat_update(void)
{
    int level = power_battery_level();
    if (level < 0) return;   // 取得できないときは判定しない

    bool low = judge(s_low, level, LOWBAT_LOW_PERCENT);
    if (low != s_low) {
        s_low   = low;
        s_event = true;
        if (low) LOG_W("Battery low (%d%%): flag ON", level);
        else     LOG_I("Battery recovered (%d%%): low flag OFF", level);
    }

    bool alarm = judge(s_alarm, level, LOWBAT_ALARM_PERCENT);
    if (alarm != s_alarm) {
        s_alarm = alarm;
        s_event = true;
        if (alarm) LOG_W("Battery empty (%d%%): alarm vibration ON", level);
        else       LOG_I("Battery recovered (%d%%): alarm OFF", level);
        vibration_set_alarm(alarm);
    }
}

static bool lowbat_take_event(void)
{
    bool e = s_event;
    s_event = false;
    return e;
}

static size_t lowbat_encode(uint8_t *buf, size_t cap)
{
    if (cap < sizeof(LowbatPayload)) return 0;
    LowbatPayload p;
    p.level = (int8_t)power_battery_level();
    p.flags = (s_low ? LOWBAT_FLAG_LOW : 0) | (s_alarm ? LOWBAT_FLAG_ALARM : 0);
    memcpy(buf, &p, sizeof(p));
    return sizeof(p);
}

static int lowbat_to_json(const uint8_t *data, size_t len, char *out, size_t cap)
{
    if (len != sizeof(LowbatPayload)) return 0;
    LowbatPayload p;
    memcpy(&p, data, sizeof(p));
    return snprintf(out, cap, "\"battery\":{\"level\":%d,\"low\":%u,\"alarm\":%u}",
                    p.level, (p.flags & LOWBAT_FLAG_LOW) ? 1 : 0, (p.flags & LOWBAT_FLAG_ALARM) ? 1 : 0);
}

static int lowbat_to_text(const uint8_t *data, size_t len, char *out, size_t cap)
{
    if (len != sizeof(LowbatPayload)) return 0;
    LowbatPayload p;
    memcpy(&p, data, sizeof(p));
    if (p.flags & LOWBAT_FLAG_ALARM) return snprintf(out, cap, "BATTERY EMPTY\n");
    if (p.flags & LOWBAT_FLAG_LOW)   return snprintf(out, cap, "LOW BAT %d%%\n", p.level);
    return 0;   // 平常時は画面に出さない（残量はフッタに表示済み）
}

extern const Module module_lowbat;
const Module module_lowbat = {
    "lowbat",
    MODULE_ID_LOWBAT,
    LOWBAT_PERIOD_MS,
    nullptr,
    lowbat_update,
    lowbat_encode,
    lowbat_to_json,
    lowbat_to_text,
    nullptr,
    lowbat_take_event,
};

#endif  // MODULE_LOWBAT_ENABLE

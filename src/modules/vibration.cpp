/**
 * 振動モジュール（M5StickC Vibration Hat / U159）
 *
 * 【ハード】Hat の振動モーターを G26 の PWM（10 kHz / 8 bit）で駆動する
 * 【操作】modules/vibration.h の vibration_start() / vibration_stop()
 * 【送信データ】現在の強さ 1 バイト（0〜100 %）
 * 【JSON】"vibration":50
 */

#include "config.h"
#if MODULE_VIBRATION_ENABLE

#include <Arduino.h>
#include <stdio.h>
#include "core/log.h"
#include "core/protocol.h"
#include "modules/module.h"
#include "modules/vibration.h"

static constexpr uint32_t DUTY_MAX = (1u << VIBRATION_PWM_BITS) - 1;

static bool     s_ready      = false;
static bool     s_active     = false;
static uint8_t  s_strength   = 0;   // 0〜100 [%]
static uint32_t s_startMs    = 0;
static uint32_t s_durationMs = 0;   // 0 = 継続
static bool     s_alarm      = false;   // 警報中は通常の操作を受け付けない

/**
 * @brief 強さ [%] を PWM デューティに変換して出力する。
 */
static void applyStrength(uint8_t strength)
{
    ledcWrite(VIBRATION_LEDC_CHANNEL, (uint32_t)strength * DUTY_MAX / 100);
}

// =============================================
// 公開 API（modules/vibration.h）
// =============================================
void vibration_start(uint8_t strength, uint32_t duration_ms)
{
    if (!s_ready || s_alarm) return;
    if (strength == 0) {
        vibration_stop();
        return;
    }
    if (strength > 100) strength = 100;

    s_strength   = strength;
    s_startMs    = millis();
    s_durationMs = duration_ms;
    s_active     = true;
    applyStrength(strength);
    LOG_D("Vibration start: %u%% %lu ms", strength, (unsigned long)duration_ms);
}

void vibration_stop(void)
{
    if (!s_ready || s_alarm) return;
    applyStrength(0);
    s_active   = false;
    s_strength = 0;
}

bool vibration_is_active(void)
{
    return s_active;
}

void vibration_set_alarm(bool on)
{
    if (!s_ready || on == s_alarm) return;

    s_alarm      = on;
    s_active     = on;
    s_strength   = on ? 100 : 0;
    s_durationMs = 0;
    applyStrength(s_strength);
    LOG_I("Vibration alarm %s", on ? "ON" : "OFF");
}

// =============================================
// Module インターフェース
// =============================================
static bool vibration_init(void)
{
    if (ledcSetup(VIBRATION_LEDC_CHANNEL, VIBRATION_PWM_FREQ, VIBRATION_PWM_BITS) == 0) {
        LOG_E("Vibration: LEDC setup failed");
        return false;
    }
    ledcAttachPin(VIBRATION_PIN, VIBRATION_LEDC_CHANNEL);
    applyStrength(0);
    s_ready = true;

#if VIBRATION_BOOT_TEST_MS > 0
    vibration_start(50, VIBRATION_BOOT_TEST_MS);   // 起動時の動作確認
#endif
    return true;
}

/**
 * @brief 指定時間が過ぎたら止める。
 */
static void vibration_update(void)
{
    if (s_active && s_durationMs > 0 && millis() - s_startMs >= s_durationMs) {
        vibration_stop();
    }
}

static size_t vibration_encode(uint8_t *buf, size_t cap)
{
    if (cap < 1) return 0;
    buf[0] = s_strength;
    return 1;
}

static int vibration_to_json(const uint8_t *data, size_t len, char *out, size_t cap)
{
    if (len != 1) return 0;
    return snprintf(out, cap, "\"vibration\":%u", data[0]);
}

static int vibration_to_text(const uint8_t *data, size_t len, char *out, size_t cap)
{
    if (len != 1) return 0;
    return snprintf(out, cap, "VIB:%3u%%\n", data[0]);
}

extern const Module module_vibration;
const Module module_vibration = {
    "vibration",
    MODULE_ID_VIBRATION,
    VIBRATION_PERIOD_MS,
    vibration_init,
    vibration_update,
    vibration_encode,
    vibration_to_json,
    vibration_to_text,
    vibration_is_active,   // PWM（LEDC）は light sleep 中に止まるため
};

#endif  // MODULE_VIBRATION_ENABLE

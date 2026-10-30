/**
 * 飲酒検出モジュール（ビールジョッキ用）
 *
 * ジョッキに入れた M5Stick の傾きから「飲んでいる」状態を検出し、
 * 傾けている間ゴクゴクと飲んでいるような振動を出す。
 *
 * 【操作】ボタン A
 * - 起動直後は停止中。A を押して離すと、その時の静止姿勢を「直立」の基準にして検出を始める。
 *   ジョッキを机に置いた状態で押すこと。
 * - 検出中に A を短く押すと基準を取り直す。DRINK_STOP_HOLD_MS 以上長押しすると停止。
 *
 * 【検出】
 * - 基準からの傾きが DRINK_TILT_START_DEG を DRINK_START_HOLD_MS 続いたら飲み始め、
 *   DRINK_TILT_END_DEG を下回ったら飲み終わり（ヒステリシス）。
 * - 傾きの方向は問わない（M5Stick の入れ向きに依存しないため）。
 *
 * 【振動】DRINK_VIB_CONTINUOUS = 1（既定）は飲んでいる間ずっと振動する。
 *        0 は 1 回の「ゴクッ」= GULP_PATTERN のパルス列を繰り返す。傾きが深いほど間隔が短くなる。
 *        ゴクッ回数はどちらでも同じ間隔で数える。
 *
 * 【依存】accel（傾き）、vibration（振動）
 * 【送信データ】DrinkPayload 4 バイト
 * 【JSON】"drink":{"armed":1,"active":1,"tilt":52,"gulps":12}
 */

#include "config.h"
#if MODULE_DRINK_ENABLE

#include <M5Unified.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "core/log.h"
#include "core/protocol.h"
#include "modules/accel.h"
#include "modules/module.h"
#include "modules/vibration.h"

typedef struct __attribute__((packed)) {
    uint8_t  flags;      // bit0 = 飲んでいる、bit1 = 検出中（A ボタンで開始済み）
    uint8_t  tilt_deg;   // 基準からの傾き [deg]
    uint16_t gulps;      // 起動からの累計ゴクッ回数
} DrinkPayload;

// =============================================
// ゴクッ 1 回分の振動パターン（ゴ・ク の 2 打）
// =============================================
typedef struct {
    uint8_t  strength;   // [%]
    uint16_t on_ms;
    uint16_t off_ms;     // 次のパルスまでの休み
} Pulse;

static const Pulse GULP_PATTERN[] = {
    {DRINK_GULP_STRENGTH, DRINK_GULP_ON1_MS, DRINK_GULP_GAP_MS},   // ゴ
    {DRINK_GULP_STRENGTH, DRINK_GULP_ON2_MS, 0},                   // ク
};
static constexpr size_t GULP_PULSES = sizeof(GULP_PATTERN) / sizeof(GULP_PATTERN[0]);
static constexpr uint32_t GULP_CYCLE_MS = DRINK_GULP_ON1_MS + DRINK_GULP_GAP_MS + DRINK_GULP_ON2_MS;   // 間隔を除く 1 回分

// =============================================
// 状態
// =============================================
typedef enum { ST_STOPPED, ST_CALIBRATING, ST_IDLE, ST_TILTING, ST_DRINKING } DrinkState;

static DrinkState s_state = ST_STOPPED;

// 姿勢
static float    s_ref[3]       = {0, 0, 1};   // 直立時の重力方向（単位ベクトル）
static float    s_filt[3]      = {0, 0, 0};   // ローパス後の加速度
static float    s_calibSum[3]  = {0, 0, 0};
static uint16_t s_calibCount   = 0;
static float    s_tiltDeg      = 0;

// 遷移・振動タイミング
static uint32_t s_tiltSinceMs  = 0;
static uint32_t s_nextPulseMs  = 0;
static size_t   s_pulseIndex   = 0;
static uint16_t s_gulps        = 0;
static bool     s_prevBtn      = false;
static uint32_t s_btnDownMs    = 0;
static bool     s_event        = false;   // 状態が変わった（すぐ送る）

/**
 * @brief 直立基準の取り直しを始める。
 */
static void startCalibration(void)
{
    s_state      = ST_CALIBRATING;
    s_calibCount = 0;
    memset(s_calibSum, 0, sizeof(s_calibSum));
    vibration_stop();
    LOG_I("Drink: calibrating... keep the mug upright and still");
}

/**
 * @brief 検出を止める（ボタン A 長押し）。
 */
static void stopDetection(void)
{
    s_state = ST_STOPPED;
    s_event = true;
    vibration_stop();
    LOG_I("Drink: stopped (press A to start)");
}

/**
 * @brief ボタン A を処理する。離した時点で判定する（押している間は姿勢が乱れるため）。
 */
static void handleButton(uint32_t now)
{
    bool btn = M5.BtnA.isPressed();
    if (btn && !s_prevBtn) s_btnDownMs = now;
    if (!btn && s_prevBtn) {
        if (now - s_btnDownMs >= DRINK_STOP_HOLD_MS) {
            stopDetection();
        } else {
            startCalibration();   // 停止中なら開始、検出中なら基準の取り直し
        }
    }
    s_prevBtn = btn;
}

/**
 * @brief 加速度ベクトルと基準のなす角 [deg] を求める。
 */
static float angleFromRef(const float *a)
{
    float norm = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    if (norm < 0.1f) return s_tiltDeg;   // 自由落下に近いときは前回値を保つ

    float c = (a[0] * s_ref[0] + a[1] * s_ref[1] + a[2] * s_ref[2]) / norm;
    if (c > 1.0f)  c = 1.0f;
    if (c < -1.0f) c = -1.0f;
    return acosf(c) * 180.0f / (float)M_PI;
}

/**
 * @brief 傾きに応じたゴクッの間隔 [ms]。深く傾けるほど短い。
 */
static uint32_t gulpInterval(float tilt)
{
    float r = (tilt - DRINK_TILT_START_DEG) / (90.0f - DRINK_TILT_START_DEG);
    if (r < 0) r = 0;
    if (r > 1) r = 1;
    return DRINK_GULP_INTERVAL_SLOW_MS
         - (uint32_t)(r * (DRINK_GULP_INTERVAL_SLOW_MS - DRINK_GULP_INTERVAL_FAST_MS));
}

/**
 * @brief 飲んでいる間の振動パターンを進める。
 */
static void runGulp(uint32_t now)
{
    if ((int32_t)(now - s_nextPulseMs) < 0) return;

    const Pulse &p = GULP_PATTERN[s_pulseIndex];
    vibration_start(p.strength, p.on_ms);

    if (s_pulseIndex == 0) s_gulps++;

    s_pulseIndex++;
    if (s_pulseIndex < GULP_PULSES) {
        s_nextPulseMs = now + p.on_ms + p.off_ms;
    } else {
        s_pulseIndex  = 0;
        s_nextPulseMs = now + p.on_ms + gulpInterval(s_tiltDeg);
    }
}

/**
 * @brief 飲んでいる間の連続振動を保ち、ゴクッ回数をパターン時と同じ間隔で数える。
 */
static void runContinuous(uint32_t now)
{
    if (!vibration_is_active()) vibration_start(DRINK_CONTINUOUS_STRENGTH, 0);   // 電池警報が明けたら再開する

    if ((int32_t)(now - s_nextPulseMs) < 0) return;
    s_gulps++;
    s_nextPulseMs = now + GULP_CYCLE_MS + gulpInterval(s_tiltDeg);
}

// =============================================
// Module インターフェース
// =============================================
/**
 * @brief [Module.init] 停止状態（ボタン A 待ち）で始める。
 */
static bool drink_init(void)
{
    s_state = ST_STOPPED;
    LOG_I("Drink: press A to start");
    return true;
}

/**
 * @brief [Module.update] ボタン A を処理し、加速度をローパスして傾きを求め、
 *        状態（基準取得→待機→傾き中→飲んでいる）を進める。飲んでいる間はゴクッの振動を出す。
 */
static void drink_update(void)
{
    uint32_t now = millis();

    handleButton(now);
    if (s_state == ST_STOPPED) return;

    float a[3];
    if (!accel_get(&a[0], &a[1], &a[2])) return;

    // ローパスフィルタ（手ぶれ・液面の揺れを抑える）
    for (int i = 0; i < 3; i++) {
        s_filt[i] += DRINK_FILTER_ALPHA * (a[i] - s_filt[i]);
    }

    // ---- 直立基準の取得 ----
    if (s_state == ST_CALIBRATING) {
        for (int i = 0; i < 3; i++) s_calibSum[i] += a[i];
        if (++s_calibCount < DRINK_CALIB_SAMPLES) return;

        float n = sqrtf(s_calibSum[0] * s_calibSum[0] + s_calibSum[1] * s_calibSum[1]
                        + s_calibSum[2] * s_calibSum[2]);
        if (n < 0.1f) {
            startCalibration();
            return;
        }
        for (int i = 0; i < 3; i++) {
            s_ref[i]  = s_calibSum[i] / n;
            s_filt[i] = s_calibSum[i] / s_calibCount;
        }
        s_state = ST_IDLE;
        s_event = true;
        LOG_I("Drink: upright = (%.2f, %.2f, %.2f)", s_ref[0], s_ref[1], s_ref[2]);
        return;
    }

    s_tiltDeg = angleFromRef(s_filt);

    // ---- 状態遷移 ----
    switch (s_state) {
    case ST_IDLE:
        if (s_tiltDeg >= DRINK_TILT_START_DEG) {
            s_state       = ST_TILTING;
            s_tiltSinceMs = now;
        }
        break;

    case ST_TILTING:
        if (s_tiltDeg < DRINK_TILT_END_DEG) {
            s_state = ST_IDLE;
        } else if (now - s_tiltSinceMs >= DRINK_START_HOLD_MS) {
            s_state       = ST_DRINKING;
            s_event       = true;
            s_pulseIndex  = 0;
            s_nextPulseMs = now;
            LOG_I("Drink: start (tilt %.0f deg)", s_tiltDeg);
        }
        break;

    case ST_DRINKING:
        if (s_tiltDeg < DRINK_TILT_END_DEG) {
            s_state = ST_IDLE;
            s_event = true;
            vibration_stop();
            LOG_I("Drink: stop (gulps %u)", s_gulps);
        } else {
#if DRINK_VIB_CONTINUOUS
            runContinuous(now);
#else
            runGulp(now);
#endif
        }
        break;

    default:
        break;
    }
}

/**
 * @brief [Module.take_event] 状態が変わっていれば true を返し、フラグを下ろす。
 */
static bool drink_take_event(void)
{
    bool e = s_event;
    s_event = false;
    return e;
}

/**
 * @brief [Module.encode] フラグ（飲んでいる / 検出中）・傾き・累計ゴクッ回数を 4 バイトにする。
 */
static size_t drink_encode(uint8_t *buf, size_t cap)
{
    if (cap < sizeof(DrinkPayload)) return 0;
    DrinkPayload p;
    p.flags    = (s_state == ST_DRINKING ? 0x01 : 0) | (s_state != ST_STOPPED ? 0x02 : 0);
    p.tilt_deg = (uint8_t)(s_tiltDeg + 0.5f);
    p.gulps    = s_gulps;
    memcpy(buf, &p, sizeof(p));
    return sizeof(p);
}

/**
 * @brief [Module.to_json] 受信データを "drink":{"armed","active","tilt","gulps"} にする。
 */
static int drink_to_json(const uint8_t *data, size_t len, char *out, size_t cap)
{
    if (len != sizeof(DrinkPayload)) return 0;
    DrinkPayload p;
    memcpy(&p, data, sizeof(p));
    return snprintf(out, cap, "\"drink\":{\"armed\":%u,\"active\":%u,\"tilt\":%u,\"gulps\":%u}",
                    (p.flags >> 1) & 1, p.flags & 1, p.tilt_deg, p.gulps);
}

/**
 * @brief [Module.to_text] 受信データを LCD 用の 1 行にする（停止中は "PRESS A"）。
 */
static int drink_to_text(const uint8_t *data, size_t len, char *out, size_t cap)
{
    if (len != sizeof(DrinkPayload)) return 0;
    DrinkPayload p;
    memcpy(&p, data, sizeof(p));
    if (!(p.flags & 0x02)) return snprintf(out, cap, "PRESS A\n");
    return snprintf(out, cap, "%s %3udeg #%u\n", (p.flags & 0x01) ? "GOKU" : "----", p.tilt_deg, p.gulps);
}

extern const Module module_drink;
const Module module_drink = {
    "drink",
    MODULE_ID_DRINK,
    DRINK_PERIOD_MS,
    drink_init,
    drink_update,
    drink_encode,
    drink_to_json,
    drink_to_text,
    nullptr,
    drink_take_event,
};

#endif  // MODULE_DRINK_ENABLE

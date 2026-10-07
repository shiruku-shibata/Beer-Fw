/**
 * 加速度モジュール（内蔵 IMU）
 *
 * 【送信データ】AccelPayload 12 バイト（float x, y, z）
 * 【JSON】"accel":{"x":0.1234,"y":-0.4567,"z":0.9789}
 */

#include "config.h"
#if MODULE_ACCEL_ENABLE

#include <M5Unified.h>
#include <stdio.h>
#include <string.h>
#include "core/protocol.h"
#include "modules/accel.h"
#include "modules/module.h"

typedef struct __attribute__((packed)) {
    float x;
    float y;
    float z;
} AccelPayload;

static AccelPayload s_latest = {};
static bool         s_valid  = false;

bool accel_get(float *x, float *y, float *z)
{
    *x = s_latest.x;
    *y = s_latest.y;
    *z = s_latest.z;
    return s_valid;
}

static bool accel_init(void)
{
    return M5.Imu.isEnabled();
}

static void accel_update(void)
{
    if (M5.Imu.getAccelData(&s_latest.x, &s_latest.y, &s_latest.z)) {
        s_valid = true;
    }
}

static size_t accel_encode(uint8_t *buf, size_t cap)
{
    if (cap < sizeof(AccelPayload)) return 0;
    memcpy(buf, &s_latest, sizeof(AccelPayload));
    return sizeof(AccelPayload);
}

static int accel_to_json(const uint8_t *data, size_t len, char *out, size_t cap)
{
    if (len != sizeof(AccelPayload)) return 0;
    AccelPayload p;
    memcpy(&p, data, sizeof(p));
    return snprintf(out, cap, "\"accel\":{\"x\":%.4f,\"y\":%.4f,\"z\":%.4f}", p.x, p.y, p.z);
}

static int accel_to_text(const uint8_t *data, size_t len, char *out, size_t cap)
{
    if (len != sizeof(AccelPayload)) return 0;
    AccelPayload p;
    memcpy(&p, data, sizeof(p));
    return snprintf(out, cap, "X:%7.3f\nY:%7.3f\nZ:%7.3f\n", p.x, p.y, p.z);
}

extern const Module module_accel;
const Module module_accel = {
    "accel",
    MODULE_ID_ACCEL,
    ACCEL_PERIOD_MS,
    accel_init,
    accel_update,
    accel_encode,
    accel_to_json,
    accel_to_text,
    nullptr,
};

#endif  // MODULE_ACCEL_ENABLE

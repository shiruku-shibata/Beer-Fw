/**
 * 新規モジュールのひな形
 *
 * 【使い方】
 * 1. このファイルを modules/xxx.cpp にコピーし、xxx / XXX を置き換える
 * 2. core/protocol.h に MODULE_ID_XXX を追加（既存 ID の再利用禁止）
 * 3. config.h に MODULE_XXX_ENABLE と XXX_PERIOD_MS を追加
 * 4. modules/registry.cpp に extern 宣言とテーブル行を追加
 *
 * MODULE_TEMPLATE_ENABLE は定義しないため、このファイル自体はビルドされない。
 */

#include "config.h"
#if MODULE_TEMPLATE_ENABLE

#include <stdio.h>
#include <string.h>
#include "core/protocol.h"
#include "modules/module.h"

typedef struct __attribute__((packed)) {
    int16_t value;
} XxxPayload;

static XxxPayload s_latest = {};

static bool xxx_init(void)
{
    // センサーの初期化。失敗したら false
    return true;
}

static void xxx_update(void)
{
    // センサーを読んで s_latest を更新する（ブロッキング禁止）
}

static size_t xxx_encode(uint8_t *buf, size_t cap)
{
    if (cap < sizeof(XxxPayload)) return 0;
    memcpy(buf, &s_latest, sizeof(XxxPayload));
    return sizeof(XxxPayload);
}

static int xxx_to_json(const uint8_t *data, size_t len, char *out, size_t cap)
{
    if (len != sizeof(XxxPayload)) return 0;
    XxxPayload p;
    memcpy(&p, data, sizeof(p));
    return snprintf(out, cap, "\"xxx\":%d", p.value);
}

static int xxx_to_text(const uint8_t *data, size_t len, char *out, size_t cap)
{
    if (len != sizeof(XxxPayload)) return 0;
    XxxPayload p;
    memcpy(&p, data, sizeof(p));
    return snprintf(out, cap, "XXX:%d\n", p.value);
}

extern const Module module_xxx;
const Module module_xxx = {
    "xxx",
    MODULE_ID_XXX,
    XXX_PERIOD_MS,
    xxx_init,
    xxx_update,
    xxx_encode,
    xxx_to_json,
    xxx_to_text,
    nullptr,   // busy: light sleep を止める必要があれば関数を指定
    nullptr,   // take_event: 状態変化をすぐ送りたければ関数を指定
};

#endif  // MODULE_TEMPLATE_ENABLE

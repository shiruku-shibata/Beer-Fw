/**
 * モジュール登録テーブル
 *
 * モジュール名と #if を書くのはこのファイルだけ。
 * 新しいモジュールを追加したら extern 宣言とテーブル行を 1 行ずつ足す。
 */

#include "config.h"
#include "modules/module.h"

// =============================================
// モジュール宣言
// =============================================
#if MODULE_ACCEL_ENABLE
extern const Module module_accel;
#endif
#if MODULE_VIBRATION_ENABLE
extern const Module module_vibration;
#endif
#if MODULE_DRINK_ENABLE
extern const Module module_drink;
#endif
#if MODULE_LOWBAT_ENABLE
extern const Module module_lowbat;
#endif

// =============================================
// 登録テーブル
// =============================================
const Module *const g_modules[] = {
#if MODULE_ACCEL_ENABLE
    &module_accel,
#endif
#if MODULE_VIBRATION_ENABLE
    &module_vibration,
#endif
#if MODULE_DRINK_ENABLE
    &module_drink,      // accel より後に置く（同じ周回で最新値を使うため）
#endif
#if MODULE_LOWBAT_ENABLE
    &module_lowbat,
#endif
    nullptr  // 終端（モジュール 0 個でも空配列にしないため）
};

const size_t g_moduleCount = sizeof(g_modules) / sizeof(g_modules[0]) - 1;

const Module *module_find(uint8_t id)
{
    for (size_t i = 0; i < g_moduleCount; i++) {
        if (g_modules[i]->id == id) return g_modules[i];
    }
    return nullptr;
}

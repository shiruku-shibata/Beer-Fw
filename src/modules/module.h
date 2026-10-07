/**
 * センサー・機能モジュールの共通インターフェース
 *
 * 各モジュールは const Module を 1 つだけ公開し、modules/registry.cpp に登録する。
 * 内部の関数・変数はすべて static にし、他モジュールを直接呼ばない。
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#define MODULE_MAX  16

typedef struct {
    const char *name;        // ログ・表示用
    uint8_t     id;          // core/protocol.h の MODULE_ID_*
    uint32_t    period_ms;   // update() を呼ぶ周期 [ms]

    /** 初期化。失敗時 false を返すとそのモジュールだけ無効になる（nullptr 可） */
    bool   (*init)(void);

    /** 計測。ブロッキング禁止（nullptr 可） */
    void   (*update)(void);

    /** 最新値を送信用バイト列に書く。書いたバイト数を返す（0 = 送るものなし） */
    size_t (*encode)(uint8_t *buf, size_t cap);

    /** [gateway] 受信データを JSON の 1 要素（"key":value）にする。書いた文字数を返す */
    int    (*to_json)(const uint8_t *data, size_t len, char *out, size_t cap);

    /** LCD 用の短いテキストにする（改行可）。書いた文字数を返す（nullptr 可） */
    int    (*to_text)(const uint8_t *data, size_t len, char *out, size_t cap);

    /** light sleep できない状態なら true（PWM 出力中など）。nullptr = 常に sleep 可 */
    bool   (*busy)(void);

    /** すぐ送信すべき変化（状態遷移など）があれば true を返し、フラグを下ろす。nullptr 可 */
    bool   (*take_event)(void);
} Module;

/** 有効なモジュールの一覧（registry.cpp）。末尾は nullptr */
extern const Module *const g_modules[];
extern const size_t        g_moduleCount;

/**
 * @brief モジュール ID から登録済みモジュールを探す。
 *
 * @return 見つからなければ nullptr
 */
const Module *module_find(uint8_t id);

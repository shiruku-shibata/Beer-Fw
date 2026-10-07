/**
 * コンパイル時設定
 *
 * 【方針】
 * - ここには既定値だけを置く。個体ごとの値（NODE_ID など）は
 *   platformio.ini の build_flags で上書きする。
 * - プロトコル定数・モジュール ID は core/protocol.h に置く（互換性に関わるため）。
 */
#pragma once

// =============================================
// 役割（platformio.ini の環境で -D 指定）
// =============================================
#if defined(ROLE_NODE) && defined(ROLE_GATEWAY)
#error "ROLE_NODE と ROLE_GATEWAY は同時に指定できません"
#endif
#if !defined(ROLE_NODE) && !defined(ROLE_GATEWAY)
#error "ROLE_NODE か ROLE_GATEWAY を build_flags で指定してください"
#endif

// =============================================
// モジュール有効/無効
// =============================================
// 汎用モジュール（センサー・出力・電池監視）は既定で有効。
// ゲーム専用モジュールは既定で無効にし、ゲームごとの環境（platformio.ini）で有効にする。
// gateway は受信データを JSON にするため、ゲーム専用モジュールもすべて有効にする
// （gateway はモジュールの init / update を呼ばないので、センサーや振動は動かない）。
#if defined(ROLE_GATEWAY)
#define MODULE_GAME_DEFAULT     1
#else
#define MODULE_GAME_DEFAULT     0
#endif

// ---- 汎用 ----
#ifndef MODULE_ACCEL_ENABLE
#define MODULE_ACCEL_ENABLE     1
#endif
#ifndef MODULE_VIBRATION_ENABLE
#define MODULE_VIBRATION_ENABLE 1       // Vibration Hat（U159）
#endif
#ifndef MODULE_LOWBAT_ENABLE
#define MODULE_LOWBAT_ENABLE    1       // 電池残量監視（20% フラグ・切れ警報。vibration が必要）
#endif

// ---- ゲーム専用 ----
#ifndef MODULE_DRINK_ENABLE
#define MODULE_DRINK_ENABLE     MODULE_GAME_DEFAULT   // 飲酒検出・ビールジョッキ（accel + vibration が必要）
#endif

// =============================================
// モジュール別パラメータ
// =============================================
#define ACCEL_PERIOD_MS         20      // 加速度の取得周期 [ms]（50 Hz）

#define VIBRATION_PERIOD_MS     20      // 停止時刻の確認周期 [ms]（振動時間の分解能）
#define VIBRATION_PIN           26      // Hat の制御ピン
#define VIBRATION_LEDC_CHANNEL  1       // 7 は LCD バックライトが使用
#define VIBRATION_PWM_FREQ      10000   // [Hz]
#define VIBRATION_PWM_BITS      8
#ifndef VIBRATION_BOOT_TEST_MS
#define VIBRATION_BOOT_TEST_MS  0       // >0 で起動時にその時間だけ振動（動作確認用）
#endif

#define DRINK_PERIOD_MS               20     // 判定周期 [ms]
#define DRINK_TILT_START_DEG          40     // この角度以上で飲み始め候補 [deg]
#define DRINK_TILT_END_DEG            30     // この角度未満で飲み終わり [deg]
#define DRINK_START_HOLD_MS           300    // 飲み始めと判定するまでの継続時間 [ms]
#define DRINK_GULP_STRENGTH           100    // ゴクッの強さ [%]
#define DRINK_GULP_ON1_MS             160    // 「ゴ」の振動時間 [ms]（モーターの立ち上がりに約 50 ms 必要）
#define DRINK_GULP_GAP_MS             40     // 「ゴ」と「ク」の間 [ms]
#define DRINK_GULP_ON2_MS             120    // 「ク」の振動時間 [ms]
#define DRINK_GULP_INTERVAL_SLOW_MS   650    // 浅い傾きでのゴクッ間隔 [ms]
#define DRINK_GULP_INTERVAL_FAST_MS   350    // 90° 傾けたときのゴクッ間隔 [ms]
#define DRINK_CALIB_SAMPLES           25     // 直立基準を取るサンプル数（× DRINK_PERIOD_MS）
#define DRINK_FILTER_ALPHA            0.3f   // ローパス係数（小さいほど滑らか・遅い）
#define DRINK_STOP_HOLD_MS            1000   // ボタン A をこの時間以上長押しで検出停止 [ms]

#define LOWBAT_PERIOD_MS              1000   // 電池残量の確認周期 [ms]
#ifndef LOWBAT_ALARM_PERCENT
#define LOWBAT_ALARM_PERCENT          5      // この残量以下で警報振動（連続）[%]
#endif
#ifndef LOWBAT_LOW_PERCENT
#define LOWBAT_LOW_PERCENT            20     // この残量以下で「残量低下」フラグを gateway へ送る [%]
#endif
#define LOWBAT_HYSTERESIS             5      // しきい値 + この値まで戻れば解除 [%]

// =============================================
// 通信
// =============================================
#ifndef COMM_ENABLE
#define COMM_ENABLE             1       // 0: node 単体動作（WiFi / ESP-NOW を起動しない。省電力）
#endif
#define COMM_SERIAL_BAUD        115200
#define COMM_ESPNOW_CHANNEL     1       // gateway の AP チャンネル
#ifndef COMM_LOW_POWER
#define COMM_LOW_POWER          0       // 1: 送信時だけ無線を起動（省電力。node は受信できない）
#endif
#ifndef COMM_TX_PERIOD_MS
#define COMM_TX_PERIOD_MS       100     // node の定期送信周期 [ms]（状態変化時はすぐ送る）
#endif
#define COMM_SEND_TIMEOUT_MS    30      // 低電力通信で送信完了を待つ上限 [ms]
#define COMM_RX_QUEUE_LEN       16      // gateway の受信リングバッファ段数

// node の送信先（gateway の AP MAC）
#ifndef COMM_GATEWAY_MAC
#define COMM_GATEWAY_MAC        {0x00, 0x4B, 0x12, 0xA0, 0x9A, 0x5D}
#endif

// node の識別番号（1〜255）。個体ごとに build_flags で上書きする
#ifndef NODE_ID
#define NODE_ID                 1
#endif

#define GATEWAY_MAX_NODES       8       // gateway が同時に扱う node 数

// =============================================
// 電源
// =============================================
#define POWER_CHECK_PERIOD_MS   5000
#define POWER_LOW_PERCENT       15
#define POWER_CPU_FREQ_MHZ      80      // CPU クロック [MHz]（WiFi・UART が動く下限）
#ifndef POWER_LIGHT_SLEEP_ENABLE
#define POWER_LIGHT_SLEEP_ENABLE 1      // 待ち時間を light sleep にする（通信なし・画面オフ・振動なしのときだけ）
#endif

// =============================================
// 表示
// =============================================
#define DISPLAY_PERIOD_MS       100     // LCD 更新の最小間隔 [ms]
#ifndef DISPLAY_AUTO_OFF_MS
#if defined(ROLE_NODE)
#define DISPLAY_AUTO_OFF_MS     10000   // 操作がなければこの時間で画面を消す [ms]。0 = 常時点灯
#else
#define DISPLAY_AUTO_OFF_MS     0       // gateway は USB 給電前提なので常時点灯
#endif
#endif
#define DISPLAY_BRIGHTNESS      64      // 点灯時の明るさ 0〜255

// =============================================
// ログ（LOG_LEVEL_* は core/log.h で定義）
// =============================================
#ifndef LOG_LEVEL
#define LOG_LEVEL               LOG_LEVEL_INFO
#endif

// =============================================
// 整合性チェック
// =============================================
#if NODE_ID < 1 || NODE_ID > 255
#error "NODE_ID は 1〜255 で指定してください"
#endif
#if defined(ROLE_GATEWAY) && !COMM_ENABLE
#error "gateway は COMM_ENABLE=1 が必要です"
#endif
#if MODULE_DRINK_ENABLE && !(MODULE_ACCEL_ENABLE && MODULE_VIBRATION_ENABLE)
#error "MODULE_DRINK_ENABLE には MODULE_ACCEL_ENABLE と MODULE_VIBRATION_ENABLE が必要です"
#endif
#if LOWBAT_ALARM_PERCENT >= LOWBAT_LOW_PERCENT
#error "LOWBAT_ALARM_PERCENT は LOWBAT_LOW_PERCENT より小さくしてください"
#endif
#if MODULE_LOWBAT_ENABLE && !MODULE_VIBRATION_ENABLE
#error "MODULE_LOWBAT_ENABLE には MODULE_VIBRATION_ENABLE が必要です"
#endif
#if DRINK_TILT_END_DEG >= DRINK_TILT_START_DEG
#error "DRINK_TILT_END_DEG は DRINK_TILT_START_DEG より小さくしてください"
#endif

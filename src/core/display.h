/**
 * LCD の共通表示
 *
 * 画面レイアウト（タイトル・本文・フッタ）だけを提供し、
 * 何を表示するかは app/ とモジュールの to_text() が決める。
 */
#pragma once

#include <stdint.h>
#include <M5Unified.h>   // TFT_* 色定数

/**
 * @brief LCD を初期化する（横向き、黒背景）。
 */
void display_init(void);

/**
 * @brief 前回描画から DISPLAY_PERIOD_MS 経過していれば true を返し、時刻を更新する。
 *        画面が消えている間は常に false。
 *
 * @param now 現在時刻 [ms]
 */
bool display_due(uint32_t now);

/**
 * @brief 画面を点灯し、自動消灯までの時間を延長する（ボタン操作時に呼ぶ）。
 */
void display_wake(void);

/**
 * @brief DISPLAY_AUTO_OFF_MS 操作がなければ画面を消す（パネルスリープ＋バックライト消灯）。
 *
 * @param now 現在時刻 [ms]
 */
void display_update(uint32_t now);

/**
 * @brief 画面が点灯中か。
 */
bool display_is_on(void);

/**
 * @brief 画面をクリアしてタイトル行を描く。
 *
 * @param title タイトル文字列
 * @param color 文字色（TFT_*）
 */
void display_begin(const char *title, uint16_t color);

/**
 * @brief 現在のカーソル位置から文字列を描く。
 *
 * @param text 文字列（改行可）
 * @param size 文字サイズ
 * @param color 文字色（TFT_*）
 */
void display_print(const char *text, uint8_t size, uint16_t color);

/**
 * @brief 電池残量をフッタに描く。
 */
void display_footer(void);

/**
 * @brief 赤画面にメッセージを出して停止する。復帰しない。
 *
 * @param message 表示するメッセージ
 */
void display_fatal(const char *message);

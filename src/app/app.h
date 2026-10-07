/**
 * 役割ごとのアプリケーション
 *
 * 実装は app/node.cpp か app/gateway.cpp のどちらか一方
 * （platformio.ini の build_src_filter で選択）。
 */
#pragma once

/**
 * @brief 役割固有の初期化（通信・モジュールなど）。
 */
void app_init(void);

/**
 * @brief メインループから毎回呼ばれる。
 */
void app_loop(void);

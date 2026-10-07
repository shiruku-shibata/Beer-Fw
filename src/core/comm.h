/**
 * ESP-NOW 通信
 *
 * - node   : comm_send() で gateway へ送る
 * - gateway: 受信コールバックがリングバッファに積み、comm_receive() で取り出す
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief WiFi と ESP-NOW を役割に応じて初期化する。
 *
 * @return 成功時 true
 */
bool comm_init(void);

/**
 * @brief 自機の MAC 文字列を返す（node: STA、gateway: AP）。
 */
const char *comm_self_mac(void);

/**
 * @brief MAC アドレスを "AA:BB:CC:DD:EE:FF" 形式にする。
 *
 * @param mac 6 バイトの MAC
 * @param out 出力先（18 バイト以上）
 */
void comm_mac_to_str(const uint8_t *mac, char *out);

// ---- node ----

/**
 * @brief gateway へ送信する。
 *
 * @return esp_now_send() が受け付けたら true（到達は comm_last_send_ok() で確認）
 */
bool comm_send(const uint8_t *data, size_t len);

/**
 * @brief 直前の送信が相手に届いたか。
 */
bool comm_last_send_ok(void);

// ---- gateway ----

/**
 * @brief 受信リングバッファから 1 件取り出す。
 *
 * @param mac 送信元 MAC の出力先（6 バイト）
 * @param buf データの出力先（PROTOCOL_MAX_PAYLOAD バイト以上）
 * @param len 受信バイト数の出力先
 * @return 取り出せたら true
 */
bool comm_receive(uint8_t *mac, uint8_t *buf, size_t *len);

/**
 * @brief リングバッファ溢れで捨てたパケット数。
 */
uint32_t comm_dropped_count(void);

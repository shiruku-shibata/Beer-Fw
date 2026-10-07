/**
 * node → gateway の ESP-NOW パケット形式
 *
 * | PacketHeader | RecordHeader + data | RecordHeader + data | ...
 *
 * - レコードはモジュールごとに 1 つ。data の中身は各モジュールが決める。
 * - gateway は知らない module_id のレコードを length で読み飛ばす。
 * - 形式を変えたら PROTOCOL_VERSION を上げ、doc/ を更新すること。
 */
#pragma once

#include <stdint.h>

#define PROTOCOL_VERSION      1
#define PROTOCOL_MAX_PAYLOAD  250   // ESP-NOW の 1 パケット上限 [byte]

// =============================================
// モジュール ID（一度割り当てたら変更・再利用しない）
// =============================================
#define MODULE_ID_ACCEL       0x01
#define MODULE_ID_VIBRATION   0x02
#define MODULE_ID_DRINK       0x03
#define MODULE_ID_LOWBAT      0x04

// =============================================
// パケット構造
// =============================================
typedef struct __attribute__((packed)) {
    uint8_t  version;       // PROTOCOL_VERSION
    uint8_t  node_id;       // NODE_ID
    uint16_t seq;           // 送信ごとに +1（欠落検出用）
    uint32_t time_ms;       // node の millis()
    uint8_t  record_count;  // 後続レコード数
} PacketHeader;

typedef struct __attribute__((packed)) {
    uint8_t module_id;      // MODULE_ID_*
    uint8_t length;         // 直後の data のバイト数
} RecordHeader;

#include "core/comm.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "core/log.h"
#include "core/protocol.h"

static char s_selfMac[18] = "";

void comm_mac_to_str(const uint8_t *mac, char *out)
{
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

const char *comm_self_mac(void)
{
    return s_selfMac;
}

#if defined(ROLE_NODE) && !COMM_ENABLE
// =============================================
// node 単体動作: WiFi を起動しない（省電力）
// =============================================
bool comm_init(void)
{
    strncpy(s_selfMac, "standalone", sizeof(s_selfMac) - 1);
    LOG_I("Comm disabled (standalone)");
    return true;
}

bool comm_send(const uint8_t *, size_t) { return false; }
bool comm_last_send_ok(void) { return false; }
bool comm_receive(uint8_t *, uint8_t *, size_t *) { return false; }
uint32_t comm_dropped_count(void) { return 0; }

#elif defined(ROLE_NODE)
// =============================================
// node: 送信
//   COMM_LOW_POWER = 0: 無線を起動したまま送る
//   COMM_LOW_POWER = 1: 送信のたびに無線を起動し、送信完了後に止める（省電力）
// =============================================
#include <esp_wifi.h>

static const uint8_t  GATEWAY_MAC[6] = COMM_GATEWAY_MAC;
static volatile bool  s_lastSendOk   = false;
static volatile bool  s_sendDone     = false;
static bool           s_radioUp      = false;

/**
 * @brief 送信結果コールバック（WiFi タスクから呼ばれる）。
 */
static void onDataSent(const uint8_t *mac, esp_now_send_status_t status)
{
    (void)mac;
    s_lastSendOk = (status == ESP_NOW_SEND_SUCCESS);
    s_sendDone   = true;
}

/**
 * @brief 無線と ESP-NOW を起動し、gateway をピア登録する。
 */
static bool radioUp(void)
{
    if (s_radioUp) return true;

    if (esp_wifi_start() != ESP_OK) {
        LOG_E("WiFi start failed");
        return false;
    }
    esp_wifi_set_channel(COMM_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        LOG_E("ESP-NOW init failed");
        return false;
    }
    esp_now_register_send_cb(onDataSent);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, GATEWAY_MAC, 6);
    peer.channel = 0;   // 現在のチャンネルを使う
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
        LOG_E("Failed to add gateway peer");
        esp_now_deinit();
        return false;
    }

    s_radioUp = true;
    return true;
}

/**
 * @brief ESP-NOW と無線を止める（RF の消費電流をなくす）。
 */
static void radioDown(void)
{
    if (!s_radioUp) return;
    esp_now_deinit();
    esp_wifi_stop();
    s_radioUp = false;
}

bool comm_init(void)
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    strncpy(s_selfMac, WiFi.macAddress().c_str(), sizeof(s_selfMac) - 1);
    LOG_I("STA MAC: %s", s_selfMac);

    if (!radioUp()) return false;

    char gw[18];
    comm_mac_to_str(GATEWAY_MAC, gw);
    LOG_I("Gateway: %s (ch %d)%s", gw, COMM_ESPNOW_CHANNEL, COMM_LOW_POWER ? " [low power]" : "");

#if COMM_LOW_POWER
    radioDown();
#endif
    return true;
}

bool comm_send(const uint8_t *data, size_t len)
{
    if (len == 0 || len > PROTOCOL_MAX_PAYLOAD) return false;
    uint32_t tStart = millis();
    if (!radioUp()) return false;

    s_sendDone = false;
    bool ok = (esp_now_send(GATEWAY_MAC, data, len) == ESP_OK);

#if COMM_LOW_POWER
    // 送信完了（gateway の ACK 受信 or 再送打ち切り）を待ってから無線を止める
    uint32_t t0 = millis();
    while (ok && !s_sendDone && millis() - t0 < COMM_SEND_TIMEOUT_MS) {
        delay(1);
    }
    radioDown();
    LOG_D("Radio burst %lu ms (ack %s)", (unsigned long)(millis() - tStart), s_sendDone ? "done" : "timeout");
#endif
    return ok;
}

bool comm_last_send_ok(void)
{
    return s_lastSendOk;
}

bool comm_receive(uint8_t *, uint8_t *, size_t *) { return false; }
uint32_t comm_dropped_count(void) { return 0; }

#else
// =============================================
// gateway: 受信（コールバック → リングバッファ → loop）
// =============================================
typedef struct {
    uint8_t mac[6];
    uint8_t len;
    uint8_t data[PROTOCOL_MAX_PAYLOAD];
} RxSlot;

static RxSlot            s_ring[COMM_RX_QUEUE_LEN];
static volatile uint16_t s_head    = 0;   // 次に書く位置（コールバック側）
static volatile uint16_t s_tail    = 0;   // 次に読む位置（loop 側）
static volatile uint32_t s_dropped = 0;
static portMUX_TYPE      s_mux     = portMUX_INITIALIZER_UNLOCKED;

/**
 * @brief 受信コールバック（WiFi タスクから呼ばれる）。重い処理はしない。
 */
static void onDataReceived(const uint8_t *mac, const uint8_t *data, int len)
{
    if (len <= 0 || len > PROTOCOL_MAX_PAYLOAD) return;

    portENTER_CRITICAL(&s_mux);
    uint16_t next = (s_head + 1) % COMM_RX_QUEUE_LEN;
    if (next == s_tail) {
        s_dropped++;
    } else {
        RxSlot &slot = s_ring[s_head];
        memcpy(slot.mac, mac, 6);
        memcpy(slot.data, data, len);
        slot.len = (uint8_t)len;
        s_head = next;
    }
    portEXIT_CRITICAL(&s_mux);
}

bool comm_init(void)
{
    // AP モードで起動（ESP-NOW 安定動作のため）
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESPNOW-RX", nullptr, COMM_ESPNOW_CHANNEL, 1);  // 隠し AP
    delay(200);

    strncpy(s_selfMac, WiFi.softAPmacAddress().c_str(), sizeof(s_selfMac) - 1);
    LOG_I("AP  MAC: %s  <- set this to COMM_GATEWAY_MAC", s_selfMac);
    LOG_I("STA MAC: %s", WiFi.macAddress().c_str());

    if (esp_now_init() != ESP_OK) {
        LOG_E("ESP-NOW init failed");
        return false;
    }
    esp_now_register_recv_cb(onDataReceived);
    return true;
}

bool comm_receive(uint8_t *mac, uint8_t *buf, size_t *len)
{
    bool got = false;

    portENTER_CRITICAL(&s_mux);
    if (s_tail != s_head) {
        RxSlot &slot = s_ring[s_tail];
        memcpy(mac, slot.mac, 6);
        memcpy(buf, slot.data, slot.len);
        *len   = slot.len;
        s_tail = (s_tail + 1) % COMM_RX_QUEUE_LEN;
        got    = true;
    }
    portEXIT_CRITICAL(&s_mux);

    return got;
}

uint32_t comm_dropped_count(void)
{
    return s_dropped;
}

bool comm_send(const uint8_t *, size_t) { return false; }
bool comm_last_send_ok(void) { return false; }

#endif

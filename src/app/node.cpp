/**
 * node: 各モジュールで計測し、まとめて gateway へ送る（旧 Sender）
 *
 * 【手順】
 * 1. 先に gateway を書き込み、シリアルモニタか画面で AP MAC を確認する
 * 2. config.h の COMM_GATEWAY_MAC をその AP MAC に書き換える
 * 3. NODE_ID を個体ごとに変えて書き込む（platformio.ini 冒頭のコメント参照）
 */

#include "app/app.h"

#include <Arduino.h>
#include <stdio.h>
#include "config.h"
#include "core/comm.h"
#include "core/display.h"
#include "core/log.h"
#include "core/power.h"
#include "core/protocol.h"
#include "core/scheduler.h"
#include "modules/module.h"

static uint8_t  s_packet[PROTOCOL_MAX_PAYLOAD];
static uint16_t s_seq       = 0;
static uint32_t s_sendCount = 0;
static uint32_t s_lastTx    = 0;

static constexpr uint32_t IDLE_MAX_WAIT_MS = 100;   // ボタン応答のため、待ちはこれ以上にしない

/**
 * @brief 有効な全モジュールの最新値をパケットに詰める。
 *
 * @param now 現在時刻 [ms]
 * @return パケット長 [byte]
 */
static size_t buildPacket(uint32_t now)
{
    PacketHeader *hdr = reinterpret_cast<PacketHeader *>(s_packet);
    size_t  pos   = sizeof(PacketHeader);
    uint8_t count = 0;

    for (size_t i = 0; i < g_moduleCount; i++) {
        if (!scheduler_is_active(i)) continue;

        const Module *m = g_modules[i];
        size_t room = sizeof(s_packet) - pos;
        if (room <= sizeof(RecordHeader)) {
            LOG_W("Packet full, '%s' skipped", m->name);
            continue;
        }

        size_t cap = room - sizeof(RecordHeader);
        if (cap > 255) cap = 255;
        size_t len = m->encode(s_packet + pos + sizeof(RecordHeader), cap);
        if (len == 0) continue;

        RecordHeader *rec = reinterpret_cast<RecordHeader *>(s_packet + pos);
        rec->module_id = m->id;
        rec->length    = (uint8_t)len;
        pos += sizeof(RecordHeader) + len;
        count++;
    }

    hdr->version      = PROTOCOL_VERSION;
    hdr->node_id      = NODE_ID;
    hdr->seq          = s_seq++;
    hdr->time_ms      = now;
    hdr->record_count = count;
    return pos;
}

/**
 * @brief 各モジュールの最新値と送信状態を LCD に描く。
 */
static void draw(void)
{
    char buf[64];

    snprintf(buf, sizeof(buf), "== NODE #%d ==", NODE_ID);
    display_begin(buf, TFT_CYAN);

    for (size_t i = 0; i < g_moduleCount; i++) {
        if (!scheduler_is_active(i)) continue;
        const Module *m = g_modules[i];
        if (m->to_text == nullptr) continue;

        uint8_t data[PROTOCOL_MAX_PAYLOAD];
        size_t  len = m->encode(data, sizeof(data));
        if (len > 0 && m->to_text(data, len, buf, sizeof(buf)) > 0) {
            display_print(buf, 2, TFT_WHITE);
        }
    }

#if COMM_ENABLE
    bool ok = comm_last_send_ok();
    snprintf(buf, sizeof(buf), "Send:%s #%lu\n", ok ? " OK " : "FAIL", (unsigned long)s_sendCount);
    display_print(buf, 2, ok ? TFT_GREEN : TFT_RED);
#else
    display_print("SOLO\n", 2, TFT_DARKGREY);
#endif
    display_footer();
}

void app_init(void)
{
    if (!comm_init()) {
        display_fatal("ESP-NOW\nINIT\nFAILED");
    }
#if COMM_ENABLE
    display_begin("My MAC:", TFT_WHITE);
    display_print(comm_self_mac(), 2, TFT_WHITE);
    delay(2000);
#endif

    scheduler_init();
#if COMM_ENABLE
    LOG_I("Node ready. Sending every %d ms", COMM_TX_PERIOD_MS);
#else
    LOG_I("Node ready (standalone).");
#endif
}

void app_loop(void)
{
    uint32_t now = millis();

    scheduler_run(now);

#if COMM_ENABLE
    // 定期送信に加え、モジュールの状態変化（検出の開始・警報など）があればすぐ送る
    bool event = scheduler_take_events();
    if (event || now - s_lastTx >= COMM_TX_PERIOD_MS) {
        s_lastTx = now;
        size_t len = buildPacket(now);
        if (comm_send(s_packet, len)) {
            s_sendCount++;
        }
        LOG_D("Sent seq=%u len=%u last=%s", (unsigned)(s_seq - 1), (unsigned)len,
              comm_last_send_ok() ? "OK" : "FAIL");
    }
#endif

    if (display_due(now)) {
        draw();
    }

    // ---- 次の処理まで待つ（省電力） ----
    now = millis();
    uint32_t maxWait = IDLE_MAX_WAIT_MS;
#if COMM_ENABLE
    uint32_t sinceTx = now - s_lastTx;
    uint32_t toTx    = (sinceTx >= COMM_TX_PERIOD_MS) ? 0 : COMM_TX_PERIOD_MS - sinceTx;
    if (toTx < maxWait) maxWait = toTx;
#endif
    uint32_t wait = scheduler_ms_until_next(now, maxWait);

    // light sleep 中は WiFi・LCD バックライト・振動の PWM が止まるため、すべて停止中のときだけ眠る
    // （低電力通信では送信時以外は無線が止まっているので眠れる）
    bool canSleep = (!COMM_ENABLE || COMM_LOW_POWER) && !display_is_on() && !scheduler_busy();
    power_wait(wait, canSleep);
}

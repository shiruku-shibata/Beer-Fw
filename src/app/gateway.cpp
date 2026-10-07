/**
 * gateway: node からのパケットを受け、PC へ JSON で出す（旧 Receiver）
 *
 * 【シリアル出力】1 パケット = 1 行の JSON。ログ行は "#" で始まる。
 * {"node":1,"mac":"AA:BB:CC:DD:EE:FF","seq":42,"t":12345,"rx":43,"lost":0,"accel":{"x":0.1234,"y":-0.4567,"z":0.9789}}
 *
 * 【操作】ボタン A で表示する node を切り替える
 */

#include "app/app.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "core/comm.h"
#include "core/display.h"
#include "core/log.h"
#include "core/protocol.h"
#include "modules/module.h"

// =============================================
// node 管理テーブル
// =============================================
typedef struct {
    uint8_t  mac[6];
    uint8_t  nodeId;
    uint16_t lastSeq;
    uint32_t rxCount;
    uint32_t lostCount;
    uint8_t  data[PROTOCOL_MAX_PAYLOAD];   // 最新パケット（表示用）
    size_t   len;
} NodeEntry;

static NodeEntry s_nodes[GATEWAY_MAX_NODES] = {};
static uint8_t   s_nodeCount  = 0;
static uint8_t   s_dispNode   = 0;
static bool      s_dispDirty  = false;
static uint32_t  s_lastDropped = 0;

/**
 * @brief MAC で node を探し、なければ追加する。
 *
 * @return インデックス。テーブルが満杯なら -1
 */
static int findOrAddNode(const uint8_t *mac)
{
    for (int i = 0; i < s_nodeCount; i++) {
        if (memcmp(s_nodes[i].mac, mac, 6) == 0) return i;
    }
    if (s_nodeCount >= GATEWAY_MAX_NODES) return -1;

    int idx = s_nodeCount++;
    memset(&s_nodes[idx], 0, sizeof(NodeEntry));
    memcpy(s_nodes[idx].mac, mac, 6);

    char macStr[18];
    comm_mac_to_str(mac, macStr);
    LOG_I("New node #%d: %s", idx + 1, macStr);
    return idx;
}

/**
 * @brief パケット内のレコードを順に走査し、モジュールごとの処理を呼ぶ。
 *
 * @param data パケット先頭
 * @param len  パケット長
 * @param fn   レコードごとに呼ぶ関数（module は未登録なら nullptr）
 */
template <typename F>
static void forEachRecord(const uint8_t *data, size_t len, F fn)
{
    const PacketHeader *hdr = reinterpret_cast<const PacketHeader *>(data);
    size_t pos = sizeof(PacketHeader);

    for (uint8_t r = 0; r < hdr->record_count; r++) {
        if (pos + sizeof(RecordHeader) > len) break;
        const RecordHeader *rec = reinterpret_cast<const RecordHeader *>(data + pos);
        pos += sizeof(RecordHeader);
        if (pos + rec->length > len) break;

        fn(module_find(rec->module_id), rec, data + pos);
        pos += rec->length;
    }
}

/**
 * @brief 1 パケットを JSON 1 行にしてシリアルへ出す。
 */
static void sendJson(const NodeEntry &node, const PacketHeader *hdr, const uint8_t *data, size_t len)
{
    char line[768];
    char macStr[18];
    comm_mac_to_str(node.mac, macStr);

    int pos = snprintf(line, sizeof(line),
                       "{\"node\":%u,\"mac\":\"%s\",\"seq\":%u,\"t\":%lu,\"rx\":%lu,\"lost\":%lu",
                       hdr->node_id, macStr, hdr->seq, (unsigned long)hdr->time_ms,
                       (unsigned long)node.rxCount, (unsigned long)node.lostCount);

    forEachRecord(data, len, [&](const Module *m, const RecordHeader *rec, const uint8_t *body) {
        if (m == nullptr || m->to_json == nullptr) {
            LOG_D("Unknown module 0x%02X skipped", rec->module_id);
            return;
        }
        if (pos >= (int)sizeof(line) - 2) return;
        line[pos++] = ',';
        int n = m->to_json(body, rec->length, line + pos, sizeof(line) - pos - 1);
        if (n > 0 && pos + n < (int)sizeof(line) - 1) {
            pos += n;
        } else {
            pos--;   // 書けなかったらカンマを取り消す
        }
    });

    if (pos < (int)sizeof(line) - 1) {
        line[pos++] = '}';
        line[pos]   = '\0';
        Serial.println(line);
    } else {
        LOG_W("JSON line too long, dropped (node %u)", hdr->node_id);
    }
}

/**
 * @brief 受信パケット 1 件を検証・記録・出力する。
 */
static void handlePacket(const uint8_t *mac, const uint8_t *data, size_t len)
{
    if (len < sizeof(PacketHeader)) {
        LOG_W("Short packet (%u bytes)", (unsigned)len);
        return;
    }
    const PacketHeader *hdr = reinterpret_cast<const PacketHeader *>(data);
    if (hdr->version != PROTOCOL_VERSION) {
        LOG_W("Protocol mismatch: got v%u, expected v%d", hdr->version, PROTOCOL_VERSION);
        return;
    }

    int idx = findOrAddNode(mac);
    if (idx < 0) {
        LOG_W("Node table full, packet dropped");
        return;
    }

    NodeEntry &node = s_nodes[idx];
    if (node.rxCount > 0) {
        uint16_t gap = (uint16_t)(hdr->seq - node.lastSeq - 1);
        if (gap < 1000) node.lostCount += gap;   // 大きな飛びは node の再起動とみなす
    }
    node.nodeId  = hdr->node_id;
    node.lastSeq = hdr->seq;
    node.rxCount++;
    memcpy(node.data, data, len);
    node.len = len;

    sendJson(node, hdr, data, len);

    if (idx == s_dispNode) s_dispDirty = true;
}

/**
 * @brief 受信待ち画面を描く。
 */
static void drawWaiting(void)
{
    display_begin("== GATEWAY ==", TFT_CYAN);
    display_print("Waiting...\n", 2, TFT_WHITE);
    display_footer();
}

/**
 * @brief 選択中の node の最新値を描く。
 */
static void drawNode(uint8_t idx)
{
    if (idx >= s_nodeCount) return;
    const NodeEntry &node = s_nodes[idx];

    static const uint16_t headerColors[] = {TFT_GREEN, TFT_RED, TFT_YELLOW, TFT_CYAN, TFT_MAGENTA};
    uint16_t color = (idx < 5) ? headerColors[idx] : TFT_WHITE;

    char buf[64];
    snprintf(buf, sizeof(buf), "Node %d (%d/%d)", node.nodeId, idx + 1, s_nodeCount);
    display_begin(buf, color);

    forEachRecord(node.data, node.len, [&](const Module *m, const RecordHeader *rec, const uint8_t *body) {
        if (m == nullptr || m->to_text == nullptr) return;
        if (m->to_text(body, rec->length, buf, sizeof(buf)) > 0) {
            display_print(buf, 2, TFT_WHITE);
        }
    });

    snprintf(buf, sizeof(buf), "rx:%lu lost:%lu\n", (unsigned long)node.rxCount, (unsigned long)node.lostCount);
    display_print(buf, 2, TFT_GREEN);

    char macStr[18];
    comm_mac_to_str(node.mac, macStr);
    display_print(macStr, 1, TFT_LIGHTGREY);
    display_footer();
}

void app_init(void)
{
    if (!comm_init()) {
        display_fatal("ESP-NOW\nFAILED");
    }

    // 起動時に AP MAC を表示（3 秒）
    display_begin("AP MAC:", TFT_YELLOW);
    display_print(comm_self_mac(), 1, TFT_WHITE);
    display_print("\n\nSet to\nCOMM_GATEWAY_MAC", 2, TFT_CYAN);
    delay(3000);

    LOG_I("Gateway ready.");
    drawWaiting();
}

void app_loop(void)
{
    // ボタン A: 表示する node を切り替える
    if (M5.BtnA.wasPressed() && s_nodeCount > 1) {
        s_dispNode  = (s_dispNode + 1) % s_nodeCount;
        s_dispDirty = true;
    }

    // 受信キューを空になるまで処理
    uint8_t mac[6];
    uint8_t data[PROTOCOL_MAX_PAYLOAD];
    size_t  len;
    while (comm_receive(mac, data, &len)) {
        handlePacket(mac, data, len);
    }

    uint32_t dropped = comm_dropped_count();
    if (dropped != s_lastDropped) {
        LOG_W("RX queue overflow: %lu packets dropped in total", (unsigned long)dropped);
        s_lastDropped = dropped;
    }

    uint32_t now = millis();
    if (s_dispDirty && display_due(now)) {
        drawNode(s_dispNode);
        s_dispDirty = false;
    }

    delay(1);
}

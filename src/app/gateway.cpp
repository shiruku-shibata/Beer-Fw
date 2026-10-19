/**
 * gateway: node からのパケットを受け、PC へ JSON で出す（旧 Receiver）
 *
 * 【シリアル出力】1 パケット = 1 行の JSON。ログ行は "#" で始まる。
 * {"node":1,"mac":"AA:BB:CC:DD:EE:FF","seq":42,"t":12345,"rx":43,"lost":0,"accel":{"x":0.1234,"y":-0.4567,"z":0.9789}}
 * gateway 自身の加速度は GATEWAY_ACCEL_PERIOD_MS ごとに "node":0 の行で出す（GATEWAY_ACCEL_ENABLE）。
 * {"node":0,"mac":"<AP MAC>","seq":7,"t":12345,"accel":{"x":0.0012,"y":-0.0034,"z":1.0011}}
 *
 * 【操作】ボタン A で表示する node を切り替える。ボタン B で AP MAC 表示と切り替える
 */

#include "app/app.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "config.h"
#include "core/comm.h"
#include "core/display.h"
#include "core/log.h"
#include "core/protocol.h"
#include "modules/module.h"
#if GATEWAY_ACCEL_ENABLE
#include "modules/accel.h"
#endif

// =============================================
// node 管理テーブル
// =============================================
typedef struct {
    const char *mac;   // "AA:BB:CC:DD:EE:FF"
    uint8_t     nodeId;
} NodeMapEntry;

// config.h の GATEWAY_NODE_TABLE。末尾は番兵（nodeId = 0）
static const NodeMapEntry NODE_MAP[] = { GATEWAY_NODE_TABLE {nullptr, 0} };

typedef struct {
    uint8_t  mac[6];
    uint8_t  mappedId;   // 対応表の番号。0 = 表にない（パケットの node_id を使う）
    uint8_t  nodeId;     // 出力する番号
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
static bool      s_showInfo   = false;   // ボタン B の AP MAC 表示中
static uint32_t  s_lastDropped = 0;

/**
 * @brief 対応表から MAC に割り当てた番号を探す。
 *
 * @param macStr "AA:BB:CC:DD:EE:FF" 形式（大文字小文字は区別しない）
 * @return 番号。表になければ 0
 */
static uint8_t lookupMappedId(const char *macStr)
{
    for (const NodeMapEntry *e = NODE_MAP; e->nodeId != 0; e++) {
        if (strcasecmp(e->mac, macStr) == 0) return e->nodeId;
    }
    return 0;
}

/**
 * @brief MAC で node を探し、なければ対応表の番号を引いて追加する。
 *
 * @param mac    送信元 MAC
 * @param nodeId パケットの node_id（表にない node の番号に使う）
 * @return インデックス。テーブルが満杯なら -1
 */
static int findOrAddNode(const uint8_t *mac, uint8_t nodeId)
{
    for (int i = 0; i < s_nodeCount; i++) {
        if (memcmp(s_nodes[i].mac, mac, 6) == 0) return i;
    }
    if (s_nodeCount >= GATEWAY_MAX_NODES) return -1;

    int idx = s_nodeCount++;
    NodeEntry &node = s_nodes[idx];
    memset(&node, 0, sizeof(NodeEntry));
    memcpy(node.mac, mac, 6);

    char macStr[18];
    comm_mac_to_str(mac, macStr);
    node.mappedId = lookupMappedId(macStr);
    if (node.mappedId != 0) {
        LOG_I("New node #%d: %s -> node %u (table)", idx + 1, macStr, node.mappedId);
    } else {
        LOG_W("New node #%d: %s not in GATEWAY_NODE_TABLE, using NODE_ID %u. Add: {\"%s\", %u},",
              idx + 1, macStr, nodeId, macStr, nodeId);
    }
    return idx;
}

/**
 * @brief 対応表に同じ MAC・同じ番号が 2 回出ていたら警告する。
 */
static void checkNodeMap(void)
{
    for (const NodeMapEntry *a = NODE_MAP; a->nodeId != 0; a++) {
        for (const NodeMapEntry *b = a + 1; b->nodeId != 0; b++) {
            if (strcasecmp(a->mac, b->mac) == 0) LOG_W("GATEWAY_NODE_TABLE: duplicate MAC %s", a->mac);
            if (a->nodeId == b->nodeId) LOG_W("GATEWAY_NODE_TABLE: duplicate node %u", a->nodeId);
        }
    }
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
                       node.nodeId, macStr, hdr->seq, (unsigned long)hdr->time_ms,
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
        LOG_W("JSON line too long, dropped (node %u)", node.nodeId);
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

    int idx = findOrAddNode(mac, hdr->node_id);
    if (idx < 0) {
        LOG_W("Node table full, packet dropped");
        return;
    }

    NodeEntry &node = s_nodes[idx];
    if (node.rxCount > 0) {
        uint16_t gap = (uint16_t)(hdr->seq - node.lastSeq - 1);
        if (gap < 1000) node.lostCount += gap;   // 大きな飛びは node の再起動とみなす
    }
    node.nodeId  = (node.mappedId != 0) ? node.mappedId : hdr->node_id;
    node.lastSeq = hdr->seq;
    node.rxCount++;
    memcpy(node.data, data, len);
    node.len = len;

    sendJson(node, hdr, data, len);

    if (idx == s_dispNode && !s_showInfo) s_dispDirty = true;
}

// =============================================
// gateway 自身の加速度（"node":0 の行）
// =============================================
#if GATEWAY_ACCEL_ENABLE
static const Module *s_selfAccel  = nullptr;   // 加速度モジュール。使えなければ nullptr
static uint16_t      s_selfSeq    = 0;
static uint32_t      s_selfLastTx = 0;

/**
 * @brief 加速度モジュールを gateway 用に初期化する。他のモジュール（振動など）は動かさない。
 */
static void selfAccelInit(void)
{
    const Module *m = module_find(MODULE_ID_ACCEL);
    if (m == nullptr || (m->init != nullptr && !m->init())) {
        LOG_W("Gateway accel unavailable, node %d disabled", GATEWAY_SELF_NODE_ID);
        return;
    }
    s_selfAccel = m;
    LOG_I("Gateway accel: node %d every %d ms", GATEWAY_SELF_NODE_ID, GATEWAY_ACCEL_PERIOD_MS);
}

/**
 * @brief 出力周期が来たら加速度を読み、node の行と同じ形式の JSON 1 行（"node":0）をシリアルへ出す。
 *
 * @param now 現在時刻 [ms]
 */
static void selfAccelPoll(uint32_t now)
{
    if (s_selfAccel == nullptr || now - s_selfLastTx < GATEWAY_ACCEL_PERIOD_MS) return;
    s_selfLastTx = now;

    s_selfAccel->update();
    float x, y, z;
    if (!accel_get(&x, &y, &z)) return;   // まだ一度も読めていない

    uint8_t data[PROTOCOL_MAX_PAYLOAD];
    size_t  len = s_selfAccel->encode(data, sizeof(data));
    if (len == 0) return;

    char line[192];
    int pos = snprintf(line, sizeof(line), "{\"node\":%d,\"mac\":\"%s\",\"seq\":%u,\"t\":%lu,",
                       GATEWAY_SELF_NODE_ID, comm_self_mac(), s_selfSeq++, (unsigned long)now);
    int n = s_selfAccel->to_json(data, len, line + pos, sizeof(line) - pos - 1);
    if (n <= 0 || pos + n >= (int)sizeof(line) - 1) return;
    pos += n;
    line[pos++] = '}';
    line[pos]   = '\0';
    Serial.println(line);
}
#endif  // GATEWAY_ACCEL_ENABLE

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

/**
 * @brief 自機の AP MAC（node の COMM_GATEWAY_MAC に設定する値）と受信中の node 数を描く（ボタン B）。
 */
static void drawInfo(void)
{
    char buf[40];

    display_begin("== GATEWAY ==", TFT_YELLOW);
    display_print("AP MAC:\n", 2, TFT_WHITE);
    snprintf(buf, sizeof(buf), "%s\n", comm_self_mac());
    display_print(buf, 2, TFT_YELLOW);
    snprintf(buf, sizeof(buf), "Nodes: %u/%d  ch %d\n", s_nodeCount, GATEWAY_MAX_NODES, COMM_ESPNOW_CHANNEL);
    display_print(buf, 2, TFT_WHITE);
    display_print("= COMM_GATEWAY_MAC\n", 1, TFT_LIGHTGREY);
    display_print("B: back", 1, TFT_DARKGREY);
    display_footer();
}

/**
 * @brief gateway の初期化。WiFi AP と ESP-NOW を起動し、AP MAC を 3 秒表示する。
 */
void app_init(void)
{
    if (!comm_init()) {
        display_fatal("ESP-NOW\nFAILED");
    }
    checkNodeMap();
#if GATEWAY_ACCEL_ENABLE
    selfAccelInit();
#endif

    // 起動時に AP MAC を表示（3 秒）
    display_begin("AP MAC:", TFT_YELLOW);
    display_print(comm_self_mac(), 1, TFT_WHITE);
    display_print("\n\nSet to\nCOMM_GATEWAY_MAC", 2, TFT_CYAN);
    delay(3000);

    LOG_I("Gateway ready.");
    drawWaiting();
}

/**
 * @brief gateway のメインループ。ボタン A で表示 node を切り替え、受信キューを空になるまで処理し、
 *        取りこぼしを警告し、表示中の node が更新されていれば描き直す。
 */
void app_loop(void)
{
    // ボタン B: AP MAC 表示の切り替え
    if (M5.BtnB.wasPressed()) {
        s_showInfo  = !s_showInfo;
        s_dispDirty = true;
    }

    // ボタン A: 表示する node を切り替える（AP MAC 表示中なら通常表示に戻る）
    if (M5.BtnA.wasPressed()) {
        if (s_showInfo) {
            s_showInfo  = false;
            s_dispDirty = true;
        } else if (s_nodeCount > 1) {
            s_dispNode  = (s_dispNode + 1) % s_nodeCount;
            s_dispDirty = true;
        }
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
#if GATEWAY_ACCEL_ENABLE
    selfAccelPoll(now);
#endif

    if (s_dispDirty && display_due(now)) {
        if (s_showInfo)          drawInfo();
        else if (s_nodeCount > 0) drawNode(s_dispNode);
        else                     drawWaiting();
        s_dispDirty = false;
    }

    delay(1);
}

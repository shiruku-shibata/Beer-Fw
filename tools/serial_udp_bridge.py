#!/usr/bin/env python3
"""
gateway のシリアル出力を UDP に中継するブリッジ（PC 側）

gateway は 1 パケット = 1 行の JSON を USB シリアルに出す（ログ行は "#" 始まり）。
Godot 4 には標準のシリアル API がないため、JSON 行だけを UDP で Godot に渡す。

【使い方】
  python tools/serial_udp_bridge.py                 # ポートを自動検出して 127.0.0.1:9000 へ送る
  python tools/serial_udp_bridge.py --port /dev/cu.usbserial-XXXX
  python tools/serial_udp_bridge.py --demo          # 実機なしで疑似データを送る（Godot 側の確認用）

pyserial が必要（PlatformIO の Python には入っている）:
  ~/.platformio/penv/bin/python tools/serial_udp_bridge.py
"""

import argparse
import json
import math
import random
import socket
import sys
import time

DEFAULT_BAUD = 1500000   # gateway の COMM_SERIAL_BAUD（src/config.h）
DEFAULT_HOST = "127.0.0.1"
DEFAULT_UDP_PORT = 9000
STATS_PERIOD_S = 5.0     # 転送数の表示周期 [s]
RECONNECT_WAIT_S = 2.0   # シリアルが切れたときの再接続間隔 [s]


def log(msg):
    """時刻付きでメッセージを表示する。"""
    print(time.strftime("%H:%M:%S"), msg, flush=True)


# =============================================
# シリアル
# =============================================
def find_port():
    """USB シリアルのポートを 1 つ探す。見つからなければ None。"""
    from serial.tools import list_ports

    ports = [p for p in list_ports.comports() if p.vid is not None]   # USB 機器だけ（Bluetooth 等を除く）
    if not ports:
        return None
    if len(ports) > 1:
        log("USB シリアルが複数あります。--port で指定できます: " + ", ".join(p.device for p in ports))
    return ports[0].device


def open_serial(port, baud):
    """ESP32 をリセットさせずにシリアルポートを開く（DTR / RTS を下げてから開く）。"""
    import serial

    ser = serial.Serial()
    ser.port = port
    ser.baudrate = baud
    ser.timeout = 1
    # DTR / RTS を下げてから開く（ESP32 の自動リセット回路でリセットさせないため）
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def run_serial(args, sock, dest):
    """gateway のシリアル出力を読み、JSON 行（"{" 始まり）だけを UDP で dest へ送る。

    ログ行はそのまま表示する。ポートが見つからない・切れたときは待って再接続し続ける。
    """
    try:
        import serial
    except ImportError:
        sys.exit("pyserial がありません。~/.platformio/penv/bin/python で実行するか、pip install pyserial してください")

    forwarded = 0
    last_stats = time.monotonic()

    while True:
        port = args.port or find_port()
        if port is None:
            log("gateway が見つかりません。USB を接続してください…")
            time.sleep(RECONNECT_WAIT_S)
            continue

        try:
            ser = open_serial(port, args.baud)
        except serial.SerialException as e:
            log("%s を開けません: %s（pio device monitor などが使っていないか確認）" % (port, e))
            time.sleep(RECONNECT_WAIT_S)
            continue

        log("%s (%d bps) → UDP %s:%d" % (port, args.baud, dest[0], dest[1]))
        try:
            while True:
                raw = ser.readline()
                now = time.monotonic()
                if now - last_stats >= STATS_PERIOD_S:
                    log("転送 %.1f 行/s" % (forwarded / (now - last_stats)))
                    forwarded = 0
                    last_stats = now
                if not raw:
                    continue

                line = raw.decode("utf-8", errors="replace").strip()
                if line.startswith("{"):
                    sock.sendto(line.encode("utf-8"), dest)
                    forwarded += 1
                    if args.verbose:
                        print(line, flush=True)
                elif line:
                    print("  gateway", line, flush=True)   # "# " 始まりのログ・起動時の出力
        except serial.SerialException as e:
            log("シリアルが切れました: %s" % e)
            ser.close()
            time.sleep(RECONNECT_WAIT_S)


# =============================================
# 疑似データ（実機なし）
# =============================================
def run_demo(args, sock, dest):
    """gateway と同じ形式の JSON を node ごとに 10 Hz で送る。gateway 自身の行（"node":0）は 100 Hz で送る。"""
    log("疑似データ: node %d 台 → UDP %s:%d" % (args.nodes, dest[0], dest[1]))
    period = 0.01           # gateway の行の周期（GATEWAY_ACCEL_PERIOD_MS）
    node_every = 10         # node の行は gateway の行 10 回に 1 回（10 Hz）
    tick = 0
    seq = [0] * args.nodes
    gulps = [0] * args.nodes
    gw_seq = 0
    start = time.monotonic()
    next_t = start

    while True:
        t = time.monotonic() - start
        for i in range(args.nodes if tick % node_every == 0 else 0):
            node = i + 1
            # node ごとに周期をずらして傾ける
            phase = t * (0.5 + 0.2 * i)
            tilt = max(0.0, math.sin(phase)) * 70.0
            rad = math.radians(tilt)
            ax = math.sin(rad) * math.cos(phase * 0.3)
            ay = math.sin(rad) * math.sin(phase * 0.3)
            az = math.cos(rad)
            active = tilt >= 40
            if active and seq[i] % 5 == 0:
                gulps[i] += 1

            packet = {
                "node": node,
                "mac": "00:00:00:DE:00:%02X" % node,
                "seq": seq[i],
                "t": int(t * 1000),
                "rx": seq[i] + 1,
                "lost": 0,
                "accel": {
                    "x": round(ax + random.uniform(-0.02, 0.02), 4),
                    "y": round(ay + random.uniform(-0.02, 0.02), 4),
                    "z": round(az + random.uniform(-0.02, 0.02), 4),
                },
                "vibration": 100 if active else 0,
                "drink": {"armed": 1, "active": int(active), "tilt": int(tilt), "gulps": gulps[i]},
                "battery": {"level": max(0, 90 - node * 10), "low": 0, "alarm": 0},
            }
            sock.sendto(json.dumps(packet, separators=(",", ":")).encode("utf-8"), dest)
            seq[i] = (seq[i] + 1) % 65536

        # gateway 自身の加速度・角速度（"node":0。机に置いたまま、わずかに揺れる）
        gateway = {
            "node": 0,
            "mac": "00:00:00:DE:00:00",
            "seq": gw_seq,
            "t": int(t * 1000),
            "accel": {
                "x": round(random.uniform(-0.01, 0.01), 4),
                "y": round(random.uniform(-0.01, 0.01), 4),
                "z": round(1.0 + random.uniform(-0.01, 0.01), 4),
            },
            "gyro": {
                "x": round(random.uniform(-2.0, 2.0), 2),
                "y": round(random.uniform(-2.0, 2.0), 2),
                "z": round(random.uniform(-2.0, 2.0), 2),
            },
        }
        sock.sendto(json.dumps(gateway, separators=(",", ":")).encode("utf-8"), dest)
        gw_seq = (gw_seq + 1) % 65536
        tick += 1

        next_t += period
        time.sleep(max(0.0, next_t - time.monotonic()))


def main():
    """引数を読み、実機中継（既定）か疑似データ送信（--demo）を始める。Ctrl+C で終了。"""
    ap = argparse.ArgumentParser(description="gateway のシリアル JSON を UDP に中継する")
    ap.add_argument("--port", help="シリアルポート（省略時は USB シリアルを自動検出）")
    ap.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    ap.add_argument("--host", default=DEFAULT_HOST, help="送信先（Godot を動かす PC）")
    ap.add_argument("--udp-port", type=int, default=DEFAULT_UDP_PORT)
    ap.add_argument("--demo", action="store_true", help="実機なしで疑似データを送る")
    ap.add_argument("--nodes", type=int, default=2, help="--demo の node 台数")
    ap.add_argument("-v", "--verbose", action="store_true", help="転送した JSON も表示する")
    args = ap.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    dest = (args.host, args.udp_port)
    try:
        if args.demo:
            run_demo(args, sock, dest)
        else:
            run_serial(args, sock, dest)
    except KeyboardInterrupt:
        log("終了")


if __name__ == "__main__":
    main()

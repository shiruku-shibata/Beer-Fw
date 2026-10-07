# Beer-fw

M5StickC Plus2 を使ったゲーム用の共通ファームウェア。
複数台の node で計測したセンサー値を ESP-NOW で受信機（gateway）に集め、USB シリアル経由で PC（Godot など）から使える。
ゲームごとの動作はゲーム専用モジュールとビルド環境で足す。

## 機能

- **センサー送信**: node（最大 8 台）の値を gateway が JSON 1 行ずつ USB シリアルへ出力
- **モジュール構成**: センサー・機能ごとにファイルを分け、`config.h` とビルド環境で有効/無効を切り替え
- **汎用モジュール**: 加速度、振動（Vibration Hat）、電池監視（残量 20 % 以下でフラグ送信、5 % 以下で警報振動）
- **省電力**: 送信時だけ無線を起動、画面の自動消灯、CPU 80 MHz、待ち時間の light sleep

## 収録ゲーム

| ゲーム | 環境 | 内容 |
| --- | --- | --- |
| ビールジョッキ | `mug` | ジョッキに入れた node が飲む動作（傾ける）を検出し、「ゴクゴク」と振動する |

## 必要なもの

- M5StickC Plus2（node 1 台〜、gateway 1 台）
- M5StickC Vibration Hat（M5Stack U159）（振動を使う場合）
- [PlatformIO](https://platformio.org/)（VS Code 拡張または CLI）

## ビルド・書き込み

| 区分 | 環境 | 用途 |
| --- | --- | --- |
| 汎用 | `node` | 送信側。100 ms ごとに送信 |
| 汎用 | `node_lowpower` | 送信側の省電力版。送信時だけ無線を起動し、1 秒ごと＋状態変化時に送信 |
| 汎用 | `gateway` | 受信機。全ゲーム共通。受信データを JSON 1 行ずつ USB シリアルへ出力 |
| ゲーム別 | `mug` | ビールジョッキ（`node_lowpower` + 飲酒検出） |

```sh
pio run -e gateway -t upload                                   # 受信機
pio run -e node -t upload                                      # 汎用 node（NODE_ID=1）
PLATFORMIO_BUILD_FLAGS="-DNODE_ID=2" pio run -e node -t upload # 2 台目以降
pio run -e mug -t upload                                       # ビールジョッキ
pio device monitor                                             # シリアル出力（115200 bps）
```

node 側の送信先は `src/config.h` の `COMM_GATEWAY_MAC`。受信機の起動画面に出る AP MAC を設定する。

新しいゲームの足し方は [doc/system-architecture.md](doc/system-architecture.md) の「ビルド環境」を参照。

## 使い方（ビールジョッキ）

1. ジョッキを机にまっすぐ置き、A ボタンを押す（姿勢の基準を記録して検出開始）
2. 飲む（傾ける）と振動する
3. A ボタン短押しで基準の取り直し、1 秒以上の長押しで停止

## ドキュメント

- [doc/system-architecture.md](doc/system-architecture.md) — システム構成・ビルド環境・通信プロトコル・各機能の仕様
- [doc/fw-structure-proposal.md](doc/fw-structure-proposal.md) — ファイル構成の設計意図、モジュールの追加手順

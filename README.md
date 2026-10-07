# Beer-fw

ビールジョッキに入れた M5StickC Plus2 が、飲む動作（ジョッキを傾ける）を検出して「ゴクゴク」と振動するファームウェア。
計測結果は ESP-NOW で受信機（gateway）に送り、USB シリアル経由で PC（Godot など）から使える。

## 機能

- **飲酒検出**: A ボタンで開始。基準姿勢から 40° 以上傾けて 0.3 秒続くと、ゴクッの振動を繰り返す（深く傾けるほど速い）
- **電池監視**: 残量 20 % 以下で gateway へフラグを送信、5 % 以下で電源が落ちるまで振動
- **省電力**: 送信時だけ無線を起動、画面の自動消灯、CPU 80 MHz、待ち時間の light sleep
- **モジュール構成**: センサー・機能ごとにファイルを分け、`config.h` で有効/無効を切り替え

## 必要なもの

- M5StickC Plus2（ジョッキ用 1 台〜、受信機用 1 台）
- M5StickC Vibration Hat（M5Stack U159）
- [PlatformIO](https://platformio.org/)（VS Code 拡張または CLI）

## ビルド・書き込み

| 環境 | 用途 |
| --- | --- |
| `mug` | ジョッキ用。省電力（送信時だけ無線を起動し、1 秒ごと＋状態変化時に送信） |
| `node` | 送信側の常時通信版（100 ms ごとに送信） |
| `gateway` | 受信機。受信データを JSON 1 行ずつ USB シリアルへ出力 |

```sh
pio run -e gateway -t upload                                   # 受信機
pio run -e mug -t upload                                       # ジョッキ（NODE_ID=1）
PLATFORMIO_BUILD_FLAGS="-DNODE_ID=2" pio run -e mug -t upload  # 2 台目以降
pio device monitor                                             # シリアル出力（115200 bps）
```

ジョッキ側の送信先は `src/config.h` の `COMM_GATEWAY_MAC`。受信機の起動画面に出る AP MAC を設定する。

## 使い方

1. ジョッキを机にまっすぐ置き、A ボタンを押す（姿勢の基準を記録して検出開始）
2. 飲む（傾ける）と振動する
3. A ボタン短押しで基準の取り直し、1 秒以上の長押しで停止

## ドキュメント

- [doc/system-architecture.md](doc/system-architecture.md) — システム構成・通信プロトコル・各機能の仕様
- [doc/fw-structure-proposal.md](doc/fw-structure-proposal.md) — ファイル構成の設計意図、モジュールの追加手順

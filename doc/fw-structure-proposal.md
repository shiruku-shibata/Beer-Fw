# FW ファイル構成 見直し案

最終更新: 2026-10-04 / ステータス: 実装済み（段階 1〜5）

## 結論

ご提示の「`main` + `config.h` + モジュールごとのファイル」という骨格は採用し、組み込み FW として長く保守するために次の 6 点を変える。

| # | ご提示の案 | 変更案 | 理由 |
| --- | --- | --- | --- |
| 1 | `.c` ファイル | `.cpp`（中身は C 風に書く） | M5Unified / Arduino / ESP-NOW の Arduino ラッパーは C++ API。`.c` からは呼べない |
| 2 | `src/` 直下にフラット配置 | `src/core/`・`src/modules/`・`src/app/` に分ける | モジュールが 10 個を超えると共通処理と混ざって見通しが悪くなる |
| 3 | `main.c` が通信・電源などの共通処理を持つ | `main.cpp` は起動と呼び出し順だけ。共通処理は `core/` に機能ごとに置く | `main` が肥大化すると「共通処理の中のどこか」を探す羽目になる |
| 4 | Sender / Receiver の区別がない | 1 プロジェクト・2 ビルド環境（`node` / `gateway`）にする | 現状 2 プロジェクトで構造体やコードが二重管理になっている |
| 5 | モジュール追加の仕組みが未定義 | 共通インターフェース（`Module` 構造体）と登録テーブルを用意 | `main` にセンサー名を書かずにモジュールを回すため |
| 6 | 通信データ形式が固定（加速度 12 バイト） | ヘッダ + モジュールごとのレコードを並べる可変形式 | センサーを足すたびに通信部を直さずに済む |

ルートは `doc/` と `src/` に加え、PlatformIO の都合で `platformio.ini` と `scripts/` が必要になる（後述）。

## 1. ディレクトリ構成

```text
/
├── platformio.ini            # ビルド環境の定義（node / gateway）。ルート必須
├── doc/                      # 設計資料
├── scripts/
│   └── fix_dfrobot.py        # ビルド前パッチ（依存ライブラリ経由で必要）
└── src/
    ├── main.cpp              # setup() / loop()。起動順と呼び出しだけ
    ├── config.h              # コンパイル時設定・モジュール有効/無効
    │
    ├── core/                 # センサーに依存しない共通処理
    │   ├── system.cpp/.h     # ハード初期化、時刻、ウォッチドッグ
    │   ├── scheduler.cpp/.h  # モジュールを周期ごとに呼ぶ
    │   ├── comm.cpp/.h       # ESP-NOW 送受信、ピア管理
    │   ├── protocol.h        # パケット形式・モジュール ID 一覧
    │   ├── power.cpp/.h      # 電池残量、低電圧時の動作
    │   ├── display.cpp/.h    # LCD の共通表示
    │   └── log.h             # ログ出力マクロ（レベル切替）
    │
    ├── modules/              # センサー・機能モジュール
    │   ├── module.h          # 全モジュール共通のインターフェース定義
    │   ├── registry.cpp      # 有効なモジュールの一覧（ここだけ #if が並ぶ）
    │   ├── _template.cpp     # 新規モジュールのひな形
    │   ├── accel.cpp         # 加速度（現行の機能を移植）
    │   └── battery.cpp       # 例: 電池残量を「データ」として送る場合
    │
    └── app/                  # 役割ごとの動作
        ├── node.cpp          # 旧 Sender: 計測して送る
        └── gateway.cpp       # 旧 Receiver: 受けて PC へ出す
```

### ルートが `doc/` と `src/` だけにならない理由

- **`platformio.ini`**: PlatformIO はプロジェクトルートでこのファイルを探す。ビルド環境・ライブラリ・書き込み設定の置き場なので、ルートに置くのが標準。
- **`scripts/fix_dfrobot.py`**: `DFRobot_GP8XXX` は直接指定していないが、M5 系ライブラリの依存として入ってくる。現状 2 プロジェクトとも `.pio/libdeps` に存在しており、パッチは引き続き必要。
- `include/`・`lib/`・`test/` の空 README は削除してよい。ヘッダは `.cpp` の隣に置く（次節）。

### 将来の拡張先

- Godot 用のブリッジや受信サンプルを置くときは、ルートに `tools/`（PC 側スクリプト）や `godot/` を追加する。FW の `src/` には混ぜない。
- 1 つのモジュールが大きくなったら（ドライバとフィルタ処理が分かれるなど）、`modules/gps/` のようにサブディレクトリへ昇格する。

## 2. `.cpp` / `.h` の分け方

方針は「**外から呼ばれるものだけヘッダを持つ**」。

| 種類 | ヘッダ | 置き場所 | 理由 |
| --- | --- | --- | --- |
| `core/` の各機能 | あり | `.cpp` と同じディレクトリ | `main` や `app` から呼ばれる |
| `modules/module.h` | あり | `modules/` | 全モジュールと `core` が共有する契約 |
| 各モジュール（`accel.cpp` など） | **なし** | — | 外へ見せるのは `Module` 構造体 1 個だけ。`registry.cpp` で `extern` 宣言すれば足りる |
| 出力系モジュール（`vibration.cpp` など） | あり | `modules/` | 他から操作 API を呼ばれるため。モジュール無効時は何もしない inline 関数に差し替える |
| `config.h` / `protocol.h` | ヘッダのみ | `src/` / `core/` | 定数と型だけ |

モジュールの内部関数・変数はすべて `static` にする。モジュール同士が直接呼び合うことは禁止し、必要なら `core` 経由にする。

`include/` ディレクトリに集めない理由: 公開範囲が `core` / `modules` で分かれるので、実装の隣にある方が対応が追いやすい。PlatformIO は `src/` をインクルードパスに含めるため、`#include "core/comm.h"` で参照できる。

### 命名

- ファイル名は `module-xxx` ではなく `modules/xxx.cpp`。ディレクトリが分類を表すので接頭辞は不要。
- 区切りはハイフンではなくアンダースコア（`imu_bmi270.cpp`）。C の識別子（`module_imu_bmi270`）と揃い、grep しやすい。

## 3. `main.cpp`・`core`・`app`・モジュールの責務

```text
main.cpp ──▶ core/system   起動・時刻
   │
   └──▶ app/node.cpp  または  app/gateway.cpp   （ビルド環境で片方だけ）
            │
            ├──▶ core/scheduler ──▶ modules/registry ──▶ accel.cpp, gps.cpp, ...
            ├──▶ core/comm      （送る / 受ける）
            ├──▶ core/power
            └──▶ core/display
```

| 層 | 持つもの | 持たないもの |
| --- | --- | --- |
| `main.cpp` | `setup()` で `system_init()` → `app_init()`、`loop()` で `app_loop()` を呼ぶだけ | センサー名、通信の詳細、表示内容 |
| `core/` | 通信、電源、表示、スケジューラ、ログ。センサーの種類を知らない | 特定センサーの処理、モジュール名の分岐 |
| `app/node.cpp` | 計測 → パケット組み立て → 送信の流れ | 個別センサーの読み方 |
| `app/gateway.cpp` | 受信 → デコード → PC への出力（JSON など） | 個別センサーのデータ形式（デコードはモジュールに任せる） |
| `modules/xxx.cpp` | 初期化、計測、データのエンコードとデコード、（任意で）表示 | 通信、電源、他モジュールへの依存 |

### モジュール共通インターフェース

```cpp
// src/modules/module.h
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    const char *name;        // ログ・表示用
    uint8_t     id;          // protocol.h の MODULE_ID_xxx（通信上の識別子）
    uint32_t    period_ms;   // update() を呼ぶ周期

    bool   (*init)(void);                                     // 失敗時 false → そのモジュールだけ無効化
    void   (*update)(void);                                   // 計測（ブロッキング禁止）
    size_t (*encode)(uint8_t *buf, size_t cap);               // 最新値を送信用バイト列へ
    int    (*to_json)(const uint8_t *buf, size_t len,
                      char *out, size_t cap);                 // gateway 側で PC 向けに整形
    void   (*draw)(void);                                     // 任意。不要なら nullptr
} Module;
```

エンコードとデコードを同じファイルに置くことで、センサーの送信形式を変えても node と gateway で食い違わない。現状 `AccelData` を 2 プロジェクトで手作業で揃えている問題がここで解消する。

### 登録テーブル

```cpp
// src/modules/registry.cpp
#include "config.h"
#include "modules/module.h"

#if MODULE_ACCEL_ENABLE
extern const Module module_accel;
#endif
#if MODULE_GPS_ENABLE
extern const Module module_gps;
#endif

const Module *const g_modules[] = {
#if MODULE_ACCEL_ENABLE
    &module_accel,
#endif
#if MODULE_GPS_ENABLE
    &module_gps,
#endif
};
const size_t g_moduleCount = sizeof(g_modules) / sizeof(g_modules[0]);
```

`#if` によるモジュール名の列挙はこのファイルだけに集める。`core` と `app` は `g_modules` を回すだけで、センサー名を一切書かない。

static コンストラクタやリンカセクションを使った「自動登録」もできるが、組み込みでは初期化順が読みにくくデバッグしづらい。一覧が 1 ファイルで見える明示テーブルを推奨する。

### 通信データ形式

```text
| ヘッダ                                        | レコード 1          | レコード 2          | ...
| ver(1) | node_id(1) | seq(2) | time_ms(4) | n(1) | mod_id(1) len(1) data | mod_id(1) len(1) data |
```

- gateway は `mod_id` から該当モジュールの `to_json()` を呼ぶ。知らない `mod_id` は読み飛ばせるので、node だけ先に更新しても壊れない。
- `seq` で欠落を、`ver` で形式の不一致を検出できる。
- ESP-NOW の 1 パケット上限 250 バイトに収まるよう、`comm` 側で合計長を検査する。

## 4. `config.h` に持たせる設定

`config.h` はコンパイル時の既定値だけを持つ。各項目は `#ifndef` で囲み、`platformio.ini` の `build_flags` で個体ごとに上書きできるようにする。

```cpp
// src/config.h
#pragma once

// ===== 役割（platformio.ini の環境で指定） =====
// ROLE_NODE / ROLE_GATEWAY のどちらか一方を -D で渡す
#if !defined(ROLE_NODE) && !defined(ROLE_GATEWAY)
#error "ROLE_NODE か ROLE_GATEWAY を build_flags で指定してください"
#endif

// ===== モジュール有効/無効 =====
#ifndef MODULE_ACCEL_ENABLE
#define MODULE_ACCEL_ENABLE    1
#endif
#ifndef MODULE_GPS_ENABLE
#define MODULE_GPS_ENABLE      0
#endif

// ===== モジュール別パラメータ =====
#define ACCEL_PERIOD_MS        20      // 50 Hz

// ===== 通信 =====
#define COMM_ESPNOW_CHANNEL    1
#define COMM_TX_PERIOD_MS      20      // まとめて送る周期
#define COMM_SERIAL_BAUD       115200
#ifndef NODE_ID
#define NODE_ID                0       // 個体ごとに build_flags で上書き
#endif

// ===== 電源 =====
#define POWER_CHECK_PERIOD_MS  5000
#define POWER_LOW_PERCENT      15

// ===== 表示 =====
#define DISPLAY_ENABLE         1
#define DISPLAY_PERIOD_MS      100

// ===== デバッグ =====
#define LOG_LEVEL              LOG_LEVEL_INFO
```

| 区分 | 例 | 置き場所 |
| --- | --- | --- |
| 役割・個体差 | `ROLE_*`、`NODE_ID`、送信先 MAC | `platformio.ini` の環境ごとの `build_flags` |
| 機能の有無・周期・閾値 | `MODULE_*_ENABLE`、`*_PERIOD_MS` | `config.h` |
| プロトコル定数・モジュール ID | `PROTOCOL_VERSION`、`MODULE_ID_ACCEL` | `core/protocol.h`（勝手に変えると互換性が壊れるため分離） |
| 実行時に変えたい値 | キャリブレーション値、ペアリング先 | 将来は NVS（不揮発メモリ）へ。`config.h` は既定値のみ |

`platformio.ini` のイメージ:

```ini
[env]
platform = espressif32@6.5.0
board = m5stick-c
framework = arduino
lib_deps =
    m5stack/M5StickCPlus2@^1.0.2
    m5stack/M5Unified@0.1.12
extra_scripts = pre:scripts/fix_dfrobot.py

[env:node]
build_flags = -DROLE_NODE -DNODE_ID=1
build_src_filter = +<*> -<app/gateway.cpp>

[env:gateway]
build_flags = -DROLE_GATEWAY
build_src_filter = +<*> -<app/node.cpp>
```

`config.h` の末尾に整合性チェックを置き、設定ミスをビルド時に止める（例: `COMM_TX_PERIOD_MS` が最短のモジュール周期より長い、有効モジュールの最大ペイロード合計が 250 バイトを超える）。

## 5. 新しいセンサーを追加する手順

既存コードの変更は **3 か所・各 1〜2 行** に限られる。`main.cpp`・`core/`・`app/` は触らない。

1. `src/modules/_template.cpp` を `src/modules/xxx.cpp` にコピーし、`init` / `update` / `encode` / `to_json` を実装する。ファイル全体を `#if MODULE_XXX_ENABLE` で囲む。
2. `core/protocol.h` に `MODULE_ID_XXX` を追加する（既存 ID の再利用・変更は禁止）。
3. `config.h` に `MODULE_XXX_ENABLE` と周期などのパラメータを追加する。
4. `modules/registry.cpp` に `extern` 宣言とテーブル行を追加する。
5. 外部ライブラリが必要なら `platformio.ini` の `lib_deps` に追加する。
6. `pio run -e node` と `pio run -e gateway` の両方がビルドできることを確認する。
7. `doc/` の通信プロトコル・モジュール一覧を更新する。

## 6. 移行手順（案）

現行の動作を保ったまま、段階的に移す。

1. ルートに `platformio.ini`・`scripts/`・`src/` を作り、`node` / `gateway` の 2 環境でビルドできる状態にする。中身は現行コードをほぼそのまま `app/` に移す。
2. `core/comm`・`core/display`・`core/power` を切り出す。
3. `module.h`・`registry.cpp`・`accel.cpp` を作り、加速度処理をモジュール化する。
4. 通信データ形式を「ヘッダ + レコード」に切り替える。gateway の JSON 出力も合わせて変える（Godot 側への影響あり）。
5. 旧 `espnow_sender/`・`espnow_receiver/` を削除する（2026-10-04 実施済み）。

各段階でビルドが通り、実機で現行と同じ動作になることを確認してから次に進む。

## 未決事項

- [ ] この構成で進めてよいか
- [ ] 通信データ形式を変えるタイミング（段階 4 は Godot 側の受信処理にも影響する）
- [ ] 送信先 MAC を固定のまま `build_flags` に移すか、ブロードキャスト送信にするか
- [ ] git 管理を始めるか（現在このディレクトリは git リポジトリではない。移行前に初期化しておくと戻しやすい）

/**
 * ログ出力マクロ
 *
 * ログ行は必ず "# " で始める。gateway のシリアル出力では
 * データ行（JSON）と区別するため、PC 側は "#" 始まりの行を読み飛ばせばよい。
 */
#pragma once

#include <Arduino.h>

#define LOG_LEVEL_NONE   0
#define LOG_LEVEL_ERROR  1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_INFO   3
#define LOG_LEVEL_DEBUG  4

#include "config.h"

#define LOG_E(fmt, ...) do { if (LOG_LEVEL >= LOG_LEVEL_ERROR) Serial.printf("# [E] " fmt "\n", ##__VA_ARGS__); } while (0)
#define LOG_W(fmt, ...) do { if (LOG_LEVEL >= LOG_LEVEL_WARN)  Serial.printf("# [W] " fmt "\n", ##__VA_ARGS__); } while (0)
#define LOG_I(fmt, ...) do { if (LOG_LEVEL >= LOG_LEVEL_INFO)  Serial.printf("# [I] " fmt "\n", ##__VA_ARGS__); } while (0)
#define LOG_D(fmt, ...) do { if (LOG_LEVEL >= LOG_LEVEL_DEBUG) Serial.printf("# [D] " fmt "\n", ##__VA_ARGS__); } while (0)
